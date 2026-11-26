#include "Audio/DeepSynth.h"

#include "HAL/IConsoleManager.h"
#include "Math/RandomStream.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	using namespace DeepSynth;

	constexpr float TwoPi = 2.f * PI;
	constexpr float LoopCrossfadeS = 0.5f;

	int32 Samples(float Seconds) { return FMath::Max(1, FMath::RoundToInt(Seconds * SampleRate)); }
	float TimeOf(int32 Index) { return Index / float(SampleRate); }

	/** One-pole low-pass. */
	struct FLowPass
	{
		float A = 1.f;
		float Y = 0.f;
		explicit FLowPass(float CutoffHz) { SetCutoff(CutoffHz); }
		void SetCutoff(float CutoffHz) { A = 1.f - FMath::Exp(-TwoPi * CutoffHz / SampleRate); }
		float Step(float X) { Y += A * (X - Y); return Y; }
	};

	/** Half-cosine ramp 0..1 over Seconds from Start (0 before, 1 after). */
	float Ramp(float T, float Start, float Seconds)
	{
		const float U = FMath::Clamp((T - Start) / FMath::Max(Seconds, 1e-4f), 0.f, 1.f);
		return 0.5f - 0.5f * FMath::Cos(PI * U);
	}

	/** A partial with an exponential decay after a soft attack. */
	float Decay(float T, float Tau, float AttackS = 0.002f)
	{
		return T < 0.f ? 0.f : Ramp(T, 0.f, AttackS) * FMath::Exp(-T / Tau);
	}

	TArray<float> Make(float Seconds) { TArray<float> X; X.SetNumZeroed(Samples(Seconds)); return X; }

	// ---------------------------------------------------------------- one-shots

	TArray<float> Ping()
	{
		TArray<float> X = Make(1.2f);
		float Phase = 0.f;
		for (int32 I = 0; I < X.Num(); ++I)
		{
			const float T = TimeOf(I);
			const float F = 1000.f + 150.f * FMath::Exp(-T / 0.03f);
			Phase += TwoPi * F / SampleRate;
			X[I] = Decay(T, 0.28f, 0.004f) * FMath::Sin(Phase) + 0.15f * Decay(T, 0.08f, 0.004f) * FMath::Sin(2.f * Phase);
		}
		// A soft echo off the hull.
		const int32 Delay = Samples(0.27f);
		for (int32 I = X.Num() - 1; I >= Delay; --I)
		{
			X[I] += 0.3f * X[I - Delay];
		}
		return X;
	}

	TArray<float> Snap()
	{
		TArray<float> X = Make(0.5f);
		FRandomStream Rng(11);
		FLowPass Lp(1500.f);
		float Phase = 0.f;
		for (int32 I = 0; I < X.Num(); ++I)
		{
			const float T = TimeOf(I);
			const float Noise = Rng.FRandRange(-1.f, 1.f);
			const float High = Noise - Lp.Step(Noise);
			const float F = 60.f + 180.f * FMath::Exp(-T / 0.2f);
			Phase += TwoPi * F / SampleRate;
			X[I] = Decay(T, 0.015f, 0.001f) * High + 0.6f * Decay(T, 0.12f) * FMath::Sin(Phase);
		}
		return X;
	}

	TArray<float> Alarm()
	{
		TArray<float> X = Make(0.62f);
		const float Starts[] = { 0.f, 0.2f, 0.4f };
		const float BeepS = 0.14f;
		const float EdgeS = 0.012f;
		for (int32 I = 0; I < X.Num(); ++I)
		{
			const float T = TimeOf(I);
			float Env = 0.f;
			for (const float S : Starts)
			{
				if (T >= S && T <= S + BeepS)
				{
					Env = Ramp(T, S, EdgeS) * (1.f - Ramp(T, S + BeepS - EdgeS, EdgeS));
				}
			}
			const float W = TwoPi * 880.f * T;
			X[I] = Env * (FMath::Sin(W) + 0.25f * FMath::Sin(2.f * W) + 0.1f * FMath::Sin(3.f * W));
		}
		return X;
	}

	TArray<float> Clank()
	{
		TArray<float> X = Make(0.8f);
		struct FPartial { float Hz, Amp, Tau; };
		const FPartial Partials[] = { { 180.f, 1.f, 0.35f }, { 431.f, 0.7f, 0.22f }, { 797.f, 0.5f, 0.15f }, { 1213.f, 0.35f, 0.09f }, { 1789.f, 0.2f, 0.06f } };
		FRandomStream Rng(23);
		for (int32 I = 0; I < X.Num(); ++I)
		{
			const float T = TimeOf(I);
			float V = 0.8f * Decay(T, 0.08f) * FMath::Sin(TwoPi * 65.f * T);
			for (const FPartial& P : Partials)
			{
				V += P.Amp * Decay(T, P.Tau, 0.0015f) * FMath::Sin(TwoPi * P.Hz * T);
			}
			V += 0.3f * Decay(T, 0.005f, 0.001f) * Rng.FRandRange(-1.f, 1.f);
			X[I] = V;
		}
		return X;
	}

	TArray<float> Bubbles()
	{
		TArray<float> X = Make(1.f);
		FRandomStream Rng(7);
		for (int32 B = 0; B < 12; ++B)
		{
			const float T0 = Rng.FRandRange(0.f, 0.75f);
			const float F0 = Rng.FRandRange(500.f, 1400.f);
			const float Tau = Rng.FRandRange(0.02f, 0.05f);
			const float Amp = Rng.FRandRange(0.4f, 1.f);
			const int32 First = Samples(T0);
			const int32 Last = FMath::Min(X.Num(), First + Samples(6.f * Tau));
			float Phase = 0.f;
			for (int32 I = First; I < Last; ++I)
			{
				const float T = TimeOf(I) - T0;
				Phase += TwoPi * F0 * (1.f + 2.5f * T / (6.f * Tau)) / SampleRate;
				X[I] += Amp * Decay(T, Tau, 0.001f) * FMath::Sin(Phase);
			}
		}
		return X;
	}

	/** The airlock floods or drains: a pressure hiss (band-passed noise) that swells fast and dies away. */
	TArray<float> AirlockHiss()
	{
		TArray<float> X = Make(0.9f);
		FRandomStream Rng(23);
		FLowPass Top(4200.f);
		FLowPass Bottom(900.f);
		for (int32 I = 0; I < X.Num(); ++I)
		{
			const float T = TimeOf(I);
			const float Band = Top.Step(Rng.FRandRange(-1.f, 1.f));
			const float Hiss = Band - Bottom.Step(Band);
			X[I] = Ramp(T, 0.f, 0.06f) * FMath::Exp(-T / 0.3f) * Hiss;
		}
		return X;
	}

	TArray<float> Click()
	{
		TArray<float> X = Make(0.06f);
		for (int32 I = 0; I < X.Num(); ++I)
		{
			const float T = TimeOf(I);
			X[I] = Decay(T, 0.008f, 0.001f) * FMath::Sin(TwoPi * 1800.f * T) + 0.5f * Decay(T, 0.012f, 0.001f) * FMath::Sin(TwoPi * 900.f * T);
		}
		return X;
	}

	/**
	 * The helm sonar's ping (soft, Nov 17): a low round tone that swells in over 35 ms (no click), two voices a few
	 * cents apart that beat slowly, a faint octave that dies first, then dim returns off rock, each darker and later.
	 * Well under the Captain; nothing like the serpent's first ping (a bright 550 Hz chirp).
	 */
	TArray<float> SonarPing(bool bReturns = true, float Seconds = 3.4f)
	{
		constexpr float BaseHz = 392.f;
		TArray<float> Dry = Make(Seconds);
		float PhaseA = 0.f;
		float PhaseB = 0.f;
		for (int32 I = 0; I < Dry.Num(); ++I)
		{
			const float T = TimeOf(I);
			// A slight droop, like the transducer settling.
			const float F = BaseHz * (1.f - 0.012f * FMath::Min(T, 1.2f) / 1.2f);
			PhaseA += TwoPi * F / SampleRate;
			PhaseB += TwoPi * F * 1.0042f / SampleRate;
			const float Swell = Ramp(T, 0.f, 0.035f);
			Dry[I] = Swell * (FMath::Exp(-T / 0.62f) * (FMath::Sin(PhaseA) + 0.55f * FMath::Sin(PhaseB))
				+ 0.1f * FMath::Exp(-T / 0.18f) * FMath::Sin(2.f * PhaseA));
		}
		// Returns off rock: later, softer and duller each time (a one-pole low-pass per return).
		struct FTap { float DelayS; float Gain; float CutoffHz; };
		const FTap Taps[] = { { 0.38f, 0.30f, 1400.f }, { 0.81f, 0.18f, 800.f }, { 1.37f, 0.10f, 450.f } };
		TArray<float> X = Dry;
		// Immersion I1 (D-072): the dry ping leaves the returns to the walls the sonar really traced (sonar_echo).
		for (const FTap& Tap : Taps)
		{
			if (!bReturns)
			{
				break;
			}
			const int32 Delay = Samples(Tap.DelayS);
			FLowPass Lp(Tap.CutoffHz);
			for (int32 I = Delay; I < X.Num(); ++I)
			{
				X[I] += Tap.Gain * Lp.Step(Dry[I - Delay]);
			}
		}
		// A faint dark wash under the returns: the water itself answering.
		FRandomStream Rng(131);
		FLowPass Wash(260.f);
		for (int32 I = 0; I < X.Num(); ++I)
		{
			const float T = TimeOf(I);
			X[I] += (bReturns ? 0.05f : 0.03f) * Ramp(T, 0.25f, 0.4f) * FMath::Exp(-FMath::Max(0.f, T - 0.25f) / 0.9f) * Wash.Step(Rng.FRandRange(-1.f, 1.f));
		}
		return X;
	}

	/** The jam's helm ping (Deep.Sonar.Style 0, kept for comparison): a clean transmit tone, a long tail, two returns. */
	TArray<float> SonarPingJam()
	{
		TArray<float> X = Make(2.4f);
		float Phase = 0.f;
		for (int32 I = 0; I < X.Num(); ++I)
		{
			const float T = TimeOf(I);
			const float F = 1480.f - 40.f * FMath::Min(T, 1.f);
			Phase += TwoPi * F / SampleRate;
			// A held 60 ms transmit, then the ring of the transducer dying away.
			const float Body = Ramp(T, 0.f, 0.006f) * (T < 0.06f ? 1.f : FMath::Exp(-(T - 0.06f) / 0.45f));
			X[I] = Body * (FMath::Sin(Phase) + 0.08f * FMath::Sin(3.f * Phase));
		}
		// Returns off the walls: later and softer.
		const int32 First = Samples(0.42f);
		const int32 Second = Samples(0.95f);
		for (int32 I = X.Num() - 1; I >= Second; --I)
		{
			X[I] += 0.32f * X[I - First] + 0.16f * X[I - Second];
		}
		for (int32 I = Second - 1; I >= First; --I)
		{
			X[I] += 0.32f * X[I - First];
		}
		return X;
	}

	/** The sonar's proximity beep: short and dry so it can repeat fast. */
	TArray<float> SonarTick()
	{
		TArray<float> X = Make(0.11f);
		for (int32 I = 0; I < X.Num(); ++I)
		{
			const float T = TimeOf(I);
			const float Env = Ramp(T, 0.f, 0.004f) * (1.f - Ramp(T, 0.05f, 0.05f));
			X[I] = Env * (FMath::Sin(TwoPi * 1150.f * T) + 0.2f * FMath::Sin(TwoPi * 2300.f * T));
		}
		return X;
	}

	TArray<float> Harpoon()
	{
		TArray<float> X = Make(0.6f);
		FRandomStream Rng(31);
		FLowPass Lp(3000.f);
		float Phase = 0.f;
		for (int32 I = 0; I < X.Num(); ++I)
		{
			const float T = TimeOf(I);
			const float F = 45.f + 110.f * FMath::Exp(-T / 0.05f);
			Phase += TwoPi * F / SampleRate;
			const float Thunk = Decay(T, 0.1f) * FMath::Sin(Phase);
			Lp.SetCutoff(FMath::Lerp(3000.f, 400.f, FMath::Clamp(T / 0.4f, 0.f, 1.f)));
			const float Noise = Rng.FRandRange(-1.f, 1.f);
			const float Whoosh = T < 0.45f ? FMath::Sin(PI * T / 0.45f) * Lp.Step(Noise) * 3.f : 0.f;
			const float Clack = 0.5f * Decay(T, 0.004f, 0.0005f) * Noise;
			X[I] = Thunk + 0.6f * Whoosh + Clack;
		}
		return X;
	}

	TArray<float> Chime()
	{
		TArray<float> X = Make(1.4f);
		struct FNote { float Start, Hz; };
		const FNote Notes[] = { { 0.f, 784.f }, { 0.13f, 1175.f } };
		for (int32 I = 0; I < X.Num(); ++I)
		{
			const float T = TimeOf(I);
			float V = 0.f;
			for (const FNote& N : Notes)
			{
				const float U = T - N.Start;
				if (U >= 0.f)
				{
					const float W = TwoPi * N.Hz * U;
					V += Decay(U, 0.45f, 0.003f) * FMath::Sin(W) + 0.35f * Decay(U, 0.15f, 0.003f) * FMath::Sin(2.f * W)
						+ 0.15f * Decay(U, 0.1f, 0.003f) * FMath::Sin(3.01f * W);
				}
			}
			X[I] = V;
		}
		return X;
	}

	/** The radio chain on noise: band 450..3200 Hz (style guide 9) and a light crush (hold at 8 kHz, 6 bits). */
	struct FRadioBand
	{
		FLowPass Low{ 450.f };
		FLowPass High{ 3200.f };
		float Held = 0.f;
		int32 Hold = 0;

		float Step(float X)
		{
			const float Band = High.Step(X) - Low.Step(X);
			if (Hold-- <= 0)
			{
				Hold = SampleRate / 8000 - 1;
				Held = FMath::RoundToFloat(Band * 32.f) / 32.f;
			}
			return Held;
		}
	};

	/** The Captain keys his mic: a click, a burst of squelch, a short hiss. */
	TArray<float> SquelchOn()
	{
		TArray<float> X = Make(0.45f);
		FRandomStream Rng(71);
		FRadioBand Band;
		for (int32 I = 0; I < X.Num(); ++I)
		{
			const float T = TimeOf(I);
			const float Noise = Band.Step(Rng.FRandRange(-1.f, 1.f));
			const float Burst = T < 0.09f ? 1.f - Ramp(T, 0.06f, 0.03f) : 0.f;
			const float Hiss = 0.22f * (1.f - Ramp(T, 0.12f, 0.33f));
			const float Click = Decay(T, 0.006f, 0.0005f) * FMath::Sin(TwoPi * 1400.f * T);
			X[I] = 2.5f * (Burst + Hiss) * Noise + 0.6f * Click;
		}
		return X;
	}

	/** He lets go: a short burst and the click of the key. */
	TArray<float> SquelchOff()
	{
		TArray<float> X = Make(0.22f);
		FRandomStream Rng(73);
		FRadioBand Band;
		for (int32 I = 0; I < X.Num(); ++I)
		{
			const float T = TimeOf(I);
			const float Noise = Band.Step(Rng.FRandRange(-1.f, 1.f));
			const float Burst = Ramp(T, 0.f, 0.01f) * (1.f - Ramp(T, 0.07f, 0.05f));
			const float Click = Decay(T - 0.12f, 0.005f, 0.0005f) * FMath::Sin(TwoPi * 1100.f * (T - 0.12f));
			X[I] = 2.5f * Burst * Noise + 0.5f * Click;
		}
		return X;
	}

	// ---- Immersion I1 (D-072): the hull's voice and the sonar's echo ----

	/** A two-pole resonator (a ringing mode of a steel plate): rings at Hz for about 1 / (pi * BandwidthHz) seconds. */
	struct FSynthResonator
	{
		float B1 = 0.f;
		float B2 = 0.f;
		float Gain = 0.f;
		float Y1 = 0.f;
		float Y2 = 0.f;
		FSynthResonator(float Hz, float BandwidthHz)
		{
			const float R = FMath::Exp(-PI * FMath::Max(1.f, BandwidthHz) / SampleRate);
			B1 = 2.f * R * FMath::Cos(TwoPi * Hz / SampleRate);
			B2 = -R * R;
			Gain = 1.f - R;
		}
		float Step(float X)
		{
			const float Y = Gain * X + B1 * Y1 + B2 * Y2;
			Y2 = Y1;
			Y1 = Y;
			return Y;
		}
	};

	/**
	 * A creak of the pressure hull: stick-slip. The steel grips and lets go in a train of small slips whose rate glides
	 * (RateA to RateB Hz, a little uneven), each slip ringing a few inharmonic plate modes over Tone; a breath of friction
	 * noise under it; swells in and dies away. bTick ends it with the plate settling: one small bright tick.
	 */
	TArray<float> SynthHullCreak(int32 Seed, float Seconds, float RateA, float RateB, float Tone, bool bTick)
	{
		TArray<float> X = Make(Seconds);
		FRandomStream Rng(Seed);
		const float Ratios[] = { 1.f, 2.31f, 3.87f, 5.62f, 0.53f };
		const float Widths[] = { 14.f, 22.f, 35.f, 55.f, 9.f };
		const float Amps[] = { 1.f, 0.7f, 0.45f, 0.25f, 0.8f };
		TArray<FSynthResonator> Modes;
		for (int32 K = 0; K < 5; ++K)
		{
			Modes.Emplace(Tone * Ratios[K], Widths[K]);
		}
		FLowPass Top(3500.f);
		FLowPass Rub(1800.f);
		float Phase = 0.f;
		const float Wobble = Rng.FRandRange(0.f, TwoPi);
		for (int32 I = 0; I < X.Num(); ++I)
		{
			const float T = TimeOf(I);
			const float U = T / Seconds;
			const float Env = Ramp(T, 0.f, 0.1f * Seconds) * (1.f - Ramp(T, 0.55f * Seconds, 0.4f * Seconds));
			const float Rate = FMath::Lerp(RateA, RateB, U) * (1.f + 0.15f * FMath::Sin(TwoPi * 1.7f * T + Wobble));
			Phase += Rate / SampleRate;
			float Slip = 0.f;
			if (Phase >= 1.f)
			{
				Phase -= 1.f + Rng.FRandRange(-0.3f, 0.3f);
				Slip = Rng.FRandRange(0.4f, 1.f);
			}
			const float Noise = Rng.FRandRange(-1.f, 1.f);
			const float Excite = Env * (Slip * 40.f + 0.6f * Rub.Step(Noise));
			float V = 0.f;
			for (int32 K = 0; K < Modes.Num(); ++K)
			{
				V += Amps[K] * Modes[K].Step(Excite);
			}
			X[I] = Top.Step(V);
		}
		if (bTick)
		{
			const float At = 0.84f * Seconds;
			for (int32 I = Samples(At); I < X.Num(); ++I)
			{
				const float T = TimeOf(I) - At;
				X[I] += 0.12f * Decay(T, 0.04f, 0.001f) * (FMath::Sin(TwoPi * 910.f * T) + 0.6f * FMath::Sin(TwoPi * 1433.f * T));
			}
		}
		return X;
	}

	/**
	 * The hull groans as it passes a 100 m band: the whole shell taking load. A slow stick-slip (9 to 4.5 slips a second)
	 * rings low plate modes, a bent fundamental glides from 52 to 41 Hz with inharmonic partials, a rumble swells under
	 * it, and two small ticks settle it at the end.
	 */
	TArray<float> SynthHullGroan()
	{
		constexpr float Seconds = 5.5f;
		TArray<float> X = Make(Seconds);
		FRandomStream Rng(229);
		const float Hz[] = { 62.f, 118.f, 197.f, 305.f, 470.f };
		const float Widths[] = { 6.f, 8.f, 11.f, 15.f, 20.f };
		const float Amps[] = { 1.f, 0.8f, 0.55f, 0.35f, 0.2f };
		TArray<FSynthResonator> Modes;
		for (int32 K = 0; K < 5; ++K)
		{
			Modes.Emplace(Hz[K], Widths[K]);
		}
		FLowPass Rumble(120.f);
		FLowPass Top(2000.f);
		float Phase = 0.f;
		float Slips = 0.f;
		for (int32 I = 0; I < X.Num(); ++I)
		{
			const float T = TimeOf(I);
			const float U = T / Seconds;
			const float Env = Ramp(T, 0.f, 1.2f) * (1.f - Ramp(T, 3.2f, 2.0f));
			const float F = FMath::Lerp(52.f, 41.f, U);
			Phase += TwoPi * F / SampleRate;
			float Tone = 0.f;
			for (int32 K = 1; K <= 4; ++K)
			{
				Tone += FMath::Sin(Phase * FMath::Pow(float(K), 1.02f)) / float(K * K);
			}
			Slips += FMath::Lerp(9.f, 4.5f, U) / SampleRate;
			float Slip = 0.f;
			if (Slips >= 1.f)
			{
				Slips -= 1.f + Rng.FRandRange(-0.35f, 0.35f);
				Slip = Rng.FRandRange(0.5f, 1.f);
			}
			const float Noise = Rng.FRandRange(-1.f, 1.f);
			float Ring = 0.f;
			for (int32 K = 0; K < Modes.Num(); ++K)
			{
				Ring += Amps[K] * Modes[K].Step(Env * Slip * 60.f);
			}
			X[I] = Top.Step(0.6f * Env * Tone + 0.8f * Ring + 2.5f * Env * Rumble.Step(Noise));
		}
		for (const float At : { 4.3f, 4.75f })
		{
			const float Gain = At < 4.5f ? 0.1f : 0.06f;
			for (int32 I = Samples(At); I < X.Num(); ++I)
			{
				const float T = TimeOf(I) - At;
				X[I] += Gain * Decay(T, 0.05f, 0.001f) * (FMath::Sin(TwoPi * 1100.f * T) + 0.5f * FMath::Sin(TwoPi * 1730.f * T));
			}
		}
		return X;
	}

	/**
	 * The ping come back off rock (sonar_echo; the game delays, dims and darkens it by the distance): the ping's
	 * 392 Hz pair a hair lower, swelling slower and ringing shorter, smeared by three close returns a few ms apart (rough
	 * rock never answers in one piece), over a little dark water wash.
	 */
	TArray<float> SynthSonarEcho()
	{
		constexpr float Seconds = 2.f;
		TArray<float> Dry = Make(Seconds);
		float PhaseA = 0.f;
		float PhaseB = 0.f;
		for (int32 I = 0; I < Dry.Num(); ++I)
		{
			const float T = TimeOf(I);
			const float F = 392.f * (0.995f - 0.008f * FMath::Min(T, 1.f));
			PhaseA += TwoPi * F / SampleRate;
			PhaseB += TwoPi * F * 1.0042f / SampleRate;
			Dry[I] = Ramp(T, 0.f, 0.07f) * FMath::Exp(-T / 0.45f) * (FMath::Sin(PhaseA) + 0.5f * FMath::Sin(PhaseB));
		}
		TArray<float> X = Dry;
		struct FSynthSmear { float DelayS; float Gain; };
		for (const FSynthSmear& S : { FSynthSmear{ 0.011f, 0.6f }, FSynthSmear{ 0.023f, 0.45f }, FSynthSmear{ 0.037f, 0.3f } })
		{
			const int32 Delay = Samples(S.DelayS);
			for (int32 I = Delay; I < X.Num(); ++I)
			{
				X[I] += S.Gain * Dry[I - Delay];
			}
		}
		FRandomStream Rng(239);
		FLowPass Wash(300.f);
		for (int32 I = 0; I < X.Num(); ++I)
		{
			const float T = TimeOf(I);
			X[I] += 0.12f * Ramp(T, 0.05f, 0.2f) * FMath::Exp(-T / 0.5f) * Wash.Step(Rng.FRandRange(-1.f, 1.f));
		}
		return X;
	}

	// ---------------------------------------------------------------- loops (raw = loop + crossfade)

	TArray<float> MakeLoop(float LoopS) { return Make(LoopS + LoopCrossfadeS); }

	TArray<float> DroneOut()
	{
		TArray<float> X = MakeLoop(8.f);
		FRandomStream Rng(41);
		FLowPass Lp(180.f);
		for (int32 I = 0; I < X.Num(); ++I)
		{
			const float T = TimeOf(I);
			const float Swell = 0.75f + 0.25f * FMath::Sin(TwoPi * 0.12f * T);
			const float Tone = FMath::Sin(TwoPi * 55.f * T) + 0.8f * FMath::Sin(TwoPi * 55.35f * T) + 0.35f * FMath::Sin(TwoPi * 82.4f * T)
				+ 0.15f * (0.5f + 0.5f * FMath::Sin(TwoPi * 0.07f * T)) * FMath::Sin(TwoPi * 110.f * T);
			X[I] = Swell * (Tone + 4.f * Lp.Step(Rng.FRandRange(-1.f, 1.f)));
		}
		return X;
	}

	TArray<float> DroneIn()
	{
		TArray<float> X = MakeLoop(6.f);
		FRandomStream Rng(43);
		FLowPass Lp(900.f);
		FLowPass LpLow(200.f);
		for (int32 I = 0; I < X.Num(); ++I)
		{
			const float T = TimeOf(I);
			const float Hum = (0.8f + 0.2f * FMath::Sin(TwoPi * 0.25f * T))
				* (FMath::Sin(TwoPi * 60.f * T) + 0.5f * FMath::Sin(TwoPi * 120.f * T) + 0.2f * FMath::Sin(TwoPi * 180.f * T) + 0.08f * FMath::Sin(TwoPi * 240.f * T));
			const float Noise = Rng.FRandRange(-1.f, 1.f);
			const float Vent = Lp.Step(Noise) - LpLow.Step(Noise);
			X[I] = 0.6f * Hum + 2.f * Vent;
		}
		return X;
	}

	TArray<float> Breath(float PeriodS, float LoopS, int32 Seed)
	{
		TArray<float> X = MakeLoop(LoopS);
		FRandomStream Rng(Seed);
		FLowPass HissHigh(1200.f);
		FLowPass HissLow(300.f);
		FLowPass Bubble(400.f);
		for (int32 I = 0; I < X.Num(); ++I)
		{
			const float T = TimeOf(I);
			const float Ph = FMath::Fmod(T, PeriodS) / PeriodS;
			const float Noise = Rng.FRandRange(-1.f, 1.f);
			const float Hiss = HissHigh.Step(Noise) - HissLow.Step(Noise);
			const float Gurgle = Bubble.Step(Noise) * (0.6f + 0.4f * FMath::Sin(TwoPi * 11.f * T));
			float V = 0.f;
			if (Ph < 0.35f)
			{
				V = FMath::Square(FMath::Sin(PI * Ph / 0.35f)) * 2.5f * Hiss;
			}
			else if (Ph >= 0.45f && Ph < 0.9f)
			{
				V = FMath::Square(FMath::Sin(PI * (Ph - 0.45f) / 0.45f)) * 5.f * Gurgle;
			}
			X[I] = V;
		}
		return X;
	}

	/** Scared (Deep.Music): a panicked heart at 120 bpm, lub-dub, each beat a falling low thump with a little body noise. */
	TArray<float> Heartbeat()
	{
		const float PeriodS = 0.5f;
		TArray<float> X = MakeLoop(3.f);
		FRandomStream Rng(73);
		FLowPass Body(140.f);
		auto Thump = [](float Dt, float Amp) -> float
		{
			if (Dt < 0.f || Dt > 0.2f)
			{
				return 0.f;
			}
			const float F = 58.f - 18.f * Dt / 0.2f;
			return Amp * FMath::Sin(TwoPi * F * Dt) * FMath::Exp(-Dt / 0.05f);
		};
		auto Env = [](float Dt) { return Dt < 0.f || Dt > 0.2f ? 0.f : FMath::Exp(-Dt / 0.04f); };
		for (int32 I = 0; I < X.Num(); ++I)
		{
			const float Ph = FMath::Fmod(TimeOf(I), PeriodS);
			const float Noise = Body.Step(Rng.FRandRange(-1.f, 1.f));
			X[I] = Thump(Ph, 1.f) + Thump(Ph - 0.16f, 0.7f) + 3.f * Noise * (Env(Ph) + 0.7f * Env(Ph - 0.16f));
		}
		return X;
	}

	TArray<float> Winch()
	{
		TArray<float> X = MakeLoop(2.f);
		FRandomStream Rng(47);
		FLowPass Rumble(150.f);
		FLowPass TickLp(2000.f);
		const float TickEveryS = 0.125f;
		for (int32 I = 0; I < X.Num(); ++I)
		{
			const float T = TimeOf(I);
			float Motor = 0.f;
			for (int32 K = 1; K <= 6; ++K)
			{
				Motor += FMath::Sin(TwoPi * 95.f * K * T) / K;
			}
			const float Noise = Rng.FRandRange(-1.f, 1.f);
			const float SinceTick = FMath::Fmod(T, TickEveryS);
			const float Tick = Decay(SinceTick, 0.004f, 0.0005f) * (Noise - TickLp.Step(Noise));
			X[I] = 0.5f * Motor + 0.08f * FMath::Sin(TwoPi * 760.f * T) + 0.5f * Tick + 2.f * Rumble.Step(Noise);
		}
		return X;
	}

	TArray<float> Serpent()
	{
		TArray<float> X = MakeLoop(8.f);
		FRandomStream Rng(53);
		FLowPass Lp(250.f);
		float Phase = 0.f;
		float MoanPhase = 0.f;
		for (int32 I = 0; I < X.Num(); ++I)
		{
			const float T = TimeOf(I);
			const float F = 42.f + 4.f * FMath::Sin(TwoPi * 0.21f * T);
			Phase += TwoPi * F / SampleRate;
			const float Rough = 0.6f + 0.4f * FMath::Sin(TwoPi * 23.f * T);
			const float Growl = Rough * (FMath::Sin(Phase) + 0.5f * FMath::Sin(2.f * Phase) + 0.3f * FMath::Sin(3.f * Phase));
			const float Swell = 0.55f + 0.45f * FMath::Sin(TwoPi * 0.25f * T);
			const float Loop = FMath::Fmod(T, 8.f);
			float Moan = 0.f;
			if (Loop >= 2.f && Loop < 5.f)
			{
				const float U = (Loop - 2.f) / 3.f;
				MoanPhase += TwoPi * FMath::Lerp(90.f, 65.f, U) / SampleRate;
				Moan = 0.5f * FMath::Square(FMath::Sin(PI * U)) * FMath::Sin(MoanPhase);
			}
			X[I] = Swell * (Growl + 3.f * Lp.Step(Rng.FRandRange(-1.f, 1.f))) + Moan;
		}
		return X;
	}

	TArray<float> Hum()
	{
		TArray<float> X = MakeLoop(4.f);
		FRandomStream Rng(59);
		FLowPass Lp(300.f);
		for (int32 I = 0; I < X.Num(); ++I)
		{
			const float T = TimeOf(I);
			const float Chop = 0.7f + 0.3f * FMath::Sin(TwoPi * 7.f * T);
			const float Engine = FMath::Sin(TwoPi * 48.f * T) + 0.5f * FMath::Sin(TwoPi * 96.f * T) + 0.25f * FMath::Sin(TwoPi * 144.f * T);
			X[I] = Chop * Engine + 2.5f * Lp.Step(Rng.FRandRange(-1.f, 1.f)) + 0.05f * FMath::Sin(TwoPi * 380.f * T);
		}
		return X;
	}

	// ---- The serpent's own voice (lane G): spatialised at its head by ADeepSerpent ----

	/** A call: a low growl that swells, glides down into a moan and dies. Three seeds and contours = three variations. */
	TArray<float> SerpentCall(float StartHz, float EndHz, float Seconds, int32 Seed)
	{
		TArray<float> X = Make(Seconds);
		FRandomStream Rng(Seed);
		FLowPass Breath(600.f);
		FLowPass Body(180.f);
		float Phase = 0.f;
		for (int32 I = 0; I < X.Num(); ++I)
		{
			const float T = TimeOf(I);
			const float U = T / Seconds;
			const float Hz = FMath::Lerp(StartHz, EndHz, U * U) * (1.f + 0.04f * FMath::Sin(TwoPi * 5.5f * T));
			Phase += TwoPi * Hz / SampleRate;
			const float Rough = 0.55f + 0.45f * FMath::Sin(TwoPi * 31.f * T);
			const float Voice = Rough * (FMath::Sin(Phase) + 0.6f * FMath::Sin(2.f * Phase) + 0.35f * FMath::Sin(3.f * Phase) + 0.2f * FMath::Sin(5.f * Phase));
			const float Env = FMath::Square(FMath::Sin(PI * FMath::Min(1.f, U * 1.15f))) * FMath::Min(1.f, T / 0.25f);
			const float Noise = Rng.FRandRange(-1.f, 1.f);
			X[I] = Env * (Voice + 2.f * Breath.Step(Noise) + 3.f * Body.Step(Noise));
		}
		return X;
	}

	/** A click train: a burst of sharp, slightly ringing clicks that speed up (echolocation). */
	TArray<float> SerpentClick()
	{
		TArray<float> X = Make(0.35f);
		FRandomStream Rng(83);
		FLowPass Lp(3200.f);
		const float Times[] = { 0.f, 0.07f, 0.125f, 0.17f, 0.205f, 0.235f, 0.26f };
		for (int32 I = 0; I < X.Num(); ++I)
		{
			const float T = TimeOf(I);
			const float Noise = Rng.FRandRange(-1.f, 1.f);
			const float Bright = Noise - Lp.Step(Noise);
			float Y = 0.f;
			for (int32 K = 0; K < UE_ARRAY_COUNT(Times); ++K)
			{
				const float Since = T - Times[K];
				if (Since >= 0.f && Since < 0.03f)
				{
					const float Amp = 1.f - 0.08f * K;
					Y += Amp * FMath::Exp(-Since / 0.0025f) * (0.6f * Bright + FMath::Sin(TwoPi * 1900.f * Since));
				}
			}
			X[I] = Y;
		}
		return X;
	}

	/** The presence drone: two low sines beating against each other under dark filtered noise. */
	TArray<float> SerpentDrone()
	{
		TArray<float> X = MakeLoop(6.f);
		FRandomStream Rng(89);
		FLowPass Lp(140.f);
		for (int32 I = 0; I < X.Num(); ++I)
		{
			const float T = TimeOf(I);
			// Whole cycles in 6 s on both, so the loop seam crossfade lines up.
			const float Beat = FMath::Sin(TwoPi * 36.f * T) + FMath::Sin(TwoPi * (36.f + 1.f / 3.f) * T);
			const float Swell = 0.7f + 0.3f * FMath::Sin(TwoPi * T / 6.f);
			X[I] = Swell * (0.8f * Beat + 0.3f * FMath::Sin(TwoPi * 72.5f * T) + 4.f * Lp.Step(Rng.FRandRange(-1.f, 1.f)));
		}
		return X;
	}

	/**
	 * Immersion I1: water rushing along the hull (loop_flow; the game raises it, its pitch and its low-pass with speed).
	 * A dark band of noise (80 to 900 Hz) whose level breathes with slow turbulence, and a faint gurgle in the low end.
	 */
	TArray<float> SynthFlow()
	{
		constexpr float LoopS = 6.f;
		TArray<float> X = MakeLoop(LoopS);
		FRandomStream Rng(233);
		FLowPass Top(900.f);
		FLowPass Bottom(80.f);
		FLowPass Gurgle(350.f);
		FLowPass GurgleLow(120.f);
		for (int32 I = 0; I < X.Num(); ++I)
		{
			const float T = TimeOf(I);
			const float Noise = Rng.FRandRange(-1.f, 1.f);
			float Band = Top.Step(Noise);
			Band -= Bottom.Step(Band);
			// Whole cycles in the loop's 6 s, so the seam's crossfade lines up.
			const float Turb = 0.75f + 0.12f * FMath::Sin(TwoPi * T / LoopS) + 0.08f * FMath::Sin(TwoPi * 3.f * T / LoopS + 1.f)
				+ 0.05f * FMath::Sin(TwoPi * 7.f * T / LoopS + 2.f);
			float Low = Gurgle.Step(Noise);
			Low -= GurgleLow.Step(Low);
			const float Bubble = 0.5f + 0.5f * FMath::Sin(TwoPi * 13.f * T / LoopS + 0.5f * FMath::Sin(TwoPi * 2.f * T / LoopS));
			X[I] = 3.f * Turb * Band + 2.f * Bubble * Low;
		}
		return X;
	}

	/**
	 * D2: the view's servo, a small geared motor behind the cabin's panel, heard while the view swings. A thin whine
	 * (two partials an octave apart and a faint third), its gear mesh as a slow flutter, and a little brush hiss in the
	 * middle band. Whole cycles in the 2 s loop. Quiet by its row (Sounds.csv loop_servo); its pitch follows the speed.
	 */
	TArray<float> ServoWhine()
	{
		constexpr float LoopS = 2.f;
		TArray<float> X = MakeLoop(LoopS);
		FRandomStream Rng(131);
		FLowPass Top(2600.f);
		FLowPass Bottom(700.f);
		for (int32 I = 0; I < X.Num(); ++I)
		{
			const float T = TimeOf(I);
			const float Whine = FMath::Sin(TwoPi * 340.f * T) + 0.45f * FMath::Sin(TwoPi * 680.5f * T) + 0.18f * FMath::Sin(TwoPi * 1021.f * T);
			const float Gear = 1.f - 0.3f * (0.5f + 0.5f * FMath::Sin(TwoPi * 41.f * T));
			float Brush = Top.Step(Rng.FRandRange(-1.f, 1.f));
			Brush -= Bottom.Step(Brush);
			X[I] = 0.6f * Gear * Whine + 1.2f * Brush;
		}
		return X;
	}

	struct FBuiltin
	{
		const TCHAR* Name;
		bool bLoop;
		float Seconds; // one-shot length, or loop length
		TFunction<TArray<float>()> Make;
	};

	const TArray<FBuiltin>& Table()
	{
		static const TArray<FBuiltin> Builtins = {
			{ TEXT("ping"), false, 1.2f, &Ping },
			{ TEXT("snap"), false, 0.5f, &Snap },
			{ TEXT("alarm"), false, 0.62f, &Alarm },
			{ TEXT("clank"), false, 0.8f, &Clank },
			{ TEXT("bubbles"), false, 1.f, &Bubbles },
			{ TEXT("airlock_hiss"), false, 0.9f, &AirlockHiss },
			{ TEXT("click"), false, 0.06f, &Click },
			{ TEXT("harpoon"), false, 0.6f, &Harpoon },
			{ TEXT("chime"), false, 1.4f, &Chime },
			{ TEXT("squelch_on"), false, 0.45f, &SquelchOn },
			{ TEXT("squelch_off"), false, 0.22f, &SquelchOff },
			{ TEXT("sonar_ping"), false, 3.4f, [] { return SonarPing(true, 3.4f); } },
			{ TEXT("sonar_ping_dry"), false, 2.6f, [] { return SonarPing(false, 2.6f); } },
			{ TEXT("sonar_echo"), false, 2.f, &SynthSonarEcho },
			{ TEXT("sonar_ping_jam"), false, 2.4f, &SonarPingJam },
			{ TEXT("sonar_tick"), false, 0.11f, &SonarTick },
			{ TEXT("serpent_call_a"), false, 2.2f, [] { return SerpentCall(70.f, 44.f, 2.2f, 97); } },
			{ TEXT("serpent_call_b"), false, 2.6f, [] { return SerpentCall(55.f, 82.f, 2.6f, 101); } },
			{ TEXT("serpent_call_c"), false, 1.8f, [] { return SerpentCall(95.f, 38.f, 1.8f, 103); } },
			{ TEXT("serpent_click"), false, 0.35f, &SerpentClick },
			{ TEXT("hull_creak_01"), false, 1.3f, [] { return SynthHullCreak(211, 1.3f, 38.f, 22.f, 170.f, false); } },
			{ TEXT("hull_creak_02"), false, 0.9f, [] { return SynthHullCreak(223, 0.9f, 70.f, 110.f, 240.f, false); } },
			{ TEXT("hull_creak_03"), false, 1.6f, [] { return SynthHullCreak(227, 1.6f, 30.f, 18.f, 130.f, true); } },
			{ TEXT("hull_groan"), false, 5.5f, &SynthHullGroan },
			{ TEXT("drone_out"), true, 8.f, &DroneOut },
			{ TEXT("drone_in"), true, 6.f, &DroneIn },
			{ TEXT("breath"), true, 8.f, [] { return Breath(4.f, 8.f, 61); } },
			{ TEXT("breath_fast"), true, 4.f, [] { return Breath(2.f, 4.f, 67); } },
			{ TEXT("winch"), true, 2.f, &Winch },
			{ TEXT("serpent"), true, 8.f, &Serpent },
			{ TEXT("hum"), true, 4.f, &Hum },
			{ TEXT("breath_panic"), true, 4.8f, [] { return Breath(1.2f, 4.8f, 71); } },
			{ TEXT("heartbeat"), true, 3.f, &Heartbeat },
			{ TEXT("serpent_drone"), true, 6.f, &SerpentDrone },
			{ TEXT("flow"), true, 6.f, &SynthFlow },
			{ TEXT("servo_whine"), true, 2.f, &ServoWhine },
		};
		return Builtins;
	}

	const FBuiltin* Find(FName Name)
	{
		for (const FBuiltin& B : Table())
		{
			if (Name == FName(B.Name))
			{
				return &B;
			}
		}
		return nullptr;
	}
}

