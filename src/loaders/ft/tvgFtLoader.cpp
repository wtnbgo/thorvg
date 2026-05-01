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

#include <hb.h>

#include "tvgFtLoader.h"
#include "tvgFtFontManager.h"
#include "tvgStr.h"
#include "tvgMath.h"

using namespace tvg;


namespace {

#define LINE_FEED 10u

struct FtMetricsExt
{
    float baseWidth;  //first glyph width, used for italic shear anchoring
};


//Decode one UTF-8 codepoint and advance `utf8`. Mirrors the TtfLoader helper
//so we keep parity in error handling.
size_t decodeUtf8(const char** utf8, const char* end)
{
    auto p = *utf8;
    if (!(*p & 0x80U)) {
        (*utf8) += 1;
        return *p;
    } else if ((*p & 0xe0U) == 0xc0U && (p + 1 < end)) {
        (*utf8) += 2;
        return ((*p & 0x1fU) << 6) + (*(p + 1) & 0x3fU);
    } else if ((*p & 0xf0U) == 0xe0U && (p + 2 < end)) {
        (*utf8) += 3;
        return ((*p & 0x0fU) << 12) + ((*(p + 1) & 0x3fU) << 6) + (*(p + 2) & 0x3fU);
    } else if ((*p & 0xf8U) == 0xf0U && (p + 3 < end)) {
        (*utf8) += 4;
        return ((*p & 0x07U) << 18) + ((*(p + 1) & 0x3fU) << 12) + ((*(p + 2) & 0x3fU) << 6) + (*(p + 3) & 0x3fU);
    }
    TVGERR("FT", "Corrupted UTF8");
    (*utf8) += 1;
    return 0;
}


//Append `glyphPath` to `out`, translating each point by `offset`. Mirrors the
//TtfLoader::_build helper but we own the input path lifetime here so we can
//clear it after the copy.
void appendTranslated(const RenderPath& in, const Point& offset, RenderPath& out)
{
    out.cmds.push(in.cmds);
    out.pts.grow(in.pts.count);
    ARRAY_FOREACH(p, in.pts) {
        out.pts.push(*p + offset);
    }
}


void alignLineX(float align, float box, float lineWidth, uint32_t begin, uint32_t end, RenderPath& out)
{
    if (align <= 0.0f) return;
    auto shift = (box - lineWidth) * align;
    for (auto p = out.pts.data + begin; p < out.pts.data + end; ++p) {
        p->x += shift;
    }
}


void alignBlock(const Point& align, const Point& box, const Point& size, uint32_t begin, uint32_t end, RenderPath& out)
{
    if (align.x <= 0.0f && align.y <= 0.0f) return;
    auto shift = (box - size) * align;
    for (auto p = out.pts.data + begin; p < out.pts.data + end; ++p) {
        *p += shift;
    }
}

} //anonymous namespace


FtLoader::FtLoader() : FontLoader(FileType::Ttf)
{
}


FtLoader::~FtLoader()
{
    close();
}


bool FtLoader::open(const char* path)
{
#ifdef THORVG_FILE_IO_SUPPORT
    uint32_t size = 0;
    auto buf = LoadModule::open(path, size);
    if (!buf) return false;

    if (!ftFace.open(buf, size, false)) {
        tvg::free(buf);
        return false;
    }

    //transfer ownership of the file buffer to ftFace by toggling its
    //ownsData flag, since LoadModule::open()'s allocation must be freed by us.
    ftFace.ownsData = true;
    name = tvg::filename(path);
    FtFontManager::instance().enroll(&ftFace);
    return true;
#else
    return false;
#endif
}


bool FtLoader::open(const char* data, uint32_t size, const char* /*rpath*/, bool copy)
{
    if (!ftFace.open(data, size, copy)) return false;
    FtFontManager::instance().enroll(&ftFace);
    return true;
}


bool FtLoader::close()
{
    if (sharing > 0) {
        --sharing;
        return false;
    }
    FtFontManager::instance().retire(&ftFace);
    ftFace.release();
    tvg::free(name);
    name = nullptr;
    return true;
}


