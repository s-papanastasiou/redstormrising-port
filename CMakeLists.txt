# SPDX-FileCopyrightText: 2026 Stelios Papanastasiou
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke
# SPDX-License-Identifier: GPL-3.0-or-later
#
cmake_minimum_required(VERSION 3.16)
project(redstormrising-port LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(SDL3 REQUIRED CONFIG)

# Amiga machine model + AmigaOS high-level emulation.
add_library(amiga STATIC src/amiga/machine.cpp src/amiga/custom.cpp src/amiga/hle.cpp
  src/amiga/intuition.cpp src/amiga/tasks.cpp
  src/amiga/gfx_draw.cpp
  src/amiga/audio_device.cpp
  src/amiga/modules.cpp src/host/files.cpp src/host/game_archive.cpp)
target_include_directories(amiga PUBLIC src)

# The game's C++ code (src/game, docs/history/restructure-strategy.md) and its
# runtime. An object library: state tables register themselves from static
# initialisers, which a static library would drop.
add_library(game OBJECT src/runtime/runtime.cpp
  src/game/red/units.cpp src/game/red/map.cpp
  src/game/mem/os.cpp src/game/mem/trace.cpp src/game/mem/state.cpp src/game/platform/platform.cpp src/game/audio/audio.cpp
  src/game/storm/sound.cpp src/game/screen/display.cpp src/game/screen/draw.cpp
  src/game/screen/requesters.cpp src/game/screen/text.cpp src/game/screen/pictures.cpp
  src/game/storm/text.cpp src/game/storm/battle.cpp src/game/storm/state.cpp src/game/storm/ships.cpp src/game/storm/panels.cpp src/game/storm/keys.cpp src/game/storm/pointer.cpp src/game/red/texts.cpp src/game/common/strings.cpp src/game/common/memory.cpp src/game/common/startup.cpp src/game/common/stdio.cpp src/game/common/module.cpp src/game/common/devices.cpp src/game/ui/state.cpp src/game/mem/module_state.cpp src/game/mem/constants.cpp src/game/audio/state.cpp src/game/common/state.cpp src/game/common/records.cpp src/game/common/process.cpp src/game/redstorm/state.cpp src/game/redstart/state.cpp src/game/storm/texts.cpp src/game/screen/anim_state.cpp src/game/red/texts_state.cpp src/game/screen/styles.cpp src/game/screen/bitmap.cpp src/game/redstart/saves.cpp src/game/redstart/menus.cpp src/game/screen/blitter.cpp src/game/screen/sequences.cpp src/game/screen/decoder.cpp src/game/title/title.cpp src/game/redstorm/loader.cpp src/game/redstorm/display.cpp
  src/game/screen/animation.cpp src/game/input/input.cpp src/game/input/menu.cpp
  src/game/common/math.cpp src/game/common/random.cpp src/game/red/movement.cpp src/game/red/frame.cpp src/game/red/fronts.cpp src/game/red/routes.cpp src/game/red/campaign.cpp src/game/red/clock.cpp src/game/red/groups.cpp src/game/red/turn.cpp src/game/red/text.cpp src/game/red/loadout.cpp src/game/red/pages.cpp src/game/red/menu.cpp src/game/red/record.cpp src/game/red/state.cpp
  src/game/input/wait.cpp src/game/input/tick.cpp)
target_include_directories(game PUBLIC src)
target_compile_options(game PRIVATE -Wall -Wextra)

include(GNUInstallDirs)

# Optional libretro shader presenter (--crt-royale / --shader-preset),
# copied from fa18-interceptor-port src/present.
add_executable(rsr src/main.cpp src/launcher.cpp src/present/shader_preset.cpp src/present/shader_chain.cpp)
target_link_libraries(rsr PRIVATE game amiga SDL3::SDL3)
target_compile_definitions(rsr PRIVATE
  RSR_DATADIR="${CMAKE_INSTALL_FULL_DATADIR}/redstormrising"
  RSR_SOURCE_SHADER_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/shaders"
  RSR_SOURCE_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/extracted/RedStormRising/data")
if(WIN32)
  # Windows: the C++ runtime linked in, so the program needs only SDL3.dll
  # next to it (tools/package-windows.sh).
  target_link_options(rsr PRIVATE -static-libgcc -static-libstdc++ -static)
endif()
install(TARGETS rsr RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY data/shaders DESTINATION ${CMAKE_INSTALL_DATADIR}/redstormrising)

enable_testing()
add_executable(shader_preset_test tests/shader_preset.cpp src/present/shader_preset.cpp)
target_include_directories(shader_preset_test PRIVATE src)
add_test(NAME shader_preset COMMAND shader_preset_test)
