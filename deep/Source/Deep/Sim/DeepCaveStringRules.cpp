#include "Sim/DeepCaveStringRules.h"

namespace DeepCaveString
{
	/** A CSV table: the header's columns by name, then the rows' cells (empty cells kept). */
	struct FTable
	{
		TMap<FString, int32> Columns;
		TArray<TArray<FString>> Rows;
	};

	bool ReadTable(const FString& Csv, const TArray<const TCHAR*>& Needed, FTable& Out, FString& OutError)
	{
		Out = FTable();
		TArray<FString> Lines;
		Csv.ParseIntoArrayLines(Lines, true);
		if (Lines.Num() == 0)
		{
			OutError = TEXT("empty table");
			return false;
		}
		TArray<FString> Header;
		Lines[0].ParseIntoArray(Header, TEXT(","), false);
		for (int32 Index = 0; Index < Header.Num(); ++Index)
		{
			Out.Columns.Add(Header[Index].TrimStartAndEnd(), Index);
		}
		for (const TCHAR* Column : Needed)
		{
			if (!Out.Columns.Contains(Column))
			{
				OutError = FString::Printf(TEXT("missing column %s"), Column);
				return false;
			}
		}
		for (int32 Line = 1; Line < Lines.Num(); ++Line)
		{
			if (Lines[Line].TrimStartAndEnd().IsEmpty())
			{
				continue;
			}
			TArray<FString> Cells;
			Lines[Line].ParseIntoArray(Cells, TEXT(","), false);
			if (Cells.Num() < Header.Num())
			{
				OutError = FString::Printf(TEXT("row %d has %d of %d cells"), Line + 1, Cells.Num(), Header.Num());
				return false;
			}
			for (FString& Cell : Cells)
			{
				Cell.TrimStartAndEndInline();
			}
			Out.Rows.Add(MoveTemp(Cells));
		}
		return true;
	}

	const FString& Cell(const FTable& Table, const TArray<FString>& Row, const TCHAR* Column)
	{
		return Row[Table.Columns.FindChecked(Column)];
	}

	FName NameCell(const FTable& Table, const TArray<FString>& Row, const TCHAR* Column)
	{
		const FString& Text = Cell(Table, Row, Column);
		return Text.IsEmpty() ? NAME_None : FName(*Text);
	}

	double NumberCell(const FTable& Table, const TArray<FString>& Row, const TCHAR* Column)
	{
		return FCString::Atod(*Cell(Table, Row, Column));
	}

	FVector VectorCells(const FTable& Table, const TArray<FString>& Row, const TCHAR* X, const TCHAR* Y, const TCHAR* Z)
	{
		return FVector(NumberCell(Table, Row, X), NumberCell(Table, Row, Y), NumberCell(Table, Row, Z));
	}

	FVector Turn(const FVector& Local, double YawDeg)
	{
		double Sin = 0.0;
		double Cos = 1.0;
		FMath::SinCos(&Sin, &Cos, FMath::DegreesToRadians(YawDeg));
		return FVector(Local.X * Cos - Local.Y * Sin, Local.X * Sin + Local.Y * Cos, Local.Z);
	}

	const FDeepCaveChunkRow* FindRow(const TArray<FDeepCaveChunkRow>& Rows, FName Name)
	{
		return Rows.FindByPredicate([Name](const FDeepCaveChunkRow& Row) { return Row.Name == Name; });
	}
}

