#include "Audio/DeepAudioSettings.h"

UDeepAudioSettings::UDeepAudioSettings()
{
	CategoryName = TEXT("Game");
	SectionName = TEXT("DEEP Audio");
}

float UDeepAudioSettings::GroupVolume(FName Group) const
{
	static const FName Sfx(TEXT("sfx"));
	static const FName Ui(TEXT("ui"));
	static const FName Ambience(TEXT("ambience"));
	static const FName Creature(TEXT("creature"));
	if (Group == Sfx) { return SfxVolume; }
	if (Group == Ui) { return UiVolume; }
	if (Group == Ambience) { return AmbienceVolume; }
	if (Group == Creature) { return CreatureVolume; }
	if (Group == FName(TEXT("voice"))) { return VoiceVolume; }
	return 1.f;
}
