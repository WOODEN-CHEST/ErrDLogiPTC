#pragma once
#include <stddef.h>
#include <stdint.h>
#include "wr/WRError.h"
#include "wr/WRMemory.h"


/**
 * The asset manager helps store and manage asset definitions, as well as load and unload assets.
 * 
 * Heres how assets work:
 * 
 * There exist asset definitions. An asset definition describes an asset and where its resources are located,
 * it is not the actual asset itself. For example, a sprite animation asset definition can describe the locations
 * of the animation's frames, the animation's default framerate, frame step, loop and running statuses.
 *     For every asset which will be used, there must exist an asset definition.
 * Asset definitions may be added or removed at any time. They are basically recipes to how assets should be loaded.
 * Most common use is to load all asset definitions at the start of the game, then 
 * Removing an asset definition does not unload the asset it is the definition of, but it does prevent loading
 * it again in the future until it is added back.
 * Each asset definition has an asset name. The names are unique per asset type, so no 2 assets of the same type
 * may have the same name. When loading assets, the asset type and name are passed in. The manager automatically
 * resolves the correct asset definition (if one exists) from these properties and uses the asset definition to
 * figure out how to load the asset.
 * 
 * Assets themselves can be
 * 
 * 
 * There are no hard-coded asset types. Instead, asset type needs to be registered in the asset manager
 * with an 
 * 
 * 
 * The asset manager if fully thread-safe. All of its functions can be called from any thread at any time.
 * 
 */



#define ASSET_TYPE_ID_INVALID ((AssetTypeID)0)
#define ASSET_USER_ID_INVALID ((AssetUserID)0)

typedef uint64_t AssetTypeID;

typedef uint64_t AssetUserID;

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

typedef struct AssetLoadProgressStruct AssetLoadProgress;

typedef struct AssetBulkOperationStruct AssetBulkOperation;

typedef struct PromisedAssetStruct PromisedAsset;


// Functions.
Error AssetManager_Construct1(AssetManager** outSelf);

Error AssetManager_Deconstruct(AssetManager* self);

/* Strings are copied, references do not need to be kept after this method call. */
Error AssetManager_CreateAssetType(AssetManager* self,
    const unsigned char* name,
    const unsigned char* directoryName,
    AssetDefinitionConstructor definitionConstructor,
    AssetTypeID* outID);

Error AssetManager_CreateStandardAssetTypes(AssetManager* self);

Error AssetManager_RemoveAssetType(AssetManager* self, AssetTypeID id);

Error AssetManager_GetAssetTypeDirectoryName(AssetManager* self, AssetTypeID id, const unsigned char** outName);

Error AssetManager_GetAssetTypeName(AssetManager* self, AssetTypeID id, const unsigned char** outName);

Error AssetManager_ReadDefinitions(AssetManager* self, const unsigned char* directory);

Error AssetManager_SetDefinition(AssetManager* self, AssetDefinition* definition);

Error AssetManager_RemoveDefinition(AssetManager* self, AssetDefinition* definition);

Error AssetManager_GetNewUserID(AssetManager* self, AssetUserID* outID);

Error AssetManager_BorrowGenericBuffer(AssetManager* self, size_t elementSize, GenericBuffer** outBuffer);

Error AssetManager_ReturnGenericBuffer(AssetManager* self, GenericBuffer* buffer);

Error AssetManager_LoadAssetSingle(AssetManager* self,
    AssetTypeID assetType,
    const unsigned char* name,
    AssetUserID userID,
    void** outAsset);

Error AssetManager_CreateAssetBulkOperation(AssetManager* self,
    AssetUserID userID,
    AssetBulkOperation** outOperation);

Error AssetManager_LoadAssetBulk(AssetManager* self, AssetBulkOperation* operation);

Error AssetManager_AddBulkEntry(AssetManager* self,
    AssetBulkOperation* operation,
    AssetTypeID assetType,
    const unsigned char* name,
    PromisedAsset** outAsset);





AssetLoadProgress* AssetBulkOperation_GetProgress(AssetBulkOperation* self);



Error AssetLoadProgress_GetProgressFactor(AssetLoadProgress* self, double* outFactor);

Error AssetLoadProgress_GetItemCountTotal(AssetLoadProgress* self, size_t* outItemCountTotal);

Error AssetLoadProgress_GetItemCountProcessed(AssetLoadProgress* self, size_t* outItemCountProcessed);