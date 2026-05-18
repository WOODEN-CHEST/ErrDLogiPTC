#pragma once
#include "raylib/raylib.h"
#include "wr/WRMemory.h"
#include "wr/WRError.h"
#include <stddef.h>
#include <stdint.h>


/**
 * The audio engine of the game.
 * 
 * At the core of everything is the audio engine. It contains a master track and plays all the sounds.
 * Only 1 audio engine may exist at a time. Constructing a new audio engine while one already exists will
 * raise an error.
 * 
 * An audio track is a single track which contains both subtracks and sounds. There is no limit on the nesting
 * of audio tracks or the number of sounds a track can have.
 * 
 * Audio modifiers (effects) can be applies to audio tracks and sounds.
 * 
 * The audio engine, sound, track and modifier pointers and all stable and won't change after creation.
 * 
 * Some of the audio engine is thread safe, some of it is not:
 * The audio track functions and audio engine functions are thread safe and can be called any time.
 * They queue work to the audio thread when exact sample timing matters.
 * The sound instance functions are not generally thread safe. To modify sound instances, use audio commands:
 *     Schedule a command on the track timeline, then in that command's function modify the sound instance.
 *     Do not set audio instance properties or query timing-sensitive sound state while not on the audio thread.
 * For commands, if a command is scheduled on a track at a time which has already been passed, it just gets executed
 * at the next opportunity as if it just happened.
 * The command setters all copy the command objects when setting them, so the references do not need to be kept alive after
 * the setters.
 * Creating a sound instance creates the object immediately and calls the initializer in the same method call,
 * but attaching it to the track is deferred to the audio thread. The pointer remains stable after creation.
 * 
 * For the automated values, setting the duration to be instant instantly sets the current value in the same method call.
 * 
 * To execute actions (like change modifiers) on the audio track or just.
 * 
 * When sampling, samples are interpolated between the left and right sample at a point. The sample rate
 * in the sample provider determines the distance between the points (so no, the sample rate does not change
 * the playback speed, just sampling quality).
 * 
 * Sounds, after being added to the audio track, are automatically removed from it once they end and no
 * modifiers for them are leaving tails anymore.
 * AudioTrack_GetCurrentSecond returns an atomic snapshot of the track timeline at a sample boundary.
 * If queried while a buffer is currently being filled, it may observe either the boundary before or after the
 * sample currently being mixed, but it never reports a partial in-between sample time.
 * 
 * Panning additionally changes volume to adjust for perceived loudness.
 */


#define SOUND_SAMPLE_PAN_LEFT -1.0f
#define SOUND_SAMPLE_PAN_MIDDLE 0.0f
#define SOUND_SAMPLE_PAN_RIGHT 1.0f

#define AUTOMATED_VALUE_DURATION_INSTANT 0.0

#define MAX_AUDIO_CHANNELS 2

#define INLINE_AUDIO_COMMAND_USER_DATA_SIZE 128


typedef struct AudioFormatStruct
{
    float SampleRate;
    int32_t ChannelCount;
} AudioFormat;

typedef struct SoundAutomatedValueTimeStruct
{
    double _changeDurationLeftSeconds;
} SoundAutomatedValueTime;

typedef struct SoundAutomatedFloatStruct
{
    float _currentValue;
    float _targetValue;
    SoundAutomatedValueTime _time;
} SoundAutomatedFloat;

typedef struct SoundAutomatedDoubleStruct
{
    double _currentValue;
    double _targetValue;
    SoundAutomatedValueTime _time;
} SoundAutomatedDouble;

typedef struct GameSoundInstanceStruct GameSoundInstance;

typedef struct SoundModifySectionContextStruct
{
    GameSoundInstance* _sound;
    double _sampleTimeSeconds; // Current audio track second at which this modification is taking place.
    double _singleFrameDurationSeconds; // Duration of a single audio frame, in seconds.
    float* SampleBuffer;
    size_t _sampleCount; // Number of samples in the sample buffer.
    size_t _modifyCount; // Number of samples to modify in this operation.
    size_t _startIndex; // Start index in the sample buffer from which to modify.
    AudioFormat _targetFormat; // The format into which the audio is being modifier to (destination format).
} SoundModifySectionContext;

typedef struct GameSoundStruct
{
    float* _samples;
    size_t _sampleCount;
    AudioFormat _format;
} GameSound;

typedef struct AudioTrackStruct AudioTrack;

typedef struct AudioEngineStruct AudioEngine;

typedef struct ISoundModifierVTableStruct
{
    void* Self;

    /* The modify function returns whether the modifier has an active tail. */
    bool (*_modify)(void* self, SoundModifySectionContext* context); 
    void (*_resetState)(void* self); // Resets modifier related state.

} ISoundModifierVTable;

typedef struct ISoundModifierStruct
{
    ISoundModifierVTable _vtable;
} ISoundModifier;

