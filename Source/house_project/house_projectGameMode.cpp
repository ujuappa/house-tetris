// house_projectGameMode.cpp
// House Tetris

#include "house_projectGameMode.h"
#include "HTCameraPawn.h"
#include "HTPlayerController.h"
#include "GeminiAPIManager.h"
#include "FurnitureActor.h"
#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"

// JSON 직렬화용
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

Ahouse_projectGameMode::Ahouse_projectGameMode()
{
    DefaultPawnClass = AHTCameraPawn::StaticClass();
    PlayerControllerClass = AHTPlayerController::StaticClass();
}

void Ahouse_projectGameMode::BeginPlay()
{
    Super::BeginPlay();

    GeminiManager = NewObject<UGeminiAPIManager>(this);

    if (HUDWidgetClass)
    {
        HUDWidgetInstance = CreateWidget<UUserWidget>(
            GetWorld(), HUDWidgetClass);

        if (HUDWidgetInstance)
        {
            HUDWidgetInstance->AddToViewport();
            UE_LOG(LogTemp, Log, TEXT("HouseTetris: HUD widget created."));
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning,
            TEXT("HouseTetris: HUDWidgetClass not set. "
                "Open BP_HouseTetrisGameMode and assign WBP_HouseTetrisHUD."));
    }

    UE_LOG(LogTemp, Log,
        TEXT("HouseTetris: Ready. Press 1-5 to switch cameras."));
}

// ── SpawnFurniture ────────────────────────────────────────────────────────────
AFurnitureActor* Ahouse_projectGameMode::SpawnFurniture(const FFurnitureDimensions& Dims, bool bAvoidOverlap)
{
    if (!GetWorld()) return nullptr;

    const FVector  SpawnLocation(500.0f, 500.0f, 100.0f);
    const FRotator SpawnRotation(0.0f, 0.0f, 0.0f);

    AFurnitureActor* NewActor = GetWorld()->SpawnActorDeferred<AFurnitureActor>(
        AFurnitureActor::StaticClass(),
        FTransform(SpawnRotation, SpawnLocation));

    if (!NewActor)
    {
        UE_LOG(LogTemp, Error, TEXT("SpawnFurniture: Failed"));
        return nullptr;
    }

    UStaticMesh* MeshOverride = nullptr;
    if (UStaticMesh* const* Found = CategoryMeshes.Find(Dims.Category))
    {
        MeshOverride = *Found;
    }

    NewActor->InitWithDimensions(Dims, MeshOverride);
    NewActor->FinishSpawning(FTransform(SpawnRotation, SpawnLocation));

    // ── (신규) 겹치지 않는 빈 자리로 이동 ──────────────────────────────────────
    // 새 스폰만 실행. JSON 로드는 저장된 좌표를 그대로 써야 하므로 false로 건너뜀.
    // InitWithDimensions + FinishSpawning 이후라 크기·바닥높이가 확정된 상태 → extent 정확.
    if (bAvoidOverlap)
    {
        const FVector Free = NewActor->FindFreeSpawnLocation(NewActor->GetActorLocation());
        NewActor->SetActorLocation(Free);
    }

    SpawnedFurniture.Add(NewActor);

    UE_LOG(LogTemp, Log,
        TEXT("SpawnFurniture: '%s' spawned. Total: %d"),
        *Dims.ItemName, SpawnedFurniture.Num());

    return NewActor;
}

void Ahouse_projectGameMode::RemoveFurniture(AFurnitureActor* Actor)
{
    if (!Actor) return;
    SpawnedFurniture.Remove(Actor);
    Actor->Destroy();
}

// ── GetSaveFilePath ───────────────────────────────────────────────────────────

FString Ahouse_projectGameMode::GetSaveFilePath() const
{
    return FPaths::ProjectSavedDir() / TEXT("FurnitureLayout.json");
}

// ── SaveLayoutToJson ──────────────────────────────────────────────────────────

