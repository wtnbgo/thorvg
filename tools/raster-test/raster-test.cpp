/*
 * raster-test: SW engine raster output golden-compare tool.
 *
 * Renders a fixed set of scenes that mirror the raster paths a GUI toolkit
 * (cycfi/elements) actually exercises, and prints a 64bit FNV-1a hash of the
 * resulting ARGB8888 buffer per scene. Run the same binary built with
 * TVG_SIMD=ON and TVG_SIMD=OFF (or on different architectures) and diff the
 * stdout — every line must match bit-exactly, since all blend kernels are
 * integer arithmetic with identical rounding in scalar/AVX/NEON variants.
 *
 * Covered raster paths:
 *   solid_rect        rasterSolidRect        (rasterPixel32 memfill)
 *   trans_rect        rasterTranslucentRect  (AVX/NEON kernel)
 *   solid_rle_aa      rasterSolidRle         (full spans + AA edge spans)
 *   trans_rle         rasterTranslucentRle   (AVX/NEON kernel)
 *   stroke            stroke tessellation -> RLE fill
 *   grad_linear/rad   fetch + blend of gradient spans
 *   image_blit        direct image blit (identity transform)
 *   image_transform   texmap (scale + rotate)
 *   scene_opacity     composition buffer blend
 *   clip              RLE clipping
 *
 * Text is not tested directly: glyphs render as solid-color RLE fills, the
 * exact same path as solid_rle_aa / trans_rle.
 *
 * Usage:
 *   tvg-raster-test [--dump <dir>] [--perf <iters>]
 *     --dump <dir>  additionally write raw ARGB buffers as <scene>.bin
 *     --perf <N>    render each scene N times and report ms (for on-target
 *                   profiling; hashes are still printed)
 */

#include <thorvg.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <chrono>

namespace {

constexpr uint32_t W = 320;
constexpr uint32_t H = 240;

// Deterministic LCG so the destination buffer has non-trivial content —
// blending against a varied dst is what catches src/dst mixup bugs.
uint32_t lcg_state = 0x12345678u;
uint32_t lcg()
{
    lcg_state = lcg_state * 1664525u + 1013904223u;
    return lcg_state;
}

void seed_background(uint32_t* buf)
{
    lcg_state = 0x12345678u;
    for (uint32_t i = 0; i < W * H; ++i) {
        // opaque, so the premultiplied-ARGB invariant (ch <= a) always holds
        buf[i] = 0xff000000u | (lcg() & 0x00ffffffu);
    }
}

uint64_t fnv1a(const void* data, size_t len)
{
    auto p = static_cast<const uint8_t*>(data);
    uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < len; ++i) {
        h ^= p[i];
        h *= 1099511628211ull;
    }
    return h;
}

// 64x64 premultiplied-ARGB test pattern for Picture scenes
constexpr uint32_t IMG = 64;
uint32_t img_data[IMG * IMG];

void make_image()
{
    for (uint32_t y = 0; y < IMG; ++y) {
        for (uint32_t x = 0; x < IMG; ++x) {
            uint32_t a = 64 + ((x + y) * 191) / (2 * IMG - 2);  // 64..255
            uint32_t r = (x * 255) / (IMG - 1);
            uint32_t g = (y * 255) / (IMG - 1);
            uint32_t b = ((x ^ y) * 255) / (IMG - 1);
            // premultiply
            r = r * a / 255; g = g * a / 255; b = b * a / 255;
            img_data[y * IMG + x] = (a << 24) | (r << 16) | (g << 8) | b;
        }
    }
}

using SceneFn = void (*)(tvg::Canvas*);

void scene_solid_rect(tvg::Canvas* cv)
{
    auto s1 = tvg::Shape::gen();
    s1->appendRect(8, 8, 200, 100);
    s1->fill(200, 60, 30);
    cv->add(s1);
    auto s2 = tvg::Shape::gen();
    s2->appendRect(96, 64, 160, 120);
    s2->fill(30, 160, 90);
    cv->add(s2);
    // 1px wide / tall rects: leftovers path of the memfill kernels
    auto s3 = tvg::Shape::gen();
    s3->appendRect(300, 5, 1, 200);
    s3->fill(255, 255, 0);
    cv->add(s3);
}

void scene_trans_rect(tvg::Canvas* cv)
{
    uint8_t alphas[] = {32, 128, 200};
    for (int i = 0; i < 3; ++i) {
        auto s = tvg::Shape::gen();
        s->appendRect(10.f + i * 60, 10.f + i * 40, 180, 120);
        s->fill(240, 80 + i * 40, 40, alphas[i]);
        cv->add(s);
    }
    // odd x offset / odd width: exercises the unaligned head + leftover tail
    auto s = tvg::Shape::gen();
    s->appendRect(3, 200, 313, 33);
    s->fill(80, 80, 220, 100);
    cv->add(s);
}

