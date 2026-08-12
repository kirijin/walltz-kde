// SPDX-FileCopyrightText: 2026 kirijin <avel.ronin@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "WallpaperProcessor.h"
#include <QGuiApplication>
#include <QSettings>
#include <QDir>
#include <QFile>
#include <QFileInfo>
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

    // ── Holistic looks in the blur preset table: apply + locked-default + F6 ──
    p.setBlurPresetIndex(static_cast<int>(BlurPresetId::Polaroid));   // sat 1.0, γ 0.94, warm -0.04, frame 3%
    CHECK(qFuzzyCompare(p.colorGamma(), 0.94), "look: polaroid applies gamma 0.94");
    CHECK(qFuzzyCompare(p.colorWarmth(), -0.04), "look: polaroid applies warmth -0.04");
    CHECK(qFuzzyCompare(p.saturationFactor(), 1.0), "look: polaroid applies satBoost 1.0");
    CHECK(p.photoFrame() && p.photoFrameWidth() == 3, "look: polaroid applies frame 3%");
    CHECK(p.photoGrade(), "look: polaroid turns photo grading on");
    CHECK(p.textureKind() == 0, "look: polaroid uses the built-in matte frame (no texture)");
    CHECK(p.blurPresetIndex() == static_cast<int>(BlurPresetId::Polaroid), "look: preset index tracked");
    // F6 undo keeps the look state.
    p.rememberState();
    p.setBlurPresetIndex(static_cast<int>(BlurPresetId::Default));   // locked -> factory reset
    CHECK(p.photoGrade() == false, "look: default clears photo grade");
    CHECK(p.textureKind() == 0, "look: default clears texture");
    CHECK(p.photoFrame() == false, "look: default clears frame");
    CHECK(qFuzzyCompare(p.colorGamma(), 1.0), "look: default clears grade");
    p.restoreState();
    CHECK(p.photoGrade(), "F6 undo: look photo grade restored");
    CHECK(p.photoFrame(), "F6 undo: look frame restored");
    CHECK(qFuzzyCompare(p.colorGamma(), 0.94), "F6 undo: look grade restored");

    std::printf(failures == 0 ? "\nALL PASS\n" : "\n%d FAILURES\n", failures);
    return failures == 0 ? 0 : 1;
}
