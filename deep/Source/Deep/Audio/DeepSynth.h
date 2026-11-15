// Built-in generated sounds: small pure functions that return mono 16-bit PCM. Nobody on the agent side can listen,
// so every buffer goes through one safety pass (Finish): NaN to zero, peak scaled to a fixed level well under full
// scale, and for one-shots a soft fade at both ends so nothing clicks. Loops get a crossfaded seam instead.

#pragma once

#include "CoreMinimal.h"

namespace DeepSynth
{
	constexpr int32 SampleRate = 48000;
	/** One-shots are scaled to this peak (fraction of full scale). */
	constexpr float OneShotPeak = 0.45f;
	/** Loops are scaled to this peak: they play under everything else. */
	constexpr float LoopPeak = 0.3f;
	/** The tests' hard limit. */
	constexpr float PeakLimit = 0.6f;

	struct FBuffer
	{
		TArray<int16> Samples;
		bool bLoop = false;

		float DurationS() const { return Samples.Num() / float(SampleRate); }
	};

	/** Names of every built-in sound (one-shots first, then loops). */
	DEEP_API TArray<FName> BuiltinNames();
	DEEP_API bool IsBuiltin(FName Name);
	/** True for the built-ins made to loop (crossfaded seam, no end fades). */
	DEEP_API bool IsLoopBuiltin(FName Name);
	/** Generates a built-in by name. Empty buffer for an unknown name. Deterministic (fixed noise seeds). */
	DEEP_API FBuffer Generate(FName Name);
	/** Expected length in samples of a built-in (for the tests). */
	DEEP_API int32 ExpectedSamples(FName Name);

	/** Safety pass: NaN to 0, scale to Peak, one-shot end fades (attack/release). Then to int16. */
	DEEP_API TArray<int16> Finish(TArray<float>& Raw, float Peak, bool bOneShot);
	/** Turns Raw (LoopSamples + crossfade samples long) into a seamless loop of LoopSamples. */
	DEEP_API void MakeSeamless(TArray<float>& Raw, int32 LoopSamples);
}