void scene_solid_rle_aa(tvg::Canvas* cv)
{
    auto c1 = tvg::Shape::gen();
    c1->appendCircle(90, 90, 70, 70);
    c1->fill(220, 100, 40);
    cv->add(c1);
    auto rr = tvg::Shape::gen();
    rr->appendRect(140.5f, 60.25f, 150, 130, 18, 18);  // fractional pos
    rr->fill(60, 120, 220);
    cv->add(rr);
    auto tri = tvg::Shape::gen();
    tri->moveTo(20, 230);
    tri->lineTo(300, 150);
    tri->lineTo(310, 235);
    tri->close();
    tri->fill(40, 200, 120);
    cv->add(tri);
}

void scene_trans_rle(tvg::Canvas* cv)
{
    auto c1 = tvg::Shape::gen();
    c1->appendCircle(100, 100, 80, 60);
    c1->fill(220, 100, 40, 100);
    cv->add(c1);
    auto c2 = tvg::Shape::gen();
    c2->appendCircle(180, 130, 90, 80);
    c2->fill(40, 100, 220, 160);
    cv->add(c2);
    auto rr = tvg::Shape::gen();
    rr->appendRect(30.75f, 30.25f, 250, 90, 24, 24);
    rr->fill(120, 220, 60, 90);
    cv->add(rr);
}

void scene_stroke(tvg::Canvas* cv)
{
    auto l1 = tvg::Shape::gen();
    l1->moveTo(10, 10);
    l1->lineTo(310, 230);
    l1->strokeWidth(3);
    l1->strokeFill(255, 220, 40);
    cv->add(l1);
    auto l2 = tvg::Shape::gen();
    l2->moveTo(10, 230);
    l2->lineTo(310, 10);
    l2->strokeWidth(1);
    l2->strokeFill(40, 220, 255, 128);
    cv->add(l2);
    auto rr = tvg::Shape::gen();
    rr->appendRect(60, 40, 200, 160, 12, 12);
    rr->strokeWidth(2.5f);
    rr->strokeFill(230, 230, 230);
    cv->add(rr);
}

void scene_grad_linear(tvg::Canvas* cv)
{
    tvg::Fill::ColorStop stops[3] = {
        {0.0f, 255, 40, 40, 255},
        {0.5f, 40, 255, 40, 200},
        {1.0f, 40, 40, 255, 60},
    };
    auto g = tvg::LinearGradient::gen();
    g->linear(0, 0, 320, 240);
    g->colorStops(stops, 3);
    auto s = tvg::Shape::gen();
    s->appendRect(10, 10, 300, 150);
    s->fill(g);
    cv->add(s);

    auto g2 = tvg::LinearGradient::gen();
    g2->linear(0, 160, 0, 240);
    tvg::Fill::ColorStop stops2[2] = {
        {0.0f, 250, 250, 250, 255},
        {1.0f, 20, 20, 20, 255},
    };
    g2->colorStops(stops2, 2);
    auto c = tvg::Shape::gen();
    c->appendCircle(160, 195, 120, 40);
    c->fill(g2);
    cv->add(c);
}

void scene_grad_radial(tvg::Canvas* cv)
{
    tvg::Fill::ColorStop stops[3] = {
        {0.0f, 255, 255, 200, 255},
        {0.6f, 220, 120, 40, 180},
        {1.0f, 40, 20, 90, 40},
    };
    auto g = tvg::RadialGradient::gen();
    g->radial(160, 120, 140, 160, 120, 0);
    g->colorStops(stops, 3);
    auto s = tvg::Shape::gen();
    s->appendRect(0, 0, 320, 240);
    s->fill(g);
    cv->add(s);
}

void scene_image_blit(tvg::Canvas* cv)
{
    auto p = tvg::Picture::gen();
    p->load(img_data, IMG, IMG, tvg::ColorSpace::ARGB8888, true);
    p->translate(24, 24);
    cv->add(p);
}

void scene_image_transform(tvg::Canvas* cv)
{
    auto p = tvg::Picture::gen();
    p->load(img_data, IMG, IMG, tvg::ColorSpace::ARGB8888, true);
    p->translate(120, 60);
    p->scale(1.75f);
    p->rotate(30);
    cv->add(p);
}

void scene_image_scaled(tvg::Canvas* cv)
{
    // axis-aligned scaling (no rotation) — the common UI path (rasterScaledImage)
    auto p = tvg::Picture::gen();
    p->load(img_data, IMG, IMG, tvg::ColorSpace::ARGB8888, true);
    p->translate(20, 20);
    p->scale(2.6f);
    cv->add(p);
    auto p2 = tvg::Picture::gen();
    p2->load(img_data, IMG, IMG, tvg::ColorSpace::ARGB8888, true);
    p2->translate(190, 90);
    p2->scale(1.3f);
    p2->opacity(160);
    cv->add(p2);
}

