#pragma once
// Monstey-owned CPU-only include shim for the pinned Microsoft BC translation units.
// Algorithms and BC.h are unchanged. No device, WIC, COM or shader API is included.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <algorithm>
#include <cassert>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <limits>
#include <memory>
#include <new>
#include <tuple>
#include <utility>
#define _XM_NO_XMVECTOR_OVERLOADS_
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
namespace DirectX {
// Exact upstream DirectXTex.h value; only the unused encoder references it.
inline constexpr unsigned TEX_COMPRESS_BC7_QUICK = 0x100000;
}