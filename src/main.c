int _fltused;

#include <inttypes.h>
#include <stddef.h>

typedef uint8_t  u8;
typedef uint32_t u32;
typedef int8_t   i8;
typedef int32_t  i32;
typedef unsigned int uint;

#define FONT_SIZE 16
#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 50

#define BUFFER_WIDTH SCREEN_WIDTH*FONT_SIZE
#define BUFFER_HEIGHT SCREEN_HEIGHT*FONT_SIZE
u32 BackBuffer[BUFFER_WIDTH*BUFFER_HEIGHT];

#define OS_IMPLEMENTATION_WIN32
#include "os.h"

#define EZPLAT_IMPLEMENTATION
#include "ezplat.h"
ez Ez = {0};

#define EZIMG_IMPLEMENTATION
#include "ezimg.h"

void *
ReadEntireFile(char *FilePath, size_t *FileSize)
{
    if(!os_file_exists(FilePath))
    {
        return(0);
    }

    size_t _FileSize = os_file_size(FilePath);
    if(!_FileSize)
    {
        return(0);
    }

    void *FileContent = os_memory_alloc(_FileSize);
    if(!FileContent)
    {
        return(0);
    }

    if(os_file_read(FilePath, FileContent, _FileSize) != _FileSize)
    {
        os_memory_free(FileContent);
        return(0);
    }

    if(FileSize)
    {
        *FileSize = _FileSize;
    }

    return(FileContent);
}

typedef struct
image
{
    uint Width;
    uint Height;
    void *Pixels;
} image;

image
LoadImagePng(char *FilePath)
{
    image Result = {0};

    size_t FileSize;
    void *FileContent = ReadEntireFile(FilePath, &FileSize);
    if(!FileContent || !FileSize)
    {
        return(Result);
    }

    uint ImageSize = ezimg_png_size(FileContent, (uint)FileSize);
    void *Pixels = os_memory_alloc(ImageSize);
    if(!Pixels)
    {
        os_memory_free(FileContent);
        return(Result);
    }

    uint Width, Height;
    int ImageLoadResult = ezimg_png_load(
        FileContent, (uint)FileSize,
        Pixels, ImageSize,
        &Width, &Height);
    if(ImageLoadResult != EZIMG_OK)
    {
        os_memory_free(FileContent);
        os_memory_free(Pixels);
        return(Result);
    }

    /* Transform ARGB to BGRA */
    for(uint PixelIndex = 0;
        PixelIndex < Width*Height;
        ++PixelIndex)
    {
        u8 R, G, B, A;
        u32 Pixel;

        Pixel = ((u32 *)Pixels)[PixelIndex];

        A = (u8)(((Pixel & 0x000000ff) >>  0) & 0xff);
        R = (u8)(((Pixel & 0x0000ff00) >>  8) & 0xff);
        G = (u8)(((Pixel & 0x00ff0000) >> 16) & 0xff);
        B = (u8)(((Pixel & 0xff000000) >> 24) & 0xff);

        Pixel =
            (((u32)B & 0xff) << 0) |
            (((u32)G & 0xff) << 8) |
            (((u32)R & 0xff) << 16) |
            (((u32)A & 0xff) << 24);

        ((u32 *)Pixels)[PixelIndex] = Pixel;
    }

    Result.Width = Width;
    Result.Height = Height;
    Result.Pixels = Pixels;

    return(Result);
}

void
ClearBackBuffer(u32 Color)
{
    for(uint Index = 0;
        Index < BUFFER_WIDTH*BUFFER_HEIGHT;
        ++Index)
    {
        BackBuffer[Index] = Color;
    }
}

void
DrawImage(image Image, uint SrcX, uint SrcY, uint SrcW, uint SrcH, uint DestX, uint DestY)
{
    if(SrcX >= Image.Width || SrcY >= Image.Height)
    {
        return;
    }

    if(SrcW > Image.Width)
    {
        SrcW = Image.Width;
    }

    if(SrcH > Image.Height)
    {
        SrcH = Image.Height;
    }

    /* TODO */
    for(uint Y = 0; Y < SrcH; ++Y)
    {
        for(uint X = 0; X < SrcW; ++X)
        {
            if( (SrcY+Y) < Image.Height &&
                (SrcX+X) < Image.Width &&
                (DestY+Y) < SCREEN_HEIGHT*FONT_SIZE &&
                (DestX+X) < SCREEN_WIDTH*FONT_SIZE)
            {
                BackBuffer[(DestY+Y)*SCREEN_WIDTH*FONT_SIZE + (DestX+X)] = 
                    ((u32 *)Image.Pixels)[(SrcY+Y)*Image.Width + (SrcX+X)];
            }
        }
    }
}

