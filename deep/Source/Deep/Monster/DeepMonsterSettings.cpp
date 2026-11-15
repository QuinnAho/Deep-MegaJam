#include "Monster/DeepMonsterSettings.h"

UDeepMonsterSettings::UDeepMonsterSettings()
{
	CategoryName = TEXT("Game");
	SectionName = TEXT("DEEP Monster");

	FDeepMonsterStopTuning Stop2;
	Stop2.Stop = 2;
	Stop2.RecoilCooldownS = 8.f;
	Stop2.StalkSpeedMps = 5.f;
	Stop2.HuntSpeedMps = 8.f;
	Stops.Add(Stop2);

	// Stop 3 is meaner: back sooner, closes faster.
	FDeepMonsterStopTuning Stop3;
	Stop3.Stop = 3;
	Stop3.RecoilCooldownS = 4.f;
	Stop3.StalkSpeedMps = 7.f;
	Stop3.HuntSpeedMps = 9.5f;
	Stops.Add(Stop3);
}

FDeepMonsterTuning UDeepMonsterSettings::GetTuning(int32 Stop) const
{
	FDeepMonsterTuning Out = Behaviour;
	const FDeepMonsterStopTuning* Best = nullptr;
	for (const FDeepMonsterStopTuning& Entry : Stops)
	{
		if (Entry.Stop <= Stop && (!Best || Entry.Stop > Best->Stop))
		{
			Best = &Entry;
		}
	}
	if (Best)
	{
		Out.RecoilCooldownS = Best->RecoilCooldownS;
		Out.StalkSpeedMps = Best->StalkSpeedMps;
		Out.HuntSpeedMps = Best->HuntSpeedMps;
	}
	return Out;
}
