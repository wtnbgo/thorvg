# FreeType + HarfBuzz 多言語テキスト拡張

このドキュメントは、本フォーク（cmake ブランチ）が ThorVG に追加した **FreeType + HarfBuzz ベースの多言語テキスト描画機能** について説明します。upstream には存在しないフォーク独自の拡張です。

## 概要

ThorVG 標準の TTF ローダーは内蔵の最小 TrueType リーダーを使うため、合字や複雑な文字、フォールバックフォントには対応していません。本拡張は **コンパイル時に切り替えられる代替ローダー** として、以下を提供します:

- **HarfBuzz シェイピング**: 合字（`fi` / `fl` 等）、kerning、複雑文字（アラビア語・インド系）、CJK の正しい字形選択
- **コードポイント単位の自動フォールバック**: プライマリフォントに収録されていない文字を、ロード済みの他フォントから自動補完
- **混在 UPM の正規化**: 異なる units-per-EM のフォントを混在させても出力スケールが破綻しない
- **BCP47 ロケール指定**: 日本語/中国語簡体/中国語繁体/韓国語などの字形バリエーション切替
- **wrap 4 モードの完全移植**: TTF ローダーと同じ `Character` / `Word` / `Smart` / `Ellipsis` を全部サポート

既存の TTF ローダーとは **CMake 時点で相互排他**（両方 `ON` は `FATAL_ERROR`）になっており、用途に応じてビルド時に選択します。

## 必要条件

- **vcpkg**: 本フォークの CMake ビルドは vcpkg manifest モード前提。`VCPKG_ROOT` 環境変数の設定が必要
- **freetype**: vcpkg feature `freetype` で自動取得
- **harfbuzz**: 同 feature で freetype と一緒に自動取得

C++ コンパイラ要件は本体 ThorVG と同じ（C++14）。

## ビルド設定

### CMake オプション

| オプション | デフォルト | 説明 |
|---|---|---|
| `TVG_LOADER_FT` | `OFF` | FreeType + HarfBuzz テキストローダーを有効化 |
| `TVG_LOADER_TTF` | `ON` | 内蔵 TTF ローダー（`TVG_LOADER_FT` と相互排他） |

### vcpkg manifest features

`vcpkg.json` で定義済み:

| feature | 内容 |
|---|---|
| `freetype` | `freetype` + `harfbuzz` を依存に追加 |

### configure コマンド例

```bash
cmake -B build/ft --preset x64-windows \
  -DTVG_LOADER_TTF=OFF \
  -DTVG_LOADER_FT=ON \
  -DVCPKG_MANIFEST_FEATURES="freetype"
cmake --build build/ft --config Release
```

両方 `ON` にした場合は configure 時点でエラーになります:

```
CMake Error: TVG_LOADER_TTF and TVG_LOADER_FT are mutually exclusive.
```

## 基本的な使い方

公開 API は ThorVG 標準の `Text` クラスをそのまま使います。FT ローダーが有効なビルドでは `Text::load()` / `Text::font()` / `Text::text()` / `Text::size()` が自動的に FreeType + HarfBuzz 経由になります。

```cpp
#include <thorvg.h>

tvg::Initializer::init();

auto canvas = tvg::SwCanvas::gen();
canvas->target(buffer, w, w, h, tvg::ColorSpace::ARGB8888);

// 1) フォントをロード（拡張子 .ttf .ttc .otf .otc 全て対応）
tvg::Text::load("PublicSans-Regular.ttf");

// 2) Text を生成
auto text = tvg::Text::gen();
text->font("PublicSans-Regular");   // 拡張子なしのファイル名
text->size(36);
text->text("Hello, world!");
text->fill(0, 0, 0);
text->translate(20, 60);
canvas->add(text);

canvas->draw();
canvas->sync();
```

`Text::font()` の引数は **拡張子を除いたファイル名**（thorvg 標準の登録キー仕様）です。

## フォールバック / 多言語混在

複数のフォントを `Text::load()` でロードしておくと、プライマリフォントに無い文字は **ロード順に他のフォントを試して自動補完** します。

```cpp
// 順序が優先度: 英語フォントを先、次に日本語フォント
tvg::Text::load("PublicSans-Regular.ttf");
tvg::Text::load("NotoSansJP-Regular.otf");

auto text = tvg::Text::gen();
text->font("PublicSans-Regular");   // プライマリ
text->size(36);
text->text("日本語 + English 混在 テスト");  // 仮名/漢字は NotoSansJP に自動切替
canvas->add(text);
```

仕組み:

1. `Text::text()` の文字列を 1 文字（コードポイント）ずつ走査
2. プライマリに無い文字は `FtFontManager` が登録済みの他フォントを順に検索
3. 同じフォントに解決される連続区間を 1 つの **run** としてまとめ、HarfBuzz で独立にシェイピング
4. 各 run の advance / offset / outline は `unitScale = primaryUpem / runUpem` で正規化されて連結

これにより、フォント間の UPM が違っても（例: NotoEmoji の UPM=2048 vs PublicSans の UPM=1000）レイアウトが破綻しません。

## ロケール（`Text::locale`）

```cpp
text->locale("ja-JP");   // BCP47 タグ
```

HarfBuzz の `locl` GSUB feature 等を介して、同じ Unicode コードポイントでも言語ごとに異なる字形を選びます（例: `直`, `骨`, `次` など日本語/簡体字/繁体字で形が違う字）。`nullptr` を渡すと解除。

| ビルド | 戻り値 |
|---|---|
| FT ローダー有効時 | `Result::Success` |
| TTF ローダー有効時 | `Result::NonSupport` |

