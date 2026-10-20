// OWNER: slice-weapons
#include "BullySfx.h"

#include "Components/AudioComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"
#include "UObject/StrongObjectPtr.h"

DEFINE_LOG_CATEGORY_STATIC(LogBullySfx, Log, All);

namespace
{
	struct FSfxEvent
	{
		TArray<TStrongObjectPtr<USoundBase>> Sounds;
		float Volume = 1.f;
		float PitchJitter = 0.f;
		bool bLoop = false;
	};

	// Same rule as tools/ue/import_ambient.py asset_name(): runs of non [A-Za-z0-9_] become '_', trimmed.
	FString AssetName(const FString& S)
	{
		FString Out;
		bool bGap = false;
		for (TCHAR C : S)
		{
			if (FChar::IsAlnum(C) || C == TEXT('_')) { if (bGap && !Out.IsEmpty()) { Out.AppendChar(TEXT('_')); } bGap = false; Out.AppendChar(C); }
			else { bGap = true; }
		}
		while (Out.StartsWith(TEXT("_"))) { Out.RightChopInline(1); }
		while (Out.EndsWith(TEXT("_"))) { Out.LeftChopInline(1); }
		return Out.IsEmpty() ? FString(TEXT("unnamed")) : Out;
	}

	TMap<FName, FSfxEvent>& Events()
	{
		static TMap<FName, FSfxEvent> Map;
		static bool bLoaded = false;
		if (bLoaded) { return Map; }
		bLoaded = true;
		FString Text;
		TSharedPtr<FJsonObject> Root;
		const TSharedPtr<FJsonObject>* Evs = nullptr;
		const FString Path = FPaths::ProjectConfigDir() / TEXT("Sfx.json");
		if (!FFileHelper::LoadFileToString(Text, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root)
			|| !Root.IsValid() || !Root->TryGetObjectField(TEXT("events"), Evs))
		{
			UE_LOG(LogBullySfx, Warning, TEXT("BR_SFX cannot read %s: all game sounds silent"), *Path);
			return Map;
		}
		int32 Found = 0, Missing = 0;
		for (const auto& KV : (*Evs)->Values)
		{
			const TSharedPtr<FJsonObject> O = KV.Value.IsValid() ? KV.Value->AsObject() : nullptr;
			const TArray<TSharedPtr<FJsonValue>>* Waves = nullptr;
			if (!O.IsValid() || !O->TryGetArrayField(TEXT("waves"), Waves)) { continue; }
			FSfxEvent E;
			double N = 0.0;
			if (O->TryGetNumberField(TEXT("volume"), N)) { E.Volume = float(N); }
			if (O->TryGetNumberField(TEXT("pitch_jitter"), N)) { E.PitchJitter = float(N); }
			O->TryGetBoolField(TEXT("loop"), E.bLoop);
			for (const TSharedPtr<FJsonValue>& WV : *Waves)
			{
				const TSharedPtr<FJsonObject> W = WV.IsValid() ? WV->AsObject() : nullptr;
				FString Bank, Name;
				if (!W.IsValid() || !W->TryGetStringField(TEXT("bank"), Bank) || !W->TryGetStringField(TEXT("name"), Name)) { continue; }
				const FString A = AssetName(Name);
				const FString Asset = FString::Printf(TEXT("/Game/Imported/Audio/%s/%s.%s"), *Bank, *A, *A);
				if (USoundBase* S = LoadObject<USoundBase>(nullptr, *Asset, nullptr, LOAD_NoWarn | LOAD_Quiet))
				{
					E.Sounds.Add(TStrongObjectPtr<USoundBase>(S));
					++Found;
				}
				else { ++Missing; }
			}
			Map.Add(FName(FString(KV.Key)), MoveTemp(E));
		}
		UE_LOG(LogBullySfx, Display, TEXT("BR_SFX %d event(s), %d wave(s) loaded, %d not imported (tools/ue/run_import.sh --sfx)"), Map.Num(), Found, Missing);
		return Map;
	}

	const FSfxEvent* Find(FName Event)
	{
		const FSfxEvent* E = Events().Find(Event);
		return E && E->Sounds.Num() > 0 ? E : nullptr;
	}
}

bool FBullySfx::Has(FName Event)
{
	return Find(Event) != nullptr;
}

void FBullySfx::Play(const UObject* WorldContext, FName Event, const FVector& At, float VolumeScale)
{
	const FSfxEvent* E = Find(Event);
	if (!E || !WorldContext) { return; }
	USoundBase* S = E->Sounds[FMath::RandRange(0, E->Sounds.Num() - 1)].Get();
	const float Pitch = 1.f + FMath::FRandRange(-E->PitchJitter, E->PitchJitter);
	UGameplayStatics::PlaySoundAtLocation(WorldContext, S, At, E->Volume * VolumeScale, Pitch);
}

UAudioComponent* FBullySfx::Loop(AActor* Owner, USceneComponent* AttachTo, FName Event, float VolumeScale)
{
	const FSfxEvent* E = Find(Event);
	if (!E || !Owner) { return nullptr; }
	UAudioComponent* A = NewObject<UAudioComponent>(Owner);
	A->bAutoActivate = false;
	A->bAutoDestroy = false;
	A->SetSound(E->Sounds[0].Get());
	if (USoundWave* W = Cast<USoundWave>(E->Sounds[0].Get())) { W->bLooping = true; }
	A->SetVolumeMultiplier(E->Volume * VolumeScale);
	if (AttachTo) { A->SetupAttachment(AttachTo); }
	A->RegisterComponent();
	A->Play();
	return A;
}
