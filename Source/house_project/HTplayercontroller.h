// HTPlayerController.h
// House Tetris
//
// 커스텀 플레이어 컨트롤러:
//   1. 마우스 커서 항상 표시
//   2. R키로 선택된 가구를 90도 회전
//   3. WASD로 선택된 가구를 수평 이동
//   4. (신규) Q/E로 선택된 가구를 수직 이동 (바닥 아래로는 안 내려감)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "HTPlayerController.generated.h"

class AFurnitureActor;

UCLASS()
class HOUSE_PROJECT_API AHTPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    AHTPlayerController();

    // ── Selection ─────────────────────────────────────────────────────────────

    UFUNCTION(BlueprintCallable, Category = "Selection")
    void SelectFurniture(AFurnitureActor* Actor);

    UFUNCTION(BlueprintCallable, Category = "Selection")
    void ClearSelection();

    UFUNCTION(BlueprintCallable, Category = "Selection")
    AFurnitureActor* GetSelectedFurniture() const { return SelectedActor; }

protected:
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;

private:
    // 좌클릭 시 호출 — 선택된 가구가 있으면 바닥에 배치
    // (현재 데모 플랜에서는 제외됐지만 코드는 무해하게 유지)
    void OnLeftClick();

    // R키 — 선택된 가구를 90도 Yaw 회전
    void OnRotateFurniture();

    // WASD 축 입력 — 매 프레임 호출, Value는 -1 ~ +1
    // MoveFurnitureForward: W = +1, S = -1  (월드 X축)
    // MoveFurnitureRight:   D = +1, A = -1  (월드 Y축)
    void OnMoveFurnitureForward(float Value);
    void OnMoveFurnitureRight(float Value);

    // (신규) Q/E 축 입력 — 수직 이동
    // MoveFurnitureUp: E = +1(위), Q = -1(아래)
    void OnMoveFurnitureUp(float Value);

    // 마우스 커서에서 바닥으로 라인 트레이스
    bool TraceFloor(FVector& OutHitLocation) const;

    // 선택된 가구를 주어진 바닥 위치로 이동
    void PlaceFurnitureAt(const FVector& FloorLocation);

    // 공통 이동 처리 — 방향 * 속도 * DeltaTime 만큼 이동
    // 수직 이동 시 바닥 아래로 내려가지 않도록 클램프
    void NudgeSelected(const FVector& WorldDirection, float Value);

    // 현재 선택된 가구 (nullptr = 선택 없음)
    UPROPERTY()
    AFurnitureActor* SelectedActor = nullptr;

    // 이동 속도 (cm/초). 에디터에서 조절 가능.
    UPROPERTY(EditAnywhere, Category = "Furniture Movement")
    float FurnitureMoveSpeed = 200.0f;

    // 바닥 윗면의 Z 좌표 (20cm BSP 바닥 기준). 수직 이동 클램프에 사용.
    UPROPERTY(EditAnywhere, Category = "Furniture Movement")
    float FloorSurfaceZ = 10.0f;

    // 강조 색상
    static const FLinearColor HighlightColor;
    static const FLinearColor NormalColor;

    // 선택 해제 시 복원할 원래 색상
    FLinearColor OriginalColor = FLinearColor::White;
};
