// OWNER: sm-voice
#include "BullyVoice.h"

#include "BullyCutscene.h"
#include "Components/AudioComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sound/SoundWaveProcedural.h"

DEFINE_LOG_CATEGORY_STATIC(LogBullyVoice, Log, All);

UBullyVoiceSubsystem* UBullyVoiceSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	return W ? W->GetSubsystem<UBullyVoiceSubsystem>() : nullptr;
}

void UBullyVoiceSubsystem::LoadManifest()
{
	bLoaded = true;
	if (FParse::Param(FCommandLine::Get(), TEXT("BRNoVoice"))) { UE_LOG(LogBullyVoice, Display, TEXT("BR_VOICE disabled (-BRNoVoice)")); return; }
	const FString Dirs[] = {
		FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Imported/Speech")),
		FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../../imported/speech"))),
	};
	for (const FString& D : Dirs)
	{
		FString Json;
		if (!FFileHelper::LoadFileToString(Json, *FPaths::Combine(D, TEXT("manifest.json")))) { continue; }
		TSharedPtr<FJsonObject> Root0;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root0) || !Root0.IsValid()) { continue; }
		const TSharedPtr<FJsonObject>* Ls = nullptr;
		if (!Root0->TryGetObjectField(TEXT("lines"), Ls)) { continue; }
		Root = D;
		for (const auto& KV : (*Ls)->Values)
		{
			const TSharedPtr<FJsonObject> O = KV.Value->AsObject();
			if (!O.IsValid()) { continue; }
			FLine L;
			O->TryGetStringField(TEXT("text"), L.Text);
			O->TryGetStringField(TEXT("text_source"), L.TextSource);
			const TArray<TSharedPtr<FJsonValue>>* Ts = nullptr;
			if (O->TryGetArrayField(TEXT("takes"), Ts))
			{
				for (const TSharedPtr<FJsonValue>& Tv : *Ts)
				{
					const TSharedPtr<FJsonObject> T = Tv->AsObject();
					FTake K;
					double S = 0.0;
					if (T.IsValid() && T->TryGetStringField(TEXT("wav"), K.Wav) && T->TryGetNumberField(TEXT("seconds"), S))
					{
						K.Seconds = float(S);
						L.Takes.Add(K);
					}
				}
			}
			if (L.Takes.Num() > 0) { Lines.Add(FString(KV.Key), MoveTemp(L)); }
		}
		UE_LOG(LogBullyVoice, Display, TEXT("BR_VOICE manifest %s: %d line(s)"), *D, Lines.Num());
		return;
	}
	UE_LOG(LogBullyVoice, Display, TEXT("BR_VOICE not imported (run python3 -m tools.importers speech --transcribe)"));
}

bool UBullyVoiceSubsystem::HasLine(const FString& Line)
{
	if (!bLoaded) { LoadManifest(); }
	return Lines.Contains(Line);
}

bool UBullyVoiceSubsystem::Play(const FString& Line, FString& OutText, float& OutSeconds)
{
	if (!bLoaded) { LoadManifest(); }
	const FLine* L = Lines.Find(Line);
	if (!L) { return false; }
	const int32 TakeIdx = FMath::RandRange(0, L->Takes.Num() - 1);
	const FTake& T = L->Takes[TakeIdx];
	TArray<uint8> Bytes, Pcm;
	int32 Rate = 0, Channels = 0;
	if (!FFileHelper::LoadFileToArray(Bytes, *FPaths::Combine(Root, T.Wav))
		|| !UBullyCutsceneSubsystem::ParseWavBytes(Bytes, Rate, Channels, Pcm))
	{
		UE_LOG(LogBullyVoice, Warning, TEXT("BR_VOICE %s: unreadable take %s"), *Line, *T.Wav);
		return false;
	}
	Stop();
	Wave = NewObject<USoundWaveProcedural>(this);
	Wave->SetSampleRate(Rate);
	Wave->NumChannels = Channels;
	Wave->Duration = float(Pcm.Num()) / float(Rate * Channels * 2);
	Wave->SoundGroup = SOUNDGROUP_Voice;
	Wave->bLooping = false;
	Wave->QueueAudio(Pcm.GetData(), Pcm.Num());
	AudioComp = UGameplayStatics::CreateSound2D(GetWorld(), Wave, 1.f, 1.f, 0.f, nullptr, false, false);
	if (AudioComp) { AudioComp->Play(); }
	OutText = L->Text;
	OutSeconds = Wave->Duration;
	UE_LOG(LogBullyVoice, Display, TEXT("BR_VOICE play %s take=%d/%d %.2fs text=%s"), *Line, TakeIdx + 1, L->Takes.Num(),
		OutSeconds, L->TextSource.IsEmpty() ? TEXT("none") : *L->TextSource);
	return true;
}

void UBullyVoiceSubsystem::Stop()
{
	if (AudioComp) { AudioComp->Stop(); AudioComp = nullptr; }
	Wave = nullptr;
}
