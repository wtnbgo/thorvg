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

#ifdef THORVG_NEON_VECTOR_SUPPORT

#include <arm_neon.h>

#if defined(__aarch64__) || defined(__ARM_64BIT_STATE) || defined(_M_ARM64)
#define TVG_AARCH64 1
#else
#define TVG_AARCH64 0
#endif


//per byte: (c * (a + 1)) >> 8 — bit-exact with the scalar ALPHA_BLEND()
static inline uint8x8_t ALPHA_BLEND(uint8x8_t c, uint8x8_t a)
{
    auto t = vmull_u8(c, a);
    t = vaddw_u8(t, c);
    return vshrn_n_u16(t, 8);
}


#if TVG_AARCH64
static inline uint8x16_t ALPHA_BLEND(uint8x16_t c, uint8x16_t a)
{
    auto lo = vmull_u8(vget_low_u8(c), vget_low_u8(a));
    auto hi = vmull_high_u8(c, a);
    lo = vaddw_u8(lo, vget_low_u8(c));
    hi = vaddw_high_u8(hi, c);
    return vshrn_high_n_u16(vshrn_n_u16(lo, 8), hi, 8);
}
#endif


//dst[i] = src + ALPHA_BLEND(dst[i], ialpha) — the common span blend of the
//solid (partial coverage) and translucent fill paths. Bit-exact with the
//scalar loop. Per-byte add never carries for valid premultiplied input
//(src ≤ src_alpha per channel), matching the other SIMD variants.
static void neonBlendSpan32(uint32_t* dst, uint32_t src, uint8_t ialpha, int32_t len)
{
    int32_t i = 0;
#if TVG_AARCH64
    if (len >= 4) {
        auto vSrc4 = vreinterpretq_u8_u32(vdupq_n_u32(src));
        auto vIalpha4 = vdupq_n_u8(ialpha);
        for (; i + 4 <= len; i += 4) {
            auto d = vreinterpretq_u8_u32(vld1q_u32(dst + i));
            d = vaddq_u8(vSrc4, ALPHA_BLEND(d, vIalpha4));
            vst1q_u32(dst + i, vreinterpretq_u32_u8(d));
        }
    }
#endif
    if (i + 2 <= len) {
        auto vSrc2 = vreinterpret_u8_u32(vdup_n_u32(src));
        auto vIalpha2 = vdup_n_u8(ialpha);
        for (; i + 2 <= len; i += 2) {
            auto d = vreinterpret_u8_u32(vld1_u32(dst + i));
            d = vadd_u8(vSrc2, ALPHA_BLEND(d, vIalpha2));
            vst1_u32(dst + i, vreinterpret_u32_u8(d));
        }
    }
    for (; i < len; ++i) dst[i] = src + ALPHA_BLEND(dst[i], ialpha);
}


static void neonRasterGrayscale8(uint8_t* dst, uint8_t val, uint32_t offset, int32_t len)
{
    dst += offset;

    int32_t i = 0;
    const uint8x16_t valVec = vdupq_n_u8(val);
#if TVG_AARCH64
    uint8x16x4_t valQuad = {valVec, valVec, valVec, valVec};
    for (; i <= len - 16 * 4; i += 16 * 4) {
        vst1q_u8_x4(dst + i, valQuad);
    }
#else
    for (; i <= len - 16; i += 16) {
        vst1q_u8(dst + i, valVec);
    }
#endif
    for (; i < len; i++) {
        dst[i] = val;
    }
}


static void neonRasterPixel32(uint32_t *dst, uint32_t val, uint32_t offset, int32_t len)
{
    dst += offset;

    uint32x4_t vectorVal = vdupq_n_u32(val);

#if TVG_AARCH64
    uint32_t iterations = len / 16;
    uint32_t neonFilled = iterations * 16;
    uint32x4x4_t valQuad = {vectorVal, vectorVal, vectorVal, vectorVal};
    for (uint32_t i = 0; i < iterations; ++i) {
        vst4q_u32(dst, valQuad);
        dst += 16;
    }
#else
    uint32_t iterations = len / 4;
    uint32_t neonFilled = iterations * 4;
    for (uint32_t i = 0; i < iterations; ++i) {
        vst1q_u32(dst, vectorVal);
        dst += 4;
    }
#endif
    int32_t leftovers = len - neonFilled;
    while (leftovers--) *dst++ = val;
}


static bool neonRasterTranslucentRle(SwSurface* surface, const SwRle* rle, const RenderRegion& bbox, const RenderColor& c)
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
            neonBlendSpan32(&surface->buf32[span->y * surface->stride + x], src, IA(src), len);
        }
    //8bit grayscale
    } else if (surface->channelSize == sizeof(uint8_t)) {
        TVGLOG("SW_ENGINE", "Require Neon Optimization, Channel Size = %d", surface->channelSize);
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


static bool neonRasterTranslucentRect(SwSurface* surface, const RenderRegion& bbox, const RenderColor& c)
{
    auto h = bbox.h();
    auto w = bbox.w();

    //32bits channels
    if (surface->channelSize == sizeof(uint32_t)) {
        auto color = surface->join(c.r, c.g, c.b, c.a);
        auto buffer = surface->buf32 + (bbox.min.y * surface->stride) + bbox.min.x;
        auto ialpha = 255 - c.a;

        for (uint32_t y = 0; y < h; ++y) {
            neonBlendSpan32(&buffer[y * surface->stride], color, ialpha, w);
        }
    //8bit grayscale
    } else if (surface->channelSize == sizeof(uint8_t)) {
        TVGLOG("SW_ENGINE", "Require Neon Optimization, Channel Size = %d", surface->channelSize);
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

#endif