namespace DeepSynth
{
	TArray<FName> BuiltinNames()
	{
		TArray<FName> Out;
		for (const FBuiltin& B : Table())
		{
			Out.Add(FName(B.Name));
		}
		return Out;
	}

	TArray<FName> Variants(FName Name)
	{
		TArray<FName> Out;
		if (Name.IsNone() || Find(Name))
		{
			return Out;
		}
		const FString Base = Name.ToString();
		for (int32 Index = 1; Index <= 9; ++Index)
		{
			const FName Variant(*FString::Printf(TEXT("%s_%02d"), *Base, Index));
			if (!Find(Variant))
			{
				break;
			}
			Out.Add(Variant);
		}
		return Out;
	}

	bool IsBuiltin(FName Name) { return Find(Name) != nullptr || !Variants(Name).IsEmpty(); }

	bool IsLoopBuiltin(FName Name)
	{
		const FBuiltin* B = Find(Name);
		return B && B->bLoop;
	}

	int32 ExpectedSamples(FName Name)
	{
		const FBuiltin* B = Find(Name);
		if (!B)
		{
			const TArray<FName> Family = Variants(Name);
			B = Family.IsEmpty() ? nullptr : Find(Family[0]);
		}
		return B ? Samples(B->Seconds) : 0;
	}

