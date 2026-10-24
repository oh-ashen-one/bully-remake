// OWNER: sm-playloop
#include "BullyStorySubsystem.h"

#include "BullyCampaignSubsystem.h"
#include "BullyCharacter.h"
#include "BullyClockSubsystem.h"
#include "BullyHUD.h"
#include "BullyPlayerController.h"
#include "BullyProgressSubsystem.h"
#include "BullySaveGame.h"
#include "BullyWeaponComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
#include "Misc/DateTime.h"
#include "Misc/Parse.h"
#include "MissionStar.h"
#include "MissionStarSubsystem.h"
#include "MissionSubsystem.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"

DEFINE_LOG_CATEGORY_STATIC(LogBullyStory, Log, All);

namespace
{
	const FLinearColor Gold(0.95f, 0.72f, 0.20f, 1.f);
	const FLinearColor Cream(0.95f, 0.93f, 0.88f, 1.f);
	const FLinearColor Dim(0.65f, 0.65f, 0.68f, 1.f);

	// -BRStorySmoke expectations: the "new" run mutates these onto the state and writes them with Save & Quit's code
	// path; the "continue" run must read them back out of the slot. (Fixed values so each run is self-checking.)
	const int32 SmokeMoneyAdd = 13;
	const FName SmokeItem(TEXT("smoke_chocolate"));
	const int32 SmokeItemCount = 2;
	const int32 SmokeClockHour = 11;
	const int32 SmokeClockMinute = 15;
	const FName SmokeFaction(TEXT("nerds"));
	const int32 SmokeFactionRespect = 33;
	const FName SmokeAmmoWeapon(TEXT("slingshot"));
	const int32 SmokeAmmo = 17;
	const FName SmokeCollectible(TEXT("smoke_collectible"));
	const FName SmokeFlag(TEXT("world_smoke_flag"));

	APawn* PlayerPawn(UWorld* W) { return W ? UGameplayStatics::GetPlayerPawn(W, 0) : nullptr; }
	UBullyWeaponComponent* PlayerWeapons(UWorld* W)
	{
		ABullyCharacter* P = Cast<ABullyCharacter>(PlayerPawn(W));
		return P ? P->GetWeapons() : nullptr;
	}
}

// ---------------------------------------------------------------------------------------------------------------
// title screen widget (pure Slate, same approach as the Esc menu / HUD)

class SBullyTitleScreen : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBullyTitleScreen) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UBullyStorySubsystem>, Story)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		Story = InArgs._Story;
		const FSlateFontInfo Huge = FCoreStyle::GetDefaultFontStyle("Bold", 54);
		const FSlateFontInfo Sub = FCoreStyle::GetDefaultFontStyle("Regular", 16);
		const FSlateFontInfo Item = FCoreStyle::GetDefaultFontStyle("Bold", 26);
		const FSlateFontInfo Small = FCoreStyle::GetDefaultFontStyle("Regular", 13);
		const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");

		TSharedRef<SVerticalBox> Menu = SNew(SVerticalBox);
		// up to three rows (Continue / New Story / Quit): each row reads its label + highlight from the subsystem
		for (int32 i = 0; i < 3; ++i)
		{
			Menu->AddSlot().AutoHeight().Padding(0, 6, 0, 0)
			[
				SNew(SBox).MinDesiredWidth(360.f)
				.Visibility_Lambda([this, i]() { return Story.IsValid() && Story->GetTitleItems().IsValidIndex(i) ? EVisibility::Visible : EVisibility::Collapsed; })
				[
					SNew(SBorder).BorderImage(White).Padding(FMargin(26.f, 9.f, 26.f, 11.f))
					.BorderBackgroundColor_Lambda([this, i]()
					{
						const bool bSel = Story.IsValid() && Story->GetTitleSelected() == i;
						return bSel ? FLinearColor(0.95f, 0.72f, 0.20f, 0.22f) : FLinearColor(0.f, 0.f, 0.f, 0.f);
					})
					.OnMouseMove_Lambda([this, i](const FGeometry&, const FPointerEvent&)
					{
						if (Story.IsValid() && Story->GetTitleSelected() != i) { Story->TitleSelect(i); }
						return FReply::Unhandled();
					})
					.OnMouseButtonDown_Lambda([this, i](const FGeometry&, const FPointerEvent& E)
					{
						if (E.GetEffectingButton() == EKeys::LeftMouseButton && Story.IsValid())
						{
							Story->TitleSelect(i);
							Story->TitleActivate();
							return FReply::Handled();
						}
						return FReply::Unhandled();
					})
					[
						SNew(STextBlock).Font(Item)
						.ColorAndOpacity_Lambda([this, i]() { return Story.IsValid() && Story->GetTitleSelected() == i ? Gold : Cream; })
						.Text_Lambda([this, i]()
						{
							if (!Story.IsValid() || !Story->GetTitleItems().IsValidIndex(i)) { return FText::GetEmpty(); }
							const bool bSel = Story->GetTitleSelected() == i;
							return FText::FromString((bSel ? TEXT("> ") : TEXT("   ")) + Story->GetTitleItems()[i].Label);
						})
					]
				]
			];
		}

		ChildSlot
		[
			SNew(SBorder).BorderImage(White).BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.58f))
			.HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(STextBlock).Font(Huge).ColorAndOpacity(Gold)
					.ShadowOffset(FVector2D(3, 3)).ShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.85f))
					.Text(FText::FromString(TEXT("BULLY REMAKE")))
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 4, 0, 34)
				[
					SNew(STextBlock).Font(Sub).ColorAndOpacity(Dim)
					.Text(FText::FromString(TEXT("Story Mode   -   unofficial fan project")))
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					Menu
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 18, 0, 0)
				[
					SNew(STextBlock).Font(Small).ColorAndOpacity(Gold)
					.Text_Lambda([this]() { return FText::FromString(Story.IsValid() ? Story->GetTitleDetail() : FString()); })
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 30, 0, 0)
				[
					SNew(STextBlock).Font(Small).ColorAndOpacity(Dim)
					.Text(FText::FromString(TEXT("W / S, arrows or D-pad: choose        Enter, Space or A: confirm")))
				]
			]
		];
	}

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& Geo, const FKeyEvent& Event) override
	{
		if (!Story.IsValid()) { return FReply::Unhandled(); }
		const FKey K = Event.GetKey();
		if (K == EKeys::Up || K == EKeys::W || K == EKeys::Gamepad_DPad_Up || K == EKeys::Gamepad_LeftStick_Up) { Story->TitleMove(-1); return FReply::Handled(); }
		if (K == EKeys::Down || K == EKeys::S || K == EKeys::Gamepad_DPad_Down || K == EKeys::Gamepad_LeftStick_Down) { Story->TitleMove(+1); return FReply::Handled(); }
		if (K == EKeys::Enter || K == EKeys::SpaceBar || K == EKeys::Gamepad_FaceButton_Bottom || K == EKeys::Gamepad_Special_Right)
		{
			Story->TitleActivate();
			return FReply::Handled();
		}
		if (K == EKeys::Escape) { return FReply::Handled(); }   // the title is not the pause menu
		return SCompoundWidget::OnKeyDown(Geo, Event);
	}

