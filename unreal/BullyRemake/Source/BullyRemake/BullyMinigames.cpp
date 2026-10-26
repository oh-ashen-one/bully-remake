// OWNER: sm-minigames
#include "BullyMinigames.h"

#include "BullyAnimInstance.h"
#include "BullyCharacter.h"
#include "MissionSubsystem.h"

#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EnhancedInputSubsystems.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Rendering/SlateRenderer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "ImageUtils.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "HighResScreenshot.h"
#include "Rendering/DrawElements.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SLeafWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogBullyMinigame, Log, All);

namespace
{
	const FLinearColor MgGold(0.95f, 0.72f, 0.20f, 1.f);
	const FLinearColor MgCream(0.95f, 0.93f, 0.88f, 1.f);
	const FLinearColor MgPanel(0.03f, 0.03f, 0.05f, 0.78f);
	const FLinearColor MgGreen(0.35f, 0.85f, 0.35f, 1.f);
	const FLinearColor MgRed(0.95f, 0.30f, 0.25f, 1.f);

	// ClassChem action ids (scripts/Library/LibQTE.lua's _CLASSCHEMACTION_* table): 0 cross, 1 circle, 2 square,
	// 3 triangle, 4 L1, 5 L2, 7 R1, 8 R2, 34 end marker. Xbox 360 glyphs: cross = A, circle = B, square = X, triangle = Y.
	constexpr int32 ChemEndAct = 34;
	FKey PadKeyFor(int32 Act)
	{
		switch (Act)
		{
		case 0: return EKeys::Gamepad_FaceButton_Bottom;
		case 1: return EKeys::Gamepad_FaceButton_Right;
		case 2: return EKeys::Gamepad_FaceButton_Left;
		case 3: return EKeys::Gamepad_FaceButton_Top;
		case 4: return EKeys::Gamepad_LeftShoulder;
		case 5: return EKeys::Gamepad_LeftTrigger;
		case 7: return EKeys::Gamepad_RightShoulder;
		case 8: return EKeys::Gamepad_RightTrigger;
		default: return EKeys::Invalid;
		}
	}
	// keyboard: the four face buttons sit on the arrow keys in the same diamond (down = cross, right = circle,
	// left = square, up = triangle); the shoulder actions (classes 3+) on 1..4
	FKey KeyboardKeyFor(int32 Act)
	{
		switch (Act)
		{
		case 0: return EKeys::Down;
		case 1: return EKeys::Right;
		case 2: return EKeys::Left;
		case 3: return EKeys::Up;
		case 4: return EKeys::One;
		case 5: return EKeys::Two;
		case 7: return EKeys::Three;
		case 8: return EKeys::Four;
		default: return EKeys::Invalid;
		}
	}
	const TCHAR* PadGlyphFor(int32 Act)
	{
		switch (Act)
		{
		case 0: return TEXT("HUD_buttonCross");
		case 1: return TEXT("HUD_buttonCircle");
		case 2: return TEXT("HUD_buttonSquare");
		case 3: return TEXT("HUD_buttonTriangle");
		default: return nullptr;
		}
	}
	const TCHAR* KeyGlyphFor(int32 Act)
	{
		switch (Act)
		{
		case 0: return TEXT("Button_Down_Arrow");
		case 1: return TEXT("Button_Right_Arrow");
		case 2: return TEXT("Button_Left_Arrow");
		case 3: return TEXT("Button_Up_Arrow");
		default: return nullptr;
		}
	}
	const TCHAR* ActLabel(int32 Act, bool bPad)
	{
		static const TCHAR* Pad[] = { TEXT("A"), TEXT("B"), TEXT("X"), TEXT("Y"), TEXT("LB"), TEXT("LT"), TEXT("?"), TEXT("RB"), TEXT("RT") };
		static const TCHAR* Kb[] = { TEXT("\u2193"), TEXT("\u2192"), TEXT("\u2190"), TEXT("\u2191"), TEXT("1"), TEXT("2"), TEXT("?"), TEXT("3"), TEXT("4") };
		return (Act >= 0 && Act <= 8) ? (bPad ? Pad[Act] : Kb[Act]) : TEXT("?");
	}
	FLinearColor ActColor(int32 Act)
	{
		switch (Act)
		{
		case 0: return FLinearColor(0.35f, 0.75f, 0.25f, 1.f);
		case 1: return FLinearColor(0.85f, 0.2f, 0.18f, 1.f);
		case 2: return FLinearColor(0.2f, 0.4f, 0.9f, 1.f);
		case 3: return FLinearColor(0.95f, 0.75f, 0.1f, 1.f);
		default: return MgCream;
		}
	}

	// our own wording when imported/minigames/text.json is absent (clean clone)
	const TCHAR* FallbackText(const FString& Key)
	{
		static const TMap<FString, const TCHAR*> M = {
			{ TEXT("C4_GETREADY"), TEXT("Ready...") },
			{ TEXT("C4_GETREADY2"), TEXT("Once more, carefully") },
			{ TEXT("C4_GETREADY3"), TEXT("Final attempt") },
			{ TEXT("C4_BEGIN"), TEXT("Go!") },
			{ TEXT("C4_INST01"), TEXT("Press each button as its icon reaches the ring.") },
			{ TEXT("M_PASS"), TEXT("Class passed") },
			{ TEXT("M_FAIL"), TEXT("Class failed") },
			{ TEXT("C4_UNLOCK01"), TEXT("New chem set recipe:\nfirecrackers") },
			{ TEXT("TUT_CHEM1C1"), TEXT("Your dorm-room chem set can now brew firecrackers.") },
			{ TEXT("TUT_PICK1"), TEXT("Turn the dial the way the arrow points, slowly, and feel for the click.") },
			{ TEXT("CLASS_CHEMISTRY"), TEXT("Chemistry") },
			{ TEXT("CLASS_CHEMISTRY_1"), TEXT("Chemistry 1") },
			{ TEXT("LOCKPICK_TITLE"), TEXT("Locker") },
			{ TEXT("LOCK_PICKED"), TEXT("Open!") },
			{ TEXT("BUT_PICK_LOCK"), TEXT("Pick the lock") },
			{ TEXT("BUT_ROTATE_PAD"), TEXT("Turn") },
			{ TEXT("BUT_ROTATE_PC"), TEXT("Turn") },
		};
		const TCHAR* const* F = M.Find(Key);
		return F ? *F : TEXT("");
	}

	FString RepoImported(const TCHAR* Sub)
	{
		return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../../imported/minigames"), Sub));
	}
}

// ---------------------------------------------------------------------------------------------------------------
// overlay

