// SPDX-FileCopyrightText: 2026 kirijin <avel.ronin@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "blur_presets.h"
#include "WallpaperProcessor.h"   // for WalltzDefaults

// ── Canonical preset table ──────────────────────────────────────────────
// Order must match BlurPresetId enum exactly. The "default" row derives its
// sigma/sat/brightness from WalltzDefaults so there is one source of truth (Q5).
static const BlurConfig s_presets[] = {
    // id       display       σ     sat   bright  ovlOp  ovlColor   vig   grain  frameEn  frameW   γ     warm  lift  texKind  texOp texBlend texOver texColor  photo
    { "default", "Default", WalltzDefaults::blurRadius, WalltzDefaults::saturationFactor, WalltzDefaults::blurBrightness, WalltzDefaults::overlayOpacity, 0x000000, WalltzDefaults::vignetteStrength, WalltzDefaults::grainStrength, false, 0,  1.0,  0.0,  0.0,  0, 0.0,  13,  false, 0,    false },
    { "auto",    "Auto",      0.0,   1.0,  1.00,   0.0,   0x000000,  0.0,  0.0,  false, 0,  1.0,  0.0,  0.0,  0, 0.0,  13,  false, 0,    false },
    { "serenity","Serenity", 25,    0.30,  0.65,   0.55,  0x181824,  0.15,  0.0,  false, 0,  1.0,  0.0,  0.0,  0, 0.0,  13,  false, 0,    false },
    { "focus",   "Focus",     8,    0.55,  0.90,   0.15,  0x1c1c1c,  0.0,  0.0,  false, 0,  1.0,  0.0,  0.0,  0, 0.0,  13,  false, 0,    false },
    { "comfort", "Comfort",  20,    0.80,  0.78,   0.35,  0x2a1f14,  0.25,  0.02, false, 0,  1.0,  0.0,  0.0,  0, 0.0,  13,  false, 0,    false },
    { "apple",   "Apple",   30,    1.8,  1.02,   0.50,  0x1a1a1a,  0.0,  0.0,  false, 0,  1.0,  0.0,  0.0,  0, 0.0,  13,  false, 0,    false },
    { "gnome",   "GNOME",   30,    1.0,  0.60,   0.0,   0x000000,  0.0,  0.0,  false, 0,  1.0,  0.0,  0.0,  0, 0.0,  13,  false, 0,    false },
    { "mica",    "Mica",    10,    0.9,  0.85,   0.30,  0x323232,  0.0,  0.0,  false, 0,  1.0,  0.0,  0.0,  0, 0.0,  13,  false, 0,    false },
    { "acrylic", "Acrylic", 30,    1.0,  0.90,   0.60,  0x202020,  0.0,  0.03, false, 0,  1.0,  0.0,  0.0,  0, 0.0,  13,  false, 0,    false },
    // Reddit lightbox (verified 2026-08-10, live DOM, DARK theme):
    // backdrop = image itself blurred blur(24px)≈σ24 at 30% opacity over dark
    // neutral #0E1113 → 0.3×blur + 0.7×#0B0E0F. "Lightbox" is a misnomer — the
    // backdrop is DARK, not light. Photo wears the built-in matte frame at 1%
    // width; corner rounding stays the frame default (no new frame style).
    { "reddit",  "Reddit",  24,    1.0,  1.00,   0.70,  0x0B0E0F,  0.0,  0.0,  true, 1,  1.0,  0.0,  0.0,  0, 0.0,  13,  false, 0,    false },
    // Holistic photo looks (2026-08-11): photoGrade rows grade the PHOTO
    // itself + background, and bundle frame/grain/vignette/texture — the
    // "olden era" presets. Curve shapes re-derived from XnRetro's LUTs and
    // classic film conventions, calm-tamed starters for the tune loop.
    // textureAsset (optional) resolves in the overlays dir; absent = look
    // still applies, texture stays off.
    { "kodachrome","Kodachrome", 90,  1.5,  1.00,   0.0,  0x000000,  0.0,  0.02, false, 0,  0.98,  0.22,  0.05,  1, 0.20,  13,  true,  0xE8A050, true  },
    { "polaroid",  "Polaroid",   90,  1.0,  1.00,   0.0,  0x000000,  0.15, 0.03, true,  3,  0.94, -0.04, 0.18,  2, 1.00,   0,   true,  0x000000, true  },
    { "vintage",   "Vintage",    90,  1.2,  1.00,   0.0,  0x000000,  0.20, 0.04, true,  1,  1.06,  0.30, 0.00,  1, 0.28,  13,  true,  0xC87030, true  },
    { "trix",      "Tri-X",      90,  0.0,  1.00,   0.0,  0x000000,  0.25, 0.08, true,  1,  1.10,  0.00, 0.06,  3, 0.90,  13,  true,  0x000000, true  },
    { "coolfilm",  "Cool Film",  90,  1.1,  1.00,   0.0,  0x000000,  0.10, 0.02, false, 0,  1.03, -0.20, 0.08,  1, 0.18,  13,  true,  0x70A8D0, true  },
};
static constexpr int PRESET_COUNT = sizeof(s_presets) / sizeof(s_presets[0]);

int blurPresetCount()
{
    return PRESET_COUNT;
}

QString blurPresetId(int index)
{
    if (index < 0 || index >= PRESET_COUNT) return QStringLiteral("default");
    return QString::fromUtf8(s_presets[index].id);
}

QString blurPresetName(int index)
{
    if (index < 0 || index >= PRESET_COUNT) return {};
    return QString::fromUtf8(s_presets[index].displayName);
}

const BlurConfig &blurPresetForId(const QString &id)
{
    QByteArray ba = id.toUtf8();
    const char *idStr = ba.constData();
    for (int i = 0; i < PRESET_COUNT; ++i) {
        if (qstrcmp(s_presets[i].id, idStr) == 0)
            return s_presets[i];
    }
    // Fallback to default
    return s_presets[0];
}

const BlurConfig &blurPresetConfig(int index)
{
    if (index < 0 || index >= PRESET_COUNT)
        return s_presets[0];
    return s_presets[index];
}