void scene_scene_opacity(tvg::Canvas* cv)
{
    auto sc = tvg::Scene::gen();
    auto s1 = tvg::Shape::gen();
    s1->appendRect(20, 20, 200, 140);
    s1->fill(255, 60, 60);
    sc->add(s1);
    auto s2 = tvg::Shape::gen();
    s2->appendCircle(200, 140, 90, 70);
    s2->fill(60, 60, 255);
    sc->add(s2);
    sc->opacity(128);
    cv->add(sc);
}

void scene_clip(tvg::Canvas* cv)
{
    auto s = tvg::Shape::gen();
    s->appendRect(0, 0, 320, 240);
    tvg::Fill::ColorStop stops[2] = {
        {0.0f, 250, 160, 30, 255},
        {1.0f, 30, 160, 250, 255},
    };
    auto g = tvg::LinearGradient::gen();
    g->linear(0, 0, 320, 0);
    g->colorStops(stops, 2);
    s->fill(g);
    auto clip = tvg::Shape::gen();
    clip->appendRect(40.5f, 30.5f, 240, 180, 40, 40);
    s->clip(clip);
    cv->add(s);
}

struct SceneDef { const char* name; SceneFn fn; };
const SceneDef scenes[] = {
    {"solid_rect",      scene_solid_rect},
    {"trans_rect",      scene_trans_rect},
    {"solid_rle_aa",    scene_solid_rle_aa},
    {"trans_rle",       scene_trans_rle},
    {"stroke",          scene_stroke},
    {"grad_linear",     scene_grad_linear},
    {"grad_radial",     scene_grad_radial},
    {"image_blit",      scene_image_blit},
    {"image_scaled",    scene_image_scaled},
    {"image_transform", scene_image_transform},
    {"scene_opacity",   scene_scene_opacity},
    {"clip",            scene_clip},
};

} // namespace

int main(int argc, char** argv)
{
    const char* dump_dir = nullptr;
    int perf_iters = 0;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--dump") && i + 1 < argc) dump_dir = argv[++i];
        else if (!strcmp(argv[i], "--perf") && i + 1 < argc) perf_iters = atoi(argv[++i]);
        else {
            fprintf(stderr, "usage: %s [--dump <dir>] [--perf <iters>]\n", argv[0]);
            return 2;
        }
    }

    if (tvg::Initializer::init(0) != tvg::Result::Success) {
        fprintf(stderr, "tvg init failed\n");
        return 1;
    }
    make_image();

    static uint32_t buf[W * H];
    uint64_t total = 1469598103934665603ull;
    bool failed = false;

    for (auto& sd : scenes) {
        // fresh canvas per scene: no cross-scene state
        auto cv = tvg::SwCanvas::gen(tvg::EngineOption::None);
        if (!cv) { fprintf(stderr, "canvas gen failed\n"); return 1; }
        cv->target(buf, W, W, H, tvg::ColorSpace::ARGB8888);
        seed_background(buf);
        sd.fn(cv);
        if (cv->update() != tvg::Result::Success ||
            cv->draw(false) != tvg::Result::Success ||
            cv->sync() != tvg::Result::Success) {
            printf("%-16s RENDER-FAILED\n", sd.name);
            failed = true;
            delete cv;
            continue;
        }

        uint64_t h = fnv1a(buf, sizeof(buf));
        total ^= h;
        total *= 1099511628211ull;

        if (perf_iters > 0) {
            // re-render the same paint list; update() marks everything dirty
            auto t0 = std::chrono::steady_clock::now();
            for (int it = 0; it < perf_iters; ++it) {
                cv->update();
                cv->draw(false);
                cv->sync();
            }
            auto t1 = std::chrono::steady_clock::now();
            double ms = std::chrono::duration<double, std::milli>(t1 - t0).count() / perf_iters;
            printf("%-16s %016llx  %8.3f ms\n", sd.name, (unsigned long long)h, ms);
        } else {
            printf("%-16s %016llx\n", sd.name, (unsigned long long)h);
        }

        if (dump_dir) {
            char path[1024];
            snprintf(path, sizeof(path), "%s/%s.bin", dump_dir, sd.name);
            if (FILE* f = fopen(path, "wb")) {
                fwrite(buf, 1, sizeof(buf), f);
                fclose(f);
            } else {
                fprintf(stderr, "cannot write %s\n", path);
            }
        }
        delete cv;
    }

    printf("%-16s %016llx\n", "TOTAL", (unsigned long long)total);
    tvg::Initializer::term();
    return failed ? 1 : 0;
}
