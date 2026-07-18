// HTCameraPawn.cpp
// House Tetris

#include "HTCameraPawn.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/StaticMeshActor.h"

// ── Room dimensions (must match your BSP room) ────────────────────────────────
// Room floor: 1000 x 1000 cm, walls at ±490
// Camera height from floor: 600cm (high enough to see the whole room)
// Camera corner offset: pulled slightly outside the wall line for a clean angle

static const float RoomHalfSize = 490.0f;  // wall inner edge
static const float CameraHeight = 400.0f;  // Z height above floor
static const float CornerOffset = 400.0f;  // how far inside the corner the cam sits
static const float TopDownHeight = 400.0f; // height for the overhead view

// ── Constructor ───────────────────────────────────────────────────────────────

AHTCameraPawn::AHTCameraPawn()
{
    PrimaryActorTick.bCanEverTick = false;

    // Simple scene root — the camera moves by SetActorLocationAndRotation
    USceneComponent* SceneRoot =
        CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    RootComponent = SceneRoot;

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(RootComponent);
    Camera->bUsePawnControlRotation = false;

    // Allow the mouse cursor to show (needed for UI clicks and floor clicks)
    bUseControllerRotationYaw = false;
    bUseControllerRotationPitch = false;
    bUseControllerRotationRoll = false;
}

// ── BeginPlay ─────────────────────────────────────────────────────────────────

void AHTCameraPawn::BeginPlay()
{
    Super::BeginPlay();
    AutoReceiveInput = EAutoReceiveInput::Player0;
    BuildCameraPresets();

    SwitchToCamera(0); // 검증된 프리셋 0번 각도에서 Orbit 시작
}

// ── BuildCameraPresets ────────────────────────────────────────────────────────
// Defines all 5 camera positions.
//
// Corner cameras look inward toward the room center (0, 0) at a 45-degree
// downward angle — like security cameras mounted at ceiling corners.
//
// FRotator(Pitch, Yaw, Roll)
//   Pitch: -45 = looking 45 degrees downward
//   Yaw:   angle to face room center from each corner
void AHTCameraPawn::BuildCameraPresets()
{
    CameraPresets.Empty();

    const float D = 1500.0f;  // 중심에서 수평 거리 (방 반경 1000 + 여유)
    const float H = 300.0f;  // 카메라 높이

    // Camera 0: 앞-오른쪽에서 비스듬히
    { FCameraPreset P; P.Label = TEXT("Corner View 1");
      P.Location = FVector( D,  D, H); P.Rotation = FRotator(-40, -135, 0);
      CameraPresets.Add(P); }

    // Camera 1: 앞-왼쪽
    { FCameraPreset P; P.Label = TEXT("Corner View 2");
      P.Location = FVector( D, 0, H); P.Rotation = FRotator(-40, 135, 0);
      CameraPresets.Add(P); }

    // Camera 2: 뒤-오른쪽
    { FCameraPreset P; P.Label = TEXT("Corner View 3");
      P.Location = FVector(0,  D, H); P.Rotation = FRotator(-40, -45, 0);
      CameraPresets.Add(P); }

    // Camera 3: 뒤-왼쪽
    { FCameraPreset P; P.Label = TEXT("Corner View 4");
      P.Location = FVector(100, 100, H); P.Rotation = FRotator(-40, 45, 0);
      CameraPresets.Add(P); }

    // Camera 4: 정수리 탑다운
    { FCameraPreset P; P.Label = TEXT("Top-Down");
      P.Location = FVector(1000, 1000, 300); P.Rotation = FRotator(-90, 0, 0);
      CameraPresets.Add(P); }
}

// ── SwitchToCamera ────────────────────────────────────────────────────────────
// Instantly moves the pawn to the preset position and rotation.
// Both keyboard (1-5) and UI buttons call this.

void AHTCameraPawn::SwitchToCamera(int32 Index)
{
    if (!CameraPresets.IsValidIndex(Index))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("HTCameraPawn: Invalid camera index %d"), Index);
        return;
    }

    ActiveCameraIndex = Index;
    const FCameraPreset& Preset = CameraPresets[Index];

    // ── 프리셋 위치로부터 Orbit 파라미터를 역산 ────────────────────────────
    // 이 프리셋 위치에서 Orbit이 이어서 돌 수 있도록 Yaw/Pitch/Distance를 맞춤.
    // 이렇게 하면 1~5로 잘 잡힌 각도에서 시작해 우클릭으로 자유 회전 가능.
    const FVector Offset = Preset.Location - OrbitCenter; // 중심에서 카메라까지 벡터
    OrbitDistance = Offset.Size();                        // 거리 = 벡터 길이

    // 수평 거리(XY 평면 투영)와 높이로부터 각도 복원
    const float HorizDist = FMath::Sqrt(Offset.X * Offset.X + Offset.Y * Offset.Y);
    OrbitYaw   = FMath::RadiansToDegrees(FMath::Atan2(Offset.Y, Offset.X));
    OrbitPitch = -FMath::RadiansToDegrees(FMath::Atan2(Offset.Z, HorizDist));

    // 계산된 파라미터로 카메라 배치 (프리셋과 동일한 뷰가 됨)
    UpdateOrbitCamera();

    UE_LOG(LogTemp, Log,
        TEXT("HTCameraPawn: Switched to camera %d (%s), Yaw=%.0f Pitch=%.0f Dist=%.0f"),
        Index, *Preset.Label, OrbitYaw, OrbitPitch, OrbitDistance);
}


