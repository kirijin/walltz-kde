// SPDX-FileCopyrightText: 2026 kirijin <avel.ronin@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

// Holistic "retro look" presets (4th tab, 2026-08-11).
// Each look bundles a complete PHOTO treatment: color grade (gamma/warmth/
// blackLift), saturation, frame, grain, vignette, and an optional texture
// asset (file name inside the user's overlays dir — resolved at apply time,
// gracefully skipped when absent). Curve shapes are re-derived from XnRetro's
// extracted LUTs and classic film conventions, calm-tamed; values are
// starters for the render-and-tune loop. NOT transcribed bytes.

#pragma once
#include <QtGlobal>

struct LookConfig {
    const char *id;
    const char *displayName;
    double satBoost;         // whole-composition saturation (0 = B&W)
    double gamma;            // per-channel curve
    double warmth;           // -1..1, R/B balance (positive = warm)
    double blackLift;        // shadow floor 0..1 (faded-film look)
    double vignette;
    double grain;
    bool   frameEnabled;
    int    frameWidthPct;    // % of min dim (matte frame)
    const char *textureAsset; // file name in overlays dir; nullptr = none
    double textureOpacity;
    int    textureBlendMode;  // QPainter CompositionMode; frames = SourceOver (0)
    bool   textureOverPhoto;  // draw the texture ON TOP of the photo (film borders)
};

int retroLookCount();
const LookConfig &retroLookConfig(int index);
const char *retroLookId(int index);
const char *retroLookName(int index);