	FBuffer Generate(FName Name)
	{
		FBuffer Out;
		const FBuiltin* B = Find(Name);
		if (!B)
		{
			// A family name ("hull_creak") stands for its first variation.
			const TArray<FName> Family = Variants(Name);
			B = Family.IsEmpty() ? nullptr : Find(Family[0]);
		}
		if (!B)
		{
			return Out;
		}
		TArray<float> Raw = B->Make();
		Out.bLoop = B->bLoop;
		if (B->bLoop)
		{
			MakeSeamless(Raw, Samples(B->Seconds));
		}
		Out.Samples = Finish(Raw, B->bLoop ? LoopPeak : OneShotPeak, !B->bLoop);
		return Out;
	}

	void MakeSeamless(TArray<float>& Raw, int32 LoopSamples)
	{
		const int32 Fade = Raw.Num() - LoopSamples;
		if (LoopSamples <= 0 || Fade <= 0 || Fade > LoopSamples)
		{
			return;
		}
		// Equal-power crossfade of the head with the material just after the loop end: the last loop sample
		// is followed (on wrap) by what originally came right after it.
		for (int32 I = 0; I < Fade; ++I)
		{
			const float U = (I + 0.5f) / Fade;
			Raw[I] = Raw[I] * FMath::Sin(0.5f * PI * U) + Raw[LoopSamples + I] * FMath::Cos(0.5f * PI * U);
		}
		Raw.SetNum(LoopSamples);
	}

