# Asset Manager — API Design Proposal

> Status: **proposal / for review**. No implementation yet. Header signatures below are a sketch —
> full Doxygen docs, `const` qualification, and error-code enumeration come at implementation time.
> Sections marked **[DRAFT]** need your input before they can be finalized (models/animations).

This document proposes a complete public API for the asset manager, explains how it is used, and shows
how each requirement you listed maps onto a concrete design decision. Open questions are collected at
the end and also surfaced as review questions.

---

## 1. Goals recap

The manager must:

1. Load assets by **(type, name)**, resolved through **asset definitions** (recipes).
2. Support **dynamic asset types** (registered at runtime, no enum, no recompile → modding-friendly).
3. Load **static-state** assets — no per-instance state, freely shareable across the whole program.
4. Resolve asset resources from **files *or* in-memory/reference locations** (e.g. fetched from a server).
5. Keep definition ingestion **not hard-wired to JSON** (JSON is just the default format).
6. Support **runtime-created definitions** (built in code, not parsed from a file).
7. Support **resource packs / overrides** via an ordered list of search roots (Minecraft-style).
8. Attribute loaded assets to **users**; release-all-by-user; auto-unload at **0 users**.
9. Load on the **main thread** (Raylib/GL constraint), keeping the frame responsive by loading in
   **steps** rather than blocking.
10. Own **all** asset memory and **reuse buffers** via pools to avoid small allocations.
11. Provide **bulk loading** with queryable progress and ergonomic **promised assets**.
12. Return **custom wrapper types** (`SpriteSheet`, `GameModel`, …), never raw Raylib types.

---

## 2. Architecture at a glance

The design separates three concerns that your current draft mixes together. Keeping them apart is what
makes the "not tied to JSON" and "not tied to the filesystem" requirements fall out naturally.

```
                        ┌──────────────────────────────────────────────┐
                        │                AssetManager                   │
                        │  (format-agnostic, filesystem-agnostic core)  │
                        │                                               │
   register types  ───▶ │  • Type registry      (AssetTypeID → info)    │
   add search roots ──▶ │  • Search-root stack  (ordered, priority)     │
   set definitions ───▶ │  • Definition registry(type,name → recipe)    │
                        │  • Loaded-asset table (type,name → asset)     │
                        │  • User → holds bookkeeping (refcounting)     │
                        │  • Buffer pools       (reused scratch/storage)│
                        │  • Main-thread-affine (no internal locking)   │
                        └───────────────┬───────────────┬──────────────┘
                                        │               │
                 discovery (pluggable)  │               │  format (per-type)
                                        ▼               ▼
                        ┌───────────────────────┐   ┌───────────────────────────┐
                        │  Definition source    │   │  Per-type constructor     │
                        │  "where do raw        │   │  "raw bytes → typed       │
                        │   definitions live?"  │   │   AssetDefinition"        │
                        │  default: filesystem  │   │  standard types: uses     │
                        │  walk of search roots │   │  WRJSON internally        │
                        │  (custom: server, pak)│   │  (custom types: anything) │
                        └───────────────────────┘   └───────────────────────────┘
```

**Two extension points, both format-agnostic at the core:**

- **Discovery** — *where* raw definition blobs come from. Default = walk the search roots for files. The
  core never assumes files exist; a server/archive source can feed raw bytes in the same way.
- **Format** — *how* a raw blob becomes a typed definition. This lives entirely inside each **type's own
  definition constructor**. The standard types parse JSON there; a mod's type can use GHDF, a binary
  blob, or nothing at all. The manager and the discovery layer never touch JSON.

This is the key structural decision (see Open Question 1): the format lives in the type constructor, not
in the manager. The manager only ever moves opaque byte buffers around.

---

## 3. Core identifiers and value types

```c
#define ASSET_TYPE_ID_INVALID ((AssetTypeID)0)
#define ASSET_USER_ID_INVALID ((AssetUserID)0)

typedef uint64_t AssetTypeID;   // opaque handle to a registered type; 0 = invalid
typedef uint64_t AssetUserID;   // opaque handle to a user (subsystem/scene/etc.); 0 = invalid

typedef struct AssetManagerStruct AssetManager;
```

### Asset locations (mirror of the JSON `location` concept)

A location is either a file path (relative, extension-less, resolved through the search-root stack) or a
reference name (resolved through a registered resolver — e.g. a server blob or a procedurally built
resource). This is the in-code form of the `location` field documented in `asset_structure.md`.

```c
typedef enum AssetLocationTypeEnum
{
    AssetLocationType_File,       /* relative path, no extension; resolved via search roots */
    AssetLocationType_Reference   /* name resolved via a registered reference resolver      */
} AssetLocationType;

typedef struct AssetLocationStruct
{
    AssetLocationType Type;
    const unsigned char* Value;   /* borrowed UTF-8: path or reference name */
} AssetLocation;
```

Loaders never open files themselves. They ask the manager to open a location, so search-root override
and reference resolution happen in exactly one place:

```c
/* Opens a location as a read stream. For files: resolves the extension-less relative path across the
   search roots in priority order, first hit wins. For references: dispatches to the registered
   resolver. The returned stream is caller-closed. */
Error AssetManager_OpenResource(AssetManager* self,
    AssetTypeID assetType,            /* determines the type sub-directory for file locations */
    const AssetLocation* location,
    IOStream** outStream);
```

### Resolving to a filesystem path (for loaders that cannot take a stream)

