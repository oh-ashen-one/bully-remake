// OWNER: campaign-spine
#include "BullyCampaignSubsystem.h"

#include "BullyCharacter.h"
#include "BullyClockSubsystem.h"
#include "BullyProgressSubsystem.h"
#include "BullyWeaponComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogBullyCampaign, Log, All);

namespace
{
	// Original numeric weapon/ammo ids -> Config/Weapons.json names. (437 skateboard = a ride and 328 camera = an
	// unbuilt system: logged, not granted. 397 fountain firework and other unmapped ids have no arsenal entry yet.)
	FName MapWeaponId(int32 Id)
	{
		switch (Id)
		{
		case 303: return FName(TEXT("slingshot"));
		case 306: return FName(TEXT("slingshot"));   // super slingshot; the arsenal has no separate entry yet
		default: return FName();
		}
	}

	FName MapAmmoId(int32 Id)
	{
		switch (Id)
		{
		case 309: return FName(TEXT("stinkbomb"));
		case 312: return FName(TEXT("eggs"));
		case 394: return FName(TEXT("itchpowder"));
		default: return FName();
		}
	}

	UBullyWeaponComponent* PlayerWeapons(const UGameInstance* GI)
	{
		const UWorld* World = GI ? GI->GetWorld() : nullptr;
		ABullyCharacter* Player = World ? Cast<ABullyCharacter>(UGameplayStatics::GetPlayerPawn(World, 0)) : nullptr;
		return Player ? Player->GetWeapons() : nullptr;
	}

	bool ReadVec3(const FJsonObject& O, const TCHAR* Field, FVector& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (O.TryGetArrayField(Field, A) && A->Num() >= 3)
		{
			Out = FVector((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), (*A)[2]->AsNumber());
			return true;
		}
		return false;
	}

	FVector GameToUe(const FVector& M) { return FVector(M.X * 100.0, -M.Y * 100.0, M.Z * 100.0); }
}

UBullyCampaignSubsystem* UBullyCampaignSubsystem::Get(const UObject* WorldContext)
{
	if (!WorldContext) { return nullptr; }
	const UWorld* World = WorldContext->GetWorld();
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UBullyCampaignSubsystem>() : nullptr;
}

void UBullyCampaignSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	const FString Path = FPaths::ProjectConfigDir() / TEXT("Campaign.json");
	FString Text;
	TSharedPtr<FJsonObject> Root;
	if (!FFileHelper::LoadFileToString(Text, *Path)
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
	{
		UE_LOG(LogBullyCampaign, Display, TEXT("BR_CAMPAIGN no %s (or invalid JSON): no campaign table, every mission available"), *Path);
		return;
	}

	TArray<FString> Factions;
	if (Root->TryGetStringArrayField(TEXT("factions"), Factions))
	{
		for (const FString& F : Factions) { Respect.Add(FName(*F), 0); }
	}

	const TSharedPtr<FJsonObject>* Rows = nullptr;
	if (Root->TryGetObjectField(TEXT("missions"), Rows) && Rows->IsValid())
	{
		for (const auto& Pair : (*Rows)->Values)
		{
			TSharedPtr<FJsonObject> O = Pair.Value->AsObject();
			if (!O.IsValid()) { continue; }
			FCampaignMission M;
			M.Id = FString(*Pair.Key);
			M.Title = M.Id;
			O->TryGetStringField(TEXT("title"), M.Title);
			double N = 0.0;
			if (O->TryGetNumberField(TEXT("chapter"), N)) { M.Chapter = int32(N); }
			FString S;
			if (O->TryGetStringField(TEXT("star"), S)) { M.Star = FName(*S); }
			const TSharedPtr<FJsonObject>* Loc = nullptr;
			if (O->TryGetObjectField(TEXT("star_location"), Loc) && Loc->IsValid())
			{
				double A = 0.0;
				if ((*Loc)->TryGetNumberField(TEXT("area"), A)) { M.StarArea = int32(A); }
				FVector G;
				if (ReadVec3(**Loc, TEXT("location_cm"), M.StarLocation)) { M.bHasStarLocation = true; }
				else if (ReadVec3(**Loc, TEXT("game_m"), G)) { M.StarLocation = GameToUe(G); M.bHasStarLocation = true; }
				if (!M.bHasStarLocation) { UE_LOG(LogBullyCampaign, Warning, TEXT("BR_CAMPAIGN %s: star_location has no location"), *M.Id); }
			}
			const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
			if (O->TryGetArrayField(TEXT("window"), Arr) && Arr->Num() >= 2)
			{
				M.bHasWindow = true;
				M.WindowStart = float((*Arr)[0]->AsNumber());
				M.WindowEnd = float((*Arr)[1]->AsNumber());
			}
			O->TryGetStringArrayField(TEXT("prereq"), M.Prereq);
			const TSharedPtr<FJsonObject>* OnPass = nullptr;
			if (O->TryGetObjectField(TEXT("on_pass"), OnPass) && OnPass->IsValid()) { M.OnPass = *OnPass; }
			Missions.Add(M.Id, MoveTemp(M));
		}
	}
	bLoaded = Missions.Num() > 0;
	UE_LOG(LogBullyCampaign, Display, TEXT("BR_CAMPAIGN loaded %d mission rows, %d factions, chapter %d"), Missions.Num(), Respect.Num(), Chapter);
}

