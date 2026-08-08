// SPDX-FileCopyrightText: 2026 kirijin <avel.ronin@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "blur_presets.h"
#include "WallpaperProcessor.h"   // for WalltzDefaults

// ── Canonical preset table ──────────────────────────────────────────────
// Order must match BlurPresetId enum exactly. The "default" row derives its
// sigma/sat/brightness from WalltzDefaults so there is one source of truth (Q5).
static const BlurConfig s_presets[] = {
    // id       display       σ     sat   bright  ovlOp  ovlColor   vig   grain
    { "default", "Default", WalltzDefaults::blurRadius, WalltzDefaults::saturationFactor, WalltzDefaults::blurBrightness, WalltzDefaults::overlayOpacity, 0x000000, WalltzDefaults::vignetteStrength, WalltzDefaults::grainStrength },
    { "auto",    "Auto",      0.0,   1.0,  1.00,   0.0,   0x000000,  0.0,  0.0 },
    { "serenity","Serenity", 25,    0.30,  0.65,   0.55,  0x181824,  0.15,  0.0 },
    { "focus",   "Focus",     8,    0.55,  0.90,   0.15,  0x1c1c1c,  0.0,  0.0 },
    { "comfort", "Comfort",  20,    0.80,  0.78,   0.35,  0x2a1f14,  0.25,  0.02 },
    { "apple",   "Apple",   30,    1.8,  1.02,   0.50,  0x1a1a1a,  0.0,  0.0 },
    { "gnome",   "GNOME",   30,    1.0,  0.60,   0.0,   0x000000,  0.0,  0.0 },
    { "mica",    "Mica",    10,    0.9,  0.85,   0.30,  0x323232,  0.0,  0.0 },
    { "acrylic", "Acrylic", 30,    1.0,  0.90,   0.60,  0x202020,  0.0,  0.03 },
    { "reddit",  "Reddit",   5,    1.0,  0.30,   0.70,  0x000000,  0.0,  0.0 },
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
