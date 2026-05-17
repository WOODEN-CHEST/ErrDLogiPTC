#include <stdio.h>
#include "SoundEngine.h"
#include "wr/WRThread.h"


static int PrintErrorAndExit(Error* error)
{
    if (error->Message != NULL)
    {
        printf("%s", (const char*)error->Message);
    }

    Error_Deconstruct(error);
    return 1;
}

Error SoundInitializer(GameSoundInstance* sound, void* userData)
{
    (void)userData;

    SampleProvider* Provider = GameSoundInstance_GetSampleProperties(sound);
    GameSoundInstance_SetIsLooped(sound, true);

    AutomatedFloat_SetValue(SampleProvider_GetVolume(Provider),
        1.0f,
        AUTOMATED_VALUE_DURATION_INSTANT);

    AutomatedDouble_SetValue(GameSoundInstance_GetSampleSpeed(sound), 1.0, AUTOMATED_VALUE_DURATION_INSTANT);
    

    Error Result = Error_CreateSuccess();

    ReverbSoundModifier* ReverbModifier = Memory_Allocate(sizeof(*ReverbModifier));
    ReverbSoundModifier_Construct1(ReverbModifier);

    AutomatedFloat_SetValue(&ReverbModifier->WetVolume, 0.6f, AUTOMATED_VALUE_DURATION_INSTANT);
    AutomatedFloat_SetValue(&ReverbModifier->Feedback, 0.4f, AUTOMATED_VALUE_DURATION_INSTANT);
    AutomatedDouble_SetValue(&ReverbModifier->DelaySeconds, 0.2f, AUTOMATED_VALUE_DURATION_INSTANT);
    AutomatedFloat_SetValue(&ReverbModifier->Damping, 0.25f, AUTOMATED_VALUE_DURATION_INSTANT);

    Result = SampleProvider_AddModifier(Provider, ReverbSoundModifier_GetModifier(ReverbModifier), 0);

    return Result;
}


int main(void)
{
    Wave TargetWave = LoadWave("/home/wooden_chest/Desktop/ghdf/music.ogg");
    float* Samples = LoadWaveSamples(TargetWave);
    size_t SampleCount = TargetWave.frameCount * TargetWave.channels;
    UnloadWave(TargetWave);


    AudioEngine* Engine;
    Error Result = AudioEngine_Construct1(&Engine);
    if (Result.Code != ErrorCode_Success)
    {
        return PrintErrorAndExit(&Result);
    }

    GameSound TargetSound;
    Result = GameSound_Construct1(&TargetSound,
        Samples,
        SampleCount,
        (AudioFormat) { .ChannelCount = TargetWave.channels, .SampleRate = TargetWave.sampleRate });
    if (Result.Code != ErrorCode_Success)
    {
        return PrintErrorAndExit(&Result);
    }

    GameSoundInstance* SoundInstance;
    AudioTrack_CreateSoundInstance(AudioEngine_GetMasterTrack(Engine),
        &TargetSound,
        &SoundInitializer,
        &SoundInstance);

    while (true)
    {
        Thread_Sleep(1000);
    }


    return 0;
}
