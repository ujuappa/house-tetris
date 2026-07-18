// FurnitureActor.cpp
// House Tetris

#include "FurnitureActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h" // GetAllActorsOfClass 사용을 위해 필요 
#include "Engine/StaticMesh.h"

// ── Constructor ───────────────────────────────────────────────────────────────
FVector AFurnitureActor::GetWorldExtent() const
{
    FVector Origin, Extent;
    // 현재 회전까지 반영된 월드 공간 AABB를 계산 (false = 눈에 보이는 메시 전체 기준)
    GetActorBounds(false, Origin, Extent);
    return Extent; // Extent = 중심에서 각 면까지의 거리(반쪽 크기)
}

bool AFurnitureActor::WouldOverlapAt(const FVector& TestCenter) const
{
    const float Margin = 1.0f; // 1cm 여유 — 딱 붙는 배치는 허용하고, float 오차로 막히는 것 방지

    FVector MyExt = GetWorldExtent() - FVector(Margin); // 내 크기를 살짝 줄여 여유 확보
    FVector MyMin = TestCenter - MyExt; // 내 박스의 최소 모서리
    FVector MyMax = TestCenter + MyExt; // 내 박스의 최대 모서리

    // 월드의 모든 가구를 가져옴. 이동/스폰은 '키 입력 시에만' 일어나므로 매 프레임이 아님 → 성능 걱정 없음.
    TArray<AActor*> Found;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AFurnitureActor::StaticClass(), Found);

    for (AActor* A : Found)
    {
        if (A == this) continue; // 자기 자신은 제외

        const AFurnitureActor* Other = Cast<AFurnitureActor>(A);
        if (!IsValid(Other)) continue; // 삭제 중(PendingKill)인 가구는 건너뜀

        FVector OExt = Other->GetWorldExtent();   // 상대 가구 크기
        FVector OC   = Other->GetActorLocation(); // 상대 가구 중심
        FVector OMin = OC - OExt;
        FVector OMax = OC + OExt;

        // AABB 겹침 판정: X, Y, Z 세 축이 '모두' 겹쳐야 실제 충돌
        const bool bX = (MyMin.X < OMax.X) && (MyMax.X > OMin.X);
        const bool bY = (MyMin.Y < OMax.Y) && (MyMax.Y > OMin.Y);
        const bool bZ = (MyMin.Z < OMax.Z) && (MyMax.Z > OMin.Z);

        if (bX && bY && bZ)
        {
            return true; // 한 가구라도 겹치면 즉시 종료
        }
    }
    return false; // 아무와도 안 겹침
}

bool AFurnitureActor::TryMoveTo(const FVector& NewLocation)
{
    FVector Clamped = NewLocation;

    const FVector Ext = GetWorldExtent();
    const float LimitX = RoomHalfXY - Ext.X - WallPadding;
    const float LimitY = RoomHalfXY - Ext.Y - WallPadding;

    // 방 중심 기준으로 클램프 (중심 ± Limit 범위 안으로 가둠)
    Clamped.X = FMath::Clamp(Clamped.X, RoomCenterX - LimitX, RoomCenterX + LimitX);
    Clamped.Y = FMath::Clamp(Clamped.Y, RoomCenterY - LimitY, RoomCenterY + LimitY);

    if (WouldOverlapAt(Clamped))
    {
        return false;
    }

    SetActorLocation(Clamped);
    return true;
}

bool AFurnitureActor::TryMoveBy(const FVector& Delta)
{
    return TryMoveTo(GetActorLocation() + Delta); // 현재 위치 + 이동량을 목표로
}