typedef struct SampleProviderStruct
{
    SoundAutomatedDouble _sampleRate;
    SoundAutomatedFloat _volume;
    SoundAutomatedFloat _pan; // [-1 left, 0 middle, 1 right]
    GenericBuffer* _modifiers;
    AudioEngine* _ownerEngine;
    void* _owner;
    bool _ownerIsTrack;
} SampleProvider;

typedef Error (*AudioCommandFunction)(AudioTrack* track, void* userData);

typedef struct AudioCommandStruct
{
    unsigned char _userData[INLINE_AUDIO_COMMAND_USER_DATA_SIZE];
    AudioCommandFunction _function;
} AudioCommand;

typedef enum SoundInstanceStateEnum
{
    SoundInstanceState_Playing,
    SoundInstanceState_Paused, 
    SoundInstanceState_Ended,
} SoundInstanceState;

struct GameSoundInstanceStruct
{
    GameSound* _source;
    SoundInstanceState _state;
    double _sampleIndex;
    SoundAutomatedDouble _sampleSpeed;
    SampleProvider _sampleProperties;
    bool _isLooped;
    AudioCommand _loopCommand; // Ran when the audio loops (forwards or backwards).
    AudioCommand _endCommand ; // Ran when the audio ends (tail from modifiers not accounted for).
    AudioCommand _tailEndCommand; // Ran when the sound and all modifier tail have ended.
};

typedef struct ReverbSoundModifierStruct
{
    ISoundModifier _modifier;
    SoundAutomatedFloat DryVolume;
    SoundAutomatedFloat WetVolume;
    SoundAutomatedFloat Feedback;
    SoundAutomatedDouble DelaySeconds;
    SoundAutomatedFloat Damping;
    float* _delayBuffer;
    size_t _delayBufferFrameCount;
    size_t _delayBufferWriteFrameIndex;
    float _previousLowPassLeft;
    float _previousLowPassRight;
} ReverbSoundModifier;

typedef enum BiQuadPassTypeEnum
{
    BiQuadPassType_Low,
    BiQuadPassType_High,
} BiQuadPassTypeEnum;

typedef struct BiQuadPassSoundModifierStruct
{
    ISoundModifier _modifier;
    BiQuadPassTypeEnum PassType;
    SoundAutomatedFloat WetVolume;
    SoundAutomatedFloat CutoffFrequency;
    SoundAutomatedFloat Resonance;
    float _x1Left;
    float _x2Left;
    float _y1Left;
    float _y2Left;
    float _x1Right;
    float _x2Right;
    float _y1Right;
    float _y2Right;
} BiQuadPassSoundModifier;

typedef struct BitCrusherModifierStruct
{
    ISoundModifier _modifier;
    SoundAutomatedFloat WetVolume;
    SoundAutomatedDouble HoldSeconds;
    SoundAutomatedFloat BitDepth;
    double _holdSecondsLeft;
    float _heldLeft;
    float _heldRight;
} BitCrusherModifier;

typedef Error (*SoundInstanceInitializer)(GameSoundInstance* soundInstance, void* userData);


// Functions.

// Audio Engine.
Error AudioEngine_Construct1(AudioEngine** outEngine);

Error AudioEngine_Deconstruct(AudioEngine* self);

AudioFormat AudioEngine_GetAudioFormat(AudioEngine* self);

AudioTrack* AudioEngine_GetMasterTrack(AudioEngine* self);

/* The number of seconds the latest callback to the audio buffer fill operation took. */
double AudioEngine_GetBufferFillDurationSeconds(AudioEngine* self);

static inline double AudioEngine_GetSecondsPerFrame(AudioEngine* self)
{
    return 1.0 / (double)AudioEngine_GetAudioFormat(self).SampleRate;
}


// Audio Track.
SampleProvider* AudioTrack_GetProperties(AudioTrack* self);

Error AudioTrack_CreateSubTrack(AudioTrack* self, AudioTrack** outSubTrack);

Error AudioTrack_RemoveSubTrack(AudioTrack* self, AudioTrack* subTrackToRemove);

Error AudioTrack_GetSubTracks(AudioTrack* self, GenericBuffer* outTrackPointers);

size_t AudioTrack_GetSoundInstanceCount(AudioTrack* self);

/* The current time, in seconds, of this audio track as an atomic sample-boundary snapshot. */
double AudioTrack_GetCurrentSecond(AudioTrack* self);

/* Creates a new sound instance and instantly (in this method call) calls the initializer on it.
* Writes out the created instance. */
Error AudioTrack_CreateSoundInstance(AudioTrack* self,
    GameSound* sourceSound,
    SoundInstanceInitializer initializer,
    GameSoundInstance** outSoundInstance);

/* Removes a sound from the track. Does not error if the sound isn't present, instead a bool is returned indicating that. */
Error AudioTrack_RemoveSoundInstance(AudioTrack* self, GameSoundInstance* soundInstance, bool* wasRemoved);

/* The second in track timeline is the second at which this command it to be executed. */
Error AudioTrack_ScheduleCommand(AudioTrack* self, double secondInTrackTimeline, AudioCommand* command);



