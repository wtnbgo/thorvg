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

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include <hb-ft.h>

#include "tvgFtFace.h"
#include "tvgLock.h"

using namespace tvg;

namespace {

//Process-wide FT_Library, lazily initialized. FreeType is thread-safe across
//FT_Faces only if each face is used by a single thread; the library itself
//can be created once and shared.
struct FtLibraryHolder
{
    FT_Library lib = nullptr;
    Key key;
};

static FtLibraryHolder gFtLib;


FT_Library acquireFtLibrary()
{
    ScopedLock lock(gFtLib.key);
    if (!gFtLib.lib) {
        if (FT_Init_FreeType(&gFtLib.lib) != 0) {
            gFtLib.lib = nullptr;
        }
    }
    return gFtLib.lib;
}


//Convert a 26.6 fixed-point FreeType coordinate to float.
inline float ftPosToFloat(FT_Pos x)
{
    return static_cast<float>(x) / 64.0f;
}


//Append the outline contours to `out`. Coordinates are in 26.6 units when the
//glyph is loaded with hinting/scaling, or in font units when loaded with
//FT_LOAD_NO_SCALE. Either way the same /64 conversion applies because the
//caller compensates via `scale` (e.g. 64.0f for NO_SCALE).
//Y is flipped so the result follows thorvg's y-down convention.
void outlineToPath(const FT_Outline* outline, float scale, RenderPath& out)
{
    if (!outline) return;

    int contourStart = 0;
    for (int c = 0; c < outline->n_contours; ++c) {
        int contourEnd = outline->contours[c];
        bool firstPoint = true;

        for (int p = contourStart; p <= contourEnd; ++p) {
            const FT_Vector& pt = outline->points[p];
            char tag = outline->tags[p] & 0x03;

            float x = ftPosToFloat(pt.x) * scale;
            float y = -ftPosToFloat(pt.y) * scale;

            if (firstPoint) {
                out.moveTo({x, y});
                firstPoint = false;
            } else if (tag == FT_CURVE_TAG_ON) {
                out.lineTo({x, y});
            } else if (tag == FT_CURVE_TAG_CONIC) {
                //Quadratic Bezier control point. We need an end point: it's
                //either the next on-curve point, or the midpoint between this
                //and the next conic control point (TrueType implicit on-curve).
                int nextIdx = (p + 1 > contourEnd) ? contourStart : p + 1;
                const FT_Vector& nextPt = outline->points[nextIdx];
                char nextTag = outline->tags[nextIdx] & 0x03;

                float nextX = ftPosToFloat(nextPt.x) * scale;
                float nextY = -ftPosToFloat(nextPt.y) * scale;

                if (nextTag == FT_CURVE_TAG_CONIC) {
                    nextX = (x + nextX) * 0.5f;
                    nextY = (y + nextY) * 0.5f;
                }

                //Promote quadratic to cubic: the cubic control points are at
                //2/3 of the way from the endpoints to the quadratic control.
                Point prev = out.pts.last();
                float cp1x = prev.x + (2.0f / 3.0f) * (x - prev.x);
                float cp1y = prev.y + (2.0f / 3.0f) * (y - prev.y);
                float cp2x = nextX + (2.0f / 3.0f) * (x - nextX);
                float cp2y = nextY + (2.0f / 3.0f) * (y - nextY);

                out.cubicTo({cp1x, cp1y}, {cp2x, cp2y}, {nextX, nextY});

                //If the next point was a real on-curve, we already consumed
                //it. Skip it on the next iteration unless we wrapped around.
                if (nextTag != FT_CURVE_TAG_CONIC && nextIdx > p) p = nextIdx - 1;
            } else if (tag == FT_CURVE_TAG_CUBIC) {
                //First control of a cubic. The next point is the second
                //control, the one after is the on-curve end.
                int nextIdx = (p + 1 > contourEnd) ? contourStart : p + 1;
                int endIdx = (p + 2 > contourEnd) ? contourStart : p + 2;

                const FT_Vector& cp2 = outline->points[nextIdx];
                const FT_Vector& endPt = outline->points[endIdx];

                float cp2x = ftPosToFloat(cp2.x) * scale;
                float cp2y = -ftPosToFloat(cp2.y) * scale;
                float endX = ftPosToFloat(endPt.x) * scale;
                float endY = -ftPosToFloat(endPt.y) * scale;

                out.cubicTo({x, y}, {cp2x, cp2y}, {endX, endY});

                p += 2;
            }
        }

        out.close();
        contourStart = contourEnd + 1;
    }
}

} //anonymous namespace


