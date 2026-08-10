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

#include "tvgGwFace.h"

using namespace tvg;

namespace {

const TvgGwBridge* bridge() { return tvgGwGetBridge(); }

//RenderPath adapter for TvgGwPathSink
struct PathSinkCtx
{
    RenderPath* out;
};

void sinkMoveTo(void* ctx, float x, float y)
{
    static_cast<PathSinkCtx*>(ctx)->out->moveTo({x, y});
}

void sinkLineTo(void* ctx, float x, float y)
{
    static_cast<PathSinkCtx*>(ctx)->out->lineTo({x, y});
}

void sinkCubicTo(void* ctx, float c1x, float c1y, float c2x, float c2y, float x, float y)
{
    static_cast<PathSinkCtx*>(ctx)->out->cubicTo({c1x, c1y}, {c2x, c2y}, {x, y});
}

void sinkClose(void* ctx)
{
    static_cast<PathSinkCtx*>(ctx)->out->close();
}

} //anonymous namespace


GwFace::~GwFace()
{
    release();
}


bool GwFace::open(const char* data, uint32_t size, bool copy)
{
    auto* b = bridge();
    if (!b || !data || size == 0) return false;
    handle = b->openFace(b->ctx, data, size, copy ? 1 : 0);
    return handle != nullptr;
}


bool GwFace::open(const char* key)
{
    auto* b = bridge();
    if (!b || !b->openFaceByKey || !key || !*key) return false;
    handle = b->openFaceByKey(b->ctx, key);
    return handle != nullptr;
}


void GwFace::release()
{
    if (!handle) return;
    if (auto* b = bridge()) b->closeFace(b->ctx, handle);
    handle = nullptr;
}


const char* GwFace::family() const
{
    auto* b = bridge();
    if (!b || !handle) return nullptr;
    return b->faceFamily(b->ctx, handle);
}


const char* GwFace::style() const
{
    auto* b = bridge();
    if (!b || !handle) return nullptr;
    return b->faceStyle(b->ctx, handle);
}


uint32_t GwFace::glyphIndex(uint32_t codepoint) const
{
    auto* b = bridge();
    if (!b || !handle) return 0;
    return b->glyphIndex(b->ctx, handle, codepoint);
}


bool GwFace::outline(uint32_t glyphId, float scale, RenderPath& out) const
{
    auto* b = bridge();
    if (!b || !handle) return false;

    PathSinkCtx ctx = {&out};
    TvgGwPathSink sink;
    sink.ctx = &ctx;
    sink.moveTo = sinkMoveTo;
    sink.lineTo = sinkLineTo;
    sink.cubicTo = sinkCubicTo;
    sink.close = sinkClose;
    return b->glyphOutline(b->ctx, handle, glyphId, scale, &sink) != 0;
}


uint16_t GwFace::unitsPerEm() const
{
    auto* b = bridge();
    if (!b || !handle) return 0;
    return b->unitsPerEm(b->ctx, handle);
}


int32_t GwFace::advance(uint32_t glyphId) const
{
    auto* b = bridge();
    if (!b || !handle) return 0;
    return b->glyphAdvance(b->ctx, handle, glyphId);
}


int16_t GwFace::ascent() const
{
    auto* b = bridge();
    if (!b || !handle) return 0;
    return b->ascender(b->ctx, handle);
}


int16_t GwFace::descent() const
{
    auto* b = bridge();
    if (!b || !handle) return 0;
    return b->descender(b->ctx, handle);
}


int16_t GwFace::lineHeight() const
{
    auto* b = bridge();
    if (!b || !handle) return 0;
    return b->height(b->ctx, handle);
}


namespace {

struct ShapeEmitCtx
{
    Array<GwFace::Shaped>* out;
};

void shapeEmit(void* emitCtx, const TvgGwShapedGlyph* g)
{
    GwFace::Shaped s;
    s.gid = g->gid;
    s.xAdv = g->xAdvance;
    s.xOff = g->xOffset;
    s.yOff = g->yOffset;
    s.cluster = g->cluster;
    static_cast<ShapeEmitCtx*>(emitCtx)->out->push(s);
}

} //anonymous namespace


void GwFace::shape(const char* utf8, uint32_t len, const char* locale,
                   Array<Shaped>& out) const
{
    auto* b = bridge();
    if (!b || !handle || !utf8 || len == 0) return;
    ShapeEmitCtx ctx = {&out};
    b->shapeRun(b->ctx, handle, utf8, len, locale, shapeEmit, &ctx);
}
