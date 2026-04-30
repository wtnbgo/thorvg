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
#ifndef _TVG_FT_FONT_MANAGER_H_
#define _TVG_FT_FONT_MANAGER_H_

#include "tvgArray.h"
#include "tvgFtFace.h"

namespace tvg
{

//Process-wide registry of loaded FtFaces. Used to walk the fallback chain
//when the primary font lacks a glyph for a particular codepoint.
//Registration order = fallback priority order.
struct FtFontManager
{
    static FtFontManager& instance();

    //Register / unregister a face. Both are no-ops on nullptr.
    void enroll(FtFace* face);
    void retire(FtFace* face);

    //Returns a face (other than `primary`) that contains a glyph for
    //`codepoint`, or nullptr if no registered face does.
    FtFace* fallback(uint32_t codepoint, FtFace* primary) const;

    //For tests: number of currently registered faces.
    uint32_t size() const { return faces.count; }

private:
    FtFontManager() = default;
    Array<FtFace*> faces;
};

}

#endif //_TVG_FT_FONT_MANAGER_H_
