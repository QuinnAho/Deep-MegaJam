#include "Audio/DeepProcWave.h"

void UDeepProcWave::Init(TSharedPtr<const TArray<int16>, ESPMode::ThreadSafe> InSamples, int32 InSampleRate, bool bInLoop)
{
	Pcm = MoveTemp(InSamples);
	bLoopPcm = bInLoop;
	Cursor = 0;
	SetSampleRate(uint32(InSampleRate));
	NumChannels = 1;
	const int32 Num = Pcm.IsValid() ? Pcm->Num() : 0;
	Duration = bInLoop ? INDEFINITELY_LOOPING_DURATION : Num / float(FMath::Max(InSampleRate, 1));
	bLooping = false;
}

int32 UDeepProcWave::OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples)
{
	const int32 Total = Pcm.IsValid() ? Pcm->Num() : 0;
	if (Total <= 0 || NumSamples <= 0 || (!bLoopPcm && Cursor >= Total))
	{
		return 0; // the base class writes silence
	}
	// Append after anything still buffered, so nothing already queued is dropped.
	const int32 Offset = OutAudio.Num();
	OutAudio.SetNumUninitialized(Offset + NumSamples * int32(sizeof(int16)));
	int16* Out = reinterpret_cast<int16*>(OutAudio.GetData() + Offset);
	const int16* In = Pcm->GetData();
	for (int32 I = 0; I < NumSamples; ++I)
	{
		if (Cursor >= Total)
		{
			if (bLoopPcm)
			{
				Cursor = 0;
			}
		}
		Out[I] = Cursor < Total ? In[Cursor++] : 0;
	}
	return OutAudio.Num() / int32(sizeof(int16));
}