const FCampaignMission* UBullyCampaignSubsystem::FindMission(const FString& Id) const
{
	return Missions.Find(Id);
}

bool UBullyCampaignSubsystem::IsAvailable(const FString& Id) const
{
	const FCampaignMission* M = Missions.Find(Id);
	if (!M) { return true; }   // outside the campaign table: sandbox/smoke missions stay ungated
	if (Completion.Contains(Id)) { return false; }   // v1: story missions are one-shot
	if (M->Chapter > 0 && M->Chapter != Chapter) { return false; }
	for (const FString& P : M->Prereq)
	{
		if (!Completion.Contains(P)) { return false; }
	}
	if (M->bHasWindow)
	{
		const UBullyClockSubsystem* Clock = UBullyClockSubsystem::Get(GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr);
		if (Clock && !Clock->IsInWindow(M->WindowStart, M->WindowEnd)) { return false; }
	}
	return true;
}

void UBullyCampaignSubsystem::SetChapter(int32 NewChapter)
{
	if (NewChapter == Chapter) { return; }
	Chapter = NewChapter;
	UE_LOG(LogBullyCampaign, Display, TEXT("BR_CAMPAIGN chapter -> %d"), Chapter);
	OnChanged.Broadcast();
}

void UBullyCampaignSubsystem::SetRespect(FName Faction, int32 Value)
{
	if (Respect.Contains(Faction) || !Faction.IsNone())
	{
		Respect.FindOrAdd(Faction) = Value;
		UE_LOG(LogBullyCampaign, Display, TEXT("BR_CAMPAIGN respect %s = %d"), *Faction.ToString(), Value);
		OnChanged.Broadcast();
	}
}

void UBullyCampaignSubsystem::NotifyPassed(const FString& Id)
{
	if (Passing.Contains(Id))
	{
		UE_LOG(LogBullyCampaign, Warning, TEXT("BR_CAMPAIGN NotifyPassed %s: recursion (a force_complete cycle?), skipped"), *Id);
		return;
	}
	Passing.Add(Id);
	if (const FCampaignMission* M = Missions.Find(Id))
	{
		ApplyOnPass(*M);
	}
	else
	{
		UE_LOG(LogBullyCampaign, Display, TEXT("BR_CAMPAIGN passed %s: no table row (counted only)"), *Id);
	}
	Passing.Remove(Id);
	const int32 Count = (Completion.FindOrAdd(Id) += 1);
	UE_LOG(LogBullyCampaign, Display, TEXT("BR_CAMPAIGN passed %s (count %d)"), *Id, Count);
	OnChanged.Broadcast();
}

