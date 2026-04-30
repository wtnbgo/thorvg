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
#ifndef _TVG_FT_LOADER_H_
#define _TVG_FT_LOADER_H_

#include "tvgLoader.h"
#include "tvgFtFace.h"

struct FtLoader : public tvg::FontLoader
{
    tvg::FtFace ftFace;

    FtLoader();
    ~FtLoader();

    using FontLoader::open;
    using FontLoader::read;

    bool open(const char* path) override;
    bool open(const char* data, uint32_t size, const char* rpath, bool copy) override;
    bool close() override;
    bool get(tvg::FontMetrics& fm, char* text, uint32_t len, tvg::RenderPath& out) override;
    void transform(tvg::Paint* paint, tvg::FontMetrics& fm, float italicShear) override;
    void release(tvg::FontMetrics& fm) override;
    void metrics(const tvg::FontMetrics& fm, tvg::TextMetrics& out) override;
    bool metrics(const tvg::FontMetrics& fm, const char* ch, tvg::GlyphMetrics& out) override;
    void copy(const tvg::FontMetrics& in, tvg::FontMetrics& out) override;
};

#endif //_TVG_FT_LOADER_H_
