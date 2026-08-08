// SPDX-FileCopyrightText: 2026 kirijin <avel.ronin@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

// Standalone render-core verification for walltz (not shipped; built in /tmp).
// Exercises the unified renderCore in all three background modes + effects,
// asserting output validity — the Demonstration for the refactor.
#include "WallpaperProcessor.h"
#include <QGuiApplication>
#include <cstdio>

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

    std::printf(failures == 0 ? "\nALL PASS\n" : "\n%d FAILURES\n", failures);
    return failures == 0 ? 0 : 1;
}