void UBullyCampaignSubsystem::ApplyOnPass(const FCampaignMission& Row)
{
	const TSharedPtr<FJsonObject>& P = Row.OnPass;
	if (!P.IsValid()) { return; }
	for (const auto& Pair : P->Values)
	{
		const FString Key(*Pair.Key);
		const TSharedPtr<FJsonValue>& V = Pair.Value;
		if (Key == TEXT("give_weapon")) { ApplyWeaponGrant(V); }
		else if (Key == TEXT("give_ammo")) { ApplyAmmoGrant(V); }
		else if (Key == TEXT("respect"))
		{
			const TSharedPtr<FJsonObject> R = V->AsObject();
			if (R.IsValid())
			{
				for (const auto& Rp : R->Values) { SetRespect(FName(*Rp.Key), int32(Rp.Value->AsNumber())); }
			}
		}
		else if (Key == TEXT("force_complete"))
		{
			if (V->Type == EJson::Array)
			{
				for (const TSharedPtr<FJsonValue>& E : V->AsArray()) { NotifyPassed(E->AsString()); }
			}
			else { NotifyPassed(V->AsString()); }
		}
		else if (Key == TEXT("chapter_set")) { SetChapter(int32(V->AsNumber())); }
		else if (Key == TEXT("unlock_collectible"))
		{
			const FName C(*V->AsString());
			Collectibles.Add(C);
			UE_LOG(LogBullyCampaign, Display, TEXT("BR_CAMPAIGN collectible unlocked: %s"), *C.ToString());
		}
		else if (Key == TEXT("money"))
		{
			// on_pass money is in the original's cents; the persistent wallet counts whole dollars.
			const int32 Cents = int32(V->AsNumber());
			if (UBullyProgressSubsystem* Prog = UBullyProgressSubsystem::Get(GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr))
			{
				Prog->AddMoney(Cents / 100);
			}
		}
		else if (Key == TEXT("clock"))
		{
			const FString S = V->AsString();
			if (UBullyClockSubsystem* Clock = UBullyClockSubsystem::Get(GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr))
			{
				FString Hs, Ms;
				if (S.Equals(TEXT("pause"), ESearchCase::IgnoreCase)) { Clock->PauseClock(); }
				else if (S.Equals(TEXT("unpause"), ESearchCase::IgnoreCase)) { Clock->UnpauseClock(); }
				else if (S.Split(TEXT(":"), &Hs, &Ms)) { Clock->SetClock(FCString::Atoi(*Hs), FCString::Atoi(*Ms)); }
				else { UE_LOG(LogBullyCampaign, Warning, TEXT("BR_CAMPAIGN %s on_pass: bad clock value \"%s\""), *Row.Id, *S); }
			}
		}
		else
		{
			UE_LOG(LogBullyCampaign, Warning, TEXT("BR_CAMPAIGN %s on_pass: unknown key \"%s\" (ignored)"), *Row.Id, *Key);
		}
	}
}

void UBullyCampaignSubsystem::ApplyWeaponGrant(const TSharedPtr<FJsonValue>& V)
{
	if (!V.IsValid()) { return; }
	if (V->Type == EJson::String) { GiveWeaponByName(FName(*V->AsString())); }
	else { GiveWeaponByGameId(int32(V->AsNumber())); }
}

void UBullyCampaignSubsystem::ApplyAmmoGrant(const TSharedPtr<FJsonValue>& V)
{
	if (!V.IsValid()) { return; }
	if (V->Type == EJson::Array)
	{
		for (const TSharedPtr<FJsonValue>& E : V->AsArray()) { ApplyAmmoGrant(E); }
		return;
	}
	const TSharedPtr<FJsonObject> O = V->AsObject();
	if (!O.IsValid()) { UE_LOG(LogBullyCampaign, Warning, TEXT("BR_CAMPAIGN give_ammo: expected {id, n}")); return; }
	double Id = 0.0, N = 0.0;
	FString Name;
	O->TryGetNumberField(TEXT("n"), N);
	if (O->TryGetStringField(TEXT("id"), Name)) { GiveAmmoByName(FName(*Name), int32(N)); }
	else if (O->TryGetNumberField(TEXT("id"), Id)) { GiveAmmoByGameId(int32(Id), int32(N)); }
}