void
DrawImageMono(image Image, uint SrcX, uint SrcY, uint SrcW, uint SrcH, uint DestX, uint DestY, u32 Color)
{
    if(SrcX >= Image.Width || SrcY >= Image.Height)
    {
        return;
    }

    if(SrcW > Image.Width)
    {
        SrcW = Image.Width;
    }

    if(SrcH > Image.Height)
    {
        SrcH = Image.Height;
    }

    /* TODO */
    for(uint Y = 0; Y < SrcH; ++Y)
    {
        for(uint X = 0; X < SrcW; ++X)
        {
            if( (SrcY+Y) < Image.Height &&
                (SrcX+X) < Image.Width &&
                (DestY+Y) < SCREEN_HEIGHT*FONT_SIZE &&
                (DestX+X) < SCREEN_WIDTH*FONT_SIZE)
            {
                u32 PixelColor = ((u32 *)Image.Pixels)[(SrcY+Y)*Image.Width + (SrcX+X)];
                if((PixelColor & 0xffffff) == 0xffffff)
                {
                    PixelColor = Color;
                    BackBuffer[(DestY+Y)*SCREEN_WIDTH*FONT_SIZE + (DestX+X)] = PixelColor;
                }
            }
        }
    }
}

image FontImage;

void
DrawChar(int CharToDraw, uint X, uint Y, u32 Color)
{
    uint SrcX = ((uint)CharToDraw%16)*FONT_SIZE;
    uint SrcY = ((uint)CharToDraw/16)*FONT_SIZE;
    DrawImageMono(FontImage, SrcX, SrcY, FONT_SIZE, FONT_SIZE, X*FONT_SIZE, Y*FONT_SIZE, Color);
}

typedef enum
action_type
{
    ACT_NONE,
    ACT_MOVE,
    ACT_ESCAPE,
    ACT_COUNT
} action_type;

typedef struct
action
{
    action_type Type;
    int Dx, Dy;
} action;

typedef struct
entity
{
    int Alive;
    int RenderType;
    int X, Y;
    u32 Color;
} entity;

#define MAX_ENTITIES 100
entity Entities[MAX_ENTITIES];
uint EntityCount;

entity *
CreateEntity()
{
    if(EntityCount >= MAX_ENTITIES)
    {
        return(0);
    }

    entity *Entity = &Entities[EntityCount];
    EntityCount += 1;

    Entity->Alive = 1;

    return(Entity);
}

void
MoveEntity(entity *Entity, int Dx, int Dy)
{
    Entity->X += Dx;
    Entity->Y += Dy;
}

void
DrawEntity(entity *Entity)
{
    if(Entity->RenderType >= 0 && Entity->RenderType < 256)
    {
        DrawChar(
            Entity->RenderType,
            (uint)Entity->X, (uint)Entity->Y,
            Entity->Color);
    }
}

#include <windows.h>

int GameIsRunning;

void
main(void)
{
    Ez.Display.Name = "r0gu3";
    Ez.Display.Width = FONT_SIZE*SCREEN_WIDTH;
    Ez.Display.Height = FONT_SIZE*SCREEN_HEIGHT;
    Ez.Display.Pixels = (void *)BackBuffer;
    Ez.Display.RenderingType = EZ_RENDERING_SOFTWARE;
    Ez.Display.PixelFormat = EZ_PIXEL_FORMAT_ARGB;

    if(!EzInitialize(&Ez))
    {
        ExitProcess(1);
    }

    entity *Player = CreateEntity();
    Player->RenderType = '@';
    Player->Color = 0xffffff;
    Player->X = SCREEN_WIDTH/2;
    Player->Y = SCREEN_HEIGHT/2;

    entity *Npc = CreateEntity();
    Npc->RenderType = 'M';
    Npc->Color = 0xff0000;
    Npc->X = SCREEN_WIDTH/2 - 5;
    Npc->Y = SCREEN_HEIGHT/2 - 3;

    FontImage = LoadImagePng("res/font16x16.png");
    GameIsRunning = 1;
    while(GameIsRunning && Ez.Running)
    {
        EzUpdate(&Ez);

        /* Input */
        action Action = {0};
        if(Ez.Input.Keys[EZ_KEY_UP].Pressed)
        {
            Action.Type = ACT_MOVE;
            Action.Dx = 0;
            Action.Dy = -1;
        }
        else if(Ez.Input.Keys[EZ_KEY_DOWN].Pressed)
        {
            Action.Type = ACT_MOVE;
            Action.Dx = 0;
            Action.Dy = 1;
        }
        else if(Ez.Input.Keys[EZ_KEY_LEFT].Pressed)
        {
            Action.Type = ACT_MOVE;
            Action.Dx = -1;
            Action.Dy = 0;
        }
        else if(Ez.Input.Keys[EZ_KEY_RIGHT].Pressed)
        {
            Action.Type = ACT_MOVE;
            Action.Dx = 1;
            Action.Dy = 0;
        }
        else if(Ez.Input.Keys[EZ_KEY_ESCAPE].Pressed)
        {
            Action.Type = ACT_ESCAPE;
        }

        /* Logic */
        switch(Action.Type)
        {
            case ACT_MOVE:
            {
                MoveEntity(Player, Action.Dx, Action.Dy);
            } break;

            case ACT_ESCAPE:
            {
                GameIsRunning = 0;
            } break;
        }

        if(!GameIsRunning)
        {
            break;
        }

        /* Render */
        ClearBackBuffer(0x000000);
        DrawEntity(Player);
        DrawEntity(Npc);
    }

    EzClose(&Ez);

    ExitProcess(0);
}
