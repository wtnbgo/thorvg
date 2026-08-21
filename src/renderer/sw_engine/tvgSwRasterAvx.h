/*
 * Copyright (c) 2021 - 2026 ThorVG project. All rights reserved.

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

#ifdef THORVG_AVX_VECTOR_SUPPORT

#include <immintrin.h>

#define N_32BITS_IN_128REG 4
#define N_32BITS_IN_256REG 8

//per byte: (c * (a + 1)) >> 8 — bit-exact with the scalar ALPHA_BLEND().
//@p a must carry the alpha value replicated in every byte lane.
static inline __m128i ALPHA_BLEND(__m128i c, __m128i a)
{
    auto AG = _mm_set1_epi32(0xff00ff00);
    auto RB = _mm_set1_epi32(0x00ff00ff);

    //alpha at the low byte of each 16bit lane
    auto aRB = _mm_and_si128(a, RB);
    //R/B (and A/G shifted down) channel bytes at the low byte of each 16bit lane
    auto cRB = _mm_and_si128(c, RB);
    auto cAG = _mm_and_si128(_mm_srli_epi16(c, 8), RB);

    //c * a + c == c * (a + 1); max 0xff00 per lane, no overflow
    auto even = _mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(cRB, aRB), cRB), 8);
    auto odd = _mm_and_si128(_mm_add_epi16(_mm_mullo_epi16(cAG, aRB), cAG), AG);

    return _mm_or_si128(even, odd);
}


//dst[i] = src + ALPHA_BLEND(dst[i], ialpha) — the common span blend of the
//solid (partial coverage) and translucent fill paths. Bit-exact with the
//scalar loop.
static void avxBlendSpan32(uint32_t* dst, uint32_t src, uint8_t ialpha, int32_t len)
{
    int32_t i = 0;
    if (len >= N_32BITS_IN_128REG) {
        auto avxSrc = _mm_set1_epi32(src);
        auto avxIalpha = _mm_set1_epi8(ialpha);
        for (; i + N_32BITS_IN_128REG <= len; i += N_32BITS_IN_128REG) {
            auto d = _mm_loadu_si128((__m128i*)(dst + i));
            d = _mm_add_epi32(avxSrc, ALPHA_BLEND(d, avxIalpha));
            _mm_storeu_si128((__m128i*)(dst + i), d);
        }
    }
    for (; i < len; ++i) dst[i] = src + ALPHA_BLEND(dst[i], ialpha);
}


static void avxRasterGrayscale8(uint8_t* dst, uint8_t val, uint32_t offset, int32_t len)
{
    dst += offset;

    __m256i vecVal = _mm256_set1_epi8(val);

    int32_t i = 0;
    for (; i <= len - 32; i += 32) {
        _mm256_storeu_si256((__m256i*)(dst + i), vecVal);
    }

    for (; i < len; ++i) {
        dst[i] = val;
    }
}


static void avxRasterPixel32(uint32_t *dst, uint32_t val, uint32_t offset, int32_t len)
{
    //1. calculate how many iterations we need to cover the length
    uint32_t iterations = len / N_32BITS_IN_256REG;
    uint32_t avxFilled = iterations * N_32BITS_IN_256REG;

    //2. set the beginning of the array
    dst += offset;

    //3. fill the octets
    for (uint32_t i = 0; i < iterations; ++i, dst += N_32BITS_IN_256REG) {
        _mm256_storeu_si256((__m256i*)dst, _mm256_set1_epi32(val));
    }

    //4. fill leftovers (in the first step we have to set the pointer to the place where the avx job is done)
    int32_t leftovers = len - avxFilled;
    while (leftovers--) *dst++ = val;
}


static bool avxRasterTranslucentRect(SwSurface* surface, const RenderRegion& bbox, const RenderColor& c)
{
    auto h = bbox.h();
    auto w = bbox.w();

    //32bits channels
    if (surface->channelSize == sizeof(uint32_t)) {
        auto color = surface->join(c.r, c.g, c.b, c.a);
        auto buffer = surface->buf32 + (bbox.min.y * surface->stride) + bbox.min.x;
        auto ialpha = 255 - c.a;

        for (uint32_t y = 0; y < h; ++y) {
            avxBlendSpan32(&buffer[y * surface->stride], color, ialpha, w);
        }
    //8bit grayscale
    } else if (surface->channelSize == sizeof(uint8_t)) {
        TVGLOG("SW_ENGINE", "Require AVX Optimization, Channel Size = %d", surface->channelSize);
        auto buffer = surface->buf8 + (bbox.min.y * surface->stride) + bbox.min.x;
        auto ialpha = ~c.a;
        for (uint32_t y = 0; y < h; ++y) {
            auto dst = &buffer[y * surface->stride];
            for (uint32_t x = 0; x < w; ++x, ++dst) {
                *dst = c.a + MULTIPLY(*dst, ialpha);
            }
        }
    }
    return true;
}


static bool avxRasterTranslucentRle(SwSurface* surface, const SwRle* rle, const RenderRegion& bbox, const RenderColor& c)
{
    const SwSpan* end;
    int32_t x, len;

    //32bit channels
    if (surface->channelSize == sizeof(uint32_t)) {
        auto color = surface->join(c.r, c.g, c.b, c.a);
        uint32_t src;

        for (auto span = rle->fetch(bbox, &end); span < end; ++span) {
            if (!span->fetch(bbox, x, len)) continue;
            if (span->coverage < 255) src = ALPHA_BLEND(color, span->coverage);
            else src = color;
            avxBlendSpan32(&surface->buf32[span->y * surface->stride + x], src, IA(src), len);
        }
    //8bit grayscale
    } else if (surface->channelSize == sizeof(uint8_t)) {
        TVGLOG("SW_ENGINE", "Require AVX Optimization, Channel Size = %d", surface->channelSize);
        uint8_t src;
        for (auto span = rle->fetch(bbox, &end); span < end; ++span) {
            if (!span->fetch(bbox, x, len)) continue;
            auto dst = &surface->buf8[span->y * surface->stride + x];
            if (span->coverage < 255) src = MULTIPLY(span->coverage, c.a);
            else src = c.a;
            auto ialpha = ~c.a;
            for (auto x = 0; x < len; ++x, ++dst) {
                *dst = src + MULTIPLY(*dst, ialpha);
            }
        }
    }
    return true;
}


#endif