FtFace::~FtFace()
{
    release();
}


bool FtFace::open(const char* data, uint32_t size, bool copy)
{
    if (!data || size == 0) return false;

    auto lib = acquireFtLibrary();
    if (!lib) return false;

    if (copy) {
        auto buf = tvg::malloc<char>(size);
        if (!buf) return false;
        memcpy(buf, data, size);
        this->data = buf;
        this->ownsData = true;
    } else {
        //FT_New_Memory_Face does not modify the buffer, but the const_cast is
        //unavoidable due to FreeType's C API signature.
        this->data = const_cast<char*>(data);
        this->ownsData = false;
    }
    this->size = size;

    if (FT_New_Memory_Face(lib, reinterpret_cast<const FT_Byte*>(this->data),
                           static_cast<FT_Long>(size), 0, &face) != 0) {
        if (ownsData) tvg::free(this->data);
        this->data = nullptr;
        this->size = 0;
        this->ownsData = false;
        face = nullptr;
        return false;
    }

    //Attach a HarfBuzz font and lock its scale to font units so the shaper
    //returns advances/offsets that match the FT_LOAD_NO_SCALE outlines we
    //extract elsewhere. The hb_font holds a non-owning reference to FT_Face;
    //we destroy it before FT_Done_Face in release().
    hbFont = hb_ft_font_create(face, nullptr);
    if (hbFont) {
        auto upem = face->units_per_EM ? face->units_per_EM : 1000;
        hb_font_set_scale(hbFont, upem, upem);
    }

    return true;
}


void FtFace::release()
{
    if (hbFont) {
        hb_font_destroy(hbFont);
        hbFont = nullptr;
    }
    if (face) {
        FT_Done_Face(face);
        face = nullptr;
    }
    if (ownsData && data) tvg::free(data);
    data = nullptr;
    size = 0;
    ownsData = false;
}


uint32_t FtFace::glyphIndex(uint32_t codepoint) const
{
    if (!face) return 0;
    return FT_Get_Char_Index(face, codepoint);
}


bool FtFace::outline(uint32_t glyphId, float scale, RenderPath& out) const
{
    if (!face) return false;

    //FT_LOAD_NO_SCALE keeps coordinates in font units; the caller scales them
    //via `scale` (typically scale = 1/unitsPerEm * desired_em).
    if (FT_Load_Glyph(face, glyphId,
                      FT_LOAD_NO_SCALE | FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP) != 0) {
        return false;
    }

    if (face->glyph->format != FT_GLYPH_FORMAT_OUTLINE) return false;

    //FT_LOAD_NO_SCALE returns coords already in font units, but ftPosToFloat
    //still divides by 64, so multiply by 64 here to undo it.
    outlineToPath(&face->glyph->outline, scale * 64.0f, out);
    return true;
}


uint16_t FtFace::unitsPerEm() const
{
    if (!face) return 0;
    return static_cast<uint16_t>(face->units_per_EM);
}


int32_t FtFace::advance(uint32_t glyphId) const
{
    if (!face) return 0;
    if (FT_Load_Glyph(face, glyphId,
                      FT_LOAD_NO_SCALE | FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP) != 0) {
        return 0;
    }
    //With FT_LOAD_NO_SCALE the advance is reported in font units (not 26.6).
    return static_cast<int32_t>(face->glyph->advance.x);
}


int16_t FtFace::ascent() const
{
    if (!face) return 0;
    return face->ascender;
}


int16_t FtFace::descent() const
{
    if (!face) return 0;
    //ThorVG-wide convention: TextMetrics::descent is negative (matches TtfReader's
    //hhea.descent passthrough and FtLoader::metrics's "negative per FT convention"
    //comment). An earlier version negated this; downstream sign cancelled for
    //horizontal placement but inverted measured box height so descenders escaped.
    return face->descender;
}


int16_t FtFace::lineHeight() const
{
    if (!face) return 0;
    return face->height;
}
