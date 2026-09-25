// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "SimWorldSubsystem.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_UOBJECT");
void EmptyLinkFunctionForGeneratedCodeSimWorldSubsystem() {}

// ********** Begin Cross Module References ********************************************************
ENGINE_API UClass* Z_Construct_UClass_UTickableWorldSubsystem(ETypeConstructPhase);
// ********** End Cross Module References **********************************************************

// ********** Begin Same Module References *********************************************************
UPackage* Z_Construct_UPackage__Script_SimRuntime(ETypeConstructPhase);
SIMRUNTIME_API UClass* Z_Construct_UClass_USimWorldSubsystem(ETypeConstructPhase);
SIMRUNTIME_API UClass* Z_Construct_UClass_USimWorldSubsystem(ETypeConstructPhase);
// ********** End Same Module References ***********************************************************
#define UHT_STRUCT_BASE(INIT) UE::CodeGen::ConstInit::TCompiledInObjectPtr<const FStructBaseChain>(UE::Private::AsStructBaseChain(INIT))

// ********** Begin Class USimWorldSubsystem Function GetSimDay ************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UFunction_USimWorldSubsystem_GetSimDay_Statics
struct UHT_STATICS
{
	struct SimWorldSubsystem_eventGetSimDay_Parms
	{
		int64 ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "Category", "Sim" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** Current sim day (1-based; day 1 = the morning the prisoner arrives). */" },
#endif
		{ "ModuleRelativePath", "Public/SimWorldSubsystem.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Current sim day (1-based; day 1 = the morning the prisoner arrives)." },
#endif
	};
#endif // WITH_METADATA

// ********** Begin Function GetSimDay constinit property declarations *****************************
	static const UECodeGen_Private::FInt64PropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Function GetSimDay constinit property declarations *******************************
	static const UECodeGen_Private::FFunctionParams FuncParams;
};

// ********** Begin Function GetSimDay Property Definitions ****************************************
const UECodeGen_Private::FInt64PropertyParams UHT_STATICS::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Int64, nullptr, nullptr, 1, STRUCT_OFFSET(SimWorldSubsystem_eventGetSimDay_Parms, ReturnValue), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Function GetSimDay Property Definitions ******************************************
const UECodeGen_Private::FFunctionParams UHT_STATICS::FuncParams = { { (FTypeConstructFunc*)Z_Construct_UClass_USimWorldSubsystem, nullptr, "GetSimDay", UHT_STATICS::PropPointers, UE_ARRAY_COUNT(UHT_STATICS::PropPointers), DataSizeOf<UHT_STATICS::SimWorldSubsystem_eventGetSimDay_Parms>(), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x14022401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)},  };
static_assert(sizeof(UHT_STATICS::SimWorldSubsystem_eventGetSimDay_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_USimWorldSubsystem_GetSimDay(ETypeConstructPhase Phase)
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, UHT_STATICS::FuncParams);
	}
	return ReturnFunction;
}
#undef UHT_STATICS
DEFINE_FUNCTION(USimWorldSubsystem::execGetSimDay)
{
	P_FINISH;
	P_NATIVE_BEGIN;
	*(int64*)Z_Param__Result=USimWorldSubsystem::GetSimDay();
	P_NATIVE_END;
}
// ********** End Class USimWorldSubsystem Function GetSimDay **************************************

// ********** Begin Class USimWorldSubsystem Function GetSimDrought ********************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UFunction_USimWorldSubsystem_GetSimDrought_Statics
struct UHT_STATICS
{
	struct SimWorldSubsystem_eventGetSimDrought_Parms
	{
		int32 ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "Category", "Sim" },
		{ "ModuleRelativePath", "Public/SimWorldSubsystem.h" },
	};
#endif // WITH_METADATA

// ********** Begin Function GetSimDrought constinit property declarations *************************
	static const UECodeGen_Private::FIntPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Function GetSimDrought constinit property declarations ***************************
	static const UECodeGen_Private::FFunctionParams FuncParams;
};