FVector AFurnitureActor::FindFreeSpawnLocation(const FVector& Desired) const
{
    const FVector Ext = GetWorldExtent();

    const float LimitX = RoomHalfXY - Ext.X - WallPadding;
    const float LimitY = RoomHalfXY - Ext.Y - WallPadding;

    // 후보가 방 안인지 검사 (방 중심 기준)
    auto InsideRoom = [&](const FVector& P) -> bool
    {
        return FMath::Abs(P.X - RoomCenterX) <= LimitX
            && FMath::Abs(P.Y - RoomCenterY) <= LimitY;
    };

    if (InsideRoom(Desired) && !WouldOverlapAt(Desired))
    {
        return Desired;
    }

    const float Step = FMath::Max(Ext.X, Ext.Y) * 1.2f + 20.f;
    const int32 MaxRings = 30;

    // 탐색 중심을 방 한가운데(1000,1000)로 → 안쪽부터 채움
    const FVector Center(RoomCenterX, RoomCenterY, Desired.Z);

    for (int32 Ring = 1; Ring <= MaxRings; ++Ring)
    {
        for (int32 dx = -Ring; dx <= Ring; ++dx)
        {
            for (int32 dy = -Ring; dy <= Ring; ++dy)
            {
                if (FMath::Abs(dx) != Ring && FMath::Abs(dy) != Ring) continue;

                FVector Candidate = Center + FVector(dx * Step, dy * Step, 0.f);

                if (InsideRoom(Candidate) && !WouldOverlapAt(Candidate))
                {
                    return Candidate;
                }
            }
        }
    }

    return Center; // 못 찾으면 방 중앙
}

AFurnitureActor::AFurnitureActor()
{
    PrimaryActorTick.bCanEverTick = false;

    // ── Collision box as root ─────────────────────────────────────────────────
    CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
    RootComponent = CollisionBox;
    CollisionBox->SetBoxExtent(FVector(50.0f, 50.0f, 50.0f));
    CollisionBox->SetCollisionProfileName(TEXT("BlockAll"));

    // ── Mesh ──────────────────────────────────────────────────────────────────
    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
    MeshComponent->SetupAttachment(RootComponent);

    // 기본 큐브 — 커스텀 모델이 없는 카테고리의 폴백용
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
        TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMesh.Succeeded())
    {
        MeshComponent->SetStaticMesh(CubeMesh.Object);
    }

    // 메시는 충돌 없음 — 충돌은 BoxComponent가 전담
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // 물리 시뮬레이션 없음 — 가구는 놓은 자리에 고정
    CollisionBox->SetSimulatePhysics(false);
    CollisionBox->SetEnableGravity(false);
}

// ── BeginPlay ─────────────────────────────────────────────────────────────────

void AFurnitureActor::BeginPlay()
{
    Super::BeginPlay();
}

// ── InitWithDimensions ────────────────────────────────────────────────────────

void AFurnitureActor::InitWithDimensions(const FFurnitureDimensions& Dims,
                                         UStaticMesh* MeshOverride)
{
    StoredDimensions = Dims;

    const float LengthCm = Dims.Length.MidCm();
    const float WidthCm = Dims.Width.MidCm();
    const float HeightCm = Dims.Height.MidCm();

    UE_LOG(LogTemp, Log,
        TEXT("FurnitureActor: Spawning '%s' at %.0f x %.0f x %.0f cm (%s mesh)"),
        *Dims.ItemName, LengthCm, WidthCm, HeightCm,
        MeshOverride ? TEXT("custom") : TEXT("cube"));

    // 커스텀 모델이 있으면 그걸로 교체, 없으면 생성자에서 지정한 큐브 유지
    if (MeshOverride)
    {
        MeshComponent->SetStaticMesh(MeshOverride);
    }

    ApplyMeshAndScale(MeshComponent->GetStaticMesh(), LengthCm, WidthCm, HeightCm);
    ApplyCategoryColor(Dims.Category);
}

// ── ApplyMeshAndScale ─────────────────────────────────────────────────────────
// 어떤 메시든 (큐브든 Blender 모델이든) 목표 치수에 맞게 스케일하고
// 충돌 박스 중앙에 정렬한다. 모델 원본 크기와 피벗 위치에 무관하게 동작.