class SBullyMinigameOverlay : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SBullyMinigameOverlay) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UBullyMinigameSubsystem>, Owner)
	SLATE_END_ARGS()

	void Construct(const FArguments& In) { Owner = In._Owner; SetCanTick(false); }
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1.f, 1.f); }

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Clip, FSlateWindowElementList& Out, int32 Layer,
		const FWidgetStyle& Style, bool bParentEnabled) const override
	{
		const UBullyMinigameSubsystem* S = Owner.Get();
		if (!S) { return Layer; }
		const FVector2f Size = Geo.GetLocalSize();
		if (Size.X < 8.f || Size.Y < 8.f) { return Layer; }
		int32 L = Layer;
		const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");

		auto Box = [&](const FVector2f& P, const FVector2f& Sz, const FLinearColor& C)
		{
			FSlateDrawElement::MakeBox(Out, L++, Geo.ToPaintGeometry(Sz, FSlateLayoutTransform(P)), White, ESlateDrawEffect::None, C);
		};
		auto Txt = [&](const FString& T, const FVector2f& P, int32 Pt, const FLinearColor& C, bool bCenter = false, float W = 0.f)
		{
			const FSlateFontInfo F = FCoreStyle::GetDefaultFontStyle("Bold", Pt);
			FVector2f At = P;
			if (bCenter)
			{
				const TSharedRef<FSlateFontMeasure> M = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
				const FVector2f Ext = FVector2f(M->Measure(T, F));
				At.X -= Ext.X * 0.5f;
			}
			FSlateDrawElement::MakeText(Out, L++, Geo.ToPaintGeometry(FVector2f(W > 0.f ? W : 900.f, Pt * 2.6f), FSlateLayoutTransform(At + FVector2f(2.f, 2.f))), T, F, ESlateDrawEffect::None, FLinearColor(0, 0, 0, 0.85f));
			FSlateDrawElement::MakeText(Out, L++, Geo.ToPaintGeometry(FVector2f(W > 0.f ? W : 900.f, Pt * 2.6f), FSlateLayoutTransform(At)), T, F, ESlateDrawEffect::None, C);
		};
		// a glyph image if imported, else a coloured key cap with the label
		auto GlyphOr = [&](const TCHAR* Name, const FString& Label, const FVector2f& P, float Sz, const FLinearColor& Cap, float Alpha = 1.f)
		{
			if (const FSlateBrush* B = Name ? S->Glyph(Name) : nullptr)
			{
				FSlateDrawElement::MakeBox(Out, L++, Geo.ToPaintGeometry(FVector2f(Sz, Sz), FSlateLayoutTransform(P)), B, ESlateDrawEffect::None, FLinearColor(1, 1, 1, Alpha));
				return;
			}
			Box(P, FVector2f(Sz, Sz), FLinearColor(0.f, 0.f, 0.f, 0.8f * Alpha));
			Box(P + FVector2f(3.f, 3.f), FVector2f(Sz - 6.f, Sz - 6.f), Cap.CopyWithNewOpacity(Alpha));
			Txt(Label, P + FVector2f(Sz * 0.5f, Sz * 0.18f), int32(Sz * 0.42f), FLinearColor(0.05f, 0.05f, 0.07f, Alpha), true);
		};
		auto Ring = [&](const FVector2f& C, float R, const FLinearColor& Col, float Thick, int32 Seg = 48)
		{
			TArray<FVector2f> Pts;
			for (int32 i = 0; i <= Seg; ++i)
			{
				const float A = 2.f * UE_PI * i / Seg;
				Pts.Add(C + FVector2f(FMath::Cos(A), FMath::Sin(A)) * R);
			}
			FSlateDrawElement::MakeLines(Out, L++, Geo.ToPaintGeometry(), Pts, ESlateDrawEffect::None, Col, true, Thick);
		};

		const double Now = S->Now();
		const UBullyMinigameSubsystem::EPhase Ph = S->GetPhase();
		const bool bPad = S->UsingGamepad();

		// ---- climb prompt (the mission HUD's prompt style, lower centre)
		const FString CP = S->ClimbPrompt();
		if (!CP.IsEmpty() && !S->IsRunning())
		{
			const FVector2f P(Size.X * 0.5f - 150.f, Size.Y - 200.f);
			Box(P, FVector2f(300.f, 46.f), MgPanel);
			GlyphOr(bPad ? TEXT("HUD_buttonTriangle") : TEXT("Button_E"), bPad ? TEXT("Y") : TEXT("E"), P + FVector2f(8.f, 5.f), 36.f, MgGold);
			Txt(CP, P + FVector2f(54.f, 10.f), 16, MgCream);
		}

		if (S->GetKind() == UBullyMinigameSubsystem::EKind::Chem)
		{
			const UBullyMinigameSubsystem::FChemState& C = S->GetChem();
			// lane (scrolling prompts, ClassChemSetScrollyOnly)
			const float LaneW = FMath::Min(820.f, Size.X - 80.f), LaneH = 104.f;
			const FVector2f LaneP(Size.X * 0.5f - LaneW * 0.5f, Size.Y - LaneH - 130.f);   // clear of the letterbox / subtitle row
			const float RingX = LaneP.X + 110.f, MidY = LaneP.Y + LaneH * 0.5f;
			const float PxPerSec = 300.f, Icon = 64.f;
			if (Ph == UBullyMinigameSubsystem::EPhase::Ready || Ph == UBullyMinigameSubsystem::EPhase::Begin || Ph == UBullyMinigameSubsystem::EPhase::Play || Ph == UBullyMinigameSubsystem::EPhase::Failed)
			{
				Box(LaneP, FVector2f(LaneW, LaneH), MgPanel);
				if (const FSlateBrush* R = S->Glyph(TEXT("HUD_circleoutline")))
				{
					FSlateDrawElement::MakeBox(Out, L++, Geo.ToPaintGeometry(FVector2f(92.f, 92.f), FSlateLayoutTransform(FVector2f(RingX - 46.f, MidY - 46.f))), R, ESlateDrawEffect::None, MgGold);
				}
				else { Ring(FVector2f(RingX, MidY), 44.f, MgGold, 4.f); }
				for (int32 i = 0; i < C.Prompts.Num(); ++i)
				{
					const UBullyMinigameSubsystem::FChemPrompt& Pr = C.Prompts[i];
					const float X = RingX + float(Pr.DueAt - Now) * PxPerSec;
					if (X < LaneP.X - Icon || X > LaneP.X + LaneW - Icon * 0.5f) { continue; }
					if (Pr.Result == 1 && X < RingX - Icon) { continue; }
					const FVector2f IP(X - Icon * 0.5f, MidY - Icon * 0.5f);
					if (Pr.bEnd)
					{
						Box(FVector2f(X - 3.f, LaneP.Y + 10.f), FVector2f(6.f, LaneH - 20.f), MgCream);
						continue;
					}
					const float A = Pr.Result == 1 ? 0.45f : 1.f;
					GlyphOr(bPad ? PadGlyphFor(Pr.Act) : KeyGlyphFor(Pr.Act), ActLabel(Pr.Act, bPad), IP, Icon, ActColor(Pr.Act), A);
					if (Pr.Result == 1) { GlyphOr(TEXT("Grade_check"), TEXT("+"), IP + FVector2f(Icon - 26.f, -6.f), 28.f, MgGreen); }
					if (Pr.Result == -1) { GlyphOr(TEXT("Grade_cross"), TEXT("x"), IP + FVector2f(Icon * 0.5f - 20.f, Icon * 0.5f - 20.f), 40.f, MgRed); }
				}
				// runs left (three experiments)
				for (int32 r = 0; r < C.Runs; ++r)
				{
					const bool bUsed = r < C.Run || (r == C.Run && Ph == UBullyMinigameSubsystem::EPhase::Failed);
					Box(FVector2f(LaneP.X + LaneW - 24.f - r * 20.f, LaneP.Y - 18.f), FVector2f(14.f, 10.f), bUsed ? MgRed : MgGold);
				}
				Txt(C.ClassTitle, FVector2f(LaneP.X, LaneP.Y - 28.f), 14, MgGold);
			}
			const FVector2f Mid(Size.X * 0.5f, Size.Y * 0.32f);
			if (Ph == UBullyMinigameSubsystem::EPhase::Ready || Ph == UBullyMinigameSubsystem::EPhase::Begin)
			{
				const TCHAR* Key = Ph == UBullyMinigameSubsystem::EPhase::Begin ? TEXT("C4_BEGIN") : (C.Run == 0 ? TEXT("C4_GETREADY") : (C.Run == 1 ? TEXT("C4_GETREADY2") : TEXT("C4_GETREADY3")));
				Txt(S->Text(Key), Mid, 40, MgGold, true);
				if (Ph == UBullyMinigameSubsystem::EPhase::Ready)
				{
					const FString Inst = S->Text(TEXT("C4_INST01"));
					Box(FVector2f(Size.X * 0.5f - 300.f, 40.f), FVector2f(600.f, 54.f), MgPanel);
					Txt(Inst, FVector2f(Size.X * 0.5f - 286.f, 54.f), 14, MgCream, false, 572.f);
				}
			}
			else if (Ph == UBullyMinigameSubsystem::EPhase::Grades || Ph == UBullyMinigameSubsystem::EPhase::Unlock)
			{
				// grade screen (MinigameSetGrades): pass / fail + the class with its mark
				const FVector2f P(Size.X * 0.5f - 230.f, Size.Y * 0.24f);
				Box(P, FVector2f(460.f, 170.f), MgPanel);
				Box(P, FVector2f(460.f, 4.f), C.bPassed ? MgGold : MgRed);
				Txt(S->Text(C.bPassed ? TEXT("M_PASS") : TEXT("M_FAIL")), FVector2f(Size.X * 0.5f, P.Y + 18.f), 32, C.bPassed ? MgGold : MgRed, true);
				Txt(C.ClassTitle, FVector2f(P.X + 90.f, P.Y + 92.f), 22, MgCream);
				GlyphOr(C.bPassed ? TEXT("Grade_check") : TEXT("Grade_cross"), C.bPassed ? TEXT("+") : TEXT("x"), FVector2f(P.X + 340.f, P.Y + 82.f), 52.f, C.bPassed ? MgGreen : MgRed);
				if (Ph == UBullyMinigameSubsystem::EPhase::Unlock)
				{
					const FVector2f U(Size.X * 0.5f - 260.f, P.Y + 190.f);
					Box(U, FVector2f(520.f, 120.f), MgPanel);
					Txt(S->Text(TEXT("C4_UNLOCK01")), FVector2f(U.X + 18.f, U.Y + 14.f), 18, MgGold, false, 490.f);
					Txt(S->Text(TEXT("TUT_CHEM1C1")), FVector2f(U.X + 18.f, U.Y + 74.f), 13, MgCream, false, 490.f);
				}
			}
			return L;
		}

		if (S->GetKind() == UBullyMinigameSubsystem::EKind::Lock)
		{
			const UBullyMinigameSubsystem::FLockState& K = S->GetLock();
			const FVector2f C(Size.X * 0.72f, Size.Y * 0.5f);
			const float R = FMath::Min(150.f, Size.Y * 0.2f);
			const bool bClick = Now - K.ClickAt < 0.35;
			Box(C - FVector2f(R + 40.f, R + 90.f), FVector2f(2.f * R + 80.f, 2.f * R + 170.f), MgPanel);
			Txt(S->Text(TEXT("LOCKPICK_TITLE")), FVector2f(C.X, C.Y - R - 84.f), 18, MgGold, true);
			// dial face: 40 numbers turning under a fixed index at the top; + = clockwise (numbers run down under the index)
			Ring(C, R, bClick ? MgGold : MgCream, 3.f, 64);
			Ring(C, R * 0.35f, MgCream.CopyWithNewOpacity(0.5f), 2.f, 32);
			for (int32 n = 0; n < 40; ++n)
			{
				const float A = (float(n) - K.Dial) / 40.f * 2.f * UE_PI - UE_PI * 0.5f;
				const FVector2f Dir(FMath::Cos(A), FMath::Sin(A));
				const float In = (n % 5 == 0) ? 0.82f : 0.9f;
				TArray<FVector2f> Tick = { C + Dir * R * In, C + Dir * R * 0.98f };
				FSlateDrawElement::MakeLines(Out, L++, Geo.ToPaintGeometry(), Tick, ESlateDrawEffect::None, MgCream, true, n % 5 == 0 ? 2.5f : 1.2f);
				if (n % 5 == 0)
				{
					const FVector2f NP = C + Dir * R * 0.66f - FVector2f(0.f, 9.f);
					Txt(FString::FromInt(n), NP, 11, MgCream, true);
				}
			}
			// index + current number
			Box(FVector2f(C.X - 3.f, C.Y - R - 22.f), FVector2f(6.f, 22.f), bClick ? MgGold : MgRed);
			Txt(FString::Printf(TEXT("%d"), int32(FMath::RoundToInt(K.Dial)) % 40), FVector2f(C.X, C.Y - 14.f), 20, MgGold, true);
			// arrow: which way to turn now (the tutorial's "in the direction of the arrow")
			if (K.Step < 3)
			{
				const bool bCw = UBullyMinigameSubsystem::LockDirFor(K.Step) > 0;
				GlyphOr(bCw ? TEXT("arrow_right") : TEXT("arrow_left"), bCw ? TEXT(">") : TEXT("<"), FVector2f(C.X - 28.f, C.Y + R + 10.f), 56.f, MgGold);
			}
			// tumblers
			for (int32 t = 0; t < 3; ++t)
			{
				Box(FVector2f(C.X - 50.f + t * 36.f, C.Y - R - 54.f), FVector2f(28.f, 12.f), t < K.Step ? MgGold : FLinearColor(0.3f, 0.3f, 0.32f, 1.f));
			}
			// controls row
			const FVector2f Row(C.X - R - 30.f, C.Y + R + 74.f);
			if (bPad) { GlyphOr(TEXT("Analog_R"), TEXT("RS"), Row, 32.f, MgCream); }
			else
			{
				GlyphOr(TEXT("Button_A"), TEXT("A"), Row, 32.f, MgCream);
				GlyphOr(TEXT("Button_D"), TEXT("D"), Row + FVector2f(36.f, 0.f), 32.f, MgCream);
			}
			Txt(S->Text(bPad ? TEXT("BUT_ROTATE_PAD") : TEXT("BUT_ROTATE_PC")).Replace(TEXT("{rstick}"), TEXT("")).Replace(TEXT("{scroll_weapl}"), TEXT("")).Replace(TEXT("{scroll_weapr}"), TEXT("")).TrimStartAndEnd(),
				Row + FVector2f(bPad ? 40.f : 76.f, 6.f), 14, MgCream);
			GlyphOr(bPad ? TEXT("HUD_buttonCircle") : TEXT("Button_Backspace"), bPad ? TEXT("B") : TEXT("<-"), Row + FVector2f(2.f * R - 30.f, 0.f), 32.f, MgCream);
			// tutorial card (RUSLOCKX / TUT_PICK1)
			FString Tut = S->Text(TEXT("TUT_PICK1"));
			Tut = Tut.Replace(TEXT("{rstick}"), bPad ? TEXT("right stick") : TEXT("dial (A / D)"));
			Box(FVector2f(Size.X * 0.5f - 300.f, 40.f), FVector2f(600.f, 54.f), MgPanel);
			Txt(Tut, FVector2f(Size.X * 0.5f - 286.f, 54.f), 14, MgCream, false, 572.f);
			if (Ph == UBullyMinigameSubsystem::EPhase::Open) { Txt(S->Text(TEXT("LOCK_PICKED")), FVector2f(C.X, C.Y + 30.f), 22, MgGreen, true); }
		}
		return L;
	}

