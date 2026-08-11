// SPDX-FileCopyrightText: 2026 kirijin <avel.ronin@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

// Default-preset render baseline (Phase 0, 2026-08-11).
// Renders the Default preset EXACTLY as the preset table defines it, at a fixed
// small size, saves the PNG and prints its sha256. Every later phase re-renders
// and must reproduce this hash byte-for-byte — the factory-baseline invariant.
// Determinism: Bayer dither is a static table (no RNG) and Default has grain=0,
// so the same inputs must produce identical bytes across runs; we render twice
// and assert that first.
#include "WallpaperProcessor.h"
#include "blur_presets.h"
#include <QGuiApplication>
#include <QImage>
#include <QBuffer>
#include <QCryptographicHash>
#include <cstdio>
#include <cstdlib>

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { std::printf("FAIL: %s\n", msg); ++failures; } \
    else { std::printf("ok:   %s\n", msg); } \
} while (0)

static QString sha256Hex(const QByteArray &ba)
{
    return QString::fromLatin1(QCryptographicHash::hash(ba, QCryptographicHash::Sha256).toHex());
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("walltz-test"));
    QCoreApplication::setApplicationName(QStringLiteral("walltz-baseline-test"));

    // Fixed synthetic source (deterministic; no randomness anywhere in the path).
    QImage src(320, 200, QImage::Format_ARGB32_Premultiplied);
    for (int y = 0; y < 200; ++y)
        for (int x = 0; x < 320; ++x)
            src.setPixelColor(x, y, QColor::fromHsv((x * 360) / 320, 160, 100 + y / 2));

    // Default preset row -> snapshot, exactly the app's Default path.
    const BlurConfig &cfg = blurPresetConfig(0);   // 0 == Default (locked)
    RenderSnapshot rs;
    rs.W = 640; rs.H = 360;
    rs.blurMode = true;
    rs.blurRadius       = qMax(0, (int)cfg.sigma);
    rs.saturationFactor = cfg.satBoost;
    rs.overlayOpacity   = cfg.overlayOpacity;
    rs.overlayColor     = cfg.overlayColor;
    rs.blurBrightness   = cfg.brightness;
    rs.vignetteStrength = cfg.vignette;
    rs.grainStrength    = cfg.grain;
    rs.photoFrame       = cfg.frameEnabled;
    rs.photoFrameWidth  = cfg.frameEnabled ? cfg.frameWidthPct : 0;
    rs.sourceImage      = src;

    QImage a = WallpaperProcessor::renderCore(rs, nullptr, nullptr, nullptr);
    QImage b = WallpaperProcessor::renderCore(rs, nullptr, nullptr, nullptr);
    CHECK(!a.isNull() && a.size() == QSize(640, 360), "baseline: output valid");
    CHECK(a == b, "baseline: render is deterministic (two passes identical)");

    QByteArray ba;
    QBuffer buffer(&ba);
    buffer.open(QIODevice::WriteOnly);
    a.save(&buffer, "PNG");
    QString hash = sha256Hex(ba);
    std::printf("BASELINE_SHA256 %s\n", qPrintable(hash));

    const char *outPath = std::getenv("WALLTZ_BASELINE_OUT");
    if (outPath && !a.save(QString::fromUtf8(outPath), "PNG"))
        std::printf("FAIL: could not save baseline png to %s\n", outPath);

    if (failures == 0)
        std::printf("ALL BASELINE CHECKS PASSED\n");
    return failures == 0 ? 0 : 1;
}
