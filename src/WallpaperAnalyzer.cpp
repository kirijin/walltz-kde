// SPDX-FileCopyrightText: 2026 kirijin <avel.ronin@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "WallpaperAnalyzer.h"

#include <QtMath>
#include <QImage>
#include <QColor>
#include <QVector>
#include <algorithm>

// ── Helpers ──────────────────────────────────────────────────────────

/// Perceived luminance (ITU-R BT.601, fast path)
static inline double lum(double r, double g, double b) {
    return 0.299 * r + 0.587 * g + 0.114 * b;
}

/// Hue angle in degrees from RGB, with saturation weight
static double hueDeg(double r, double g, double b, double sat, double &outWeight)
{
    if (sat < 1e-6) {
        outWeight = 0;
        return 0;
    }
    double max = qMax(r, qMax(g, b));
    double min = qMin(r, qMin(g, b));
    double delta = max - min;
    double h = 0;
    if (qFuzzyCompare(delta, 0.0)) {
        outWeight = 0;
        return 0;
    }
    if (qFuzzyCompare(max, r))
        h = 60.0 * fmod((g - b) / delta, 6.0);
    else if (qFuzzyCompare(max, g))
        h = 60.0 * ((b - r) / delta + 2.0);
    else
        h = 60.0 * ((r - g) / delta + 4.0);
    if (h < 0) h += 360.0;
    outWeight = sat;       // weight by saturation
    return h;
}

// ── 5×5 Sobel kernels for edge-energy estimation ────────────────────

static const int s_sobelX[25] = {
    2, 1, 0, -1, -2,
    3, 2, 0, -2, -3,
    4, 3, 0, -3, -4,
    3, 2, 0, -2, -3,
    2, 1, 0, -1, -2,
};
static const int s_sobelY[25] = {
     2,  3,  4,  3,  2,
     1,  2,  3,  2,  1,
     0,  0,  0,  0,  0,
    -1, -2, -3, -2, -1,
    -2, -3, -4, -3, -2,
};

// ── analyseImage ─────────────────────────────────────────────────────

