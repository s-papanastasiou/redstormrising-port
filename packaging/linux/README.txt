Red Storm Rising for Linux
==========================

The game (MicroProse, 1990) as it ran on the Amiga, in C++. This package
holds the program only: the game's files are MicroProse's and must come
from your own copy of the game.

It runs on x86-64 Linux distributions from 2022 on (glibc 2.35 or newer:
Ubuntu 22.04, Debian 12, Fedora 36 and later). It brings its own SDL3
(lib/); the CRT looks need OpenGL 3.

Setting up
----------
Put one of these in this folder, next to rsr:
  - the WHDLoad install of the game, RedStormRising_v1.0_1603.lha, or
  - both disk images of version v843.02 (the two .adf files; unzip them
    if they came zipped).
That is all: there is no need to unpack anything else.

The first start reads the game's files out of them, checks that they are
this build's (other versions of the game, such as v843.01, differ and are
refused) and keeps them in ~/.local/share/redstormrising/data. After that
the archive or the disks are not needed.

(An unpacked install works too: copy its RedStormRising/data folder
here as "data". Or give the archive, the disks or the folder to rsr.)

Playing
-------
Run ./rsr (or double-click it in a file manager). The launch screen offers:
  CRT look   Off, or a CRT monitor look (Lottes fast, Guest fast, Guest,
             Royale; the last needs a fast graphics card)
  Display    Fullscreen or a window
  Input      Responsive (every click and key press counts) or Original
             (as in 1990: a quick click, or digits typed quickly in a
             battle, can be lost)
  Color      Original or Alternative (a dark blue grid on the weapon
             control display, F2, instead of the bright cyan)
  Map        Original or Detailed (the ports the briefings send you to,
             such as Murmansk and Arkhangel'sk, marked and named on the
             strategic map, and Spitsbergen named)
  START / QUIT
Your choices are remembered for the next start.

Options skip the launch screen:
  ./rsr --help                all the options
  ./rsr --fullscreen --crt-lottes-fast
In the game:
  F11                         fullscreen on / off
  F12                         capture / release the mouse
The Amiga's Help key is Home. keys.pdf lists every key of the game.

Saved games and the hall of fame are kept in ~/.local/share/redstormrising.

Licence
-------
This program is free software under the GNU General Public License,
version 3 or later: see LICENSE.txt. Its complete source code is in
source.zip, next to this file. Parts of it come from fa18-interceptor-port
by Ingo Ruhnke (GPL-3.0-or-later).

The CRT shaders are libretro's (shaders/libretro/README.md lists their
licences). lib/libSDL3.so.0 is SDL (zlib licence).

Red Storm Rising itself (its programs, pictures, sounds and texts)
remains the copyright of its owners (MicroProse, 1990) and is not part of
this package.
