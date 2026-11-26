// D2: how smooth the pilot's view is, as a number (Debug/DeepJudder records it, Deep.Judder). Judder is motion that
// does not lie on a smooth path between its neighbours: each frame's place is compared with the time-weighted midpoint
// of the frame before and the frame after. A steady drive, a turn or an easing start leave almost nothing (under a
// millimetre at 60 fps); a frame that lags, a step that comes twice or a camera placed before the hull leave centimetres.
#pragma once

#include "CoreMinimal.h"

/** One frame: the view (the player's camera) and the hull, in world space. */
struct FDeepJudderSample
{
	double TimeS = 0.0;
	FVector CamCm = FVector::ZeroVector;
	FQuat CamRot = FQuat::Identity;
	FVector HullCm = FVector::ZeroVector;
	FQuat HullRot = FQuat::Identity;
};

/** Root mean square, 95th percentile and worst of one residual. */
struct FDeepJudderStat
{
	double Rms = 0.0;
	double P95 = 0.0;
	double Max = 0.0;
};

struct FDeepJudderStats
{
	int32 Frames = 0;
	double MeanDtMs = 0.0;
	/** Spread of the frame time (standard deviation, ms). */
	double DtSpreadMs = 0.0;
	/** The camera's place off its smooth path (cm) and its rotation (deg). */
	FDeepJudderStat CamCm;
	FDeepJudderStat CamDeg;
	/** The hull's place (cm) and heading (deg). */
	FDeepJudderStat HullCm;
	FDeepJudderStat HullDeg;
	/** Where the hull sits in the camera's frame (cm): what the eye sees shake. */
	FDeepJudderStat HullInViewCm;
	/** Frames where the hull did not move between two frames in which it did (a stalled step). */
	int32 StallFrames = 0;
	/** The hull's mean speed over the record (m/s), so a still record is not read as smooth. */
	double HullMeanMps = 0.0;
};

namespace DeepJudder
{
	/** The residual of a place at time T against the line from (T0, A) to (T1, B). */
	DEEP_API double Residual(const FVector& A, double T0, const FVector& P, double T, const FVector& B, double T1);

	/** The angle (deg) between a rotation at time T and the slerp from (T0, A) to (T1, B). */
	DEEP_API double ResidualDeg(const FQuat& A, double T0, const FQuat& Q, double T, const FQuat& B, double T1);

	/** Everything over a record of frames (at least three, in time order). */
	DEEP_API FDeepJudderStats Measure(const TArray<FDeepJudderSample>& Samples);

	/** One JSON object (no braces' label): the stats as DEEPJUDDER prints them. */
	DEEP_API FString ToJson(const FDeepJudderStats& Stats, const FString& Label);
}