private:
	TWeakObjectPtr<UBullyMinigameSubsystem> Owner;
};

// ---------------------------------------------------------------------------------------------------------------
// lifecycle

UBullyMinigameSubsystem* UBullyMinigameSubsystem::Get(const UWorld* World)
{
	return World ? World->GetSubsystem<UBullyMinigameSubsystem>() : nullptr;
}

bool UBullyMinigameSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* W = Cast<UWorld>(Outer);
	return W && (W->WorldType == EWorldType::Game || W->WorldType == EWorldType::PIE);
}

void UBullyMinigameSubsystem::Deinitialize()
{
	ShowOverlay(false);
	Super::Deinitialize();
}

TStatId UBullyMinigameSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBullyMinigameSubsystem, STATGROUP_Tickables);
}

double UBullyMinigameSubsystem::Now() const
{
	const UWorld* W = GetWorld();
	return W ? W->GetTimeSeconds() : 0.0;
}

ABullyCharacter* UBullyMinigameSubsystem::Player() const
{
	const UWorld* W = GetWorld();
	const APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
	return PC ? Cast<ABullyCharacter>(PC->GetPawn()) : nullptr;
}

void UBullyMinigameSubsystem::Fire(FName Event)
{
	if (Event.IsNone()) { return; }
	if (UMissionSubsystem* M = UMissionSubsystem::Get(GetWorld())) { M->FireEvent(Event); }
}

FString UBullyMinigameSubsystem::Text(const TCHAR* Key) const
{
	if (!bAssetsLoaded) { const_cast<UBullyMinigameSubsystem*>(this)->LoadAssets(); }
	if (const FString* T = Texts.Find(Key)) { return *T; }
	return FallbackText(Key);
}

const FSlateBrush* UBullyMinigameSubsystem::Glyph(const TCHAR* Name) const
{
	return Brushes.Find(FName(Name));
}

