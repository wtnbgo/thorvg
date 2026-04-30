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

#include "tvgFtFontManager.h"
#include "tvgLock.h"

using namespace tvg;

namespace {

Key gFtMgrKey;

}


FtFontManager& FtFontManager::instance()
{
    static FtFontManager mgr;
    return mgr;
}


void FtFontManager::enroll(FtFace* face)
{
    if (!face) return;
    ScopedLock lock(gFtMgrKey);
    //Avoid duplicate enrollment if the same loader is reopened.
    for (uint32_t i = 0; i < faces.count; ++i) {
        if (faces.data[i] == face) return;
    }
    faces.push(face);
}


void FtFontManager::retire(FtFace* face)
{
    if (!face) return;
    ScopedLock lock(gFtMgrKey);
    for (uint32_t i = 0; i < faces.count; ++i) {
        if (faces.data[i] == face) {
            //Order-preserving removal: shift remaining entries down so the
            //fallback priority is preserved.
            for (uint32_t j = i + 1; j < faces.count; ++j) {
                faces.data[j - 1] = faces.data[j];
            }
            --faces.count;
            return;
        }
    }
}


FtFace* FtFontManager::fallback(uint32_t codepoint, FtFace* primary) const
{
    ScopedLock lock(gFtMgrKey);
    for (uint32_t i = 0; i < faces.count; ++i) {
        auto* f = faces.data[i];
        if (f == primary) continue;
        if (!f->face) continue;
        if (f->glyphIndex(codepoint) != 0) return f;
    }
    return nullptr;
}
