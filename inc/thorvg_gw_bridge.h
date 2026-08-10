/*
 * thorvg_gw_bridge.h — host-injected font engine bridge for the "gw" text
 * loader (TVG_LOADER_GW).
 *
 * The gw loader renders text exactly like the ft loader (FreeType+HarfBuzz,
 * see src/loaders/ft/) but does not link FreeType/HarfBuzz itself: the host
 * application injects a bridge that supplies faces, metrics, shaping and
 * outlines from its own unified font engine (e.g. glyphware). This removes
 * the duplicated FT/HB stack from ThorVG builds embedded in such hosts.
 *
 * Conventions (all mirror the ft loader's internal units):
 *  - metrics / advances / offsets are in FONT UNITS of the face
 *  - descender is negative (FreeType convention), height is FT face->height
 *  - shapeRun yOffset is Y-DOWN (positive moves the glyph down)
 *  - glyphOutline emits Y-DOWN coordinates, conics upgraded to cubics, with
 *    `scale` already applied to every coordinate
 *
 * The bridge must be registered (tvgGwSetBridge) before any tvg::Text use.
 * All calls happen on the thread that drives ThorVG (the host UI thread).
 */
#ifndef _THORVG_GW_BRIDGE_H_
#define _THORVG_GW_BRIDGE_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Path receiver for glyph outlines. */
typedef struct TvgGwPathSink
{
    void* ctx;
    void (*moveTo)(void* ctx, float x, float y);
    void (*lineTo)(void* ctx, float x, float y);
    void (*cubicTo)(void* ctx, float c1x, float c1y, float c2x, float c2y, float x, float y);
    void (*close)(void* ctx);
} TvgGwPathSink;

/* One shaped glyph in face font units. yOffset is y-down. */
typedef struct TvgGwShapedGlyph
{
    uint32_t gid;
    float xAdvance;
    float xOffset;
    float yOffset;
    uint32_t cluster;   /* byte offset into the run's UTF-8 */
} TvgGwShapedGlyph;

typedef struct TvgGwBridge
{
    void* ctx;

    /* Open a face over font bytes. When copy != 0 the bridge must duplicate
       the bytes; otherwise the caller keeps them alive until closeFace.
       Returns an opaque face handle, or NULL on failure. */
    void* (*openFace)(void* ctx, const char* data, uint32_t size, int copy);
    void  (*closeFace)(void* ctx, void* face);

    /* Family / subfamily names (UTF-8). Pointers stay valid until closeFace.
       May return NULL. */
    const char* (*faceFamily)(void* ctx, void* face);
    const char* (*faceStyle)(void* ctx, void* face);

    uint32_t (*glyphIndex)(void* ctx, void* face, uint32_t codepoint);
    uint16_t (*unitsPerEm)(void* ctx, void* face);
    int32_t  (*glyphAdvance)(void* ctx, void* face, uint32_t gid);
    int16_t  (*ascender)(void* ctx, void* face);
    int16_t  (*descender)(void* ctx, void* face);   /* negative (FT convention) */
    int16_t  (*height)(void* ctx, void* face);      /* FT face->height equivalent */

    /* Emit the outline of `gid` into `sink`, y-down, conics upgraded to
       cubics, all coordinates multiplied by `scale`. Returns 0 on failure
       (e.g. bitmap-only glyph). */
    int (*glyphOutline)(void* ctx, void* face, uint32_t gid, float scale,
                        const TvgGwPathSink* sink);

    /* Shape a single-face UTF-8 run. `locale` is a BCP47 tag or NULL.
       Calls `emit` once per shaped glyph in visual order. Returns the number
       of glyphs emitted. */
    uint32_t (*shapeRun)(void* ctx, void* face, const char* utf8, uint32_t len,
                         const char* locale,
                         void (*emit)(void* emitCtx, const TvgGwShapedGlyph* g),
                         void* emitCtx);
} TvgGwBridge;

/* Register the host bridge (the pointed-to struct is copied). Passing NULL
   clears the bridge. */
void tvgGwSetBridge(const TvgGwBridge* bridge);

/* Currently registered bridge, or NULL. */
const TvgGwBridge* tvgGwGetBridge(void);

#ifdef __cplusplus
}
#endif

#endif /* _THORVG_GW_BRIDGE_H_ */
