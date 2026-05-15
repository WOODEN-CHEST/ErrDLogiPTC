#pragma once
#include <stddef.h>
#include <stdint.h>
#include "wr/WRHashMap.h"
#include "wr/WRError.h"
#include "wr/WRMemory.h"

#define ASSET_TYPE_ID_INVALID ((AssetTypeID)0)

typedef uint64_t AssetTypeID;

typedef struct AssetTypeStruct
{
    AssetTypeID _id;
    const unsigned char* _typeName;
    const unsigned char* _directoryName;
} AssetType;

typedef struct AssetTypeRegistryStruct
{
    AssetTypeID _nextAvailableAssetTypeID;
    GenericBuffer _assetTypes;
} AssetTypeRegistry;



typedef struct AssetDefinitionStruct
{
    void* Self;
    AssetTypeID _type;
} AssetDefinition;

typedef struct AnimationDefinitionStruct
{

} AnimationDefinition;



typedef struct AssetManagerStruct
{

} AssetManager;



// Functions.
Error AssetTypeRegistry_Construct1(AssetTypeRegistry* self);

Error AssetTypeRegistry_Deconstruct(AssetTypeRegistry* self);

Error AssetTypeRegistry_RegisterAssetType(AssetTypeRegistry* self,
    const unsigned char* typeName,
    const unsigned char* directoryName,
    AssetTypeID* outID);

Error AssetTypeRegistry_UnregisterAssetType(AssetTypeRegistry* self, AssetTypeID* outID);

Error AssetTypeREgistry_GetAssetTypeInfo(AssetTypeRegistry* self, AssetTypeID id, AssetType* outInfo);