Raylib model loading is filename-only — there is no `LoadModelFromMemory`, and `.gltf`/`.obj` loaders
resolve external `.bin`/`.mtl`/texture siblings by path — so a stream cannot feed the model loader. For
these few loaders the manager exposes a second resolution primitive that yields a real path. A **file**
location returns its resolved real path (no copy); a **reference** location is materialized to a temp
file in a manager-owned cache directory and that path is returned. The opaque handle remembers whether a
temp file was created, so releasing it deletes the temp (and is a no-op for a real path). The temp file
is short-lived: `LoadModel` reads it synchronously into RAM/GPU, then the handle is released immediately.

```c
typedef struct AssetResourcePathStruct AssetResourcePath;   /* opaque; knows if it wraps a temp or real path */

/* Resolves a location to a usable filesystem path. File locations return their resolved real path;
   reference/in-memory locations are written to a temp file (named with preferredExtension) whose path is
   returned. Use ONLY in loaders that cannot accept an IOStream (Raylib model loading). preferredExtension
   is the extension for a materialized temp file (e.g. u8"glb"); ignored for file locations, which keep
   their own resolved extension. */
Error AssetManager_AcquireResourcePath(AssetManager* self, AssetTypeID assetType,
    const AssetLocation* location, const unsigned char* preferredExtension, AssetResourcePath** outHandle);

/* The null-terminated filesystem path the handle resolved to; borrowed, valid until release. */
const unsigned char* AssetResourcePath_Get(AssetResourcePath* self);

/* Releases the handle, deleting the temp file if one was materialized (no-op for a real resolved path). */
Error AssetManager_ReleaseResourcePath(AssetManager* self, AssetResourcePath* handle);

/* Configures where temp files are materialized. Defaults to a hidden subdirectory of the working
   directory. The cache directory is cleared on manager construct AND deconstruct so a crash mid-load
   cannot leave orphaned temp files behind. */
Error AssetManager_SetCacheDirectory(AssetManager* self, const unsigned char* directory);
```

Because the temp-file materialization is centralized here, only the manager ever deals with temp files;
every non-model loader stays on the clean stream API and never knows this primitive exists. **Limitation
(documented):** a reference/in-memory model must be a self-contained single file (`.glb` recommended),
since a materialized temp cannot resolve external sibling files from the cache directory. File-based
models have no such limit — their siblings sit next to them on disk.

### Reference resolvers (in-memory / server-fetched resources)

```c
/* Produces a read stream for a named non-file resource. Called by AssetManager_OpenResource when a
   location is AssetLocationType_Reference. Must be thread-safe. */
typedef Error (*AssetReferenceResolver)(AssetManager* manager,
    const UserData* userData,
    const unsigned char* referenceName,
    IOStream** outStream);

Error AssetManager_SetReferenceResolver(AssetManager* self,
    AssetReferenceResolver resolver, const UserData* userData);

/* Convenience: register a fixed in-memory blob under a reference name (manager copies the bytes into
   pooled storage). Lets you drop a server-fetched buffer in without writing a resolver. */
Error AssetManager_SetReferenceBytes(AssetManager* self,
    const unsigned char* referenceName, const unsigned char* bytes, size_t byteCount);
```

---

## 4. Asset types (dynamic registration)

A type ties together its display name, its on-disk sub-directory, and the constructor that turns a raw
definition blob into a typed definition. Registration returns an opaque `AssetTypeID`.

```c
typedef struct AssetTypeInfoStruct
{
    const unsigned char* Name;           /* unique type name, e.g. u8"sprite_sheet"; copied */
    const unsigned char* DirectoryName;  /* sub-directory under each search root; copied     */
    AssetDefinitionConstructor Constructor;
    UserData ConstructorUserData;        /* passed to Constructor on every call               */
} AssetTypeInfo;

Error AssetManager_CreateAssetType(AssetManager* self, const AssetTypeInfo* info, AssetTypeID* outID);
Error AssetManager_RemoveAssetType(AssetManager* self, AssetTypeID id);

Error AssetManager_GetAssetTypeName(AssetManager* self, AssetTypeID id, const unsigned char** outName);
Error AssetManager_GetAssetTypeDirectoryName(AssetManager* self, AssetTypeID id, const unsigned char** outName);
Error AssetManager_GetAssetTypeByName(AssetManager* self, const unsigned char* name, AssetTypeID* outID);
```

### The definition constructor (the "format" extension point)

The constructor receives **raw bytes**, not a parsed JSON object. This is what keeps the manager free of
JSON. Standard types run WRJSON on the bytes internally; a modder's type can parse anything.

```c
/* Builds a typed AssetDefinition from a raw definition blob. The constructor owns parsing and validation
   (including reading the asset's name out of the blob). On success it allocates a concrete definition
   and returns it upcast to AssetDefinition*. Manager takes ownership on registration. */
typedef Error (*AssetDefinitionConstructor)(AssetManager* manager,
    const UserData* userData,              /* per-type constructor context; see pooling note below */
    const GenericBuffer* rawData,          /* the raw bytes, e.g. UTF-8 JSON text */
    const unsigned char* sourceDescription,/* for error messages: file path or reference name */
    AssetDefinition** outDefinition);
```

> **Shared JSON object pool (your pooling request).** `ConstructorUserData` is the clean vehicle for
> sharing a JSON object pool across all definition parsing. The standard (JSON) types are all registered
> with the *same* `JSONObjectPool*` stored in their `ConstructorUserData`; each constructor pulls it out,
> parses with `JSON_Deserialize(pool, rawData, &value)`, builds the typed definition, then returns the
> value tree to the pool — so compounds, arrays, and string buffers are recycled across every file
> instead of being reallocated per definition. The manager core never sees the pool; only the JSON
> constructors do. Since all loading is main-thread (§10), the single shared pool needs no locking. See §9.1.

