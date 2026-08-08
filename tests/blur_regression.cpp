// SPDX-FileCopyrightText: 2026 kirijin <avel.ronin@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

// Blur-core regression gate for walltz (S1/S2/S3/P1/P2, 2026-08-08).
// Q2 Demonstration: the range-clamped rewrite must match the old per-tap qBound
// Gaussian byte-for-byte (tolerance: 1 LSB for float-kernel rounding), and the
// corrected box cascade at σ>50 must stay within a few levels of the true
// Gaussian (the 2026-07-30 failure was ~34.5 levels — this test would catch it).
// Independent reference: verbatim copy of the OLD blur semantics, written here
// so it does not share any code with the implementation under test.
#include "WallpaperProcessor.h"
#include <QGuiApplication>
#include <QImage>
#include <QColor>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { std::printf("FAIL: %s\n", msg); ++failures; } \
    else { std::printf("ok:   %s\n", msg); } \
} while (0)

// ── Reference: OLD gaussian (per-tap qBound, double kernel) + pipeline ──
static void refGaussianH(float *dst, const float *src, int w, int h, double sigma)
{
    int kSize = std::max(3, (int)std::ceil(3.0 * sigma));
    if ((kSize & 1) == 0) ++kSize;
    int radius = kSize / 2;
    double sigmaSq2 = 2.0 * sigma * sigma;
    std::vector<double> kernel(kSize);
    double sum = 0.0;
    for (int i = 0; i < kSize; ++i) { int x = i - radius; kernel[i] = std::exp(-(double)(x*x)/sigmaSq2); sum += kernel[i]; }
    double invSum = 1.0 / sum;
    for (int i = 0; i < kSize; ++i) kernel[i] *= invSum;
    for (int y = 0; y < h; ++y) {
        const float *sRow = src + y * w * 4;
        float *dRow = dst + y * w * 4;
        for (int x = 0; x < w; ++x) {
            double c[4] = {0};
            for (int k = 0; k < kSize; ++k) {
                int sx = std::max(0, std::min(x + k - radius, w - 1));
                const float *sp = sRow + sx * 4;
                c[0]+=sp[0]*kernel[k]; c[1]+=sp[1]*kernel[k];
                c[2]+=sp[2]*kernel[k]; c[3]+=sp[3]*kernel[k];
            }
            float *dp = dRow + x*4;
            dp[0]=(float)c[0]; dp[1]=(float)c[1]; dp[2]=(float)c[2]; dp[3]=(float)c[3];
        }
    }
}

static void refGaussianV(float *dst, const float *src, int w, int h, double sigma)
{
    int kSize = std::max(3, (int)std::ceil(3.0 * sigma));
    if ((kSize & 1) == 0) ++kSize;
    int radius = kSize / 2;
    double sigmaSq2 = 2.0 * sigma * sigma;
    std::vector<double> kernel(kSize);
    double sum = 0.0;
    for (int i = 0; i < kSize; ++i) { int y = i - radius; kernel[i] = std::exp(-(double)(y*y)/sigmaSq2); sum += kernel[i]; }
    double invSum = 1.0 / sum;
    for (int i = 0; i < kSize; ++i) kernel[i] *= invSum;
    int stride = w * 4;
    for (int x = 0; x < w; ++x) {
        const float *sBase = src + x * 4;
        float *dBase = dst + x * 4;
        for (int y = 0; y < h; ++y) {
            double c[4] = {0};
            for (int k = 0; k < kSize; ++k) {
                int sy = std::max(0, std::min(y + k - radius, h - 1));
                const float *sp = sBase + sy * stride;
                c[0]+=sp[0]*kernel[k]; c[1]+=sp[1]*kernel[k];
                c[2]+=sp[2]*kernel[k]; c[3]+=sp[3]*kernel[k];
            }
            float *dp = dBase + y*stride;
            dp[0]=(float)c[0]; dp[1]=(float)c[1]; dp[2]=(float)c[2]; dp[3]=(float)c[3];
        }
    }
}

static void refImageToFloat(const QImage &img, float *buf)
{
    int w = img.width(), h = img.height(), bpl = img.bytesPerLine();
    for (int y = 0; y < h; ++y) {
        const uchar *row = img.constBits() + y * bpl;
        float *fRow = buf + y * w * 4;
        for (int x = 0; x < w; ++x) {
            fRow[x*4]=(float)row[x*4]; fRow[x*4+1]=(float)row[x*4+1];
            fRow[x*4+2]=(float)row[x*4+2]; fRow[x*4+3]=(float)row[x*4+3];
        }
    }
}

