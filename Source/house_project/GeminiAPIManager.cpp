// GeminiAPIManager.cpp
// House Tetris

#include "GeminiAPIManager.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

static const FString GeminiEndpoint =
TEXT("https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash-lite:generateContent");

// ── RequestFurnitureDimensions ────────────────────────────────────────────────

void UGeminiAPIManager::RequestFurnitureDimensions(const FString& ImagePath,
    FOnDimensionsReceived Callback)
{
    // Strip surrounding quotes typed in the console
    FString CleanPath = ImagePath.TrimQuotes();

    // 1. Load API key
    const FString APIKey = LoadAPIKey();
    if (APIKey.IsEmpty())
    {
        UE_LOG(LogTemp, Error,
            TEXT("GeminiAPIManager: No API key found. "
                "Add GeminiAPIKey= to Config/DefaultGame.ini under [HouseTetris]"));
        Callback.ExecuteIfBound(false, FFurnitureDimensions(),
            TEXT("Missing API key in DefaultGame.ini"));
        return;
    }

    // 2. Read and encode the image
    const FString Base64Image = EncodeImageToBase64(CleanPath);
    if (Base64Image.IsEmpty())
    {
        UE_LOG(LogTemp, Error,
            TEXT("GeminiAPIManager: Could not read image at: %s"), *CleanPath);
        Callback.ExecuteIfBound(false, FFurnitureDimensions(),
            FString::Printf(TEXT("Could not read image: %s"), *CleanPath));
        return;
    }

    const FString MimeType = GetMimeType(CleanPath);

    // 3. Build and fire the HTTP request
    const FString RequestBody = BuildRequestBody(Base64Image, MimeType);
    const FString URL = FString::Printf(TEXT("%s?key=%s"), *GeminiEndpoint, *APIKey);

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest =
        FHttpModule::Get().CreateRequest();

    HttpRequest->SetURL(URL);
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    HttpRequest->SetContentAsString(RequestBody);

    TWeakObjectPtr<UGeminiAPIManager> WeakThis(this);
    HttpRequest->OnProcessRequestComplete().BindLambda(
        [WeakThis, Callback](FHttpRequestPtr Req, FHttpResponsePtr Res, bool bSuccess)
        {
            if (WeakThis.IsValid())
            {
                WeakThis->OnResponseReceived(Req, Res, bSuccess, Callback);
            }
        });

    HttpRequest->ProcessRequest();
    UE_LOG(LogTemp, Log,
        TEXT("GeminiAPIManager: Request sent for: %s"), *CleanPath);
}

// ── LoadAPIKey ────────────────────────────────────────────────────────────────

FString UGeminiAPIManager::LoadAPIKey() const
{
    FString APIKey;
    GConfig->GetString(TEXT("HouseTetris"), TEXT("GeminiAPIKey"),
        APIKey, GGameIni);
    return APIKey.TrimStartAndEnd();
}

// ── EncodeImageToBase64 ───────────────────────────────────────────────────────

FString UGeminiAPIManager::EncodeImageToBase64(const FString& ImagePath) const
{
    TArray<uint8> ImageBytes;
    if (!FFileHelper::LoadFileToArray(ImageBytes, *ImagePath))
    {
        return FString();
    }
    return FBase64::Encode(ImageBytes);
}

// ── GetMimeType ───────────────────────────────────────────────────────────────

FString UGeminiAPIManager::GetMimeType(const FString& ImagePath) const
{
    const FString Ext = FPaths::GetExtension(ImagePath).ToLower();
    if (Ext == TEXT("png")) return TEXT("image/png");
    return TEXT("image/jpeg");
}

// ── BuildRequestBody ──────────────────────────────────────────────────────────
// The prompt is structured in three parts:
//   1. Role — tells Gemini what kind of expert it is
//   2. Task — exactly what to analyze and return
//   3. Format — strict JSON schema with field-by-field instructions