### Standard types

Fixed the two problems in the current draft (missing typedef name; no way to receive the IDs):

```c
typedef struct StandardAssetTypesStruct
{
    AssetTypeID SpriteSheet;
    AssetTypeID SpriteAnimation;
    AssetTypeID Sound;
    AssetTypeID Font;
    AssetTypeID Shader;
    AssetTypeID Model;      /* [DRAFT] pending model design */
} StandardAssetTypes;

/* Registers all standard (JSON-based) types, writes their IDs into outTypes, and creates a shared JSON
   object pool wired into every standard type's ConstructorUserData so all definition parsing recycles
   JSON compounds/arrays/strings rather than reallocating. The caller owns *outDefinitionPool and frees
   it with JSONObjectPool_Deconstruct after (or alongside) the manager. Reuse the same pool as the
   ConstructorUserData of any custom JSON-based type to extend the recycling to it. JSONObjectPool is
   forward-declared in this header (opaque); no WRJSON include is pulled into the public header. */
Error AssetManager_CreateStandardAssetTypes(AssetManager* self, StandardAssetTypes* outTypes,
    JSONObjectPool** outDefinitionPool);
```

---

## 5. Asset definitions

An `AssetDefinition` is an **abstract base** (WR abstract-class pattern: vtable pointer + shared concrete
fields). Concrete definitions embed it as their first member so a concrete pointer upcasts to
`AssetDefinition*` for free.

```c
typedef struct AssetDefinitionVTableStruct
{
    /* Loads a concrete asset from this recipe into outLoaded. Opens resources via the manager (so
       search-root override + reference resolution apply), builds the custom wrapper type using manager
       buffer pools, and records how to unload it (see LoadedAsset). Called on the loading thread. */
    Error (*LoadAsset)(void* self, AssetManager* manager, AssetUserID user, LoadedAsset* outLoaded);

    /* Frees the definition object itself (not any asset loaded from it). */
    void (*Destroy)(void* self);
} AssetDefinitionVTable;

typedef struct AssetDefinitionStruct
{
    const AssetDefinitionVTable* VTable;
    AssetTypeID Type;         /* set by the manager at registration */
    unsigned char* Name;      /* owned; parsed from the blob by the constructor */
} AssetDefinition;

/* Concrete example (defined in SpriteSheet's definition module):
   struct SpriteSheetDefinitionStruct { AssetDefinition Base;  ...recipe fields... }; */
```

### Registering, overriding, removing, building

```c
/* Walks the search roots with the default filesystem source and registers every definition found.
   Higher-priority roots override lower ones on a (type, name) collision. */
Error AssetManager_ReadDefinitions(AssetManager* self);

/* Turns a raw blob into a definition using the type's constructor, WITHOUT registering it. Lets a custom
   (non-filesystem) source — e.g. server-fetched definitions — reuse per-type parsing. */
Error AssetManager_BuildDefinition(AssetManager* self, AssetTypeID type,
    const GenericBuffer* rawData, const unsigned char* sourceDescription, AssetDefinition** outDefinition);

/* Registers (or overrides) a definition by its (Type, Name). Manager takes ownership. This is also the
   entry point for runtime-created definitions and for custom sources. */
Error AssetManager_SetDefinition(AssetManager* self, AssetDefinition* definition);

/* Removes a definition. Per your requirement this does NOT unload any asset already loaded from it; it
   only prevents future loads until re-added. */
Error AssetManager_RemoveDefinition(AssetManager* self, AssetTypeID type, const unsigned char* name);

Error AssetManager_HasDefinition(AssetManager* self, AssetTypeID type, const unsigned char* name, bool* outExists);
```

