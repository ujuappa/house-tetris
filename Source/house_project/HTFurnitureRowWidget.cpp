// HTFurnitureRowWidget.cpp
// House Tetris

#include "HTFurnitureRowWidget.h"
#include "HTPlayerController.h"
#include "house_projectGameMode.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/WidgetBlueprintLibrary.h" // ★ (포커스 수정) SetFocusToGameViewport용

// ── NativeConstruct ───────────────────────────────────────────────────────────

void UHTFurnitureRowWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (SelectButton)
    {
        SelectButton->OnClicked.AddDynamic(this,
            &UHTFurnitureRowWidget::OnSelectClicked);
    }

    if (DeleteButton)
    {
        DeleteButton->OnClicked.AddDynamic(this,
            &UHTFurnitureRowWidget::OnDeleteClicked);
    }
}

// ── InitRow ───────────────────────────────────────────────────────────────────

void UHTFurnitureRowWidget::InitRow(AFurnitureActor* InActor)
{
    BoundActor = InActor;

    if (!InActor || !NameLabel) return;

    const FFurnitureDimensions& Dims = InActor->GetFurnitureDimensions();

// 이름만 표시 — 치수는 3D 화면으로 확인 가능하므로 리스트는 간결하게
    NameLabel->SetText(FText::FromString(Dims.ItemName));
}

// ── OnSelectClicked ───────────────────────────────────────────────────────────

void UHTFurnitureRowWidget::OnSelectClicked()
{
    if (!BoundActor) return;

    AHTPlayerController* PC = Cast<AHTPlayerController>(
        UGameplayStatics::GetPlayerController(GetWorld(), 0));

    if (PC)
    {
        PC->SelectFurniture(BoundActor);
    }

    // ★ (포커스 수정) 여기가 제일 중요한 지점.
    // 사용자 흐름이 "Select 클릭 → 곧바로 R키" 인데,
    // Select 버튼이 키보드 포커스를 가진 채로 두면 R이 버튼에 삼켜진다.
    // 클릭 처리 직후 포커스를 게임 뷰포트로 돌려서 R이 컨트롤러에 도달하게 한다.
    UWidgetBlueprintLibrary::SetFocusToGameViewport();
}

// ── OnDeleteClicked ───────────────────────────────────────────────────────────

void UHTFurnitureRowWidget::OnDeleteClicked()
{
    if (!BoundActor) return;

    Ahouse_projectGameMode* GM = Cast<Ahouse_projectGameMode>(
        UGameplayStatics::GetGameMode(GetWorld()));

    if (GM)
    {
        GM->RemoveFurniture(BoundActor);
        BoundActor = nullptr;

        // 이 행을 리스트에서 제거
        RemoveFromParent();
    }

    // ★ (포커스 수정) 삭제 후에도 포커스를 게임 뷰포트로 복귀
    UWidgetBlueprintLibrary::SetFocusToGameViewport();
}