namespace {

//A shaped, positioned glyph ready to be placed by a wrap policy. Coordinates
//are in primary font units (after unitScale), spacing.x already applied.
struct Glyph
{
    FtFace* face;       //owning face, used to extract the outline at emit time
    uint32_t gid;
    float xAdv;         //horizontal advance (primary units)
    float xOff;         //horizontal offset (primary units)
    float yOff;         //vertical offset (primary units, y-down convention)
    uint32_t cluster;   //byte offset within the source line (for word breaks)
};

}


bool FtLoader::get(FontMetrics& fm, char* text, uint32_t len, RenderPath& out)
{
    out.clear();
    fm.lines = 1;
    fm.size = {};

    if (!text || fm.fontSize <= 0.0f) return false;
    if (!ftFace.face || !ftFace.hbFont) return false;

    auto primaryUpem = ftFace.unitsPerEm();
    if (primaryUpem == 0) return false;

    fm.scale = static_cast<float>(primaryUpem) / (fm.fontSize * FontLoader::DPI);

    if (!fm.engine) fm.engine = tvg::calloc<FtMetricsExt>(1, sizeof(FtMetricsExt));
    auto* ext = static_cast<FtMetricsExt*>(fm.engine);
    ext->baseWidth = 0.0f;

    auto box = fm.box * fm.scale;
    auto lineAdvance = static_cast<float>(ftFace.lineHeight()) * fm.spacing.y;

    Point cursor = {};
    uint32_t lineBegin = 0;
    bool firstGlyph = true;

    auto& mgr = FtFontManager::instance();

    hb_buffer_t* hbBuf = hb_buffer_create();
    if (!hbBuf) return false;

    //Pick the face that owns `cp`: primary first, then first fallback that
    //has it, else primary again (which will produce .notdef tofu).
    auto resolve = [&](uint32_t cp) -> FtFace* {
        if (ftFace.glyphIndex(cp) != 0) return &ftFace;
        if (auto* fb = mgr.fallback(cp, &ftFace)) return fb;
        return &ftFace;
    };

    //Shape a same-face run within a source line, append results to glyphs[].
    //All advances/offsets are normalized to primary font units here.
    auto shapeRun = [&](FtFace* face, const char* runStart, const char* lineStart,
                        uint32_t runLen, Array<Glyph>& glyphs) {
        if (!face || !face->hbFont || runLen == 0) return;
        auto runUpem = face->unitsPerEm();
        if (runUpem == 0) return;
        auto unitScale = static_cast<float>(primaryUpem) / static_cast<float>(runUpem);

        hb_buffer_reset(hbBuf);
        hb_buffer_add_utf8(hbBuf, runStart, static_cast<int>(runLen), 0,
                           static_cast<int>(runLen));
        hb_buffer_guess_segment_properties(hbBuf);
        if (fm.locale) {
            //hb_language_from_string interns the tag; cheap to call per run.
            hb_buffer_set_language(hbBuf, hb_language_from_string(fm.locale, -1));
        }
        hb_shape(face->hbFont, hbBuf, nullptr, 0);

        unsigned int count = 0;
        auto* infos = hb_buffer_get_glyph_infos(hbBuf, &count);
        auto* poses = hb_buffer_get_glyph_positions(hbBuf, &count);

        uint32_t runOffset = static_cast<uint32_t>(runStart - lineStart);
        for (unsigned int i = 0; i < count; ++i) {
            Glyph g{};
            g.face = face;
            g.gid = infos[i].codepoint;
            g.xAdv = static_cast<float>(poses[i].x_advance) * unitScale * fm.spacing.x;
            g.xOff = static_cast<float>(poses[i].x_offset) * unitScale;
            g.yOff = -static_cast<float>(poses[i].y_offset) * unitScale;
            g.cluster = runOffset + infos[i].cluster;
            glyphs.push(g);
        }
    };

    //Walk one source line, run-segment by face, shape into glyphs[].
    auto shapeLineToGlyphs = [&](const char* lineStart, const char* lineEnd,
                                 Array<Glyph>& glyphs) {
        glyphs.clear();
        const char* runStart = lineStart;
        FtFace* runFace = nullptr;
        const char* p = lineStart;
        while (p < lineEnd) {
            const char* cpStart = p;
            auto cp = decodeUtf8(&p, lineEnd);
            auto* f = resolve(cp);
            if (!runFace) {
                runFace = f;
            } else if (f != runFace) {
                shapeRun(runFace, runStart, lineStart,
                         static_cast<uint32_t>(cpStart - runStart), glyphs);
                runFace = f;
                runStart = cpStart;
            }
        }
        if (runFace) {
            shapeRun(runFace, runStart, lineStart,
                     static_cast<uint32_t>(p - runStart), glyphs);
        }
    };

    //Emit a single glyph at (cursor + g offsets), updating baseWidth on the
    //very first emitted glyph. Does NOT advance cursor.x.
    auto emitGlyph = [&](const Glyph& g) {
        RenderPath glyphPath;
        if (g.gid == 0 || !g.face) return;
        auto runUpem = g.face->unitsPerEm();
        if (runUpem == 0) return;
        auto unitScale = static_cast<float>(primaryUpem) / static_cast<float>(runUpem);
        if (!g.face->outline(g.gid, unitScale, glyphPath)) return;

        Point at = {cursor.x + g.xOff, cursor.y + g.yOff};
        appendTranslated(glyphPath, at, out);

        if (firstGlyph) {
            float minX = 0, maxX = 0;
            for (uint32_t k = 0; k < glyphPath.pts.count; ++k) {
                auto& q = glyphPath.pts.data[k];
                if (k == 0) { minX = maxX = q.x; }
                else {
                    if (q.x < minX) minX = q.x;
                    if (q.x > maxX) maxX = q.x;
                }
            }
            auto w = maxX - minX;
            if (w > 0.0f) ext->baseWidth = w;
            firstGlyph = false;
        }
    };

    //Emit one output line: glyphs[from..to) at the current cursor, then align
    //horizontally, and (if `moreLines`) bump cursor.y / lines.
    auto emitLine = [&](const Glyph* gs, uint32_t from, uint32_t to,
                        float lineWidth, bool moreLines) {
        for (uint32_t i = from; i < to; ++i) {
            emitGlyph(gs[i]);
            cursor.x += gs[i].xAdv;
        }
        if (cursor.x > fm.size.x) fm.size.x = cursor.x;
        alignLineX(fm.align.x, box.x, lineWidth, lineBegin, out.pts.count, out);
        lineBegin = out.pts.count;
        if (moreLines) {
            cursor.x = 0.0f;
            cursor.y += lineAdvance;
            ++fm.lines;
        }
    };

    auto layoutNone = [&](const Glyph* gs, uint32_t n, bool finalSourceLine) {
        float w = 0;
        for (uint32_t i = 0; i < n; ++i) w += gs[i].xAdv;
        emitLine(gs, 0, n, w, !finalSourceLine);
    };

    auto layoutChar = [&](const Glyph* gs, uint32_t n, bool finalSourceLine) {
        uint32_t lineFrom = 0;
        float lineW = 0;
        for (uint32_t i = 0; i < n; ++i) {
            auto a = gs[i].xAdv;
            if (lineW + a > box.x && i > lineFrom) {
                emitLine(gs, lineFrom, i, lineW, true);
                lineFrom = i;
                lineW = 0;
            }
            lineW += a;
        }
        emitLine(gs, lineFrom, n, lineW, !finalSourceLine);
    };

    auto isWordBreakCluster = [](const char* lineText, uint32_t lineLen, uint32_t cluster) {
        if (cluster >= lineLen) return false;
        char c = lineText[cluster];
        return c == ' ' || c == '\t';
    };

    auto layoutWord = [&](const Glyph* gs, uint32_t n,
                          const char* lineText, uint32_t lineLen,
                          bool finalSourceLine, bool smart) {
        uint32_t lineFrom = 0;
        uint32_t wordFrom = 0;
        float lineW = 0;
        float wordW = 0;
        for (uint32_t i = 0; i < n; ++i) {
            auto a = gs[i].xAdv;

            //about to overflow the line: try to break at the current word
            if (lineW + a > box.x && i > lineFrom) {
                if (wordFrom > lineFrom && wordW + a <= box.x) {
                    //the partial word starting at wordFrom fits if put alone
                    //on the next line: break before it
                    emitLine(gs, lineFrom, wordFrom, lineW - wordW, true);
                    lineFrom = wordFrom;
                    lineW = wordW;
                } else if (smart) {
                    //word too big for any line: char-wrap fallback right here
                    emitLine(gs, lineFrom, i, lineW, true);
                    lineFrom = i;
                    wordFrom = i;
                    lineW = 0;
                    wordW = 0;
                }
                //plain word wrap with an oversized word: keep adding (overflow)
            }

            lineW += a;
            wordW += a;
            if (isWordBreakCluster(lineText, lineLen, gs[i].cluster)) {
                wordFrom = i + 1;
                wordW = 0;
            }
        }
        emitLine(gs, lineFrom, n, lineW, !finalSourceLine);
    };

    auto layoutEllipsis = [&](const Glyph* gs, uint32_t n, bool finalSourceLine) {
        //Pre-compute the ellipsis width using the primary face's '.' glyph
        auto dotGid = ftFace.glyphIndex('.');
        Glyph dot{};
        dot.face = &ftFace;
        dot.gid = dotGid;
        dot.xAdv = static_cast<float>(ftFace.advance(dotGid)) * fm.spacing.x;
        float ellipsisW = dot.xAdv * 3.0f;

        //Find how many leading glyphs fit when reserving room for "..."
        uint32_t take = n;
        float lineW = 0;
        for (uint32_t i = 0; i < n; ++i) {
            auto a = gs[i].xAdv;
            if (lineW + a + ellipsisW > box.x && i > 0) {
                take = i;
                break;
            }
            lineW += a;
        }
        //Emit selected glyphs
        for (uint32_t i = 0; i < take; ++i) {
            emitGlyph(gs[i]);
            cursor.x += gs[i].xAdv;
        }
        //Append "..." if we truncated
        if (take < n && dotGid != 0) {
            for (int k = 0; k < 3; ++k) {
                emitGlyph(dot);
                cursor.x += dot.xAdv;
                lineW += dot.xAdv;
            }
        }
        if (cursor.x > fm.size.x) fm.size.x = cursor.x;
        alignLineX(fm.align.x, box.x, lineW, lineBegin, out.pts.count, out);
        lineBegin = out.pts.count;
        if (!finalSourceLine) {
            cursor.x = 0.0f;
            cursor.y += lineAdvance;
            ++fm.lines;
        }
    };

    //Walk source lines split on '\n'. Each is shaped, then handed to a wrap
    //policy that may produce one or more output lines.
    Array<Glyph> glyphs;
    const char* lineStart = text;
    const char* end = text + len;
    bool wrapping = (box.x > 0.0f) && (fm.wrap != TextWrap::None);

    for (const char* p = text; p <= end; ++p) {
        if (p == end || *p == '\n') {
            shapeLineToGlyphs(lineStart, p, glyphs);
            bool isFinal = (p == end);
            uint32_t lineLen = static_cast<uint32_t>(p - lineStart);

            if (!wrapping) {
                layoutNone(glyphs.data, glyphs.count, isFinal);
            } else if (fm.wrap == TextWrap::Character) {
                layoutChar(glyphs.data, glyphs.count, isFinal);
            } else if (fm.wrap == TextWrap::Word) {
                layoutWord(glyphs.data, glyphs.count, lineStart, lineLen, isFinal, false);
            } else if (fm.wrap == TextWrap::Smart) {
                layoutWord(glyphs.data, glyphs.count, lineStart, lineLen, isFinal, true);
            } else if (fm.wrap == TextWrap::Ellipsis) {
                layoutEllipsis(glyphs.data, glyphs.count, isFinal);
            } else {
                layoutNone(glyphs.data, glyphs.count, isFinal);
            }

            if (isFinal) break;
            lineStart = p + 1;
        }
    }

    hb_buffer_destroy(hbBuf);

    fm.size.y = static_cast<float>(fm.lines) * lineAdvance;
    alignBlock(fm.align, box, {fm.size.x, fm.size.y}, 0, out.pts.count, out);

    return true;
}


