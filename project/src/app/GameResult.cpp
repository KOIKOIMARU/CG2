#include "app/GameResult.h"

namespace {

bool gIsClear = false;
int gScore = 0;

} // namespace

void GameResult::SetResult(bool isClear, int score)
{
    gIsClear = isClear;
    gScore = score;
}

bool GameResult::IsClear()
{
    return gIsClear;
}

int GameResult::GetScore()
{
    return gScore;
}
