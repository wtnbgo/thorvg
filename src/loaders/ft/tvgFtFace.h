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
#ifndef _TVG_FT_FACE_H_
#define _TVG_FT_FACE_H_

#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb.h>

#include "tvgCommon.h"
#include "tvgRender.h"

namespace tvg
{

//Wraps an FT_Face plus its companion hb_font_t. Owns the lifecycle of both.
//FT_Library is shared process-wide and lazily initialized.
struct FtFace
{
    FT_Face face = nullptr;
    hb_font_t* hbFont = nullptr;  //companion HarfBuzz font, scaled to font units
    char* data = nullptr;         //font data backing the FT_Face
    uint32_t size = 0;
    bool ownsData = false;        //true if `data` was allocated by us and must be freed

    ~FtFace();

    //Loads a font from memory. When `copy` is true the data is duplicated and
    //owned by FtFace; otherwise the caller must keep `data` alive until release.
    bool open(const char* data, uint32_t size, bool copy);

    //Closes the FT_Face and releases owned data.
    void release();

    //Returns the glyph index for the given Unicode codepoint, or 0 if missing.
    uint32_t glyphIndex(uint32_t codepoint) const;

    //Appends the outline of `glyphId` to `out` as a thorvg path.
    //Output coordinates are in font units (FT_LOAD_NO_SCALE) with Y flipped
    //(FT is y-up, thorvg is y-down). The given `scale` is applied to all
    //coordinates after the unit conversion. Returns false for color glyphs
    //or non-outline formats.
    bool outline(uint32_t glyphId, float scale, RenderPath& out) const;

    //Returns the unitsPerEm value of the font (e.g. 1000, 2048).
    uint16_t unitsPerEm() const;

    //Font-unit horizontal advance for the given glyph (FT_LOAD_NO_SCALE).
    //Returns 0 if the glyph cannot be loaded.
    int32_t advance(uint32_t glyphId) const;

    //Font-unit ascender / descender / line height. Descender is positive
    //(absolute value) for thorvg's y-down convention.
    int16_t ascent() const;
    int16_t descent() const;
    int16_t lineHeight() const;
};

}

#endif //_TVG_FT_FACE_H_