private:
	TWeakObjectPtr<UBullyStorySubsystem> Story;
};

// ---------------------------------------------------------------------------------------------------------------
// lifecycle

UBullyStorySubsystem* UBullyStorySubsystem::Get(UWorld* World)
{
	return World ? World->GetSubsystem<UBullyStorySubsystem>() : nullptr;
}

bool UBullyStorySubsystem::IsStoryRunRequested()
{
	const TCHAR* CL = FCommandLine::Get();
	FString V;
	const bool bFlag = FParse::Param(CL, TEXT("BRStory")) || FParse::Param(CL, TEXT("BRStorySmoke")) || FParse::Value(CL, TEXT("-BRStorySmoke="), V);
	if (!bFlag) { return false; }
	// the mission harnesses always win: they must behave exactly as before
	return !(FParse::Value(CL, TEXT("-BRMission="), V) || FParse::Param(CL, TEXT("BRMissionSmoke")) || FParse::Value(CL, TEXT("-BRMissionSmoke="), V)
		|| FParse::Param(CL, TEXT("BRCampaignSmoke")) || FParse::Value(CL, TEXT("-BRMissionShots="), V));
}

bool UBullyStorySubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer)) { return false; }
	const UWorld* W = Cast<UWorld>(Outer);
	return W && W->IsGameWorld() && W->GetNetMode() != NM_DedicatedServer && !FParse::Param(FCommandLine::Get(), TEXT("BRNoMission"));
}

TStatId UBullyStorySubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBullyStorySubsystem, STATGROUP_Tickables);
}

FString UBullyStorySubsystem::SlotName() const
{
	return Slot.IsEmpty() ? FString(UBullySaveGame::DefaultSlot()) : Slot;
}

void UBullyStorySubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	const TCHAR* CL = FCommandLine::Get();
	FString SmokeVal;
	const bool bAskedStory = FParse::Param(CL, TEXT("BRStory")) || FParse::Param(CL, TEXT("BRStorySmoke")) || FParse::Value(CL, TEXT("-BRStorySmoke="), SmokeVal);
	bStory = IsStoryRunRequested();
	if (bAskedStory && !bStory)
	{
		UE_LOG(LogBullyStory, Display, TEXT("BR_STORY story flag ignored: a mission harness flag is on the command line (harness runs stay as before)"));
	}
	if (!bStory) { return; }

	const bool bSmoke = FParse::Param(CL, TEXT("BRStorySmoke")) || !SmokeVal.IsEmpty();
	FString SlotVal;
	if (FParse::Value(CL, TEXT("-BRStorySlot="), SlotVal) && !SlotVal.IsEmpty()) { Slot = SlotVal; }
	else if (bSmoke) { Slot = TEXT("BullyStorySmoke"); }
	if (bSmoke)
	{
		Smoke.Mode = SmokeVal.Equals(TEXT("continue"), ESearchCase::IgnoreCase) ? FSmoke::EMode::Continue
			: SmokeVal.Equals(TEXT("shots"), ESearchCase::IgnoreCase) ? FSmoke::EMode::Shots : FSmoke::EMode::New;
		if (Smoke.Mode == FSmoke::EMode::Shots)
		{
			if (!FParse::Value(CL, TEXT("-BRShotDir="), Smoke.ShotDir) || Smoke.ShotDir.IsEmpty()) { Smoke.ShotDir = FPaths::ProjectSavedDir() / TEXT("StoryShots"); }
			IFileManager::Get().MakeDirectory(*Smoke.ShotDir, true);
		}
		if (Smoke.Mode == FSmoke::EMode::New)
		{
			UGameplayStatics::DeleteGameInSlot(SlotName(), 0);   // a clean New Story: no Continue offered
		}
	}
	if (UBullyCampaignSubsystem* Camp = UBullyCampaignSubsystem::Get(&InWorld)) { LastChapterSeen = Camp->GetChapter(); }
	if (UMissionSubsystem* M = UMissionSubsystem::Get(&InWorld))
	{
		M->OnMissionFinished.AddUObject(this, &UBullyStorySubsystem::OnMissionDone);
		M->OnStageBegan.AddUObject(this, &UBullyStorySubsystem::OnStageBegan);
		M->OnMissionEvent.AddLambda([this](FName Event)
		{
			const FString E = Event.ToString();
			if (E.StartsWith(TEXT("world_")) && !StoryFlags.Contains(Event))
			{
				StoryFlags.Add(Event);
				UE_LOG(LogBullyStory, Display, TEXT("BR_STORY story flag '%s'"), *E);
			}
		});
	}
	if (UBullyCampaignSubsystem* Camp = UBullyCampaignSubsystem::Get(&InWorld))
	{
		Camp->OnChanged.AddLambda([this]()
		{
			UBullyCampaignSubsystem* C = UBullyCampaignSubsystem::Get(GetWorld());
			if (C && C->GetChapter() != LastChapterSeen)
			{
				LastChapterSeen = C->GetChapter();
				RequestAutosave(FString::Printf(TEXT("autosave:chapter %d"), LastChapterSeen));
			}
		});
	}
	bSubscribed = true;
	UE_LOG(LogBullyStory, Display, TEXT("BR_STORY armed: slot '%s', save %s%s"), *SlotName(), SaveExists() ? TEXT("present") : TEXT("absent"),
		Smoke.Mode == FSmoke::EMode::Off ? TEXT("") : (Smoke.Mode == FSmoke::EMode::New ? TEXT(", smoke=new") : Smoke.Mode == FSmoke::EMode::Shots ? TEXT(", smoke=shots") : TEXT(", smoke=continue")));
}

void UBullyStorySubsystem::Deinitialize()
{
	CloseTitle();
	if (bSubscribed)
	{
		if (UMissionSubsystem* M = UMissionSubsystem::Get(GetWorld()))
		{
			M->OnMissionFinished.RemoveAll(this);
			M->OnStageBegan.RemoveAll(this);
			M->bKeepHud = false;
		}
	}
	Super::Deinitialize();
}

// ---------------------------------------------------------------------------------------------------------------
// title screen

