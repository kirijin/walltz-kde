// SPDX-FileCopyrightText: 2026 kirijin <avel.ronin@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef BLUR_PRESETS_H
#define BLUR_PRESETS_H

#include <QString>
#include <QColor>

/// Parameter set for a named blur preset.
/// Each entry drives the existing pipeline (sigma, sat, dither) + overlay + brightness,
/// plus the optional built-in photo frame (width % of min dim; Reddit uses 1%).
struct BlurConfig {
    const char *id;          // "default", "apple", "gnome", "mica", "acrylic", "reddit"
    const char *displayName; // "Default", "Apple", "GNOME", "Mica", "Acrylic", "Reddit"

    double sigma        = 0.0;   // set from WalltzDefaults::blurRadius in the table
    double satBoost     = 1.0;
    double brightness   = 1.0;
    double overlayOpacity = 0.0;   // 0-1, 0 = no tint overlay
    QRgb  overlayColor  = 0;       // sRGB, unused when overlayOpacity == 0
    double vignette     = 0.0;
    double grain        = 0.0;

    // Frame carried by the preset — uses the built-in matte frame (no new style).
    // Reddit forces it on at 1% width; every other preset leaves it OFF.
    bool frameEnabled   = false;
    int  frameWidthPct  = 0;       // frame width % of min dim (Reddit: 1)

    // Float color grade (Phase 1) — neutral unless a preset sets them.
    double gamma      = 1.0;       // per-channel curve
    double warmth     = 0.0;       // -1..1, R/B balance (positive = warm)
    double blackLift  = 0.0;       // shadow floor 0..1 (faded-film look)

    // Holistic look fields (2026-08-11): photo-grade + texture composition.
    // photoGrade rows are full photo looks: the grade applies to the PHOTO
    // itself (and its blurred background), the texture (if any) resolves
    // against the user's overlays dir at apply time. Non-look rows never
    // touch texture state.
    const char *textureAsset = nullptr;  // file name in overlays dir
    double textureOpacity = 0.0;
    int    textureBlendMode = 13;        // QPainter CompositionMode
    bool   textureOverPhoto = false;     // draw the texture ON TOP of the photo
    bool   photoGrade = false;           // look: grade the photo itself

    /// The "Default" preset is the immutable factory baseline — must not be overwritten.
    bool isLocked() const { return qstrcmp(id, "default") == 0; }
};

enum class BlurPresetId {
    Default,
    Auto,
    Serenity,
    Focus,
    Comfort,
    Apple,
    Gnome,
    Mica,
    Acrylic,
    Reddit,
    Kodachrome,
    Polaroid,
    Vintage,
    TriX,
    CoolFilm,
    Count   // sentinel — not a real preset
};

/// Canonical definitions
int          blurPresetCount();
QString      blurPresetId(int index);
QString      blurPresetName(int index);

/// Look up a BlurConfig by id string.  Returns the Default preset for unknown ids.
const BlurConfig &blurPresetForId(const QString &id);

/// Look up a BlurConfig by preset index (0-based).  Bounds-checked, returns Default on overflow.
const BlurConfig &blurPresetConfig(int index);

#endif // BLUR_PRESETS_H
