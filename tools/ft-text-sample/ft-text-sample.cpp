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

//Sample: render mixed-language text (English + Japanese) via the FreeType +
//HarfBuzz loader, then encode the canvas to a PNG using lodepng.
//
//Usage:
//   tvg-ft-text-sample <english.ttf> <japanese.otf> <output.png>
//
//The first font is the primary; the second is registered as a fallback.
//Codepoints absent from the primary fall through to the secondary.

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <thorvg.h>
#include "lodepng.h"

constexpr int WIDTH = 800;
constexpr int HEIGHT = 200;


//Convert ARGB8888 (thorvg native) to RGBA bytes for lodepng.
static void argbToRgba(const uint32_t* in, unsigned char* out, int w, int h)
{
    for (int i = 0; i < w * h; ++i) {
        uint32_t n = in[i];
        out[i * 4 + 0] = (n >> 16) & 0xff;
        out[i * 4 + 1] = (n >> 8) & 0xff;
        out[i * 4 + 2] = n & 0xff;
        out[i * 4 + 3] = (n >> 24) & 0xff;
    }
}


static const char* basename(const char* path)
{
    auto p = path + strlen(path);
    while (p > path && p[-1] != '/' && p[-1] != '\\') --p;
    return p;
}


//Strip directory and extension to get the font registration name used by
//Text::font(). thorvg keys file-loaded fonts by basename without extension.
static void fontKey(const char* path, char* dst, size_t cap)
{
    auto base = basename(path);
    size_t n = 0;
    while (base[n] && base[n] != '.' && n + 1 < cap) {
        dst[n] = base[n];
        ++n;
    }
    dst[n] = '\0';
}


int main(int argc, char** argv)
{
    if (argc < 4) {
        std::fprintf(stderr,
            "Usage: %s <primary-font> <fallback-font> <output.png>\n"
            "Example:\n"
            "  %s test/resources/PublicSans-Regular.ttf "
            "test/resources/NotoSansJP-Regular.otf out.png\n",
            argv[0], argv[0]);
        return 1;
    }

    const char* primaryPath = argv[1];
    const char* fallbackPath = argv[2];
    const char* outPath = argv[3];

    static uint32_t buffer[WIDTH * HEIGHT];
    //White background (ARGB)
    for (int i = 0; i < WIDTH * HEIGHT; ++i) buffer[i] = 0xffffffff;

    if (tvg::Initializer::init() != tvg::Result::Success) {
        std::fprintf(stderr, "Initializer::init failed\n");
        return 2;
    }

    auto canvas = tvg::SwCanvas::gen();
    if (!canvas) {
        std::fprintf(stderr, "SwCanvas::gen failed\n");
        return 2;
    }
    canvas->target(buffer, WIDTH, WIDTH, HEIGHT, tvg::ColorSpace::ARGB8888);

    //Order matters: primary first, fallback second.
    if (tvg::Text::load(primaryPath) != tvg::Result::Success) {
        std::fprintf(stderr, "Failed to load primary font: %s\n", primaryPath);
        return 3;
    }
    if (tvg::Text::load(fallbackPath) != tvg::Result::Success) {
        std::fprintf(stderr, "Failed to load fallback font: %s\n", fallbackPath);
        return 3;
    }

    char primaryKey[256];
    fontKey(primaryPath, primaryKey, sizeof(primaryKey));

    //Three text lines demonstrating mixed-script rendering.
    const char* samples[] = {
        "Hello, world!",
        "こんにちは、ThorVG。",
        "日本語 + English 混在 テスト",
    };

    float y = 20.0f;
    for (auto* s : samples) {
        auto t = tvg::Text::gen();
        t->font(primaryKey);
        t->size(36.0f);
        t->text(s);
        t->fill(0, 0, 0);
        t->locale("ja-JP");
        t->translate(20.0f, y);
        canvas->add(t);
        y += 56.0f;
    }

    canvas->draw();
    canvas->sync();

    auto rgba = (unsigned char*)std::malloc(WIDTH * HEIGHT * 4);
    if (!rgba) {
        std::fprintf(stderr, "malloc failed\n");
        return 4;
    }
    argbToRgba(buffer, rgba, WIDTH, HEIGHT);

    unsigned err = lodepng_encode32_file(outPath, rgba, WIDTH, HEIGHT);
    std::free(rgba);
    if (err) {
        std::fprintf(stderr, "lodepng error %u: %s\n", err, lodepng_error_text(err));
        return 5;
    }

    std::printf("Wrote %s (%dx%d)\n", outPath, WIDTH, HEIGHT);

    tvg::Initializer::term();
    return 0;
}
