#pragma once
#include <windows.h>

struct IDXGISwapChain;

namespace overlay {


void frame(IDXGISwapChain* sc, UINT sync, UINT flags);

}