void FtLoader::transform(Paint* paint, FontMetrics& fm, float italicShear)
{
    if (!ftFace.face || fm.scale == 0.0f) return;

    auto scale = 1.0f / fm.scale;  //font units -> output units
    auto baseWidth = static_cast<FtMetricsExt*>(fm.engine)->baseWidth;
    auto ascent = static_cast<float>(ftFace.ascent());

    //Same matrix shape as TtfLoader::transform — translate by ascent so the
    //caller-supplied origin is the baseline, and shear horizontally for italic.
    Matrix m = {
        scale, -italicShear * scale, italicShear * baseWidth * scale,
        0,     scale,                ascent * scale,
        0,     0,                    1
    };
    paint->transform(m);
}


void FtLoader::release(FontMetrics& fm)
{
    tvg::free(fm.engine);
    fm.engine = nullptr;
}


void FtLoader::metrics(const FontMetrics& fm, TextMetrics& out)
{
    auto scale = (fm.fontSize * FontLoader::DPI) / static_cast<float>(ftFace.unitsPerEm());
    out.advance = static_cast<float>(ftFace.lineHeight()) * scale;
    out.ascent = static_cast<float>(ftFace.ascent()) * scale;
    out.descent = static_cast<float>(ftFace.descent()) * scale;   //negative per FT convention
    //FreeType doesn't surface linegap directly via the public face fields.
    //hhea: face->height = ascender - descender + lineGap, with descender < 0.
    //So lineGap = height - (ascender - descender) = advance - ascent + descent.
    //(Earlier `advance - (ascent + descent)` had the descent sign inverted, which
    //inflated linegap by ~2|descent| and overgrew text bounding boxes vertically.)
    out.linegap = out.advance - out.ascent + out.descent;
    if (out.linegap < 0.0f) out.linegap = 0.0f;
}


