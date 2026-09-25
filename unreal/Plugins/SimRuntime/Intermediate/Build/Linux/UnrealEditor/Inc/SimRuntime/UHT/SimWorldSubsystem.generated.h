// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

// IWYU pragma: private, include "SimWorldSubsystem.h"

#ifdef SIMRUNTIME_SimWorldSubsystem_generated_h
#error "SimWorldSubsystem.generated.h already included, missing '#pragma once' in SimWorldSubsystem.h"
#endif
#define SIMRUNTIME_SimWorldSubsystem_generated_h

#include "UObject/ObjectMacros.h"
#include "UObject/ReflectedTypeAccessors.h"
#include "UObject/ScriptMacros.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS

// ********** Begin Class USimWorldSubsystem *******************************************************
#define FID_Desktop_game_schizo_game_unreal_Plugins_SimRuntime_Source_SimRuntime_Public_SimWorldSubsystem_h_21_RPC_WRAPPERS_NO_PURE_DECLS \
	DECLARE_FUNCTION(execAdvanceSimDays); \
	DECLARE_FUNCTION(execGetSimDrought); \
	DECLARE_FUNCTION(execSetSimDrought); \
	DECLARE_FUNCTION(execGetSimPrice); \
	DECLARE_FUNCTION(execGetSimSeason); \
	DECLARE_FUNCTION(execGetSimDay);


struct Z_Construct_UClass_USimWorldSubsystem_Statics;
SIMRUNTIME_API UClass* Z_Construct_UClass_USimWorldSubsystem(ETypeConstructPhase);

#define FID_Desktop_game_schizo_game_unreal_Plugins_SimRuntime_Source_SimRuntime_Public_SimWorldSubsystem_h_21_INCLASS_NO_PURE_DECLS \
private: \
	friend struct ::Z_Construct_UClass_USimWorldSubsystem_Statics; \
	friend SIMRUNTIME_API UClass* ::Z_Construct_UClass_USimWorldSubsystem(ETypeConstructPhase); \
public: \
	DECLARE_CLASS2(USimWorldSubsystem, UTickableWorldSubsystem, COMPILED_IN_FLAGS(0 | CLASS_Config), CASTCLASS_None, TEXT("/Script/SimRuntime"), Z_Construct_UClass_USimWorldSubsystem) \
	DECLARE_SERIALIZER(USimWorldSubsystem) \
	static constexpr const TCHAR* StaticConfigName() {return TEXT("Engine");} \



#define FID_Desktop_game_schizo_game_unreal_Plugins_SimRuntime_Source_SimRuntime_Public_SimWorldSubsystem_h_21_ENHANCED_CONSTRUCTORS \
	/** Standard constructor, called after all reflected properties have been initialized */ \
	NO_API USimWorldSubsystem(); \
	/** Deleted move- and copy-constructors, should never be used */ \
	USimWorldSubsystem(USimWorldSubsystem&&) = delete; \
	USimWorldSubsystem(const USimWorldSubsystem&) = delete; \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, USimWorldSubsystem); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(USimWorldSubsystem); \
	DEFINE_DEFAULT_CONSTRUCTOR_CALL(USimWorldSubsystem) \
	NO_API virtual ~USimWorldSubsystem();


#define FID_Desktop_game_schizo_game_unreal_Plugins_SimRuntime_Source_SimRuntime_Public_SimWorldSubsystem_h_18_PROLOG
#define FID_Desktop_game_schizo_game_unreal_Plugins_SimRuntime_Source_SimRuntime_Public_SimWorldSubsystem_h_21_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	FID_Desktop_game_schizo_game_unreal_Plugins_SimRuntime_Source_SimRuntime_Public_SimWorldSubsystem_h_21_RPC_WRAPPERS_NO_PURE_DECLS \
	FID_Desktop_game_schizo_game_unreal_Plugins_SimRuntime_Source_SimRuntime_Public_SimWorldSubsystem_h_21_INCLASS_NO_PURE_DECLS \
	FID_Desktop_game_schizo_game_unreal_Plugins_SimRuntime_Source_SimRuntime_Public_SimWorldSubsystem_h_21_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS


class USimWorldSubsystem;

// ********** End Class USimWorldSubsystem *********************************************************

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID FID_Desktop_game_schizo_game_unreal_Plugins_SimRuntime_Source_SimRuntime_Public_SimWorldSubsystem_h

PRAGMA_ENABLE_DEPRECATION_WARNINGS