FString UGeminiAPIManager::BuildRequestBody(const FString& Base64Image,
    const FString& MimeType) const
{
    const FString Prompt =
        // ── Role ──────────────────────────────────────────────────────────────
        TEXT("You are an expert furniture analyst and interior design assistant. ")
        TEXT("You have deep knowledge of standard furniture dimensions across all ")
        TEXT("categories including seating, storage, tables, and beds. ")

        // ── Task ──────────────────────────────────────────────────────────────
        TEXT("Analyze the furniture item in this image carefully. ")
        TEXT("Identify what the item is, its category, its physical shape characteristics, ")
        TEXT("and estimate its real-world dimensions based on typical sizes for that item type. ")
        TEXT("For dimensions, provide a realistic min-max range in centimetres ")
        TEXT("reflecting natural variation in that furniture type. ")
        TEXT("For shape, describe notable physical features such as: ")
        TEXT("number of legs (if visible), armrests, backrest style, ")
        TEXT("shelves or compartments, drawers, cushions, dents or damage, ")
        TEXT("and any attached or integrated secondary items (e.g. lamp on side table). ")

        // ── Failure instruction ───────────────────────────────────────────────
        TEXT("If the image does not clearly show a furniture item, or is too blurry, ")
        TEXT("too dark, or shows something that is not furniture, do NOT guess. ")
        TEXT("Instead set identification_failed to true and explain why in failure_reason. ")

        // ── Format ────────────────────────────────────────────────────────────
        TEXT("Respond ONLY with a single valid JSON object. ")
        TEXT("No markdown, no code fences, no explanation outside the JSON. ")
        TEXT("Use exactly this schema: ")
        TEXT("{")
        TEXT("\"identification_failed\": boolean, ")
        TEXT("\"failure_reason\": \"string or empty string if successful\", ")
        TEXT("\"item_name\": \"specific descriptive name e.g. 3-Seater Fabric Sofa\", ")
        TEXT("\"category\": must be EXACTLY one of: \"Sofa\", \"Chair\", \"Table\", \"Other\". - \"Sofa\": couches, loveseats, sectionals, multi-person upholstered seating - \"Chair\": single-person seating - dining chairs, office chairs, stools, armchairs - \"Table\": dining tables, desks, coffee tables, nightstands Do not use any other category value.")
        TEXT("\"length_min_cm\": number, ")
        TEXT("\"length_max_cm\": number, ")
        TEXT("\"width_min_cm\": number, ")
        TEXT("\"width_max_cm\": number, ")
        TEXT("\"height_min_cm\": number, ")
        TEXT("\"height_max_cm\": number, ")
        TEXT("\"shape_description\": \"2-3 sentence description of physical features\" ")
        TEXT("}");

    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);

    Writer->WriteObjectStart();
    Writer->WriteArrayStart(TEXT("contents"));
    Writer->WriteObjectStart();
    Writer->WriteArrayStart(TEXT("parts"));

    // Part 1: the image
    Writer->WriteObjectStart();
    Writer->WriteObjectStart(TEXT("inline_data"));
    Writer->WriteValue(TEXT("mime_type"), MimeType);
    Writer->WriteValue(TEXT("data"), Base64Image);
    Writer->WriteObjectEnd();
    Writer->WriteObjectEnd();

    // Part 2: the prompt
    Writer->WriteObjectStart();
    Writer->WriteValue(TEXT("text"), Prompt);
    Writer->WriteObjectEnd();

    Writer->WriteArrayEnd();
    Writer->WriteObjectEnd();
    Writer->WriteArrayEnd();
    Writer->WriteObjectEnd();

    Writer->Close();
    return OutputString;
}

// ── OnResponseReceived ────────────────────────────────────────────────────────

void UGeminiAPIManager::OnResponseReceived(FHttpRequestPtr Request,
    FHttpResponsePtr Response,
    bool bWasSuccessful,
    FOnDimensionsReceived Callback)
{
    if (!bWasSuccessful || !Response.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("GeminiAPIManager: Network error."));
        Callback.ExecuteIfBound(false, FFurnitureDimensions(),
            TEXT("Network error: no response received"));
        return;
    }

    const int32 StatusCode = Response->GetResponseCode();
    const FString ResponseBody = Response->GetContentAsString();

    if (StatusCode != 200)
    {
        UE_LOG(LogTemp, Error,
            TEXT("GeminiAPIManager: HTTP %d -- %s"), StatusCode, *ResponseBody);
        Callback.ExecuteIfBound(false, FFurnitureDimensions(),
            FString::Printf(TEXT("HTTP error %d -- check your API key and quota"),
                StatusCode));
        return;
    }

    FFurnitureDimensions Result;
    if (!ParseGeminiResponse(ResponseBody, Result))
    {
        UE_LOG(LogTemp, Error,
            TEXT("GeminiAPIManager: Parse failed. Raw response: %s"), *ResponseBody);
        Callback.ExecuteIfBound(false, FFurnitureDimensions(),
            TEXT("Failed to parse Gemini response"));
        return;
    }

    // Gemini identified a failure (blurry photo, not furniture, etc.)
    if (Result.bIdentificationFailed)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("GeminiAPIManager: Identification failed -- %s"),
            *Result.FailureReason);
        // We return bSuccess=false so the caller can show a retry prompt
        Callback.ExecuteIfBound(false, Result, Result.FailureReason);
        return;
    }

    UE_LOG(LogTemp, Log,
        TEXT("GeminiAPIManager: [SUCCESS] %s"), *Result.ToString());
    Callback.ExecuteIfBound(true, Result, TEXT(""));
}

// ── ParseGeminiResponse ───────────────────────────────────────────────────────

