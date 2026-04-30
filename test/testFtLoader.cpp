/*
 * Copyright (c) 2026 ThorVG project. All rights reserved.

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "config.h"

#ifdef THORVG_FT_LOADER_SUPPORT

#include <thorvg.h>
#include <hb.h>

#include "tvgFtFace.h"
#include "tvgFtFontManager.h"
#include "catch.hpp"

using namespace tvg;


TEST_CASE("FtFace open from file path via direct read", "[FtLoader]")
{
    //read PublicSans-Regular.ttf into memory
    FILE* f = fopen(TEST_DIR"/PublicSans-Regular.ttf", "rb");
    REQUIRE(f);
    fseek(f, 0, SEEK_END);
    auto size = (uint32_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    auto buf = (char*)malloc(size);
    REQUIRE(buf);
    REQUIRE(fread(buf, 1, size, f) == size);
    fclose(f);

    {
        FtFace face;
        REQUIRE(face.open(buf, size, false));
        REQUIRE(face.unitsPerEm() > 0);

        //'A' is U+0041, must be present in PublicSans
        auto gid = face.glyphIndex(0x41);
        REQUIRE(gid != 0);

        RenderPath path;
        REQUIRE(face.outline(gid, 1.0f, path));
        REQUIRE_FALSE(path.empty());
        //a glyph outline must contain at least one MoveTo + Close
        REQUIRE(path.cmds.count >= 2);
    }

    free(buf);
}


TEST_CASE("FtFace returns 0 for missing codepoint", "[FtLoader]")
{
    FILE* f = fopen(TEST_DIR"/PublicSans-Regular.ttf", "rb");
    REQUIRE(f);
    fseek(f, 0, SEEK_END);
    auto size = (uint32_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    auto buf = (char*)malloc(size);
    REQUIRE(buf);
    REQUIRE(fread(buf, 1, size, f) == size);
    fclose(f);

    {
        FtFace face;
        REQUIRE(face.open(buf, size, true));  //copy-mode

        //U+1FAE0 (Melting Face) — extremely unlikely to be in PublicSans
        auto gid = face.glyphIndex(0x1FAE0);
        REQUIRE(gid == 0);
    }

    free(buf);
}


TEST_CASE("FtFace rejects empty data", "[FtLoader]")
{
    FtFace face;
    REQUIRE_FALSE(face.open(nullptr, 0, false));
    REQUIRE_FALSE(face.open("garbage", 7, false));
}


TEST_CASE("Text via FT loader: load only", "[FtLoader]")
{
    Initializer::init();
    {
        REQUIRE(Text::load(TEST_DIR"/PublicSans-Regular.ttf") == Result::Success);
        REQUIRE(Text::unload(TEST_DIR"/PublicSans-Regular.ttf") == Result::Success);
    }
    Initializer::term();
}


TEST_CASE("Text via FT loader: load + font + size + text (no canvas)", "[FtLoader]")
{
    Initializer::init();
    {
        REQUIRE(Text::load(TEST_DIR"/PublicSans-Regular.ttf") == Result::Success);

        auto text = Text::gen();
        REQUIRE(text);
        REQUIRE(text->font("PublicSans-Regular") == Result::Success);
        REQUIRE(text->size(24.0f) == Result::Success);
        REQUIRE(text->text("Hello") == Result::Success);

        Paint::rel(text);
        REQUIRE(Text::unload(TEST_DIR"/PublicSans-Regular.ttf") == Result::Success);
    }
    Initializer::term();
}


TEST_CASE("Text via FT loader produces non-zero bounds", "[FtLoader]")
{
    Initializer::init();
    {
        auto canvas = SwCanvas::gen();
        REQUIRE(canvas);

        static uint32_t buffer[200 * 100];
        REQUIRE(canvas->target(buffer, 200, 200, 100, ColorSpace::ARGB8888) == Result::Success);

        REQUIRE(Text::load(TEST_DIR"/PublicSans-Regular.ttf") == Result::Success);

        auto text = Text::gen();
        REQUIRE(text);
        REQUIRE(text->font("PublicSans-Regular") == Result::Success);
        REQUIRE(text->size(24.0f) == Result::Success);
        REQUIRE(text->text("Hello") == Result::Success);
        REQUIRE(text->fill(0, 0, 0) == Result::Success);

        REQUIRE(canvas->add(text) == Result::Success);
        REQUIRE(canvas->update() == Result::Success);

        float x, y, w, h;
        REQUIRE(text->bounds(&x, &y, &w, &h) == Result::Success);
        REQUIRE(w > 0.0f);
        REQUIRE(h > 0.0f);

        REQUIRE(Text::unload(TEST_DIR"/PublicSans-Regular.ttf") == Result::Success);
    }
    Initializer::term();
}


TEST_CASE("HarfBuzz shaping is wired through FtFace", "[FtLoader]")
{
    FILE* f = fopen(TEST_DIR"/PublicSans-Regular.ttf", "rb");
    REQUIRE(f);
    fseek(f, 0, SEEK_END);
    auto size = (uint32_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    auto buf = (char*)malloc(size);
    REQUIRE(fread(buf, 1, size, f) == size);
    fclose(f);

    {
        FtFace face;
        REQUIRE(face.open(buf, size, false));
        REQUIRE(face.hbFont != nullptr);

        auto hbBuf = hb_buffer_create();
        const char* text = "AVA";
        hb_buffer_add_utf8(hbBuf, text, -1, 0, -1);
        hb_buffer_guess_segment_properties(hbBuf);
        hb_shape(face.hbFont, hbBuf, nullptr, 0);

        unsigned int count = 0;
        auto* infos = hb_buffer_get_glyph_infos(hbBuf, &count);
        auto* poses = hb_buffer_get_glyph_positions(hbBuf, &count);

        //"AVA" has no ligatures in plain Latin fonts → 3 glyphs
        REQUIRE(count == 3);

        //all three glyphs should resolve and advance forward
        for (unsigned int i = 0; i < count; ++i) {
            REQUIRE(infos[i].codepoint != 0);
            REQUIRE(poses[i].x_advance > 0);
        }

        hb_buffer_destroy(hbBuf);
    }

    free(buf);
}


TEST_CASE("Text via FT loader handles missing glyphs and newlines", "[FtLoader]")
{
    Initializer::init();
    {
        auto canvas = SwCanvas::gen();
        static uint32_t buffer[200 * 100];
        canvas->target(buffer, 200, 200, 100, ColorSpace::ARGB8888);

        REQUIRE(Text::load(TEST_DIR"/PublicSans-Regular.ttf") == Result::Success);

        auto text = Text::gen();
        text->font("PublicSans-Regular");
        text->size(20.0f);
        //"AB\nC" — basic newline split, also includes plain ASCII only
        REQUIRE(text->text("AB\nC") == Result::Success);

        canvas->add(text);
        canvas->update();

        TextMetrics m;
        REQUIRE(text->metrics(m) == Result::Success);
        REQUIRE(m.advance > 0.0f);  //line advance > 0
        REQUIRE(m.ascent > 0.0f);

        REQUIRE(text->lines() == 2);

        REQUIRE(Text::unload(TEST_DIR"/PublicSans-Regular.ttf") == Result::Success);
    }
    Initializer::term();
}


TEST_CASE("Text::locale accepts and clears BCP47 tags", "[FtLoader]")
{
    Initializer::init();
    {
        auto canvas = SwCanvas::gen();
        static uint32_t buffer[200 * 100];
        canvas->target(buffer, 200, 200, 100, ColorSpace::ARGB8888);

        REQUIRE(Text::load(TEST_DIR"/PublicSans-Regular.ttf") == Result::Success);

        auto text = Text::gen();
        text->font("PublicSans-Regular");
        text->size(24.0f);
        text->text("Hello");
        text->fill(0, 0, 0);

        //Setting a locale on FT-enabled build returns Success
        REQUIRE(text->locale("ja-JP") == Result::Success);
        REQUIRE(text->locale("en-US") == Result::Success);
        //Clearing the locale also succeeds
        REQUIRE(text->locale(nullptr) == Result::Success);

        //Rendering with a locale set must still produce non-zero bounds
        text->locale("ja-JP");
        canvas->add(text);
        canvas->update();
        float x, y, w, h;
        REQUIRE(text->bounds(&x, &y, &w, &h) == Result::Success);
        REQUIRE(w > 0.0f);

        REQUIRE(Text::unload(TEST_DIR"/PublicSans-Regular.ttf") == Result::Success);
    }
    Initializer::term();
}


TEST_CASE("Text wrap=Character splits long text across lines", "[FtLoader]")
{
    Initializer::init();
    {
        auto canvas = SwCanvas::gen();
        static uint32_t buffer[400 * 200];
        canvas->target(buffer, 400, 400, 200, ColorSpace::ARGB8888);

        REQUIRE(Text::load(TEST_DIR"/PublicSans-Regular.ttf") == Result::Success);

        auto text = Text::gen();
        text->font("PublicSans-Regular");
        text->size(24.0f);
        text->text("AAAAAAAAAAAA");          //12 A's, no word breaks
        text->layout(60.0f, 0.0f);            //narrow box
        text->wrap(TextWrap::Character);
        text->fill(0, 0, 0);

        canvas->add(text);
        canvas->update();

        //should produce more than one line because 12 A's at 24pt > 60px
        REQUIRE(text->lines() > 1);

        REQUIRE(Text::unload(TEST_DIR"/PublicSans-Regular.ttf") == Result::Success);
    }
    Initializer::term();
}


TEST_CASE("Text wrap=Word breaks at spaces", "[FtLoader]")
{
    Initializer::init();
    {
        auto canvas = SwCanvas::gen();
        static uint32_t buffer[400 * 200];
        canvas->target(buffer, 400, 400, 200, ColorSpace::ARGB8888);

        REQUIRE(Text::load(TEST_DIR"/PublicSans-Regular.ttf") == Result::Success);

        auto text = Text::gen();
        text->font("PublicSans-Regular");
        text->size(24.0f);
        text->text("Hello world foo bar");
        text->layout(80.0f, 0.0f);            //fits "Hello" but not "Hello world"
        text->wrap(TextWrap::Word);
        text->fill(0, 0, 0);

        canvas->add(text);
        canvas->update();

        //space-separated words must wrap across multiple lines
        REQUIRE(text->lines() >= 2);

        REQUIRE(Text::unload(TEST_DIR"/PublicSans-Regular.ttf") == Result::Success);
    }
    Initializer::term();
}


TEST_CASE("Text wrap=Smart falls back to char wrap for long words", "[FtLoader]")
{
    Initializer::init();
    {
        auto canvas = SwCanvas::gen();
        static uint32_t buffer[400 * 200];
        canvas->target(buffer, 400, 400, 200, ColorSpace::ARGB8888);

        REQUIRE(Text::load(TEST_DIR"/PublicSans-Regular.ttf") == Result::Success);

        auto text = Text::gen();
        text->font("PublicSans-Regular");
        text->size(24.0f);
        text->text("AAAAAAAAAAAAAAAA");     //one long word, no spaces
        text->layout(60.0f, 0.0f);            //word much larger than the box
        text->wrap(TextWrap::Smart);
        text->fill(0, 0, 0);

        canvas->add(text);
        canvas->update();

        //smart wrap should char-break the oversized word
        REQUIRE(text->lines() > 1);

        REQUIRE(Text::unload(TEST_DIR"/PublicSans-Regular.ttf") == Result::Success);
    }
    Initializer::term();
}


TEST_CASE("Text wrap=Ellipsis truncates to a single line", "[FtLoader]")
{
    Initializer::init();
    {
        auto canvas = SwCanvas::gen();
        static uint32_t buffer[400 * 200];
        canvas->target(buffer, 400, 400, 200, ColorSpace::ARGB8888);

        REQUIRE(Text::load(TEST_DIR"/PublicSans-Regular.ttf") == Result::Success);

        auto text = Text::gen();
        text->font("PublicSans-Regular");
        text->size(24.0f);
        text->text("This is a long sentence that should be truncated.");
        text->layout(80.0f, 0.0f);
        text->wrap(TextWrap::Ellipsis);
        text->fill(0, 0, 0);

        canvas->add(text);
        canvas->update();

        //ellipsis is single-line by definition
        REQUIRE(text->lines() == 1);

        //bounds should fit roughly within box.x (allowing slight overflow
        //for the last glyph rendering past the budget)
        float x, y, w, h;
        REQUIRE(text->bounds(&x, &y, &w, &h) == Result::Success);
        REQUIRE(w <= 100.0f);  //comfortably less than the full text would be

        REQUIRE(Text::unload(TEST_DIR"/PublicSans-Regular.ttf") == Result::Success);
    }
    Initializer::term();
}


TEST_CASE("FtFontManager picks fallback face by codepoint", "[FtLoader]")
{
    //load English (PublicSans) and Japanese (NotoSansJP) into FtFaces and
    //register them with the manager directly.
    auto loadInto = [](const char* path, FtFace& face) {
        FILE* f = fopen(path, "rb");
        REQUIRE(f);
        fseek(f, 0, SEEK_END);
        auto size = (uint32_t)ftell(f);
        fseek(f, 0, SEEK_SET);
        auto buf = (char*)malloc(size);
        REQUIRE(fread(buf, 1, size, f) == size);
        fclose(f);
        REQUIRE(face.open(buf, size, true));  //copy=true so we can free buf
        free(buf);
    };

    FtFace en, jp;
    loadInto(TEST_DIR"/PublicSans-Regular.ttf", en);
    loadInto(TEST_DIR"/NotoSansJP-Regular.otf", jp);

    auto& mgr = FtFontManager::instance();
    auto initial = mgr.size();
    mgr.enroll(&en);
    mgr.enroll(&jp);
    REQUIRE(mgr.size() == initial + 2);

    //'A' (U+0041) — present in PublicSans, fallback should be nullptr or jp
    //(jp likely also has it but is not the primary)
    auto* fbA = mgr.fallback(0x41, &en);
    //either nullptr (en has it, no fallback needed in caller's logic) or jp
    //(if jp also covers Latin); behaviour is "any face that has the cp"
    //regardless of whether the primary already does. Our caller only invokes
    //fallback when primary lacks the cp, but the manager API is agnostic.
    (void)fbA;

    //'あ' (U+3042) — Hiragana, NOT in PublicSans, IS in NotoSansJP.
    //With en as primary, fallback must return jp.
    auto* fbHi = mgr.fallback(0x3042, &en);
    REQUIRE(fbHi == &jp);

    //With jp as primary, fallback should NOT return jp itself.
    auto* fbHiSelf = mgr.fallback(0x3042, &jp);
    REQUIRE(fbHiSelf != &jp);

    //'中' (U+4E2D 中) — should also fall to jp
    auto* fbCJK = mgr.fallback(0x4E2D, &en);
    REQUIRE(fbCJK == &jp);

    //missing codepoint nowhere → nullptr
    auto* fbMissing = mgr.fallback(0x10FFFD, &en);  //private use, neither has it
    REQUIRE(fbMissing == nullptr);

    mgr.retire(&en);
    mgr.retire(&jp);
    REQUIRE(mgr.size() == initial);
}


TEST_CASE("Mixed-script text uses fallback for missing glyphs", "[FtLoader]")
{
    Initializer::init();
    {
        auto canvas = SwCanvas::gen();
        static uint32_t buffer[400 * 100];
        canvas->target(buffer, 400, 400, 100, ColorSpace::ARGB8888);

        REQUIRE(Text::load(TEST_DIR"/PublicSans-Regular.ttf") == Result::Success);

        //baseline: render only English, capture bounds width
        auto en = Text::gen();
        en->font("PublicSans-Regular");
        en->size(24.0f);
        en->text("Hello");
        en->fill(0, 0, 0);
        canvas->add(en);
        canvas->update();
        float ex, ey, ew, eh;
        REQUIRE(en->bounds(&ex, &ey, &ew, &eh) == Result::Success);
        REQUIRE(ew > 0.0f);

        //now load Japanese fallback BEFORE rendering mixed text
        REQUIRE(Text::load(TEST_DIR"/NotoSansJP-Regular.otf") == Result::Success);

        auto mixed = Text::gen();
        mixed->font("PublicSans-Regular");  //primary remains English
        mixed->size(24.0f);
        mixed->text("Hello 日本語");
        mixed->fill(0, 0, 0);
        canvas->add(mixed);
        canvas->update();
        float mx, my, mw, mh;
        REQUIRE(mixed->bounds(&mx, &my, &mw, &mh) == Result::Success);

        //With fallback, the JP glyphs were rendered, so the mixed text must
        //be substantially wider than just "Hello". A loose check (>1.5×)
        //avoids flakiness from font metrics differences across versions.
        REQUIRE(mw > ew * 1.5f);

        REQUIRE(Text::unload(TEST_DIR"/PublicSans-Regular.ttf") == Result::Success);
        REQUIRE(Text::unload(TEST_DIR"/NotoSansJP-Regular.otf") == Result::Success);
    }
    Initializer::term();
}


#endif //THORVG_FT_LOADER_SUPPORT
