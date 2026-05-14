#pragma once
#include "raylib/raylib.h"
#include <stddef.h>
#include "wr/WRCompile.h"


/**
 * The renderer stuff for ErrDLogi.
 * 
 * The render context is used for all render functions. 
 * It can have an optional custom render buffer which is just a target to render to. If it isn't present
 * (_hasCustomRenderBuffer is false), then the rendering is done to the window's backbuffer and the render buffer properties
 * are those of the window backbuffer.
 * The render context is supposed to be constructed before rendering. It is safe and recommended to just create the render context objects
 * via the create method before rendering a frame, they can be disposed after the frame. Though probably shouldn't heap allocate them
 * every frame, use stack variables instead, as the render context objects do not require any data to be stored on the heap.
 * 
 * 
 * To help make rendering more consistent across screen sizes, the render context stores some aspect ratio data.
 * The target aspect ratio is the aspect ratio that the game is supposed to be viewed in. An imaginary area called the
 * render target area, which is the target aspect ratio, is placed in the render buffer. It is fitted perfectly so that one if its
 * lengths is the render buffer's length. For example, a target ratio of 16:9 on a 1920*1200 render buffer, the render target area would
 * be 1920*1080, the remaining 120 pixels would be outside of it. 
 *     The render target area relative position just says where to
 * place the target area if the render buffer's aspect ratio isn't the target one. If set to (0.5, 0.5), then the target area will
 * always be placed in the middle of the render buffer with even spacing on both sides (horizontal or vertical) if the render buffer's
 * aspect ratio is not the target aspect ratio. Following the previous example, that would be 60 pixels from the top,
 * with 60 remaining to the bottom.
 * 
 * The render position for 2D textures and text is specified by a render position struct. It has a vector value and render type.
 * If set to pixel type, the vector represents render buffer pixel coordinates.
 * If set to normalized relative, it represents a normalized [0;1] coordinate position in the render buffer.
 * If set to normalized fitted, the normalized [0;1] coordinates correspond to the target render area, not whole render buffer.
 * In all cases, the coordinates may be out of their range and still be fine (so not hard-clamped to their range).
 * 
 * The render size for 2D textures works the same as the position. Either pixel size, window size or render target area size.
 * 
 * For fonts, a normalized size of 1 means that vertically a single line's height will take up 100%
 * of the render value type height. That means, for normalized fitted coords: 100% of the render target area height.
 * For normalized relative: 100% of the render buffer height. Pixel value type still means 1 pixel for such a character,
 */


typedef struct RenderContextStruct
{
    bool _hasCustomRenderBuffer;
    RenderTexture2D _renderBuffer;
    Vector2 _renderBufferSizePixels;
    float _renderBufferAspectRatio;

    float _targetAspectRatio;
    Vector2 _targetRelativePosition;

    size_t _textureDrawCount;
    size_t _stringDrawCount;
} RenderContext;

typedef struct RenderColorStruct
{
    Color Tint; // Alpha value ignored.
    float Brightness; // [0;1]
    float Opacity; // [0;1]
} RenderColor;

typedef enum RenderValueTypeEnum
{
    /* Pixel coordinate. */
    RenderValueType_Pixel, 

    /* Values in range [0;1], (0, 0) being top left and (1, 1) bottom right. */
    RenderValueType_NormalizedRelative,

    /* Values where the range [0;1]. (0, 0) being top left of the targeted render area, (1, 1) being bottom right of it. */
    RenderValueType_NormalizedFitted 
} RenderValueType;

typedef struct RenderFloatStruct
{
    float Value;
    RenderValueType Type;
} RenderFloat;

typedef struct RenderVector2DStruct
{
    Vector2 Value;
    RenderValueType Type;
} RenderVector2D;

typedef struct TextureRenderArgumentsStruct
{
    Texture2D Texture;
    RenderVector2D Position; // Position, the texture will be rendered as if the origin is placed here.
    Rectangle RelativeSourceRectangle; // Area of the texture to draw.
    RenderVector2D Size; // Total size of the final rectangle that will be drawn.
    Vector2 RelativeOrigin; // Relative origin in the texture.
    float RotationRad; // Rotation around origin, in radians.
    RenderColor TargetColor; // The rendering color.
} TextureRenderArguments;

