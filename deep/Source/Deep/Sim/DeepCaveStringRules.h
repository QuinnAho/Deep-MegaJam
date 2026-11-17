// Deep.World.Caves: the baked cave chunks strung into one dive (D-055, D-059). Pure rules: the Content/Data tables
// parsed, the chunks composed end to end, and the hull's place in the string's frame from the game state.
//
// Frames: a chunk's entry frame has its joint centre at the origin, +X into the chunk, Z up (cm, degrees). The string's
// frame has the start chunk's berth (its Park socket) at the origin facing +X; it hangs at stop 1's still frame, turned
// by the hull's home heading, so the game state's offset, depth and yaw are the hull's pose in it.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DeepCaveStringRules.generated.h"

/** One row of CaveChunks.csv: the exit frame and the bounds in the chunk's entry frame (cm, degrees). */
USTRUCT(BlueprintType)
struct DEEP_API FDeepCaveChunkRow
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Deep")
	FName Name;

	/** Static mesh object path (/Game/...). */
	UPROPERTY(BlueprintReadOnly, Category = "Deep")
	FString Mesh;

	/** start, tunnel, cavern, stop or end. */
	UPROPERTY(BlueprintReadOnly, Category = "Deep")
	FName Type;

	/** Joint types (rift, key, bed, ...); None for a start's entry or an end's exit. */
	UPROPERTY(BlueprintReadOnly, Category = "Deep")
	FName InType;

	UPROPERTY(BlueprintReadOnly, Category = "Deep")
	FName OutType;

	UPROPERTY(BlueprintReadOnly, Category = "Deep")
	FVector ExitCm = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Deep")
	float ExitYawDeg = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Deep")
	FVector MinCm = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Deep")
	FVector MaxCm = FVector::ZeroVector;
};

/** One row of CaveChunkSockets.csv: a named point in a chunk's entry frame. */
USTRUCT(BlueprintType)
struct DEEP_API FDeepCaveSocketRow
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Deep")
	FName Chunk;

	UPROPERTY(BlueprintReadOnly, Category = "Deep")
	FName Socket;

	UPROPERTY(BlueprintReadOnly, Category = "Deep")
	FVector LocalCm = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Deep")
	float YawDeg = 0.f;
};

/** One sample of CaveChunkPath.csv: where the hull's origin (the hatch) goes on the dive, in a chunk's entry frame. */
USTRUCT(BlueprintType)
struct DEEP_API FDeepCavePathPoint
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Deep")
	FName Chunk;

	UPROPERTY(BlueprintReadOnly, Category = "Deep")
	FVector LocalCm = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Deep")
	float YawDeg = 0.f;

	/** neck, tunnel, squeeze or cavern. */
	UPROPERTY(BlueprintReadOnly, Category = "Deep")
	FName Region;
};

/** A chunk's entry frame in the string's frame (cm, degrees). */
USTRUCT(BlueprintType)
struct DEEP_API FDeepCavePlace
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Deep")
	FName Chunk;

	UPROPERTY(BlueprintReadOnly, Category = "Deep")
	FVector LocationCm = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Deep")
	float YawDeg = 0.f;
};

/** A pose in the string's frame: the hull's origin (cm) and heading (degrees). */
USTRUCT(BlueprintType)
struct DEEP_API FDeepCavePose
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Deep")
	FVector LocationCm = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Deep")
	float YawDeg = 0.f;
};

UCLASS()
class DEEP_API UDeepCaveStringRules : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** CaveChunks.csv text (header row first; columns found by name). False, with the reason, on a missing column or a short row. */
	static bool ParseChunks(const FString& Csv, TArray<FDeepCaveChunkRow>& OutRows, FString& OutError);

	/** CaveChunkSockets.csv text. */
	static bool ParseSockets(const FString& Csv, TArray<FDeepCaveSocketRow>& OutRows, FString& OutError);

	/** CaveChunkPath.csv text (Chunk, X, Y, Z, Yaw, Region). */
	static bool ParsePath(const FString& Csv, TArray<FDeepCavePathPoint>& OutPoints, FString& OutError);

	/**
	 * The string's joint types chain (cavestring.type_check): the first chunk has no entry, the last no exit, and each
	 * exit type is the next chunk's entry type. Empty when they do, else what does not match.
	 */
	static FString CheckJoints(const TArray<FDeepCaveChunkRow>& Rows, const TArray<FName>& String);

	/** A point in a chunk's entry frame, in the string's frame: P + Rz(yaw) * local. */
	static FVector ToString(const FDeepCavePlace& Place, const FVector& LocalCm);

	/** A pose in a chunk's entry frame, in the string's frame (the yaws add). */
	static FDeepCavePose ToString(const FDeepCavePlace& Place, const FVector& LocalCm, float LocalYawDeg);

	/** A point in the string's frame, in a chunk's entry frame. */
	static FVector ToChunk(const FDeepCavePlace& Place, const FVector& StringCm);

	/**
	 * Composes the string as cavestring.string_places does (P_next = P + Rz(yaw) * exit, yaw_next = yaw + exit yaw), then
	 * moves it so the berth (a socket of one of its chunks) is the origin facing +X. False, with the reason, on an unknown
	 * chunk, a bad joint or a berth outside the string.
	 */
	static bool ComposeString(const TArray<FDeepCaveChunkRow>& Rows, const TArray<FName>& String, const FDeepCaveSocketRow& Berth, TArray<FDeepCavePlace>& OutPlaces, FString& OutError);

	static const FDeepCavePlace* FindPlace(const TArray<FDeepCavePlace>& Places, FName Chunk);

	static const FDeepCaveSocketRow* FindSocket(const TArray<FDeepCaveSocketRow>& Sockets, FName Chunk, FName Socket);

	/** The hull's origin in the string's frame from the game state: offset ahead and to starboard, depth below the top stop. */
	static FVector HullCm(const FVector2D& OffsetM, float DepthM, float TopDepthM);

	/** The other way: the game state's offset and depth for a hull origin in the string's frame. */
	static void HullToState(const FVector& HullCm, float TopDepthM, FVector2D& OutOffsetM, float& OutDepthM);

	/** The hull's motion (sub frame velocity m/s, heave + down) as a velocity in the string's frame (m/s, Z up). */
	static FVector VelocityMps(const FVector2D& VelocityMps, float HeaveMps);

	/** Path samples in the string's frame (points in unknown chunks are skipped). */
	static TArray<FDeepCavePose> PathPoses(const TArray<FDeepCavePlace>& Places, const TArray<FDeepCavePathPoint>& Path);

	/** Metres along the path to each sample (the first 0). */
	static TArray<float> PathMetres(const TArray<FDeepCavePose>& Poses);

	/**
	 * The sample a Deep.Cave.Goto names: a number is metres along the path; a chunk name is that chunk's first sample, moved
	 * on by Extra metres; a region (neck, tunnel, squeeze, cavern) is the middle of its first run. INDEX_NONE if none.
	 */
	static int32 FindPathSample(const TArray<FDeepCavePathPoint>& Path, const TArray<float>& Metres, const FString& Where, float ExtraM);
};
