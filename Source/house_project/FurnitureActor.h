// FurnitureActor.h
// House Tetris
//
// 방 안의 가구 하나를 나타내는 Actor.
//
// (신규) 커스텀 모델 지원:
//   - InitWithDimensions에 UStaticMesh*를 넘기면 그 모델을 사용
//   - 모델의 원본 크기를 측정해 Gemini 치수에 맞게 자동 스케일
//   - 피벗 위치와 무관하게 충돌 박스 중앙에 자동 정렬
//   - 모델이 없으면(nullptr) 기존 큐브로 폴백

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FurnitureDimensions.h"
#include "FurnitureActor.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class UStaticMesh;

UCLASS()
class HOUSE_PROJECT_API AFurnitureActor : public AActor
{
    GENERATED_BODY()

public:

    AFurnitureActor();
    // 방 안쪽 경계 (바닥 중심 0,0 기준, 벽 안쪽면까지의 절반 거리, cm)
    // Floor Scale 20 = 2000cm 이므로 절반은 1000cm    // 방 중심 좌표 (바닥 한가운데의 월드 XY). Floor가 (1000,1000) 중심이면 여기에 맞춤.
    static constexpr float RoomCenterX = 1000.0f;
    static constexpr float RoomCenterY = 1000.0f;

    // 방 안쪽 경계 (중심에서 벽 안쪽면까지의 절반 거리, cm). 20m 방이면 1000.
    static constexpr float RoomHalfXY = 1000.0f;

    // 벽 안쪽 여유 (cm)
    static constexpr float WallPadding = 5.0f;
    // ── Setup ─────────────────────────────────────────────────────────────────
    // 스폰 직후 호출 — 치수/색 적용.
    // MeshOverride: 카테고리에 매핑된 커스텀 모델 (nullptr이면 기본 큐브 사용)
    void InitWithDimensions(const FFurnitureDimensions& Dims,
                            UStaticMesh* MeshOverride = nullptr);

    // ── Accessors ─────────────────────────────────────────────────────────────
    const FFurnitureDimensions& GetFurnitureDimensions() const { return StoredDimensions; }
    // 이 가구의 현재 월드 크기(반쪽 크기, half-extent). 회전이 반영된 실제 크기 기준.
    FVector GetWorldExtent() const;

    // TestCenter 위치에 이 가구를 놓으면 다른 가구와 겹치는지 검사
    bool WouldOverlapAt(const FVector& TestCenter) const;

    // NewLocation으로 이동 시도. 겹치면 이동하지 않고 false 반환.
    bool TryMoveTo(const FVector& NewLocation);

    // 현재 위치에서 Delta만큼 이동 시도.
    bool TryMoveBy(const FVector& Delta);

    // 스폰 시 겹치지 않는 가장 가까운 빈 자리를 찾아 반환.
    FVector FindFreeSpawnLocation(const FVector& Desired) const;
protected:
    // ── Components ────────────────────────────────────────────────────────────

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Furniture")
    UStaticMeshComponent* MeshComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Furniture")
    UBoxComponent* CollisionBox;

    virtual void BeginPlay() override;

private:
    // 메시 적용 + 목표 치수에 맞게 스케일/정렬
    void ApplyMeshAndScale(UStaticMesh* Mesh,
                           float LengthCm, float WidthCm, float HeightCm);

    void ApplyCategoryColor(EFurnitureCategory Category);
    FLinearColor GetColorForCategory(EFurnitureCategory Category) const;

    FFurnitureDimensions StoredDimensions;
};