	TArray<int16> Finish(TArray<float>& Raw, float Peak, bool bOneShot)
	{
		float Max = 0.f;
		for (float& V : Raw)
		{
			if (!FMath::IsFinite(V))
			{
				V = 0.f;
			}
			Max = FMath::Max(Max, FMath::Abs(V));
		}
		const float Scale = Max > 1e-6f ? FMath::Min(Peak, PeakLimit) / Max : 0.f;
		if (bOneShot && Raw.Num() > 0)
		{
			const int32 Attack = FMath::Min(Samples(0.003f), Raw.Num() / 4);
			const int32 Release = FMath::Min(Samples(0.015f), Raw.Num() / 4);
			for (int32 I = 0; I < Attack; ++I)
			{
				Raw[I] *= 0.5f - 0.5f * FMath::Cos(PI * I / Attack);
			}
			for (int32 I = 0; I < Release; ++I)
			{
				Raw[Raw.Num() - 1 - I] *= 0.5f - 0.5f * FMath::Cos(PI * I / Release);
			}
		}
		TArray<int16> Out;
		Out.SetNumUninitialized(Raw.Num());
		for (int32 I = 0; I < Raw.Num(); ++I)
		{
			Out[I] = int16(FMath::Clamp(FMath::RoundToInt(Raw[I] * Scale * 32767.f), -32767, 32767));
		}
		return Out;
	}
}

