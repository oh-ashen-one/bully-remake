// OWNER: sm-playloop
#include "BullySaveGame.h"

FString UBullySaveGame::Summary() const
{
	int32 Passed = 0;
	for (const TPair<FString, int32>& P : Completion) { if (P.Value > 0) { ++Passed; } }
	return FString::Printf(TEXT("Chapter %d  -  %d missions done  -  $%d  -  Day %d, %d:%02d"), Chapter, Passed, Money, ClockDay, ClockHour, ClockMinute);
}