bool FtLoader::metrics(const FontMetrics& fm, const char* ch, GlyphMetrics& out)
{
    if (!ch || !ftFace.face || fm.fontSize <= 0.0f) return false;
    auto p = ch;
    auto code = decodeUtf8(&p, ch + strlen(ch));

    //Apply the same primary→fallback resolve order as the full-text shaping
    //path (see resolve() in generate()). Without this, callers that compute
    //a string's total width by summing per-glyph advances (e.g. Elements
    //text_backend_tvg::measure_text) underestimate widths whenever the
    //primary font lacks the glyph (CJK / emoji), causing layout overflow.
    auto* face = &ftFace;
    auto gid = face->glyphIndex(code);
    if (gid == 0) {
        if (auto* fb = FtFontManager::instance().fallback(code, &ftFace)) {
            face = fb;
            gid = face->glyphIndex(code);
        }
    }
    if (gid == 0) return false;

    auto upem = face->unitsPerEm();
    if (upem == 0) return false;
    auto scale = (fm.fontSize * FontLoader::DPI) / static_cast<float>(upem);
    out.advance = static_cast<float>(face->advance(gid)) * scale;
    //bearing/min/max require the glyph bbox; produce a coarse approximation
    //from the glyph outline. Phase 2 keeps it simple.
    out.bearing = 0.0f;
    out.min = {0, 0};
    out.max = {out.advance, 0};
    return true;
}


void FtLoader::copy(const FontMetrics& in, FontMetrics& out)
{
    release(out);
    out = in;
    if (in.engine) {
        out.engine = tvg::calloc<FtMetricsExt>(1, sizeof(FtMetricsExt));
        *static_cast<FtMetricsExt*>(out.engine) = *static_cast<const FtMetricsExt*>(in.engine);
    } else {
        out.engine = nullptr;
    }
}
