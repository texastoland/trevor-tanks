# Tanks!

A single-player Linux recreation of Wii Play's *Tanks!* minigame: 20 missions,
9 enemy tank types, ricocheting shells, and mines, rendered as a 3D diorama.
All assets (models, textures, sounds) are generated procedurally — the binary
is fully self-contained.

## Build

Needs CMake >= 3.16 and a C++17 compiler. Uses system raylib 5.x if installed,
otherwise fetches and builds it (which needs X11/GL dev headers: on
Debian/Ubuntu `sudo apt install libx11-dev libxrandr-dev libxinerama-dev
libxcursor-dev libxi-dev libgl1-mesa-dev libasound2-dev`).

    cmake -B build && cmake --build build -j
    ./build/tanks

## Controls

WASD drive - mouse aims the turret - left click fires (max 5 shells, one
ricochet each) - right click or Space lays a mine (max 2) - Esc quits.

## Tests

    ./build/tanks_tests

Progress (highest mission cleared) is saved to
`$XDG_DATA_HOME/tanks/progress.txt`.