**Runtime-created definitions** have two ergonomic paths:
- Implement the `AssetDefinitionVTable` yourself and hand the object to `AssetManager_SetDefinition`.
- Use a per-type builder helper that constructs the concrete definition for you, e.g.
  `SpriteSheetDefinition_Create(...) -> AssetDefinition*`, then `SetDefinition`. (Recommended for the
  standard types; they'll ship these builders.)

---

## 6. Search roots & resource packs

An ordered list of root directories. **Index 0 = highest priority** (override winner), matching your
"comes first ⇒ wins" rule. A resource pack or mod inserts itself at the front to override base assets;
appends to the back to only add new ones.

```c
Error AssetManager_AddSearchRoot(AssetManager* self, const unsigned char* directory);          /* lowest priority */
Error AssetManager_InsertSearchRoot(AssetManager* self, size_t index, const unsigned char* directory);
Error AssetManager_RemoveSearchRoot(AssetManager* self, const unsigned char* directory);
size_t AssetManager_GetSearchRootCount(AssetManager* self);
Error AssetManager_GetSearchRoot(AssetManager* self, size_t index, const unsigned char** outDirectory);
```

Both **definitions** and **resources** honor this stack:
- Definitions: on a (type, name) collision across roots, the highest-priority root's definition wins.
- Resource files: `AssetManager_OpenResource` resolves the extension-less relative path against each root
  in priority order and opens the first match (`{root}/{typeDir}/{relPath}{ext}`).

---

## 7. Loading, users, and auto-unload

### Static-state assets

A loaded asset is keyed by (type, name) and **shared**: a second load of the same (type, name) returns
the *same* wrapper pointer and just records another holder. The returned `void*` is a **borrowed** custom
wrapper (`SpriteSheet*`, `GameModel*`, …) — owned by the manager, valid until its user count reaches 0.

```c
Error AssetManager_LoadAssetSingle(AssetManager* self,
    AssetTypeID assetType, const unsigned char* name, AssetUserID user, void** outAsset);
```

### Users & holds

A "user" is any part of the program that wants to keep assets alive — a scene, the main menu, a mod, an
entity system. You mint one, load assets under it, and later release the whole user in one call.

```c
Error AssetManager_GetNewUserID(AssetManager* self, AssetUserID* outID);

/* Drops this user's hold on ONE asset. If that was the last user, the asset is unloaded immediately. */
Error AssetManager_ReleaseAsset(AssetManager* self, AssetTypeID assetType, const unsigned char* name, AssetUserID user);

/* Drops this user's hold on EVERY asset it holds (your primary teardown call). Any asset that reaches 0
   users is unloaded. The user ID stays valid for reuse. */
Error AssetManager_ReleaseAllAssetsForUser(AssetManager* self, AssetUserID user);

/* Releases all of a user's assets AND retires the ID. */
Error AssetManager_RetireUser(AssetManager* self, AssetUserID user);
```

**Hold semantics** (see Open Question 2): the recommended model is **presence-based** — each asset stores
a *set* of user IDs. A user loading the same asset five times still counts as one holder, and one
`ReleaseAsset`/`ReleaseAllAssetsForUser` fully removes it. This matches the "scene loads a bunch of
things, scene closes, everything drops" mental model and avoids leak-by-unbalanced-count bugs. The
alternative (counted holds per user) is available if you prefer strict acquire/release balance.

### Asset-to-asset dependencies

Some assets depend on others (a sprite animation frame sourced from a sprite sheet; a model's textures).
Internally the manager reuses the same holder machinery with the **depending asset acting as a user** of
its dependencies. When the dependent unloads, it releases its dependencies, which then unload if nobody
else holds them. This keeps dependency lifetime correct without a separate mechanism and is invisible to
callers.

---

## 8. Bulk loading & promised assets (stepped, main-thread)

Because Raylib/GL loading must happen on the main thread (§10), the bulk operation does **not** run on a
worker thread and does **not** block. Instead it is a resumable list you drive one asset at a time: each
call to `CompleteStep` loads exactly one queued asset and returns control, so your normal game loop can
update and render a loading screen between steps. This trades a fully smooth load for occasional
per-asset lag spikes, but avoids the crashes Raylib causes when assets are loaded off the main thread.

```c
typedef struct AssetBulkOperationStruct AssetBulkOperation;
typedef struct PromisedAssetStruct PromisedAsset;
typedef struct AssetLoadProgressStruct AssetLoadProgress;

/* All entries loaded by an operation are attributed to this user. Building the list is cheap: no I/O. */
Error AssetManager_CreateAssetBulkOperation(AssetManager* self, AssetUserID user, AssetBulkOperation** outOp);
Error AssetBulkOperation_Deconstruct(AssetBulkOperation* self);

/* Queues one asset. Returns a promise immediately (before anything is loaded). */
Error AssetBulkOperation_AddEntry(AssetBulkOperation* self,
    AssetTypeID assetType, const unsigned char* name, PromisedAsset** outPromise);

/* Loads exactly ONE pending entry (the next in the list) and returns. Call once (or a few times) per
   frame on the main thread, rendering a loading bar in between. If the operation is already complete
   this is a no-op. outDidWork is false when there was nothing left to load. A FAILED asset load does NOT
   fail this call: the failure is recorded on that entry's promise and the step still counts as done. */
Error AssetBulkOperation_CompleteStep(AssetBulkOperation* self, bool* outDidWork);

/* True once every entry has been processed (whether it succeeded or failed). */
bool AssetBulkOperation_IsComplete(AssetBulkOperation* self);

/* Progress for the loading bar. No atomics needed (single-threaded), but kept as an object so the UI
   code stays decoupled from the operation internals. */
AssetLoadProgress* AssetBulkOperation_GetProgress(AssetBulkOperation* self);
double AssetLoadProgress_GetFactor(AssetLoadProgress* self);       /* 0.0 .. 1.0 */
size_t AssetLoadProgress_GetItemCountTotal(AssetLoadProgress* self);
size_t AssetLoadProgress_GetItemCountProcessed(AssetLoadProgress* self);
bool   AssetLoadProgress_IsComplete(AssetLoadProgress* self);
```

> A `CompleteSteps(self, maxSteps, outStepsDone)` batch variant can be added if one-asset-per-frame is too
> slow for large lists — it lets you tune assets-per-frame against frame time. Left out of the core sketch
> for now; easy to add.

### Making promised assets not painful to use

The friction you worried about comes from consumers constantly asking "is it ready yet?". The design
sidesteps that with a clear two-phase contract:

- **While stepping**: you only ever touch the *progress* object (for the bar). You never dereference a
  promise mid-load.
- **After `IsComplete`**: every promise is resolved. You "unwrap" once — read the final asset pointer out
  of each promise — and from then on you hold plain `SpriteSheet*` / `GameModel*` pointers exactly like a
  single load would give you. The promise has done its job and can be discarded with the operation.

```c
bool  PromisedAsset_IsResolved(PromisedAsset* self);
Error PromisedAsset_GetAsset(PromisedAsset* self, void** outAsset); /* errors if unresolved/failed */
Error PromisedAsset_GetError(PromisedAsset* self);                 /* per-entry load error, if any  */
```

So the usage is: build → step each frame while rendering the bar → on complete, unwrap into your own
struct fields → free the operation. No per-frame null checks, no accessor indirection in your hot path.

**Partial failure**: per your decision, one failed entry never cancels the rest. That entry's promise
resolves to a failed state (`PromisedAsset_GetError`); every other entry still loads. `CompleteStep`
itself returns success for a step whose asset failed — the failure lives only on the promise, so you
inspect per-entry results after `IsComplete` and decide what to do (retry, fall back, abort the screen).

---

## 9. Memory ownership & buffer reuse

Everything the manager hands out is manager-owned. Callers borrow; the manager frees.

- **Loaded-asset storage** (a sprite sheet's entry buffer, an animation's frame buffer, decoded pixel
  data staged for upload, …) is drawn from manager buffer pools where it makes sense, and returned to the
  pool on unload. This is why the loaded asset carries its own teardown info:

```c
typedef struct LoadedAssetVTableStruct
{
    /* Frees the wrapper and returns any pooled buffers it borrowed. Runs when the asset hits 0 users.
       Independent of the definition, so removing a definition never orphans a loaded asset. */
    void (*Destroy)(void* self, AssetManager* manager);
} LoadedAssetVTable;

typedef struct LoadedAssetStruct
{
    void* Asset;                     /* the custom wrapper handed back to callers, e.g. SpriteSheet* */
    const LoadedAssetVTable* VTable;
    void* DestroyContext;            /* owned resources the destructor needs (pooled buffers, textures) */
} LoadedAsset;
```

- **Scratch during load** (decode buffers, temporary parse structures) is borrowed from and returned to
  the pool within a single load, so repeated loads don't churn the allocator:

```c
Error AssetManager_BorrowGenericBuffer(AssetManager* self, size_t elementSize, GenericBuffer** outBuffer);
Error AssetManager_ReturnGenericBuffer(AssetManager* self, GenericBuffer* buffer);
```

These wrap `WRBufferPool`. Since all loading is main-thread (§10), no mutex is needed around them.

### 9.1 Shared JSON object pool (definition parsing)

`WRJSON` requires every compound/array/string to be borrowed from a `JSONObjectPool` and returned to it;
returning a value tree recycles those objects for the next parse. We exploit that to eliminate
per-definition allocation churn:

- `AssetManager_CreateStandardAssetTypes` creates **one** `JSONObjectPool` and stores its pointer in the
  `ConstructorUserData` of every standard type. It is handed back to the caller (`outDefinitionPool`),
  who owns and frees it.
- Each JSON constructor does: pull the pool from `userData` → `JSON_Deserialize(pool, rawData, &value)` →
  read fields / build the typed definition → `JSONObjectPool_ReturnValue(pool, &value)`. The compounds,
  arrays, and string buffers go straight back into the pool's sub-pools, so the next definition reuses
  them. Parsing 500 definitions touches roughly the allocation footprint of parsing the single largest
  one, not the sum of all of them.
- Custom JSON-based types opt in by passing the same pool as their own `ConstructorUserData`.
- The manager core never references the pool — this is entirely a property of the JSON constructors, so
  the "manager is not tied to JSON" rule holds. `JSONObjectPool` appears in the public header only as an
  opaque forward declaration, used solely by the standard-types helper's signature.

This is clean precisely because loading is single-threaded: one pool, no synchronization, deterministic
recycle points.

---

## 10. Threading model — main-thread loading

Raylib has poor support for multi-threaded asset loading (GL is single-context and several loaders are
not thread-safe), so **all loading, unloading, definition mutation, and asset use happen on the main
thread**. This is a deliberate reversal of the original "load off-thread" idea: it trades smoothness
(occasional per-asset lag spikes during a bulk load) for not crashing.

Consequences:

- **No internal locking.** The manager is *main-thread-affine*: the registries, loaded-asset table,
  holder bookkeeping, buffer pools, and the shared JSON pool are all touched only from the main thread,
  so there is no RW-lock and no pool mutex. This is a large simplification over the original design.
- **Responsiveness comes from stepping, not threads** (§8): a bulk load advances one asset per
  `CompleteStep`, and your game loop renders a frame between steps.
- The public headers must **document the main-thread requirement** on every mutating call, so callers
  don't reintroduce the crash by touching the manager from a worker.
- If you later need read-only queries from another thread (e.g. a network thread asking "does this
  definition exist?"), we can add a narrowly-scoped lock around just those lookups without bringing back
  the full concurrent-load machinery. Flagged in §15 (4a) — but not needed for the current design.

---

## 11. Standard type wrappers

Loaders return the existing custom wrappers, never raw Raylib types:

| Type            | Wrapper (`outAsset` points to) | Notes                                            |
|-----------------|--------------------------------|--------------------------------------------------|
| `sprite_sheet`  | `SpriteSheet*`                 | entry buffer from a manager pool                 |
| `sprite_animation` | `SpriteAnimation*`          | may depend on `sprite_sheet` assets              |
| `sound`         | `GameSound*` (new wrapper)     | wraps Raylib `Sound`/`Music`                      |
| `font`          | `GameFont*`                    | existing                                          |
| `shader`        | `GameShader*` (new wrapper)    | wraps Raylib `Shader`                             |
| `model`         | `GameModel*`                   | **[DRAFT]**, see below                            |

> New wrapper modules (`GameSound`, `GameShader`) would be called out per the AGENTS "new module" rule
> when we implement. `GameModel` already exists but is currently a bare Raylib wrapper.

---

## 12. Models & animations

Decisions from review: target **OBJ** and **GLTF/GLB** (both loadable by Raylib); animation clips live in
**separate files** and are bound to a model at runtime; materials are **overridable** from the
definition; skeletal support is **undecided**, so the design keeps the static-mesh path primary and
treats skinning as an opt-in extension that doesn't disturb it.

Consequences of "OBJ + GLTF":
- **OBJ** is static-only (no skeleton, no animation). A `model` asset therefore always works as a plain
  static mesh + materials; that's the baseline everything supports.
- **GLTF/GLB** can additionally carry a skeleton. So the model definition has an *optional* rig section
  that OBJ models simply omit. Nothing in the static path depends on it.

### Model definition (extends `asset_structure.md`)

```
{
    "name": "player_character",
    "location": asset location,          // the model file (.obj / .gltf / .glb)

    "transform": {                       // [OPTIONAL] baked import transform applied at load
        "scale": number or vector3,      // uniform if a single number
        "rotation_euler": vector3,       // degrees
        "translation": vector3
    },

    "material_overrides": [              // [OPTIONAL] rebind material slots to game-managed assets
        {
            "slot": 0,                   // material index within the model, OR:
            "material_name": "body",     // ...address by name (GLTF names materials)
            "albedo": asset location,    // [OPTIONAL] texture asset location or reference
            "tint": color,               // [OPTIONAL]
            "texture_properties": texture properties  // [OPTIONAL]
        }
    ],

    "format": "glb",                     // [OPTIONAL] file extension WITHOUT the dot. REQUIRED only when
                                         // "location" is a reference (in-memory) model, since the format
                                         // can't be inferred from bytes; ignored for file locations
                                         // (their real extension is used). Reference models must be
                                         // self-contained single files (.glb recommended).

    "rig": "none"                        // [OPTIONAL, GLTF only] "none" (default) | "skinned"
                                         // present only if/when skeletal support lands; OBJ ignores it
}
```

The model loader is the one standard loader that uses `AssetManager_AcquireResourcePath` (§3) instead of
`OpenResource`, because Raylib needs a filename. For a file location it loads the resolved path directly;
for a reference location the bytes are materialized to a temp file named with `format`, loaded, and the
handle released immediately after `LoadModel` returns.

Because material overrides reference texture asset locations, a loaded `model` may **depend on** texture
assets — handled by the asset-to-asset dependency mechanism in §7 (the model acts as a user of its
textures; they unload with it).

### 3D animation definition (separate asset type from 2D sprite animations)

Clips are their own asset in their own file, and are bound to a model at runtime rather than in the
definition — so one clip can drive several compatible models and vice versa.

```
{
    "name": "run",
    "location": asset location,          // the separate animation file
    "clip": "Run"                        // [OPTIONAL] name or index of the clip within the file;
                                         //            defaults to the first/only clip
}
```

Runtime binding (kept out of the definition on purpose) would look like a small API on the model wrapper,
e.g. `GameModel_BindAnimation(model, animation)` returning a playable handle. **This binding API and the
`GameModelAnimation` wrapper are deferred** until the skeletal-vs-static question (Open Question 5a) is
settled — static-only models need no animation binding at all, so committing to the skinning API now
would be premature. The `model` asset type and its static path can ship first; the 3D `animation` type
slots in later without changing the manager API.

---

## 13. Consolidated public header sketch

Grouped for review; this is the shape `AssetManager.h` would take (docs elided here).

```c
// ---- lifecycle ----
Error AssetManager_Construct1(AssetManager** outSelf);
Error AssetManager_Deconstruct(AssetManager* self);

// ---- types ----
Error AssetManager_CreateAssetType(AssetManager* self, const AssetTypeInfo* info, AssetTypeID* outID);
Error AssetManager_CreateStandardAssetTypes(AssetManager* self, StandardAssetTypes* outTypes, JSONObjectPool** outDefinitionPool);
Error AssetManager_RemoveAssetType(AssetManager* self, AssetTypeID id);
Error AssetManager_GetAssetTypeName(AssetManager* self, AssetTypeID id, const unsigned char** outName);
Error AssetManager_GetAssetTypeDirectoryName(AssetManager* self, AssetTypeID id, const unsigned char** outName);
Error AssetManager_GetAssetTypeByName(AssetManager* self, const unsigned char* name, AssetTypeID* outID);

// ---- search roots / resource packs ----
Error AssetManager_AddSearchRoot(AssetManager* self, const unsigned char* directory);
Error AssetManager_InsertSearchRoot(AssetManager* self, size_t index, const unsigned char* directory);
Error AssetManager_RemoveSearchRoot(AssetManager* self, const unsigned char* directory);
size_t AssetManager_GetSearchRootCount(AssetManager* self);
Error AssetManager_GetSearchRoot(AssetManager* self, size_t index, const unsigned char** outDirectory);

// ---- reference resolution (in-memory / server) ----
Error AssetManager_SetReferenceResolver(AssetManager* self, AssetReferenceResolver resolver, const UserData* userData);
Error AssetManager_SetReferenceBytes(AssetManager* self, const unsigned char* referenceName, const unsigned char* bytes, size_t byteCount);
Error AssetManager_OpenResource(AssetManager* self, AssetTypeID assetType, const AssetLocation* location, IOStream** outStream);

// ---- path resolution (loaders that need a real file, e.g. models) ----
Error AssetManager_AcquireResourcePath(AssetManager* self, AssetTypeID assetType, const AssetLocation* location, const unsigned char* preferredExtension, AssetResourcePath** outHandle);
const unsigned char* AssetResourcePath_Get(AssetResourcePath* self);
Error AssetManager_ReleaseResourcePath(AssetManager* self, AssetResourcePath* handle);
Error AssetManager_SetCacheDirectory(AssetManager* self, const unsigned char* directory);

// ---- definitions ----
Error AssetManager_ReadDefinitions(AssetManager* self);
Error AssetManager_BuildDefinition(AssetManager* self, AssetTypeID type, const GenericBuffer* rawData, const unsigned char* sourceDescription, AssetDefinition** outDefinition);
Error AssetManager_SetDefinition(AssetManager* self, AssetDefinition* definition);
Error AssetManager_RemoveDefinition(AssetManager* self, AssetTypeID type, const unsigned char* name);
Error AssetManager_HasDefinition(AssetManager* self, AssetTypeID type, const unsigned char* name, bool* outExists);

// ---- users ----
Error AssetManager_GetNewUserID(AssetManager* self, AssetUserID* outID);
Error AssetManager_ReleaseAsset(AssetManager* self, AssetTypeID assetType, const unsigned char* name, AssetUserID user);
Error AssetManager_ReleaseAllAssetsForUser(AssetManager* self, AssetUserID user);
Error AssetManager_RetireUser(AssetManager* self, AssetUserID user);

// ---- single load (main thread, synchronous) ----
Error AssetManager_LoadAssetSingle(AssetManager* self, AssetTypeID assetType, const unsigned char* name, AssetUserID user, void** outAsset);

// ---- typed convenience loaders (thin inline wrappers over LoadAssetSingle; standard types only) ----
Error AssetManager_LoadSpriteSheet(AssetManager* self, const unsigned char* name, AssetUserID user, SpriteSheet** outAsset);
Error AssetManager_LoadSpriteAnimation(AssetManager* self, const unsigned char* name, AssetUserID user, SpriteAnimation** outAsset);
Error AssetManager_LoadSound(AssetManager* self, const unsigned char* name, AssetUserID user, GameSound** outAsset);
Error AssetManager_LoadFont(AssetManager* self, const unsigned char* name, AssetUserID user, GameFont** outAsset);
Error AssetManager_LoadShader(AssetManager* self, const unsigned char* name, AssetUserID user, GameShader** outAsset);
Error AssetManager_LoadModel(AssetManager* self, const unsigned char* name, AssetUserID user, GameModel** outAsset);

// ---- bulk load (stepped, main thread) ----
Error AssetManager_CreateAssetBulkOperation(AssetManager* self, AssetUserID user, AssetBulkOperation** outOp);
Error AssetBulkOperation_AddEntry(AssetBulkOperation* self, AssetTypeID assetType, const unsigned char* name, PromisedAsset** outPromise);
Error AssetBulkOperation_CompleteStep(AssetBulkOperation* self, bool* outDidWork);
bool  AssetBulkOperation_IsComplete(AssetBulkOperation* self);
Error AssetBulkOperation_Deconstruct(AssetBulkOperation* self);
AssetLoadProgress* AssetBulkOperation_GetProgress(AssetBulkOperation* self);
double AssetLoadProgress_GetFactor(AssetLoadProgress* self);
size_t AssetLoadProgress_GetItemCountTotal(AssetLoadProgress* self);
size_t AssetLoadProgress_GetItemCountProcessed(AssetLoadProgress* self);
bool   AssetLoadProgress_IsComplete(AssetLoadProgress* self);
bool   PromisedAsset_IsResolved(PromisedAsset* self);
Error  PromisedAsset_GetAsset(PromisedAsset* self, void** outAsset);
Error  PromisedAsset_GetError(PromisedAsset* self);

// ---- buffer pools ----
Error AssetManager_BorrowGenericBuffer(AssetManager* self, size_t elementSize, GenericBuffer** outBuffer);
Error AssetManager_ReturnGenericBuffer(AssetManager* self, GenericBuffer* buffer);
```

---

## 14. End-to-end usage sketches

**Startup + single load:**

```c
AssetManager* manager;
AssetManager_Construct1(&manager);

StandardAssetTypes types;
JSONObjectPool* defPool;
AssetManager_CreateStandardAssetTypes(manager, &types, &defPool); // shared JSON pool for all parsing

AssetManager_AddSearchRoot(manager, u8"assets/base");   // priority 0 (only root so far)
AssetManager_InsertSearchRoot(manager, 0, u8"assets/mods/cool_pack"); // now overrides base
AssetManager_ReadDefinitions(manager);

AssetUserID menuUser;
AssetManager_GetNewUserID(manager, &menuUser);

SpriteSheet* sheet;
AssetManager_LoadSpriteSheet(manager, u8"ui/main_menu", menuUser, &sheet); // typed convenience loader
// ... use sheet ...

AssetManager_ReleaseAllAssetsForUser(manager, menuUser); // menu closes; sheet unloads if unused elsewhere
```

**Stepped bulk load with a loading bar (all on the main thread):**

```c
AssetBulkOperation* op;
AssetManager_CreateAssetBulkOperation(manager, levelUser, &op);

PromisedAsset* pSheet;
PromisedAsset* pMusic;
AssetBulkOperation_AddEntry(op, types.SpriteSheet, u8"level1/tiles", &pSheet);
AssetBulkOperation_AddEntry(op, types.Sound,       u8"level1/theme", &pMusic);

AssetLoadProgress* progress = AssetBulkOperation_GetProgress(op);
while (!AssetBulkOperation_IsComplete(op))    // one asset per frame; UI stays live
{
    bool didWork;
    AssetBulkOperation_CompleteStep(op, &didWork);     // load ONE asset, then return

    BeginDrawing();
    DrawLoadingBar(AssetLoadProgress_GetFactor(progress));
    EndDrawing();                                       // render a frame between loads
}

SpriteSheet* tiles; GameSound* theme;
PromisedAsset_GetAsset(pSheet, (void**)&tiles);        // unwrap once, now they're plain pointers
PromisedAsset_GetAsset(pMusic, (void**)&theme);
AssetBulkOperation_Deconstruct(op);                    // tiles/theme stay alive: held by levelUser
```

---

## 15. Decisions

### Round 1

1. **Definition format decoupling** → *constructor parses raw bytes*. The manager and discovery layer
   never touch JSON; standard-type constructors use WRJSON internally, modded types pick any format.
2. **Hold semantics** → *presence-based / idempotent*. Each asset stores a set of user IDs; one release
   fully removes a user regardless of how many times it loaded the asset.
3. **Promised-asset ergonomics** → *poll progress, then unwrap once* via `PromisedAsset_GetAsset`.
4. **Threading** → *(revised in round 2, see below)*.
5. **Models** → target *OBJ + GLTF/GLB*; *separate* animation files bound at runtime; *overridable*
   materials; skeletal support kept as an opt-in extension over a primary static path (§12).

### Round 2

- **Threading (revises 4)** → **all loading is on the main thread.** Raylib's multi-threaded loading
  support is too poor/crash-prone. The manager is main-thread-affine with **no internal locking** (§10).
  Bulk operations are **stepped**: `AssetBulkOperation_CompleteStep` loads one asset and returns, so the
  game loop renders between loads. Accepted tradeoff: occasional per-asset lag spikes instead of crashes.
  This must be **documented** on every mutating call in the headers.
- **Partial bulk failure (3b)** → **per-promise errors; never cancel the rest.** `CompleteStep` returns
  success even when the asset it loaded failed; the failure lives on that entry's promise
  (`PromisedAsset_GetError`). Inspect per-entry results after `IsComplete`.
- **Models (5a)** → **ship the static path first.** The `model` type is static mesh + materials +
  transform now; the 3D `animation` type and runtime bind API (`GameModel_BindAnimation`,
  `GameModelAnimation`) are deferred until skeletal is decided.
- **Typed convenience loaders (6)** → **yes.** Thin inline wrappers over `LoadAssetSingle` for the
  standard types (`AssetManager_LoadSpriteSheet`, `_LoadModel`, …), avoiding `void**` casts at call sites.
- **Shared JSON object pool** → **done cleanly (§9.1).** One `JSONObjectPool` created by
  `AssetManager_CreateStandardAssetTypes`, shared via each type's `ConstructorUserData`, returned to the
  caller to own. JSON compounds/arrays/strings are recycled across every definition parse. The manager
  core stays JSON-free; single-threaded loading means no synchronization on the pool.

### Round 3

- **Raylib models are file-only** (no `LoadModelFromMemory`). Rather than compromise the stream API, the
  manager adds a second resolution primitive, `AssetManager_AcquireResourcePath` (§3), used only by
  path-requiring loaders. File locations resolve to their real path; reference/in-memory locations are
  materialized to a temp file in a manager-owned cache directory (cleared on construct + deconstruct),
  and the handle release deletes it. The model definition gains an optional `format` field, required only
  for reference models (to name the temp file's extension). Reference models must be self-contained
  single files (`.glb` recommended). Every non-model loader stays on the clean `OpenResource` stream API.

### Nothing outstanding (design)

All design questions are resolved.

---

## 16. Implementation status

**Done and verified (`gcc -std=c2x -Werror -Wall -Wextra -Wpedantic`, syntax-clean):**
- `include/AssetManager.h` — the complete public header (rewritten from the draft), with full Doxygen.
  Added `AssetManager_CloseResource` (a returned stream's heap wrapper + backing buffer can't be freed by
  `IOStream_Deconstruct` alone).
- `source/AssetManager.c` — the complete manager core: dynamic types, search roots (priority override),
  reference resolvers + in-memory bytes, `OpenResource`/`AcquireResourcePath` (stem-matched file
  resolution + temp-file materialization + cache dir), definitions (read/build/set/remove, priority
  override), users + presence-based holds + cascade-safe auto-unload, dependency-user machinery, single +
  typed convenience loaders, stepped bulk operations + promises, and the shared JSON pool wiring.
- `source/AssetTypesStandard.h` — private header declaring the six standard-type constructors + their
  type/directory names.

**Key internal design decisions (worth a look before the next increment):**
- **Interned string keys.** WR maps store keys by value with no string deep-copy, so `(type, name)` map
  keys hold a borrowed `Name` pointer into an owned, stable string (a definition's `Name`, or a loaded
  record's own copy). Comparator does a string compare.
- **Dependency user.** Each loaded asset gets its own internal `AssetUserID`; a loader attributes
  dependency loads to it, so dependencies release automatically when the asset unloads (reuses the holder
  machinery, no separate mechanism).
- **Cascade-safe unload.** Release removes the user from all holder sets (no map mutation), then
  repeatedly finds-and-unloads any zero-holder record, re-querying the live map each time so no record
  pointer dangles across a cascade.
- **Stream ownership.** `OpenResource` returns a heap `OpenedResource` (a `union { FileStream; MemoryStream }`
  first member, so the `IOStream*` casts back); `CloseResource` frees the wrapper + any backing buffer.

**Remaining increment (the concrete types — nothing actually loads until these land):**
- Six type modules implementing the constructors + `LoadAsset`/`Destroy` per `asset_structure.md`:
  `SpriteSheetDefinition`, `SpriteAnimationDefinition`, `SoundDefinition`, `FontDefinition`,
  `ShaderDefinition`, `ModelDefinition`. (New modules — flagged per the AGENTS "new module" rule.)
- Wrapper modules: `GameSound`, `GameShader` (new), and fixing `GameModel.h`'s `GameMode` typo.
- These are independent of each other given the fixed header contract, so they can be built (and reviewed)
  one at a time.
```