bool UDeepCaveStringRules::ParseChunks(const FString& Csv, TArray<FDeepCaveChunkRow>& OutRows, FString& OutError)
{
	using namespace DeepCaveString;
	OutRows.Reset();
	FTable Table;
	if (!ReadTable(Csv, { TEXT("Name"), TEXT("Mesh"), TEXT("Type"), TEXT("InType"), TEXT("OutType"), TEXT("ExitX"), TEXT("ExitY"), TEXT("ExitZ"), TEXT("ExitYaw"),
		TEXT("MinX"), TEXT("MinY"), TEXT("MinZ"), TEXT("MaxX"), TEXT("MaxY"), TEXT("MaxZ") }, Table, OutError))
	{
		return false;
	}
	for (const TArray<FString>& Row : Table.Rows)
	{
		FDeepCaveChunkRow& Out = OutRows.AddDefaulted_GetRef();
		Out.Name = NameCell(Table, Row, TEXT("Name"));
		Out.Mesh = Cell(Table, Row, TEXT("Mesh"));
		Out.Type = NameCell(Table, Row, TEXT("Type"));
		Out.InType = NameCell(Table, Row, TEXT("InType"));
		Out.OutType = NameCell(Table, Row, TEXT("OutType"));
		Out.ExitCm = VectorCells(Table, Row, TEXT("ExitX"), TEXT("ExitY"), TEXT("ExitZ"));
		Out.ExitYawDeg = float(NumberCell(Table, Row, TEXT("ExitYaw")));
		Out.MinCm = VectorCells(Table, Row, TEXT("MinX"), TEXT("MinY"), TEXT("MinZ"));
		Out.MaxCm = VectorCells(Table, Row, TEXT("MaxX"), TEXT("MaxY"), TEXT("MaxZ"));
	}
	return true;
}

bool UDeepCaveStringRules::ParseSockets(const FString& Csv, TArray<FDeepCaveSocketRow>& OutRows, FString& OutError)
{
	using namespace DeepCaveString;
	OutRows.Reset();
	FTable Table;
	if (!ReadTable(Csv, { TEXT("Chunk"), TEXT("Socket"), TEXT("X"), TEXT("Y"), TEXT("Z"), TEXT("Yaw") }, Table, OutError))
	{
		return false;
	}
	for (const TArray<FString>& Row : Table.Rows)
	{
		FDeepCaveSocketRow& Out = OutRows.AddDefaulted_GetRef();
		Out.Chunk = NameCell(Table, Row, TEXT("Chunk"));
		Out.Socket = NameCell(Table, Row, TEXT("Socket"));
		Out.LocalCm = VectorCells(Table, Row, TEXT("X"), TEXT("Y"), TEXT("Z"));
		Out.YawDeg = float(NumberCell(Table, Row, TEXT("Yaw")));
	}
	return true;
}

bool UDeepCaveStringRules::ParsePath(const FString& Csv, TArray<FDeepCavePathPoint>& OutPoints, FString& OutError)
{
	using namespace DeepCaveString;
	OutPoints.Reset();
	FTable Table;
	if (!ReadTable(Csv, { TEXT("Chunk"), TEXT("X"), TEXT("Y"), TEXT("Z"), TEXT("Yaw"), TEXT("Region") }, Table, OutError))
	{
		return false;
	}
	OutPoints.Reserve(Table.Rows.Num());
	for (const TArray<FString>& Row : Table.Rows)
	{
		FDeepCavePathPoint& Out = OutPoints.AddDefaulted_GetRef();
		Out.Chunk = NameCell(Table, Row, TEXT("Chunk"));
		Out.LocalCm = VectorCells(Table, Row, TEXT("X"), TEXT("Y"), TEXT("Z"));
		Out.YawDeg = float(NumberCell(Table, Row, TEXT("Yaw")));
		Out.Region = NameCell(Table, Row, TEXT("Region"));
	}
	return true;
}

FString UDeepCaveStringRules::CheckJoints(const TArray<FDeepCaveChunkRow>& Rows, const TArray<FName>& String)
{
	if (String.Num() == 0)
	{
		return TEXT("the string is empty");
	}
	TArray<const FDeepCaveChunkRow*> Chain;
	for (const FName Name : String)
	{
		const FDeepCaveChunkRow* Row = DeepCaveString::FindRow(Rows, Name);
		if (!Row)
		{
			return FString::Printf(TEXT("no chunk %s"), *Name.ToString());
		}
		Chain.Add(Row);
	}
	if (!Chain[0]->InType.IsNone())
	{
		return FString::Printf(TEXT("%s has an entry joint (%s): the string must start at a start"), *Chain[0]->Name.ToString(), *Chain[0]->InType.ToString());
	}
	if (!Chain.Last()->OutType.IsNone())
	{
		return FString::Printf(TEXT("%s has an exit joint (%s): the string must end at an end"), *Chain.Last()->Name.ToString(), *Chain.Last()->OutType.ToString());
	}
	for (int32 Index = 0; Index + 1 < Chain.Num(); ++Index)
	{
		if (Chain[Index]->OutType.IsNone() || Chain[Index]->OutType != Chain[Index + 1]->InType)
		{
			return FString::Printf(TEXT("%s exits by %s but %s enters by %s"), *Chain[Index]->Name.ToString(), *Chain[Index]->OutType.ToString(),
				*Chain[Index + 1]->Name.ToString(), *Chain[Index + 1]->InType.ToString());
		}
	}
	return FString();
}

