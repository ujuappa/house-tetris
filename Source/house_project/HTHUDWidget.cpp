// HTHUDWidget.cpp
// House Tetris

#include "HTHUDWidget.h"
#include "HTFurnitureRowWidget.h"
#include "house_projectGameMode.h"
#include "HTCameraPawn.h"
#include "HTPlayerController.h"
#include "FurnitureActor.h"
#include "GeminiAPIManager.h"

#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"

#include "Kismet/GameplayStatics.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Engine/GameViewportClient.h"
#include "Blueprint/WidgetBlueprintLibrary.h" // 포커스 복귀용

// ── NativeConstruct ───────────────────────────────────────────────────────────

void UHTHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (AddFurnitureButton)
        AddFurnitureButton->OnClicked.AddDynamic(
            this, &UHTHUDWidget::OnAddFurnitureClicked);

    if (CameraButton1)
        CameraButton1->OnClicked.AddDynamic(this, &UHTHUDWidget::OnCamera1Clicked);
    if (CameraButton2)
        CameraButton2->OnClicked.AddDynamic(this, &UHTHUDWidget::OnCamera2Clicked);
    if (CameraButton3)
        CameraButton3->OnClicked.AddDynamic(this, &UHTHUDWidget::OnCamera3Clicked);
    if (CameraButton4)
        CameraButton4->OnClicked.AddDynamic(this, &UHTHUDWidget::OnCamera4Clicked);
    if (CameraButton5)
        CameraButton5->OnClicked.AddDynamic(this, &UHTHUDWidget::OnCamera5Clicked);

    // ── (Week 8) 저장/불러오기 버튼 바인딩 ─────────────────────────────────────
    // BindWidgetOptional이라 버튼이 아직 없으면 nullptr — if 체크로 안전하게 통과
    if (SaveButton)
        SaveButton->OnClicked.AddDynamic(this, &UHTHUDWidget::OnSaveClicked);
    if (LoadButton)
        LoadButton->OnClicked.AddDynamic(this, &UHTHUDWidget::OnLoadClicked);

    RefreshFurnitureList();
    SetStatusText(TEXT("Ready. Click 'Add Furniture' to begin."));
}

// ── Camera handlers ───────────────────────────────────────────────────────────

void UHTHUDWidget::OnCamera1Clicked() { SwitchCamera(0); }
void UHTHUDWidget::OnCamera2Clicked() { SwitchCamera(1); }
void UHTHUDWidget::OnCamera3Clicked() { SwitchCamera(2); }
void UHTHUDWidget::OnCamera4Clicked() { SwitchCamera(3); }
void UHTHUDWidget::OnCamera5Clicked() { SwitchCamera(4); }

void UHTHUDWidget::SwitchCamera(int32 Index)
{
    AHTCameraPawn* Pawn = GetCameraPawn();
    if (Pawn)
    {
        Pawn->SwitchToCamera(Index);
        SetStatusText(FString::Printf(
            TEXT("Camera: %s"), *Pawn->GetActiveCameraLabel()));
    }

    // 클릭 후 키보드 포커스를 게임 뷰포트로 복귀
    UWidgetBlueprintLibrary::SetFocusToGameViewport();
}

// ── (Week 8) OnSaveClicked ────────────────────────────────────────────────────

void UHTHUDWidget::OnSaveClicked()
{
    Ahouse_projectGameMode* GM = GetHouseGameMode();
    if (!GM)
    {
        SetStatusText(TEXT("Error: GameMode not found."));
        return;
    }

    const int32 Count = GM->GetAllFurniture().Num();
    if (GM->SaveLayoutToJson())
    {
        SetStatusText(FString::Printf(
            TEXT("Layout saved (%d items)."), Count));
    }
    else
    {
        SetStatusText(TEXT("Save failed. Check Output Log."));
    }

    UWidgetBlueprintLibrary::SetFocusToGameViewport();
}

// ── (Week 8) OnLoadClicked ────────────────────────────────────────────────────

void UHTHUDWidget::OnLoadClicked()
{
    Ahouse_projectGameMode* GM = GetHouseGameMode();
    if (!GM)
    {
        SetStatusText(TEXT("Error: GameMode not found."));
        return;
    }

    if (GM->LoadLayoutFromJson())
    {
        // 불러온 가구들로 리스트 다시 채우기
        RefreshFurnitureList();
        SetStatusText(FString::Printf(
            TEXT("Layout loaded (%d items)."), GM->GetAllFurniture().Num()));
    }
    else
    {
        SetStatusText(TEXT("Load failed. No save file yet?"));
    }

    UWidgetBlueprintLibrary::SetFocusToGameViewport();
}

// ── OnAddFurnitureClicked ─────────────────────────────────────────────────────