// ********** Begin Function GetSimDrought Property Definitions ************************************
const UECodeGen_Private::FIntPropertyParams UHT_STATICS::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Int, nullptr, nullptr, 1, STRUCT_OFFSET(SimWorldSubsystem_eventGetSimDrought_Parms, ReturnValue), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Function GetSimDrought Property Definitions **************************************
const UECodeGen_Private::FFunctionParams UHT_STATICS::FuncParams = { { (FTypeConstructFunc*)Z_Construct_UClass_USimWorldSubsystem, nullptr, "GetSimDrought", UHT_STATICS::PropPointers, UE_ARRAY_COUNT(UHT_STATICS::PropPointers), DataSizeOf<UHT_STATICS::SimWorldSubsystem_eventGetSimDrought_Parms>(), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x14022401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)},  };
static_assert(sizeof(UHT_STATICS::SimWorldSubsystem_eventGetSimDrought_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_USimWorldSubsystem_GetSimDrought(ETypeConstructPhase Phase)
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, UHT_STATICS::FuncParams);
	}
	return ReturnFunction;
}
#undef UHT_STATICS
DEFINE_FUNCTION(USimWorldSubsystem::execGetSimDrought)
{
	P_FINISH;
	P_NATIVE_BEGIN;
	*(int32*)Z_Param__Result=USimWorldSubsystem::GetSimDrought();
	P_NATIVE_END;
}
// ********** End Class USimWorldSubsystem Function GetSimDrought **********************************

// ********** Begin Class USimWorldSubsystem Function GetSimPrice **********************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UFunction_USimWorldSubsystem_GetSimPrice_Statics
struct UHT_STATICS
{
	struct SimWorldSubsystem_eventGetSimPrice_Parms
	{
		FString CityId;
		FString ItemId;
		int64 ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "Category", "Sim" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** Silver-grain price of an item in a city's market (canon ids). */" },
#endif
		{ "ModuleRelativePath", "Public/SimWorldSubsystem.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Silver-grain price of an item in a city's market (canon ids)." },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_CityId_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ItemId_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA

// ********** Begin Function GetSimPrice constinit property declarations ***************************
	static const UECodeGen_Private::FStrPropertyParams NewProp_CityId;
	static const UECodeGen_Private::FStrPropertyParams NewProp_ItemId;
	static const UECodeGen_Private::FInt64PropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Function GetSimPrice constinit property declarations *****************************
	static const UECodeGen_Private::FFunctionParams FuncParams;
};

// ********** Begin Function GetSimPrice Property Definitions **************************************
const UECodeGen_Private::FStrPropertyParams UHT_STATICS::NewProp_CityId = { "CityId", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Str, nullptr, nullptr, 1, STRUCT_OFFSET(SimWorldSubsystem_eventGetSimPrice_Parms, CityId), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_CityId_MetaData), NewProp_CityId_MetaData) };
const UECodeGen_Private::FStrPropertyParams UHT_STATICS::NewProp_ItemId = { "ItemId", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Str, nullptr, nullptr, 1, STRUCT_OFFSET(SimWorldSubsystem_eventGetSimPrice_Parms, ItemId), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ItemId_MetaData), NewProp_ItemId_MetaData) };
const UECodeGen_Private::FInt64PropertyParams UHT_STATICS::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Int64, nullptr, nullptr, 1, STRUCT_OFFSET(SimWorldSubsystem_eventGetSimPrice_Parms, ReturnValue), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_CityId,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ItemId,
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Function GetSimPrice Property Definitions ****************************************
const UECodeGen_Private::FFunctionParams UHT_STATICS::FuncParams = { { (FTypeConstructFunc*)Z_Construct_UClass_USimWorldSubsystem, nullptr, "GetSimPrice", UHT_STATICS::PropPointers, UE_ARRAY_COUNT(UHT_STATICS::PropPointers), DataSizeOf<UHT_STATICS::SimWorldSubsystem_eventGetSimPrice_Parms>(), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x14022401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)},  };
static_assert(sizeof(UHT_STATICS::SimWorldSubsystem_eventGetSimPrice_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_USimWorldSubsystem_GetSimPrice(ETypeConstructPhase Phase)
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, UHT_STATICS::FuncParams);
	}
	return ReturnFunction;
}
#undef UHT_STATICS
DEFINE_FUNCTION(USimWorldSubsystem::execGetSimPrice)
{
	P_GET_PROPERTY(FStrProperty,Z_Param_CityId);
	P_GET_PROPERTY(FStrProperty,Z_Param_ItemId);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(int64*)Z_Param__Result=USimWorldSubsystem::GetSimPrice(Z_Param_CityId,Z_Param_ItemId);
	P_NATIVE_END;
}
// ********** End Class USimWorldSubsystem Function GetSimPrice ************************************

