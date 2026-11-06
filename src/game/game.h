#pragma once
#include "common.h"


namespace game {

struct PlayerData {
    bool  valid      = false;
    bool  isAgent    = false;
    bool  disguised  = false;
    bool  isLocal    = false;
    bool  alive      = true;
    bool  onScreen   = false;
    float hp         = 0.0f;
    float hpMax      = 100.0f;
    float dist       = 0.0f;
    float feet[2]    = { 0, 0 };
    float head[2]    = { 0, 0 };
    char  name[48]   = { 0 };
};

bool try_init();
void shutdown();
bool ready();
void build_snapshot(std::vector<PlayerData>& out, float screenW, float screenH);

}
