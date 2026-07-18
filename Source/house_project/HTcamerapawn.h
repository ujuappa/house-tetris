// HTCameraPawn.h
// House Tetris
//
// A simple pawn with 5 fixed camera positions around the room.
// No movement — the camera snaps between preset viewpoints.
//
// Camera layout (top-down view of room):
//
//   Cam1 ─────────── Cam2
//    |                 |
//    |    (room)       |
//    |                 |
//   Cam3 ─────────── Cam4
//
//   Cam5 = directly overhead (top-down)
//
// Switch via:
//   Keyboard: 1, 2, 3, 4, 5
//   On-screen buttons: built in the UI widget (next session)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "HTCameraPawn.generated.h"

class UCameraComponent;

// ── Camera Preset ─────────────────────────────────────────────────────────────
// Stores the world position and rotation for one fixed viewpoint

USTRUCT(BlueprintType)
struct FCameraPreset
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector  Location = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FRotator Rotation = FRotator::ZeroRotator;

    // Label shown on the UI button, e.g. "Top-Left Corner"
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString  Label = TEXT("Camera");
};

// ─────────────────────────────────────────────────────────────────────────────

UCLASS()
class HOUSE_PROJECT_API AHTCameraPawn : public APawn
{
    GENERATED_BODY()

public:
    AHTCameraPawn();

    // ── Switch Camera ─────────────────────────────────────────────────────────
    // Index 0-4: TopLeft, TopRight, BottomLeft, BottomRight, TopDown
    // Called by both keyboard input and UI buttons

    UFUNCTION(BlueprintCallable, Category = "Camera")
    void SwitchToCamera(int32 Index);

    // Label of the currently active camera (e.g. "Top-Left Corner")
    UFUNCTION(BlueprintCallable, Category = "Camera")
    FString GetActiveCameraLabel() const;

    // Index of currently active camera (0-4)
    UFUNCTION(BlueprintCallable, Category = "Camera")
    int32 GetActiveCameraIndex() const { return ActiveCameraIndex; }

    // Exposed so the UI widget can read labels to populate button text
    UFUNCTION(BlueprintCallable, Category = "Camera")
    TArray<FCameraPreset> GetCameraPresets() const { return CameraPresets; }

protected:
    virtual void BeginPlay() override;
    virtual void SetupPlayerInputComponent(
        UInputComponent* PlayerInputComponent) override;

private:
    // ── Components ────────────────────────────────────────────────────────────
    UPROPERTY(VisibleAnywhere, Category = "Camera")
    UCameraComponent* Camera;
    // ── Orbit 카메라 상태 ──────────────────────────────────────────────────
    // 방 중심을 바라보며 구(sphere) 위를 도는 카메라.
    // 세 값으로 위치가 완전히 결정됨: Yaw(좌우), Pitch(상하), Distance(거리)
    FVector OrbitCenter = FVector(1000.0f, 1000.0f, 100.0f); // 방 중심 (바라볼 지점)
    float OrbitYaw      = 45.0f;    // 좌우 각도 (도)
    float OrbitPitch    = -45.0f;   // 상하 각도 (도, 음수 = 내려다봄)
    float OrbitDistance = 1000.0f;  // 중심에서 카메라까지 거리 (cm)

    bool  bIsOrbiting   = false;    // 우클릭 누르는 중인지

    // Orbit 조작 감도 / 한계
    static constexpr float OrbitRotateSpeed = 0.3f;   // 마우스 1픽셀당 회전 각도
    static constexpr float OrbitZoomSpeed   = 150.0f; // 휠 1클릭당 거리 변화
    static constexpr float OrbitMinDistance = 400.0f; // 최소 줌 거리
    static constexpr float OrbitMaxDistance = 2000.0f;// 최대 줌 거리
    static constexpr float OrbitMinPitch    = -85.0f; // 너무 수직으로 뒤집히지 않게
    static constexpr float OrbitMaxPitch    = -10.0f; // 너무 수평으로 눕지 않게

    // Yaw/Pitch/Distance로부터 카메라 위치·회전을 다시 계산해 적용
    void UpdateOrbitCamera();

    // 입력 핸들러
    void OnRightMousePressed()  { bIsOrbiting = true; }
    void OnRightMouseReleased() { bIsOrbiting = false; }
    void OnMouseX(float Value);
    void OnMouseY(float Value);
    void OnMouseWheel(float Value);
    // ── State ─────────────────────────────────────────────────────────────────
    TArray<FCameraPreset> CameraPresets;
    int32 ActiveCameraIndex = 0;

    // Builds the 5 hardcoded presets based on room dimensions
    void BuildCameraPresets();

    // One handler per key — delegates to SwitchToCamera()
    void Input_Camera1() { SwitchToCamera(0); }
    void Input_Camera2() { SwitchToCamera(1); }
    void Input_Camera3() { SwitchToCamera(2); }
    void Input_Camera4() { SwitchToCamera(3); }
    void Input_Camera5() { SwitchToCamera(4); }
};