// ********** Begin Class USimWorldSubsystem Function GetSimSeason *********************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UFunction_USimWorldSubsystem_GetSimSeason_Statics
struct UHT_STATICS
{
	struct SimWorldSubsystem_eventGetSimSeason_Parms
	{
		FString ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "Category", "Sim" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** Season id for today: rains, sowing, harvest, vintage (D-015). */" },
#endif
		{ "ModuleRelativePath", "Public/SimWorldSubsystem.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Season id for today: rains, sowing, harvest, vintage (D-015)." },
#endif
	};
#endif // WITH_METADATA

// ********** Begin Function GetSimSeason constinit property declarations **************************
	static const UECodeGen_Private::FStrPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Function GetSimSeason constinit property declarations ****************************
	static const UECodeGen_Private::FFunctionParams FuncParams;
};

// ********** Begin Function GetSimSeason Property Definitions *************************************
const UECodeGen_Private::FStrPropertyParams UHT_STATICS::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Str, nullptr, nullptr, 1, STRUCT_OFFSET(SimWorldSubsystem_eventGetSimSeason_Parms, ReturnValue), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Function GetSimSeason Property Definitions ***************************************
const UECodeGen_Private::FFunctionParams UHT_STATICS::FuncParams = { { (FTypeConstructFunc*)Z_Construct_UClass_USimWorldSubsystem, nullptr, "GetSimSeason", UHT_STATICS::PropPointers, UE_ARRAY_COUNT(UHT_STATICS::PropPointers), DataSizeOf<UHT_STATICS::SimWorldSubsystem_eventGetSimSeason_Parms>(), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x14022401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)},  };
static_assert(sizeof(UHT_STATICS::SimWorldSubsystem_eventGetSimSeason_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_USimWorldSubsystem_GetSimSeason(ETypeConstructPhase Phase)
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, UHT_STATICS::FuncParams);
	}
	return ReturnFunction;
}
#undef UHT_STATICS
DEFINE_FUNCTION(USimWorldSubsystem::execGetSimSeason)
{
	P_FINISH;
	P_NATIVE_BEGIN;
	*(FString*)Z_Param__Result=USimWorldSubsystem::GetSimSeason();
	P_NATIVE_END;
}
// ********** End Class USimWorldSubsystem Function GetSimSeason ***********************************