namespace DeepSynth
{
	TArray<int16> Repeat(const TArray<int16>& One, int32 Count, float SpacingS, float Gain)
	{
		TArray<int16> Out;
		if (One.Num() == 0 || Count <= 0)
		{
			return Out;
		}
		const int32 Step = Samples(FMath::Max(0.f, SpacingS));
		TArray<float> Mix;
		Mix.SetNumZeroed(Step * (Count - 1) + One.Num());
		for (int32 C = 0; C < Count; ++C)
		{
			for (int32 I = 0; I < One.Num(); ++I)
			{
				Mix[C * Step + I] += One[I] * Gain;
			}
		}
		Out.SetNumUninitialized(Mix.Num());
		for (int32 I = 0; I < Mix.Num(); ++I)
		{
			Out[I] = int16(FMath::Clamp(FMath::RoundToInt(Mix[I]), -32767, 32767));
		}
		return Out;
	}

	TArray<int16> RenderEcho(FName Ping, FName Echo, const TArray<FEchoTap>& Taps, float EchoGain)
	{
		const FBuffer Dry = Generate(Ping);
		const FBuffer Wet = Generate(Echo);
		int32 Length = Dry.Samples.Num();
		for (const FEchoTap& Tap : Taps)
		{
			Length = FMath::Max(Length, Samples(FMath::Max(0.f, Tap.DelayS)) + Wet.Samples.Num());
		}
		TArray<float> Mix;
		Mix.SetNumZeroed(Length);
		for (int32 I = 0; I < Dry.Samples.Num(); ++I)
		{
			Mix[I] += Dry.Samples[I];
		}
		for (const FEchoTap& Tap : Taps)
		{
			// As the game plays it: the echo row at the tap's gain, through a low-pass at the tap's cutoff.
			const int32 Delay = Samples(FMath::Max(0.f, Tap.DelayS));
			FLowPass Lp(FMath::Max(50.f, Tap.LowPassHz));
			for (int32 I = 0; I < Wet.Samples.Num(); ++I)
			{
				Mix[Delay + I] += EchoGain * Tap.Gain * Lp.Step(float(Wet.Samples[I]));
			}
		}
		TArray<int16> Out;
		Out.SetNumUninitialized(Mix.Num());
		for (int32 I = 0; I < Mix.Num(); ++I)
		{
			Out[I] = int16(FMath::Clamp(FMath::RoundToInt(Mix[I]), -32767, 32767));
		}
		return Out;
	}

