// house_projectGameMode.h
// House Tetris

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GeminiAPIManager.h"
#include "FurnitureDimensions.h"
#include "FurnitureActor.h"
#include "house_projectGameMode.generated.h"

class UStaticMesh;

UCLASS(minimalapi)
class Ahouse_projectGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    Ahouse_projectGameMode();

protected:
    virtual void BeginPlay() override;

public:
    // ── Gemini ────────────────────────────────────────────────────────────────
    UPROPERTY()
    UGeminiAPIManager* GeminiManager;

    // ── Spawn ─────────────────────────────────────────────────────────────────
    UFUNCTION(BlueprintCallable, Category = "Furniture")
    AFurnitureActor* SpawnFurniture(const FFurnitureDimensions& Dims, bool bAvoidOverlap = true);

    // ── Furniture Registry ────────────────────────────────────────────────────
    UFUNCTION(BlueprintCallable, Category = "Furniture")
    const TArray<AFurnitureActor*>& GetAllFurniture() const { return SpawnedFurniture; }

    UFUNCTION(BlueprintCallable, Category = "Furniture")
    void RemoveFurniture(AFurnitureActor* Actor);

    // ── JSON 저장/불러오기 ─────────────────────────────────────────────────────
    UFUNCTION(BlueprintCallable, Category = "SaveLoad")
    bool SaveLayoutToJson();

    UFUNCTION(BlueprintCallable, Category = "SaveLoad")
    bool LoadLayoutFromJson();

    // ── HUD Widget class ──────────────────────────────────────────────────────
    UPROPERTY(EditDefaultsOnly, Category = "HUD")
    TSubclassOf<class UUserWidget> HUDWidgetClass;

    // ── (신규) 카테고리 → 커스텀 모델 매핑 ─────────────────────────────────────
    // BP_HouseTetrisGameMode의 Class Defaults에서 채운다:
    //   Seating → SM_Sofa, Storage → SM_Shelf, Table → SM_Table, ...
    // 매핑이 없는 카테고리는 기본 큐브로 폴백되므로,
    // 모델을 하나씩 완성할 때마다 여기에 추가하면 된다. (코드 수정 불필요)
    UPROPERTY(EditDefaultsOnly, Category = "Furniture Models")
    TMap<EFurnitureCategory, UStaticMesh*> CategoryMeshes;

private:
    FString GetSaveFilePath() const;
    void ClearAllFurniture();

    UPROPERTY()
    TArray<AFurnitureActor*> SpawnedFurniture;

    UPROPERTY()
    class UUserWidget* HUDWidgetInstance;
};
