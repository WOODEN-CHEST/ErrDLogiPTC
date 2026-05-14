#include "Renderer.h"
#include <limits.h>
#include <stdint.h>
#include "wr/WRMemory.h"
#include "wr/WRMath.h"


// Static functions.
static inline Vector2 RenderContext_GetPixelVector(RenderContext* self, RenderVector2D vector, bool isOffsetIncluded)
{
    if (vector.Type == RenderValueType_NormalizedFitted)
    {
        return RenderContext_VectorFittedToPixel(self, vector.Value, isOffsetIncluded);
    }
    if (vector.Type == RenderValueType_NormalizedRelative)
    {
        return RenderContext_VectorRelativeToPixel(self, vector.Value);
    }
    return vector.Value;
}



// Functions.
Color RenderColor_GetFinalColor(RenderColor color)
{
    Color FinalColor = color.Tint;

    FinalColor.a = (unsigned char)(UCHAR_MAX * color.Opacity);

    FinalColor.r = (unsigned char)(FinalColor.r * color.Brightness);
    FinalColor.g = (unsigned char)(FinalColor.g * color.Brightness);
    FinalColor.b = (unsigned char)(FinalColor.b * color.Brightness);

    return FinalColor;
}

void RenderContext_Deconstruct(RenderContext* self)
{
    Memory_Zero(self, sizeof(*self));
}

void RenderContext_Create(RenderContext* self,
    RenderTexture2D* renderBuffer,
    float targetAspectRatio,
    Vector2 targetRelativePosition)
{
    Vector2 RenderBufferSize;

    if (renderBuffer)
    {
        self->_renderBuffer = *renderBuffer;
        self->_hasCustomRenderBuffer = true;
        RenderBufferSize = (Vector2)
        {
            .x = renderBuffer->texture.width,
            .y = renderBuffer->texture.height
        };
    }
    else
    {
        Memory_Zero(&self->_renderBuffer, sizeof(self->_renderBuffer));
        self->_hasCustomRenderBuffer = false;
        RenderBufferSize = (Vector2)
        {
            .x = GetRenderWidth(),
            .y = GetRenderHeight(),
        };
    }

    self->_renderBufferSizePixels = RenderBufferSize;
    self->_renderBufferAspectRatio = RenderBufferSize.x / RenderBufferSize.y;

    self->_targetAspectRatio = targetAspectRatio; 
    self->_targetRelativePosition = targetRelativePosition;

    self->_textureDrawCount = 0;
    self->_stringDrawCount = 0;
}

void RenderContext_RenderTexture2D(RenderContext* self, const TextureRenderArguments* args)
{
    Vector2 TextureSize = (Vector2) 
    {
        .x = args->Texture.width,
        .y = args->Texture.height,
    };
    Vector2 PixelPosition = RenderContext_GetPixelVector(self, args->Position, true);
    Vector2 PixelSize = RenderContext_GetPixelVector(self, args->Size, false);
    Rectangle Source = (Rectangle) 
    {
        .x = args->RelativeSourceRectangle.x * TextureSize.x,
        .y = args->RelativeSourceRectangle.y * TextureSize.y,
        .width = args->RelativeSourceRectangle.width * TextureSize.x,
        .height = args->RelativeSourceRectangle.height * args->Texture.height,
    };
    Rectangle Destination = (Rectangle) 
    {
        .x = PixelPosition.x - (PixelSize.x * args->RelativeOrigin.x),
        .y = PixelPosition.y - (PixelSize.y * args->RelativeOrigin.y),
        .width = PixelSize.x,
        .height = PixelSize.y
    };
        Vector2 PixelOrigin = (Vector2) 
    {
        .x = Source.x + (Source.width * args->RelativeOrigin.x),
        .y = Source.y + (Source.height * args->RelativeOrigin.y),
    };
    Color FinalTint = RenderColor_GetFinalColor(args->TargetColor);
    float RotationDeg = Math_RadToDegFloat(args->RotationRad);
    DrawTexturePro(args->Texture,
        Source,
        Destination,
        PixelOrigin,
        RotationDeg,
        FinalTint);
    self->_textureDrawCount++;
}

void RenderContext_RenderText2D(RenderContext* self, const TextRenderArguments* args);

void RenderContext_BeginRendering(RenderContext* self)
{
    if (self->_hasCustomRenderBuffer)
    {
        BeginTextureMode(self->_renderBuffer);
    }
    else
    {
        BeginDrawing();
    }
}

void RenderContext_EndRendering(RenderContext* self)
{
    if (self->_hasCustomRenderBuffer)
    {
        EndTextureMode();
    }
    else
    {
        EndDrawing();
    }
}