// OWNER: campaign-spine
#include "BullyClockSubsystem.h"

#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogBullyClock, Log, All);

UBullyClockSubsystem* UBullyClockSubsystem::Get(const UObject* WorldContext)
{
	if (!WorldContext) { return nullptr; }
	const UWorld* World = WorldContext->GetWorld();
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UBullyClockSubsystem>() : nullptr;
}

void UBullyClockSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// Config-driven defaults (Config/Campaign.json "clock" block); missing file/block keeps the code defaults.
	FString Text;
	TSharedPtr<FJsonObject> Root;
	const TSharedPtr<FJsonObject>* Clock = nullptr;
	if (FFileHelper::LoadFileToString(Text, *(FPaths::ProjectConfigDir() / TEXT("Campaign.json")))
		&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) && Root.IsValid()
		&& Root->TryGetObjectField(TEXT("clock"), Clock) && Clock && Clock->IsValid())
	{
		double N = 0.0;
		if ((*Clock)->TryGetNumberField(TEXT("start_hour"), N)) { Hour = FMath::Clamp(int32(N), 0, 23); }
		if ((*Clock)->TryGetNumberField(TEXT("start_minute"), N)) { Minute = FMath::Clamp(int32(N), 0, 59); }
		if ((*Clock)->TryGetNumberField(TEXT("minutes_per_second"), N)) { MinutesPerSecond = FMath::Max(0.f, float(N)); }
	}
	UE_LOG(LogBullyClock, Display, TEXT("BR_CLOCK init: day %d %02d:%02d, %.2f game-min/s"), Day, Hour, Minute, MinutesPerSecond);
}

TStatId UBullyClockSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBullyClockSubsystem, STATGROUP_Tickables);
}

void UBullyClockSubsystem::Tick(float DeltaTime)
{
	if (MinutesPerSecond <= 0.f) { return; }
	MinuteCarry += DeltaTime * MinutesPerSecond;
	const int32 Whole = FMath::FloorToInt(MinuteCarry);
	if (Whole <= 0) { return; }
	MinuteCarry -= Whole;
	const int32 OldHour = Hour;
	Minute += Whole;
	while (Minute >= 60) { Minute -= 60; ++Hour; }
	while (Hour >= 24) { Hour -= 24; ++Day; }
	if (Hour != OldHour)
	{
		UE_LOG(LogBullyClock, Display, TEXT("BR_CLOCK day %d %02d:%02d"), Day, Hour, Minute);
		OnHourChanged.Broadcast(Hour);
	}
}

void UBullyClockSubsystem::PauseClock()
{
	if (bPaused) { return; }
	bPaused = true;
	UE_LOG(LogBullyClock, Display, TEXT("BR_CLOCK paused at day %d %02d:%02d"), Day, Hour, Minute);
}

void UBullyClockSubsystem::UnpauseClock()
{
	if (!bPaused) { return; }
	bPaused = false;
	UE_LOG(LogBullyClock, Display, TEXT("BR_CLOCK unpaused at day %d %02d:%02d"), Day, Hour, Minute);
}

void UBullyClockSubsystem::SetClock(int32 NewHour, int32 NewMinute)
{
	const int32 OldHour = Hour;
	Hour = FMath::Clamp(NewHour, 0, 23);
	Minute = FMath::Clamp(NewMinute, 0, 59);
	MinuteCarry = 0.f;
	UE_LOG(LogBullyClock, Display, TEXT("BR_CLOCK set -> day %d %02d:%02d%s"), Day, Hour, Minute, bPaused ? TEXT(" (paused)") : TEXT(""));
	if (Hour != OldHour) { OnHourChanged.Broadcast(Hour); }
}

bool UBullyClockSubsystem::IsInWindow(float StartH, float EndH) const
{
	const float H = GetHourFloat();
	if (StartH <= EndH) { return H >= StartH && H < EndH; }
	return H >= StartH || H < EndH;   // overnight window (e.g. 20 -> 1)
}

// ---------------------------------------------------------------------------------------------------------------
// console commands (testing / headless verification)

namespace
{
	UBullyClockSubsystem* SysFor(UWorld* W) { return UBullyClockSubsystem::Get(W); }

	FAutoConsoleCommandWithWorld GStatus(TEXT("Clock.Status"), TEXT("Log the game clock"),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W)
		{
			if (UBullyClockSubsystem* S = SysFor(W))
			{
				UE_LOG(LogBullyClock, Display, TEXT("BR_CLOCK status: day %d %02d:%02d%s"), S->GetDay(), S->GetHour(), S->GetMinute(), S->IsPaused() ? TEXT(" paused") : TEXT(""));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GSet(TEXT("Clock.Set"), TEXT("Clock.Set <h> <m>: set the game clock"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& A, UWorld* W)
		{
			if (UBullyClockSubsystem* S = SysFor(W)) { S->SetClock(A.Num() >= 1 ? FCString::Atoi(*A[0]) : 8, A.Num() >= 2 ? FCString::Atoi(*A[1]) : 0); }
		}));

	FAutoConsoleCommandWithWorld GPause(TEXT("Clock.Pause"), TEXT("Pause the game clock"),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W) { if (UBullyClockSubsystem* S = SysFor(W)) { S->PauseClock(); } }));

	FAutoConsoleCommandWithWorld GUnpause(TEXT("Clock.Unpause"), TEXT("Unpause the game clock"),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W) { if (UBullyClockSubsystem* S = SysFor(W)) { S->UnpauseClock(); } }));
}