void UBullyMinigameSubsystem::LoadAssets()
{
	if (bAssetsLoaded) { return; }
	bAssetsLoaded = true;
	FString Json;
	if (FFileHelper::LoadFileToString(Json, *RepoImported(TEXT("text.json"))))
	{
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(Json);
		const TSharedPtr<FJsonObject>* TextObj = nullptr;
		if (FJsonSerializer::Deserialize(R, Root) && Root.IsValid() && Root->TryGetObjectField(TEXT("text"), TextObj))
		{
			for (const auto& Kv : (*TextObj)->Values) { Texts.Add(FString(*Kv.Key), Kv.Value->AsString()); }
		}
	}
	// glyph textures only where something renders them (a -nullrhi harness has no use for them)
	int32 NGlyphs = 0;
	if (!FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
	{
		TArray<FString> Files;
		IFileManager::Get().FindFiles(Files, *FPaths::Combine(RepoImported(TEXT("glyphs")), TEXT("*.png")), true, false);
		for (const FString& F : Files)
		{
			UTexture2D* T = FImageUtils::ImportFileAsTexture2D(FPaths::Combine(RepoImported(TEXT("glyphs")), F));
			if (!T) { continue; }
			GlyphTextures.Add(T);
			FSlateBrush B;
			B.SetResourceObject(T);
			B.ImageSize = FVector2D(T->GetSizeX(), T->GetSizeY());
			B.DrawAs = ESlateBrushDrawType::Image;
			Brushes.Add(FName(*FPaths::GetBaseFilename(F)), B);
			++NGlyphs;
		}
	}
	UE_LOG(LogBullyMinigame, Display, TEXT("BR_MINIGAME assets: %d original strings, %d original glyphs%s"), Texts.Num(), NGlyphs,
		Texts.Num() == 0 ? TEXT(" (not imported: python3 -m tools.importers minigame -- using our own wording)") : TEXT(""));
}

void UBullyMinigameSubsystem::ShowOverlay(bool bOn)
{
	if (bOn)
	{
		LoadAssets();
		if (Overlay.IsValid() || !GEngine || !GEngine->GameViewport || !FSlateApplication::IsInitialized()) { return; }
		Overlay = SNew(SBullyMinigameOverlay).Owner(this);
		GEngine->GameViewport->AddViewportWidgetContent(Overlay.ToSharedRef(), 60);   // over the mission HUD (50)
	}
	else if (Overlay.IsValid())
	{
		if (GEngine && GEngine->GameViewport) { GEngine->GameViewport->RemoveViewportWidgetContent(Overlay.ToSharedRef()); }
		Overlay.Reset();
	}
}

void UBullyMinigameSubsystem::LockInput(bool bOn, bool bFullBlock)
{
	UWorld* W = GetWorld();
	APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
	if (!PC || bOn == bInputLocked) { return; }
	bInputLocked = bOn;
	PC->SetIgnoreMoveInput(bOn);   // a stacked counter: balanced by bInputLocked
	UEnhancedInputLocalPlayerSubsystem* EIS = PC->GetLocalPlayer() ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()) : nullptr;
	if (!EIS) { return; }
	if (bOn && bFullBlock)
	{
		// a top-priority context mapping the action keys to a consuming no-op: the class's / dial's buttons must not
		// also jump, swing, sprint or cycle weapons underneath
		if (!BlockContext)
		{
			BlockAction = NewObject<UInputAction>(this);
			BlockAction->ValueType = EInputActionValueType::Boolean;
			BlockAction->bConsumeInput = true;
			BlockContext = NewObject<UInputMappingContext>(this);
			const FKey Keys[] = { EKeys::SpaceBar, EKeys::LeftMouseButton, EKeys::RightMouseButton, EKeys::F, EKeys::Q, EKeys::LeftShift,
				EKeys::MouseScrollUp, EKeys::MouseScrollDown, EKeys::Gamepad_FaceButton_Bottom, EKeys::Gamepad_FaceButton_Right,
				EKeys::Gamepad_FaceButton_Left, EKeys::Gamepad_FaceButton_Top, EKeys::Gamepad_LeftShoulder, EKeys::Gamepad_RightShoulder,
				EKeys::Gamepad_RightTrigger, EKeys::Gamepad_LeftTrigger, EKeys::Gamepad_DPad_Left, EKeys::Gamepad_DPad_Right,
				EKeys::Gamepad_LeftThumbstick, EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four };
			for (const FKey& K : Keys) { BlockContext->MapKey(BlockAction, K); }
		}
		EIS->AddMappingContext(BlockContext, 1000);
	}
	else if (!bOn && BlockContext)
	{
		EIS->RemoveMappingContext(BlockContext);
	}
}

void UBullyMinigameSubsystem::SetPlayerClip(FName Clip, float Seconds)
{
	ABullyCharacter* P = Player();
	UBullyAnimInstance* AI = P && P->GetMesh() ? Cast<UBullyAnimInstance>(P->GetMesh()->GetAnimInstance()) : nullptr;
	if (!AI) { return; }
	if (Clip.IsNone()) { AI->SetRideClip(NAME_None); ClipEndsAt = -1.0; return; }
	if (!AI->FindClip(Clip)) { return; }
	AI->SetRideClip(Clip);
	ClipEndsAt = Seconds > 0.f ? Now() + Seconds : -1.0;   // <= 0: a loop that stays until replaced/cleared
}

void UBullyMinigameSubsystem::Rumble(float Intensity, float Seconds)
{
	if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		PC->PlayDynamicForceFeedback(Intensity, Seconds, true, true, true, true);
	}
}

// ---------------------------------------------------------------------------------------------------------------
// begin / finish

bool UBullyMinigameSubsystem::Begin(const FStart& S)
{
	if (IsRunning()) { Abort(TEXT("restarted")); }
	Args = S;
	Kind = S.Kind;
	bLastOk = false;
	if (Kind == EKind::Chem)
	{
		TArray<FChemPrompt> Steps;
		FString Title;
		int32 Runs = 3;
		LoadAssets();   // the class title comes from the imported strings when present
		if (!LoadChemConfig(S.Config, Steps, ChemFirstExtra, ChemAllowed, Runs, Title))
		{
			UE_LOG(LogBullyMinigame, Warning, TEXT("BR_MINIGAME chem: class config '%s' missing from Config/Minigames/ClassChem.json"), *S.Config);
			Kind = EKind::None;
			return false;
		}
		Chem = FChemState();
		Chem.Prompts = Steps;
		Chem.ClassTitle = Title;
		Chem.Runs = FMath::Max(1, Runs);
		// F_SetDifficulty + main(): non-end entries get a random action from the first `allowedActions` of gActions on the
		// first run; the retries keep the same acts (the randomizer only runs while no run has failed)
		static const int32 Pool[] = { 0, 1, 2, 3, 4, 7, 5, 8 };
		ChemActs.Reset();
		for (FChemPrompt& P : Chem.Prompts)
		{
			if (!P.bEnd) { P.Act = Pool[FMath::RandRange(0, FMath::Clamp(ChemAllowed, 1, 8) - 1)]; }
			ChemActs.Add(P.Act);
		}
		Phase = EPhase::Intro;
		Chem.PhaseAt = Now();
		bSmokeBotched = false;
		FString Acts;
		for (int32 A : ChemActs) { Acts += FString::Printf(TEXT("%d "), A); }
		UE_LOG(LogBullyMinigame, Display, TEXT("BR_MINIGAME chem '%s' begins: %s, %d prompts (acts %s), %d runs"), *S.Id.ToString(), *Title, Chem.Prompts.Num(), *Acts.TrimEnd(), Chem.Runs);
		LockInput(true, true);
	}
	else if (Kind == EKind::Lock)
	{
		Lock = FLockState();
		// MGLockPicking F_AssignRandomValue: a fresh combo, unless the last attempt failed within 5 m (same locker)
		if (bHaveLastCombo && FVector::Dist(LastComboAt, S.At) <= 500.f)
		{
			for (int32 i = 0; i < 3; ++i) { Lock.Combo[i] = LastCombo[i]; }
			UE_LOG(LogBullyMinigame, Display, TEXT("BR_MINIGAME lock: same locker as the failed attempt -> same combination"));
		}
		else
		{
			for (int32 i = 0; i < 3; ++i) { Lock.Combo[i] = FMath::RandRange(0, 39); }
		}
		bHaveLastCombo = false;
		Lock.Dial = float(FMath::RandRange(0, 39));
		LockBeganAt = Now();
		Phase = EPhase::Play;
		UE_LOG(LogBullyMinigame, Display, TEXT("BR_MINIGAME lock '%s' begins: combo %d-%d-%d dial at %.0f"), *S.Id.ToString(), Lock.Combo[0], Lock.Combo[1], Lock.Combo[2], Lock.Dial);
		LockInput(true, true);
		// face the locker: the nearest wall around him at chest height (the lock spot is a point, not a facing)
		if (ABullyCharacter* P = Player())
		{
			const FVector Chest = P->GetActorLocation() + FVector(0, 0, 40.f);
			FCollisionQueryParams Q(SCENE_QUERY_STAT(BullyLockFace), false, P);
			float BestD = 200.f, BestYaw = P->GetActorRotation().Yaw;
			for (int32 i = 0; i < 24; ++i)
			{
				const float Yaw = i * 15.f;
				FHitResult H;
				if (GetWorld()->LineTraceSingleByChannel(H, Chest, Chest + FRotator(0.f, Yaw, 0.f).Vector() * 200.f, ECC_Visibility, Q) && H.Distance < BestD)
				{
					BestD = H.Distance; BestYaw = Yaw;
				}
			}
			P->SetActorRotation(FRotator(0.f, BestYaw, 0.f));
			if (AController* Ctl = P->GetController()) { Ctl->SetControlRotation(FRotator(-10.f, BestYaw, 0.f)); }
		}
		SetPlayerClip(TEXT("IDLE2PICK"), 0.6f);
	}
	else { return false; }
	ShowOverlay(true);
	return true;
}

