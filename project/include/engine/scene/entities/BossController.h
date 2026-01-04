#pragma once
#include "Boss.h"

struct Player;
class Stage;


class BossController {
public:
    void Update(Boss& boss, const Player& player, const Stage& stage, float dt);
};
