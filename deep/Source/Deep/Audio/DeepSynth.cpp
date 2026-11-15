#include "Audio/DeepSynth.h"

#include "Math/RandomStream.h"

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
			{ TEXT("click"), false, 0.06f, &Click },
			{ TEXT("harpoon"), false, 0.6f, &Harpoon },
			{ TEXT("chime"), false, 1.4f, &Chime },
			{ TEXT("drone_out"), true, 8.f, &DroneOut },
			{ TEXT("drone_in"), true, 6.f, &DroneIn },
			{ TEXT("breath"), true, 8.f, [] { return Breath(4.f, 8.f, 61); } },
			{ TEXT("breath_fast"), true, 4.f, [] { return Breath(2.f, 4.f, 67); } },
			{ TEXT("winch"), true, 2.f, &Winch },
			{ TEXT("serpent"), true, 8.f, &Serpent },
			{ TEXT("hum"), true, 4.f, &Hum },
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

	bool IsBuiltin(FName Name) { return Find(Name) != nullptr; }

	bool IsLoopBuiltin(FName Name)
	{
		const FBuiltin* B = Find(Name);
		return B && B->bLoop;
	}

	int32 ExpectedSamples(FName Name)
	{
		const FBuiltin* B = Find(Name);
		return B ? Samples(B->Seconds) : 0;
	}

	FBuffer Generate(FName Name)
	{
		FBuffer Out;
		const FBuiltin* B = Find(Name);
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
