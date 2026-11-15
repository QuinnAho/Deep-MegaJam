// Plays one generated PCM buffer (DeepSynth) through the audio mixer. The buffer is immutable and shared; the
// audio render thread only advances a read cursor. A loop wraps forever; a one-shot plays silence after its end
// (UDeepAudioSubsystem stops it then).

#pragma once

#include "CoreMinimal.h"
#include "Sound/SoundWaveProcedural.h"
#include "DeepProcWave.generated.h"

UCLASS()
class DEEP_API UDeepProcWave : public USoundWaveProcedural
{
	GENERATED_BODY()

public:
	/** Call once, before the wave is played. */
	void Init(TSharedPtr<const TArray<int16>, ESPMode::ThreadSafe> InSamples, int32 InSampleRate, bool bInLoop);

	// Audio render thread.
	virtual int32 OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples) override;

private:
	TSharedPtr<const TArray<int16>, ESPMode::ThreadSafe> Pcm;
	int32 Cursor = 0;
	bool bLoopPcm = false;
};
