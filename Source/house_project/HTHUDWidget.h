// HTHUDWidget.h
// House Tetris

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FurnitureDimensions.h"
#include "HTHUDWidget.generated.h"

class UVerticalBox;
class UTextBlock;
class UButton;
class AFurnitureActor;
class Ahouse_projectGameMode;
class AHTCameraPawn;
class AHTPlayerController;

UCLASS()
class HOUSE_PROJECT_API UHTHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;

    UFUNCTION(BlueprintCallable, Category = "HUD")
    void RefreshFurnitureList();

    UFUNCTION(BlueprintCallable, Category = "HUD")
    void SetStatusText(const FString& Text);

    // 각 가구 행에 사용할 블루프린트 위젯 클래스 (WBP_FurnitureRow 지정)
    UPROPERTY(EditDefaultsOnly, Category = "HUD")
    TSubclassOf<class UHTFurnitureRowWidget> FurnitureRowWidgetClass;

protected:
    // ── 위젯 바인딩 (이름이 블루프린트 레이아웃과 정확히 일치해야 함) ──────────
    UPROPERTY(meta = (BindWidget))
    UVerticalBox* FurnitureListBox;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* StatusText;

    UPROPERTY(meta = (BindWidget))
    UButton* AddFurnitureButton;

    UPROPERTY(meta = (BindWidget))
    UButton* CameraButton1;

    UPROPERTY(meta = (BindWidget))
    UButton* CameraButton2;

    UPROPERTY(meta = (BindWidget))
    UButton* CameraButton3;

    UPROPERTY(meta = (BindWidget))
    UButton* CameraButton4;

    UPROPERTY(meta = (BindWidget))
    UButton* CameraButton5;

    // ── (Week 8) 저장/불러오기 버튼 ────────────────────────────────────────────
    // BindWidgetOptional: 블루프린트에 아직 버튼이 없어도 컴파일이 깨지지 않음.
    // WBP_HUD에 "SaveButton", "LoadButton" 이름으로 버튼 2개를 추가해야 동작.
    UPROPERTY(meta = (BindWidgetOptional))
    UButton* SaveButton;

    UPROPERTY(meta = (BindWidgetOptional))
    UButton* LoadButton;

private:
    UFUNCTION() void OnAddFurnitureClicked();
    UFUNCTION() void OnCamera1Clicked();
    UFUNCTION() void OnCamera2Clicked();
    UFUNCTION() void OnCamera3Clicked();
    UFUNCTION() void OnCamera4Clicked();
    UFUNCTION() void OnCamera5Clicked();

    // (Week 8) 저장/불러오기 핸들러
    UFUNCTION() void OnSaveClicked();
    UFUNCTION() void OnLoadClicked();

    void SwitchCamera(int32 Index);
    FString OpenFilePicker();

    UFUNCTION()
    void OnGeminiResult(bool bSuccess, FFurnitureDimensions Dims, FString Error);

    Ahouse_projectGameMode* GetHouseGameMode() const;
    AHTCameraPawn* GetCameraPawn()    const;
    AHTPlayerController* GetHTController()  const;
};
