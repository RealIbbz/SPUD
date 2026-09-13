#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "TestSaveObject.h"
#include "ReferenceSaveObject.generated.h"

USTRUCT()
struct FTestReferenceStruct
{
	GENERATED_BODY()
	UPROPERTY() TObjectPtr<UTexture2D> Texture = nullptr;
	UPROPERTY() TSoftObjectPtr<UTexture2D> SoftTexture;
	UPROPERTY() FDataTableRowHandle Row;
};

UCLASS()
class UTestReferenceSaveObject : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY(SaveGame) TObjectPtr<UTexture2D> Texture = nullptr;
	UPROPERTY(SaveGame) TObjectPtr<UTexture2D> NullTexture = nullptr;
	UPROPERTY(SaveGame) TObjectPtr<UObject> Object = nullptr;
	UPROPERTY(SaveGame) TSoftObjectPtr<UTexture2D> SoftTexture;
	UPROPERTY(SaveGame) TSoftClassPtr<AActor> SoftClass;
	UPROPERTY(SaveGame) FTestReferenceStruct References;
	UPROPERTY(SaveGame) TArray<TObjectPtr<UTexture2D>> Textures;
	UPROPERTY(SaveGame) TArray<TSoftObjectPtr<UTexture2D>> SoftTextures;
	UPROPERTY(SaveGame) TArray<FTestReferenceStruct> Structs;
	UPROPERTY(SaveGame) TMap<FString, FTestReferenceStruct> Map;
	UPROPERTY(SaveGame) int32 Health = 0;
};
