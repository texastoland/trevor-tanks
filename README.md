# Tanks!

A single-player Linux recreation of Wii Play's *Tanks!* minigame: 20 missions,
9 enemy tank types, ricocheting shells, and mines, rendered as a 3D diorama.
All assets (models, textures, sounds) are generated procedurally — the binary
is fully self-contained.

## Build

Needs CMake >= 3.16 and a C++17 compiler. raylib 6 comes from Conan: `mise`
installs CMake and the Conan CLI, and `mise run build` resolves the dependency,
configures with the generated toolchain and compiles. On a profile with no
prebuilt package, `mise run build:conan` compiles it from source.

    mise run build
    mise run run

## Controls

WASD drive - mouse aims the turret - left click fires (max 5 shells, one
ricochet each) - right click or Space lays a mine (max 2) - Esc quits.

## Tests

    mise run test

Progress (highest mission cleared) is saved to
`$XDG_DATA_HOME/tanks/progress.txt`.