// Sound modifiers.
static inline bool ISoundModifier_Modify(ISoundModifier* self, SoundModifySectionContext* context)
{
    return self->_vtable._modify(self->_vtable.Self, context);
}

static inline void ISoundModifier_ResetState(ISoundModifier* self)
{
    self->_vtable._resetState(self->_vtable.Self);
}

Error ReverbSoundModifier_Construct1(ReverbSoundModifier* self);

void ReverbSoundModifier_Deconstruct(ReverbSoundModifier* self);

void ReverbSoundModifier_ResetState(ReverbSoundModifier* self);

static inline ISoundModifier* ReverbSoundModifier_GetModifier(ReverbSoundModifier* self)
{
    return &self->_modifier;
}

Error BiQuadPassSoundModifier_Construct1(BiQuadPassSoundModifier* self, BiQuadPassTypeEnum passType);

void BiQuadPassSoundModifier_Deconstruct(BiQuadPassSoundModifier* self);

void BiQuadPassSoundModifier_ResetState(BiQuadPassSoundModifier* self);

static inline ISoundModifier* BiQuadPassSoundModifier_GetModifier(BiQuadPassSoundModifier* self)
{
    return &self->_modifier;
}

Error BitCrusherModifier_Construct1(BitCrusherModifier* self);

void BitCrusherModifier_Deconstruct(BitCrusherModifier* self);

void BitCrusherModifier_ResetState(BitCrusherModifier* self);

static inline ISoundModifier* BitCrusherModifier_GetModifier(BitCrusherModifier* self)
{
    return &self->_modifier;
}


// Automated values.

/* Automated values prohibit infinity and NaN. */

Error AutomatedFloat_SetValue(SoundAutomatedFloat* value, float newTarget, double changeDurationSeconds);

Error AutomatedDouble_SetValue(SoundAutomatedDouble* value, double newTarget, double changeDurationSeconds);


// Sample provider.
static inline SoundAutomatedDouble* SampleProvider_GetSampleRate(SampleProvider* self)
{
    return &self->_sampleRate;
}

static inline SoundAutomatedFloat* SampleProvider_GetPan(SampleProvider* self)
{
    return &self->_pan;
}

static inline SoundAutomatedFloat* SampleProvider_GetVolume(SampleProvider* self)
{
    return &self->_volume;
}

/* Modifier must be kept alive through the entirety of the sample provider's lifetime. */
Error SampleProvider_AddModifier(SampleProvider* self, ISoundModifier* modifier, size_t index);

Error SampleProvider_RemoveModifier(SampleProvider* self, ISoundModifier* modifier);

Error SampleProvider_ClearModifiers(SampleProvider* self);


// Sounds.
/* Sound does not own the audio samples, only borrows them. */
Error GameSound_Construct1(GameSound* self, float* samples, size_t sampleCount, AudioFormat format);

Error GameSound_Deconstruct(GameSound* self);


// Sound instances.
Error GameSoundInstance_SetSampleIndex(GameSoundInstance* self, double sampleIndex);

static inline double GameSoundInstance_GetSampleIndex(GameSoundInstance* self)
{
    return self->_sampleIndex;
}

Error GameSoundInstance_SetSampleSecond(GameSoundInstance* self, double second);

static inline double GameSoundInstance_GetSampleSecond(GameSoundInstance* self)
{
    return self->_sampleIndex / (double)self->_source->_format.SampleRate;
}

static inline bool GameSoundInstance_GetIsLooped(GameSoundInstance* self)
{
    return self->_isLooped;
}

static inline Error GameSoundInstance_SetIsLooped(GameSoundInstance* self, bool value)
{
    self->_isLooped = value;
    return Error_CreateSuccess();
}

static inline SoundAutomatedDouble* GameSoundInstance_GetSampleSpeed(GameSoundInstance* self)
{
    return &self->_sampleSpeed;
}

static inline SampleProvider* GameSoundInstance_GetSampleProperties(GameSoundInstance* self)
{
    return &self->_sampleProperties;
}

static inline Error GameSoundInstance_SetEndCommand(GameSoundInstance* self, AudioCommand* command)
{
    self->_endCommand = *command;
    return Error_CreateSuccess();
}

static inline Error GameSoundInstance_SetTailEndCommand(GameSoundInstance* self, AudioCommand* command)
{
    self->_tailEndCommand = *command;
    return Error_CreateSuccess();
}

static inline Error GameSoundInstance_SetLoopCommand(GameSoundInstance* self, AudioCommand* command)
{
    self->_loopCommand = *command;
    return Error_CreateSuccess();
}

static inline SoundInstanceState GameSoundInstance_GetState(GameSoundInstance* self)
{
    return self->_state;
}

static inline Error GameSoundInstance_SetState(GameSoundInstance* self, SoundInstanceState state)
{
    self->_state = state;
    return Error_CreateSuccess();
}
