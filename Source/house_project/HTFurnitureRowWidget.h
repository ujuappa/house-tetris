// HTFurnitureRowWidget.h
// House Tetris
//
// One row in the furniture list. Owns its own Select and Delete buttons.
// Holds a pointer to the AFurnitureActor it represents.
// Because buttons use DECLARE_DYNAMIC_MULTICAST_DELEGATE, the handlers
// must be UFUNCTIONs on a UObject — this widget serves that purpose.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FurnitureActor.h"
#include "HTFurnitureRowWidget.generated.h"

class UTextBlock;
class UButton;

UCLASS()
class HOUSE_PROJECT_API UHTFurnitureRowWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // Call this immediately after creating the row to bind it to an actor
    void InitRow(AFurnitureActor* InActor);

    // The actor this row represents
    UPROPERTY()
    AFurnitureActor* BoundActor = nullptr;

protected:
    // ── Widget bindings (must match names in Blueprint layout) ────────────────
    UPROPERTY(meta = (BindWidget))
    UTextBlock* NameLabel;

    UPROPERTY(meta = (BindWidget))
    UButton* SelectButton;

    UPROPERTY(meta = (BindWidget))
    UButton* DeleteButton;

    virtual void NativeConstruct() override;

private:
    UFUNCTION()
    void OnSelectClicked();

    UFUNCTION()
    void OnDeleteClicked();
};