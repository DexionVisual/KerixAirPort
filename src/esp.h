#pragma once
#include <vector>
#include "game/game.h"

namespace esp {


std::vector<game::PlayerData>& lastPlayers();

void draw(const std::vector<game::PlayerData>& players);

}