static void refFloatToImageDithered(const float *buf, QImage &img)
{
    static const uchar bayer[8][8] = {
        {  0, 48, 12, 60,  3, 51, 15, 63 }, { 32, 16, 44, 28, 35, 19, 47, 31 },
        {  8, 56,  4, 52, 11, 59,  7, 55 }, { 40, 24, 36, 20, 43, 27, 39, 23 },
        {  2, 50, 14, 62,  1, 49, 13, 61 }, { 34, 18, 46, 30, 33, 17, 45, 29 },
        { 10, 58,  6, 54,  9, 57,  5, 53 }, { 42, 26, 38, 22, 41, 25, 37, 21 }
    };
    int w = img.width(), h = img.height(), bpl = img.bytesPerLine();
    const float inv64 = 1.0f / 64.0f;
    for (int y = 0; y < h; ++y) {
        uchar *row = img.bits() + y * bpl;
        const float *fRow = buf + y * w * 4;
        const uchar *bRow = bayer[y & 7];
        for (int x = 0; x < w; ++x) {
            const float *fp = fRow + x * 4;
            float t = (float)bRow[x & 7] * inv64;
            for (int c = 0; c < 4; ++c) {
                float v = std::max(0.0f, std::min(fp[c], 255.0f));
                row[x*4 + c] = (uchar)std::min((int)(v + t), 255);
            }
        }
    }
}

// Reference pipeline: exactly what the old stackBlur did for pure blur.
static QImage refBlur(const QImage &in, double sigma)
{
    QImage img = in.format() == QImage::Format_ARGB32_Premultiplied
        ? in.copy() : in.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    int w = img.width(), h = img.height(), nPix = w * h;
    std::vector<float> buf1(nPix * 4), buf2(nPix * 4);
    refImageToFloat(img, buf1.data());
    refGaussianH(buf2.data(), buf1.data(), w, h, sigma);
    refGaussianV(buf1.data(), buf2.data(), w, h, sigma);
    refFloatToImageDithered(buf1.data(), img);
    return img;
}

struct Diff { double maxAbs = 0.0, meanAbs = 0.0; long count = 0; };