FVector UDeepCaveStringRules::ToString(const FDeepCavePlace& Place, const FVector& LocalCm)
{
	return Place.LocationCm + DeepCaveString::Turn(LocalCm, Place.YawDeg);
}

FDeepCavePose UDeepCaveStringRules::ToString(const FDeepCavePlace& Place, const FVector& LocalCm, float LocalYawDeg)
{
	FDeepCavePose Pose;
	Pose.LocationCm = ToString(Place, LocalCm);
	Pose.YawDeg = Place.YawDeg + LocalYawDeg;
	return Pose;
}

FVector UDeepCaveStringRules::ToChunk(const FDeepCavePlace& Place, const FVector& StringCm)
{
	return DeepCaveString::Turn(StringCm - Place.LocationCm, -Place.YawDeg);
}

bool UDeepCaveStringRules::ComposeString(const TArray<FDeepCaveChunkRow>& Rows, const TArray<FName>& String, const FDeepCaveSocketRow& Berth, TArray<FDeepCavePlace>& OutPlaces, FString& OutError)
{
	OutPlaces.Reset();
	OutError = CheckJoints(Rows, String);
	if (!OutError.IsEmpty())
	{
		return false;
	}
	if (!String.Contains(Berth.Chunk))
	{
		OutError = FString::Printf(TEXT("the berth's chunk %s is not in the string"), *Berth.Chunk.ToString());
		return false;
	}
	FDeepCavePlace Place;
	for (const FName Name : String)
	{
		const FDeepCaveChunkRow* Row = DeepCaveString::FindRow(Rows, Name);
		Place.Chunk = Name;
		OutPlaces.Add(Place);
		const FDeepCavePose Next = ToString(Place, Row->ExitCm, Row->ExitYawDeg);
		Place.LocationCm = Next.LocationCm;
		Place.YawDeg = Next.YawDeg;
	}
	// Move the whole string so the berth is the origin facing +X.
	const FDeepCavePose At = ToString(*FindPlace(OutPlaces, Berth.Chunk), Berth.LocalCm, Berth.YawDeg);
	for (FDeepCavePlace& Each : OutPlaces)
	{
		Each.LocationCm = DeepCaveString::Turn(Each.LocationCm - At.LocationCm, -At.YawDeg);
		Each.YawDeg = FRotator::NormalizeAxis(Each.YawDeg - At.YawDeg);
	}
	return true;
}

const FDeepCavePlace* UDeepCaveStringRules::FindPlace(const TArray<FDeepCavePlace>& Places, FName Chunk)
{
	return Places.FindByPredicate([Chunk](const FDeepCavePlace& Place) { return Place.Chunk == Chunk; });
}

const FDeepCaveSocketRow* UDeepCaveStringRules::FindSocket(const TArray<FDeepCaveSocketRow>& Sockets, FName Chunk, FName Socket)
{
	return Sockets.FindByPredicate([Chunk, Socket](const FDeepCaveSocketRow& Row) { return Row.Chunk == Chunk && Row.Socket == Socket; });
}

FVector UDeepCaveStringRules::HullCm(const FVector2D& OffsetM, float DepthM, float TopDepthM)
{
	return FVector(OffsetM.X * 100.0, OffsetM.Y * 100.0, (double(TopDepthM) - double(DepthM)) * 100.0);
}

void UDeepCaveStringRules::HullToState(const FVector& HullCm, float TopDepthM, FVector2D& OutOffsetM, float& OutDepthM)
{
	OutOffsetM = FVector2D(HullCm.X / 100.0, HullCm.Y / 100.0);
	OutDepthM = float(double(TopDepthM) - HullCm.Z / 100.0);
}