	bool WriteWav(const TArray<int16>& Pcm, const FString& Path)
	{
		// A plain 16-bit mono PCM RIFF file.
		TArray<uint8> Bytes;
		auto U32 = [&Bytes](uint32 V) { for (int32 B = 0; B < 4; ++B) { Bytes.Add(uint8((V >> (8 * B)) & 0xFF)); } };
		auto U16 = [&Bytes](uint16 V) { Bytes.Add(uint8(V & 0xFF)); Bytes.Add(uint8(V >> 8)); };
		auto Tag = [&Bytes](const char* T) { for (int32 B = 0; B < 4; ++B) { Bytes.Add(uint8(T[B])); } };
		const uint32 DataBytes = uint32(Pcm.Num()) * 2u;
		Tag("RIFF"); U32(36u + DataBytes); Tag("WAVE");
		Tag("fmt "); U32(16u); U16(1u); U16(1u); U32(uint32(SampleRate)); U32(uint32(SampleRate) * 2u); U16(2u); U16(16u);
		Tag("data"); U32(DataBytes);
		for (const int16 S : Pcm)
		{
			U16(uint16(S));
		}
		return FFileHelper::SaveArrayToFile(Bytes, *Path);
	}
}

namespace
{
	FAutoConsoleCommand ExportCommand(
		TEXT("Deep.Audio.Export"),
		TEXT("Deep.Audio.Export <builtin> <path.wav> [count=1] [spacingS=0] [gain=1]: writes a built-in sound (repeated count times, spacingS apart, scaled by gain) to a wav file."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			if (Args.Num() < 2)
			{
				UE_LOG(LogTemp, Warning, TEXT("Deep.Audio.Export <builtin> <path.wav> [count] [spacingS] [gain]"));
				return;
			}
			const FName Name(*Args[0]);
			const DeepSynth::FBuffer Buffer = DeepSynth::Generate(Name);
			if (Buffer.Samples.Num() == 0)
			{
				UE_LOG(LogTemp, Warning, TEXT("Deep.Audio.Export: no built-in %s"), *Args[0]);
				return;
			}
			const int32 Count = Args.Num() > 2 ? FMath::Max(1, FCString::Atoi(*Args[2])) : 1;
			const float SpacingS = Args.Num() > 3 ? FCString::Atof(*Args[3]) : 0.f;
			const float Gain = Args.Num() > 4 ? FCString::Atof(*Args[4]) : 1.f;
			const FString Path = FPaths::ConvertRelativePathToFull(Args[1]);
			const bool bOk = DeepSynth::WriteWav(DeepSynth::Repeat(Buffer.Samples, Count, SpacingS, Gain), Path);
			UE_LOG(LogTemp, Display, TEXT("DEEPAUDIO export %s x%d every %.2fs gain %.2f -> %s ok=%d"), *Args[0], Count, SpacingS, Gain, *Path, bOk ? 1 : 0);
		}));
}