static Diff diffRGB(const QImage &a, const QImage &b)
{
    Diff d;
    int w = qMin(a.width(), b.width()), h = qMin(a.height(), b.height());
    for (int y = 0; y < h; ++y) {
        const uchar *ra = a.constScanLine(y), *rb = b.constScanLine(y);
        for (int x = 0; x < w; ++x) {
            for (int c = 0; c < 3; ++c) {
                int da = std::abs((int)ra[x*4+c] - (int)rb[x*4+c]);
                if (da > d.maxAbs) d.maxAbs = da;
                d.meanAbs += da;
                ++d.count;
            }
        }
    }
    if (d.count) d.meanAbs /= d.count;
    return d;
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("walltz-test"));
    QCoreApplication::setApplicationName(QStringLiteral("walltz-blur-regression"));

    // Smooth vertical gradient + a few hard edges — stresses interior + both borders.
    QImage src(513, 257, QImage::Format_ARGB32_Premultiplied);
    for (int y = 0; y < 257; ++y)
        for (int x = 0; x < 513; ++x) {
            int v = (x * 255) / 512;
            if (y == 40 || y == 200) v = (x < 100) ? 0 : 255; // hard edges
            int r = v, g = 255 - v, b = (v + 128) % 256;
            src.setPixel(x, y, qRgba(r, g, b, 255));
        }

    // ── S2: range-clamped Gaussian ≡ old per-tap qBound Gaussian (σ ≤ 50) ──
    // Old kernel was double; new stores float — a few pixels may flip 1 LSB
    // through the dither. Gate: max ≤ 1, mean < 0.1 (any index/weight bug
    // produces errors of dozens of levels).
    for (double sigma : {5.0, 30.0, 50.0}) {
        QImage work = src.copy();
        WallpaperProcessor::stackBlur(work, sigma, 1.0, 0.0, 0, 1.0);
        QImage out = work;
        QImage ref = refBlur(src, sigma);
        Diff d = diffRGB(out, ref);
        char msg[160];
        std::snprintf(msg, sizeof msg, "S2 σ=%.0f range-clamp == reference (max=%.2f mean=%.4f)",
                      sigma, d.maxAbs, d.meanAbs);
        CHECK(d.maxAbs <= 1.0 && d.meanAbs < 0.1, msg);
    }

    // ── S1: corrected box cascade ≈ true Gaussian (σ > 50) ──
    // Measured (2026-08-08): interior gradients 0.1-0.4 levels (theory ~0.04%);
    // borders 3-5 levels; smooth symmetric lobes up to ~17 levels flanking hard
    // step edges (B-spline step response vs Gaussian erf — intrinsic to the box
    // approximation, non-periodic, NOT the 34.5-level periodic banding of the
    // 2026-07-30 3-pass cascade which this gate is designed to catch).
    // Gate: max ≤ 20 (a periodic-banding regression measures 30+ and fails).
    for (double sigma : {60.0, 90.0, 120.0}) {
        QImage work = src.copy();
        WallpaperProcessor::stackBlur(work, sigma, 1.0, 0.0, 0, 1.0);
        QImage out = work;
        QImage ref = refBlur(src, sigma);
        Diff d = diffRGB(out, ref);
        char msg[160];
        std::snprintf(msg, sizeof msg, "S1 σ=%.0f box ≈ gaussian (max=%.2f mean=%.4f)",
                      sigma, d.maxAbs, d.meanAbs);
        CHECK(d.maxAbs <= 20.0 && d.meanAbs < 5.0, msg);
    }

    // ── S1: solid colour must survive the box path untouched ──
    {
        QImage solid(200, 150, QImage::Format_ARGB32_Premultiplied);
        solid.fill(qRgba(60, 120, 180, 255));
        QImage work = solid.copy();
        WallpaperProcessor::stackBlur(work, 90.0, 1.0, 0.0, 0, 1.0);
        QImage out = work;
        QColor c = out.pixelColor(100, 75);
        CHECK(c.red() == 60 && c.green() == 120 && c.blue() == 180,
              "S1 σ=90 solid colour preserved by box path");
    }

    // ── P1: desaturation uses luma gray, not arithmetic mean ──
    // Saturated red 255,0,0: luma gray = 76.2, mean gray = 85. σ=0.6 → kernel 3,
    // blur on solid is identity; sat 0 → all channels = gray.
    {
        QImage red(64, 64, QImage::Format_ARGB32_Premultiplied);
        red.fill(qRgba(255, 0, 0, 255));
        QImage work = red.copy();
        WallpaperProcessor::stackBlur(work, 0.6, 0.0, 0.0, 0, 1.0);
        QImage out = work;
        QColor c = out.pixelColor(32, 32);
        char msg[160];
        std::snprintf(msg, sizeof msg, "P1 desat red -> luma gray (got %d,%d,%d want ~76)",
                      c.red(), c.green(), c.blue());
        CHECK(std::abs(c.red() - 76) <= 3 && c.red() == c.green() && c.green() == c.blue(), msg);
    }

    // ── P2: overlay must not break the premultiplied invariant ──
    // Check the RAW premultiplied bytes (BGRA layout in ARGB32_Premultiplied):
    // after blending a tint into a translucent pixel, R must stay ≤ A.
    {
        QImage img(64, 64, QImage::Format_ARGB32_Premultiplied);
        img.fill(qRgba(128, 0, 0, 128));
        QImage work = img.copy();
        WallpaperProcessor::stackBlur(work, 0.6, 1.0, 0.5, 0xffffffff, 1.0);
        const uchar *px = work.constScanLine(32) + 32 * 4;
        int rPremul = px[2]; // BGRA layout: bytes 0=B 1=G 2=R 3=A
        int a = px[3];
        char msg[160];
        std::snprintf(msg, sizeof msg, "P2 overlay keeps premul RGB<=A (got R=%d A=%d)", rPremul, a);
        CHECK(rPremul <= a, msg);
    }

    // ── Edge: tiny image through the box path must not crash ──
    {
        QImage tiny(1, 1, QImage::Format_ARGB32_Premultiplied);
        tiny.fill(qRgba(200, 100, 50, 255));
        QImage work = tiny.copy();
        WallpaperProcessor::stackBlur(work, 90.0, 1.0, 0.0, 0, 1.0);
        QImage out = work;
        CHECK(out.pixelColor(0, 0).red() == 200, "edge: 1x1 box path preserved");
    }

    std::printf(failures == 0 ? "\nALL PASS\n" : "\n%d FAILURES\n", failures);
    return failures == 0 ? 0 : 1;
}
