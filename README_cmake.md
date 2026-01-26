# ThorVG CMake ビルドガイド

このドキュメントでは、CMake を使用して ThorVG をビルドする方法について説明します。

## 必要条件

- **CMake**: バージョン 3.14 以上
- **C++ コンパイラ**: C++14 をサポートするコンパイラ
  - MSVC 2017 以上
  - GCC 5.0 以上
  - Clang 3.4 以上
- **オプション依存関係**:
  - OpenGL/OpenGL ES (GL エンジン使用時)
  - WebGPU (WG エンジン使用時)
  - libpng (PNG ローダー使用時)
  - libjpeg/libjpeg-turbo (JPG ローダー使用時)
  - libwebp (WebP ローダー使用時)

## クイックスタート

### 基本的なビルド

```bash
# ビルドディレクトリを作成して構成
cmake -B build -DCMAKE_BUILD_TYPE=Release

# ビルド
cmake --build build --config Release
```

### インストール

```bash
cmake --install build --prefix /usr/local
```

## ビルドオプション

CMake のオプションは `-D` フラグを使用して設定します。

### 基本オプション

| オプション | デフォルト | 説明 |
|------------|-----------|------|
| `TVG_BUILD_SHARED` | `ON` | 共有ライブラリ (.dll/.so/.dylib) をビルド。`OFF` の場合は静的ライブラリ |
| `TVG_THREADS` | `ON` | マルチスレッドタスクスケジューラを有効化 |
| `TVG_PARTIAL` | `ON` | 部分レンダリングを有効化 |
| `TVG_SIMD` | `OFF` | CPU ベクトル化 (AVX/NEON) を有効化 |
| `TVG_LOG` | `OFF` | ログメッセージを有効化 |
| `TVG_FILE_IO` | `ON` | ファイル I/O 操作を有効化 |

### エンジンオプション

| オプション | デフォルト | 説明 |
|------------|-----------|------|
| `TVG_ENGINE_SW` | `ON` | ソフトウェアラスタライザエンジン |
| `TVG_ENGINE_GL` | `OFF` | OpenGL ラスタライザエンジン |
| `TVG_ENGINE_WG` | `OFF` | WebGPU ラスタライザエンジン |

### ローダーオプション

| オプション | デフォルト | 説明 |
|------------|-----------|------|
| `TVG_LOADER_SVG` | `ON` | SVG ファイルローダー |
| `TVG_LOADER_PNG` | `OFF` | PNG ファイルローダー (内蔵デコーダー使用) |
| `TVG_LOADER_JPG` | `OFF` | JPEG ファイルローダー (内蔵デコーダー使用) |
| `TVG_LOADER_LOTTIE` | `ON` | Lottie (JSON) アニメーションローダー |
| `TVG_LOADER_TTF` | `ON` | TrueType フォントローダー |
| `TVG_LOADER_WEBP` | `OFF` | WebP ファイルローダー (内蔵デコーダー使用) |

### セーバーオプション

| オプション | デフォルト | 説明 |
|------------|-----------|------|
| `TVG_SAVER_GIF` | `OFF` | GIF 形式へのエクスポート機能 |

### バインディングオプション

| オプション | デフォルト | 説明 |
|------------|-----------|------|
| `TVG_BINDINGS_CAPI` | `OFF` | C 言語 API バインディング |

### ツールオプション

| オプション | デフォルト | 説明 |
|------------|-----------|------|
| `TVG_TOOL_SVG2PNG` | `OFF` | svg2png 変換ツールをビルド |
| `TVG_TOOL_LOTTIE2GIF` | `OFF` | lottie2gif 変換ツールをビルド |

### 追加オプション

| オプション | デフォルト | 説明 |
|------------|-----------|------|
| `TVG_LOTTIE_EXPRESSIONS` | `ON` | Lottie Expressions (JavaScript) サポート |
| `TVG_OPENMP` | `ON` | OpenMP による並列処理 |
| `TVG_OPENGL_ES` | `OFF` | OpenGL の代わりに OpenGL ES を使用 |
| `TVG_BUILD_TESTS` | `OFF` | ユニットテストをビルド |
| `TVG_STATIC_MODULES` | `OFF` | 静的リンクモジュールを強制 |