void AFurnitureActor::ApplyMeshAndScale(UStaticMesh* Mesh,
    float LengthCm, float WidthCm, float HeightCm)
{
    // 최소 크기 강제
    LengthCm = FMath::Max(LengthCm, 20.0f);
    WidthCm = FMath::Max(WidthCm, 20.0f);
    HeightCm = FMath::Max(HeightCm, 20.0f);

    if (Mesh)
    {
        // ── 모델의 원본 크기 측정 ──────────────────────────────────────────────
        // GetBounds().BoxExtent는 "절반 크기"이므로 x2 하면 전체 크기.
        // Blender에서 어떤 크기로 만들었든 여기서 실측되므로 자유롭게 모델링 가능.
        const FBoxSphereBounds MeshBounds = Mesh->GetBounds();
        const FVector NativeSize = MeshBounds.BoxExtent * 2.0f;

        // 0 나누기 방지 (완전히 평평한 축이 있는 비정상 메시 대비)
        const FVector SafeNative(
            FMath::Max(NativeSize.X, 1.0f),
            FMath::Max(NativeSize.Y, 1.0f),
            FMath::Max(NativeSize.Z, 1.0f));

        // 축별 배율 = 목표 크기 / 원본 크기
        const FVector Scale(
            LengthCm / SafeNative.X,
            WidthCm / SafeNative.Y,
            HeightCm / SafeNative.Z);

        MeshComponent->SetRelativeScale3D(Scale);

        // ── 피벗 보정 ─────────────────────────────────────────────────────────
        // Blender 모델은 피벗이 바닥 중앙일 수도, 임의 위치일 수도 있다.
        // bounds의 중심(Origin)이 충돌 박스의 정중앙(로컬 0,0,0)에 오도록
        // 메시를 반대 방향으로 밀어서 정렬한다. (배율 반영해서 계산)
        MeshComponent->SetRelativeLocation(-MeshBounds.Origin * Scale);
    }

    // 충돌 박스 크기 맞추기 (BoxExtent는 축별 절반 크기)
    CollisionBox->SetBoxExtent(FVector(
        LengthCm * 0.5f,
        WidthCm * 0.5f,
        HeightCm * 0.5f
    ));

    // 바닥면 위에 밑면이 닿도록 Z를 들어올림 (바닥 윗면 = Z 10)
// 바닥면 위에 밑면이 정확히 닿도록 Z 보정 (실측 높이 기준)
    const float FloorSurfaceZ = 10.0f;

    FVector Origin, Extent;
    GetActorBounds(false, Origin, Extent); // 스케일·피벗 반영된 실제 월드 크기

    FVector Loc = GetActorLocation();
    Loc.Z = FloorSurfaceZ + Extent.Z; // 실측 반높이만큼 올림 → 밑면이 바닥에 딱
    SetActorLocation(Loc);
}

// ── ApplyCategoryColor ────────────────────────────────────────────────────────
// 카테고리 색을 모든 머티리얼 슬롯에 입힌다.
// 커스텀 모델도 같은 방식으로 칠해서, 선택 하이라이트(노란색)가
// 기존 코드(슬롯 0의 MID에 Color 파라미터 변경) 그대로 작동한다.

void AFurnitureActor::ApplyCategoryColor(EFurnitureCategory Category)
{
    UMaterial* BaseMaterial = LoadObject<UMaterial>(
        nullptr,
        TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

    if (!BaseMaterial) return;

    UMaterialInstanceDynamic* DynMat =
        UMaterialInstanceDynamic::Create(BaseMaterial, this);

    if (!DynMat) return;

    DynMat->SetVectorParameterValue(TEXT("Color"), GetColorForCategory(Category));

    // 커스텀 모델은 머티리얼 슬롯이 여러 개일 수 있으므로 전부 같은 MID로 교체
    const int32 NumMaterials = MeshComponent->GetNumMaterials();
    for (int32 i = 0; i < NumMaterials; i++)
    {
        MeshComponent->SetMaterial(i, DynMat);
    }
}

// ── GetColorForCategory ───────────────────────────────────────────────────────

FLinearColor AFurnitureActor::GetColorForCategory(EFurnitureCategory Category) const
{
    switch (Category)
    {
    case EFurnitureCategory::Chair: return FLinearColor(0.2f, 0.4f, 0.8f); // blue
    case EFurnitureCategory::Sofa: return FLinearColor(0.2f, 0.7f, 0.3f); // green
    case EFurnitureCategory::Table:   return FLinearColor(0.9f, 0.5f, 0.1f); // orange 
    case EFurnitureCategory::Other:   return FLinearColor(0.6f, 0.6f, 0.6f); // gray
    default:                          return FLinearColor(1.0f, 1.0f, 1.0f); // white
    }
}
