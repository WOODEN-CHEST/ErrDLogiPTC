#include <stdio.h>
#include "Renderer.h"
#include "wr/WRMemory.h"

int main(void)
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1280, 720, "Test");

    Texture2D TestTexture = LoadTexture("/home/wooden_chest/Desktop/ghdf/test.png");

    Font DefaultFont = GetFontDefault();
    
    const float TEXT_SPACING = 0.075f;
    const unsigned char* Text = u8"ABCDEF";
    Vector2 TextSize = Renderer_MeasureTextNormalized(DefaultFont, Text, TEXT_SPACING);

    while (!WindowShouldClose())
    {
        RenderContext Context;
        RenderContext_Create(&Context, NULL, 16.0f / 9.0f, RenderTargetPosition_Centered());

        ClearBackground(YELLOW);

        RenderContext_BeginRendering(&Context);

        TextureRenderArguments TextureArgs =
        {
            .Position = RenderVector2D_Fitted((Vector2) { .x = 0.5f, .y = 0.5f }),
            .RelativeOrigin = (Vector2) { .x = 0.5f, .y = 0.5f },
            .RelativeSourceRectangle = (Rectangle) { .x = 0, .y = 0, .width = 1, .height = 1 },
            .RotationRad = 0.785f,
            .TargetColor = RenderColor_White(),
            .Size = RenderVector2D_Fitted((Vector2) { .x = 1, .y = 1 }),
            .Texture = TestTexture,
        };

        RenderContext_RenderTexture2D(&Context, &TextureArgs);


        TextRenderArguments TextArgs = 
        {
            .Position = RenderVector2D_Fitted((Vector2) { .x = 0.5f, .y = 0.5f }),
            .SizeRelativeSpacing = TEXT_SPACING,
            .RelativeOrigin = (Vector2) { .x = 0.5f, .y = 0.5f },
            .Size = RenderFloat_Fitted(0.25f),
            .RotationRad = 0.785f,
            .Text = Text,
            .TargetFont = DefaultFont,
            .TargetColor = (RenderColor) { .Tint = RED, .Brightness = 1.0f, .Opacity = 1.0f },

            .HasCachedDrawSize = true,
            .CachedDrawSize = TextSize
        };

        RenderContext_RenderText2D(&Context, &TextArgs);

        RenderContext_EndRendering(&Context);
    }

    return 0;
}