bool Ahouse_projectGameMode::SaveLayoutToJson()
{
    TArray<TSharedPtr<FJsonValue>> FurnitureArray;

    for (AFurnitureActor* Actor : SpawnedFurniture)
    {
        if (!IsValid(Actor)) continue;

        const FFurnitureDimensions& Dims = Actor->GetFurnitureDimensions();
        const FVector  Loc = Actor->GetActorLocation();
        const FRotator Rot = Actor->GetActorRotation();

        TSharedPtr<FJsonObject> Obj = MakeShareable(new FJsonObject());

        Obj->SetStringField(TEXT("item_name"), Dims.ItemName);
        Obj->SetStringField(TEXT("category"),
            StaticEnum<EFurnitureCategory>()->GetNameStringByValue((int64)Dims.Category));

        Obj->SetNumberField(TEXT("length_min_cm"), Dims.Length.MinCm);
        Obj->SetNumberField(TEXT("length_max_cm"), Dims.Length.MaxCm);
        Obj->SetNumberField(TEXT("width_min_cm"), Dims.Width.MinCm);
        Obj->SetNumberField(TEXT("width_max_cm"), Dims.Width.MaxCm);
        Obj->SetNumberField(TEXT("height_min_cm"), Dims.Height.MinCm);
        Obj->SetNumberField(TEXT("height_max_cm"), Dims.Height.MaxCm);

        Obj->SetStringField(TEXT("shape_description"), Dims.ShapeDescription);

        Obj->SetNumberField(TEXT("pos_x"), Loc.X);
        Obj->SetNumberField(TEXT("pos_y"), Loc.Y);
        Obj->SetNumberField(TEXT("pos_z"), Loc.Z);
        Obj->SetNumberField(TEXT("yaw"), Rot.Yaw);

        FurnitureArray.Add(MakeShareable(new FJsonValueObject(Obj)));
    }

    TSharedPtr<FJsonObject> Root = MakeShareable(new FJsonObject());
    Root->SetArrayField(TEXT("furniture"), FurnitureArray);

    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    if (!FJsonSerializer::Serialize(Root.ToSharedRef(), Writer))
    {
        UE_LOG(LogTemp, Error, TEXT("SaveLayout: JSON serialize failed"));
        return false;
    }

    const FString FilePath = GetSaveFilePath();
    if (!FFileHelper::SaveStringToFile(OutputString, *FilePath))
    {
        UE_LOG(LogTemp, Error, TEXT("SaveLayout: Failed to write %s"), *FilePath);
        return false;
    }

    UE_LOG(LogTemp, Log,
        TEXT("SaveLayout: Saved %d items to %s"),
        FurnitureArray.Num(), *FilePath);
    return true;
}

// ── LoadLayoutFromJson ────────────────────────────────────────────────────────

bool Ahouse_projectGameMode::LoadLayoutFromJson()
{
    const FString FilePath = GetSaveFilePath();

    FString JsonString;
    if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("LoadLayout: No save file at %s"), *FilePath);
        return false;
    }

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("LoadLayout: JSON parse failed"));
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* FurnitureArray = nullptr;
    if (!Root->TryGetArrayField(TEXT("furniture"), FurnitureArray))
    {
        UE_LOG(LogTemp, Error, TEXT("LoadLayout: 'furniture' array missing"));
        return false;
    }

    ClearAllFurniture();

    int32 LoadedCount = 0;
    for (const TSharedPtr<FJsonValue>& Value : *FurnitureArray)
    {
        const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
        if (!Value->TryGetObject(ObjPtr) || !ObjPtr->IsValid()) continue;
        const TSharedPtr<FJsonObject>& Obj = *ObjPtr;

        FFurnitureDimensions Dims;
        Dims.ItemName = Obj->GetStringField(TEXT("item_name"));
        Dims.ShapeDescription = Obj->GetStringField(TEXT("shape_description"));

        const FString CategoryStr = Obj->GetStringField(TEXT("category"));
        const int64 CategoryValue =
            StaticEnum<EFurnitureCategory>()->GetValueByNameString(CategoryStr);
        Dims.Category = (CategoryValue != INDEX_NONE)
            ? (EFurnitureCategory)CategoryValue
            : EFurnitureCategory::Other;

        Dims.Length.MinCm = Obj->GetNumberField(TEXT("length_min_cm"));
        Dims.Length.MaxCm = Obj->GetNumberField(TEXT("length_max_cm"));
        Dims.Width.MinCm = Obj->GetNumberField(TEXT("width_min_cm"));
        Dims.Width.MaxCm = Obj->GetNumberField(TEXT("width_max_cm"));
        Dims.Height.MinCm = Obj->GetNumberField(TEXT("height_min_cm"));
        Dims.Height.MaxCm = Obj->GetNumberField(TEXT("height_max_cm"));

        // SpawnFurniture가 카테고리 → 모델 매핑도 처리하므로
        // 불러온 가구도 자동으로 올바른 커스텀 모델로 복원된다
        AFurnitureActor* NewActor = SpawnFurniture(Dims, false); // 저장된 좌표 그대로 복원
        if (!NewActor) continue;

        const FVector SavedLocation(
            Obj->GetNumberField(TEXT("pos_x")),
            Obj->GetNumberField(TEXT("pos_y")),
            Obj->GetNumberField(TEXT("pos_z"))
        );
        const FRotator SavedRotation(
            0.0f,
            Obj->GetNumberField(TEXT("yaw")),
            0.0f
        );

        NewActor->SetActorLocationAndRotation(SavedLocation, SavedRotation);
        LoadedCount++;
    }

    UE_LOG(LogTemp, Log,
        TEXT("LoadLayout: Restored %d items from %s"),
        LoadedCount, *FilePath);
    return true;
}

// ── ClearAllFurniture ─────────────────────────────────────────────────────────

void Ahouse_projectGameMode::ClearAllFurniture()
{
    TArray<AFurnitureActor*> ToDestroy = SpawnedFurniture;
    for (AFurnitureActor* Actor : ToDestroy)
    {
        if (IsValid(Actor))
        {
            Actor->Destroy();
        }
    }
    SpawnedFurniture.Empty();
}
