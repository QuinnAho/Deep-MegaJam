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
	/**
	 * A family of variations: "hull_creak" -> hull_creak_01, _02, .. (the built-ins that exist, in order). Empty when
	 * the name is a built-in itself or has none. A row naming the family plays one at random (UDeepAudioSubsystem).
	 */
	DEEP_API TArray<FName> Variants(FName Name);
	/** A built-in, or a family of them. */
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

	/** Count copies of a finished one-shot, SpacingS apart and scaled by Gain, mixed and clamped (for the wav export). */
	DEEP_API TArray<int16> Repeat(const TArray<int16>& One, int32 Count, float SpacingS, float Gain = 1.f);

	/** One return of an echo, as UDeepImmersionAudio plays it. */
	struct FEchoTap
	{
		float DelayS = 0.f;
		float Gain = 0.f;
		float LowPassHz = 0.f;
	};

	/** The ping and its echo's returns mixed as the game plays them (Deep.Echo.Export): Echo at each tap x EchoGain. */
	DEEP_API TArray<int16> RenderEcho(FName Ping, FName Echo, const TArray<FEchoTap>& Taps, float EchoGain);

	/** Writes 16-bit mono PCM at SampleRate as a RIFF wav (Deep.Audio.Export). */
	DEEP_API bool WriteWav(const TArray<int16>& Pcm, const FString& Path);
}
