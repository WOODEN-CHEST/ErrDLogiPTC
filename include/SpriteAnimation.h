#pragma once
#include "raylib/raylib.h"
#include "wr/WRMemory.h"
#include "wr/WREvent.h"
#include "wr/WRError.h"

/**
 * 2D animation related things.
 * 
 * Sprite animations are made up of frames. Each frame can either have a standalone texture or have the texture be borrowed
 * from a sprite sheet. There is no requirement that all frames be of same type.
 * The sprite animation does not own any of the buffers and textured passed into it, thus deconstructing a sprite animation
 * doesn't free the data passed into it. That has to be done from where the animation was constructed.
 * 
 * An animation has a default framerate, frame step (frame increment amount per frame change), whether they are looped or running.
 * 
 * Animation instances contain actual animation state and are instances of animations.
 * Animation instances created via their constructor start with no backed animation (this is valid).
 * Ones created via the animation class' create instance method start out with the backed animation and its default parameters.
 * The sprite animation instance events are raised just after they happen, so the animation state is post-event.
 * Subscribers of the sprite animation events are allowed to modify the sprite animation during the event handler, because
 * everything has already been processed.
 **/


#define SPRITE_ANIMATION_FPS_MIN 0.0
#define SPRITE_ANIMATION_FPS_MAX 1e30
#define SPRITE_ANIMATION_FRAME_STEP_MIN -1000000000
#define SPRITE_ANIMATION_FRAME_STEP_MAX 1000000000

#define SPRITE_ANIMATION_FPS_DEFAULT 0.0
#define SPRITE_ANIMATION_FRAME_STEP_DEFAULT 0
#define SPRITE_ANIMATION_IS_RUNNING_DEFAULT false
#define SPRITE_ANIMATION_IS_LOOPED_DEFAULT false



typedef struct SpriteAnimationFrameStruct
{
    Texture2D _texture;
    Rectangle _areaInTexture;
    bool _isTextureStandalone;
} SpriteAnimationFrame;

typedef struct SpriteAnimationStruct
{
    GenericBuffer* _frames;
    double _defaultFPS;
    int64_t _defaultFrameStep;
    bool _defaultIsRunning;
    bool _defaultIsLooped;
} SpriteAnimation;


typedef enum SpriteAnimationReachedStateEnum
{
    SpriteAnimationReachedState_None,
    SpriteAnimationReachedState_End,
    SpriteAnimationReachedState_Loop
} SpriteAnimationReachedState;

typedef enum SpriteAnimationStateDirectionEnum
{
    SpriteAnimationStateDirection_None,
    SpriteAnimationStateDirection_Forwards,
    SpriteAnimationStateDirection_Backwards
} SpriteAnimationStateDirection;

typedef struct SpriteAnimationEndArgs
{
    SpriteAnimationStateDirection _direction;
};

typedef struct SpriteAnimationLoopArgs
{
    SpriteAnimationStateDirection _direction;
};

typedef union SpriteAnimationEventSpecificArgsUnion
{
    SpriteAnimationEndArgs _endArgs;
    SpriteAnimationLoopArgs _loopArgs;
} SpriteAnimationEventSpecificArgs;

typedef struct SpriteAnimationStateReachEventArgsStruct
{
    SpriteAnimationInstance* _animationInstance;
    SpriteAnimationReachedState _reachedState;
    SpriteAnimationEventSpecificArgs _args;

} SpriteAnimationStateReachEventArgs;

typedef struct SpriteAnimationInstanceStruct
{
    SpriteAnimation* _source;
    size_t _frameIndex;
    double _timeSinceFrameChangeSeconds;
    double _fps;
    int64_t _frameStep;
    bool _isRunning;
    bool _isLooped;
    WREvent _stateReachEvent;
} SpriteAnimationInstance;


// Functions.
Error SpriteAnimation_Construct1(SpriteAnimation* self, GenericBuffer* frames);

Error SpriteAnimation_Deconstruct1(SpriteAnimation* self);

Error SpriteAnimation_CreateInstance(SpriteAnimation* self, SpriteAnimationInstance* outInstance);


Error SpriteAnimationInstance_Construct1(SpriteAnimationInstance* self);

Error SpriteAnimationInstance_Deconstruct(SpriteAnimationInstance* self);

/* Copies the animation state except for the actual animation which this instance uses. */
Error SpriteAnimationInstance_CopyAnimationStateTo(SpriteAnimationInstance* self, SpriteAnimationInstance* destination);

/* Copies everything to the destination. */
Error SpriteAnimationInstance_CopyEntireStateTo(SpriteAnimationInstance* self, SpriteAnimationInstance* destination);

static inline double SpriteAnimationInstance_GetFPS(SpriteAnimationInstance* self)
{
    return self->_fps;
}

Error SpriteAnimationInstance_SetFPS(SpriteAnimationInstance* self, double fps);

static inline size_t SpriteAnimationInstance_GetFrameIndex(SpriteAnimationInstance* self)
{
    return self->_frameIndex;
}

Error SpriteAnimationInstance_SetFrameIndex(SpriteAnimationInstance* self, size_t frameIndex);

static inline int64_t SpriteAnimationInstance_GetFrameStep(SpriteAnimationInstance* self)
{
    return self->_frameStep;
}

Error SpriteAnimationInstance_SetFrameStep(SpriteAnimationInstance* self, int64_t frameStep);

static inline double SpriteAnimationInstance_GetSecondsSinceFrameChange(SpriteAnimationInstance* self)
{
    return self->_timeSinceFrameChangeSeconds;
}

Error SpriteAnimationInstance_SetSecondsSinceFrameChange(SpriteAnimationInstance* self, double seconds);

static inline int64_t SpriteAnimationInstance_GetIsRunning(SpriteAnimationInstance* self)
{
    return self->_isRunning;
}

Error SpriteAnimationInstance_SetIsRunning(SpriteAnimationInstance* self, bool value);

static inline int64_t SpriteAnimationInstance_GetIsLooped(SpriteAnimationInstance* self)
{
    return self->_isLooped;
}

Error SpriteAnimationInstance_SetIsLooped(SpriteAnimationInstance* self, bool value);

static inline WREvent* SpriteAnimationInstance_GetStateReachEvent(SpriteAnimationInstance* self)
{
    return &self->_stateReachEvent;
}

/* Resets the animation properties to the backed source's default values. */
Error SpriteAnimationInstance_ResetProperties(SpriteAnimationInstance* self);

/* Resets the animation state to be the start of the animation. */
Error SpriteAnimation_ResetAnimation(SpriteAnimationInstance* self);