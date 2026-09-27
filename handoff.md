# Handoff — restart notes for trevor-tanks

The repo was deliberately reset to `1473e66` ("Fix the macOS window corruption caused by
casting GetWindowHandle()") to restart the gamepad-input work with a fresh agent. Nothing
was lost: the previous attempt is preserved on the local branch `wip/gamepad-and-mapping`.
This file is loaded automatically via `.omp/AGENTS.md`; once you have re-landed the work,
delete this file and `.omp/AGENTS.md` in the same commit.

## 1. Do not rewrite the gamepad code — recover it

The prior attempt produced a complete, tested implementation. Recover before writing anything:

- Branch: `wip/gamepad-and-mapping` (local only, never delete it, never `git clean`).
- Tip `71a6b28` adds OpenSpec planning for a controller-mapping change on top of `4557def`.
- `4557def` is the implementation checkpoint: header-only `src/gamepad.h` (pure mapping
  core: raylib-free `PadSnapshot`, radial deadzone with rescale, logical action table,
  press-edge latch, `InputRouter` device arbitration), `tests/test_gamepad.cpp` (11 tests),
  `main.cpp` wiring, README controls. 97 tests passed, 0 failures.
- Read files with `git show wip/gamepad-and-mapping:<path>` or cherry-pick `4557def`.

## 2. Architecture constraints (breaking these breaks things silently)

- Simulation (`arena physics combat ai levels player replay` in `tanks_logic`) is
  raylib-free and unit-tested; the entire platform boundary is `InputState`
  (`src/game.h:107`): six bools (up/down/left/right/fire/mine) plus `aimWorld`.
- The replay codec packs exactly those six booleans plus two aim floats per tick
  (`src/replay.cpp`). Widening `InputState` breaks replay compatibility — if you need
  analog data, version the format, do not widen it.
- `readInput()` in `src/main.cpp` is the only `InputState` producer; `mouseToFloor()`
  (`src/render.cpp:107`) is the only aim producer.
- Fixed timestep: `DT = 1/120`, accumulator clamped at 0.25 s (`src/main.cpp`).
  Already requestAnimationFrame-friendly.
- Build/test/run: `mise run build`, `mise run test`, `mise run run`
  (CMake >= 3.16, C++17, raylib 6 via Conan).

## 3. Gamepad platform facts (verified in the linked GLFW 3.4.0 / raylib 6.0 sources)

- raylib desktop reports "available" from `glfwJoystickPresent`
  (`rcore_desktop_glfw.c:1310`) but input comes from `glfwGetGamepadState`. A pad missing
  from GLFW's embedded SDL mapping DB therefore shows as available with completely dead
  input, and raylib logs nothing. This is raylib issue #3651's family.
- GLFW matches mappings by EXACT GUID string (`findValidMapping`, `input.c:100-102`) — no
  version tolerance. `_glfwUpdateGamepadGUIDCocoa` only rewrites all-zero vendor/product
  GUIDs. A firmware update changes the GUID's version component and silently kills a
  working definition.
- `glfwUpdateGamepadMappings` re-resolves already-connected joysticks before returning
  (`input.c:1301-1308`): call `SetGamepadMappings` after `InitWindow` and a hot pad works.
- Later definition lines replace earlier ones for the same GUID (`input.c:1281`) —
  precedence is file order, no merge code needed.
- A line whose `platform:` field mismatches is rejected entirely; GUIDs are lowercased.
- `isValidElementForJoystick` rejects a whole mapping when any element index exceeds the
  device's real element counts: bad definitions fail closed AND silently.
- GLFW's macOS backend reads raw IOHID and ignores Apple's GameController framework — a
  pad macOS itself profiles (Apple synthesizes `Xbox360Controller` for the GameSir G8+)
  is still invisible to GLFW.
- Trigger shapes differ: desktop synthesizes `LEFT/RIGHT_TRIGGER_2` buttons from trigger
  axes above 0.1 (`rcore_desktop_glfw.c:1382-95`) and rests those axes at `-1`; the web
  core (`rcore_web.c`) forwards only Emscripten's digital buttons and applies no stick
  deadzone. Accept both shapes, normalize with `(x + 1) * 0.5`, and own the deadzone.

## 4. The GameSir G8+ (the pad that started this)

- Bluetooth "GameSir-G8+", vendor `0x3537`, product `0x1108`, firmware 2.12,
  GLFW GUID `030000003735000008110000db070000` (last component is the reported version),
  30 raw buttons / 6 axes, reports `present=1 isGamepad=0`.
- Absent from GLFW 3.4.0 `mappings.h` (zero vendor-`0x3537` rows), GLFW master, and SDL's
  `gamecontrollerdb.txt` (its only macOS GameSir rows are T3 2.02 and X4A). There is no
  published mapping; one must be captured or hand-written.
- It ships as paired-but-"Not Connected" until powered on, and only iOS mode uses Apple's
  standard gamepad profile. Check `system_profiler SPBluetoothDataType` before blaming code.

## 5. Diagnostic recipe (reusable)

- Connection state: `system_profiler SPBluetoothDataType` (look for "Not Connected").
- HID presence by vendor id: `ioreg -r -c IOHIDDevice`.
- Decisive check: a throwaway probe calling `glfwJoystickIsGamepad` / `glfwGetJoystickGUID`,
  compiled against the project's own Conan libs:

  ```sh
  R=/Users/texas/.conan2/p/b/rayli5f6a143e7a625/p
  G=/Users/texas/.conan2/p/b/glfwcff061c1cf7b0/p
  clang++ -std=c++17 -I"$R/include" probe.cpp "$R/lib/libraylib.a" -L"$G/lib" -lglfw3 \
    -framework Cocoa -framework IOKit -framework CoreVideo -framework OpenGL \
    -framework CoreFoundation -framework CoreAudio -framework AudioToolbox -o probe
  ```

- Source of truth to grep: GLFW 3.4.0 at
  `/Users/texas/.conan2/p/glfwe704f140af55b/s/src/src`, raylib 6.0 at
  `/Users/texas/.conan2/p/rayli6476be4a44f24/s/src/src`.

## 6. If the web port is next

`-DPLATFORM=Web` makes raylink's CMake add `-sUSE_GLFW=3 -sEXPORTED_RUNTIME_METHODS=ccall`;
WebGL2 needs `GRAPHICS_API_OPENGL_ES3`. Conan's raylib/glfw do not extend to wasm — use
`emcmake` plus raylib from source. Blockers found: `src/models.cpp` hardcodes GLSL
`#version 330` (needs `#version 300 es` + precision qualifier); `main()`'s
`while (!WindowShouldClose())` must become `emscripten_set_main_loop`, which unwinds the
stack so `Session`/`Renderer`/buffers must move to static or heap storage; `std::ofstream`
progress writes land in MEMFS and vanish on reload (IDBFS or localStorage); GitHub Pages
cannot set COOP/COEP headers, so no SharedArrayBuffer/pthreads; unhashed `index.js`/`.wasm`
names risk stale-cache mismatches.

## 7. OpenSpec notes

A prior attempt used `openspec` (CLI 1.13): `openspec new change <name>` scaffolds
`openspec/changes/<name>/` with required `.openspec.yaml`; artifacts proposal → specs →
design → tasks via `openspec instructions <artifact> --change <name> --json`; validate with
`openspec validate <name>`. A capability's requirements become durable only at
`openspec archive`, so until then a later change cannot write a MODIFIED delta against it
and must declare its own capability. Multiple in-progress changes coexist fine.