void UBullyMinigameSubsystem::Abort(const TCHAR* Why)
{
	if (!IsRunning()) { return; }
	UE_LOG(LogBullyMinigame, Display, TEXT("BR_MINIGAME %s aborted (%s)"), Kind == EKind::Chem ? TEXT("chem") : TEXT("lock"), Why);
	Kind = EKind::None;
	Phase = EPhase::Idle;
	LockInput(false, false);
	SetPlayerClip(NAME_None, 0.f);
	if (Climbs.Num() == 0) { ShowOverlay(false); }
}

void UBullyMinigameSubsystem::Finish(bool bOk)
{
	const EKind K = Kind;
	bLastOk = bOk;
	Kind = EKind::None;
	Phase = EPhase::Idle;
	// a timed one-shot (OPEN_LOCKER, the class's celebration) plays out on its own; one that holds the player keeps the lock
	if (!(bClipLocksMove && ClipEndsAt >= 0.0)) { LockInput(false, false); }
	if (ClipEndsAt < 0.0) { SetPlayerClip(NAME_None, 0.f); }
	if (Climbs.Num() == 0) { ShowOverlay(false); }
	UE_LOG(LogBullyMinigame, Display, TEXT("BR_MINIGAME %s '%s' %s -> event %s"), K == EKind::Chem ? TEXT("chem") : TEXT("lock"), *Args.Id.ToString(),
		bOk ? TEXT("SUCCESS") : TEXT("FAIL"), *(bOk ? Args.SuccessEvent : Args.FailEvent).ToString());
	Fire(bOk ? Args.SuccessEvent : Args.FailEvent);
}

// ---------------------------------------------------------------------------------------------------------------
// chemistry class

bool UBullyMinigameSubsystem::LoadChemConfig(const FString& Name, TArray<FChemPrompt>& Out, float& OutFirstExtra, int32& OutAllowed, int32& OutRuns, FString& OutTitle) const
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("Minigames/ClassChem.json")))) { return false; }
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(R, Root) || !Root.IsValid()) { return false; }
	const TSharedPtr<FJsonObject>* Classes = nullptr;
	const TSharedPtr<FJsonObject>* Cls = nullptr;
	if (!Root->TryGetObjectField(TEXT("classes"), Classes) || !(*Classes)->TryGetObjectField(Name.IsEmpty() ? TEXT("1") : Name, Cls)) { return false; }
	const FJsonObject& C = **Cls;
	const float Short = float(C.GetNumberField(TEXT("short_s")));
	const float Long = float(C.GetNumberField(TEXT("long_s")));
	const float Window = float(C.GetNumberField(TEXT("window_s")));
	OutFirstExtra = float(C.GetNumberField(TEXT("first_extra_s")));
	OutAllowed = int32(C.GetNumberField(TEXT("allowed_actions")));
	OutRuns = C.HasField(TEXT("runs")) ? int32(C.GetNumberField(TEXT("runs"))) : 3;
	FString TitleKey;
	OutTitle = C.TryGetStringField(TEXT("title_key"), TitleKey) ? Text(*TitleKey) : FString();
	if (OutTitle.IsEmpty()) { C.TryGetStringField(TEXT("title"), OutTitle); }
	Out.Reset();
	const TArray<TSharedPtr<FJsonValue>>* Steps = nullptr;
	if (!C.TryGetArrayField(TEXT("steps"), Steps)) { return false; }
	for (const TSharedPtr<FJsonValue>& V : *Steps)
	{
		const TArray<TSharedPtr<FJsonValue>>& E = V->AsArray();
		if (E.Num() < 2) { continue; }
		FChemPrompt P;
		const FString Gap = E[0]->AsString(), What = E[1]->AsString();
		P.Window = Window;
		P.DueAt = Gap == TEXT("long") ? Long : Short;   // the step's waitTime until the run schedules it (ChemStartRun)
		P.bEnd = What == TEXT("end");
		P.Act = P.bEnd ? ChemEndAct : 0;
		if (!P.bEnd && What != TEXT("-")) { P.Clip = FName(*What); }
		Out.Add(P);
	}
	return Out.Num() > 0;
}

void UBullyMinigameSubsystem::ChemStartRun()
{
	// schedule this run: the first prompt reaches the ring waitTime + 2 s after "Begin" (ClassChemAddAction(.., waitTime + 2,
	// ..) for key 1), every later one waitTime after the previous prompt's window closes
	TArray<FChemPrompt> Steps;
	FString Title;
	int32 Runs = 3;
	LoadChemConfig(Args.Config, Steps, ChemFirstExtra, ChemAllowed, Runs, Title);
	double T = Now();
	for (int32 i = 0; i < Steps.Num() && i < Chem.Prompts.Num(); ++i)
	{
		const float Wait = float(Steps[i].DueAt) + (i == 0 ? ChemFirstExtra : 0.f);
		T += Wait + (i > 0 ? Steps[i - 1].Window : 0.f);
		Chem.Prompts[i].DueAt = T;
		Chem.Prompts[i].Result = 0;
		Chem.Prompts[i].Act = ChemActs.IsValidIndex(i) ? ChemActs[i] : Chem.Prompts[i].Act;
	}
	Chem.Next = 0;
	Phase = EPhase::Play;
	Chem.PhaseAt = Now();
	UE_LOG(LogBullyMinigame, Display, TEXT("BR_MINIGAME chem run %d/%d starts (%d prompts, end at +%.1fs)"), Chem.Run + 1, Chem.Runs, Chem.Prompts.Num(),
		Chem.Prompts.Num() ? float(Chem.Prompts.Last().DueAt - Now()) : 0.f);
}

void UBullyMinigameSubsystem::ChemPress(int32 Act)
{
	if (Phase != EPhase::Play || !Chem.Prompts.IsValidIndex(Chem.Next)) { return; }
	FChemPrompt& P = Chem.Prompts[Chem.Next];
	if (P.bEnd) { return; }
	const double Dt = Now() - P.DueAt;
	if (FMath::Abs(Dt) > P.Window * 0.5f) { return; }   // nothing inside the ring: a stray press is ignored
	if (Act != P.Act)
	{
		P.Result = -1;
		ChemFailRun(TEXT("wrong button"));
		return;
	}
	P.Result = 1;
	UE_LOG(LogBullyMinigame, Verbose, TEXT("BR_MINIGAME chem prompt %d ok (%+.2fs)"), Chem.Next + 1, Dt);
	// F_ActionsCallback(pass): the step's animation on Jimmy
	if (!P.Clip.IsNone()) { SetPlayerClip(P.Clip, 1.2f); }
	++Chem.Next;
}

void UBullyMinigameSubsystem::ChemFailRun(const TCHAR* Why)
{
	UE_LOG(LogBullyMinigame, Display, TEXT("BR_MINIGAME chem run %d/%d failed at prompt %d (%s)"), Chem.Run + 1, Chem.Runs, Chem.Next + 1, Why);
	Phase = EPhase::Failed;
	Chem.PhaseAt = Now();
	// the bad reaction: smoke on the first two failures, the explosion on the last (FailAnimTable)
	const bool bLast = Chem.Run + 1 >= Chem.Runs;
	SetPlayerClip(bLast ? TEXT("MINIC_REACT_EXPLODE") : TEXT("MINIC_REACT_SMOKE"), bLast ? 2.4f : 2.6f);
	Rumble(bLast ? 1.f : 0.4f, bLast ? 1.5f : 0.4f);
}