`inc/thorvg.h` に追加された fork 拡張 API です。upstream にはありません。

## Wrap モード

`Text::layout(width, height)` で box を指定し `Text::wrap(mode)` で改行ポリシーを選びます:

| `TextWrap` | 動作 |
|---|---|
| `None` | 折り返しなし。1 行で box を超えても描画 |
| `Character` | 文字単位で折り返し |
| `Word` | 半角空白 / タブで単語境界折り返し。語が box より大きい場合は 1 行に収めようとして overflow を許容 |
| `Smart` | `Word` と同じだが、語が box より大きい場合は文字単位 fallback |
| `Ellipsis` | 1 行に固定。box を超える前に末尾を `"..."` で切り詰め |

`box.x == 0`（横方向の制約なし）にすると wrap モードに関わらず `None` 相当になります。明示的な `'\n'` は全モードで強制改行として扱われます。

```cpp
text->layout(300.0f, 0.0f);          // 横 300px、縦は制約なし
text->wrap(tvg::TextWrap::Word);
text->text("Hello world. This is a long sentence.");
```

## サンプル: PNG への日本語混在描画

`tools/ft-text-sample/` に小さな実行ファイルを同梱しています。`TVG_LOADER_FT=ON` で自動的にビルドされます。

```bash
# ビルド
cmake --build build/ft --config Release --target tvg-ft-text-sample

# 実行: 800x200 の PNG に 3 行のテキストを書き出す
./build/ft/Release/tvg-ft-text-sample.exe \
  test/resources/PublicSans-Regular.ttf \
  test/resources/NotoSansJP-Regular.otf \
  out.png
```

このサンプルは:
- 第 1 引数のフォントをプライマリ、第 2 引数をフォールバックとしてロード
- 純粋 ASCII / 日本語混在 / フル混在 の 3 行をレンダリング
- `Text::locale("ja-JP")` を各テキストに設定
- ARGB8888 → RGBA に変換して lodepng で PNG エンコード

混在描画の視覚的な動作確認に便利です。

## 内部構造（参考）

実装ファイル:

```
src/loaders/ft/
├── tvgFtFace.{h,cpp}         FT_Face + hb_font_t ラッパ、outline 抽出
├── tvgFtFontManager.{h,cpp}  プロセスワイドのフォント登録 + フォールバック検索
└── tvgFtLoader.{h,cpp}       FontLoader 実装、HB シェイピング、wrap policy
```

- `FT_LOAD_NO_SCALE` で常に font units 単位で outline を取得し、出力時に `fm.scale = unitsPerEm / (fontSize * DPI)` で換算
- `hb_font_set_scale(font, upem, upem)` で HB の advance も font units に固定し、outline と単位を完全一致させる
- 行内テキストは `\n` で source line に分割し、各 source line を **face 別 run** にさらに分割して HB に渡す
- wrap policy は シェイプ後の `Glyph[]` ストリーム（face / gid / xAdv / xOff / yOff / cluster）を見て決定。HB の cluster 値から元 byte の空白判定で word break を検出

詳細はコード冒頭のコメントと `CLAUDE.md` の "FT loader" セクションを参照。

## テスト

`test/testFtLoader.cpp` の `[FtLoader]` タグに 15 ケース / 97 アサーション:

```bash
./build/ft/Release/tvgUnitTests.exe "[FtLoader]"
```

主な検証内容:
- `FtFace` 直接 API（outline 抽出、欠落コードポイント、不正データ拒否）
- HarfBuzz シェイピングが face に到達してグリフ ID を返すこと
- `FtFontManager` の登録順優先・フォールバック解決
- 公開 API 経由の英語+日本語混在描画（bounds 比較で fallback の効果検証）
- 4 つの wrap モード（`Character` / `Word` / `Smart` / `Ellipsis`）
- `Text::locale()` の Success / NonSupport 切替

テスト用フォント（`test/resources/`）:
- `PublicSans-Regular.ttf` — 既存。英語プライマリ用
- `NotoSansJP-Regular.otf` — 本拡張で追加。日本語フォールバック検証用
- `NotoEmoji-Regular.ttf` — 本拡張で追加。将来の絵文字対応検証用（現状はスコープ外）

## スコープ外（将来課題）

以下は意図的に未対応です:

| 項目 | 理由 |
|---|---|
| カラー絵文字（PNG 系: sbix / CBDT、ベクター: COLRv1） | `TextImpl` の構造を `Shape*` 単独 → `Scene` + `Shape` / `Picture` 子の構成に変える破壊的改修が必要 |
| meson 側のビルド配線 | フォークの cmake ブランチ専用機能。upstream に上げるタイミングで対応予定 |
| C API バインディング（`tvg_text_locale` 等） | C++ 側のみで先行。要望に応じて追加 |
| Android クロスビルド検証 | vcpkg android triplet で freetype/harfbuzz の動作未確認 |

未収録グリフ（フォールバック先にも無い文字）は黙ってスキップされます（advance のみ消費して .notdef は描画しない）。

## upstream との関係

本拡張は upstream の ThorVG プロジェクトには含まれません。`Text::locale()` API も fork 独自です。upstream へ提案する場合は以下の調整が必要になります:

- meson オプション側の対応
- C API バインディングの追加
- 絵文字対応の方針合意（`TextImpl` 構造変更を含むか）

## 関連コミット

- `a954053` `[ft_loader]: Add FreeType + HarfBuzz multilingual text loader` — ローダー本体（Phase 0-6）
- `3f06c56` `[ft_loader][sample]: Add Japanese-rendering PNG sample` — サンプル追加
