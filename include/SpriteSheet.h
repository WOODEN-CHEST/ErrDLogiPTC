#pragma once
#include "wr/WRMemory.h"
#include "wr/WRError.h"
#include "raylib/raylib.h"


/**
 * A sprite sheet just stores a bunch of sprites in a single image for performance reasons.
 * 
 * The sprite sheet object does not own the buffer or texture passed into it and thus simply
 * deconstructing the sprite sheet does not free the memory it uses. That has to be done
 * by the same part of the code which constructed and passed in the texture and buffer when constructing the sprite sheet.
 */


#define SPRITE_SHEET_ENTRY_MAX_NAME_LENGTH 128

typedef struct SpriteSheetEntryStruct
{
    unsigned char _name[SPRITE_SHEET_ENTRY_MAX_NAME_LENGTH];
    Rectangle _textureArea;
} SpriteSheetEntry;

typedef struct SpriteSheetStruct
{
    Texture2D _texture;
    GenericBuffer* _entries;
} SpriteSheet;


// Functions.
Error SpriteSheet_Construct1(SpriteSheet* self, Texture2D texture, GenericBuffer* entries);

Error SpriteSheet_Deconstruct(SpriteSheet* self);

/* Returns error if no entry with the given name exists. */
Error SpriteSheet_GetTextureArea(SpriteSheet* self, const unsigned char* entryName, Rectangle* outArea);