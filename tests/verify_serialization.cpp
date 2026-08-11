// SPDX-FileCopyrightText: 2026 kirijin <avel.ronin@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "WallpaperProcessor.h"
#include <QGuiApplication>
#include <QSettings>
#include <QDir>
#include <QFile>
#include <QImage>
#include <cstdio>
static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { std::printf("FAIL: %s\n", msg); ++failures; } \
    else { std::printf("ok:   %s\n", msg); } \
} while (0)
int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("walltz-verify"));
    QCoreApplication::setApplicationName(QStringLiteral("walltz-verify"));
    WallpaperProcessor p;
    p.setBlurMode(true);
    p.setBlurRadius(42);
    p.setSaturationFactor(2.1);
    p.setBgZoom(1.4);
    p.setVignetteStrength(0.7);
    p.setPhotoFrame(true);
    p.setPhotoFrameWidth(9);
    p.rememberState();
    p.setBlurMode(false);
    p.setBlurRadius(5);
    p.setSaturationFactor(0.5);
    p.setBgZoom(0.6);
    p.setVignetteStrength(0.0);
    p.setPhotoFrame(false);
    p.setPhotoFrameWidth(0);
    CHECK(p.blurRadius() == 5, "mutated blurRadius differs");
    p.restoreState();
    CHECK(p.blurMode() == true, "F6 restore: blurMode");
    CHECK(p.blurRadius() == 42, "F6 restore: blurRadius == 42");
    CHECK(qFuzzyCompare(p.saturationFactor(), 2.1), "F6 restore: saturation == 2.1");
    CHECK(qFuzzyCompare(p.bgZoom(), 1.4), "F6 restore: bgZoom == 1.4");
    CHECK(qFuzzyCompare(p.vignetteStrength(), 0.7), "F6 restore: vignette == 0.7");
    CHECK(p.photoFrame() && p.photoFrameWidth() == 9, "F6 restore: frame on, width 9");
    // Phase 1: float color grade params survive F6 undo round-trip.
    p.setColorGamma(1.3);
    p.setColorWarmth(-0.4);
    p.setColorBlackLift(0.25);
    p.rememberState();
    p.setColorGamma(1.0);
    p.setColorWarmth(0.0);
    p.setColorBlackLift(0.0);
    p.restoreState();
    CHECK(qFuzzyCompare(p.colorGamma(), 1.3), "F6 restore: colorGamma == 1.3");
    CHECK(qFuzzyCompare(p.colorWarmth(), -0.4), "F6 restore: colorWarmth == -0.4");
    CHECK(qFuzzyCompare(p.colorBlackLift(), 0.25), "F6 restore: colorBlackLift == 0.25");
    p.setBgGradientStyle(2);
    p.setAutoMood(3);
    p.setUseV2(true);
    p.setColorGamma(0.9);
    p.setColorWarmth(0.6);
    p.setColorBlackLift(0.1);
    p.saveParamPreset(QStringLiteral("verify-test-preset"));
    CHECK(p.paramPresetNames().contains(QStringLiteral("verify-test-preset")),
          "F2: preset saved and listed");
    p.setBgGradientStyle(0);
    p.setAutoMood(0);
    p.setUseV2(false);
    p.setColorGamma(1.0);
    p.setColorWarmth(0.0);
    p.setColorBlackLift(0.0);
    p.applyParamPreset(QStringLiteral("verify-test-preset"));
    CHECK(p.bgGradientStyle() == 2, "F2: apply restored gradient style 2");
    CHECK(p.autoMood() == 3, "F2: apply restored mood 3");
    CHECK(p.useV2() == true, "F2: apply restored useV2");
    CHECK(qFuzzyCompare(p.colorGamma(), 0.9), "F2: apply restored colorGamma 0.9");
    CHECK(qFuzzyCompare(p.colorWarmth(), 0.6), "F2: apply restored colorWarmth 0.6");
    CHECK(qFuzzyCompare(p.colorBlackLift(), 0.1), "F2: apply restored colorBlackLift 0.1");
    p.deleteParamPreset(QStringLiteral("verify-test-preset"));
    CHECK(!p.paramPresetNames().contains(QStringLiteral("verify-test-preset")),
          "F2: preset deleted");
    p.setBlurRadius(9999);
    CHECK(p.blurRadius() == 120, "clamp: blurRadius capped at 120");
    p.setBgZoom(9.0);
    CHECK(qFuzzyCompare(p.bgZoom(), 3.0), "clamp: bgZoom capped at 3.0");

    // ── B3: pattern params must survive presets/undo (were silently dropped) ──
    p.setBgPatternEnabled(true);
    p.setBgPatternType(123);          // motif 23
    p.setBgPatternColor(QColor(10, 20, 30));
    p.setBgPatternScale(2.5);
    p.setBgPatternRotation(45.0);
    p.setBgPatternSpacing(1.5);
    p.setBgPatternRandomRotate(true);
    p.setBgPatternJitter(true);
    p.setBgPatternGridAmplitude(0.4);
    p.setBgPatternMixEnabled(true);
    p.setBgPatternMixMotifs(QVariantList() << 100 << 105);
    p.rememberState();
    p.setBgPatternEnabled(false);
    p.setBgPatternType(0);
    p.setBgPatternScale(1.0);
    p.setBgPatternMixEnabled(false);
    p.restoreState();
    CHECK(p.bgPatternEnabled() == true, "B3 undo: pattern enabled restored");
    CHECK(p.bgPatternType() == 123, "B3 undo: pattern type restored");
    CHECK(p.bgPatternColor() == QColor(10, 20, 30), "B3 undo: pattern color restored");
    CHECK(qFuzzyCompare(p.bgPatternScale(), 2.5), "B3 undo: pattern scale restored");
    CHECK(p.bgPatternRandomRotate() == true, "B3 undo: randomRotate restored");
    CHECK(p.bgPatternMixEnabled() == true, "B3 undo: mix enabled restored");
    CHECK(p.bgPatternMixMotifs() == QVariantList() << 100 << 105, "B3 undo: mix motifs restored");

    // Preset round-trip with patterns too.
    p.saveParamPreset(QStringLiteral("verify-b3-preset"));
    p.setBgPatternEnabled(false);
    p.setBgPatternType(0);
    p.applyParamPreset(QStringLiteral("verify-b3-preset"));
    CHECK(p.bgPatternEnabled() == true && p.bgPatternType() == 123,
          "B3 preset: pattern params restored from QSettings");
    p.deleteParamPreset(QStringLiteral("verify-b3-preset"));

    // ── Phase 2: overlay path handling + serialization ──
    // Missing asset -> path stays empty (never silently accepted).
    p.setTexturePath(QStringLiteral("/nonexistent/definitely-not-here.png"));
    CHECK(p.texturePath().isEmpty(), "overlay: missing asset keeps path empty");

    // Real asset -> set + F6 round-trip + F2 round-trip.
    const QString tmpOverlay = QDir::tempPath() + QStringLiteral("/walltz-verify-overlay.png");
    QImage ov(16, 16, QImage::Format_ARGB32_Premultiplied);
    ov.fill(QColor(200, 100, 50));
    if (!ov.save(tmpOverlay, "PNG")) {
        CHECK(false, "overlay: could not create temp asset");
    } else {
        p.setTexturePath(tmpOverlay);
        CHECK(p.texturePath() == tmpOverlay, "overlay: real asset accepted");
        p.setTextureOpacity(0.42);
        p.rememberState();
        p.setTexturePath(QString());
        p.setTextureOpacity(0.0);
        p.restoreState();
        CHECK(p.texturePath() == tmpOverlay, "F6 undo: overlay path restored");
        CHECK(qFuzzyCompare(p.textureOpacity(), 0.42), "F6 undo: overlay opacity restored");

        p.saveParamPreset(QStringLiteral("verify-overlay-preset"));
        p.setTexturePath(QString());
        p.setTextureOpacity(0.0);
        p.applyParamPreset(QStringLiteral("verify-overlay-preset"));
        CHECK(p.texturePath() == tmpOverlay, "F2 preset: overlay path restored");
        CHECK(qFuzzyCompare(p.textureOpacity(), 0.42), "F2 preset: overlay opacity restored");
        p.deleteParamPreset(QStringLiteral("verify-overlay-preset"));

        // Clear path and verify the resolved image is dropped too (render must
        // be unaffected by a previously set overlay).
        p.setTexturePath(QString());
        CHECK(p.texturePath().isEmpty(), "overlay: clear path");
        QFile::remove(tmpOverlay);
    }

    // ── Phase 3: retro preset application + locked-default invariant ──
    p.setBlurPresetIndex(static_cast<int>(BlurPresetId::RetroVintage));
    CHECK(qFuzzyCompare(p.colorGamma(), 1.08), "retro: vintage applies gamma 1.08");
    CHECK(qFuzzyCompare(p.colorWarmth(), 0.28), "retro: vintage applies warmth 0.28");
    CHECK(qFuzzyCompare(p.colorBlackLift(), 0.0), "retro: vintage applies blackLift 0.0");
    CHECK(qFuzzyCompare(p.saturationFactor(), 1.6), "retro: vintage applies satBoost 1.6");
    // Default is locked: re-selecting it must restore fully neutral color grade.
    p.setBlurPresetIndex(static_cast<int>(BlurPresetId::Default));
    CHECK(qFuzzyCompare(p.colorGamma(), 1.0), "retro->default: gamma neutral");
    CHECK(qFuzzyCompare(p.colorWarmth(), 0.0), "retro->default: warmth neutral");
    CHECK(qFuzzyCompare(p.colorBlackLift(), 0.0), "retro->default: blackLift neutral");

    std::printf(failures == 0 ? "\nALL PASS\n" : "\n%d FAILURES\n", failures);
    return failures == 0 ? 0 : 1;
}