void UBullyCampaignSubsystem::GiveWeaponByGameId(int32 GameId)
{
	if (GameId == 437) { UE_LOG(LogBullyCampaign, Display, TEXT("BR_CAMPAIGN give_weapon 437 (skateboard): a ride, not an arsenal entry — logged only")); return; }
	if (GameId == 328) { UE_LOG(LogBullyCampaign, Display, TEXT("BR_CAMPAIGN give_weapon 328 (camera): photography system not built — logged only")); return; }
	const FName Mapped = MapWeaponId(GameId);
	if (Mapped.IsNone()) { UE_LOG(LogBullyCampaign, Warning, TEXT("BR_CAMPAIGN give_weapon %d: no Weapons.json mapping"), GameId); return; }
	if (GameId == 306) { UE_LOG(LogBullyCampaign, Display, TEXT("BR_CAMPAIGN give_weapon 306 (super slingshot) -> slingshot (no separate entry yet)")); }
	GiveWeaponByName(Mapped);
}

void UBullyCampaignSubsystem::GiveWeaponByName(FName WeaponId)
{
	GrantedWeapons.Add(WeaponId);   // remembered for the story save (weapon unlocks)
	// v1: every weapon starts unlocked (BullyWeaponComponent), so a grant is a top-up plus the log line.
	if (UBullyWeaponComponent* W = PlayerWeapons(GetGameInstance()))
	{
		W->GiveWeapon(WeaponId);
	}
	UE_LOG(LogBullyCampaign, Display, TEXT("BR_CAMPAIGN give_weapon %s"), *WeaponId.ToString());
}

void UBullyCampaignSubsystem::GiveAmmoByGameId(int32 GameId, int32 N)
{
	const FName Mapped = MapAmmoId(GameId);
	if (Mapped.IsNone()) { UE_LOG(LogBullyCampaign, Warning, TEXT("BR_CAMPAIGN give_ammo %d x%d: no Weapons.json mapping"), GameId, N); return; }
	GiveAmmoByName(Mapped, N);
}

void UBullyCampaignSubsystem::GiveAmmoByName(FName WeaponId, int32 N)
{
	if (UBullyWeaponComponent* W = PlayerWeapons(GetGameInstance()))
	{
		W->GiveAmmo(WeaponId, N);
	}
	UE_LOG(LogBullyCampaign, Display, TEXT("BR_CAMPAIGN give_ammo %s x%d"), *WeaponId.ToString(), N);
}

// ---------------------------------------------------------------------------------------------------------------
// console commands (testing / headless verification)

namespace
{
	UBullyCampaignSubsystem* SysFor(UWorld* W) { return UBullyCampaignSubsystem::Get(W); }

	FAutoConsoleCommandWithWorld GCampaignStatus(TEXT("Campaign.Status"), TEXT("Log chapter, completed missions and respect"),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W)
		{
			if (UBullyCampaignSubsystem* S = SysFor(W))
			{
				UE_LOG(LogBullyCampaign, Display, TEXT("BR_CAMPAIGN status: chapter=%d rows=%d"), S->GetChapter(), S->GetMissions().Num());
				for (const auto& Pair : S->GetMissions())
				{
					UE_LOG(LogBullyCampaign, Display, TEXT("BR_CAMPAIGN   %s '%s' ch%d completed=%d available=%d"),
						*Pair.Key, *Pair.Value.Title, Pair.Value.Chapter, S->GetCompletionCount(Pair.Key), S->IsAvailable(Pair.Key) ? 1 : 0);
				}
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GPass(TEXT("Campaign.Pass"), TEXT("Campaign.Pass <id>: force-pass a mission (applies on_pass)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& A, UWorld* W)
		{
			if (UBullyCampaignSubsystem* S = SysFor(W)) { if (A.Num()) { S->NotifyPassed(A[0]); } }
		}));

	FAutoConsoleCommandWithWorldAndArgs GChapter(TEXT("Campaign.Chapter"), TEXT("Campaign.Chapter <n>: set the current chapter (1-based)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& A, UWorld* W)
		{
			if (UBullyCampaignSubsystem* S = SysFor(W)) { if (A.Num()) { S->SetChapter(FCString::Atoi(*A[0])); } }
		}));

	FAutoConsoleCommandWithWorldAndArgs GRespectFaction(TEXT("Campaign.Respect"), TEXT("Campaign.Respect <faction> <n>: set a faction's respect"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& A, UWorld* W)
		{
			if (UBullyCampaignSubsystem* S = SysFor(W)) { if (A.Num() >= 2) { S->SetRespect(FName(*A[0]), FCString::Atoi(*A[1])); } }
		}));
}