void UHTHUDWidget::OnAddFurnitureClicked()
{
    SetStatusText(TEXT("Opening file picker..."));

    const FString ImagePath = OpenFilePicker();
    if (ImagePath.IsEmpty())
    {
        SetStatusText(TEXT("Cancelled."));
        UWidgetBlueprintLibrary::SetFocusToGameViewport();
        return;
    }

    SetStatusText(TEXT("Scanning photo with Gemini AI..."));

    Ahouse_projectGameMode* GM = GetHouseGameMode();
    if (!GM || !GM->GeminiManager)
    {
        SetStatusText(TEXT("Error: GameMode not found."));
        UWidgetBlueprintLibrary::SetFocusToGameViewport();
        return;
    }

    FOnDimensionsReceived Callback;
    Callback.BindDynamic(this, &UHTHUDWidget::OnGeminiResult);
    GM->GeminiManager->RequestFurnitureDimensions(ImagePath, Callback);

    UWidgetBlueprintLibrary::SetFocusToGameViewport();
}

// ── OpenFilePicker ────────────────────────────────────────────────────────────

FString UHTHUDWidget::OpenFilePicker()
{
    IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
    if (!DesktopPlatform) return FString();

    void* ParentWindowHandle = nullptr;
    if (GEngine && GEngine->GameViewport)
    {
        TSharedPtr<SWindow> ParentWindow = GEngine->GameViewport->GetWindow();
        if (ParentWindow.IsValid())
        {
            ParentWindowHandle =
                ParentWindow->GetNativeWindow()->GetOSWindowHandle();
        }
    }

    TArray<FString> SelectedFiles;
    const bool bOpened = DesktopPlatform->OpenFileDialog(
        ParentWindowHandle,
        TEXT("Select Furniture Photo"),
        FPaths::GetProjectFilePath(),
        TEXT(""),
        TEXT("Image Files|*.jpg;*.jpeg;*.png"),
        EFileDialogFlags::None,
        SelectedFiles
    );

    if (bOpened && SelectedFiles.Num() > 0)
    {
        return SelectedFiles[0];
    }

    return FString();
}

// ── OnGeminiResult ────────────────────────────────────────────────────────────

void UHTHUDWidget::OnGeminiResult(bool bSuccess,
    FFurnitureDimensions Dims,
    FString Error)
{
    if (!bSuccess)
    {
        SetStatusText(Dims.bIdentificationFailed
            ? FString::Printf(TEXT("Could not identify item: %s. Try a clearer photo."), *Error)
            : FString::Printf(TEXT("API Error: %s"), *Error));
        return;
    }

    Ahouse_projectGameMode* GM = GetHouseGameMode();
    if (GM)
    {
        GM->SpawnFurniture(Dims);
        RefreshFurnitureList();
        SetStatusText(FString::Printf(
            TEXT("'%s' added! Select it from the list to place it."),
            *Dims.ItemName));
    }
}

// ── RefreshFurnitureList ──────────────────────────────────────────────────────

void UHTHUDWidget::RefreshFurnitureList()
{
    if (!FurnitureListBox) return;
    FurnitureListBox->ClearChildren();

    Ahouse_projectGameMode* GM = GetHouseGameMode();
    if (!GM) return;

    const TArray<AFurnitureActor*>& AllFurniture = GM->GetAllFurniture();

    if (AllFurniture.Num() == 0)
    {
        UTextBlock* EmptyText = NewObject<UTextBlock>(this);
        EmptyText->SetText(FText::FromString(TEXT("No furniture added yet.")));
        FurnitureListBox->AddChild(EmptyText);
        return;
    }

    if (!FurnitureRowWidgetClass)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("HTHUDWidget: FurnitureRowWidgetClass not set. "
                "Assign WBP_FurnitureRow in the Blueprint Details panel."));
        return;
    }

    for (AFurnitureActor* Actor : AllFurniture)
    {
        if (!Actor) continue;

        UHTFurnitureRowWidget* Row = CreateWidget<UHTFurnitureRowWidget>(
            GetOwningPlayer(), FurnitureRowWidgetClass);

        if (Row)
        {
            Row->InitRow(Actor);
            FurnitureListBox->AddChild(Row);
        }
    }
}

// ── SetStatusText ─────────────────────────────────────────────────────────────

void UHTHUDWidget::SetStatusText(const FString& Text)
{
    if (StatusText)
    {
        StatusText->SetText(FText::FromString(Text));
    }
    UE_LOG(LogTemp, Log, TEXT("HUD: %s"), *Text);
}

// ── Helpers ───────────────────────────────────────────────────────────────────

Ahouse_projectGameMode* UHTHUDWidget::GetHouseGameMode() const
{
    return Cast<Ahouse_projectGameMode>(
        UGameplayStatics::GetGameMode(GetWorld()));
}

AHTCameraPawn* UHTHUDWidget::GetCameraPawn() const
{
    return Cast<AHTCameraPawn>(
        UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
}

AHTPlayerController* UHTHUDWidget::GetHTController() const
{
    return Cast<AHTPlayerController>(
        UGameplayStatics::GetPlayerController(GetWorld(), 0));
}