bool UGeminiAPIManager::ParseGeminiResponse(const FString& ResponseText,
    FFurnitureDimensions& OutDimensions) const
{
    // Step 1: Parse outer Gemini envelope
    TSharedPtr<FJsonObject> OuterJson;
    TSharedRef<TJsonReader<>> OuterReader =
        TJsonReaderFactory<>::Create(ResponseText);

    if (!FJsonSerializer::Deserialize(OuterReader, OuterJson) || !OuterJson.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("ParseGeminiResponse: Outer JSON invalid"));
        return false;
    }

    // Step 2: Drill to candidates[0].content.parts[0].text
    const TArray<TSharedPtr<FJsonValue>>* Candidates;
    if (!OuterJson->TryGetArrayField(TEXT("candidates"), Candidates) ||
        Candidates->Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("ParseGeminiResponse: No candidates"));
        return false;
    }

    const TSharedPtr<FJsonObject>* ContentObj;
    if (!(*Candidates)[0]->AsObject()->TryGetObjectField(TEXT("content"), ContentObj))
    {
        UE_LOG(LogTemp, Error, TEXT("ParseGeminiResponse: No content field"));
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* Parts;
    if (!(*ContentObj)->TryGetArrayField(TEXT("parts"), Parts) ||
        Parts->Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("ParseGeminiResponse: No parts field"));
        return false;
    }

    FString InnerText;
    if (!(*Parts)[0]->AsObject()->TryGetStringField(TEXT("text"), InnerText))
    {
        UE_LOG(LogTemp, Error, TEXT("ParseGeminiResponse: No text field"));
        return false;
    }

    // Step 3: Strip markdown fences if Gemini added them anyway
    InnerText.TrimStartAndEndInline();
    if (InnerText.StartsWith(TEXT("```")))
    {
        int32 FirstNewline = INDEX_NONE;
        InnerText.FindChar('\n', FirstNewline);
        if (FirstNewline != INDEX_NONE)
        {
            InnerText = InnerText.RightChop(FirstNewline + 1);
        }
        InnerText.RemoveFromEnd(TEXT("```"));
        InnerText.TrimStartAndEndInline();
    }

    UE_LOG(LogTemp, Log, TEXT("ParseGeminiResponse: Inner text = %s"), *InnerText);

    // Step 4: Parse the inner furniture JSON
    TSharedPtr<FJsonObject> FurnitureJson;
    TSharedRef<TJsonReader<>> InnerReader =
        TJsonReaderFactory<>::Create(InnerText);

    if (!FJsonSerializer::Deserialize(InnerReader, FurnitureJson) ||
        !FurnitureJson.IsValid())
    {
        UE_LOG(LogTemp, Error,
            TEXT("ParseGeminiResponse: Inner JSON invalid: %s"), *InnerText);
        return false;
    }

    // Step 5: Check identification_failed first
    bool bFailed = false;
    FurnitureJson->TryGetBoolField(TEXT("identification_failed"), bFailed);
    OutDimensions.bIdentificationFailed = bFailed;

    if (bFailed)
    {
        FurnitureJson->TryGetStringField(TEXT("failure_reason"),
            OutDimensions.FailureReason);
        return true; // parsed successfully — caller handles the failure flag
    }

    // Step 6: Extract all fields
    FurnitureJson->TryGetStringField(TEXT("item_name"), OutDimensions.ItemName);
    FurnitureJson->TryGetStringField(TEXT("shape_description"),
        OutDimensions.ShapeDescription);

    // Category
    FString CategoryString;
    FurnitureJson->TryGetStringField(TEXT("category"), CategoryString);
    OutDimensions.Category = ParseCategory(CategoryString);

    // Dimension ranges
    double LMin = 0, LMax = 0, WMin = 0, WMax = 0, HMin = 0, HMax = 0;
    FurnitureJson->TryGetNumberField(TEXT("length_min_cm"), LMin);
    FurnitureJson->TryGetNumberField(TEXT("length_max_cm"), LMax);
    FurnitureJson->TryGetNumberField(TEXT("width_min_cm"), WMin);
    FurnitureJson->TryGetNumberField(TEXT("width_max_cm"), WMax);
    FurnitureJson->TryGetNumberField(TEXT("height_min_cm"), HMin);
    FurnitureJson->TryGetNumberField(TEXT("height_max_cm"), HMax);

    OutDimensions.Length = { (float)LMin, (float)LMax };
    OutDimensions.Width = { (float)WMin, (float)WMax };
    OutDimensions.Height = { (float)HMin, (float)HMax };

    if (!OutDimensions.IsValid())
    {
        UE_LOG(LogTemp, Error,
            TEXT("ParseGeminiResponse: Incomplete result: %s"),
            *OutDimensions.ToString());
        return false;
    }

    return true;
}

// ── ParseCategory ─────────────────────────────────────────────────────────────
// Converts the category string Gemini returns into our enum.

EFurnitureCategory UGeminiAPIManager::ParseCategory(const FString& CategoryStr) const
{
    if (CategoryStr.Equals(TEXT("Sofa"), ESearchCase::IgnoreCase))  return EFurnitureCategory::Sofa;
    if (CategoryStr.Equals(TEXT("Chair"), ESearchCase::IgnoreCase)) return EFurnitureCategory::Chair;
    if (CategoryStr.Equals(TEXT("Table"), ESearchCase::IgnoreCase)) return EFurnitureCategory::Table;

    UE_LOG(LogTemp, Warning, TEXT("Unknown category '%s', falling back to Other"), *CategoryStr);
    return EFurnitureCategory::Other;
}