ImageStats analyseImage(const QImage &image)
{
    ImageStats stats;
    if (image.isNull()) return stats;

    // Downsample to at most 64×64
    int sw = image.width();
    int sh = image.height();
    const int MAX_DIM = 64;
    if (sw > MAX_DIM || sh > MAX_DIM) {
        double scale = qMin(MAX_DIM / (double)sw, MAX_DIM / (double)sh);
        QImage thumb = image.scaled(
            qMax(1, (int)(sw * scale)),
            qMax(1, (int)(sh * scale)),
            Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        return analyseImage(thumb);   // recurse once on the thumbnail
    }

    int w = sw, h = sh;
    int n = w * h;
    if (n == 0) return stats;

    // Accumulators
    double sumL = 0, sumS = 0;
    double sumLsq = 0;
    int    hueBins[12] = {};       // 30° bins
    double hueWts[12] = {};

    // Edge energy: use grayscale image for Sobel
    QImage gray = image.convertedTo(QImage::Format_Grayscale8);
    double edgeSum = 0;

    const int stride = qMax(1, n / 4096);   // sample stride for large >64 but still handleable
    int sampled = 0;

    for (int y = 0; y < h; ++y) {
        const QRgb *row = reinterpret_cast<const QRgb*>(image.constScanLine(y));
        for (int x = 0; x < w; ++x) {
            if (sampled % stride != 0) { ++sampled; continue; }
            QRgb px = row[x];
            double r = qRed(px)   / 255.0;
            double g = qGreen(px) / 255.0;
            double b = qBlue(px)  / 255.0;

            double L = lum(r, g, b);
            sumL  += L;
            sumLsq += L * L;

            double maxC = qMax(r, qMax(g, b));
            double minC = qMin(r, qMin(g, b));
            double S = (maxC > 1e-6) ? (maxC - minC) / maxC : 0;
            sumS += S;

            double wS;
            double hVal = hueDeg(r, g, b, S, wS);
            if (wS > 0) {
                int bin = qMin(11, (int)(hVal / 30.0));
                hueBins[bin]++;
                hueWts[bin] += wS;
            }
            ++sampled;
        }
    }

    int count = sampled;
    if (count == 0) return stats;

    stats.meanLuminance    = sumL / count;
    stats.meanSaturation   = sumS / count;
    stats.contrast         = qSqrt((sumLsq - sumL * sumL / count) / count);

    // Dominant hue: find bin with highest total weighted hue
    int bestBin = -1;
    double bestWt = 0;
    for (int i = 0; i < 12; ++i) {
        if (hueWts[i] > bestWt) {
            bestWt = hueWts[i];
            bestBin = i;
        }
    }
    if (bestBin >= 0 && bestWt > count * 0.05) {   // at least 5% of pixels have hue signal
        stats.dominantHue = bestBin * 30.0 + 15.0;   // centre of bin
        stats.dominantHueWarm = (bestBin < 2);       // bin 0-1 = 0°-60° = warm
    } else {
        stats.dominantHue = -1;                      // neutral / achromatic
        stats.dominantHueWarm = false;
    }

    // Edge energy from Sobel on grayscale (only need to check valid pixels)
    int edgeCount = 0;
    for (int y = 2; y < h - 2; ++y) {
        for (int x = 2; x < w - 2; ++x) {
            double gx = 0, gy = 0;
            for (int ky = -2; ky <= 2; ++ky) {
                const uchar *krow = gray.constScanLine(y + ky);
                for (int kx = -2; kx <= 2; ++kx) {
                    int ki = (ky + 2) * 5 + (kx + 2);
                    int val = krow[x + kx];
                    gx += val * s_sobelX[ki];
                    gy += val * s_sobelY[ki];
                }
            }
            edgeSum += qSqrt(gx * gx + gy * gy) / (255.0 * 4.0);  // normalise
            ++edgeCount;
        }
    }
    stats.edgeEnergy = (edgeCount > 0)
        ? qMin(1.0, edgeSum / edgeCount)
        : 0.3;

    return stats;
}

// ── computeSmartAuto ─────────────────────────────────────────────────

SmartAutoParams computeSmartAuto(const ImageStats &stats)
{
    SmartAutoParams p;

    // ── Blur sigma ─────────────────────────────────────────────────
    // Busy images (high edge energy) get more blur.
    // Range: σ ∈ [10, 35] — mild-to-strong, never muddy.
    p.sigma = 10 + stats.edgeEnergy * 25;

    // ── Saturation ─────────────────────────────────────────────────
    // Low saturation = low arousal.  Reduce vibrant images more.
    // Calm target output saturation ~0.25-0.35.
    double srcSat = stats.meanSaturation;
    if (srcSat < 0.20) {
        p.satBoost = 1.0;                      // already near-achromatic — leave it
    } else if (srcSat > 0.60) {
        p.satBoost = 0.35;                     // heavy desaturation for vibrant images
    } else {
        // Smooth ramp: srcSat=0.20 → 1.0,  srcSat=0.60 → 0.35
        p.satBoost = 1.0 - (srcSat - 0.20) * (0.65 / 0.40);
        p.satBoost = qBound(0.35, p.satBoost, 1.0);
    }

    // ── Brightness ─────────────────────────────────────────────────
    // Target perceived luminance ≈ 0.40 (research-calibrated calm level).
    // Clamp to [0.55, 1.15] to avoid extreme values.
    double srcL = stats.meanLuminance;
    if (srcL > 0.01) {
        p.brightness = qBound(0.55, 0.40 / srcL, 1.15);
    } else {
        p.brightness = 0.85;
    }

    // ── Overlay ────────────────────────────────────────────────────
    // Brighter images get more overlay.  Tint is hue-adaptive:
    //   warm sources → cool-tinted overlay (#1a1a24)
    //   cool sources → warm-tinted overlay (#24201a)
    //   neutral      → pure dark (#1a1a1a)
    p.overlayOpacity = qBound(0.18, 0.18 + srcL * 0.30, 0.45);

    if (stats.dominantHueWarm) {
        p.overlayColor = 0xff1a1a24;           // cool blue-grey
    } else if (stats.dominantHue < 0) {
        p.overlayColor = 0xff1a1a1a;           // neutral dark
    } else {
        p.overlayColor = 0xff24201a;           // slight warm tint
    }

    // ── Vignette / grain ──────────────────────────────────────────
    // Always off for Smart Auto — minimalism.
    p.vignette = 0;
    p.grain    = 0;

    return p;
}
