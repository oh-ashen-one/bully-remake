// OWNER: sm-voice
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BullyVoice.generated.h"

class UAudioComponent;
class USoundWaveProcedural;

/**
 * Original scripted mission speech, played from the owner's install via the importer's output
 * (`python3 -m tools.importers speech --transcribe` -> imported/speech/manifest.json + <line>/v<take>.wav, git-ignored).
 *
 * Mission data names a line by its Speech.lst stem without the take (`"voice": "Davis_1-03_002"` on a speech_sequence
 * line). Play() picks one of the recorded takes (the original varies takes), returns the line's original subtitle
 * text when the importer matched one (else its transcript, else empty -> caller keeps its own text) and the take's
 * length. Nothing imported (clean clone) or -BRNoVoice: Play() returns false and the mission shows its own text only.
 */
UCLASS()
class BULLYREMAKE_API UBullyVoiceSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UBullyVoiceSubsystem* Get(const UObject* WorldContext);

	/** Start the line's audio (stopping any line still playing). Out: subtitle text (may be empty), seconds. */
	bool Play(const FString& Line, FString& OutText, float& OutSeconds);
	void Stop();
	bool HasLine(const FString& Line);

private:
	struct FTake { FString Wav; float Seconds = 0.f; };
	struct FLine { TArray<FTake> Takes; FString Text, TextSource; };
	void LoadManifest();
	bool bLoaded = false;
	FString Root;
	TMap<FString, FLine> Lines;

	UPROPERTY() TObjectPtr<UAudioComponent> AudioComp;
	UPROPERTY() TObjectPtr<USoundWaveProcedural> Wave;
};
