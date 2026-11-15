#include "Story/DeepCaptainLines.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/Csv/CsvParser.h"
#include "Story/DeepEventBus.h"

DEFINE_LOG_CATEGORY_STATIC(LogDeepData, Log, All);

FString DeepData::DataPath(const FString& FileName)
{
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("Data") / FileName);
}

TArray<TMap<FString, FString>> DeepData::ParseCsv(const FString& Text)
{
	TArray<TMap<FString, FString>> Out;
	const FCsvParser Parser(Text);
	const FCsvParser::FRows& Rows = Parser.GetRows();
	if (Rows.Num() < 2)
	{
		return Out;
	}
	TArray<FString> Header;
	for (const TCHAR* Cell : Rows[0])
	{
		Header.Add(FString(Cell).TrimStartAndEnd());
	}
	for (int32 RowIndex = 1; RowIndex < Rows.Num(); ++RowIndex)
	{
		const TArray<const TCHAR*>& Row = Rows[RowIndex];
		TMap<FString, FString> Map;
		bool bAnyCell = false;
		for (int32 Col = 0; Col < Header.Num() && Col < Row.Num(); ++Col)
		{
			const FString Cell(Row[Col]);
			bAnyCell |= !Cell.TrimStartAndEnd().IsEmpty();
			Map.Add(Header[Col], Cell);
		}
		if (bAnyCell)
		{
			Out.Add(MoveTemp(Map));
		}
	}
	return Out;
}

TArray<TMap<FString, FString>> DeepData::LoadCsv(const FString& FileName)
{
	const FString Path = DataPath(FileName);
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		UE_LOG(LogDeepData, Warning, TEXT("Could not read %s"), *Path);
		return {};
	}
	return ParseCsv(Text);
}

const TArray<FString>& DeepData::FeeLabels()
{
	static TArray<FString> Labels;
	static bool bLoaded = false;
	if (!bLoaded)
	{
		bLoaded = true;
		for (const TMap<FString, FString>& Row : LoadCsv(TEXT("SettlementFees.csv")))
		{
			if (const FString* Label = Row.Find(TEXT("label")); Label && !Label->IsEmpty())
			{
				Labels.Add(*Label);
			}
		}
		if (Labels.IsEmpty())
		{
			Labels.Add(TEXT("Fees"));
		}
	}
	return Labels;
}

int32 FDeepCaptainLines::LoadFromString(const FString& Csv)
{
	ByEvent.Reset();
	LastVariant.Reset();
	int32 Count = 0;
	for (const TMap<FString, FString>& Row : DeepData::ParseCsv(Csv))
	{
		const FString* Event = Row.Find(TEXT("event"));
		const FString* Text = Row.Find(TEXT("text"));
		if (!Event || !Text || Event->IsEmpty() || Text->IsEmpty())
		{
			continue;
		}
		FDeepCaptainLine Line;
		Line.Event = FName(*Event->TrimStartAndEnd());
		Line.Text = *Text;
		if (const FString* Id = Row.Find(TEXT("id")))
		{
			Line.Id = *Id;
		}
		if (const FString* Variant = Row.Find(TEXT("variant")))
		{
			Line.Variant = FCString::Atoi(**Variant);
		}
		if (const FString* Tone = Row.Find(TEXT("tone")))
		{
			Line.Tone = ParseTone(*Tone);
		}
		ByEvent.FindOrAdd(Line.Event).Add(MoveTemp(Line));
		++Count;
	}
	return Count;
}

int32 FDeepCaptainLines::LoadDefault()
{
	const FString Path = DeepData::DataPath(TEXT("CaptainLines.csv"));
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		UE_LOG(LogDeepData, Warning, TEXT("Could not read %s; the Captain is silent"), *Path);
		return 0;
	}
	const int32 Count = LoadFromString(Text);
	UE_LOG(LogDeepData, Log, TEXT("Loaded %d Captain lines for %d events from %s"), Count, ByEvent.Num(), *Path);
	return Count;
}

int32 FDeepCaptainLines::Num() const
{
	int32 Count = 0;
	for (const TPair<FName, TArray<FDeepCaptainLine>>& Pair : ByEvent)
	{
		Count += Pair.Value.Num();
	}
	return Count;
}

int32 FDeepCaptainLines::NumVariants(FName Event) const
{
	const TArray<FDeepCaptainLine>* Lines = ByEvent.Find(Event);
	return Lines ? Lines->Num() : 0;
}

FName FDeepCaptainLines::ParseTone(const FString& Cell)
{
	const FString Word = Cell.TrimStartAndEnd().ToLower();
	if (Word == TEXT("calm") || Word == TEXT("tense") || Word == TEXT("scared"))
	{
		return FName(*Word);
	}
	return NAME_None;
}

bool FDeepCaptainLines::Pick(FName Event, FRandomStream& Rng, FDeepCaptainLine& Out, FName Tone)
{
	const TArray<FDeepCaptainLine>* Lines = ByEvent.Find(Event);
	if (!Lines || Lines->IsEmpty())
	{
		return false;
	}
	// The candidates: rows of the tone, else rows of any tone, else all of them.
	TArray<int32, TInlineAllocator<16>> Candidates;
	auto Collect = [&](auto&& Keep)
	{
		Candidates.Reset();
		for (int32 I = 0; I < Lines->Num(); ++I)
		{
			if (Keep((*Lines)[I]))
			{
				Candidates.Add(I);
			}
		}
	};
	if (!Tone.IsNone())
	{
		Collect([Tone](const FDeepCaptainLine& L) { return L.Tone == Tone; });
		if (Candidates.IsEmpty())
		{
			Collect([](const FDeepCaptainLine& L) { return L.Tone.IsNone(); });
		}
	}
	if (Candidates.IsEmpty())
	{
		Collect([](const FDeepCaptainLine&) { return true; });
	}
	// Never the line picked last time for this event when there is another.
	if (Candidates.Num() > 1)
	{
		if (const int32* Last = LastVariant.Find(Event))
		{
			Candidates.Remove(*Last);
		}
	}
	const int32 Index = Candidates[Candidates.Num() > 1 ? Rng.RandRange(0, Candidates.Num() - 1) : 0];
	LastVariant.Add(Event, Index);
	Out = (*Lines)[Index];
	return true;
}

FString FDeepCaptainLines::FillTokens(const FString& Text, const FDeepEventPayload& Payload)
{
	FString Out = Text;
	Out.ReplaceInline(TEXT("{n}"), *FString::FromInt(Payload.DiverNumber));
	Out.ReplaceInline(TEXT("{gross}"), *FString::FromInt(Payload.Gross));
	Out.ReplaceInline(TEXT("{cut}"), *FString::FromInt(Payload.Cut));
	Out.ReplaceInline(TEXT("{net}"), *FString::FromInt(Payload.Net));
	Out.ReplaceInline(TEXT("{depth}"), *FString::FromInt(FMath::RoundToInt(Payload.DepthM)));
	return Out;
}
