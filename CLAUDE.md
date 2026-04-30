# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Repository context

This is a fork of [thorvg/thorvg](https://github.com/thorvg/thorvg) — a C++14 vector graphics engine (SVG / Lottie / fonts / shapes) with software, OpenGL, and WebGPU backends.

**This fork's purpose**: the upstream project builds with Meson only. The `cmake` branch (currently checked out) adds a parallel CMake build system that links via vcpkg manifest mode. Both build systems coexist — when adding/removing source files, **update both `meson.build` files and the top-level `CMakeLists.txt`** or one build will silently break. Fork-specific docs are in `README_cmake.md` (Japanese).

`機能追加.txt` is the user's working note (Japanese) for the FreeType + HarfBuzz multilingual text feature. **This feature is now implemented** in `src/loaders/ft/` (commits `a954053`, `3f06c56`) — see the FT loader section below. The note is kept in-tree as historical spec but is untracked. Reference implementation that informed the design lives at `../richtext` (outside this repo).

### FT loader (FreeType + HarfBuzz)

Compile-time alternative to the built-in TTF loader, mutually exclusive via CMake (`TVG_LOADER_FT` vs `TVG_LOADER_TTF`). Enable with `-DTVG_LOADER_FT=ON -DTVG_LOADER_TTF=OFF -DVCPKG_MANIFEST_FEATURES=freetype`. Provides:

- **`src/loaders/ft/tvgFtFace`**: FT_Face + companion `hb_font_t` wrapper. Outline extraction (FT_LOAD_NO_SCALE, quadratic→cubic Bezier conversion, Y-axis flip into thorvg's y-down convention). `hb_font` scale is locked to font units so HB advances match outline coords without per-call rescaling.
- **`src/loaders/ft/tvgFtFontManager`**: process-wide registry. `Text::load()` order = fallback priority order. `fallback(codepoint, primary)` returns the first registered face other than `primary` that has the codepoint.
- **`src/loaders/ft/tvgFtLoader`**: `FontLoader` impl. Each source line is run-segmented by face, each run is shaped independently with HB, outputs are normalized to primary font units via `unitScale = primaryUpem / runUpem` so mixed-UPM fonts compose correctly. All four wrap modes (Character/Word/Smart/Ellipsis) are ported and operate on the post-shape `Glyph[]` stream — word-break detection uses HB cluster values to look up source whitespace.
- **`Text::locale(const char*)`** in `inc/thorvg.h`: BCP47 tag for HB language-sensitive shaping (CJK character variants). Returns `Result::NonSupport` on TTF builds. Marked as fork extension in the doc comment; not in upstream.
- **`tools/ft-text-sample/`**: a small executable (built when `TVG_LOADER_FT=ON`) that loads a primary + fallback font and renders mixed-language text to a PNG via lodepng. Useful for visual smoke-testing.

Tests live in `test/testFtLoader.cpp` under the `[FtLoader]` Catch tag. They poke `FtFace` and `FtFontManager` directly via private include paths added in the test target's CMake config — when you add a new internal class, add its include to that block too.

**Out of scope (deferred):** color emoji (sbix/CBDT bitmap and COLRv1 vector — would require restructuring `TextImpl` from `Shape*` to a `Scene` with mixed Shape + Picture children), meson-side build wiring, C API binding for `Text::locale`, and Android cross-toolchain config.

## Build

Two build systems produce the same library. Pick one per build directory.

### Meson (upstream-canonical)
```
meson setup builddir [-Dloaders=svg,lottie,ttf,png,jpg,webp] [-Dengines=sw,gl,wg] \
                     [-Dsavers=gif] [-Dbindings=capi] [-Dtools=svg2png,lottie2gif] \
                     [-Dtests=true] [-Dlog=true]
ninja -C builddir
ninja -C builddir test       # if -Dtests=true
ninja -C builddir install
```
Options live in `meson_options.txt`. CI uses `meson setup build -Dtests=true -Dloaders=all -Dsavers=all -Dbindings=capi -Dtools=all -Dlog=true -Db_sanitize=address,undefined`.

### CMake (this fork)
```
cmake -B build -DCMAKE_BUILD_TYPE=Release [-DTVG_LOADER_PNG=ON ...]
cmake --build build --config Release
ctest --test-dir build       # if -DTVG_BUILD_TESTS=ON
```
Or via vcpkg presets in `CMakePresets.json` (requires `VCPKG_ROOT` env): `cmake --preset x64-windows && cmake --build --preset x64-windows`. Presets default to `TVG_BUILD_SHARED=OFF`, GL+GLES with `TVG_GL_INITPROC=ON`, CAPI bindings on, SIMD on. Option names mirror Meson with a `TVG_` prefix (`TVG_ENGINE_SW`, `TVG_LOADER_LOTTIE`, etc.; see top of `CMakeLists.txt`).

### Run a single unit test
Tests use Catch2 v1 (vendored as `test/catch.hpp`). Filter by tag:
```
./builddir/test/tvgUnitTests "[Shape]"        # by tag
./builddir/test/tvgUnitTests -# "TestName"    # by name
```
`TEST_DIR` is baked in at compile time and points to `test/resources/`.

## Architecture

ThorVG is a **scene-graph + pluggable backends** engine. Public API is a single header `inc/thorvg.h` using PIMPL throughout (`pImpl` pointer per class, real types in `src/renderer/tvg*.h`). The C ABI in `src/bindings/capi/` is a thin wrapper over this.

### Layered structure (top → bottom)
- **`src/renderer/`** — engine-agnostic core. `Paint` is the base of the scene graph; `Shape`, `Picture`, `Scene`, `Text` derive from it. `Canvas` owns paints, `Initializer` manages global lifecycle, `TaskScheduler` runs the optional threadpool, `Loader`/`Saver` dispatch to format modules.
- **`src/renderer/{sw,gl,wg}_engine/`** — three render backends. Each implements the `RenderMethod` interface (`tvgRender.h`). SW is the reference; GL/WG mirror it. Backend selection happens in `Initializer` based on `CanvasEngine` enum.
- **`src/loaders/`** — format readers, each behind `LoadModule`. `svg/`, `lottie/`, `ttf/`, `raw/` are always built-in source. `png/`, `jpg/`, `webp/` ship **dual implementations**: an internal decoder (lodepng / jpgd / vendored libwebp) under e.g. `src/loaders/png/`, and an `external_*` adapter that calls the system library. The build picks one at configure time via `TVG_STATIC_MODULES` / availability of `find_package(PNG)` etc.
- **`src/loaders/lottie/jerryscript/`** — vendored ECMAScript engine for Lottie Expressions. Compiled in only when `TVG_LOTTIE_EXPRESSIONS=ON` and Lottie loader enabled. Big footprint; disable for MCU targets.
- **`src/loaders/lottie/rapidjson/`** — vendored JSON parser, header-only.
- **`src/savers/gif/`** — only saver currently. Pulled in transitively by the `lottie2gif` tool.
- **`src/common/`** — `Array`, `Inlist` (intrusive list), `Lock`, `Allocator`, `Math`, `Color`, `Str`, `Compressor`. **No STL containers in hot paths** — code consistently uses `tvg::Array<T>`. Match this when adding code.
- **`src/bindings/capi/`** — C API in `thorvg_capi.h`. New C++ APIs that should be exposed need a corresponding `tvg_*` C wrapper here.
- **`tools/`** — `svg2png` (uses thorvg + bundled lodepng) and `lottie2gif` (uses thorvg + GIF saver). They link `thorvg` as a normal consumer.

### Things that aren't obvious from grepping
- **Compile flags are deliberately strict**: `-fno-exceptions -fno-rtti -fno-stack-protector -fno-math-errno -fno-unwind-tables`. Do not introduce `try`/`throw`/`dynamic_cast`/`typeid` — they will not compile on non-MSVC. Errors propagate via `tvg::Result`.
- **Symbol visibility is hidden by default** (`CXX_VISIBILITY_PRESET hidden`). Public symbols must be marked with the `TVG_API` macro from `thorvg.h`. Internal functions with external linkage will not be reachable from tests/tools.
- **`TVG_BUILD_SHARED=OFF` requires `TVG_STATIC` to be defined on consumers** — the CMake build does this for `tvg-svg2png`, `tvg-lottie2gif`, and `tvgUnitTests` automatically; if you add a new consumer, do the same.
- **`config.h` is generated** (`cmake/config.h.in` → `${BUILD}/config.h`, or Meson equivalent). Feature toggles like `THORVG_LOTTIE_LOADER_SUPPORT` come from there — do not hand-define them.
- **GL backend has two ways to load GL function pointers**: ThorVG's own loader (default), or `tvg::glInitProc()` when built with `TVG_GL_INITPROC=ON`. The fork's preset default is the latter — designed for GLFW/SDL embedding where the host already has a context.
- **Text rendering today** goes through `tvgText.cpp` + the TTF loader (`src/loaders/ttf/`) using ThorVG's own minimal TTF reader. The planned FreeType+HarfBuzz path (see `機能追加.txt`) is meant to be a compile-time alternative, not a replacement — keep both paths buildable.

## Conventions

- **Format with clang-format 18** before submitting (`.clang-format` at repo root). Helper: `./tvg-format.sh <git-sha>` then `git commit --amend`.
- **Commit message format**: `[Module][Feature]: Title` then a body. Module = subfolder name in lowercase (`sw_engine`, `svg_loader`, `build`, `common`, ...). Feature is optional. Examples: `[lottie_loader][parser]: Fixed null deref on empty layers`.
- **No new STL container in render hot paths** — use `tvg::Array<T>` from `src/common/`.
- **Files use `tvg` prefix** for all internal sources (`tvgFoo.cpp`, `tvgFoo.h`). Match this when adding new files.