void UBullyStorySubsystem::OpenTitle()
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (IsTitleOpen() || !PC) { return; }

	TitleItems.Reset();
	if (SaveExists()) { TitleItems.Add({ FTitleItem::EKind::Continue, TEXT("Continue") }); }
	TitleItems.Add({ FTitleItem::EKind::NewStory, TEXT("New Story") });
	TitleItems.Add({ FTitleItem::EKind::Quit, TEXT("Quit") });
	TitleSelected = 0;

	if (GEngine && GEngine->GameViewport && FSlateApplication::IsInitialized())
	{
		TSharedRef<SBullyTitleScreen> W = SNew(SBullyTitleScreen).Story(this);
		TitleWidget = W;
		GEngine->GameViewport->AddViewportWidgetContent(W, 150);   // above the HUD (50) and the Esc menu (100)
		bTitleOnViewport = true;
		PC->FlushPressedKeys();
		PC->SetIgnoreLookInput(true);
		PC->SetIgnoreMoveInput(true);
		PC->SetPause(true);
		FInputModeGameAndUI Mode;
		Mode.SetWidgetToFocus(W);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(Mode);
		PC->SetShowMouseCursor(true);
	}
	else
	{
		UE_LOG(LogBullyStory, Warning, TEXT("BR_STORY title: no game viewport / Slate; menu is logic-only"));
		TitleWidget = SNullWidget::NullWidget;   // keeps IsTitleOpen() true so the flow below still runs
		bTitleOnViewport = false;
		PC->SetPause(true);
	}
	FString Items;
	for (const FTitleItem& I : TitleItems) { Items += (Items.IsEmpty() ? TEXT("") : TEXT(" | ")) + I.Label; }
	UE_LOG(LogBullyStory, Display, TEXT("BR_STORY title open: [%s] selected '%s' paused=%d"), *Items, *TitleItems[0].Label, PC->IsPaused() ? 1 : 0);
}

void UBullyStorySubsystem::CloseTitle()
{
	if (!TitleWidget.IsValid()) { return; }
	if (GEngine && GEngine->GameViewport && bTitleOnViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(TitleWidget.ToSharedRef());
	}
	TitleWidget.Reset();
	bTitleOnViewport = false;
	UWorld* World = GetWorld();
	if (APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr)
	{
		PC->SetPause(false);
		PC->SetIgnoreLookInput(false);
		PC->SetIgnoreMoveInput(false);
		PC->FlushPressedKeys();
		PC->SetShowMouseCursor(false);
		if (ABullyPlayerController* B = Cast<ABullyPlayerController>(PC)) { B->RestoreGameInput(); }
	}
}

void UBullyStorySubsystem::TitleMove(int32 Dir)
{
	if (TitleItems.Num() == 0) { return; }
	TitleSelected = (TitleSelected + Dir + TitleItems.Num()) % TitleItems.Num();
	UE_LOG(LogBullyStory, Display, TEXT("BR_STORY title select '%s'"), *TitleItems[TitleSelected].Label);
}

void UBullyStorySubsystem::TitleSelect(int32 Index)
{
	if (TitleItems.IsValidIndex(Index)) { TitleSelected = Index; }
}

FString UBullyStorySubsystem::GetTitleDetail() const
{
	if (TitleItems.IsValidIndex(TitleSelected) && TitleItems[TitleSelected].Kind == FTitleItem::EKind::Continue)
	{
		if (const UBullySaveGame* S = LoadSave()) { return S->Summary(); }
	}
	return FString();
}

void UBullyStorySubsystem::TitleActivate()
{
	if (!IsTitleOpen() || !TitleItems.IsValidIndex(TitleSelected)) { return; }
	const FTitleItem::EKind Kind = TitleItems[TitleSelected].Kind;
	UE_LOG(LogBullyStory, Display, TEXT("BR_STORY title activate '%s'"), *TitleItems[TitleSelected].Label);
	CloseTitle();
	bTitleDone = true;
	switch (Kind)
	{
	case FTitleItem::EKind::Continue:
		if (!ContinueStory()) { UE_LOG(LogBullyStory, Warning, TEXT("BR_STORY continue failed; starting a new story")); BeginNewStory(); }
		break;
	case FTitleItem::EKind::NewStory: BeginNewStory(); break;
	case FTitleItem::EKind::Quit: QuitGame(); break;
	}
}

void UBullyStorySubsystem::QuitGame()
{
	if (UWorld* World = GetWorld()) { GEngine->Exec(World, TEXT("quit")); }
}

// ---------------------------------------------------------------------------------------------------------------
// flow

void UBullyStorySubsystem::BeginNewStory()
{
	UMissionSubsystem* M = UMissionSubsystem::Get(GetWorld());
	if (!M) { UE_LOG(LogBullyStory, Error, TEXT("BR_STORY new story: no mission subsystem")); return; }
	LastSafe = nullptr;
	StoryFlags.Reset();
	// arrival (the gates) chains into day_one (the first morning), which hands off to campaign free roam. The boot
	// pair is not a table row, so the start needs no dev override, but pass it so a future table row cannot block it.
	UE_LOG(LogBullyStory, Display, TEXT("BR_STORY new story: starting arrival -> day_one"));
	M->StartMission(TEXT("arrival"), 0, /*bDevOverride*/ true);
}

bool UBullyStorySubsystem::ContinueStory()
{
	UBullySaveGame* Save = LoadSave();
	if (!Save) { UE_LOG(LogBullyStory, Warning, TEXT("BR_STORY continue: no readable save in slot '%s'"), *SlotName()); return false; }
	if (!Apply(Save)) { return false; }
	LastSafe = Save;
	LastPassed = Save->LastMission;
	UE_LOG(LogBullyStory, Display, TEXT("BR_STORY continue: restored %s (saved %s, %s)"), *Save->Summary(), *Save->SavedAtUtc, *Save->Reason);
	return true;
}

// ---------------------------------------------------------------------------------------------------------------
// save / load

bool UBullyStorySubsystem::SaveExists() const
{
	return UGameplayStatics::DoesSaveGameExist(SlotName(), 0);
}

UBullySaveGame* UBullyStorySubsystem::LoadSave() const
{
	if (!SaveExists()) { return nullptr; }
	USaveGame* Raw = UGameplayStatics::LoadGameFromSlot(SlotName(), 0);
	UBullySaveGame* S = Cast<UBullySaveGame>(Raw);
	if (!S) { UE_LOG(LogBullyStory, Warning, TEXT("BR_STORY save slot '%s' unreadable"), *SlotName()); return nullptr; }
	if (S->Version > UBullySaveGame::CurrentVersion)
	{
		UE_LOG(LogBullyStory, Warning, TEXT("BR_STORY save slot '%s' is version %d (this build reads <= %d)"), *SlotName(), S->Version, UBullySaveGame::CurrentVersion);
		return nullptr;
	}
	return S;
}

