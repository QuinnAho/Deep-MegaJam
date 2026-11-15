// Text data from Content/Data (shipped loose: DirectoriesToAlwaysStageAsNonUFS in DefaultGame.ini).
// FDeepCaptainLines holds CaptainLines.csv by event and picks variants; no world, so it is tested directly.

#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"

struct FDeepEventPayload;

namespace DeepData
{
	/** Absolute path of a file in Content/Data. Works in editor and packaged builds. */
	DEEP_API FString DataPath(const FString& FileName);

	/** Parses CSV text with a header row into one map per row (header -> cell). Quoted cells may hold commas. */
	DEEP_API TArray<TMap<FString, FString>> ParseCsv(const FString& Text);

	/** Loads and parses a Content/Data CSV. Empty (and a warning) if missing. */
	DEEP_API TArray<TMap<FString, FString>> LoadCsv(const FString& FileName);

	/** The `label` column of SettlementFees.csv, loaded once and cached. */
	DEEP_API const TArray<FString>& FeeLabels();
}

struct DEEP_API FDeepCaptainLine
{
	FString Id;
	FName Event;
	int32 Variant = 0;
	FString Text;
	/** The `tone` column: calm, tense or scared. NAME_None = any (also a missing column or an empty cell). */
	FName Tone;
};

class DEEP_API FDeepCaptainLines
{
public:
	/** Replaces all lines from CSV text with columns id, event, variant, text and optional tone. Returns the number of lines. */
	int32 LoadFromString(const FString& Csv);

	/** Loads Content/Data/CaptainLines.csv. */
	int32 LoadDefault();

	int32 Num() const;
	bool HasEvent(FName Event) const { return ByEvent.Contains(Event); }
	int32 NumVariants(FName Event) const;

	/**
	 * Picks a line for the event, never the variant picked last time for it when there are two or more. With a Tone,
	 * only rows of that tone, else rows of any tone, else every row. NAME_None: every row (the old picker).
	 */
	bool Pick(FName Event, FRandomStream& Rng, FDeepCaptainLine& Out, FName Tone = NAME_None);

	/** Lower-case tone name; `any`, empty and unknown words are NAME_None. */
	static FName ParseTone(const FString& Cell);

	/** Replaces {n}, {gross}, {cut}, {net} and {depth}. */
	static FString FillTokens(const FString& Text, const FDeepEventPayload& Payload);

private:
	TMap<FName, TArray<FDeepCaptainLine>> ByEvent;
	TMap<FName, int32> LastVariant;
};
