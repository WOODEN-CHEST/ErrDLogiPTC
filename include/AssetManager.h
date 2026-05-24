#pragma once
#include <stddef.h>
#include <stdint.h>
#include "wr/WRError.h"
#include "wr/WRMemory.h"

#define ASSET_TYPE_ID_INVALID ((AssetTypeID)0)

typedef uint64_t AssetTypeID;

typedef struct AssetManagerStruct AssetManager;

typedef Error (*AssetDefinitionConstructor)();

typedef struct AssetDefinitionVTableStruct
{
    void* Self;

} AssetDefinitionVTable;

typedef struct AssetDefinitionStruct
{
    void* Self;
    AssetDefinitionVTable _vtable;
} AssetDefinition;

typedef struct AnimationDefinitionStruct AnimationDefinition;

/* Does not include ALL asset types, just the most commonly used ones. */
typedef struct StandardAssetTypes
{
    AssetTypeID SpriteSheet;
    AssetTypeID SpriteAnimation;
    AssetTypeID Sound;
    AssetTypeID Font;
    AssetTypeID Shader;
};


// Functions.
Error AssetManager_Construct1(AssetManager** outSelf);

Error AssetManager_Deconstruct(AssetManager* self);

/* Strings are copied, references do not need to be kept after this method call. */
Error AssetManager_CreateAssetType(AssetManager* self,
    const unsigned char* name,
    const unsigned char* directoryName,
    AssetTypeID* outID);

Error AssetManager_CreateStandardAssetTypes(AssetManager* self);

Error AssetManager_RemoveAssetType(AssetManager* self, AssetTypeID id);

Error AssetManager_GetAssetTypeDirectoryName(AssetManager* self, AssetTypeID id, const unsigned char** outName);

Error AssetManager_GetAssetTypeName(AssetManager* self, AssetTypeID id, const unsigned char** outName);

Error AssetManager_ReadDefinitions(AssetManager* self, const unsigned char* directory);

Error AssetManager_SetDefinition(AssetManager* self, AssetDefinition* definition);

Error AssetManager_RemoveDefinition(AssetManager* self, AssetDefinition* definition);