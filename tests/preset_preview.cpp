// SPDX-FileCopyrightText: 2026 kirijin <avel.ronin@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

// Preset preview renderer (Phase 3 tune loop, 2026-08-11).
// Renders EVERY preset from the table on three synthetic wallpaper-like
// sources (landscape / dark / vibrant) at 480x270 and saves PNGs to the
// given directory. Human eyes do the tuning; this makes the loop cheap.
// Usage: walltz_preset_preview <outdir> [optional: /path/to/photo.jpg ...]
#include "WallpaperProcessor.h"
#include "blur_presets.h"
#include <QGuiApplication>
#include <QImage>
#include <QDir>
#include <QFileInfo>
#include <cstdio>
#include <cmath>

static QImage landscapeSource()
{
    QImage img(320, 180, QImage::Format_ARGB32_Premultiplied);
    for (int y = 0; y < 180; ++y) {
        for (int x = 0; x < 320; ++x) {
            // sky-blue top fading to warm horizon, green-ish lower band
            QColor c;
            if (y < 110) {
                int t = y * 255 / 110;
                c = QColor::fromHsv(210 - t / 6, 120 + t / 2, 140 + t / 2);
            } else if (y < 140) {
                c = QColor::fromHsv(30, 150, 190 + (y - 110) * 2);
            } else {
                c = QColor::fromHsv(120, 90, 60 + (y - 140) * 2);
            }
            img.setPixelColor(x, y, c);
        }
    }
    return img;
}

static QImage darkSource()
{
    QImage img(320, 180, QImage::Format_ARGB32_Premultiplied);
    for (int y = 0; y < 180; ++y)
        for (int x = 0; x < 320; ++x)
            img.setPixelColor(x, y, QColor::fromHsv((x * 200) / 320, 90, 25 + y / 3));
    return img;
}

static QImage vibrantSource()
{
    QImage img(320, 180, QImage::Format_ARGB32_Premultiplied);
    for (int y = 0; y < 180; ++y)
        for (int x = 0; x < 320; ++x)
            img.setPixelColor(x, y, QColor::fromHsv((x * 360) / 320, 220, 130 + y / 3));
    return img;
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("walltz-test"));
    QCoreApplication::setApplicationName(QStringLiteral("walltz-preset-preview"));

    QString outDir = argc > 1 ? QString::fromUtf8(argv[1]) : QStringLiteral(".");
    QDir().mkpath(outDir);

    struct Source { QString name; QImage img; };
    QVector<Source> sources = {
        { QStringLiteral("landscape"), landscapeSource() },
        { QStringLiteral("dark"),      darkSource() },
        { QStringLiteral("vibrant"),   vibrantSource() },
    };
    // Optional real photos appended via argv[2..]
    for (int i = 2; i < argc; ++i) {
        QImage photo(QString::fromUtf8(argv[i]));
        if (!photo.isNull()) {
            QImage scaled = photo.scaled(320, 180, Qt::KeepAspectRatioByExpanding);
            // Crop to the exact cell so every preview shares one aspect.
            scaled = scaled.copy((scaled.width() - 320) / 2, (scaled.height() - 180) / 2, 320, 180);
            sources.append({ QFileInfo(QString::fromUtf8(argv[i])).completeBaseName(), scaled });
        }
    }

    for (int p = 0; p < blurPresetCount(); ++p) {
        const BlurConfig &cfg = blurPresetConfig(p);
        for (const Source &s : sources) {
            RenderSnapshot rs;
            rs.W = 480; rs.H = 270;
            rs.blurMode = true;
            rs.blurRadius       = qMax(0, (int)cfg.sigma);
            rs.saturationFactor = cfg.satBoost;
            rs.overlayOpacity   = cfg.overlayOpacity;
            rs.overlayColor     = cfg.overlayColor;
            rs.blurBrightness   = cfg.brightness;
            rs.vignetteStrength = cfg.vignette;
            rs.grainStrength    = cfg.grain;
            rs.colorGamma       = cfg.gamma;
            rs.colorWarmth      = cfg.warmth;
            rs.colorBlackLift   = cfg.blackLift;
            rs.photoGrade       = cfg.photoGrade;
            rs.photoFrame       = cfg.frameEnabled;
            rs.photoFrameWidth  = cfg.frameEnabled ? cfg.frameWidthPct : 0;
            rs.sourceImage      = s.img;
            QImage out = WallpaperProcessor::renderCore(rs, nullptr, nullptr, nullptr);
            QString fname = outDir + QStringLiteral("/%1_%2.png")
                              .arg(QString::fromUtf8(cfg.id), s.name);
            if (!out.save(fname, "PNG"))
                std::printf("FAIL: could not save %s\n", qPrintable(fname));
            else
                std::printf("saved %s\n", qPrintable(fname));
        }
    }
    return 0;
}
