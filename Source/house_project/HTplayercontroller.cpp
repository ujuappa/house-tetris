// HTPlayerController.cpp
// House Tetris

#include "HTPlayerController.h"
#include "FurnitureActor.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "Blueprint/WidgetBlueprintLibrary.h" // 포커스 복귀용

// 선택 시 노란색, 해제 시 원래 색으로 복원
const FLinearColor AHTPlayerController::HighlightColor = FLinearColor(1.0f, 0.85f, 0.0f);
const FLinearColor AHTPlayerController::NormalColor = FLinearColor(1.0f, 1.0f, 1.0f);

// ── Constructor ───────────────────────────────────────────────────────────────

AHTPlayerController::AHTPlayerController()
{
    bShowMouseCursor = true;
    bEnableClickEvents = true;
    bEnableMouseOverEvents = true;
}

// ── BeginPlay ─────────────────────────────────────────────────────────────────

void AHTPlayerController::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogTemp, Warning, TEXT(">>> HTPlayerController active"));

    FInputModeGameAndUI InputMode;
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    InputMode.SetHideCursorDuringCapture(false);
    SetInputMode(InputMode);

    // 키보드 포커스를 게임 뷰포트에 부여 (R/WASD/QE가 UMG에 삼켜지지 않게)
    UWidgetBlueprintLibrary::SetFocusToGameViewport();
}

// ── SetupInputComponent ───────────────────────────────────────────────────────

void AHTPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    // 좌클릭 — 바닥 배치 (플랜에서 제외됐지만 무해하게 유지)
    InputComponent->BindAction(
        TEXT("LeftClick"), IE_Pressed,
        this, &AHTPlayerController::OnLeftClick);

    // R키 — 90도 회전
    InputComponent->BindAction(
        TEXT("RotateFurniture"), IE_Pressed,
        this, &AHTPlayerController::OnRotateFurniture);

    // WASD — 수평 이동
    InputComponent->BindAxis(
        TEXT("MoveFurnitureForward"),
        this, &AHTPlayerController::OnMoveFurnitureForward);

    InputComponent->BindAxis(
        TEXT("MoveFurnitureRight"),
        this, &AHTPlayerController::OnMoveFurnitureRight);

    // ── (신규) Q/E — 수직 이동 ─────────────────────────────────────────────────
    // Project Settings > Input > Axis Mappings에 추가 필요:
    //   MoveFurnitureUp : E = +1.0, Q = -1.0
    InputComponent->BindAxis(
        TEXT("MoveFurnitureUp"),
        this, &AHTPlayerController::OnMoveFurnitureUp);
}

// ── SelectFurniture ───────────────────────────────────────────────────────────

void AHTPlayerController::SelectFurniture(AFurnitureActor* Actor)
{
    ClearSelection();

    if (!IsValid(Actor)) return;

    SelectedActor = Actor;

    UStaticMeshComponent* Mesh =
        SelectedActor->FindComponentByClass<UStaticMeshComponent>();

    if (Mesh)
    {
        UMaterialInstanceDynamic* Mat =
            Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0));

        if (Mat)
        {
            Mat->GetVectorParameterValue(
                FMaterialParameterInfo(TEXT("Color")), OriginalColor);

            Mat->SetVectorParameterValue(TEXT("Color"), HighlightColor);
        }
    }

    UE_LOG(LogTemp, Log,
        TEXT("HTPlayerController: Selected '%s'. WASD=move, Q/E=down/up, R=rotate."),
        *Actor->GetFurnitureDimensions().ItemName);
}

// ── ClearSelection ────────────────────────────────────────────────────────────

void AHTPlayerController::ClearSelection()
{
    if (!IsValid(SelectedActor)) return;

    UStaticMeshComponent* Mesh =
        SelectedActor->FindComponentByClass<UStaticMeshComponent>();

    if (Mesh)
    {
        UMaterialInstanceDynamic* Mat =
            Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0));
        if (Mat)
        {
            Mat->SetVectorParameterValue(TEXT("Color"), OriginalColor);
        }
    }

    UE_LOG(LogTemp, Log,
        TEXT("HTPlayerController: Deselected '%s'"),
        *SelectedActor->GetFurnitureDimensions().ItemName);

    SelectedActor = nullptr;
}

// ── OnLeftClick ───────────────────────────────────────────────────────────────

void AHTPlayerController::OnLeftClick()
{
    if (!IsValid(SelectedActor)) return;

    FVector HitLocation;
    if (TraceFloor(HitLocation))
    {
        PlaceFurnitureAt(HitLocation);
    }
}

