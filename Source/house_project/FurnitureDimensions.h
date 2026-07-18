// FurnitureDimensions.h
// House Tetris
//
// Data struct holding everything Gemini returns for one furniture item.
// Updated to include: category, shape description, min/max dimension ranges,
// and an error field for when Gemini cannot identify the item.

#pragma once

#include "CoreMinimal.h"
#include "FurnitureDimensions.generated.h"

// ── Item Category ─────────────────────────────────────────────────────────────
// Broad classification of the furniture type.
// "Unknown" is only set when Gemini explicitly cannot identify the item.

UENUM(BlueprintType)
enum class EFurnitureCategory : uint8
{
    Sofa        UMETA(DisplayName = "Sofa"),
    Chair       UMETA(DisplayName = "Chair"),
    Table       UMETA(DisplayName = "Table"),
    Other       UMETA(DisplayName = "Other")
};
// ── Dimension Range ───────────────────────────────────────────────────────────
// Instead of a single number, Gemini returns a min and max estimate.
// For planning purposes: use Min for tight fits, Max for safe clearance.

USTRUCT(BlueprintType)
struct FDimensionRange
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    float MinCm = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    float MaxCm = 0.0f;

    // Midpoint — convenient for spawning the 3D box at a typical size
    float MidCm() const { return (MinCm + MaxCm) * 0.5f; }

    FString ToString() const
    {
        return FString::Printf(TEXT("%.0f-%.0f cm"), MinCm, MaxCm);
    }
};

// ── Main Struct ───────────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct FFurnitureDimensions
{
    GENERATED_BODY()

    // ── Identification ────────────────────────────────────────────────────────

    // Human-readable name, e.g. "3-Seater Sofa", "Bookshelf", "Coffee Table"
    UPROPERTY(BlueprintReadOnly)
    FString ItemName;

    // Broad category enum
    UPROPERTY(BlueprintReadOnly)
    EFurnitureCategory Category = EFurnitureCategory::Other;

    // ── Dimensions (min/max ranges in centimetres) ────────────────────────────

    UPROPERTY(BlueprintReadOnly)
    FDimensionRange Length;   // front-to-back depth

    UPROPERTY(BlueprintReadOnly)
    FDimensionRange Width;    // left-to-right span

    UPROPERTY(BlueprintReadOnly)
    FDimensionRange Height;   // floor-to-top

    // ── Shape Description ─────────────────────────────────────────────────────

    // Free-text description of notable physical features:
    // legs, arms, curved back, cushions, shelves, drawers, dents, etc.
    // Used later for the UI info panel tooltip.
    UPROPERTY(BlueprintReadOnly)
    FString ShapeDescription;

    // ── Error Handling ────────────────────────────────────────────────────────

    // True when Gemini could not identify the item.
    // When true, prompt the user to retry with a clearer photo.
    UPROPERTY(BlueprintReadOnly)
    bool bIdentificationFailed = false;

    // Why Gemini could not identify the item (only set when bIdentificationFailed)
    UPROPERTY(BlueprintReadOnly)
    FString FailureReason;

    // ── Helpers ───────────────────────────────────────────────────────────────

    // Returns true if the struct was fully populated and is ready to use
    bool IsValid() const
    {
        return !bIdentificationFailed
            && !ItemName.IsEmpty()
            && Length.MaxCm > 0.0f
            && Width.MaxCm > 0.0f
            && Height.MaxCm > 0.0f;
    }

    // Full summary for the Output Log
    FString ToString() const
    {
        if (bIdentificationFailed)
        {
            return FString::Printf(
                TEXT("IDENTIFICATION FAILED: %s"), *FailureReason);
        }

        return FString::Printf(
            TEXT("Item: %s | Category: %s | L: %s | W: %s | H: %s | Shape: %s"),
            *ItemName,
            *StaticEnum<EFurnitureCategory>()->GetDisplayNameTextByValue(
                (int64)Category).ToString(),
            *Length.ToString(),
            *Width.ToString(),
            *Height.ToString(),
            *ShapeDescription
        );
    }
};