// ── UpdateOrbitCamera ─────────────────────────────────────────────────────────
// Yaw/Pitch/Distance(구면 좌표)를 월드 위치로 변환해 카메라를 배치하고,
// 항상 방 중심(OrbitCenter)을 바라보게 회전을 맞춘다.

void AHTCameraPawn::UpdateOrbitCamera()
{
    const float YawRad   = FMath::DegreesToRadians(OrbitYaw);
    const float PitchRad = FMath::DegreesToRadians(OrbitPitch);

    const float CosP = FMath::Cos(PitchRad);
    FVector Offset;
    Offset.X = OrbitDistance * CosP * FMath::Cos(YawRad);
    Offset.Y = OrbitDistance * CosP * FMath::Sin(YawRad);
    Offset.Z = OrbitDistance * -FMath::Sin(PitchRad);

    const FVector CamLocation = OrbitCenter + Offset;
    const FRotator LookAt = (OrbitCenter - CamLocation).Rotation();

    UE_LOG(LogTemp, Warning, TEXT("Orbit cam at (%.0f, %.0f, %.0f) looking at center (%.0f, %.0f, %.0f)"),
        CamLocation.X, CamLocation.Y, CamLocation.Z,
        OrbitCenter.X, OrbitCenter.Y, OrbitCenter.Z);

    SetActorLocationAndRotation(CamLocation, LookAt);
}

// ── 마우스 입력 핸들러 ─────────────────────────────────────────────────────────

void AHTCameraPawn::OnMouseX(float Value)
{
    if (Value != 0.0f)
    {
        UE_LOG(LogTemp, Warning, TEXT("MouseX input: %.3f, Orbiting: %d"), Value, bIsOrbiting);
    }
    if (!bIsOrbiting || Value == 0.0f) return;
    OrbitYaw += Value * OrbitRotateSpeed;
    UpdateOrbitCamera();
}

void AHTCameraPawn::OnMouseY(float Value)
{
    if (!bIsOrbiting || Value == 0.0f) return;
    // 위로 드래그하면 위에서 내려다보게 (Pitch 감소), 범위 제한
    OrbitPitch = FMath::Clamp(
        OrbitPitch - Value * OrbitRotateSpeed,
        OrbitMinPitch, OrbitMaxPitch);
    UpdateOrbitCamera();
}

void AHTCameraPawn::OnMouseWheel(float Value)
{
    if (Value != 0.0f)
    {
        UE_LOG(LogTemp, Warning, TEXT("Wheel input: %.2f, Dist before: %.0f"), Value, OrbitDistance);
    }
    if (Value == 0.0f) return;
    OrbitDistance = FMath::Clamp(
        OrbitDistance - Value * OrbitZoomSpeed,
        OrbitMinDistance, OrbitMaxDistance);
    UpdateOrbitCamera();
}
// ── GetActiveCameraLabel ──────────────────────────────────────────────────────

FString AHTCameraPawn::GetActiveCameraLabel() const
{
    if (CameraPresets.IsValidIndex(ActiveCameraIndex))
    {
        return CameraPresets[ActiveCameraIndex].Label;
    }
    return TEXT("Unknown");
}

// ── SetupPlayerInputComponent ─────────────────────────────────────────────────
// Binds number keys 1-5 to camera switching.
// UI buttons are handled separately in the widget (next session).

void AHTCameraPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    PlayerInputComponent->BindAction(
        TEXT("Camera1"), IE_Pressed, this, &AHTCameraPawn::Input_Camera1);
    PlayerInputComponent->BindAction(
        TEXT("Camera2"), IE_Pressed, this, &AHTCameraPawn::Input_Camera2);
    PlayerInputComponent->BindAction(
        TEXT("Camera3"), IE_Pressed, this, &AHTCameraPawn::Input_Camera3);
    PlayerInputComponent->BindAction(
        TEXT("Camera4"), IE_Pressed, this, &AHTCameraPawn::Input_Camera4);
    PlayerInputComponent->BindAction(
        TEXT("Camera5"), IE_Pressed, this, &AHTCameraPawn::Input_Camera5);
    PlayerInputComponent->BindAxis(
        TEXT("Zoom"), this, &AHTCameraPawn::OnMouseWheel);
        // 우클릭 누르는 동안만 회전
    PlayerInputComponent->BindAction(
        TEXT("OrbitCam"), IE_Pressed, this, &AHTCameraPawn::OnRightMousePressed);
    PlayerInputComponent->BindAction(
        TEXT("OrbitCam"), IE_Released, this, &AHTCameraPawn::OnRightMouseReleased);

    // 마우스 이동
    PlayerInputComponent->BindAxis(
        TEXT("MouseX"), this, &AHTCameraPawn::OnMouseX);
    PlayerInputComponent->BindAxis(
        TEXT("MouseY"), this, &AHTCameraPawn::OnMouseY);
}