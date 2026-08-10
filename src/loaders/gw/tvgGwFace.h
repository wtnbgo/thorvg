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
#ifndef _TVG_GW_FACE_H_
#define _TVG_GW_FACE_H_

#include "tvgCommon.h"
#include "tvgRender.h"
#include "thorvg_gw_bridge.h"

namespace tvg
{

//Bridge-backed counterpart of FtFace (see src/loaders/ft/tvgFtFace.h): the
//same interface, but faces/metrics/outlines/shaping come from the host font
//engine via TvgGwBridge instead of an in-tree FreeType+HarfBuzz stack.
struct GwFace
{
    void* handle = nullptr;   //bridge face handle (nullptr = closed)

    //One shaped glyph in face font units (yOff y-down), see GwFace::shape().
    struct Shaped
    {
        uint32_t gid;
        float xAdv;
        float xOff;
        float yOff;
        uint32_t cluster;
    };

    ~GwFace();

    //Opens a face over font bytes through the bridge. When `copy` is false the
    //caller must keep `data` alive until release().
    bool open(const char* data, uint32_t size, bool copy);

    //Opens a face by HOST KEY through the bridge (shared bytes on the host
    //side, no copy). Fails when the bridge has no key resolver.
    bool open(const char* key);

    void release();

    bool valid() const { return handle != nullptr; }

    const char* family() const;
    const char* style() const;

    uint32_t glyphIndex(uint32_t codepoint) const;

    //Appends the outline of `glyphId` to `out` (y-down, conic->cubic upgraded,
    //`scale` applied). Returns false for color/bitmap-only glyphs.
    bool outline(uint32_t glyphId, float scale, RenderPath& out) const;

    uint16_t unitsPerEm() const;
    int32_t advance(uint32_t glyphId) const;    //font units
    int16_t ascent() const;                     //font units, positive up
    int16_t descent() const;                    //negative (FT convention)
    int16_t lineHeight() const;

    //Shape a single-face UTF-8 run; appends glyphs in visual order with
    //advances/offsets in this face's font units.
    void shape(const char* utf8, uint32_t len, const char* locale,
               Array<Shaped>& out) const;
};

}

#endif //_TVG_GW_FACE_H_