UBullySaveGame* UBullyStorySubsystem::Capture(const FString& Reason, bool bWithPlayer) const
{
	UWorld* World = GetWorld();
	UBullySaveGame* S = NewObject<UBullySaveGame>(GetTransientPackage());
	S->SavedAtUtc = FDateTime::UtcNow().ToIso8601();
	S->Reason = Reason;
	S->LastMission = LastPassed;
	if (UBullyCampaignSubsystem* C = UBullyCampaignSubsystem::Get(World))
	{
		S->Chapter = C->Chapter;
		S->Completion = C->Completion;
		S->Attempts = C->Attempts;
		S->Respect = C->Respect;
		S->Collectibles = C->Collectibles.Array();
		S->GrantedWeapons = C->GrantedWeapons.Array();
	}
	S->StoryFlags = StoryFlags.Array();
	if (UBullyClockSubsystem* K = UBullyClockSubsystem::Get(World))
	{
		S->ClockDay = K->Day; S->ClockHour = K->Hour; S->ClockMinute = K->Minute; S->bClockPaused = K->bPaused;
	}
	if (UBullyProgressSubsystem* P = UBullyProgressSubsystem::Get(World))
	{
		S->Money = P->Money; S->WalletRespect = P->Respect; S->Items = P->Items;
	}
	if (UBullyWeaponComponent* W = PlayerWeapons(World)) { S->WeaponAmmo = W->Ammo; }
	if (bWithPlayer)
	{
		if (const APawn* Pawn = PlayerPawn(World))
		{
			S->bHasRespawn = true;
			S->RespawnLocation = Pawn->GetActorLocation();
			S->RespawnYaw = Pawn->GetActorRotation().Yaw;
		}
	}
	else if (LastSafe)
	{
		S->bHasRespawn = LastSafe->bHasRespawn; S->RespawnLocation = LastSafe->RespawnLocation; S->RespawnYaw = LastSafe->RespawnYaw;
	}
	return S;
}

bool UBullyStorySubsystem::WriteSlot(UBullySaveGame* Save)
{
	const bool bOk = UGameplayStatics::SaveGameToSlot(Save, SlotName(), 0);
	UE_LOG(LogBullyStory, Display, TEXT("BR_STORY save %s: slot '%s' (%s) -> %s"), bOk ? TEXT("written") : TEXT("FAILED"), *SlotName(), *Save->Reason, *Save->Summary());
	return bOk;
}

bool UBullyStorySubsystem::SaveNow(const FString& Reason)
{
	UBullySaveGame* S = Capture(Reason, /*bWithPlayer*/ true);
	if (!WriteSlot(S)) { return false; }
	LastSafe = S;
	return true;
}

bool UBullyStorySubsystem::SaveAndQuit(bool bQuit)
{
	UMissionSubsystem* M = UMissionSubsystem::Get(GetWorld());
	bool bSaved = false;
	if (M && (M->IsActive() || M->HasPendingStart()))
	{
		// never write mid-mission state: the last autosave (a free-roam snapshot) stays the save; the mission restarts on Continue
		UE_LOG(LogBullyStory, Display, TEXT("BR_STORY save&quit mid-mission: keeping the last autosave (%s)"), LastSafe ? *LastSafe->Summary() : TEXT("none yet"));
		bSaved = LastSafe != nullptr;
	}
	else
	{
		bSaved = SaveNow(TEXT("save_and_quit"));
	}
	if (bQuit) { QuitGame(); }
	return bSaved;
}

bool UBullyStorySubsystem::Apply(const UBullySaveGame* Save)
{
	UWorld* World = GetWorld();
	UBullyCampaignSubsystem* C = UBullyCampaignSubsystem::Get(World);
	UBullyClockSubsystem* K = UBullyClockSubsystem::Get(World);
	UBullyProgressSubsystem* P = UBullyProgressSubsystem::Get(World);
	if (!C || !K || !P || !Save) { return false; }

	C->Chapter = Save->Chapter;
	C->Completion = Save->Completion;
	C->Attempts = Save->Attempts;
	for (const TPair<FName, int32>& R : Save->Respect) { C->Respect.FindOrAdd(R.Key) = R.Value; }
	C->Collectibles = TSet<FName>(Save->Collectibles);
	C->GrantedWeapons = TSet<FName>(Save->GrantedWeapons);
	StoryFlags = TSet<FName>(Save->StoryFlags);
	LastChapterSeen = C->Chapter;

	K->Day = Save->ClockDay; K->Hour = FMath::Clamp(Save->ClockHour, 0, 23); K->Minute = FMath::Clamp(Save->ClockMinute, 0, 59);
	K->MinuteCarry = 0.f;
	K->bPaused = Save->bClockPaused;

	P->Money = Save->Money; P->Respect = Save->WalletRespect; P->Items = Save->Items;

	if (UBullyWeaponComponent* W = PlayerWeapons(World))
	{
		for (const TPair<FName, int32>& A : Save->WeaponAmmo) { W->Ammo.Add(A.Key, A.Value); }
	}

	if (Save->bHasRespawn)
	{
		if (APawn* Pawn = PlayerPawn(World))
		{
			Pawn->SetActorLocation(Save->RespawnLocation, false, nullptr, ETeleportType::TeleportPhysics);
			if (ACharacter* Ch = Cast<ACharacter>(Pawn)) { if (Ch->GetCharacterMovement()) { Ch->GetCharacterMovement()->StopMovementImmediately(); } }
			if (AController* Ctl = Pawn->GetController()) { Ctl->SetControlRotation(FRotator(0.f, Save->RespawnYaw, 0.f)); }
		}
	}

	// refresh everything that listens: stars re-spawn from the restored table, the wallet readout updates
	C->OnChanged.Broadcast();
	P->OnChanged.Broadcast();
	K->OnHourChanged.Broadcast(K->Hour);
	if (UMissionStarSubsystem* Stars = UMissionStarSubsystem::Get(World)) { Stars->Refresh(); }
	return true;
}

void UBullyStorySubsystem::RequestAutosave(const FString& Reason)
{
	bAutosaveDirty = true;
	AutosaveReason = Reason;
}

void UBullyStorySubsystem::TickAutosave()
{
	if (!bAutosaveDirty) { return; }
	UMissionSubsystem* M = UMissionSubsystem::Get(GetWorld());
	if (!M || M->IsActive() || M->HasPendingStart() || IsTitleOpen() || !PlayerPawn(GetWorld())) { return; }
	bAutosaveDirty = false;
	if (SaveNow(AutosaveReason))
	{
		++AutosaveCount;
		AutosaveToastAt = GetWorld()->GetRealTimeSeconds();
		if (FBullyHUD* Hud = M->AcquireHud()) { Hud->ShowHint(TEXT("Progress saved."), 3.f); }
	}
}

// ---------------------------------------------------------------------------------------------------------------
// mission hooks

void UBullyStorySubsystem::OnStageBegan(const FMissionDef& M, int32 Index)
{
	if (Index != 0 || M.Id == SnapMissionId) { return; }
	SnapMissionId = M.Id;
	UWorld* World = GetWorld();
	if (UBullyProgressSubsystem* P = UBullyProgressSubsystem::Get(World)) { SnapMoney = P->GetMoney(); SnapWalletRespect = P->GetRespect(); }
	if (UBullyCampaignSubsystem* C = UBullyCampaignSubsystem::Get(World)) { SnapRespect = C->Respect; SnapGranted = C->GetGrantedWeapons(); }
}