## プラットフォーム別ビルド

### Windows (Visual Studio)

```bash
# Visual Studio 2022 用のプロジェクトファイルを生成
cmake -B build -G "Visual Studio 17 2022" -A x64

# リリースビルド
cmake --build build --config Release

# デバッグビルド
cmake --build build --config Debug
```

Visual Studio IDE から直接開く場合:
```bash
cmake -B build -G "Visual Studio 17 2022"
# build/thorvg.sln を Visual Studio で開く
```

### Windows (MinGW)

```bash
cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Linux

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

### macOS

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(sysctl -n hw.ncpu)
```

Xcode プロジェクトを生成する場合:
```bash
cmake -B build -G Xcode
# build/thorvg.xcodeproj を Xcode で開く
```

## ビルド例

### 最小構成 (SW エンジンのみ)

```bash
cmake -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DTVG_ENGINE_SW=ON \
    -DTVG_LOADER_SVG=OFF \
    -DTVG_LOADER_LOTTIE=OFF \
    -DTVG_LOADER_TTF=OFF
```

### Lottie プレイヤー用構成

```bash
cmake -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DTVG_LOADER_LOTTIE=ON \
    -DTVG_LOTTIE_EXPRESSIONS=ON \
    -DTVG_LOADER_PNG=ON \
    -DTVG_LOADER_JPG=ON \
    -DTVG_LOADER_WEBP=ON
```

### 高パフォーマンス構成 (SIMD 有効)

```bash
cmake -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DTVG_SIMD=ON \
    -DTVG_THREADS=ON \
    -DTVG_OPENMP=ON
```

### 全機能有効化

```bash
cmake -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DTVG_ENGINE_SW=ON \
    -DTVG_LOADER_SVG=ON \
    -DTVG_LOADER_PNG=ON \
    -DTVG_LOADER_JPG=ON \
    -DTVG_LOADER_LOTTIE=ON \
    -DTVG_LOADER_TTF=ON \
    -DTVG_LOADER_WEBP=ON \
    -DTVG_SAVER_GIF=ON \
    -DTVG_BINDINGS_CAPI=ON \
    -DTVG_SIMD=ON
```

### ツールのビルド

```bash
# svg2png ツール
cmake -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DTVG_TOOL_SVG2PNG=ON \
    -DTVG_LOADER_PNG=ON

# lottie2gif ツール
cmake -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DTVG_TOOL_LOTTIE2GIF=ON \
    -DTVG_SAVER_GIF=ON
```

### 静的ライブラリとしてビルド

```bash
cmake -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DTVG_BUILD_SHARED=OFF
```

### デバッグ用構成

```bash
cmake -B build \
    -DCMAKE_BUILD_TYPE=Debug \
    -DTVG_LOG=ON \
    -DTVG_BUILD_TESTS=ON
```

## インストール

### システムへのインストール

```bash
# ビルド後
cmake --install build

# インストール先を指定
cmake --install build --prefix /opt/thorvg
```

### インストールされるファイル

- **ヘッダーファイル**: `<prefix>/include/thorvg.h`
- **ライブラリ**: 
  - 共有: `<prefix>/lib/thorvg-1.dll` (Windows), `libthorvg.so.1` (Linux), `libthorvg.1.dylib` (macOS)
  - 静的: `<prefix>/lib/thorvg.lib` (Windows), `libthorvg.a` (Linux/macOS)
- **pkg-config**: `<prefix>/lib/pkgconfig/thorvg.pc`
- **CMake パッケージ**: `<prefix>/lib/cmake/thorvg/`

## プロジェクトへの統合

### CMake で find_package を使用

```cmake
find_package(thorvg REQUIRED)
target_link_libraries(your_target PRIVATE thorvg::thorvg)
```

### CMake で add_subdirectory を使用

```cmake
add_subdirectory(thorvg)
target_link_libraries(your_target PRIVATE thorvg)
```

### pkg-config を使用

```bash
pkg-config --cflags --libs thorvg
```

### 手動リンク

```cmake
# Windows
target_link_libraries(your_target PRIVATE thorvg-1)

# Linux/macOS
target_link_libraries(your_target PRIVATE thorvg)
```

