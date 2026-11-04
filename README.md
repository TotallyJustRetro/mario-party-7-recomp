# Mario Party 7 Recompiled

A native PC port of Mario Party 7 (GameCube, USA — GP7E01) built on [gcn-recomp](https://github.com/ethannicholas/gcn-recomp).

**No game data is included.** You need your own disc image (RVZ, ISO, or CISO).

## Building (Windows)

```
git clone --recurse-submodules https://github.com/TotallyJustRetro/mario-party-7-recomp
cd mario-party-7-recomp
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DMP7_DISC=C:/path/to/your/game.rvz
cmake --build build
```

## Building (Linux)

```
git clone --recurse-submodules https://github.com/TotallyJustRetro/mario-party-7-recomp
cd mario-party-7-recomp
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DMP7_DISC=/path/to/your/game.rvz
cmake --build build
```

## Requirements

- CMake 3.20+, Ninja, Python 3, a C++20 compiler
- SDL2 (Linux: `sudo apt install libsdl2-dev`; Windows: built automatically)
- Your own Mario Party 7 USA disc image