void UBullyStorySubsystem::OnMissionDone(const FMissionDef& M, bool bSuccess)
{
	if (!bSuccess) { return; }
	UWorld* World = GetWorld();
	UMissionSubsystem* Mission = UMissionSubsystem::Get(World);
	// reward line for the pass banner (money / respect / weapon unlocks accumulated since the mission began)
	TArray<FString> Parts;
	if (SnapMissionId == M.Id)
	{
		if (UBullyProgressSubsystem* P = UBullyProgressSubsystem::Get(World))
		{
			if (P->GetMoney() > SnapMoney) { Parts.Add(FString::Printf(TEXT("$%d"), P->GetMoney() - SnapMoney)); }
			if (P->GetRespect() > SnapWalletRespect) { Parts.Add(FString::Printf(TEXT("%d respect"), P->GetRespect() - SnapWalletRespect)); }
		}
		if (UBullyCampaignSubsystem* C = UBullyCampaignSubsystem::Get(World))
		{
			for (const TPair<FName, int32>& R : C->Respect)
			{
				const int32 Was = SnapRespect.FindRef(R.Key);
				if (R.Value != Was) { Parts.Add(FString::Printf(TEXT("%s respect %s%d"), *R.Key.ToString(), R.Value > Was ? TEXT("+") : TEXT(""), R.Value - Was)); }
			}
			for (const FName& G : C->GetGrantedWeapons())
			{
				if (!SnapGranted.Contains(G)) { Parts.Add(FString::Printf(TEXT("unlocked %s"), *G.ToString())); }
			}
		}
	}
	SnapMissionId.Reset();
	LastPassed = M.Id;
	RequestAutosave(FString::Printf(TEXT("autosave:%s"), *M.Id));
	// a mission that chains straight into another (arrival -> day_one, 1_02B -> 1_02C) is one beat of a longer sequence
	if (Mission && !Mission->HasPendingStart())
	{
		if (FBullyHUD* Hud = Mission->GetHUD())
		{
			const FString Sub = Parts.Num() ? FString::Printf(TEXT("%s   -   %s"), *M.Title, *FString::Join(Parts, TEXT(", "))) : M.Title;
			Hud->ShowBanner(TEXT("MISSION PASSED"), Sub, 5.f, false);
			UE_LOG(LogBullyStory, Display, TEXT("BR_STORY mission passed '%s': %s"), *M.Id, *Sub);
			if (UBullyProgressSubsystem* P = UBullyProgressSubsystem::Get(World)) { Hud->SetWallet(P->GetMoney(), P->GetRespect(), true); }
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------
// free-roam guidance

bool UBullyStorySubsystem::PickGuidance(FString& OutText, FVector& OutLoc, bool& bOutBlip) const
{
	UWorld* World = GetWorld();
	UBullyCampaignSubsystem* Camp = UBullyCampaignSubsystem::Get(World);
	UMissionStarSubsystem* Stars = UMissionStarSubsystem::Get(World);
	const APawn* Pawn = PlayerPawn(World);
	if (!Camp || !Camp->IsLoaded() || !Pawn) { return false; }
	const FVector Me = Pawn->GetActorLocation();

	// nearest spawned gold star (story mission)
	const FCampaignMission* Best = nullptr;
	FVector BestLoc = FVector::ZeroVector;
	double BestD = TNumericLimits<double>::Max();
	for (const TPair<FString, FCampaignMission>& P : Camp->GetMissions())
	{
		const FCampaignMission& Row = P.Value;
		if (!Row.bHasStarLocation || Row.Star == FName(TEXT("red"))) { continue; }
		const AMissionStar* Star = Stars ? Stars->FindStar(FName(*Row.Id)) : nullptr;
		if (!Star) { continue; }
		const double D = FVector::DistSquared2D(Me, Star->GetActorLocation());
		if (D < BestD) { BestD = D; Best = &Row; BestLoc = Star->GetActorLocation(); }
	}
	if (Best)
	{
		OutText = FString::Printf(TEXT("Go to the yellow star: %s"), *Best->Title);
		OutLoc = BestLoc;
		bOutBlip = true;
		return true;
	}

	// nothing up right now: say why (a story mission waiting on the clock) or that the chapter's missions are done
	const UBullyClockSubsystem* Clock = UBullyClockSubsystem::Get(World);
	for (const TPair<FString, FCampaignMission>& P : Camp->GetMissions())
	{
		const FCampaignMission& Row = P.Value;
		if (!Row.bHasStarLocation || Row.Star == FName(TEXT("red")) || Camp->IsCompleted(Row.Id)) { continue; }
		if (Row.Chapter > 0 && Row.Chapter != Camp->GetChapter()) { continue; }
		bool bPrereq = true;
		for (const FString& Pre : Row.Prereq) { if (!Camp->IsCompleted(Pre)) { bPrereq = false; break; } }
		if (bPrereq && Row.bHasWindow && Clock && !Clock->IsInWindow(Row.WindowStart, Row.WindowEnd))
		{
			OutText = FString::Printf(TEXT("%s starts between %d:00 and %d:00 - explore Bullworth until then"), *Row.Title, FMath::FloorToInt(Row.WindowStart), FMath::FloorToInt(Row.WindowEnd));
			bOutBlip = false;
			return true;
		}
	}
	OutText = TEXT("No story mission available right now - explore Bullworth");
	bOutBlip = false;
	return true;
}

void UBullyStorySubsystem::TickGuidance(float Dt)
{
	UWorld* World = GetWorld();
	UMissionSubsystem* M = UMissionSubsystem::Get(World);
	if (!M) { return; }
	if (M->IsActive() || M->HasPendingStart())
	{
		if (bGuiding) { bGuiding = false; M->bKeepHud = false; GuidanceText.Reset(); }
		return;
	}
	FBullyHUD* Hud = M->GetHUD();
	if (M->GetState() == UMissionSubsystem::EState::Complete && Hud && Hud->IsShown() && Hud->GetState().BannerLeft > 0.f) { return; }   // let MISSION PASSED play
	if (!PlayerPawn(World)) { return; }
	if (!Hud || !Hud->IsShown()) { Hud = M->AcquireHud(); }
	if (!Hud) { return; }
	M->bKeepHud = true;
	bGuiding = true;

	if (World->GetRealTimeSeconds() >= NextGuidanceAt)
	{
		NextGuidanceAt = World->GetRealTimeSeconds() + 0.4;
		bGuidanceBlip = false;
		if (!PickGuidance(GuidanceText, GuidanceLoc, bGuidanceBlip)) { GuidanceText.Reset(); }
	}
	if (UBullyProgressSubsystem* P = UBullyProgressSubsystem::Get(World)) { Hud->SetWallet(P->GetMoney(), P->GetRespect(), false); }
	if (GuidanceText.IsEmpty()) { return; }
	const UBullyCampaignSubsystem* Camp = UBullyCampaignSubsystem::Get(World);
	const FString Header = FString::Printf(TEXT("Chapter %d"), Camp ? Camp->GetChapter() : 1);
	const FBullyHUDState& S = Hud->GetState();
	if (S.ObjectiveText != GuidanceText || S.MissionTitle != Header || S.StageCount != 0) { Hud->SetObjective(Header, 0, 0, GuidanceText); }
	Hud->SetBlip(bGuidanceBlip, GuidanceLoc);
}

// ---------------------------------------------------------------------------------------------------------------
// tick

void UBullyStorySubsystem::Tick(float DeltaTime)
{
	if (!bStory) { return; }
	UWorld* World = GetWorld();
	if (!World) { return; }
	Elapsed += DeltaTime;

	if (!bTitleDone && !IsTitleOpen())
	{
		if (Elapsed > (Smoke.Mode != FSmoke::EMode::Off ? 2.0 : 0.8) && PlayerPawn(World) && World->GetFirstPlayerController())
		{
			OpenTitle();
		}
	}
	if (Smoke.Mode != FSmoke::EMode::Off) { TickSmoke(DeltaTime); }
	if (!bTitleDone || IsTitleOpen()) { return; }

	TickAutosave();
	TickGuidance(DeltaTime);
}

// ---------------------------------------------------------------------------------------------------------------
// -BRStorySmoke

void UBullyStorySubsystem::SmokeCheck(const TCHAR* Name, bool bOk, const FString& Detail)
{
	++(bOk ? Smoke.Passes : Smoke.Fails);
	UE_LOG(LogBullyStory, Display, TEXT("BR_STORY %s %s (%s)"), Name, bOk ? TEXT("PASS") : TEXT("FAIL"), *Detail);
}

void UBullyStorySubsystem::SmokeDone()
{
	Smoke.bDone = true;
	const bool bAll = Smoke.Fails == 0;
	UE_LOG(LogBullyStory, Display, TEXT("BR_STORY SMOKE %s %s %d/%d (t=%.1fs)"), Smoke.Mode == FSmoke::EMode::New ? TEXT("new") : Smoke.Mode == FSmoke::EMode::Shots ? TEXT("shots") : TEXT("continue"),
		bAll ? TEXT("PASS") : TEXT("FAIL"), Smoke.Passes, Smoke.Passes + Smoke.Fails, Smoke.Elapsed);
	QuitGame();
}

void UBullyStorySubsystem::TickSmoke(float Dt)
{
	if (Smoke.bDone) { return; }
	Smoke.Elapsed += Dt;
	UWorld* World = GetWorld();
	UMissionSubsystem* M = UMissionSubsystem::Get(World);
	UBullyCampaignSubsystem* Camp = UBullyCampaignSubsystem::Get(World);
	UBullyClockSubsystem* Clock = UBullyClockSubsystem::Get(World);
	UBullyProgressSubsystem* Prog = UBullyProgressSubsystem::Get(World);
	UMissionStarSubsystem* Stars = UMissionStarSubsystem::Get(World);
	if (!World || !M || !Camp || !Clock || !Prog || !Stars) { return; }
	if (Smoke.Elapsed > (Smoke.Mode == FSmoke::EMode::Shots ? 900.0 : 400.0)) { SmokeCheck(TEXT("timeout"), false, TEXT("overall time limit")); SmokeDone(); return; }
	const double InPhase = Smoke.Elapsed - Smoke.PhaseAt;
	auto Next = [&]() { ++Smoke.Phase; Smoke.PhaseAt = Smoke.Elapsed; };
	APlayerController* PC = World->GetFirstPlayerController();

	// the free-roam HUD state the player would read
	auto HudOk = [&](FString& Why) -> bool
	{
		FBullyHUD* Hud = M->GetHUD();
		if (!Hud || !Hud->IsShown()) { Why = TEXT("HUD not shown"); return false; }
		const FBullyHUDState& S = Hud->GetState();
		const AMissionStar* Star = Stars->FindStar(TEXT("1_02B"));
		const bool bText = S.ObjectiveText.Contains(TEXT("yellow star")) && S.ObjectiveText.Contains(TEXT("This Is Your School"));
		const bool bBlip = S.bBlip && Star && FVector::Dist(S.BlipWorld, Star->GetActorLocation()) < 1.f;
		Why = FString::Printf(TEXT("objective '%s' (header '%s'), blip=%d at %s"), *S.ObjectiveText, *S.MissionTitle, S.bBlip ? 1 : 0, *S.BlipWorld.ToCompactString());
		return bText && bBlip;
	};

	if (Smoke.Mode == FSmoke::EMode::New)
	{
		switch (Smoke.Phase)
		{
		case 0:   // title screen up (paused), no Continue (clean slot)
			if (IsTitleOpen())
			{
				const bool bShape = TitleItems.Num() == 2 && TitleItems[0].Kind == FTitleItem::EKind::NewStory && TitleSelected == 0;
				SmokeCheck(TEXT("1_title_new_only"), bShape && PC && PC->IsPaused() && !SaveExists(),
					FString::Printf(TEXT("%d items, first '%s', paused=%d, save present=%d (want no Continue)"), TitleItems.Num(), TitleItems.Num() ? *TitleItems[0].Label : TEXT("-"), PC && PC->IsPaused() ? 1 : 0, SaveExists() ? 1 : 0));
				// drive the same path a key press takes: move down and back up (wraps), then confirm
				TitleMove(+1); TitleMove(-1);
				TitleActivate();
				Next();
			}
			else if (InPhase > 20.0) { SmokeCheck(TEXT("1_title_new_only"), false, TEXT("title never opened")); SmokeDone(); }
			return;
		case 1:   // New Story: arrival must be running, game unpaused; hand the mission to the smoke driver
			if (M->GetState() == UMissionSubsystem::EState::Running && M->GetMission())
			{
				SmokeCheck(TEXT("2_new_story_boot"), M->GetMission()->Id == TEXT("arrival") && PC && !PC->IsPaused() && !IsTitleOpen(),
					FString::Printf(TEXT("mission '%s' running, paused=%d, title open=%d"), *M->GetMission()->Id, PC && PC->IsPaused() ? 1 : 0, IsTitleOpen() ? 1 : 0));
				M->DriveSmoke(TEXT(""), /*bQuitOnDone*/ false);
				Next();
			}
			else if (InPhase > 15.0) { SmokeCheck(TEXT("2_new_story_boot"), false, TEXT("New Story did not start arrival")); SmokeDone(); }
			return;
		case 2:   // the driver plays arrival -> day_one; wait for the day_one pass + the autosave
			if (AutosaveCount > 0 && !M->IsActive() && !M->HasPendingStart())
			{
				const UBullySaveGame* S = LoadSave();
				const bool bOk = S && S->Completion.Contains(TEXT("arrival")) && S->Completion.Contains(TEXT("day_one")) && S->Chapter == 1 && S->bHasRespawn
					&& S->LastMission == TEXT("day_one") && S->Reason == TEXT("autosave:day_one");
				SmokeCheck(TEXT("3_autosave_written"), bOk,
					FString::Printf(TEXT("autosaves=%d, file %s: %s, reason '%s'"), AutosaveCount, SaveExists() ? TEXT("present") : TEXT("MISSING"), S ? *S->Summary() : TEXT("unreadable"), S ? *S->Reason : TEXT("-")));
				Next();
			}
			else if (InPhase > 240.0) { SmokeCheck(TEXT("3_autosave_written"), false, TEXT("no autosave after the boot chain within 240 s")); SmokeDone(); }
			return;
		case 3:   // free roam: the 1_02B star is up (1_03 is not), the HUD points at it
		{
			FString Why;
			const bool bStar = Stars->FindStar(TEXT("1_02B")) != nullptr && Camp->IsAvailable(TEXT("1_02B")) && !Camp->IsAvailable(TEXT("1_03"));
			if ((bStar && HudOk(Why)) || InPhase > 8.0)
			{
				SmokeCheck(TEXT("4_star_and_guidance"), bStar && HudOk(Why), FString::Printf(TEXT("1_02B star %s, 1_03 available=%d; %s"),
					Stars->FindStar(TEXT("1_02B")) ? TEXT("up") : TEXT("MISSING"), Camp->IsAvailable(TEXT("1_03")) ? 1 : 0, *Why));
				Next();
			}
			return;
		}
		case 4:   // mutate the state, then write it through Save & Quit's code path (without quitting)
		{
			Clock->SetClock(SmokeClockHour, SmokeClockMinute);
			Clock->PauseClock();
			Prog->AddMoney(SmokeMoneyAdd);
			Prog->AddItem(SmokeItem, SmokeItemCount);
			Camp->SetRespect(SmokeFaction, SmokeFactionRespect);
			Camp->Collectibles.Add(SmokeCollectible);
			StoryFlags.Add(SmokeFlag);
			if (UBullyWeaponComponent* W = PlayerWeapons(World)) { W->Ammo.Add(SmokeAmmoWeapon, SmokeAmmo); }
			Smoke.SavedMoney = Prog->GetMoney();
			if (const APawn* Pawn = PlayerPawn(World)) { Smoke.ExpectLoc = Pawn->GetActorLocation(); }
			const bool bSaved = SaveAndQuit(/*bQuit*/ false);
			const UBullySaveGame* S = LoadSave();
			const bool bOk = bSaved && S && S->Reason == TEXT("save_and_quit") && S->Money == Smoke.SavedMoney && S->Items.FindRef(SmokeItem) == SmokeItemCount
				&& S->ClockHour == SmokeClockHour && S->ClockMinute == SmokeClockMinute && S->bClockPaused && S->Respect.FindRef(SmokeFaction) == SmokeFactionRespect
				&& S->WeaponAmmo.FindRef(SmokeAmmoWeapon) == SmokeAmmo && S->Collectibles.Contains(SmokeCollectible) && S->StoryFlags.Contains(SmokeFlag)
				&& FVector::Dist(S->RespawnLocation, Smoke.ExpectLoc) < 1.f;
			SmokeCheck(TEXT("5_save_and_quit_written"), bOk, FString::Printf(TEXT("%s; respawn %s"), S ? *S->Summary() : TEXT("unreadable"), S ? *S->RespawnLocation.ToCompactString() : TEXT("-")));
			Next();
			return;
		}
		default:
			SmokeDone();
			return;
		}
	}

	if (Smoke.Mode == FSmoke::EMode::Shots)
	{
		// rendered evidence (offscreen): wait for compilation to drain, then title -> Continue -> the star from two distances
		auto Shoot = [&](const TCHAR* Name)
		{
			const FString File = Smoke.ShotDir / (FString(Name) + TEXT(".png"));
			IFileManager::Get().Delete(*File);
			FScreenshotRequest::RequestScreenshot(File, /*bShowUI*/ true, /*bAddUniqueSuffix*/ false);
			UE_LOG(LogBullyStory, Display, TEXT("BR_STORY shot requested: %s"), *File);
		};
		switch (Smoke.Phase)
		{
		case 0:   // compilation drained + warm-up (uncooked -game compiles shaders on demand)
		{
			int32 Assets = 0, ShaderJobs = 0;
#if WITH_EDITOR
			Assets = FAssetCompilingManager::Get().GetNumRemainingAssets();
			ShaderJobs = GShaderCompilingManager ? GShaderCompilingManager->GetNumRemainingJobs() : 0;
#endif
			if (Assets == 0 && ShaderJobs == 0) { if (Smoke.CompileIdleSince < 0.0) { Smoke.CompileIdleSince = Smoke.Elapsed; } } else { Smoke.CompileIdleSince = -1.0; }
			if (FMath::Fmod(Smoke.Elapsed, 10.0) < Dt) { UE_LOG(LogBullyStory, Display, TEXT("BR_STORY shots: waiting for compilation: %d assets, %d shader jobs"), Assets, ShaderJobs); }
			if (IsTitleOpen() && Smoke.CompileIdleSince >= 0.0 && Smoke.Elapsed - Smoke.CompileIdleSince > 10.0) { Shoot(TEXT("00_title")); Next(); }
			else if (InPhase > 600.0) { SmokeCheck(TEXT("shots_compile"), false, TEXT("compilation never drained")); SmokeDone(); }
			return;
		}
		case 1:   // title shot written; Continue
			if (InPhase > 4.0)
			{
				SmokeCheck(TEXT("shot_00_title"), IFileManager::Get().FileExists(*(Smoke.ShotDir / TEXT("00_title.png"))), TEXT("title screen PNG"));
				TitleActivate();
				Next();
			}
			return;
		case 2:   // restored: stand 18 m from the 1_02B star looking at it
		case 4:   // ... then 5 m
		{
			if (InPhase < 6.0) { return; }
			const AMissionStar* Star = Stars->FindStar(TEXT("1_02B"));
			APawn* Pawn = PlayerPawn(World);
			if (!Star || !Pawn) { SmokeCheck(TEXT("shots_star"), false, TEXT("1_02B star or pawn missing")); SmokeDone(); return; }
			const double Dist = Smoke.Phase == 2 ? 1800.0 : 500.0;
			const FVector S = Star->GetActorLocation();
			bool bPlaced = false;
			for (int32 k = 0; k < 16 && !bPlaced; ++k)
			{
				const float A = FMath::DegreesToRadians(22.5f * k);
				const FVector2D XY(S.X + Dist * FMath::Cos(A), S.Y + Dist * FMath::Sin(A));
				FHitResult Hit;
				FCollisionQueryParams Q(SCENE_QUERY_STAT(BullyStoryShotGround), true);
				if (!World->LineTraceSingleByObjectType(Hit, FVector(XY, S.Z + 350.0), FVector(XY, S.Z - 300.0), FCollisionObjectQueryParams(ECC_WorldStatic), Q)) { continue; }
				const FVector P = Hit.ImpactPoint + FVector(0, 0, 100.0);
				FHitResult Sight;
				if (World->LineTraceSingleByChannel(Sight, P + FVector(0, 0, 60.0), S + FVector(0, 0, 150.0), ECC_Visibility, Q)) { continue; }   // wall in the way
				Pawn->SetActorLocation(P, false, nullptr, ETeleportType::TeleportPhysics);
				// aim ~25 degrees off the star so the player's own body never hides it
				if (AController* Ctl = Pawn->GetController()) { Ctl->SetControlRotation(FRotator(-6.f, (S - P).Rotation().Yaw + 25.f, 0.f)); }
				bPlaced = true;
				UE_LOG(LogBullyStory, Display, TEXT("BR_STORY shots: player placed %.0f m from the star at %s"), Dist / 100.0, *P.ToCompactString());
			}
			if (!bPlaced) { SmokeCheck(TEXT("shots_star"), false, TEXT("no clear sight line to the star")); SmokeDone(); return; }
			Next();
			return;
		}
		case 3:
		case 5:
			if (InPhase > 5.0)
			{
				FVector2D Base2D(-1, -1), Top2D(-1, -1);
				if (PC)
				{
					const AMissionStar* Star = Stars->FindStar(TEXT("1_02B"));
					const FVector S = Star ? Star->GetActorLocation() : FVector::ZeroVector;
					PC->ProjectWorldLocationToScreen(S, Base2D, false);
					PC->ProjectWorldLocationToScreen(S + FVector(0, 0, 900), Top2D, false);
				}
				UE_LOG(LogBullyStory, Display, TEXT("BR_STORY shots: star base on screen at %s, beam top at %s"), *Base2D.ToString(), *Top2D.ToString());
			}
			if (InPhase > 5.0) { Shoot(Smoke.Phase == 3 ? TEXT("01_star_18m") : TEXT("02_star_5m")); Next(); }
			return;
		case 6:
			if (InPhase > 4.0) { SmokeCheck(TEXT("shot_star"), IFileManager::Get().FileExists(*(Smoke.ShotDir / TEXT("01_star_18m.png"))) && IFileManager::Get().FileExists(*(Smoke.ShotDir / TEXT("02_star_5m.png"))), TEXT("star PNGs")); SmokeDone(); }
			return;
		}
		return;
	}

	// ---- continue run
	switch (Smoke.Phase)
	{
	case 0:   // title offers Continue first, with the save summary
		if (IsTitleOpen())
		{
			const bool bShape = TitleItems.Num() == 3 && TitleItems[0].Kind == FTitleItem::EKind::Continue && TitleSelected == 0;
			SmokeCheck(TEXT("1_title_has_continue"), bShape && !GetTitleDetail().IsEmpty() && PC && PC->IsPaused(),
				FString::Printf(TEXT("%d items, first '%s' selected, detail '%s', paused=%d"), TitleItems.Num(), TitleItems.Num() ? *TitleItems[0].Label : TEXT("-"), *GetTitleDetail(), PC && PC->IsPaused() ? 1 : 0));
			TitleActivate();   // Continue
			Next();
		}
		else if (InPhase > 20.0) { SmokeCheck(TEXT("1_title_has_continue"), false, TEXT("title never opened")); SmokeDone(); }
		return;
	case 1:   // let the stars/HUD settle, then verify the restore against the slot and the fixed expectations
		if (InPhase > 3.0)
		{
			const UBullySaveGame* S = LoadSave();
			if (!S) { SmokeCheck(TEXT("2_state_restored"), false, TEXT("slot unreadable")); SmokeDone(); return; }
			UBullyWeaponComponent* W = PlayerWeapons(World);
			const APawn* Pawn = PlayerPawn(World);
			bool bCamp = Camp->GetChapter() == S->Chapter && Camp->IsCompleted(TEXT("arrival")) && Camp->IsCompleted(TEXT("day_one")) && !Camp->IsCompleted(TEXT("1_02B"))
				&& Camp->GetCompletionCount(TEXT("day_one")) == S->Completion.FindRef(TEXT("day_one")) && Camp->GetAttempts(TEXT("day_one")) == S->Attempts.FindRef(TEXT("day_one"))
				&& Camp->GetRespect(SmokeFaction) == SmokeFactionRespect && Camp->HasCollectible(SmokeCollectible) && StoryFlags.Contains(SmokeFlag);
			bool bClock = Clock->GetHour() == SmokeClockHour && Clock->GetMinute() == SmokeClockMinute && Clock->IsPaused() && Clock->GetDay() == S->ClockDay;
			bool bWallet = Prog->GetMoney() == S->Money && Prog->GetMoney() > 0 && Prog->GetItemCount(SmokeItem) == SmokeItemCount;
			bool bAmmo = W && W->GetAmmo(SmokeAmmoWeapon) == SmokeAmmo;
			bool bLoc = Pawn && S->bHasRespawn && FVector::Dist2D(Pawn->GetActorLocation(), S->RespawnLocation) < 150.0;
			SmokeCheck(TEXT("2_state_restored"), bCamp && bClock && bWallet && bAmmo && bLoc,
				FString::Printf(TEXT("campaign=%d (chapter %d, arrival+day_one done, 1_02B open, respect/collectible/flag) clock=%d (%02d:%02d %s day %d) wallet=%d ($%d, items %d) ammo=%d (%d) respawn=%d (%s vs saved %s)"),
					bCamp ? 1 : 0, Camp->GetChapter(), bClock ? 1 : 0, Clock->GetHour(), Clock->GetMinute(), Clock->IsPaused() ? TEXT("paused") : TEXT("running"), Clock->GetDay(),
					bWallet ? 1 : 0, Prog->GetMoney(), Prog->GetItemCount(SmokeItem), bAmmo ? 1 : 0, W ? W->GetAmmo(SmokeAmmoWeapon) : -1,
					bLoc ? 1 : 0, Pawn ? *Pawn->GetActorLocation().ToCompactString() : TEXT("-"), *S->RespawnLocation.ToCompactString()));
			FString Why;
			const bool bStar = Stars->FindStar(TEXT("1_02B")) != nullptr && Camp->IsAvailable(TEXT("1_02B")) && !Camp->IsAvailable(TEXT("1_03"));
			SmokeCheck(TEXT("3_free_roam_stars_and_guidance"), bStar && HudOk(Why) && !M->IsActive() && PC && !PC->IsPaused(),
				FString::Printf(TEXT("1_02B star %s, 1_03 available=%d, mission active=%d, paused=%d; %s"), Stars->FindStar(TEXT("1_02B")) ? TEXT("up") : TEXT("MISSING"),
					Camp->IsAvailable(TEXT("1_03")) ? 1 : 0, M->IsActive() ? 1 : 0, PC && PC->IsPaused() ? 1 : 0, *Why));
			Next();
		}
		return;
	default:
		SmokeDone();
		return;
	}
}
