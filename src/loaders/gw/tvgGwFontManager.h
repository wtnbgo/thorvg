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
#ifndef _TVG_GW_FONT_MANAGER_H_
#define _TVG_GW_FONT_MANAGER_H_

#include "tvgArray.h"
#include "tvgGwFace.h"

namespace tvg
{

//Process-wide registry of loaded GwFaces (mirror of FtFontManager). Used to
//walk the fallback chain when the primary font lacks a glyph for a particular
//codepoint. Registration order = fallback priority order.
struct GwFontManager
{
    static GwFontManager& instance();

    void enroll(GwFace* face);
    void retire(GwFace* face);

    //Returns a face (other than `primary`) that contains a glyph for
    //`codepoint`, or nullptr if no registered face does.
    GwFace* fallback(uint32_t codepoint, GwFace* primary) const;

    uint32_t size() const { return faces.count; }

private:
    GwFontManager() = default;
    Array<GwFace*> faces;
};

}

#endif //_TVG_GW_FONT_MANAGER_H_