FVector UDeepCaveStringRules::HullCm(const FVector2D& OffsetM, float ReadDepthM, float TopDepthM, const TArray<FDeepCaveDepthKnot>& Map)
{
	return HullCm(OffsetM, PhysicalDepthM(Map, ReadDepthM), TopDepthM);
}

void UDeepCaveStringRules::HullToState(const FVector& HullCm, float TopDepthM, const TArray<FDeepCaveDepthKnot>& Map, FVector2D& OutOffsetM, float& OutReadDepthM)
{
	float Physical = 0.f;
	HullToState(HullCm, TopDepthM, OutOffsetM, Physical);
	OutReadDepthM = ReadDepthM(Map, Physical);
}

bool UDeepCaveStringRules::ParseDepthMap(const FString& Csv, TArray<FDeepCaveDepthKnot>& OutKnots, FString& OutError)
{
	using namespace DeepCaveString;
	OutKnots.Reset();
	FTable Table;
	if (!ReadTable(Csv, { TEXT("PhysicalM"), TEXT("ReadM") }, Table, OutError))
	{
		return false;
	}
	for (const TArray<FString>& Row : Table.Rows)
	{
		FDeepCaveDepthKnot& Out = OutKnots.AddDefaulted_GetRef();
		Out.PhysicalM = float(NumberCell(Table, Row, TEXT("PhysicalM")));
		Out.ReadM = float(NumberCell(Table, Row, TEXT("ReadM")));
	}
	OutError = CheckDepthMap(OutKnots);
	return OutError.IsEmpty();
}

FString UDeepCaveStringRules::CheckDepthMap(const TArray<FDeepCaveDepthKnot>& Knots)
{
	if (Knots.IsEmpty())
	{
		return TEXT("the depth map has no knots");
	}
	for (int32 Index = 1; Index < Knots.Num(); ++Index)
	{
		if (Knots[Index].PhysicalM <= Knots[Index - 1].PhysicalM || Knots[Index].ReadM <= Knots[Index - 1].ReadM)
		{
			return FString::Printf(TEXT("knot %d (%.2f m reads %.2f m) does not lie below knot %d (%.2f m reads %.2f m)"), Index, Knots[Index].PhysicalM, Knots[Index].ReadM,
				Index - 1, Knots[Index - 1].PhysicalM, Knots[Index - 1].ReadM);
		}
	}
	return FString();
}

namespace DeepCaveString
{
	/** Piecewise linear from one column to the other, slope 1 past either end. */
	float MapDepth(const TArray<FDeepCaveDepthKnot>& Knots, float From, bool bToRead)
	{
		if (Knots.IsEmpty())
		{
			return From;
		}
		const auto In = [bToRead](const FDeepCaveDepthKnot& K) { return double(bToRead ? K.PhysicalM : K.ReadM); };
		const auto Out = [bToRead](const FDeepCaveDepthKnot& K) { return double(bToRead ? K.ReadM : K.PhysicalM); };
		if (From <= In(Knots[0]))
		{
			return float(Out(Knots[0]) + (From - In(Knots[0])));
		}
		for (int32 Index = 1; Index < Knots.Num(); ++Index)
		{
			if (From <= In(Knots[Index]))
			{
				const double Span = In(Knots[Index]) - In(Knots[Index - 1]);
				const double Alpha = Span > 0.0 ? (From - In(Knots[Index - 1])) / Span : 1.0;
				return float(Out(Knots[Index - 1]) + Alpha * (Out(Knots[Index]) - Out(Knots[Index - 1])));
			}
		}
		return float(Out(Knots.Last()) + (From - In(Knots.Last())));
	}
}

float UDeepCaveStringRules::ReadDepthM(const TArray<FDeepCaveDepthKnot>& Knots, float PhysicalM)
{
	return DeepCaveString::MapDepth(Knots, PhysicalM, true);
}

float UDeepCaveStringRules::PhysicalDepthM(const TArray<FDeepCaveDepthKnot>& Knots, float ReadM)
{
	return DeepCaveString::MapDepth(Knots, ReadM, false);
}

