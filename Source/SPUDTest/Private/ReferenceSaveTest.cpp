#include "ReferenceSaveObject.h"
#include "Misc/AutomationTest.h"
#include "SpudState.h"
#include "SpudMemoryReaderWriter.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpudAssetReferenceTest, "SPUDTest.AssetReferences",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSpudAssetReferenceTest::RunTest(const FString& Parameters)
{
	UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture"));
	if (!TestNotNull(TEXT("Fixture asset loads"), Texture)) return false;
	auto* Saved = NewObject<UTestReferenceSaveObject>();
	Saved->Texture = Texture;
	Saved->Object = Texture; // A generic UObject property must also support assets.
	Saved->SoftTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/SPUDTest/Unloaded.Unloaded")));
	Saved->SoftClass = TSoftClassPtr<AActor>(FSoftObjectPath(TEXT("/Script/Engine.Actor")));
	Saved->References.Texture = Texture;
	Saved->References.SoftTexture = Saved->SoftTexture;
	auto* Table = NewObject<UDataTable>(CreatePackage(*FString::Printf(TEXT("/Temp/SPUDTest_%s"), *FGuid::NewGuid().ToString())),
		TEXT("Table"), RF_Public | RF_Standalone);
	Saved->References.Row.DataTable = Table;
	Saved->References.Row.RowName = TEXT("ExampleRow");
	Saved->Textures = {Texture, nullptr, Texture};
	Saved->SoftTextures = {Saved->SoftTexture, TSoftObjectPtr<UTexture2D>()};
	Saved->Structs = {Saved->References, FTestReferenceStruct()};
	Saved->Map.Add(TEXT("Filled"), Saved->References);
	Saved->Map.Add(TEXT("Empty"), FTestReferenceStruct());
	Saved->Health = 37;
	auto* State = NewObject<USpudState>();
	State->StoreGlobalObject(Saved, TEXT("References"));
	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes);
	State->SaveToArchive(Writer);
	TestFalse(TEXT("Archive write succeeds"), Writer.IsError());

	for (bool SlowPath : {false, true})
	{
		auto* RestoredState = NewObject<USpudState>();
		FMemoryReader Reader(Bytes);
		FSpudChunkedDataArchive ChunkedReader(Reader);
		RestoredState->SaveData.ReadFromArchive(ChunkedReader, true, FString());
		RestoredState->bTestRequireFastPath = !SlowPath;
		RestoredState->bTestRequireSlowPath = SlowPath;
		auto* Loaded = NewObject<UTestReferenceSaveObject>();
		Loaded->NullTexture = Texture;
		RestoredState->RestoreGlobalObject(Loaded, TEXT("References"));
		TestEqual(TEXT("Hard reference resolves to shared asset"), Loaded->Texture.Get(), Texture);
		TestEqual(TEXT("Generic object reference resolves to shared asset"), Loaded->Object.Get(), static_cast<UObject*>(Texture));
		TestNull(TEXT("Null reference clears defaults"), Loaded->NullTexture.Get());
		TestEqual(TEXT("Unloaded soft path survives"), Loaded->SoftTexture.ToSoftObjectPath(), Saved->SoftTexture.ToSoftObjectPath());
		TestFalse(TEXT("Soft reference remains unloaded"), Loaded->SoftTexture.IsValid());
		TestEqual(TEXT("Soft class path survives"), Loaded->SoftClass.ToSoftObjectPath(), Saved->SoftClass.ToSoftObjectPath());
		TestEqual(TEXT("Nested struct asset"), Loaded->References.Texture.Get(), Texture);
		TestEqual(TEXT("Row name survives"), Loaded->References.Row.RowName, Saved->References.Row.RowName);
		TestEqual(TEXT("Data table remains a shared asset"), Loaded->References.Row.DataTable.Get(), static_cast<const UDataTable*>(Table));
		TestTrue(TEXT("Asset array including null survives"), Loaded->Textures == Saved->Textures);
		TestTrue(TEXT("Soft array including null survives"), Loaded->SoftTextures == Saved->SoftTextures);
		if (TestEqual(TEXT("Struct array size"), Loaded->Structs.Num(), 2))
		{
			TestEqual(TEXT("Struct array asset"), Loaded->Structs[0].Texture.Get(), Texture);
			TestNull(TEXT("Struct array null"), Loaded->Structs[1].Texture.Get());
		}
		if (TestTrue(TEXT("Map entry exists"), Loaded->Map.Contains(TEXT("Filled"))))
		{
			TestEqual(TEXT("Map asset"), Loaded->Map[TEXT("Filled")].Texture.Get(), Texture);
			TestEqual(TEXT("Map soft path"), Loaded->Map[TEXT("Filled")].SoftTexture.ToSoftObjectPath(), Saved->SoftTexture.ToSoftObjectPath());
		}
		TestEqual(TEXT("Mutable state survives"), Loaded->Health, 37);
		TestFalse(TEXT("Archive read succeeds"), Reader.IsError());
	}

	// Nested runtime objects retain the legacy class-ID encoding, including in version 2 saves.
	Saved->Object = NewObject<UTestNestedUObject>(Saved);
	CastChecked<UTestNestedUObject>(Saved->Object)->NestedIntVal = 73;
	State->StoreGlobalObject(Saved, TEXT("References"));
	auto* Loaded = NewObject<UTestReferenceSaveObject>();
	Loaded->Object = Texture; // A previous asset must not receive restored runtime properties.
	State->RestoreGlobalObject(Loaded, TEXT("References"));
	auto* Nested = Cast<UTestNestedUObject>(Loaded->Object);
	if (TestNotNull(TEXT("Runtime object replaces asset default"), Nested))
	{
		TestEqual(TEXT("Nested state survives"), Nested->NestedIntVal, 73);
		TestEqual(TEXT("Runtime object is owned by restored object"), Nested->GetOuter(), static_cast<UObject*>(Loaded));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpudLegacyReferenceTest, "SPUDTest.LegacyV2References",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSpudLegacyReferenceTest::RunTest(const FString& Parameters)
{
	// These fields use exactly the version 2 wire format: class ID, then nested scalar properties.
	auto* Saved = NewObject<UTestSaveObjectParent>();
	Saved->UObjectVal1 = NewObject<UTestNestedChild1>(Saved);
	Saved->UObjectVal1->NestedStringVal1 = TEXT("Legacy saved state");
	auto* State = NewObject<USpudState>();
	State->StoreGlobalObject(Saved, TEXT("Legacy"));
	State->SaveData.Info.SystemVersion = 2;
	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes);
	FSpudChunkedDataArchive ChunkedWriter(Writer);
	State->SaveData.WriteToArchive(ChunkedWriter);
	auto* RestoredState = NewObject<USpudState>();
	FMemoryReader Reader(Bytes);
	FSpudChunkedDataArchive ChunkedReader(Reader);
	RestoredState->SaveData.ReadFromArchive(ChunkedReader, true, FString());
	auto* Loaded = NewObject<UTestSaveObjectParent>();
	RestoredState->RestoreGlobalObject(Loaded, TEXT("Legacy"));
	if (TestNotNull(TEXT("Version 2 nested object restores"), Loaded->UObjectVal1))
		TestEqual(TEXT("Version 2 value restores"), Loaded->UObjectVal1->NestedStringVal1, Saved->UObjectVal1->NestedStringVal1);
	TestNull(TEXT("Version 2 null restores"), Loaded->UObjectVal2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpudInvalidReferenceTest, "SPUDTest.InvalidAssetReferences",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSpudInvalidReferenceTest::RunTest(const FString& Parameters)
{
	auto* Loaded = NewObject<UTestReferenceSaveObject>();
	auto* Property = FindFProperty<FObjectProperty>(Loaded->GetClass(), GET_MEMBER_NAME_CHECKED(UTestReferenceSaveObject, Texture));
	FSpudPropertyDef Definition;
	Definition.DataType = SpudPropertyUtil::GetPropertyDataType(Property);
	FSpudClassMetadata Metadata;
	for (const FString Path : {FString(TEXT("/Script/Engine.Actor")), FString(TEXT("/Engine/EngineResources/SPUDMissingAsset.SPUDMissingAsset"))})
	{
		TArray<uint8> Bytes;
		FSpudMemoryWriter Writer(Bytes);
		uint32 Marker = SPUDDATA_CLASSID_ASSET;
		FString StoredPath = Path;
		Writer << Marker << StoredPath;
		FSpudMemoryReader Reader(Bytes);
		Loaded->Texture = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture"));
		AddExpectedError(Path.StartsWith(TEXT("/Script")) ? TEXT("is incompatible with property") : TEXT("Cannot resolve asset"), EAutomationExpectedErrorFlags::Contains, 1);
		SpudPropertyUtil::RestoreProperty(Loaded, Property, Loaded, Definition, nullptr, Metadata, 0, Reader);
		TestNull(TEXT("Invalid reference clears previous value"), Loaded->Texture.Get());
		TestEqual(TEXT("Invalid reference consumes its entire record"), Reader.Tell(), static_cast<int64>(Bytes.Num()));
	}
	return true;
}
