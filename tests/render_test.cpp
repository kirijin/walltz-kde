// SPDX-FileCopyrightText: 2026 kirijin <avel.ronin@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

// Standalone render-core verification for walltz (not shipped; built in /tmp).
// Exercises the unified renderCore in all three background modes + effects,
// asserting output validity — the Demonstration for the refactor.
#include "WallpaperProcessor.h"
#include <QGuiApplication>
#include <cstdio>
#include <cmath>

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { std::printf("FAIL: %s\n", msg); ++failures; } \
    else { std::printf("ok:   %s\n", msg); } \
} while (0)

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("walltz-test"));
    QCoreApplication::setApplicationName(QStringLiteral("walltz-render-test"));

    // 400x300 gradient test source
    QImage src(400, 300, QImage::Format_ARGB32_Premultiplied);
    for (int y = 0; y < 300; ++y)
        for (int x = 0; x < 400; ++x)
            src.setPixelColor(x, y, QColor::fromHsv((x * 360) / 400, 180, 120 + y / 3));

    // ── Blur mode, default params, 1920x1080 ──
    {
        RenderSnapshot rs;
        rs.W = 1920; rs.H = 1080;
        rs.blurMode = true;
        rs.blurRadius = 90;
        rs.saturationFactor = 1.8;
        rs.sourceImage = src;
        double mn = 0, mx = 0;
        QImage out = WallpaperProcessor::renderCore(rs, &mn, &mx, nullptr);
        CHECK(!out.isNull(), "blur: output non-null");
        CHECK(out.size() == QSize(1920, 1080), "blur: correct size");
        CHECK(mx > 0.5 && mx <= 1.0, "blur: fg zoom bounds sane");
        CHECK(out.pixelColor(960, 540) != QColor(0, 0, 0), "blur: center not pure black");
        CHECK(out.pixelColor(960, 540).alpha() == 255, "blur: opaque");
    }

    // ── Colour mode: solid + autoColor ──
    {
        RenderSnapshot rs;
        rs.W = 1280; rs.H = 720;
        rs.blurMode = false;
        rs.bgGradientStyle = 0;
        rs.autoColor = true;
        rs.sourceImage = src;
        QImage out = WallpaperProcessor::renderCore(rs, nullptr, nullptr, nullptr);
        CHECK(!out.isNull() && out.size() == QSize(1280, 720), "solid: output valid");
        // Corner should be a mood-derived colour, foreground centered.
        QColor corner = out.pixelColor(0, 0);
        CHECK(corner.alpha() == 255, "solid: opaque bg");
    }

    // ── Colour mode: gradient preset ──
    {
        RenderSnapshot rs;
        rs.W = 1280; rs.H = 720;
        rs.blurMode = false;
        rs.bgGradientStyle = 1;
        rs.bgGradientPreset = 3;
        rs.gradientAngle = 45.0;
        rs.sourceImage = src;
        QImage out = WallpaperProcessor::renderCore(rs, nullptr, nullptr, nullptr);
        CHECK(!out.isNull(), "gradient: output valid");
        QColor tl = out.pixelColor(5, 5), br = out.pixelColor(1275, 715);
        CHECK(tl != br, "gradient: two ends differ (real gradient)");
    }

    // ── Mood (auto) gradient ──
    {
        RenderSnapshot rs;
        rs.W = 1280; rs.H = 720;
        rs.blurMode = false;
        rs.bgGradientStyle = 2;
        rs.moodColorA = 0xff306090;
        rs.moodColorB = 0xff90c060;
        rs.sourceImage = src;
        QImage out = WallpaperProcessor::renderCore(rs, nullptr, nullptr, nullptr);
        CHECK(!out.isNull(), "mood: output valid");
        QColor tl = out.pixelColor(5, 5), br = out.pixelColor(1275, 715);
        CHECK(tl != br, "mood: gradient ends differ");
    }

    // ── Pattern overlay (mix of motif + svg-geo — Q8 collision regression) ──
    {
        RenderSnapshot rs;
        rs.W = 800; rs.H = 600;
        rs.blurMode = false;
        rs.bgGradientStyle = 0;
        rs.autoColor = false;
        rs.bgColor = 0xff404040;
        rs.bgPatternEnabled = true;
        rs.bgPatternMixEnabled = true;
        // 2 motifs + 2 SVG-geo: mixed tile must render both kinds correctly
        // (the old code collapsed SVG-geo indices into the motif table → Q8).
        rs.bgPatternMixMotifs = QList<int>() << WallpaperProcessor::MOTIF_OFFSET + 0   // cat
                                              << WallpaperProcessor::MOTIF_OFFSET + 14  // leaf
                                              << WallpaperProcessor::SVG_GEO_OFFSET + 0 // squares
                                              << WallpaperProcessor::SVG_GEO_OFFSET + 2;// circles
        rs.sourceImage = src;
        QImage out = WallpaperProcessor::renderCore(rs, nullptr, nullptr, nullptr);
        CHECK(!out.isNull(), "pattern-mix: output valid");
        CHECK(out.pixelColor(400, 300).alpha() == 255, "pattern-mix: opaque");
    }

    // ── Effects: vignette + grain + CA + frame (all on) ──
    {
        RenderSnapshot rs;
        rs.W = 1280; rs.H = 800;
        rs.blurMode = false;
        rs.bgGradientStyle = 0;
        rs.autoColor = false;
        rs.bgColor = 0xffe0e0e0;
        rs.vignetteStrength = 0.5;
        rs.grainStrength = 0.4;
        rs.caStrength = 0.6;
        rs.photoFrame = true;
        rs.photoFrameWidth = 5;
        rs.sourceImage = src;
        QImage out = WallpaperProcessor::renderCore(rs, nullptr, nullptr, nullptr);
        CHECK(!out.isNull(), "effects: output valid");
        CHECK(out.pixelColor(640, 400).alpha() == 255, "effects: opaque");
    }

    // ── Edge: null source must not crash ──
    {
        RenderSnapshot rs;
        rs.W = 100; rs.H = 100;
        rs.sourceImage = QImage();
        QImage out = WallpaperProcessor::renderCore(rs, nullptr, nullptr, nullptr);
        CHECK(out.isNull(), "null source: returns null (no crash)");
    }

    // ── Float color grade (Phase 1): gamma / warmth / blackLift ──
    // All tests render a flat source; corners sample pure background. NOTE:
    // blur mode draws a 25-alpha black overlay before blurring (composition
    // design, renderCore), so the pre-grade gray is 128*(1-25/255) ≈ 115.45,
    // not 128. Expectations are therefore derived from the NEUTRAL control
    // render (same composition, no grade) — self-consistent, robust to
    // composition details. Dither tolerance ±2-3.
    QImage flat(64, 64, QImage::Format_ARGB32_Premultiplied);
    flat.fill(QColor(128, 128, 128));
    QImage black(64, 64, QImage::Format_ARGB32_Premultiplied);
    black.fill(QColor(0, 0, 0));

    auto neutralBase = [&](const QImage &src) {
        RenderSnapshot rs;
        rs.W = 128; rs.H = 128;
        rs.blurMode = true;
        rs.blurRadius = 4;
        rs.saturationFactor = 1.0;
        rs.sourceImage = src;
        QImage out = WallpaperProcessor::renderCore(rs, nullptr, nullptr, nullptr);
        return out.pixelColor(4, 4);
    };

    {
        // gamma=2.0 on mid-gray: out = 255*(v0/255)^2 where v0 = neutral gray
        QColor n = neutralBase(flat);
        RenderSnapshot rs;
        rs.W = 128; rs.H = 128;
        rs.blurMode = true;
        rs.blurRadius = 4;
        rs.saturationFactor = 1.0;
        rs.colorGamma = 2.0;
        rs.sourceImage = flat;
        QImage out = WallpaperProcessor::renderCore(rs, nullptr, nullptr, nullptr);
        int expected = qRound(255.0 * std::pow(n.red() / 255.0, 2.0));
        int v = out.pixelColor(4, 4).red();
        CHECK(qAbs(v - expected) <= 3, "color-grade: gamma=2.0 matches 255*(v0/255)^2");
        CHECK(out.pixelColor(4, 4).alpha() == 255, "color-grade: opaque");
    }
    {
        // warmth=1.0: r *= 1.15, b *= 0.85 on the neutral base
        QColor n = neutralBase(flat);
        RenderSnapshot rs;
        rs.W = 128; rs.H = 128;
        rs.blurMode = true;
        rs.blurRadius = 4;
        rs.saturationFactor = 1.0;
        rs.colorWarmth = 1.0;
        rs.sourceImage = flat;
        QImage out = WallpaperProcessor::renderCore(rs, nullptr, nullptr, nullptr);
        QColor c = out.pixelColor(4, 4);
        int er = qRound(qMin(255.0, n.red() * 1.15));
        int eb = qRound(qMin(255.0, n.blue() * 0.85));
        CHECK(qAbs(c.red() - er) <= 3 && qAbs(c.blue() - eb) <= 3,
              "color-grade: warmth=1.0 scales r up, b down per design");
    }
    {
        // blackLift=0.5 on black: floor at 127.5 -> >= 126
        RenderSnapshot rs;
        rs.W = 128; rs.H = 128;
        rs.blurMode = true;
        rs.blurRadius = 4;
        rs.saturationFactor = 1.0;
        rs.colorBlackLift = 0.5;
        rs.sourceImage = black;
        QImage out = WallpaperProcessor::renderCore(rs, nullptr, nullptr, nullptr);
        QColor c = out.pixelColor(4, 4);
        CHECK(c.red() >= 126 && c.green() >= 126 && c.blue() >= 126,
              "color-grade: blackLift=0.5 floors black at ~127");
    }
    {
        // Neutral params == no-op: gray stays gray, no channel skew
        QColor c = neutralBase(flat);
        CHECK(qAbs(c.red() - c.blue()) <= 2, "color-grade: neutral params leave gray neutral");
    }

    // ── Procedural overlay textures (looks): generator + placement ──
    // Generator direct tests.
    {
        // kind 1 (light leak): alpha falls off from the top-right corner.
        QImage tex = WallpaperProcessor::generateOverlayTexture(1, 0, 128, 128, 30, 30, 68, 68);
        CHECK(tex.pixelColor(124, 4).alpha() > 60, "tex: leak strong near top-right");
        CHECK(tex.pixelColor(124, 4).alpha() > tex.pixelColor(4, 4).alpha(),
              "tex: leak falls off away from the corner");
    }
    {
        // kind 2 (polaroid frame): white band hugging the rect, transparent center.
        QImage tex = WallpaperProcessor::generateOverlayTexture(2, 0, 128, 128, 30, 30, 68, 68);
        QColor topBand = tex.pixelColor(64, 24);    // inside pr.top..+bw (bw=10, pr.top=20)
        QColor center = tex.pixelColor(64, 64);
        CHECK(topBand.red() > 245 && topBand.green() > 245, "tex: polaroid top band is white");
        CHECK(center.alpha() == 0, "tex: polaroid center is transparent");
    }
    {
        // kind 3 (film border): dark at the edges, clear in the middle.
        QImage tex = WallpaperProcessor::generateOverlayTexture(3, 0, 128, 128, 30, 30, 68, 68);
        CHECK(tex.pixelColor(2, 64).alpha() > tex.pixelColor(64, 64).alpha(),
              "tex: film border dark at the edges");
    }
    {
        // kind 0: nothing.
        QImage tex = WallpaperProcessor::generateOverlayTexture(0, 0, 128, 128, 30, 30, 68, 68);
        CHECK(tex.isNull(), "tex: kind 0 returns null");
    }

    // Placement through renderCore (blurMode off, white background, gray photo).
    auto texRender = [&](int kind, bool over, double opacity = 1.0) {
        RenderSnapshot rs;
        rs.W = 128; rs.H = 128;
        rs.blurMode = false;
        rs.bgGradientStyle = 0;
        rs.autoColor = false;
        rs.bgColor = 0xffffffff;   // white background
        rs.textureKind = kind;
        rs.textureOpacity = opacity;
        rs.textureOverPhoto = over;
        rs.sourceImage = flat;
        return WallpaperProcessor::renderCore(rs, nullptr, nullptr, nullptr);
    };
    {
        // Leak UNDER the photo: background tinted warm top-right, photo clean.
        QImage out = texRender(1, false);
        QColor corner = out.pixelColor(124, 4);      // background, leak-strong zone
        QColor center = out.pixelColor(64, 64);      // photo
        CHECK(corner.red() > corner.blue(), "tex: leak under tints the background warm");
        CHECK(qAbs(center.red() - center.blue()) <= 8,
              "tex: leak under leaves the photo untinted (z-order locked)");
    }
    {
        // Leak OVER the photo: the photo itself gets the warm tint.
        QImage out = texRender(1, true);
        QColor center = out.pixelColor(64, 64);
        CHECK(center.red() > center.blue(), "tex: leak over tints the photo itself");
    }
    {
        // Film border under: the edge pixel darkens vs the same render with no
        // texture (the center is the photo, not a valid background reference).
        QImage plain = texRender(0, false);
        QImage out = texRender(3, false);
        auto lum = [](const QColor &c) { return c.red() + c.green() + c.blue(); };
        CHECK(lum(out.pixelColor(2, 64)) < lum(plain.pixelColor(2, 64)),
              "tex: film border darkens the edges");
    }
    {
        // No texture (kind 0): background stays pure white.
        QImage out = texRender(0, false);
        QColor c = out.pixelColor(4, 4);
        CHECK(c.red() > 250 && c.green() > 250 && c.blue() > 250,
              "tex: kind 0 leaves the background untouched");
    }

    // ── Retro look (4th tab): photo-grade + texture placement ──
    {
        // photoGrade: the FOREGROUND photo itself gets the grade (warmth=1.0
        // on flat gray -> red up, blue down at the canvas center == photo).
        RenderSnapshot rs;
        rs.W = 128; rs.H = 128;
        rs.blurMode = false;
        rs.bgGradientStyle = 0;
        rs.autoColor = false;
        rs.bgColor = 0xffffffff;
        rs.photoGrade = true;
        rs.saturationFactor = 1.0;
        rs.colorWarmth = 1.0;
        rs.sourceImage = flat;
        QImage out = WallpaperProcessor::renderCore(rs, nullptr, nullptr, nullptr);
        QColor c = out.pixelColor(64, 64);   // photo center
        CHECK(c.red() - c.blue() > 25, "look: photoGrade warms the photo itself");
    }
    {
        // B&W look: satBoost 0 through gradedCopy -> the photo turns gray.
        QImage redPhoto(64, 64, QImage::Format_ARGB32_Premultiplied);
        redPhoto.fill(QColor(255, 0, 0));
        RenderSnapshot rs;
        rs.W = 128; rs.H = 128;
        rs.blurMode = false;
        rs.bgGradientStyle = 0;
        rs.autoColor = false;
        rs.bgColor = 0xffffffff;
        rs.photoGrade = true;
        rs.saturationFactor = 0.0;
        rs.sourceImage = redPhoto;
        QImage out = WallpaperProcessor::renderCore(rs, nullptr, nullptr, nullptr);
        QColor c = out.pixelColor(64, 64);
        CHECK(qAbs(c.red() - c.green()) <= 3 && qAbs(c.green() - c.blue()) <= 3,
              "look: satBoost 0 renders the photo grayscale (B&W look)");
    }
    {
        // Look table sanity: every photoGrade row in range, every look has a
        // texture kind (procedural overlays are part of the look).
        int lookRows = 0;
        for (int i = 0; i < blurPresetCount(); ++i) {
            const BlurConfig &l = blurPresetConfig(i);
            if (!l.photoGrade) continue;
            lookRows++;
            CHECK(l.satBoost >= 0.0 && l.satBoost <= 3.0, "look: satBoost in range");
            CHECK(l.gamma >= 0.5 && l.gamma <= 2.5, "look: gamma in range");
            CHECK(l.warmth >= -1.0 && l.warmth <= 1.0, "look: warmth in range");
            CHECK(l.blackLift >= 0.0 && l.blackLift <= 1.0, "look: blackLift in range");
            CHECK(l.frameWidthPct >= 0 && l.frameWidthPct <= 25, "look: frame width in range");
            // A look is complete with a frame OR a procedural texture
            // (Polaroid uses the built-in matte frame — default rounding +
            // shadow; the others carry atmosphere textures).
            CHECK((l.textureKind >= 1 && l.textureKind <= 3) || l.frameEnabled,
                  "look: every look has a frame or a texture");
        }
        CHECK(lookRows == 5, "look: exactly 5 photoGrade rows");
    }

    std::printf(failures == 0 ? "\nALL PASS\n" : "\n%d FAILURES\n", failures);
    return failures == 0 ? 0 : 1;
}
