// SPDX-FileCopyrightText: 2026 kirijin <avel.ronin@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef WALLPAPERANALYZER_H
#define WALLPAPERANALYZER_H

#include <QImage>
#include <QColor>

/// Aggregated statistics extracted from a wallpaper source image.
struct ImageStats {
    /// Perceived luminance ∈ [0, 1] (ITU-R BT.601 weights)
    double meanLuminance = 0.5;

    /// Mean saturation ∈ [0, 1]  (HSV saturation, max-min)
    double meanSaturation = 0.5;

    /// Dominant hue angle ∈ [0, 360), or -1 if no dominant hue (achromatic)
    double dominantHue = -1.0;

    /// Whether the dominant hue is in the warm range (0–60°)
    bool dominantHueWarm = false;

    /// Edge energy ∈ [0, 1] — relative measure of high-frequency content
    double edgeEnergy = 0.3;

    /// Contrast — standard deviation of luminance ∈ [0, 1]
    double contrast = 0.2;
};

/// Analyse a wallpaper image and extract the 5 metrics above.
///
/// The analysis works on a heavily downsampled copy (max 64×64) so the cost
/// is negligible even for megapixel source images.  All channels are
/// normalised to [0, 1] for writing parameter-mapping code.
ImageStats analyseImage(const QImage &image);

/// Compute Smart-Auto parameters from image statistics.
///
/// Research base:
///   - Wilms & Oberfeld (2018): low saturation → low arousal
///   - Zhang et al. (2026): low-saturation backgrounds improve working memory
///   - JOV visual discomfort: blur attenuates high-spatial-frequency stress
///   - Colour psychology: dark overlays reduce perceived brightness → calm
struct SmartAutoParams {
    double sigma          = 15;
    double satBoost       = 0.85;
    double brightness     = 0.92;
    double overlayOpacity = 0.35;
    QRgb  overlayColor    = 0xff1a1a1a;
    double vignette       = 0;
    double grain          = 0;
};

SmartAutoParams computeSmartAuto(const ImageStats &stats);

#endif // WALLPAPERANALYZER_H