typedef struct TextRenderArgumentsStruct
{
    const unsigned char* Text;
    Font TargetFont;
    RenderVector2D Position;
    RenderFloat Size;
    RenderFloat Spacing;
    Vector2 RelativeOrigin;
    float RotationRad;
    RenderColor TargetColor;

    /* For performance reasons it is recommended to pass in the fitted draw size of the text to the draw functions.
    * If the draw size isn't cached and has to be recalculated for the calls anyway, it can not be passed and the calculation
    * will be done automatically. */
    Vector2 CachedFittedDrawSize;
    bool HasCachedFittedDrawSize;
} TextRenderArguments;


// Functions.
static inline RenderFloat RenderFloat_Window(float value)
{
    return (RenderFloat) 
    {
        .Value = value,
        .Type = RenderValueType_Pixel
    };
}

static inline RenderFloat RenderFloat_Relative(float value)
{
    return (RenderFloat) 
    {
        .Value = value,
        .Type = RenderValueType_NormalizedRelative
    };
}

static inline RenderFloat RenderFloat_Fitted(float value)
{
    return (RenderFloat) 
    {
        .Value = value,
        .Type = RenderValueType_NormalizedFitted
    };
}

static inline RenderVector2D RenderVector2D_Window(Vector2 value)
{
    return (RenderVector2D) 
    {
        .Value = value,
        .Type = RenderValueType_Pixel
    };
}

static inline RenderVector2D RenderVector2D_Relative(Vector2 value)
{
    return (RenderVector2D) 
    {
        .Value = value,
        .Type = RenderValueType_NormalizedRelative
    };
}

static inline RenderVector2D RenderVector2D_Fitted(Vector2 value)
{
    return (RenderVector2D) 
    {
        .Value = value,
        .Type = RenderValueType_NormalizedFitted
    };
}


Color RenderColor_GetFinalColor(RenderColor color);

static inline RenderColor RenderColor_White(void)
{
    return (RenderColor) 
    {
        .Tint = WHITE,
        .Brightness = 1.0f,
        .Opacity = 1.0f
    };
}

static inline RenderColor RenderColor_Black(void)
{
    return (RenderColor) 
    {
        .Tint = WHITE,
        .Brightness = 0.0f,
        .Opacity = 1.0f
    };
}

static inline RenderColor RenderColor_Transparent(void)
{
    return (RenderColor) 
    {
        .Tint = WHITE,
        .Brightness = 1.0f,
        .Opacity = 0.0f
    };
}


/* Render buffer may be null to indicate the window's backbuffer.  */
void RenderContext_Create(RenderContext* self,
    RenderTexture2D* renderBuffer,
    float targetAspectRatio,
    Vector2 targetRelativePosition);

/* Does NOT free the render buffer. */
void RenderContext_Deconstruct(RenderContext* self);

static inline Vector2 RenderTargetPosition_Centered(void)
{
    return (Vector2) 
    {
        .x = 0.5f,
        .y = 0.5f
    };
}


void RenderContext_RenderTexture2D(RenderContext* self, const TextureRenderArguments* args);

void RenderContext_RenderText2D(RenderContext* self, const TextRenderArguments* args);

void RenderContext_BeginRendering(RenderContext* self);

void RenderContext_EndRendering(RenderContext* self);


/* From relative. */
static inline Vector2 RenderContext_VectorRelativeToPixel(RenderContext* self, Vector2 relativeCoords)
{
    return (Vector2) 
    {
        .x = relativeCoords.x * self->_renderBufferSizePixels.x,
        .y = relativeCoords.y * self->_renderBufferSizePixels.y,
    };
}

static inline Vector2 RenderContext_VectorRelativeToFitted(RenderContext* self, Vector2 relativeCoords, bool isOffsetIncluded)
{
    const float AspectRatioQuotient = self->_targetAspectRatio / self->_renderBufferAspectRatio;
    const float FittedWidth = (AspectRatioQuotient >= 1.0f) ? 1.0f : AspectRatioQuotient;
    const float FittedHeight = (AspectRatioQuotient >= 1.0f) ? (1.0f / AspectRatioQuotient) : 1.0f;

    Vector2 Result = relativeCoords;

    if (isOffsetIncluded)
    {
        Result.x -= (1.0f - FittedWidth) * self->_targetRelativePosition.x;
        Result.y -= (1.0f - FittedHeight) * self->_targetRelativePosition.y;
    }

    Result.x /= FittedWidth;
    Result.y /= FittedHeight;

    return Result;
}


