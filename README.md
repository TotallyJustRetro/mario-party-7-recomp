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

## Cruise Navigator startup

On Windows, the desktop executable opens the native Cruise Navigator disc picker before starting the game. It remembers the selected ISO/CISO/GCM path in `cruise_navigator.cfg` beside the executable. Passing a disc path directly on the command line bypasses the picker.

CMake applies the small, idempotent startup hook to the checked-out `gcn-recomp/runtime/main.cpp` before building. If the upstream entry point changes, configuration stops with a clear error instead of producing an executable that silently skips the launcher. The HTML file supplied with the launcher design is only a visual prototype; it is not connected to the native launch backend.

The Windows GUI executable redirects runtime diagnostics to `CruiseNavigator.log` beside the executable rather than opening a separate command window. A crash after selecting Launch can still indicate a separate recompilation issue; the launcher does not repair missing guest functions.
