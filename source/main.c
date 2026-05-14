#include <stdio.h>
#include "Renderer.h"
#include "wr/WRMemory.h"

int main(void)
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1280, 720, "Test");

    Texture2D TestTexture = LoadTexture("/home/wooden_chest/Desktop/ghdf/test.png");

    RenderContext Context;
    RenderContext_Create(&Context, NULL, 16.0f / 9.0f, RenderTargetPosition_Centered());

    // Font DefaultFont = GetFontDefault();
    
    // const unsigned char* Text = u8"0";
    // Vector2 TextSize = RenderContext_MeasureTextFitted(&Context, DefaultFont, Text, 1.0f, 0.0f);

    while (!WindowShouldClose())
    {
        RenderContext_BeginRendering(&Context);

        TextureRenderArguments TextureArgs =
        {
            .Position = RenderVector2D_Fitted((Vector2) { .x = 0.5f, .y = 0.5f }),
            .RelativeOrigin = (Vector2) { .x = 0.5f, .y = 0.5f },
            .RelativeSourceRectangle = (Rectangle) { .x = 0, .y = 0, .width = 1, .height = 1 },
            .RotationRad = 0.0f,
            .TargetColor = RenderColor_White(),
            .Size = RenderVector2D_Fitted((Vector2) { .x = 1, .y = 1 }),
            .Texture = TestTexture,
        };

        RenderContext_RenderTexture2D(&Context, &TextureArgs);

        RenderContext_EndRendering(&Context);
    }

    return 0;
}