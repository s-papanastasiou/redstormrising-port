Red Storm Rising for Windows
============================

The game (MicroProse, 1990) as it ran on the Amiga, in C++. This package
holds the program only: the game's files are MicroProse's and must come
from your own copy of the game.

Setting up
----------
Put one of these in this folder, next to rsr.exe:
  - the WHDLoad install of the game, RedStormRising_v1.0_1603.lha, or
  - both disk images of version v843.02 (the two .adf files; unzip them
    if they came zipped).
That is all: there is no need to unpack anything else.

The first start reads the game's files out of them, checks that they are
this build's (other versions of the game, such as v843.01, differ and are
refused) and keeps them in %APPDATA%\RedStormRising\data. After that the
archive or the disks are not needed.

(An unpacked install works too: copy its RedStormRising\data folder
here as "data". Or give the archive, the disks or the folder to rsr.exe.)

Playing
-------
Double-click run.bat (or rsr.exe). The launch screen offers:
  CRT look   Off, or a CRT monitor look (Lottes fast, Guest fast, Guest,
             Royale; the last needs a fast graphics card)
  Display    Fullscreen or a window
  Input      Responsive (every click and key press counts) or Original
             (as in 1990: a quick click, or digits typed quickly in a
             battle, can be lost)
  START / QUIT
Your choices are remembered for the next start.

From a command prompt, options skip the launch screen:
  rsr.exe --help              all the options
  rsr.exe --fullscreen --crt-lottes-fast
In the game:
  F11                         fullscreen on / off
  F12                         capture / release the mouse
The Amiga's Help key is Home. keys.pdf lists every key of the game.

Saved games and the hall of fame are kept in %APPDATA%\RedStormRising.

Licence
-------
This program is free software under the GNU General Public License,
version 3 or later: see LICENSE.txt. Its complete source code is in
source.zip, next to this file. Parts of it come from fa18-interceptor-port
by Ingo Ruhnke (GPL-3.0-or-later).

The CRT shaders are libretro's (shaders\libretro\README.md lists their
licences). SDL3.dll is SDL (zlib licence).

Red Storm Rising itself (its programs, pictures, sounds and texts)
remains the copyright of its owners (MicroProse, 1990) and is not part of
this package.