void UBullyMinigameSubsystem::TickChem(float Dt, APlayerController* PC)
{
	const double T = Now() - Chem.PhaseAt;
	switch (Phase)
	{
	case EPhase::Intro:
		if (T >= 2.0) { Phase = EPhase::Ready; Chem.PhaseAt = Now(); }
		break;
	case EPhase::Ready:
		if (T >= 2.5) { Phase = EPhase::Begin; Chem.PhaseAt = Now(); }
		break;
	case EPhase::Begin:
		if (T >= 1.0) { ChemStartRun(); }
		break;
	case EPhase::Play:
	{
		// input: the class's action buttons (gamepad face buttons / arrow keys)
		if (PC)
		{
			for (int32 Act = 0; Act <= 8; ++Act)
			{
				const FKey Pad = PadKeyFor(Act), Kb = KeyboardKeyFor(Act);
				const bool bPadHit = Pad.IsValid() && PC->WasInputKeyJustPressed(Pad);
				if (bPadHit) { bGamepad = true; }
				if (Kb.IsValid() && PC->WasInputKeyJustPressed(Kb)) { bGamepad = false; ChemPress(Act); }
				else if (bPadHit) { ChemPress(Act); }
				if (Phase != EPhase::Play) { return; }
			}
		}
		// harness: answer through the same press path
		if (SmokeAssist > 0 && Chem.Prompts.IsValidIndex(Chem.Next))
		{
			const FChemPrompt& P = Chem.Prompts[Chem.Next];
			if (!P.bEnd && Now() >= P.DueAt - 0.05)
			{
				// -BRMinigameFailAll: botch every run (the three-strikes fail path: explosion, failed grade, MissionFail)
				const bool bBotch = Chem.Next == 2 && (FParse::Param(FCommandLine::Get(), TEXT("BRMinigameFailAll")) || (!bSmokeBotched && Chem.Run == 0));
				if (bBotch) { bSmokeBotched = true; UE_LOG(LogBullyMinigame, Display, TEXT("BR_MINIGAME smoke botches prompt 3 of run %d on purpose (retry rule)"), Chem.Run + 1); }
				ChemPress(bBotch ? (P.Act + 1) % 4 : P.Act);
				if (Phase != EPhase::Play) { return; }
			}
		}
		if (!Chem.Prompts.IsValidIndex(Chem.Next)) { break; }
		FChemPrompt& P = Chem.Prompts[Chem.Next];
		if (P.bEnd)
		{
			if (Now() >= P.DueAt)
			{
				// the end marker reached the ring: experiment done (MinigameIsSuccess)
				P.Result = 1;
				Chem.bPassed = true;
				Phase = EPhase::Grades;
				Chem.PhaseAt = Now();
				SetPlayerClip(TEXT("MG_CELEB_HI_2"), 2.0f);
				UE_LOG(LogBullyMinigame, Display, TEXT("BR_MINIGAME chem run %d/%d clean -> PASS (grade screen)"), Chem.Run + 1, Chem.Runs);
			}
		}
		else if (Now() > P.DueAt + P.Window * 0.5f)
		{
			P.Result = -1;
			ChemFailRun(TEXT("missed"));
		}
		break;
	}
	case EPhase::Failed:
		// F_ActionsCallback(fail) waits 1 s, the loop waits 2 s more before the next MinigameStart
		if (T >= 3.3)
		{
			if (Chem.Run + 1 < Chem.Runs)
			{
				++Chem.Run;
				Phase = EPhase::Ready;
				Chem.PhaseAt = Now();
			}
			else
			{
				Chem.bPassed = false;
				Phase = EPhase::Grades;
				Chem.PhaseAt = Now();
				UE_LOG(LogBullyMinigame, Display, TEXT("BR_MINIGAME chem: all %d runs failed -> FAIL (grade screen)"), Chem.Runs);
			}
		}
		break;
	case EPhase::Grades:
	{
		const bool bSkip = T > 1.5 && PC && (PC->WasInputKeyJustPressed(EKeys::Enter) || PC->WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Bottom));
		if (T >= 4.0 || bSkip)
		{
			if (Chem.bPassed)
			{
				// F_EndPresentation: the unlock card while Jimmy plays the weapon-earned clip
				Phase = EPhase::Unlock;
				Chem.PhaseAt = Now();
				SetPlayerClip(TEXT("EARN_WEAPON_A"), 2.0f);
			}
			else { Finish(false); }
		}
		break;
	}
	case EPhase::Unlock:
		if (T >= 5.5 || (T > 1.5 && PC && (PC->WasInputKeyJustPressed(EKeys::Enter) || PC->WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Bottom)))) { Finish(true); }
		break;
	default: break;
	}
}

FKey UBullyMinigameSubsystem::HumanChemKey(bool bWrong) const
{
	if (Kind != EKind::Chem || Phase != EPhase::Play || !Chem.Prompts.IsValidIndex(Chem.Next)) { return EKeys::Invalid; }
	const FChemPrompt& P = Chem.Prompts[Chem.Next];
	if (P.bEnd || FMath::Abs(Now() - P.DueAt) > P.Window * 0.35f) { return EKeys::Invalid; }
	return KeyboardKeyFor(bWrong ? (P.Act + 1) % 4 : P.Act);
}

// ---------------------------------------------------------------------------------------------------------------
// lockpick dial

namespace
{
	constexpr float LockKeySpeed = 8.f;        // numbers / s while a dial key is held (a slow, clicking turn)
	constexpr float LockClickMaxSpeed = 14.f;  // turning faster than this rolls past the number without the click
}

void UBullyMinigameSubsystem::LockTurn(float Numbers)
{
	if (Phase != EPhase::Play || FMath::IsNearlyZero(Numbers)) { return; }
	const float Before = Lock.Dial;
	Lock.Dial = FMath::Fmod(Lock.Dial - Numbers + 400.f, 40.f);   // + (clockwise) runs the numbers down under the index
	Lock.bTurned = true;
	if (Lock.Step >= 3) { return; }
	const int32 Dir = Numbers > 0.f ? 1 : -1;
	if (Dir != LockDirFor(Lock.Step) || FMath::Abs(Lock.Speed) > LockClickMaxSpeed) { return; }
	// did the index pass the target number on the way? (dial moved from Before by -Numbers, cyclic)
	const float Target = float(Lock.Combo[Lock.Step]);
	const float Travel = FMath::Abs(Numbers);
	float Dist = Dir > 0 ? Before - Target : Target - Before;   // distance still to go in the turning direction
	Dist = FMath::Fmod(Dist + 400.f, 40.f);
	// reversing onto the number the dial already shows (a repeated digit) takes a full turn, like a real lock
	const bool bFirstNudge = Lock.StepTravel <= 0.f;
	Lock.StepTravel += Travel;
	if (Dist <= Travel + KINDA_SMALL_NUMBER && !(bFirstNudge && Dist < 0.5f))
	{
		Lock.StepTravel = 0.f;
		Lock.Dial = Target;
		++Lock.Step;
		Lock.ClickAt = Now();
		Rumble(0.6f, 0.15f);
		UE_LOG(LogBullyMinigame, Display, TEXT("BR_MINIGAME lock click %d/3 at %d"), Lock.Step, Lock.Combo[Lock.Step - 1]);
		if (Lock.Step >= 3)
		{
			Phase = EPhase::Open;
			Lock.ClickAt = Now();
			SetPlayerClip(TEXT("OPEN_LOCKER"), 2.8f);
			bClipLocksMove = ClipEndsAt >= 0.0;   // stays put while the locker swings open
		}
	}
}

void UBullyMinigameSubsystem::TickLock(float Dt, APlayerController* PC)
{
	if (Phase == EPhase::Open)
	{
		if (Now() - Lock.ClickAt >= 1.4) { Finish(true); }
		return;
	}
	float Turn = 0.f;
	bool bExit = false;
	if (PC)
	{
		if (PC->IsInputKeyDown(EKeys::D) || PC->IsInputKeyDown(EKeys::Right)) { Turn += LockKeySpeed * Dt; bGamepad = false; }
		if (PC->IsInputKeyDown(EKeys::A) || PC->IsInputKeyDown(EKeys::Left)) { Turn -= LockKeySpeed * Dt; bGamepad = false; }
		if (PC->WasInputKeyJustPressed(EKeys::MouseScrollDown)) { Turn += 1.f; bGamepad = false; }
		if (PC->WasInputKeyJustPressed(EKeys::MouseScrollUp)) { Turn -= 1.f; bGamepad = false; }
		// right stick: turn it round like the dial (half the dial per stick revolution); the left stick sideways also turns
		const FVector2D RS(PC->GetInputAnalogKeyState(EKeys::Gamepad_RightX), PC->GetInputAnalogKeyState(EKeys::Gamepad_RightY));
		static float LastAngle = 0.f;
		static bool bHadAngle = false;
		if (RS.Size() > 0.5f)
		{
			bGamepad = true;
			const float A = FMath::Atan2(float(RS.Y), float(RS.X));
			if (bHadAngle)
			{
				float D = A - LastAngle;
				if (D > UE_PI) { D -= 2.f * UE_PI; } else if (D < -UE_PI) { D += 2.f * UE_PI; }
				Turn += -D / (2.f * UE_PI) * 20.f;   // stick Y up is +: clockwise on screen = decreasing angle
			}
			LastAngle = A;
			bHadAngle = true;
		}
		else { bHadAngle = false; }
		const float LX = PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftX);
		if (FMath::Abs(LX) > 0.3f) { Turn += LX * LockKeySpeed * Dt; bGamepad = true; }
		bExit = PC->WasInputKeyJustPressed(EKeys::BackSpace) || PC->WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Right);
	}
	// harness: walk away once after the first click (the combo must survive), then turn the arrow's way
	if (SmokeAssist > 0)
	{
		if (!bSmokeWalkedAway && Lock.Step == 1)
		{
			bSmokeWalkedAway = true;
			UE_LOG(LogBullyMinigame, Display, TEXT("BR_MINIGAME smoke walks away from the lock after the first click (combo memory rule)"));
			bExit = true;
		}
		else { Turn += LockDirFor(Lock.Step) * LockKeySpeed * Dt; }
	}
	if (bExit)
	{
		// MGLockPicking: leaving the prop fails the attempt; the same locker keeps its combination
		bHaveLastCombo = true;
		LastComboAt = Args.At;
		for (int32 i = 0; i < 3; ++i) { LastCombo[i] = Lock.Combo[i]; }
		SetPlayerClip(TEXT("PICK2IDLE"), 0.9f);
		Finish(false);
		return;
	}
	Lock.Speed = Dt > 0.f ? FMath::FInterpTo(Lock.Speed, Turn / Dt, Dt, 10.f) : 0.f;
	if (ClipEndsAt < 0.0 || Now() > ClipEndsAt)   // after IDLE2PICK: turning hands (PICKLOCK_R / _L) or the picking idle
	{
		SetPlayerClip(Turn > 0.f ? FName(TEXT("PICKLOCK_R")) : (Turn < 0.f ? FName(TEXT("PICKLOCK_L")) : FName(TEXT("PICKLOOP"))), 0.f);
	}
	if (FMath::Abs(Turn) > 0.f) { LockTurn(Turn); }
}