// ********** Begin Class USimWorldSubsystem Function SetSimDrought ********************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UFunction_USimWorldSubsystem_SetSimDrought_Statics
struct UHT_STATICS
{
	struct SimWorldSubsystem_eventSetSimDrought_Parms
	{
		int32 Stage;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
		{ "Category", "Sim" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** The drought that is breaking the world: 0 = none, rises through the acts. */" },
#endif
		{ "ModuleRelativePath", "Public/SimWorldSubsystem.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "The drought that is breaking the world: 0 = none, rises through the acts." },
#endif
	};
#endif // WITH_METADATA

// ********** Begin Function SetSimDrought constinit property declarations *************************
	static const UECodeGen_Private::FIntPropertyParams NewProp_Stage;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Function SetSimDrought constinit property declarations ***************************
	static const UECodeGen_Private::FFunctionParams FuncParams;
};

// ********** Begin Function SetSimDrought Property Definitions ************************************
const UECodeGen_Private::FIntPropertyParams UHT_STATICS::NewProp_Stage = { "Stage", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Int, nullptr, nullptr, 1, STRUCT_OFFSET(SimWorldSubsystem_eventSetSimDrought_Parms, Stage), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_Stage,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Function SetSimDrought Property Definitions **************************************
const UECodeGen_Private::FFunctionParams UHT_STATICS::FuncParams = { { (FTypeConstructFunc*)Z_Construct_UClass_USimWorldSubsystem, nullptr, "SetSimDrought", UHT_STATICS::PropPointers, UE_ARRAY_COUNT(UHT_STATICS::PropPointers), DataSizeOf<UHT_STATICS::SimWorldSubsystem_eventSetSimDrought_Parms>(), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04022401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)},  };
static_assert(sizeof(UHT_STATICS::SimWorldSubsystem_eventSetSimDrought_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_USimWorldSubsystem_SetSimDrought(ETypeConstructPhase Phase)
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, UHT_STATICS::FuncParams);
	}
	return ReturnFunction;
}
#undef UHT_STATICS
DEFINE_FUNCTION(USimWorldSubsystem::execSetSimDrought)
{
	P_GET_PROPERTY(FIntProperty,Z_Param_Stage);
	P_FINISH;
	P_NATIVE_BEGIN;
	USimWorldSubsystem::SetSimDrought(Z_Param_Stage);
	P_NATIVE_END;
}
// ********** End Class USimWorldSubsystem Function SetSimDrought **********************************

// ********** Begin Class USimWorldSubsystem *******************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_Construct_UClass_USimWorldSubsystem_Statics
struct UHT_STATICS
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Type_MetaData[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "/**\n * The simulation's engine home. Binding rules (plan v2 \xc2\xa7""6, D-010):\n *  - the world is created from the shipped canon (Content/Sim/canon) and one seed;\n *  - one sim day advances per SimDaysPerRealMinute of engine time;\n *  - readers (player, NPCs, UI) call the getters; NOTHING writes module state\n *    except the kernel's own tick \xe2\x80\x94 the kernel's rule is the engine's rule.\n */" },
#endif
		{ "IncludePath", "SimWorldSubsystem.h" },
		{ "ModuleRelativePath", "Public/SimWorldSubsystem.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "The simulation's engine home. Binding rules (plan v2 \xc2\xa7""6, D-010):\n - the world is created from the shipped canon (Content/Sim/canon) and one seed;\n - one sim day advances per SimDaysPerRealMinute of engine time;\n - readers (player, NPCs, UI) call the getters; NOTHING writes module state\n   except the kernel's own tick \xe2\x80\x94 the kernel's rule is the engine's rule." },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_SimDaysPerRealMinute_MetaData[] = {
		{ "Category", "Sim" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/**\n\x09 * Real minutes of engine time per sim day (parent architecture \xc2\xa7""4.2 default:\n\x09 * 45). Settable so the acts' pacing and the vertical slice can differ.\n\x09 */" },
#endif
		{ "ModuleRelativePath", "Public/SimWorldSubsystem.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Real minutes of engine time per sim day (parent architecture \xc2\xa7""4.2 default:\n45). Settable so the acts' pacing and the vertical slice can differ." },
#endif
	};
#endif // WITH_METADATA

// ********** Begin Class USimWorldSubsystem constinit property declarations ***********************
	static const UECodeGen_Private::FFloatPropertyParams NewProp_SimDaysPerRealMinute;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Class USimWorldSubsystem constinit property declarations *************************
	static constexpr UE::CodeGen::FClassNativeFunction Funcs[] = {
		{ .NameUTF8 = UTF8TEXT("GetSimDay"), .Pointer = &USimWorldSubsystem::execGetSimDay },
		{ .NameUTF8 = UTF8TEXT("GetSimDrought"), .Pointer = &USimWorldSubsystem::execGetSimDrought },
		{ .NameUTF8 = UTF8TEXT("GetSimPrice"), .Pointer = &USimWorldSubsystem::execGetSimPrice },
		{ .NameUTF8 = UTF8TEXT("GetSimSeason"), .Pointer = &USimWorldSubsystem::execGetSimSeason },
		{ .NameUTF8 = UTF8TEXT("SetSimDrought"), .Pointer = &USimWorldSubsystem::execSetSimDrought },
	};
	static FTypeConstructFunc* DependentSingletons[];
	static constexpr FClassFunctionLinkInfo FuncInfo[] = {
		{ &Z_Construct_UFunction_USimWorldSubsystem_GetSimDay, "GetSimDay" }, // ebde33c753c5976988df72c183fc84d6cf47def5
		{ &Z_Construct_UFunction_USimWorldSubsystem_GetSimDrought, "GetSimDrought" }, // d7d580493d9bb82c280200cf107d4f4a43250138
		{ &Z_Construct_UFunction_USimWorldSubsystem_GetSimPrice, "GetSimPrice" }, // cb0ab9d8479f352171458b859faf4d1927d88479
		{ &Z_Construct_UFunction_USimWorldSubsystem_GetSimSeason, "GetSimSeason" }, // 61b8fd7843a1815b0b8a6efbcb52c9245596e0ef
		{ &Z_Construct_UFunction_USimWorldSubsystem_SetSimDrought, "SetSimDrought" }, // ef8c87c0d92150581198a78b02c7b7007faef5b0
	};
	static_assert(UE_ARRAY_COUNT(FuncInfo) < 2048);
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<USimWorldSubsystem>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
}; // struct UHT_STATICS

// ********** Begin Class USimWorldSubsystem Property Definitions **********************************
const UECodeGen_Private::FFloatPropertyParams UHT_STATICS::NewProp_SimDaysPerRealMinute = { "SimDaysPerRealMinute", nullptr, (EPropertyFlags)0x0010000000004001, UECodeGen_Private::EPropertyGenFlags::Float, nullptr, nullptr, 1, STRUCT_OFFSET(USimWorldSubsystem, SimDaysPerRealMinute), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_SimDaysPerRealMinute_MetaData), NewProp_SimDaysPerRealMinute_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const UHT_STATICS::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&UHT_STATICS::NewProp_SimDaysPerRealMinute,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::PropPointers) < 2048);
// ********** End Class USimWorldSubsystem Property Definitions ************************************
FTypeConstructFunc* UHT_STATICS::DependentSingletons[] = {
	(FTypeConstructFunc*)Z_Construct_UClass_UTickableWorldSubsystem,
	(FTypeConstructFunc*)Z_Construct_UPackage__Script_SimRuntime,
};
static_assert(UE_ARRAY_COUNT(UHT_STATICS::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams UHT_STATICS::ClassParams = {
	&Z_Construct_UClass_USimWorldSubsystem,
	"Engine",
	&StaticCppClassTypeInfo,
	DependentSingletons,
	FuncInfo,
	UHT_STATICS::PropPointers,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	UE_ARRAY_COUNT(FuncInfo),
	UE_ARRAY_COUNT(UHT_STATICS::PropPointers),
	0,
	0x001000A4u,
	METADATA_PARAMS(UE_ARRAY_COUNT(UHT_STATICS::Type_MetaData), UHT_STATICS::Type_MetaData)
};
static void USimWorldSubsystem_StaticRegisterNativesUSimWorldSubsystem()
{
	UClass* Class = USimWorldSubsystem::StaticClass();
	FNativeFunctionRegistrar::RegisterFunctions(Class, 		MakeConstArrayView(UHT_STATICS::Funcs));
}
FClassRegistrationInfo Z_Registration_Info_UClass_USimWorldSubsystem;
UClass* Z_Construct_UClass_USimWorldSubsystem(ETypeConstructPhase Phase)
{
	if (Phase == ETypeConstructPhase::Inner)
	{
		using TClass = USimWorldSubsystem;
		if (!Z_Registration_Info_UClass_USimWorldSubsystem.InnerSingleton)
		{
			GetPrivateStaticClassBody(
				TClass::StaticPackage(),
				TEXT("SimWorldSubsystem"),
				Z_Registration_Info_UClass_USimWorldSubsystem.InnerSingleton,
				USimWorldSubsystem_StaticRegisterNativesUSimWorldSubsystem,
				DataSizeOf<TClass>(),
				alignof(TClass),
				TClass::StaticClassFlags,
				TClass::StaticClassCastFlags(),
				TClass::StaticConfigName(),
				(UClass::ClassConstructorType)InternalConstructor<TClass>,
				(UClass::ClassVTableHelperCtorCallerType)InternalVTableHelperCtorCaller<TClass>,
				UOBJECT_CPPCLASS_STATICFUNCTIONS_FORCLASS(TClass),
				&TClass::Super::StaticClass,
				&TClass::WithinClass::StaticClass
			);
		}
		return Z_Registration_Info_UClass_USimWorldSubsystem.InnerSingleton;
	}
	if (!Z_Registration_Info_UClass_USimWorldSubsystem.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_USimWorldSubsystem.OuterSingleton, UHT_STATICS::ClassParams);
	}
	return Z_Registration_Info_UClass_USimWorldSubsystem.OuterSingleton;
}
#undef UHT_STATICS
USimWorldSubsystem::USimWorldSubsystem() {}
DEFINE_VTABLE_PTR_HELPER_CTOR_NS(, USimWorldSubsystem);
USimWorldSubsystem::~USimWorldSubsystem() {}
// ********** End Class USimWorldSubsystem *********************************************************

// ********** Begin Registration *******************************************************************
#ifdef UHT_STATICS
#error UHT_STATICS already defined
#endif
#define UHT_STATICS Z_CompiledInDeferFile_FID_Desktop_game_schizo_game_unreal_Plugins_SimRuntime_Source_SimRuntime_Public_SimWorldSubsystem_h__Script_SimRuntime_Statics
struct UHT_STATICS
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_USimWorldSubsystem, TEXT("USimWorldSubsystem"), &Z_Registration_Info_UClass_USimWorldSubsystem, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(USimWorldSubsystem), 1079786366U) },
	};
}; // UHT_STATICS 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Desktop_game_schizo_game_unreal_Plugins_SimRuntime_Source_SimRuntime_Public_SimWorldSubsystem_h__Script_SimRuntime_24f8da6a98fdaded61ff5ae41270efe053d8e99b{
	TEXT("/Script/SimRuntime"),
	UHT_STATICS::ClassInfo, UE_ARRAY_COUNT(UHT_STATICS::ClassInfo),
	nullptr, 0,
	nullptr, 0,
	nullptr, 0,
};
#undef UHT_STATICS
// ********** End Registration *********************************************************************
#undef UHT_STRUCT_BASE

PRAGMA_ENABLE_DEPRECATION_WARNINGS