bool UDeepCaveStringRules::StringDepthMap(const TArray<FDeepCavePlace>& Places, const TArray<FDeepCaveSocketRow>& Sockets, const TArray<FName>& Berths, FName ParkSocket,
	const TArray<float>& StopDepthsM, TArray<FDeepCaveDepthKnot>& OutKnots, FString& OutError)
{
	OutKnots.Reset();
	if (Berths.IsEmpty() || StopDepthsM.IsEmpty())
	{
		OutError = TEXT("no berths or no stops");
		return false;
	}
	const double Top = StopDepthsM[0];
	const auto TrueDepth = [Top](double ZCm) { return float(Top - ZCm / 100.0); };
	for (int32 Index = 0; Index < Berths.Num() && Index < StopDepthsM.Num(); ++Index)
	{
		const FDeepCavePlace* Place = FindPlace(Places, Berths[Index]);
		const FDeepCaveSocketRow* Park = FindSocket(Sockets, Berths[Index], ParkSocket);
		if (!Place || !Park)
		{
			OutError = FString::Printf(TEXT("no %s socket on a placed %s"), *ParkSocket.ToString(), *Berths[Index].ToString());
			return false;
		}
		if (Index > 0)
		{
			// True down to the chunk's entry joint, carried on from the knots above.
			const float Entry = TrueDepth(Place->LocationCm.Z);
			OutKnots.Add({ Entry, ReadDepthM(OutKnots, Entry) });
		}
		OutKnots.Add({ TrueDepth(ToString(*Place, Park->LocalCm).Z), StopDepthsM[Index] });
	}
	OutError = CheckDepthMap(OutKnots);
	return OutError.IsEmpty();
}

FVector UDeepCaveStringRules::VelocityMps(const FVector2D& VelocityMps, float HeaveMps)
{
	return FVector(VelocityMps.X, VelocityMps.Y, -HeaveMps);
}

TArray<FDeepCavePose> UDeepCaveStringRules::PathPoses(const TArray<FDeepCavePlace>& Places, const TArray<FDeepCavePathPoint>& Path)
{
	TArray<FDeepCavePose> Poses;
	Poses.Reserve(Path.Num());
	for (const FDeepCavePathPoint& Point : Path)
	{
		if (const FDeepCavePlace* Place = FindPlace(Places, Point.Chunk))
		{
			Poses.Add(ToString(*Place, Point.LocalCm, Point.YawDeg));
		}
	}
	return Poses;
}

TArray<float> UDeepCaveStringRules::PathMetres(const TArray<FDeepCavePose>& Poses)
{
	TArray<float> Metres;
	Metres.Reserve(Poses.Num());
	double Along = 0.0;
	for (int32 Index = 0; Index < Poses.Num(); ++Index)
	{
		if (Index > 0)
		{
			Along += FVector::Dist(Poses[Index - 1].LocationCm, Poses[Index].LocationCm) / 100.0;
		}
		Metres.Add(float(Along));
	}
	return Metres;
}

int32 UDeepCaveStringRules::FindPathSample(const TArray<FDeepCavePathPoint>& Path, const TArray<float>& Metres, const FString& Where, float ExtraM)
{
	const int32 Count = FMath::Min(Path.Num(), Metres.Num());
	if (Count == 0)
	{
		return INDEX_NONE;
	}
	const auto AtMetres = [&](float Target, int32 From)
	{
		for (int32 Index = From; Index < Count; ++Index)
		{
			if (Metres[Index] >= Target - 0.01f)
			{
				return Index;
			}
		}
		return Count - 1;
	};
	if (Where.IsNumeric())
	{
		return AtMetres(FCString::Atof(*Where) + ExtraM, 0);
	}
	const FName Name(*Where);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (Path[Index].Chunk == Name)
		{
			return ExtraM > 0.f ? AtMetres(Metres[Index] + ExtraM, Index) : Index;
		}
	}
	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (Path[Index].Region == Name)
		{
			int32 End = Index;
			while (End + 1 < Count && Path[End + 1].Region == Name)
			{
				++End;
			}
			const int32 Middle = (Index + End) / 2;
			return ExtraM > 0.f ? AtMetres(Metres[Middle] + ExtraM, Middle) : Middle;
		}
	}
	return INDEX_NONE;
}
