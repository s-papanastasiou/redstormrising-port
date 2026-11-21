// SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
// SPDX-License-Identifier: GPL-3.0-or-later

// Storm's native state (state.hpp): its registration.
#include "game/storm/state.hpp"
#include "game/storm/vars.hpp"
#include "game/mem/image.hpp"

#include <cstring>

namespace game::storm {

GAME_STATE_FIELDS(0, "storm", state, kMovedRanges, kScriptFields);
GAME_STATE_DATA(0, "storm", battle, kMovedData, load_battle);

namespace {
ShipType g_torpedo_type;
constexpr int32_t kTorpedoTypeAt = kShipTypesAt + kTorpedoType * int32_t(sizeof(ShipType));
constexpr mem::state::Range kTorpedoTypeRange[] = {{kTorpedoTypeAt, int32_t(sizeof(ShipType))}};
void load_torpedo_type() { std::memcpy(&g_torpedo_type, mem::state::loaded_image(kTorpedoTypeAt), sizeof(ShipType)); }
}  // namespace
GAME_STATE_DATA(0, "storm", g_torpedo_type, kTorpedoTypeRange, load_torpedo_type);

ShipType& torpedo_type() { return g_torpedo_type; }

namespace {
int16_t g_tracked_weapon;
constexpr mem::state::Range kTrackedWeaponRange[] = {{kTrackedWeaponAt, 2}};
void load_tracked_weapon() { mem::image::read(mem::state::loaded_image(kTrackedWeaponAt), g_tracked_weapon); }
}  // namespace
GAME_STATE_DATA(0, "storm", g_tracked_weapon, kTrackedWeaponRange, load_tracked_weapon);

int16_t& tracked_weapon() { return g_tracked_weapon; }

}  // namespace game::storm