int32 UBullyMinigameSubsystem::HumanLockDir(FKey& Key) const
{
	if (Kind != EKind::Lock || Phase != EPhase::Play || Lock.Step >= 3) { Key = EKeys::Invalid; return 0; }
	const int32 D = LockDirFor(Lock.Step);
	Key = D > 0 ? EKeys::D : EKeys::A;
	return D;
}

// ---------------------------------------------------------------------------------------------------------------
// climb spots

void UBullyMinigameSubsystem::ArmClimb(const FClimbSpot& Spot)
{
	Climbs.RemoveAll([&](const FClimbSpot& C) { return C.Id == Spot.Id; });
	Climbs.Add(Spot);
	if (Spot.ClearMeshes.Num() > 0)
	{
		int32 N = 0;
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			if (FVector::Dist2D(It->GetActorLocation(), Spot.Base) > 1500.f) { continue; }
			TArray<UStaticMeshComponent*> Comps;
			It->GetComponents(Comps);
			for (UStaticMeshComponent* C : Comps)
			{
				const FString Mesh = C && C->GetStaticMesh() ? C->GetStaticMesh()->GetName() : FString();
				if (Mesh.IsEmpty() || !Spot.ClearMeshes.ContainsByPredicate([&](const FString& Sub) { return Mesh.Contains(Sub); })) { continue; }
				if (FVector::Dist2D(C->Bounds.Origin, Spot.Base) > 1200.f) { continue; }
				C->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Ignore);
				C->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
				C->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);   // the free-aim ray from a camera behind the canopy
				ClearedComps.Add(C);
				++N;
			}
		}
		UE_LOG(LogBullyMinigame, Display, TEXT("BR_MINIGAME climb spot '%s': %d climbable mesh(es) let pellets + camera through"), *Spot.Id.ToString(), N);
	}
	UE_LOG(LogBullyMinigame, Display, TEXT("BR_MINIGAME climb spot '%s' armed: base %s -> perch %s (%.0f cm up)"), *Spot.Id.ToString(),
		*Spot.Base.ToCompactString(), *Spot.Top.ToCompactString(), Spot.Top.Z - Spot.Base.Z);
	ShowOverlay(true);
}

void UBullyMinigameSubsystem::DisarmClimbs()
{
	if (ClimbState != EClimb::None)
	{
		// mission over mid-climb / perched: put him down at the trunk on his feet
		if (const FClimbSpot* S = FindClimb(ClimbId))
		{
			if (ABullyCharacter* P = Player())
			{
				const float HH = P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
				P->SetActorLocation(S->Base + FVector(0, 0, HH + 2.f), false, nullptr, ETeleportType::TeleportPhysics);
				P->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
			}
		}
		SetPlayerClip(NAME_None, 0.f);
		LockInput(false, false);
		ClimbState = EClimb::None;
	}
	Climbs.Reset();
	for (const TWeakObjectPtr<UPrimitiveComponent>& C : ClearedComps)
	{
		if (C.IsValid()) { C->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block); C->SetCollisionResponseToChannel(ECC_Camera, ECR_Block); C->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block); }
	}
	ClearedComps.Reset();
	if (!IsRunning()) { ShowOverlay(false); }
}

const UBullyMinigameSubsystem::FClimbSpot* UBullyMinigameSubsystem::FindClimb(FName Id) const
{
	return Climbs.FindByPredicate([&](const FClimbSpot& C) { return C.Id == Id; });
}

bool UBullyMinigameSubsystem::IsClimbArmed(FName Id) const { return FindClimb(Id) != nullptr; }

FString UBullyMinigameSubsystem::ClimbPrompt() const
{
	if (ClimbState == EClimb::Perched) { const FClimbSpot* S = FindClimb(ClimbId); return S ? S->DownPrompt : FString(); }
	if (ClimbState != EClimb::None) { return FString(); }
	const ABullyCharacter* P = Player();
	if (!P) { return FString(); }
	const float HH = P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Feet = P->GetActorLocation() - FVector(0, 0, HH);
	for (const FClimbSpot& S : Climbs)
	{
		if (FVector::Dist2D(Feet, S.Base) <= S.Radius && FMath::Abs(Feet.Z - S.Base.Z) < 150.f) { return S.UpPrompt; }
	}
	return FString();
}