// ── OnRotateFurniture ─────────────────────────────────────────────────────────

void AHTPlayerController::OnRotateFurniture()
{
    if (!IsValid(SelectedActor)) return;

    SelectedActor->AddActorWorldRotation(FRotator(0.0f, 90.0f, 0.0f));

    UE_LOG(LogTemp, Log,
        TEXT("HTPlayerController: Rotated '%s' to Yaw %.0f"),
        *SelectedActor->GetFurnitureDimensions().ItemName,
        SelectedActor->GetActorRotation().Yaw);
}

// ── 이동 핸들러 ────────────────────────────────────────────────────────────────

void AHTPlayerController::OnMoveFurnitureForward(float Value)
{
    // 월드 X축 (W = +X, S = -X)
    NudgeSelected(FVector(1.0f, 0.0f, 0.0f), Value);
}

void AHTPlayerController::OnMoveFurnitureRight(float Value)
{
    // 월드 Y축 (D = +Y, A = -Y)
    NudgeSelected(FVector(0.0f, 1.0f, 0.0f), Value);
}

void AHTPlayerController::OnMoveFurnitureUp(float Value)
{
    // (신규) 월드 Z축 (E = +Z 위, Q = -Z 아래)
    NudgeSelected(FVector(0.0f, 0.0f, 1.0f), Value);
}
void AHTPlayerController::NudgeSelected(const FVector& WorldDirection, float Value)
{
    if (Value == 0.0f) return;
    if (!IsValid(SelectedActor)) return;

    const float DeltaSeconds = GetWorld()->GetDeltaSeconds();
    const FVector Offset = WorldDirection * (Value * FurnitureMoveSpeed * DeltaSeconds);

    // 목표 위치 = 현재 위치 + 이번 프레임 이동량
    FVector Target = SelectedActor->GetActorLocation() + Offset;

    // ── 바닥 뚫림 방지 (Q/E 수직 이동일 때만) ──────────────────────────────
    // 겹침 검사 '전에' Z를 먼저 보정해서, 최종 목표 위치로 판정하게 함
    if (WorldDirection.Z != 0.0f)
    {
        const float HalfHeight = SelectedActor->GetWorldExtent().Z; // 월드 기준 반높이
        const float MinCenterZ = FloorSurfaceZ + HalfHeight;        // 허용되는 최저 중심 높이
        Target.Z = FMath::Max(Target.Z, MinCenterZ);
    }

    // 겹치면 이동하지 않음 → 그 자리에 딱 멈춤 (테트리스 방식)
    SelectedActor->TryMoveTo(Target);
}

// ── TraceFloor ────────────────────────────────────────────────────────────────

bool AHTPlayerController::TraceFloor(FVector& OutHitLocation) const
{
    FVector WorldLocation;
    FVector WorldDirection;

    if (!DeprojectMousePositionToWorld(WorldLocation, WorldDirection))
    {
        return false;
    }

    const FVector TraceStart = WorldLocation;
    const FVector TraceEnd = WorldLocation + (WorldDirection * 5000.0f);

    FHitResult HitResult;
    FCollisionQueryParams Params;
    if (IsValid(SelectedActor))
    {
        Params.AddIgnoredActor(SelectedActor);
    }

    const bool bHit = GetWorld()->LineTraceSingleByChannel(
        HitResult,
        TraceStart,
        TraceEnd,
        ECC_Visibility,
        Params
    );

    if (bHit)
    {
        OutHitLocation = HitResult.ImpactPoint;
        return true;
    }

    return false;
}

// ── PlaceFurnitureAt ──────────────────────────────────────────────────────────

void AHTPlayerController::PlaceFurnitureAt(const FVector& FloorLocation)
{
    if (!IsValid(SelectedActor)) return;

    // 가구 밑면이 클릭한 바닥면에 닿도록 중심 높이 계산
    const float HalfHeight = SelectedActor->GetWorldExtent().Z;

    FVector Target = FloorLocation;
    Target.Z = FloorLocation.Z + HalfHeight;

    // 겹치면 배치 취소 (다른 가구 위에 겹쳐 놓이는 것 방지)
    if (SelectedActor->TryMoveTo(Target))
    {
        UE_LOG(LogTemp, Log,
            TEXT("HTPlayerController: Placed '%s' at (%.0f, %.0f, %.0f)"),
            *SelectedActor->GetFurnitureDimensions().ItemName,
            Target.X, Target.Y, Target.Z);
    }
    else
    {
        UE_LOG(LogTemp, Log,
            TEXT("HTPlayerController: Placement blocked — overlaps another item."));
    }

    ClearSelection();
}