/* From pixel. */
static inline Vector2 RenderContext_VectorPixelToRelative(RenderContext* self, Vector2 pixelCoords)
{
    return (Vector2) 
    {
        .x = pixelCoords.x / self->_renderBufferSizePixels.x,
        .y = pixelCoords.y / self->_renderBufferSizePixels.y
    };
}

static inline Vector2 RenderContext_VectorPixelToFitted(RenderContext* self, Vector2 pixelCoord, bool isOffsetIncluded)
{
    return RenderContext_VectorRelativeToFitted(self, RenderContext_VectorPixelToRelative(self, pixelCoord), isOffsetIncluded);
}



/* From fitted. */
static inline Vector2 RenderContext_VectorFittedToRelative(RenderContext* self, Vector2 fittedCoords, bool isOffsetIncluded)
{
    float AspectRatioQuotient = self->_targetAspectRatio / self->_renderBufferAspectRatio;
    float FittedWidth = (AspectRatioQuotient >= 1.0f) ? 1.0f : AspectRatioQuotient;
    float FittedHeight = (AspectRatioQuotient >= 1.0f) ? (1.0f / AspectRatioQuotient) : 1.0f;

    Vector2 Result;
    Result.x = fittedCoords.x * FittedWidth;
    Result.y = fittedCoords.y * FittedHeight;

    if (isOffsetIncluded)
    {
        Result.x += (1.0f - FittedWidth) * self->_targetRelativePosition.x;
        Result.y += (1.0f - FittedHeight) * self->_targetRelativePosition.y;
    }

    return Result;
}


static inline Vector2 RenderContext_VectorFittedToPixel(RenderContext* self, Vector2 fittedCoords, bool isOffsetIncluded)
{
    Vector2 RelativeCoords = RenderContext_VectorFittedToRelative(self, fittedCoords, isOffsetIncluded);
    RelativeCoords.x *= self->_renderBufferSizePixels.x;
    RelativeCoords.y *= self->_renderBufferSizePixels.y;
    return RelativeCoords;
}


/* Vertical scale. */
static inline float RenderContext_SizeFittedToRelative(RenderContext* self, float fittedSize)
{
    const float AspectRatioQuotient = self->_targetAspectRatio / self->_renderBufferAspectRatio;
    const float FittedHeight = (AspectRatioQuotient >= 1.0f) ? (1.0f / AspectRatioQuotient) : 1.0f;
    return fittedSize * FittedHeight;
}

static inline float RenderContext_SizeRelativeToFitted(RenderContext* self, float relativeSize)
{
    const float AspectRatioQuotient = self->_targetAspectRatio / self->_renderBufferAspectRatio;
    const float FittedHeight = (AspectRatioQuotient >= 1.0f) ? (1.0f / AspectRatioQuotient) : 1.0f;
    return relativeSize / FittedHeight;
}

static inline float RenderContext_SizeRelativeToPixel(RenderContext* self, float relativeSize)
{
    return relativeSize * self->_renderBufferSizePixels.y;
}

static inline float RenderContext_SizePixelToRelative(RenderContext* self, float pixelSize)
{
    return pixelSize / self->_renderBufferSizePixels.y;
}

static inline float RenderContext_SizeFittedToPixel(RenderContext* self, float fittedSize)
{
    return RenderContext_SizeRelativeToPixel(self, RenderContext_SizeFittedToRelative(self, fittedSize));
}

static inline float RenderContext_SizePixelToFitted(RenderContext* self, float pixelSize)
{
    return RenderContext_SizeRelativeToFitted(self, RenderContext_SizePixelToRelative(self, pixelSize));
}


/* Text. */
static inline Vector2 RenderContext_MeasureTextPixels(RenderContext* self,
    Font font,
    const unsigned char* text,
    float fontSizePixels,
    float spacing)
{
    UNUSED(self);
    return MeasureTextEx(font, (const char*)text, fontSizePixels, spacing);
}

static inline Vector2 RenderContext_MeasureTextFitted(RenderContext* self,
    Font font,
    const unsigned char* text,
    float fontSizeFitted,
    float spacing)
{
    UNUSED(self);
    return MeasureTextEx(font, (const char*)text, fontSizeFitted, spacing);
}