bool UBullyMinigameSubsystem::RequestClimb(FName Id, bool bUp)
{
	const FClimbSpot* S = FindClimb(Id);
	ABullyCharacter* P = Player();
	if (!S || !P) { return false; }
	if (bUp ? ClimbState != EClimb::None : ClimbState != EClimb::Perched) { return false; }
	ClimbId = Id;
	ClimbAt = Now();
	ClimbLeg = 0;
	ClimbFrom = P->GetActorLocation() - FVector(0, 0, P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	if (bUp)
	{
		SavedMoveMode = uint8(P->GetCharacterMovement()->MovementMode.GetValue());
		LockInput(true, false);
		P->GetCharacterMovement()->StopMovementImmediately();
		P->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
		ClimbState = EClimb::Up;
		SetPlayerClip(TEXT("TREE_ON_BOT"), 0.f);
		UE_LOG(LogBullyMinigame, Display, TEXT("BR_MINIGAME climb '%s' up from %s"), *Id.ToString(), *ClimbFrom.ToCompactString());
	}
	else
	{
		ClimbState = EClimb::Down;
		SetPlayerClip(TEXT("TREE_DOWN"), 0.f);
		UE_LOG(LogBullyMinigame, Display, TEXT("BR_MINIGAME climb '%s' down"), *Id.ToString());
	}
	return true;
}

void UBullyMinigameSubsystem::TickClimb(float Dt, APlayerController* PC)
{
	ABullyCharacter* P = Player();
	if (!P || Climbs.Num() == 0) { return; }
	const float HH = P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Feet = P->GetActorLocation() - FVector(0, 0, HH);
	const bool bAction = PC && !IsRunning() && (PC->WasInputKeyJustPressed(EKeys::E) || PC->WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Top));
	if (PC && PC->WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Top)) { bGamepad = true; }
	if (PC && PC->WasInputKeyJustPressed(EKeys::E)) { bGamepad = false; }

	if (ClimbState == EClimb::None)
	{
		for (const FClimbSpot& S : Climbs)
		{
			// a stage retry puts him back where the stage began -- on the perch: sit him straight back down
			if (FVector::Dist(Feet, S.Top) < 90.f)
			{
				ClimbId = S.Id;
				SavedMoveMode = uint8(MOVE_Walking);
				LockInput(true, false);
				P->GetCharacterMovement()->StopMovementImmediately();
				P->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
				ClimbState = EClimb::Perched;
				SetPlayerClip(TEXT("TREE_IDLE_A"), 0.f);
				UE_LOG(LogBullyMinigame, Display, TEXT("BR_MINIGAME climb '%s': back on the perch (stage retry)"), *S.Id.ToString());
				Fire(FName(*FString::Printf(TEXT("climb:%s_top"), *S.Id.ToString())));
				return;
			}
			if (bAction && FVector::Dist2D(Feet, S.Base) <= S.Radius && FMath::Abs(Feet.Z - S.Base.Z) < 150.f) { RequestClimb(S.Id, true); return; }
		}
		return;
	}
	const FClimbSpot* S = FindClimb(ClimbId);
	if (!S) { ClimbState = EClimb::None; return; }
	auto Place = [&](const FVector& FeetAt, float Yaw)
	{
		P->SetActorLocation(FeetAt + FVector(0, 0, HH), false, nullptr, ETeleportType::TeleportPhysics);
		P->SetActorRotation(FRotator(0.f, Yaw, 0.f));
		P->GetCharacterMovement()->Velocity = FVector::ZeroVector;
	};
	const FVector Branch = (S->Top - S->Trunk) * FVector(1, 1, 0);
	const float FaceTrunkYaw = Branch.IsNearlyZero() ? S->TopYaw : Branch.Rotation().Yaw;
	const double T = Now() - ClimbAt;
	if (ClimbState == EClimb::Up)
	{
		// leg 0: step onto the trunk (TREE_ON_BOT), leg 1: shin up (TREE_UP_CYCLE), leg 2: out onto the branch (BRNCH_UP2)
		if (ClimbLeg == 0)
		{
			const float A = FMath::Clamp(float(T / 0.6), 0.f, 1.f);
			Place(FMath::Lerp(ClimbFrom, S->Base, A), FaceTrunkYaw);
			if (A >= 1.f) { ClimbLeg = 1; ClimbAt = Now(); SetPlayerClip(TEXT("TREE_UP_CYCLE"), 0.f); }
		}
		else if (ClimbLeg == 1)
		{
			const float Len = FMath::Max(1.f, float(S->Trunk.Z - S->Base.Z));
			const float A = FMath::Clamp(float(T * 110.0 / Len), 0.f, 1.f);
			Place(FMath::Lerp(S->Base, S->Trunk, A), FaceTrunkYaw);
			if (A >= 1.f) { ClimbLeg = 2; ClimbAt = Now(); SetPlayerClip(TEXT("BRNCH_UP2"), 0.f); }
		}
		else
		{
			const float A = FMath::Clamp(float(T / 1.6), 0.f, 1.f);
			Place(FMath::Lerp(S->Trunk, S->Top, A), FMath::Lerp(FaceTrunkYaw, S->TopYaw, A));
			if (A >= 1.f)
			{
				ClimbState = EClimb::Perched;
				SetPlayerClip(TEXT("TREE_IDLE_A"), 0.f);
				UE_LOG(LogBullyMinigame, Display, TEXT("BR_MINIGAME climb '%s' perched at %s"), *S->Id.ToString(), *S->Top.ToCompactString());
				Fire(FName(*FString::Printf(TEXT("climb:%s_top"), *S->Id.ToString())));
			}
		}
		return;
	}
	if (ClimbState == EClimb::Perched)
	{
		// pinned on the branch: free to look, aim and shoot; the action key climbs down
		const FVector Here = P->GetActorLocation() - FVector(0, 0, HH);
		if (FVector::Dist(Here, S->Top) > 2.f) { P->SetActorLocation(S->Top + FVector(0, 0, HH), false, nullptr, ETeleportType::TeleportPhysics); }
		P->GetCharacterMovement()->Velocity = FVector::ZeroVector;
		if (bAction) { RequestClimb(S->Id, false); }
		return;
	}
	// down: branch -> trunk top -> foot (TREE_DOWN), then off the trunk (TREE_OFF_BOT)
	if (ClimbLeg == 0)
	{
		const float A = FMath::Clamp(float(T / 1.0), 0.f, 1.f);
		Place(FMath::Lerp(S->Top, S->Trunk, A), FaceTrunkYaw);
		if (A >= 1.f) { ClimbLeg = 1; ClimbAt = Now(); }
	}
	else if (ClimbLeg == 1)
	{
		const float Len = FMath::Max(1.f, float(S->Trunk.Z - S->Base.Z));
		const float A = FMath::Clamp(float(T * 160.0 / Len), 0.f, 1.f);
		Place(FMath::Lerp(S->Trunk, S->Base, A), FaceTrunkYaw);
		if (A >= 1.f) { ClimbLeg = 2; ClimbAt = Now(); SetPlayerClip(TEXT("TREE_OFF_BOT"), 0.f); }
	}
	else if (T >= 0.5)
	{
		P->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		SetPlayerClip(NAME_None, 0.f);
		LockInput(false, false);
		ClimbState = EClimb::None;
		UE_LOG(LogBullyMinigame, Display, TEXT("BR_MINIGAME climb '%s' back on the ground"), *S->Id.ToString());
		Fire(FName(*FString::Printf(TEXT("climb:%s_bottom"), *S->Id.ToString())));
	}
}

// ---------------------------------------------------------------------------------------------------------------
// player clips

bool UBullyMinigameSubsystem::PlayPlayerClip(const TArray<FString>& Candidates, float Seconds, bool bLockMove)
{
	ABullyCharacter* P = Player();
	UBullyAnimInstance* AI = P && P->GetMesh() ? Cast<UBullyAnimInstance>(P->GetMesh()->GetAnimInstance()) : nullptr;
	if (!AI) { return false; }
	for (const FString& C : Candidates)
	{
		UAnimSequence* S = AI->FindClip(FName(*C.TrimStartAndEnd()));
		if (!S) { continue; }
		const float Len = Seconds > 0.f ? Seconds : S->GetPlayLength();
		SetPlayerClip(FName(*C.TrimStartAndEnd()), Len);
		bClipLocksMove = bLockMove;
		if (bLockMove)
		{
			LockInput(true, false);
			P->GetCharacterMovement()->StopMovementImmediately();
		}
		UE_LOG(LogBullyMinigame, Display, TEXT("BR_MINIGAME player clip %s (%.2fs%s)"), *C, Len, bLockMove ? TEXT(", movement held") : TEXT(""));
		return true;
	}
	UE_LOG(LogBullyMinigame, Display, TEXT("BR_MINIGAME player clip: none of [%s] imported (tools/ue/run_import_character.sh --anims-only), skipped"), *FString::Join(Candidates, TEXT(",")));
	return false;
}

void UBullyMinigameSubsystem::TickPlayerClip(float Dt)
{
	if (ClipEndsAt >= 0.0 && Now() >= ClipEndsAt)
	{
		ClipEndsAt = -1.0;
		// back to the pose underneath: the perch idle, the lock's picking loop (TickLock), or plain standing
		if (ClimbState == EClimb::Perched) { SetPlayerClip(TEXT("TREE_IDLE_A"), 0.f); }
		else if (ClimbState == EClimb::None && Kind != EKind::Lock) { SetPlayerClip(NAME_None, 0.f); }
		if (bClipLocksMove)
		{
			bClipLocksMove = false;
			if (!IsRunning() && ClimbState == EClimb::None) { LockInput(false, false); }
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------

void UBullyMinigameSubsystem::Tick(float Dt)
{
	UWorld* W = GetWorld();
	if (!W) { return; }
	APlayerController* PC = W->GetFirstPlayerController();
	if (Kind == EKind::Chem) { TickChem(Dt, PC); }
	else if (Kind == EKind::Lock) { TickLock(Dt, PC); }
	TickClimb(Dt, PC);
	TickPlayerClip(Dt);
	TickUiShots();
}

void UBullyMinigameSubsystem::TickUiShots()
{
	FString Dir;
	if (!FParse::Value(FCommandLine::Get(), TEXT("-BRMinigameUIShot="), Dir)) { return; }
	FString Name;
	if (Kind == EKind::Chem && Phase == EPhase::Play && Chem.Prompts.IsValidIndex(Chem.Next) && Now() > Chem.Prompts[0].DueAt - 0.4) { Name = FString::Printf(TEXT("ui_chem_run%d"), Chem.Run + 1); }
	else if (Kind == EKind::Chem && Phase == EPhase::Grades && Now() - Chem.PhaseAt > 1.0) { Name = TEXT("ui_chem_grades"); }
	else if (Kind == EKind::Chem && Phase == EPhase::Unlock && Now() - Chem.PhaseAt > 1.0) { Name = TEXT("ui_chem_unlock"); }
	else if (Kind == EKind::Lock && Phase == EPhase::Play && Now() - LockBeganAt > 1.5) { Name = TEXT("ui_lock"); }
	else if (ClimbState == EClimb::Perched) { Name = TEXT("ui_perch"); }
	if (Name.IsEmpty() || UiShotsTaken.Contains(Name)) { return; }
	UiShotsTaken.Add(Name);
	const FString File = FPaths::Combine(Dir, Name + TEXT(".png"));
	GetHighResScreenshotConfig().SetFilename(File);   // a shots pass leaves its last still's name there, which wins
	FScreenshotRequest::RequestScreenshot(File, true, false);
	UE_LOG(LogBullyMinigame, Display, TEXT("BR_MINIGAME ui screenshot -> %s"), *File);
}
