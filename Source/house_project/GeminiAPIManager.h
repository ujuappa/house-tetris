// GeminiAPIManager.h
// House Tetris

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Http.h"
#include "FurnitureDimensions.h"
#include "GeminiAPIManager.generated.h"

// Callback: fires when Gemini responds (success or failure)
DECLARE_DYNAMIC_DELEGATE_ThreeParams(
    FOnDimensionsReceived,
    bool, bSuccess,
    FFurnitureDimensions, Dimensions,
    FString, ErrorMessage
);

UCLASS()
class HOUSE_PROJECT_API UGeminiAPIManager : public UObject
{
    GENERATED_BODY()

public:
    // Main entry point — call this with an image path and a callback
    UFUNCTION(BlueprintCallable, Category = "House Tetris|Gemini")
    void RequestFurnitureDimensions(const FString& ImagePath,
        FOnDimensionsReceived Callback);

private:
    FString LoadAPIKey() const;
    FString EncodeImageToBase64(const FString& ImagePath) const;
    FString BuildRequestBody(const FString& Base64Image,
        const FString& MimeType) const;
    void    OnResponseReceived(FHttpRequestPtr Request,
        FHttpResponsePtr Response,
        bool bWasSuccessful,
        FOnDimensionsReceived Callback);
    bool    ParseGeminiResponse(const FString& ResponseText,
        FFurnitureDimensions& OutDimensions) const;
    FString GetMimeType(const FString& ImagePath) const;
    EFurnitureCategory ParseCategory(const FString& CategoryString) const;
};