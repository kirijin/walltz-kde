// SPDX-FileCopyrightText: 2026 kirijin <avel.ronin@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "WallpaperProcessor.h"

#include <QPainter>
#include <QScreen>
#include <QElapsedTimer>
#include <QDebug>
#include <cstdio>
#include <QGuiApplication>
#include <QWindow>
#include <QFileInfo>
#include <QtMath>
#include <QRandomGenerator>
#include <cmath>
#include <cstring>
#include <QPainterPath>
#include <QDir>
#include <QFile>
#include <QDateTime>
#include <QImageWriter>
#include <QTimer>
#include <QUrl>
#include <QCryptographicHash>
#include <QSvgRenderer>
#include <QSettings>
#include <QStandardPaths>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCall>
#include <QDBusPendingReply>
#include <QDBusPendingCallWatcher>
#include <QDBusUnixFileDescriptor>
#include <QMutex>
#include <vector>
#include <functional>
#include <algorithm>
#include <KLocalizedString>

// ── File-based profiler (gated, Q9) ────────────────────────────────────────
// Enable by exporting WALLTZ_PROFILE=1 before launch. Default off in production.
static bool walltzProfileEnabled()
{
    static const bool enabled = qEnvironmentVariableIsSet("WALLTZ_PROFILE");
    return enabled;
}
#define PROF_LOG(RENDER_ID, MSG, ...) do { \
    if (walltzProfileEnabled()) { \
        FILE *pf = fopen("/tmp/walltz-profiler.log", "a"); \
        if (pf) { fprintf(pf, "[PROF:%d] " MSG "\n", (RENDER_ID), ##__VA_ARGS__); fclose(pf); } \
    } \
} while(0)

// ── Pattern type decode (Q3 — single decode site) ──────────────────────────
PatternRef WallpaperProcessor::decodePatternType(int flat)
{
    PatternRef ref;
    ref.flat = flat;
    if (flat >= MOTIF_OFFSET) {
        ref.kind = PatternKind::Motif;
        ref.index = qBound(0, flat - MOTIF_OFFSET, MOTIF_PATTERN_COUNT - 1);
    } else if (flat >= SVG_GEO_OFFSET) {
        ref.kind = PatternKind::SvgGeo;
        ref.index = qBound(0, flat - SVG_GEO_OFFSET, SVG_GEO_COUNT - 1);
    } else {
        ref.kind = PatternKind::Geometric;
        ref.index = qBound(0, flat, GEOMETRIC_PATTERN_COUNT - 1);
    }
    return ref;
}

// ── gradient presets (color-theory-based) ─────────────────────────────────
const WallpaperProcessor::GradientPreset WallpaperProcessor::s_presets[12] = {
    { QT_TRANSLATE_NOOP("WP", "Sunset Warmth"),  0xffff6b6b, 0xfffeca57 },
    { QT_TRANSLATE_NOOP("WP", "Coral Reef"),     0xffff6b6b, 0xff48dbfb },
    { QT_TRANSLATE_NOOP("WP", "Lemonade"),       0xfffdcb6e, 0xff00cec9 },
    { QT_TRANSLATE_NOOP("WP", "Ocean Depths"),   0xff0abde3, 0xff48dbfb },
    { QT_TRANSLATE_NOOP("WP", "Tokyo Night"),    0xff1a1b26, 0xff7aa2f7 },
    { QT_TRANSLATE_NOOP("WP", "Arctic"),         0xff2e3440, 0xff88c0d0 },
    { QT_TRANSLATE_NOOP("WP", "Catppuccin"),     0xff1e1e2e, 0xffcba6f7 },
    { QT_TRANSLATE_NOOP("WP", "Gruvbox"),        0xff282828, 0xff8f3f1a },
    { QT_TRANSLATE_NOOP("WP", "Solarized"),      0xff073642, 0xff268bd2 },
    { QT_TRANSLATE_NOOP("WP", "Dusk"),           0xff6c5ce7, 0xfffd79a8 },
    { QT_TRANSLATE_NOOP("WP", "Everforest"),     0xff2b3339, 0xffa7c080 },
    { QT_TRANSLATE_NOOP("WP", "Grayscale"),      0xff444444, 0xffcccccc },
};

const char *WallpaperProcessor::s_geometricNames[GEOMETRIC_PATTERN_COUNT] = {
    QT_TRANSLATE_NOOP("WP", "Dots"),
    QT_TRANSLATE_NOOP("WP", "Stripes H"),
    QT_TRANSLATE_NOOP("WP", "Stripes V"),
    QT_TRANSLATE_NOOP("WP", "Stripes D"),
    QT_TRANSLATE_NOOP("WP", "Checkerboard"),
    QT_TRANSLATE_NOOP("WP", "Chevron"),
    QT_TRANSLATE_NOOP("WP", "Diamonds"),
    QT_TRANSLATE_NOOP("WP", "Crosshatch"),
};

const char *WallpaperProcessor::s_svgGeoNames[SVG_GEO_COUNT] = {
    QT_TRANSLATE_NOOP("WP", "Squares"),
    QT_TRANSLATE_NOOP("WP", "Triangles"),
    QT_TRANSLATE_NOOP("WP", "Circles"),
    QT_TRANSLATE_NOOP("WP", "Rings"),
    QT_TRANSLATE_NOOP("WP", "Lines"),
    QT_TRANSLATE_NOOP("WP", "Worms"),
    QT_TRANSLATE_NOOP("WP", "Dots"),
    QT_TRANSLATE_NOOP("WP", "Diamonds"),
    QT_TRANSLATE_NOOP("WP", "Square"),
    QT_TRANSLATE_NOOP("WP", "Triangle"),
    QT_TRANSLATE_NOOP("WP", "Circle"),
    QT_TRANSLATE_NOOP("WP", "Diamond"),
    QT_TRANSLATE_NOOP("WP", "Ring"),
    QT_TRANSLATE_NOOP("WP", "Wave"),
    QT_TRANSLATE_NOOP("WP", "Cross"),
    QT_TRANSLATE_NOOP("WP", "Disc"),
};

const char *WallpaperProcessor::s_motifNames[MOTIF_PATTERN_COUNT] = {
    QT_TRANSLATE_NOOP("WP", "Cat"), QT_TRANSLATE_NOOP("WP", "Dog"),
    QT_TRANSLATE_NOOP("WP", "Rabbit"), QT_TRANSLATE_NOOP("WP", "Turtle"),
    QT_TRANSLATE_NOOP("WP", "Bird"), QT_TRANSLATE_NOOP("WP", "Fish"),
    QT_TRANSLATE_NOOP("WP", "Snail"), QT_TRANSLATE_NOOP("WP", "Paw Print"),
    QT_TRANSLATE_NOOP("WP", "Squirrel"), QT_TRANSLATE_NOOP("WP", "Mouse"),
    QT_TRANSLATE_NOOP("WP", "Rat"), QT_TRANSLATE_NOOP("WP", "Bug"),
    QT_TRANSLATE_NOOP("WP", "Bone"), QT_TRANSLATE_NOOP("WP", "Footprints"),
    QT_TRANSLATE_NOOP("WP", "Leaf"), QT_TRANSLATE_NOOP("WP", "Flower"),
    QT_TRANSLATE_NOOP("WP", "Tree"), QT_TRANSLATE_NOOP("WP", "Shell"),
    QT_TRANSLATE_NOOP("WP", "Feather"), QT_TRANSLATE_NOOP("WP", "Egg"),
    QT_TRANSLATE_NOOP("WP", "Fire"), QT_TRANSLATE_NOOP("WP", "Droplets"),
    QT_TRANSLATE_NOOP("WP", "Rain"), QT_TRANSLATE_NOOP("WP", "Wind"),
    QT_TRANSLATE_NOOP("WP", "Mountain"), QT_TRANSLATE_NOOP("WP", "Waves"),
    QT_TRANSLATE_NOOP("WP", "Moon"), QT_TRANSLATE_NOOP("WP", "Sun"),
    QT_TRANSLATE_NOOP("WP", "Star"), QT_TRANSLATE_NOOP("WP", "Cloud"),
    QT_TRANSLATE_NOOP("WP", "Rainbow"), QT_TRANSLATE_NOOP("WP", "Snowflake"),
    QT_TRANSLATE_NOOP("WP", "Lightning"),
    QT_TRANSLATE_NOOP("WP", "Music"), QT_TRANSLATE_NOOP("WP", "Music 2"),
    QT_TRANSLATE_NOOP("WP", "Music 3"), QT_TRANSLATE_NOOP("WP", "Music 4"),
    QT_TRANSLATE_NOOP("WP", "Piano"), QT_TRANSLATE_NOOP("WP", "Guitar"),
    QT_TRANSLATE_NOOP("WP", "Drum"), QT_TRANSLATE_NOOP("WP", "Mic"),
    QT_TRANSLATE_NOOP("WP", "Headphones"), QT_TRANSLATE_NOOP("WP", "Bell"),
    QT_TRANSLATE_NOOP("WP", "Disc"), QT_TRANSLATE_NOOP("WP", "Palette"),
    QT_TRANSLATE_NOOP("WP", "Paintbrush"), QT_TRANSLATE_NOOP("WP", "Theater"),
    QT_TRANSLATE_NOOP("WP", "Camera"), QT_TRANSLATE_NOOP("WP", "Film"),
    QT_TRANSLATE_NOOP("WP", "Heart"), QT_TRANSLATE_NOOP("WP", "Crown"),
    QT_TRANSLATE_NOOP("WP", "Balloon"), QT_TRANSLATE_NOOP("WP", "Gift"),
    QT_TRANSLATE_NOOP("WP", "Flame"), QT_TRANSLATE_NOOP("WP", "Drumstick"),
    QT_TRANSLATE_NOOP("WP", "Pizza"), QT_TRANSLATE_NOOP("WP", "Coffee"),
    QT_TRANSLATE_NOOP("WP", "Cake"), QT_TRANSLATE_NOOP("WP", "Cookie"),
    QT_TRANSLATE_NOOP("WP", "Wine"), QT_TRANSLATE_NOOP("WP", "Book"),
    QT_TRANSLATE_NOOP("WP", "Dumbbell"), QT_TRANSLATE_NOOP("WP", "Backpack"),
    QT_TRANSLATE_NOOP("WP", "Shirt"),
    QT_TRANSLATE_NOOP("WP", "Smile"), QT_TRANSLATE_NOOP("WP", "Frown"),
    QT_TRANSLATE_NOOP("WP", "Angry"), QT_TRANSLATE_NOOP("WP", "Laugh"),
    QT_TRANSLATE_NOOP("WP", "Meh"), QT_TRANSLATE_NOOP("WP", "Annoyed"),
    QT_TRANSLATE_NOOP("WP", "Shield"), QT_TRANSLATE_NOOP("WP", "Sword"),
    QT_TRANSLATE_NOOP("WP", "Diamond"), QT_TRANSLATE_NOOP("WP", "Gem"),
    QT_TRANSLATE_NOOP("WP", "Infinity"), QT_TRANSLATE_NOOP("WP", "Eye"),
    QT_TRANSLATE_NOOP("WP", "Cross"), QT_TRANSLATE_NOOP("WP", "Church"),
    QT_TRANSLATE_NOOP("WP", "Castle"), QT_TRANSLATE_NOOP("WP", "Scroll"),
    QT_TRANSLATE_NOOP("WP", "Medal"), QT_TRANSLATE_NOOP("WP", "Trophy"),
    QT_TRANSLATE_NOOP("WP", "Orthodox Cross"), QT_TRANSLATE_NOOP("WP", "Moon Star"),
    QT_TRANSLATE_NOOP("WP", "Menorah"), QT_TRANSLATE_NOOP("WP", "Dharma Wheel"),
    QT_TRANSLATE_NOOP("WP", "Sacred Book"), QT_TRANSLATE_NOOP("WP", "Hand"),
};

const int WallpaperProcessor::s_motifCategories[MOTIF_PATTERN_COUNT] = {
    // animals: 0-13
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    // nature: 14-32
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    // art: 33-48
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    // lifestyle: 49-63
    4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
    // emotional: 64-69
    5,5,5,5,5,5,
    // symbolic: 70-87
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
};

const char *WallpaperProcessor::s_categoryNames[6] = {
    QT_TRANSLATE_NOOP("WP", "Animals"), QT_TRANSLATE_NOOP("WP", "Nature"),
    QT_TRANSLATE_NOOP("WP", "Art"), QT_TRANSLATE_NOOP("WP", "Symbolic"),
    QT_TRANSLATE_NOOP("WP", "Lifestyle"), QT_TRANSLATE_NOOP("WP", "Emotional"),
};

const char *WallpaperProcessor::s_motifSvgFiles[MOTIF_PATTERN_COUNT] = {
    "cat", "dog", "rabbit", "turtle", "bird", "fish", "snail", "paw-print",
    "squirrel", "mouse", "rat", "bug", "bone", "footprints",
    "leaf", "flower", "tree-deciduous", "shell", "feather", "egg",
    "flame", "droplets", "cloud-rain", "wind", "mountain", "waves",
    "moon", "sun", "star", "cloud", "rainbow", "snowflake", "cloud-lightning",
    "music", "music-2", "music-3", "music-4", "piano", "guitar", "drum",
    "mic", "headphones", "bell", "disc", "palette", "paintbrush", "theater", "camera", "film",
    "heart", "crown", "balloon", "gift", "flame",
    "drumstick", "pizza", "coffee", "cake", "cookie", "wine", "book", "dumbbell", "backpack", "shirt",
    "smile", "frown", "angry", "laugh", "meh", "annoyed",
    "shield", "sword", "diamond", "gem", "infinity", "eye",
    "cross", "church", "castle", "scroll", "medal", "trophy",
    "orthodox-cross", "moon-star", "menorah", "dharma-wheel", "book-open", "hand",
};

// ── constructor ──────────────────────────────────────────────────────────
WallpaperProcessor::WallpaperProcessor(QObject *parent)
    : QObject(parent)
{
    m_queueWatcher.setPendingResultsLimit(-1);
    connect(&m_queueWatcher, &QFutureWatcher<QPair<int, QString>>::resultReadyAt,
            this, [this](int i) { handleQueueResult(i, m_queueWatcher.resultAt(i).second); });
    connect(&m_queueWatcher, &QFutureWatcher<QPair<int, QString>>::finished,
            this, &WallpaperProcessor::finishQueue);
}

// ── property setters ─────────────────────────────────────────────────────
// Every render-affecting setter emits the aggregate renderParamsChanged (Q2)
// so QML needs exactly one debounce handler instead of 28.

void WallpaperProcessor::setTargetWidth(int w)
{
    if (m_targetWidth != w && w > 0 && w < 15000) {
        m_targetWidth = w;
        if (m_aspectRatio > 0.0) {
            int newH = qRound(w / m_aspectRatio);
            if (newH != m_targetHeight) m_targetHeight = newH;
        }
        Q_EMIT renderParamsChanged();
    }
}

void WallpaperProcessor::setTargetHeight(int h)
{
    if (m_targetHeight != h && h > 0 && h < 15000) {
        m_targetHeight = h;
        if (m_aspectRatio > 0.0) {
            int newW = qRound(h * m_aspectRatio);
            if (newW != m_targetWidth) m_targetWidth = newW;
        }
        Q_EMIT renderParamsChanged();
    }
}

void WallpaperProcessor::setBlurMode(bool blur)
{
    if (m_blurMode != blur) { m_blurMode = blur; Q_EMIT renderParamsChanged(); }
}

void WallpaperProcessor::setBackgroundColor(const QColor &c)
{
    if (m_bgColor != c) { m_bgColor = c; Q_EMIT renderParamsChanged(); }
}

void WallpaperProcessor::setAutoColor(bool autoC)
{
    if (m_autoColor != autoC) { m_autoColor = autoC; Q_EMIT renderParamsChanged(); }
}

void WallpaperProcessor::setBlurRadius(int r)
{
    r = qBound(0, r, WalltzDefaults::blurRadiusMax);
    if (m_blurRadius != r) { m_blurRadius = r; Q_EMIT renderParamsChanged(); }
}

void WallpaperProcessor::setSaturationFactor(double f)
{
    f = qBound(0.0, f, 3.0);
    if (!qFuzzyCompare(m_saturationFactor, f)) { m_saturationFactor = f; Q_EMIT renderParamsChanged(); }
}

void WallpaperProcessor::setColorGamma(double g)
{
    g = qBound(0.5, g, 2.5);
    if (!qFuzzyCompare(m_colorGamma, g)) { m_colorGamma = g; Q_EMIT renderParamsChanged(); }
}

void WallpaperProcessor::setColorWarmth(double w)
{
    w = qBound(-1.0, w, 1.0);
    if (!qFuzzyCompare(m_colorWarmth, w)) { m_colorWarmth = w; Q_EMIT renderParamsChanged(); }
}

void WallpaperProcessor::setColorBlackLift(double l)
{
    l = qBound(0.0, l, 1.0);
    if (!qFuzzyCompare(m_colorBlackLift, l)) { m_colorBlackLift = l; Q_EMIT renderParamsChanged(); }
}

void WallpaperProcessor::setBgGradientStyle(int s)
{
    s = qBound(0, s, 2);
    if (m_bgGradientStyle != s) { m_bgGradientStyle = s; Q_EMIT renderParamsChanged(); }
}

void WallpaperProcessor::setAspectMode(int mode)
{
    mode = qBound(0, mode, 6);
    if (m_aspectMode == mode) return;
    m_aspectMode = mode;
    if (mode == 0) {
        m_aspectRatio = 0.0;
    } else {
        m_aspectRatio = s_aspectRatios[mode];
        if (m_targetWidth >= m_targetHeight) {
            int newH = qRound(m_targetWidth / m_aspectRatio);
            if (newH != m_targetHeight) m_targetHeight = newH;
        } else {
            int newW = qRound(m_targetHeight * m_aspectRatio);
            if (newW != m_targetWidth) m_targetWidth = newW;
        }
    }
    Q_EMIT renderParamsChanged();
}

void WallpaperProcessor::setBgGradientPreset(int p)
{
    p = qBound(0, p, 11);
    if (m_bgGradientPreset != p) { m_bgGradientPreset = p; Q_EMIT renderParamsChanged(); }
}

void WallpaperProcessor::setGradientAngle(double a)
{
    a = std::fmod(a, 360.0);
    if (a < 0) a += 360.0;
    if (!qFuzzyCompare(m_gradientAngle, a)) { m_gradientAngle = a; Q_EMIT renderParamsChanged(); }
}

void WallpaperProcessor::setBgZoom(double z)
{
    z = qBound(0.5, z, 3.0);
    if (!qFuzzyCompare(m_bgZoom, z)) { m_bgZoom = z; Q_EMIT renderParamsChanged(); }
}

void WallpaperProcessor::setBgBlurAngle(double a)
{
    a = std::fmod(a, 360.0);
    if (a < 0) a += 360.0;
    if (!qFuzzyCompare(m_bgBlurAngle, a)) { m_bgBlurAngle = a; Q_EMIT renderParamsChanged(); }
}

void WallpaperProcessor::setAutoMood(int m)
{
    m = qBound(0, m, 5);
    if (m_autoMood != m) { m_autoMood = m; Q_EMIT renderParamsChanged(); }
}

void WallpaperProcessor::setUseV2(bool v2)
{
    if (m_useV2 != v2) { m_useV2 = v2; Q_EMIT renderParamsChanged(); }
}

void WallpaperProcessor::setVignetteStrength(double s)
{
    s = qBound(0.0, s, 1.0);
    if (!qFuzzyCompare(m_vignetteStrength, s)) { m_vignetteStrength = s; Q_EMIT renderParamsChanged(); }
}

void WallpaperProcessor::setGrainStrength(double s)
{
    s = qBound(0.0, s, 1.0);
    if (!qFuzzyCompare(m_grainStrength, s)) { m_grainStrength = s; Q_EMIT renderParamsChanged(); }
}

void WallpaperProcessor::setCaStrength(double s)
{
    s = qBound(0.0, s, 1.0);
    if (!qFuzzyCompare(m_caStrength, s)) { m_caStrength = s; Q_EMIT renderParamsChanged(); }
}

void WallpaperProcessor::setPhotoFrame(bool on)
{
    if (m_photoFrame != on) { m_photoFrame = on; Q_EMIT renderParamsChanged(); }
}

void WallpaperProcessor::setPhotoFrameWidth(int w)
{
    w = qBound(0, w, 25);
    if (m_photoFrameWidth != w) { m_photoFrameWidth = w; Q_EMIT renderParamsChanged(); }
}

void WallpaperProcessor::setFgZoom(double z)
{
    // Superset of slider ranges (rect 0.5..1.0, pip 1.0..4.0); render clamps per-path.
    z = qBound(0.5, z, 4.0);
    if (!qFuzzyCompare(m_fgZoom, z)) { m_fgZoom = z; Q_EMIT renderParamsChanged(); }
}

void WallpaperProcessor::setPipZoom(double z)
{
    z = qBound(1.0, z, 4.0);
    if (!qFuzzyCompare(m_pipZoom, z)) { m_pipZoom = z; Q_EMIT renderParamsChanged(); }
}

void WallpaperProcessor::setBgPatternEnabled(bool on)
{
    if (m_bgPatternEnabled != on) { m_bgPatternEnabled = on; Q_EMIT renderParamsChanged(); }
}

void WallpaperProcessor::setBgPatternType(int t)
{
    t = qBound(0, t, MOTIF_OFFSET + MOTIF_PATTERN_COUNT - 1);
    if (m_bgPatternType != t) { m_bgPatternType = t; Q_EMIT renderParamsChanged(); }
}

void WallpaperProcessor::setBgPatternColor(const QColor &c)
{
    if (m_bgPatternColor != c) { m_bgPatternColor = c; Q_EMIT renderParamsChanged(); }
}

void WallpaperProcessor::setBgPatternScale(double s)
{
    s = qBound(0.3, s, 3.0);
    if (!qFuzzyCompare(m_bgPatternScale, s)) { m_bgPatternScale = s; Q_EMIT renderParamsChanged(); }
}

void WallpaperProcessor::setBgPatternRotation(double a)
{
    a = std::fmod(a, 360.0);
    if (a < 0) a += 360.0;
    if (!qFuzzyCompare(m_bgPatternRotation, a)) { m_bgPatternRotation = a; Q_EMIT renderParamsChanged(); }
}

void WallpaperProcessor::setBgPatternSpacing(double s)
{
    s = qBound(0.0, s, 2.0);
    if (!qFuzzyCompare(m_bgPatternSpacing, s)) { m_bgPatternSpacing = s; Q_EMIT renderParamsChanged(); }
}

void WallpaperProcessor::setBgPatternRandomRotate(bool on)
{
    if (m_bgPatternRandomRotate != on) { m_bgPatternRandomRotate = on; Q_EMIT renderParamsChanged(); }
}

void WallpaperProcessor::setBgPatternJitter(bool on)
{
    if (m_bgPatternJitter != on) { m_bgPatternJitter = on; Q_EMIT renderParamsChanged(); }
}

void WallpaperProcessor::setBgPatternGridAmplitude(double v)
{
    v = qBound(0.0, v, 0.5);
    if (!qFuzzyCompare(m_bgPatternGridAmplitude, v)) { m_bgPatternGridAmplitude = v; Q_EMIT renderParamsChanged(); }
}

void WallpaperProcessor::setBgPatternMixEnabled(bool on)
{
    if (m_bgPatternMixEnabled != on) { m_bgPatternMixEnabled = on; Q_EMIT renderParamsChanged(); }
}

void WallpaperProcessor::setOverlayOpacity(double o)
{
    o = qBound(0.0, o, 1.0);
    if (!qFuzzyCompare(o, m_overlayOpacity)) { m_overlayOpacity = o; Q_EMIT renderParamsChanged(); }
}

void WallpaperProcessor::setOverlayColor(const QColor &c)
{
    if (m_overlayColor != c) { m_overlayColor = c; Q_EMIT renderParamsChanged(); }
}

void WallpaperProcessor::setBlurBrightness(double b)
{
    b = qBound(0.0, b, 5.0);
    if (!qFuzzyCompare(b, m_blurBrightness)) { m_blurBrightness = b; Q_EMIT renderParamsChanged(); }
}

// ── window binding ──────────────────────────────────────────────────────

void WallpaperProcessor::setWindow(QWindow *window)
{
    m_window = window;
    if (!m_window) return;

    connect(m_window, &QWindow::screenChanged,
            this, &WallpaperProcessor::detectFromWindow);

    double dpr = m_window->devicePixelRatio();
    if (!qFuzzyCompare(m_windowDpr, dpr)) {
        m_windowDpr = dpr;
        Q_EMIT windowDprChanged();
    }

    detectFromWindow();

    // Wayland fractional DPR can arrive well after startup, and Qt emits no
    // reliable per-window DPR-changed signal across versions. Poll on a bounded
    // schedule that self-terminates once the value has stabilised (Q11) — no
    // infinite timer. QPointer guards against window destruction (B7).
    m_dprStableCount = 0;
    QTimer::singleShot(1000, this, &WallpaperProcessor::pollDpr);
}

void WallpaperProcessor::pollDpr()
{
    if (!m_window) return;   // QPointer auto-nulls on destruction (B7)
    double dpr = m_window->devicePixelRatio();
    if (!qFuzzyCompare(dpr, m_windowDpr)) {
        m_windowDpr = dpr;
        m_dprStableCount = 0;   // changed — keep watching until it settles
        Q_EMIT windowDprChanged();
        detectFromWindow();
    } else {
        ++m_dprStableCount;
    }
    // Stop once the DPR has been unchanged for several consecutive polls.
    if (m_dprStableCount < 12)
        QTimer::singleShot(1000, this, &WallpaperProcessor::pollDpr);
}

void WallpaperProcessor::setKeepAbove(bool keep)
{
    if (m_keepAbove == keep) return;
    m_keepAbove = keep;
    Q_EMIT keepAboveChanged();
    if (m_window) {
        Qt::WindowFlags cur = m_window->flags();
        if (keep) m_window->setFlags(cur | Qt::WindowStaysOnTopHint);
        else      m_window->setFlags(cur & ~Qt::WindowStaysOnTopHint);
        m_window->show();
    }
}

// ── screen detection ────────────────────────────────────────────────────

void WallpaperProcessor::detectFromWindow()
{
    if (!m_window) return;
    QScreen *screen = m_window->screen();
    if (screen && screen->size().width() > 0 && screen->size().height() > 0) {
        QSize dips = screen->size();
        qreal dpr = m_window->devicePixelRatio();
        updateScreenSize(qRound(dips.width() * dpr), qRound(dips.height() * dpr));
        m_detectAttempt = 0;
        return;
    }
    static const int MAX_RETRIES = 6;
    static const int RETRY_MS = 200;
    if (++m_detectAttempt <= MAX_RETRIES)
        QTimer::singleShot(RETRY_MS, this, &WallpaperProcessor::detectFromWindow);
    else
        m_detectAttempt = 0;
}

void WallpaperProcessor::detectScreenSize()
{
    if (m_window) { detectFromWindow(); return; }
    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen && screen->size().width() > 0 && screen->size().height() > 0) {
        updateScreenSize(screen->size().width(), screen->size().height());
        m_detectAttempt = 0;
        return;
    }
    static const int MAX_RETRIES = 6;
    static const int RETRY_MS = 200;
    if (++m_detectAttempt <= MAX_RETRIES)
        QTimer::singleShot(RETRY_MS, this, &WallpaperProcessor::detectScreenSize);
    else
        m_detectAttempt = 0;
}

void WallpaperProcessor::updateScreenSize(int w, int h)
{
    if (w < 1 || h < 1) return;
    if (m_screenWidth != w)  { m_screenWidth = w;  Q_EMIT screenWidthChanged(); }
    if (m_screenHeight != h) { m_screenHeight = h; Q_EMIT screenHeightChanged(); }
    if (m_targetWidth != w || m_targetHeight != h) {
        m_targetWidth = w;
        m_targetHeight = h;
        Q_EMIT renderParamsChanged();
    }
}

// ── async queue processing (worker pool, B8) ────────────────────────────
// Rendering runs on the QtConcurrent thread pool so the GUI never freezes.
// Each task renders and saves one image off-thread; results return via the
// QFutureWatcher on the main thread. Cancellation is a checked atomic flag.

void WallpaperProcessor::processImage(const QString &sourcePath)
{
    processQueue({sourcePath});
}

void WallpaperProcessor::processQueue(const QStringList &paths)
{
    if (paths.isEmpty() || m_busy) return;
    m_queue = paths;
    m_queueProgress.storeRelaxed(0);
    m_cancelRequested.storeRelaxed(0);
    m_busy = true;
    Q_EMIT busyChanged();
    Q_EMIT processingStarted();
    Q_EMIT queueChanged();
    Q_EMIT queueProgressChanged();
    // Capture all render parameters on the main thread. Worker tasks must
    // NOT touch members (data race on ~40 members + mood palette arrays).
    m_queueSnapshot = captureSnapshot(QImage(), m_targetWidth, m_targetHeight);
    // Yield one frame so the busy overlay paints before the first task lands.
    QTimer::singleShot(50, this, &WallpaperProcessor::startQueue);
}

void WallpaperProcessor::startQueue()
{
    const QStringList queue = m_queue;
    const int W = m_targetWidth, H = m_targetHeight;
    const RenderSnapshot base = m_queueSnapshot;   // captured on main thread

    auto task = [this, base, W, H](const QString &path) -> QPair<int, QString> {
        const int idx = m_queue.indexOf(path);
        if (m_cancelRequested.loadRelaxed())
            return qMakePair(idx, QString());
        QImage srcImage(path);
        if (srcImage.isNull())
            return qMakePair(idx, QString());
        srcImage = limitImageSize(srcImage, W, H);
        RenderSnapshot rs = base;
        rs.W = W; rs.H = H;
        rs.sourceImage = srcImage;
        // Resolve per-image mood colors under the mood mutex (the palette
        // computation writes shared arrays; the lock serialises it against
        // the main-thread preview path). Everything else in `rs` is immutable.
        if (rs.blurMode ? rs.bgZoom < 1.0
                        : (rs.bgGradientStyle == 2 || (rs.bgGradientStyle == 0 && rs.autoColor))) {
            const auto pair = resolveMoodColors(srcImage, rs.autoMood, rs.useV2);
            rs.moodColorA = pair.first.rgb();
            rs.moodColorB = pair.second.rgb();
        }
        QImage output = renderCore(rs, nullptr, nullptr, nullptr);
        if (output.isNull() || m_cancelRequested.loadRelaxed())
            return qMakePair(idx, QString());
        QFileInfo fi(path);
        QString outPath = fi.absolutePath() + QDir::separator()
                        + fi.completeBaseName() + QStringLiteral(".wp.png");
        if (!output.save(outPath, "PNG"))
            return qMakePair(idx, QString());
        return qMakePair(idx, outPath);
    };

    QFuture<QPair<int, QString>> fut = QtConcurrent::mapped(queue, task);
    m_queueWatcher.setFuture(fut);
}

void WallpaperProcessor::handleQueueResult(int index, const QString &outPath)
{
    if (index < 0 || index >= m_queue.size()) return;
    // Q3: progress counts attempts (done), not successes — a failed image
    // must not leave the determinate bar stuck below queueSize.
    m_queueProgress.fetchAndAddRelaxed(1);
    if (!outPath.isEmpty()) {
        m_outputPath = outPath;
        Q_EMIT outputPathChanged();
        m_statusMessage = i18n("[%1/%2] Saved: %3",
                               index + 1, m_queue.size(),
                               QFileInfo(outPath).fileName());
    } else if (!m_cancelRequested.loadRelaxed()) {
        m_statusMessage = i18n("Failed: %1",
                               QFileInfo(m_queue[index]).fileName());
    }
    Q_EMIT statusMessageChanged();
    Q_EMIT queueProgressChanged();
}

void WallpaperProcessor::finishQueue()
{
    m_busy = false;
    Q_EMIT busyChanged();
    Q_EMIT processingFinished();
    Q_EMIT queueProgressChanged();
}

void WallpaperProcessor::cancelProcessing()
{
    m_cancelRequested.storeRelaxed(1);
    m_queueWatcher.cancel();
}

bool WallpaperProcessor::processSingleImage(const QString &sourcePath, QString &outPath)
{
    QImage srcImage(sourcePath);
    if (srcImage.isNull()) {
        Q_EMIT errorOccurred(i18n("Cannot load: %1", QFileInfo(sourcePath).fileName()));
        return false;
    }
    srcImage = limitImageSize(srcImage, m_targetWidth, m_targetHeight);
    RenderSnapshot rs = captureSnapshot(srcImage, m_targetWidth, m_targetHeight);
    rs.sourceImage = srcImage;
    QImage output = renderCore(rs, nullptr, nullptr, &m_renderTileCache);
    QFileInfo fi(sourcePath);
    outPath = fi.absolutePath() + QDir::separator() + fi.completeBaseName() + QStringLiteral(".wp.png");
    if (!output.save(outPath, "PNG")) {
        Q_EMIT errorOccurred(i18n("Failed to save: %1", QFileInfo(outPath).fileName()));
        return false;
    }
    return true;
}

// ── Image downscale helper ───────────────────────────────────────────────
QImage WallpaperProcessor::limitImageSize(const QImage &src, int maxW, int maxH)
{
    int imgW = src.width(), imgH = src.height();
    if (imgW <= maxW && imgH <= maxH)
        return src;
    while (imgW > maxW || imgH > maxH) {
        imgW = imgW * 2 / 5;
        imgH = imgH * 2 / 5;
    }
    return src.scaled(qMax(maxW, imgW), qMax(maxH, imgH),
                      Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

// ── snapshot capture (main thread: reads members, resolves colors) ────────

RenderSnapshot WallpaperProcessor::captureSnapshot(const QImage &src, int W, int H)
{
    RenderSnapshot rs;
    rs.W = W;
    rs.H = H;
    rs.blurMode            = m_blurMode;
    rs.bgGradientStyle     = m_bgGradientStyle;
    rs.bgZoom              = m_bgZoom;
    rs.bgBlurAngle         = m_bgBlurAngle;
    rs.blurRadius          = m_blurRadius;
    rs.saturationFactor    = m_saturationFactor;
    rs.colorGamma          = m_colorGamma;
    rs.colorWarmth         = m_colorWarmth;
    rs.colorBlackLift      = m_colorBlackLift;
    rs.textureKind        = m_textureKind;
    rs.textureColor       = m_textureColor;
    rs.textureOpacity     = m_textureOpacity;
    rs.textureBlendMode   = m_textureBlendMode;
    rs.textureOverPhoto   = m_textureOverPhoto;
    rs.photoGrade         = m_photoGrade;
    rs.overlayOpacity      = m_overlayOpacity;
    rs.overlayColor        = m_overlayColor.rgb();
    rs.blurBrightness      = m_blurBrightness;
    rs.autoColor           = m_autoColor;
    rs.bgColor             = m_bgColor.rgb();
    rs.bgGradientPreset    = m_bgGradientPreset;
    rs.gradientAngle       = m_gradientAngle;
    rs.bgPatternEnabled    = m_bgPatternEnabled;
    rs.bgPatternType       = m_bgPatternType;
    rs.bgPatternColor      = m_bgPatternColor.rgb();
    rs.bgPatternScale      = m_bgPatternScale;
    rs.bgPatternRotation   = m_bgPatternRotation;
    rs.bgPatternSpacing    = m_bgPatternSpacing;
    rs.bgPatternRandomRotate = m_bgPatternRandomRotate;
    rs.bgPatternJitter     = m_bgPatternJitter;
    rs.bgPatternGridAmplitude = m_bgPatternGridAmplitude;
    rs.bgPatternMixEnabled = m_bgPatternMixEnabled;
    rs.bgPatternMixMotifs  = m_bgPatternMixMotifs;
    rs.vignetteStrength    = m_vignetteStrength;
    rs.grainStrength       = m_grainStrength;
    rs.caStrength          = m_caStrength;
    rs.photoFrame          = m_photoFrame;
    rs.photoFrameWidth     = m_photoFrameWidth;
    rs.fgZoom              = m_fgZoom;
    rs.pipZoom             = m_pipZoom;
    rs.autoMood            = m_autoMood;
    rs.useV2               = m_useV2;
    rs.sourceImage         = src;

    // Pre-resolve gradient endpoints so the render core never reads members.
    if (!src.isNull()) {
        if (!m_moodsComputed)
            computeMoodPalettes(src);
        // Mood-mode gradient uses the selected mood pair; auto-color solid uses mood 0 A.
        const auto colors = extractHarmonizedColors(src, m_autoMood);
        rs.moodColorA = colors.first.rgb();
        rs.moodColorB = colors.second.rgb();
    }
    return rs;
}

// ── unified render core (Q1) ─────────────────────────────────────────────
// Single pipeline for both full output and preview. Thread-safe: reads only
// the snapshot. Background fill, pattern overlay, effects, composition,
// shadow, frame, foreground and CA all run here exactly once.

// Procedural overlay textures (2026-08-11): preset-owned, no user assets —
// licensing-safe, resolution-independent. kind 1 = light leak (soft tinted
// blob from the top-right), 2 = polaroid frame (white border hugging the
// photo rect, transparent center), 3 = film border (soft dark edges).
QImage WallpaperProcessor::generateOverlayTexture(int kind, QRgb color, int W, int H,
                                                  int fgCx, int fgCy, int imgW, int imgH)
{
    if (kind <= 0) return {};
    QImage img(W, H, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    switch (kind) {
    case 1: {   // light leak
        const QColor c = color ? QColor(color) : QColor(0xE8, 0xA0, 0x50);
        QRadialGradient g(W, 0, qMax(W, H) * 0.75, W, 0);   // top-right corner, reaching inward
        g.setColorAt(0.0, QColor(c.red(), c.green(), c.blue(), 110));
        g.setColorAt(0.4, QColor(c.red(), c.green(), c.blue(), 55));
        g.setColorAt(1.0, QColor(c.red(), c.green(), c.blue(), 0));
        p.setBrush(g);
        p.setPen(Qt::NoPen);
        p.drawRect(0, 0, W, H);
        break;
    }
    case 2: {   // polaroid frame: white border hugging the photo, thick bottom
        const int bw = qMax(10, qMin(W, H) / 24);
        const int bh = qMax(16, bw * 3 / 2);
        const QRectF pr(fgCx - bw, fgCy - bw, imgW + 2.0 * bw, imgH + 2.0 * bw);
        p.setBrush(Qt::white);
        p.setPen(Qt::NoPen);
        p.drawRect(QRectF(pr.left(), pr.top(), pr.width(), bw));              // top
        p.drawRect(QRectF(pr.left(), pr.bottom() - bh, pr.width(), bh));      // bottom (thick)
        p.drawRect(QRectF(pr.left(), pr.top() + bw, bw, pr.height() - bw - bh));  // left
        p.drawRect(QRectF(pr.right() - bw, pr.top() + bw, bw, pr.height() - bw - bh)); // right
        break;
    }
    case 3: {   // film border: soft vignette + dark edge strips
        const int bw = qMax(10, qMin(W, H) / 40);
        QRadialGradient g(W / 2.0, H / 2.0, qMax(W, H) * 0.62);
        g.setColorAt(0.72, QColor(0, 0, 0, 0));
        g.setColorAt(1.0, QColor(0, 0, 0, 120));
        p.setBrush(g);
        p.setPen(Qt::NoPen);
        p.drawRect(0, 0, W, H);
        p.setBrush(QColor(0, 0, 0, 90));
        p.drawRect(0, 0, W, bw);
        p.drawRect(0, H - bw, W, bw);
        break;
    }
    default:
        break;
    }
    p.end();
    return img;
}

// Forward decls — the float pipeline helpers are defined below renderCore.
static void imageToFloat(const QImage &img, float *buf);
static void floatToImageDithered(const float *buf, QImage &img);
static void boostSaturationFloat(float *buf, int nPix, double factor);
static void applyColorGradeFloat(float *buf, int nPix, double gamma, double warmth, double blackLift);
static QImage gradedCopy(const QImage &src, double sat, double gamma, double warmth, double blackLift);

QImage WallpaperProcessor::renderCore(const RenderSnapshot &rs,
                                      double *outMinZoom,
                                      double *outMaxZoom,
                                      QHash<QString, QImage> *tileCache)
{
    const QImage &src = rs.sourceImage;
    const int W = rs.W, H = rs.H;
    int imgW = src.width(), imgH = src.height();
    if (imgW < 1 || imgH < 1) return QImage();

    const double fillZoom = qMax(W / (double)imgW, H / (double)imgH);

    // ── Self-similar composition: same ratio ρ at each nesting level ──
    const double RHO = WalltzDefaults::canvasMarginRho;
    const double MIN_ZOOM = WalltzDefaults::minZoom;
    const double frameRatio = rs.photoFrame ? qBound(0.0, rs.photoFrameWidth / 100.0, 0.25) : 0.0;
    const int marginW = qMax(1, (int)(W * RHO));
    const int marginH = qMax(1, (int)(H * RHO));
    const int effW = W - 2 * marginW;
    const int effH = H - 2 * marginH;
    const double imgBudgetW = effW / (1.0 + 2.0 * frameRatio);
    const double imgBudgetH = effH / (1.0 + 2.0 * frameRatio);
    const double scaleF = qMin(imgBudgetW / qMax(1, imgW), imgBudgetH / qMax(1, imgH));
    const double gImgW = imgW * scaleF;
    const double gImgH = imgH * scaleF;
    const double gFw = rs.photoFrame ? qMax(1.0, qMin(gImgW, gImgH) * frameRatio) : 0.0;
    const double gVisualW = gImgW + 2 * gFw;
    const double gVisualH = gImgH + 2 * gFw;
    const double maxZoom = qMin(1.0, qMin(W / qMax(1.0, gVisualW), H / qMax(1.0, gVisualH)));
    if (outMinZoom) *outMinZoom = MIN_ZOOM;
    if (outMaxZoom) *outMaxZoom = maxZoom;
    const double zoom = qBound(MIN_ZOOM, rs.fgZoom, maxZoom);
    imgW = qMax(1, (int)(gImgW * zoom));
    imgH = qMax(1, (int)(gImgH * zoom));
    const int effFw = rs.photoFrame ? qMax(1, (int)(qMin(imgW, imgH) * frameRatio)) : 0;
    const int totalVisualW = imgW + 2 * effFw;
    const int totalVisualH = imgH + 2 * effFw;
    // Composition center (foreground image top-left after centering).
    const int fgCx = (W - totalVisualW) / 2 + effFw;
    const int fgCy = (H - totalVisualH) / 2 + effFw;

    constexpr int SHADOW_RADIUS = 3;
    constexpr int FRAME_RADIUS  = 2;

    QImage output(W, H, QImage::Format_ARGB32_Premultiplied);
    QPainter p;
    p.begin(&output);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    // ── Background fill ──
    if (rs.blurMode) {
        const double bgz = fillZoom * rs.bgZoom;
        if (rs.bgZoom < 1.0)
            output.fill(QColor::fromRgb(rs.moodColorA));
        else
            output.fill(Qt::white);

        const double bgW = src.width() * bgz;
        const double bgH = src.height() * bgz;
        p.save();
        p.translate(W / 2.0, H / 2.0);
        if (rs.bgBlurAngle != 0.0)
            p.rotate(rs.bgBlurAngle);
        p.translate(-bgW / 2.0, -bgH / 2.0);
        p.scale(bgz, bgz);
        p.drawImage(0, 0, src);
        p.restore();
        p.fillRect(0, 0, W, H, QColor(0, 0, 0, 25));
        p.end();

        QImage blurTarget = output.copy();
        const double sigma = rs.blurRadius > 0
            ? qMax(1.0, (double)rs.blurRadius)
            : qMax(0.5, 0.017 * H);
        WallpaperProcessor::stackBlur(blurTarget, sigma, rs.saturationFactor,
                                      rs.overlayOpacity, rs.overlayColor, rs.blurBrightness,
                                      rs.colorGamma, rs.colorWarmth, rs.colorBlackLift);
        p.begin(&output);
        p.drawImage(0, 0, blurTarget);
    } else {
        // Non-blur background: gradient / solid / mood. Painter stays active.
        switch (rs.bgGradientStyle) {
        case 1: {
            const auto &preset = s_presets[qBound(0, rs.bgGradientPreset, 11)];
            const double rad = rs.gradientAngle * M_PI / 180.0;
            const double t = W * 0.5 * qAbs(qCos(rad)) + H * 0.5 * qAbs(qSin(rad));
            const double dx = t * qCos(rad), dy = t * qSin(rad);
            QLinearGradient grad(W / 2.0 - dx, H / 2.0 - dy, W / 2.0 + dx, H / 2.0 + dy);
            grad.setColorAt(0.0, QColor(preset.color1));
            grad.setColorAt(1.0, QColor(preset.color2));
            p.fillRect(0, 0, W, H, grad);
            break;
        }
        case 2: {
            const double rad = rs.gradientAngle * M_PI / 180.0;
            const double t = W * 0.5 * qAbs(qCos(rad)) + H * 0.5 * qAbs(qSin(rad));
            const double dx = t * qCos(rad), dy = t * qSin(rad);
            QLinearGradient grad(W / 2.0 - dx, H / 2.0 - dy, W / 2.0 + dx, H / 2.0 + dy);
            grad.setColorAt(0.0, QColor::fromRgb(rs.moodColorA));
            grad.setColorAt(1.0, QColor::fromRgb(rs.moodColorB));
            p.fillRect(0, 0, W, H, grad);
            break;
        }
        default: {
            QColor bgColor = rs.autoColor ? QColor::fromRgb(rs.moodColorA)
                                          : QColor::fromRgb(rs.bgColor);
            output.fill(bgColor);
            break;
        }
        }
    }

    // ── Pattern overlay (B4: now rendered in the unified core, so the async
    //    preview shows it too — previously skipped in preview) ──
    if (rs.bgPatternEnabled) {
        p.end();   // pattern snapshot needs a finished background
        renderAdaptivePatternStatic(output, rs, tileCache);
        p.begin(&output);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
    }

    // ── Vignette ──
    if (rs.vignetteStrength > 0.001) {
        const double radius = std::sqrt((W/2.0)*(W/2.0) + (H/2.0)*(H/2.0));
        const double s = rs.vignetteStrength;
        QRadialGradient vg(W / 2.0, H / 2.0, radius);
        vg.setColorAt(0.0, QColor(0, 0, 0, 0));
        vg.setColorAt(1.0 - 0.4 * s, QColor(0, 0, 0, 0));
        vg.setColorAt(1.0, QColor(0, 0, 0, qMin(255, (int)(200 * s))));
        p.fillRect(0, 0, W, H, vg);
    }

    // ── Grain (B1: local noise, no shared static — safe on worker threads) ──
    if (rs.grainStrength > 0.001) {
        const int intensity = qMax(1, (int)(15 * rs.grainStrength));
        QImage grain(W, H, QImage::Format_Grayscale8);
        for (int y = 0; y < H; ++y) {
            unsigned char *line = grain.scanLine(y);
            for (int x = 0; x < W; ++x)
                line[x] = (unsigned char)qBound(0,
                    (int)(QRandomGenerator::global()->bounded(intensity * 2 + 1))
                    - intensity + 128, 255);
        }
        p.save();
        p.setCompositionMode(QPainter::CompositionMode_SoftLight);
        p.drawImage(0, 0, grain);
        p.restore();
    }

    // ── Texture overlay UNDER the photo (leaks; film-border feel on bg) ──
    // Procedural, preset-owned (textureKind), drawn under the photo unless
    // the look asks for over-photo placement (next block).
    if (rs.textureKind > 0 && !rs.textureOverPhoto && rs.textureOpacity > 0.001) {
        const QImage tex = WallpaperProcessor::generateOverlayTexture(
            rs.textureKind, rs.textureColor, W, H, fgCx, fgCy, imgW, imgH);
        if (!tex.isNull()) {
            p.save();
            p.setCompositionMode(static_cast<QPainter::CompositionMode>(rs.textureBlendMode));
            p.setOpacity(qBound(0.0, rs.textureOpacity, 1.0));
            p.drawImage(0, 0, tex);
            p.restore();
        }
    }

    // ── Shadow (expands to include photo frame) ──
    {
        int shCx = fgCx, shCy = fgCy + 2, shW = imgW, shH = imgH;
        if (rs.photoFrame) {
            shCx = fgCx - effFw; shCy = fgCy - effFw + 2;
            shW = imgW + 2*effFw; shH = imgH + 2*effFw;
        }
        QImage sh(W, H, QImage::Format_ARGB32_Premultiplied);
        sh.fill(Qt::transparent);
        QPainter sp(&sh);
        sp.setRenderHint(QPainter::Antialiasing);
        QPainterPath shPath;
        shPath.addRoundedRect(shCx, shCy, shW, shH, SHADOW_RADIUS, SHADOW_RADIUS);
        sp.fillPath(shPath, QColor(0, 0, 0, 102));
        sp.end();
        stackBlur(sh, qMax(0.5, 0.0046 * H / 3.0));
        p.drawImage(0, 0, sh);
    }

    // ── Photo frame (built-in matte: white fill + light-gray outline) ──
    if (rs.photoFrame) {
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::white);
        p.drawRoundedRect(fgCx - effFw, fgCy - effFw, imgW + 2*effFw, imgH + 2*effFw,
                          FRAME_RADIUS, FRAME_RADIUS);
        p.setPen(QPen(QColor(200, 200, 200), 1));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(fgCx - effFw, fgCy - effFw, imgW + 2*effFw, imgH + 2*effFw,
                          FRAME_RADIUS, FRAME_RADIUS);
    }

    // ── Foreground image with rounded clip (drawn exactly once — B3) ──
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    const int clipRadius = rs.photoFrame ? FRAME_RADIUS : SHADOW_RADIUS;
    QPainterPath clipPath;
    clipPath.addRoundedRect(fgCx, fgCy, imgW, imgH, clipRadius, clipRadius);
    p.setClipPath(clipPath);
    p.translate(fgCx + imgW / 2.0, fgCy + imgH / 2.0);
    p.scale(imgW / (double)src.width() * rs.pipZoom,
            imgH / (double)src.height() * rs.pipZoom);
    // Retro look: grade the photo itself (sat + gamma/warmth/blackLift) so the
    // subject changes with the look — the blurred background gets the same
    // params inside stackBlur, keeping the composition coherent.
    const QImage photo = rs.photoGrade
        ? gradedCopy(src, rs.saturationFactor, rs.colorGamma, rs.colorWarmth, rs.colorBlackLift)
        : src;
    p.drawImage(-src.width() / 2.0, -src.height() / 2.0, photo);
    p.restore();

    // ── Texture overlay OVER the photo (retro looks: frames/borders) ──
    // The under-photo placement (before the shadow block) serves leaks; looks
    // with textureOverPhoto draw here so the texture covers the subject —
    // the XnRetro way, where the frame/leak sits on top of the image.
    // Painter still active and canvas-transformed here (before p.end()).
    if (rs.textureKind > 0 && rs.textureOverPhoto && rs.textureOpacity > 0.001) {
        const QImage tex = WallpaperProcessor::generateOverlayTexture(
            rs.textureKind, rs.textureColor, W, H, fgCx, fgCy, imgW, imgH);
        if (!tex.isNull()) {
            p.save();
            p.setCompositionMode(static_cast<QPainter::CompositionMode>(rs.textureBlendMode));
            p.setOpacity(qBound(0.0, rs.textureOpacity, 1.0));
            p.drawImage(0, 0, tex);
            p.restore();
        }
    }
    p.end();

    // ── Chromatic aberration (B1: locals renamed, no shadow of fgCx/fgCy) ──
    if (rs.caStrength > 0.001) {
        const double maxShift = rs.caStrength * std::min(W, H) * 0.05;
        const double centerX = W / 2.0, centerY = H / 2.0;
        const double maxDist = std::sqrt(centerX * centerX + centerY * centerY);
        QImage ca(W, H, QImage::Format_ARGB32_Premultiplied);
        const int bpp = 4;
        const int stride = output.bytesPerLine();
        for (int y = 0; y < H; ++y) {
            uchar *dstLine = ca.bits() + y * stride;
            for (int x = 0; x < W; ++x) {
                const double dx = (x - centerX) / maxDist;
                const double dy = (y - centerY) / maxDist;
                const int shift = (int)(std::sqrt(dx * dx + dy * dy) * maxShift);
                const int sx = (int)(dx * shift), sy = (int)(dy * shift);
                const int rx = qBound(0, x + sx, W - 1), ry = qBound(0, y + sy, H - 1);
                const int bx = qBound(0, x - sx, W - 1), by = qBound(0, y - sy, H - 1);
                const uchar *srcPx = output.constBits() + y * stride + x * bpp;
                const uchar *rPx   = output.constBits() + ry * stride + rx * bpp;
                const uchar *bPx   = output.constBits() + by * stride + bx * bpp;
                uchar *dst = dstLine + x * bpp;
                dst[0] = bPx[0];   // B shifts inward
                dst[1] = srcPx[1]; // G unchanged
                dst[2] = rPx[2];   // R shifts outward
                dst[3] = srcPx[3]; // A unchanged
            }
        }
        output = ca;
    }

    return output;
}

// ── Pattern rendering (snapshot-driven, thread-safe) ─────────────────────
// renderAdaptivePatternStatic is a free function: it reads only `rs` and an
// optional tile cache, so the render core can call it from a worker thread.

// Embedded SVG geometric primitives (Row 1: ornate, Row 2: simple line icons)
static constexpr int kSvgGeoCount = 16;
static const char *s_svgGeoData[kSvgGeoCount] = {
    R"(<svg viewBox="0 0 100 100"><rect x="10" y="10" width="80" height="80" rx="4" fill="none" stroke="currentColor" stroke-width="3"/><rect x="24" y="24" width="52" height="52" rx="3" fill="currentColor" opacity="0.25"/><rect x="38" y="38" width="24" height="24" rx="2" fill="currentColor"/></svg>)",
    R"(<svg viewBox="0 0 100 100"><polygon points="50,8 92,82 8,82" fill="none" stroke="currentColor" stroke-width="3" stroke-linejoin="round"/><polygon points="50,92 8,18 92,18" fill="currentColor" opacity="0.2" stroke="currentColor" stroke-width="2" stroke-linejoin="round"/><circle cx="50" cy="65" r="4" fill="currentColor"/></svg>)",
    R"(<svg viewBox="0 0 100 100"><circle cx="50" cy="50" r="40" fill="none" stroke="currentColor" stroke-width="3"/><circle cx="50" cy="50" r="26" fill="currentColor" opacity="0.25"/><circle cx="50" cy="50" r="12" fill="currentColor"/></svg>)",
    R"(<svg viewBox="0 0 100 100"><circle cx="50" cy="50" r="34" fill="none" stroke="currentColor" stroke-width="8"/><circle cx="50" cy="50" r="16" fill="none" stroke="currentColor" stroke-width="5"/><circle cx="50" cy="50" r="4" fill="currentColor"/></svg>)",
    R"(<svg viewBox="0 0 100 100"><g stroke="currentColor" stroke-width="3" stroke-linecap="round"><line x1="10" y1="15" x2="40" y2="15"/><line x1="60" y1="15" x2="90" y2="15"/><line x1="30" y1="30" x2="70" y2="30"/><line x1="10" y1="45" x2="50" y2="45"/><line x1="65" y1="45" x2="90" y2="45"/><line x1="20" y1="60" x2="80" y2="60"/><line x1="10" y1="75" x2="35" y2="75"/><line x1="55" y1="75" x2="75" y2="75"/><line x1="10" y1="90" x2="45" y2="90"/><line x1="60" y1="90" x2="90" y2="90"/></g></svg>)",
    R"(<svg viewBox="0 0 100 100"><g fill="none" stroke="currentColor" stroke-width="3.5" stroke-linecap="round"><path d="M8,28 Q28,8 48,28 T88,28"/><path d="M8,50 Q28,30 48,50 T88,50"/><path d="M8,72 Q28,52 48,72 T88,72"/></g></svg>)",
    R"(<svg viewBox="0 0 100 100"><g fill="currentColor"><circle cx="18" cy="18" r="4.5"/><circle cx="50" cy="14" r="3"/><circle cx="82" cy="20" r="5"/><circle cx="28" cy="42" r="3.5"/><circle cx="55" cy="34" r="5.5"/><circle cx="78" cy="48" r="3.5"/><circle cx="14" cy="68" r="4.5"/><circle cx="42" cy="62" r="3"/><circle cx="62" cy="72" r="5"/><circle cx="86" cy="65" r="4"/><circle cx="24" cy="88" r="3.5"/><circle cx="50" cy="86" r="5.5"/><circle cx="76" cy="90" r="4.5"/></g></svg>)",
    R"(<svg viewBox="0 0 100 100"><g fill="currentColor"><polygon points="20,10 30,20 20,30 10,20"/><polygon points="50,10 60,20 50,30 40,20"/><polygon points="80,10 90,20 80,30 70,20"/><polygon points="35,32 45,42 35,52 25,42"/><polygon points="65,32 75,42 65,52 55,42"/><polygon points="20,52 30,62 20,72 10,62"/><polygon points="50,52 60,62 50,72 40,62"/><polygon points="80,52 90,62 80,72 70,62"/><polygon points="35,72 45,82 35,92 25,82"/><polygon points="65,72 75,82 65,92 55,82"/></g></svg>)",
    R"(<svg viewBox="0 0 24 24"><rect x="3" y="3" width="18" height="18" rx="2" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/></svg>)",
    R"(<svg viewBox="0 0 24 24"><polygon points="12,2 22,20 2,20" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/></svg>)",
    R"(<svg viewBox="0 0 24 24"><circle cx="12" cy="12" r="10" fill="none" stroke="currentColor" stroke-width="2"/></svg>)",
    R"(<svg viewBox="0 0 24 24"><polygon points="12,2 22,12 12,22 2,12" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/></svg>)",
    R"(<svg viewBox="0 0 24 24"><circle cx="12" cy="12" r="9" fill="none" stroke="currentColor" stroke-width="2"/><circle cx="12" cy="12" r="4" fill="none" stroke="currentColor" stroke-width="2"/></svg>)",
    R"(<svg viewBox="0 0 24 24"><path d="M2,12 Q6,6 10,12 T18,12 T22,12" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/></svg>)",
    R"(<svg viewBox="0 0 24 24"><path d="M12,4 L12,20 M4,12 L20,12" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/></svg>)",
    R"(<svg viewBox="0 0 24 24"><circle cx="12" cy="12" r="8" fill="currentColor" stroke="none"/></svg>)",
};

/// Render an embedded SVG geometric icon, recoloring to target color.
static void renderSvgGeoIcon(QPainter &p, int index,
                             double x, double y, double size, const QColor &color)
{
    if (index < 0 || index >= kSvgGeoCount) return;
    QString svgData = QString::fromUtf8(s_svgGeoData[index]);
    const QString colorStr = QStringLiteral("rgb(%1,%2,%3)")
                           .arg(color.red()).arg(color.green()).arg(color.blue());
    svgData.replace(QStringLiteral("currentColor"), colorStr);
    QSvgRenderer renderer(svgData.toUtf8());
    if (!renderer.isValid()) return;
    p.save();
    QRectF viewBox = renderer.viewBoxF();
    if (viewBox.isEmpty()) viewBox = QRectF(0, 0, 100, 100);
    double scale = qMin(size / viewBox.width(), size / viewBox.height());
    double ox = x + (size - viewBox.width() * scale) / 2.0;
    double oy = y + (size - viewBox.height() * scale) / 2.0;
    p.translate(ox, oy);
    p.scale(scale, scale);
    renderer.render(&p, viewBox);
    p.restore();
}

/// Render a Lucide SVG motif icon from the QRC, recoloring strokes.
static void renderSvgMotif(QPainter &p, const QString &svgFile,
                           double x, double y, double size, const QColor &color)
{
    QFile file(QStringLiteral(":/motifs/%1.svg").arg(svgFile));
    if (!file.open(QIODevice::ReadOnly)) return;
    QString svgData = QString::fromUtf8(file.readAll());
    file.close();
    const QString colorStr = QStringLiteral("rgb(%1,%2,%3)")
                           .arg(color.red()).arg(color.green()).arg(color.blue());
    svgData.replace(QStringLiteral("currentColor"), colorStr);
    svgData.replace(QStringLiteral("stroke=\"black\""), QStringLiteral("stroke=\"%1\"").arg(colorStr));
    svgData.replace(QStringLiteral("stroke=\"#000\""), QStringLiteral("stroke=\"%1\"").arg(colorStr));
    svgData.replace(QStringLiteral("fill=\"black\""), QStringLiteral("fill=\"%1\"").arg(colorStr));
    svgData.replace(QStringLiteral("fill=\"#000\""), QStringLiteral("fill=\"%1\"").arg(colorStr));
    QSvgRenderer renderer(svgData.toUtf8());
    if (!renderer.isValid()) return;
    p.save();
    QRectF viewBox = renderer.viewBoxF();
    if (viewBox.isEmpty()) viewBox = QRectF(0, 0, 24, 24);
    double scale = qMin(size / viewBox.width(), size / viewBox.height());
    double ox = x + (size - viewBox.width() * scale) / 2.0;
    double oy = y + (size - viewBox.height() * scale) / 2.0;
    p.translate(ox, oy);
    p.scale(scale, scale);
    renderer.render(&p, viewBox);
    p.restore();
}

// ── Geometric pattern tile generator (const, Q6) ──────────────────────────

QImage WallpaperProcessor::generateGeometricTile(int type, int tileSize,
                                                  const QColor &bg, const QColor &fg,
                                                  double scale)
{
    int S = tileSize;
    if (S < 10) S = 10;
    QImage tile(S, S, QImage::Format_ARGB32_Premultiplied);
    tile.fill(bg);
    QPainter p(&tile);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(fg);
    const double hS = S / 2.0;

    switch (type) {
    case 0: { // Dots
        double spacing = S / 4.0;
        double r = qMax(1.0, S * 0.08 * scale);
        for (double y = 0; y <= S; y += spacing)
            for (double x = 0; x <= S; x += spacing)
                p.drawEllipse(QPointF(x, y), r, r);
        break;
    }
    case 1: { // Stripes H
        double spacing = qMax(2.0, S / 5.0);
        double h = qMax(1.0, spacing * 0.4);
        for (double y = 0; y < S; y += spacing)
            p.drawRect(QRectF(0, y, S, h));
        break;
    }
    case 2: { // Stripes V
        double spacing = qMax(2.0, S / 5.0);
        double w = qMax(1.0, spacing * 0.4);
        for (double x = 0; x < S; x += spacing)
            p.drawRect(QRectF(x, 0, w, S));
        break;
    }
    case 3: { // Stripes D
        double spacing = qMax(2.0, S * 0.3);
        double sw = qMax(1.0, spacing * 0.25);
        p.save();
        p.rotate(45);
        double len = S * 1.5;
        for (double d = -len; d < len; d += spacing)
            p.drawRect(QRectF(d, -len, sw, len * 2));
        p.restore();
        break;
    }
    case 4: { // Checkerboard
        int cells = 4;
        double cs = S / (double)cells;
        for (int r = 0; r < cells; ++r)
            for (int c = 0; c < cells; ++c)
                if ((r + c) % 2 == 1)
                    p.drawRect(QRectF(c * cs, r * cs, cs, cs));
        break;
    }
    case 5: { // Chevron
        double step = S / 4.0;
        double sw = step * 0.3;
        for (double y = -step; y < S + step; y += step) {
            QPolygonF chev;
            chev << QPointF(0, y) << QPointF(hS, y + sw)
                 << QPointF(S, y) << QPointF(hS, y - sw);
            p.drawPolygon(chev);
        }
        break;
    }
    case 6: { // Diamonds
        double spacing = S / 3.0;
        double hs = spacing * 0.6;
        for (double y = -spacing; y < S + spacing; y += spacing)
            for (double x = -spacing; x < S + spacing; x += spacing) {
                QPolygonF dia;
                dia << QPointF(x, y - hs) << QPointF(x + hs, y)
                    << QPointF(x, y + hs) << QPointF(x - hs, y);
                p.drawPolygon(dia);
            }
        break;
    }
    case 7: { // Crosshatch
        double spacing = qMax(2.0, S * 0.2);
        double sw = qMax(1.0, spacing * 0.15);
        p.save();
        double len = S * 1.5;
        for (double d = -len; d < len; d += spacing)
            p.drawRect(QRectF(d, -len, sw, len * 2));
        p.rotate(90);
        for (double d = -len; d < len; d += spacing)
            p.drawRect(QRectF(d, -len, sw, len * 2));
        p.restore();
        break;
    }
    }
    p.end();
    return tile;
}

// ── Motif tile generator (const) ───────────────────────────────────────────

QImage WallpaperProcessor::generateMotifTile(int motifIndex, int tileSize,
                                              const QColor &bg, const QColor &fg,
                                              double /*scale*/)
{
    motifIndex = qBound(0, motifIndex, MOTIF_PATTERN_COUNT - 1);
    int S = qMax(20, tileSize);
    QImage tile(S, S, QImage::Format_ARGB32_Premultiplied);
    tile.fill(bg);
    QPainter p(&tile);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    const QString svgFile = QString::fromUtf8(s_motifSvgFiles[motifIndex]);
    double iconSize = S * 0.55;
    double margin = (S - iconSize) / 2.0;
    if (S > iconSize * 1.8) {
        renderSvgMotif(p, svgFile, margin, margin, iconSize, fg);
    } else {
        double half = S / 2.0;
        double is = half * 0.55;
        double hs = (half - is) / 2.0;
        for (int row = 0; row < 2; ++row)
            for (int col = 0; col < 2; ++col)
                renderSvgMotif(p, svgFile, col * half + hs, row * half + hs, is, fg);
    }
    p.end();
    return tile;
}

// ── SVG geometric tile generator (const) ──────────────────────────────────

QImage WallpaperProcessor::generateSvgGeoTile(int index, int tileSize,
                                               const QColor &bg, const QColor &fg,
                                               double /*scale*/)
{
    index = qBound(0, index, SVG_GEO_COUNT - 1);
    int S = qMax(20, tileSize);
    QImage tile(S, S, QImage::Format_ARGB32_Premultiplied);
    tile.fill(bg);
    QPainter p(&tile);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    double iconSize = S * 0.75;
    double margin = (S - iconSize) / 2.0;
    renderSvgGeoIcon(p, index, margin, margin, iconSize, fg);
    p.end();
    return tile;
}

// ── Static adaptive pattern renderer (Q14 tile cache + thread-safe) ───────
// Free function used by renderCore. Generates tiles via stateless static
// helpers, caches rendered tiles keyed by (kind, index, size, color).






// Render a single tile for the given decoded pattern (uses cache when given).
static QImage patternTileFor(const PatternRef &ref, int tileSize, const QColor &fg,
                             double scale, QHash<QString, QImage> *cache)
{
    QString key;
    if (cache)
        key = QStringLiteral("%1:%2:%3:%4:%5")
              .arg((int)ref.kind).arg(ref.index).arg(tileSize)
              .arg(fg.rgba(), 0, 16).arg(scale);
    if (cache && cache->contains(key))
        return cache->value(key);

    const QColor transparent(0, 0, 0, 0);
    QImage tile;
    switch (ref.kind) {
    case PatternKind::Motif:    tile = WallpaperProcessor::generateMotifTile(ref.index, tileSize, transparent, fg, scale); break;
    case PatternKind::SvgGeo:   tile = WallpaperProcessor::generateSvgGeoTile(ref.index, tileSize, transparent, fg, scale); break;
    case PatternKind::Geometric: tile = WallpaperProcessor::generateGeometricTile(ref.index, tileSize, transparent, fg, scale); break;
    }
    if (cache && !tile.isNull())
        cache->insert(key, tile);
    return tile;
}

void WallpaperProcessor::renderAdaptivePatternStatic(QImage &output, const RenderSnapshot &rs,
                                                      QHash<QString, QImage> *tileCache)
{
    const int W = output.width(), H = output.height();
    const int baseTile = qMax(20, (int)(80 * rs.bgPatternScale));
    const int tileW = baseTile, tileH = baseTile;

    double minExtra = 0.0;
    if (rs.bgPatternJitter)        minExtra = qMax(minExtra, rs.bgPatternGridAmplitude * 0.8);
    if (rs.bgPatternRandomRotate)  minExtra = qMax(minExtra, 0.12);
    const double spacingFactor = 1.0 + qMax(rs.bgPatternSpacing, minExtra);
    const int stepX = qMax(tileW, (int)(tileW * spacingFactor));
    const int stepY = qMax(tileH, (int)(tileH * spacingFactor));
    const int countX = (W + stepX - 1) / stepX;
    const int countY = (H + stepY - 1) / stepY;
    const int gridOffX = (W - countX * stepX) / 2;
    const int gridOffY = (H - countY * stepY) / 2;

    // Global pattern color from dominant background near center.
    QColor globalPatColor(180, 180, 180);
    {
        const int cx = W/2, cy = H/2;
        int half = qMin(W, H) / 6; if (half < 4) half = 4;
        double rSum=0,gSum=0,bSum=0; int samples=0;
        for (int dy=-half; dy<=half; dy+=half/2)
            for (int dx=-half; dx<=half; dx+=half/2) {
                QColor c = output.pixelColor(qBound(0,cx+dx,W-1), qBound(0,cy+dy,H-1));
                rSum+=c.red(); gSum+=c.green(); bSum+=c.blue(); ++samples;
            }
        if (samples>0) {
            QColor avg((int)(rSum/samples),(int)(gSum/samples),(int)(bSum/samples));
            double lum=(0.299*avg.red()+0.587*avg.green()+0.114*avg.blue())/255.0;
            if (lum>0.5)
                globalPatColor=QColor(qMax(0,(int)(avg.red()*0.25-20)),qMax(0,(int)(avg.green()*0.25-20)),qMax(0,(int)(avg.blue()*0.25-20)));
            else
                globalPatColor=QColor(qMin(255,(int)(avg.red()*0.5+140)),qMin(255,(int)(avg.green()*0.5+140)),qMin(255,(int)(avg.blue()*0.5+140)));
        }
    }

    QPainter p(&output);
    for (int ty=0; ty<countY; ++ty) {
        for (int tx=0; tx<countX; ++tx) {
            int tileX = gridOffX + tx*stepX;
            int tileY = gridOffY + ty*stepY;
            if (rs.bgPatternJitter) {
                double amp = qBound(0.0, rs.bgPatternGridAmplitude, 0.5);
                double period = 5.0;
                tileX += (int)(amp*baseTile*sin(2.0*M_PI*ty/period)+0.5);
                tileY += (int)(amp*baseTile*cos(2.0*M_PI*tx/period)+0.5);
            }
            unsigned int seed=(unsigned int)(tx*374761393u+ty*668265263u);
            seed=(seed^(seed>>13))*1274126177u;
            double randomAngle=(double)(seed%36001)/100.0;

            PatternRef ref;
            if (rs.bgPatternMixEnabled && rs.bgPatternMixMotifs.size() > 1) {
                int n = rs.bgPatternMixMotifs.size();
                int cycleIdx = n >= 4 ? (tx + 2*ty) % n : (tx + ty) % n;
                ref = decodePatternType(rs.bgPatternMixMotifs[cycleIdx]);
            } else {
                ref = decodePatternType(rs.bgPatternType);
            }

            QImage tile = patternTileFor(ref, tileW, globalPatColor, rs.bgPatternScale, tileCache);
            if (tile.isNull()) continue;

            if (rs.bgPatternRandomRotate) {
                QPointF center(tile.width()/2.0, tile.height()/2.0);
                QTransform tf = QTransform().translate(center.x(),center.y())
                                            .rotate(randomAngle)
                                            .translate(-center.x(),-center.y());
                tile = tile.transformed(tf, Qt::SmoothTransformation);
            }
            tileX += (tileW - tile.width())/2;
            tileY += (tileH - tile.height())/2;
            p.save();
            p.setOpacity(0.35);
            p.drawImage(tileX, tileY, tile);
            p.restore();
        }
    }
    p.end();
}

// ── live preview (async, worker thread) ──────────────────────────────────

QString WallpaperProcessor::generatePreview(const QString &sourcePath)
{
    QElapsedTimer t_total; t_total.start();
    const int renderId = m_nextRenderId.fetchAndAddAcquire(1);

    // Cap preview resolution: full pipeline runs at this size, scaled down by QML.
    const int maxPv = 600;
    const double aspect = (double)m_targetWidth / qMax(1, m_targetHeight);
    int pvW, pvH;
    if (aspect > 1.0) { pvW = maxPv; pvH = qMax(1, (int)(maxPv / aspect)); }
    else              { pvH = maxPv; pvW = qMax(1, (int)(maxPv * aspect)); }

    QImage src(sourcePath);
    PROF_LOG(renderId, "QImage::load %lld ms", t_total.elapsed());

    // Smart Auto needs fresh stats from the actual source before snapshotting.
    if (m_blurPresetId == QStringLiteral("auto") && !src.isNull()) {
        blockSignals(true);
        m_imageStats = analyseImage(src);
        m_statsValid = true;
        computeSmartAutoAndApply();
        blockSignals(false);
    }

    // Compute mood palettes synchronously (fast at 64px sampling) for QML row.
    if (!src.isNull())
        computeMoodPalettes(src);

    RenderSnapshot rs = captureSnapshot(src, pvW, pvH);
    rs.sourceImage = src;
    // Scale blur radius to preview size so the visual effect matches full-res.
    rs.blurRadius = (int)(rs.blurRadius * ((double)qMax(pvW, pvH) / qMax(m_targetWidth, m_targetHeight)));

    QFileInfo fi(sourcePath);
    const QString tmpDir = QDir::tempPath() + QStringLiteral("/walltz");
    QDir().mkpath(tmpDir);
    const QString sourceHash = QString::fromLatin1(QCryptographicHash::hash(
        sourcePath.toUtf8(), QCryptographicHash::Md5).toHex().left(16));
    const QString tmpName = QStringLiteral("pv_") + sourceHash
        + QStringLiteral("_") + QString::number(renderId) + QStringLiteral(".png");
    const QString tmpPath = tmpDir + QDir::separator() + tmpName;

    // Fire-and-forget render on the thread pool.
    Q_UNUSED(QtConcurrent::run([this, sourcePath, tmpPath, tmpName, sourceHash, tmpDir, rs, renderId]() {
        QElapsedTimer t_lambda; t_lambda.start();
        QImage srcImage = rs.sourceImage;
        if (srcImage.isNull()) { PROF_LOG(renderId, "%s", "LAMBDA src is NULL"); return; }
        srcImage = WallpaperProcessor::limitImageSize(srcImage, rs.W, rs.H);

        double minZoom = WalltzDefaults::minZoom, maxZoom = 1.0;
        QImage preview = renderCore(rs, &minZoom, &maxZoom, nullptr);
        PROF_LOG(renderId, "renderCore %lld ms", t_lambda.elapsed());
        if (preview.isNull()) return;

        QImageWriter writer(tmpPath, "png");
        writer.setCompression(0);
        if (!writer.write(preview)) { PROF_LOG(renderId, "%s", "QImageWriter::write FAILED"); return; }

        const QString url = QStringLiteral("file://") + tmpPath;

        // Purge stale previews for this source (strictly older render IDs, B9).
        QDir dir(tmpDir);
        QStringList filters{ QStringLiteral("pv_") + sourceHash + QStringLiteral("_*.png") };
        for (const QString &fn : dir.entryList(filters, QDir::Files, QDir::Name)) {
            if (fn == tmpName) continue;
            const int underscore = fn.lastIndexOf(QLatin1Char('_'));
            const int dot = fn.lastIndexOf(QLatin1Char('.'));
            if (underscore > 0 && dot > underscore) {
                bool ok = false;
                int oldId = fn.mid(underscore + 1, dot - underscore - 1).toInt(&ok);
                if (ok && oldId < renderId)
                    QFile::remove(dir.absoluteFilePath(fn));
            }
        }

        QMetaObject::invokeMethod(this, [this, sourcePath, url, renderId, minZoom, maxZoom]() {
            if (renderId < m_currentRenderId) return;   // stale (B10: main-thread only)
            m_currentRenderId = renderId;
            m_lastPreviewUrl = url;
            m_fgZoomMin = minZoom;
            m_fgZoomMax = maxZoom;
            Q_EMIT fgZoomBoundsChanged();
            Q_EMIT previewReady(sourcePath, url);
        }, Qt::QueuedConnection);
    }));

    PROF_LOG(renderId, "generatePreview returns %lld ms", t_total.elapsed());
    return m_lastPreviewUrl;
}

// ── harmonized color extraction (saturation-weighted hue histogram) ──────

void WallpaperProcessor::computeMoodPalettes(const QImage &image)
{
    QMutexLocker lock(&m_moodMutex);
    static const int HUE_BINS = 24;
    static const float SAT_THRESHOLD = 0.15f;
    static const float LIGHT_MIN = 0.15f;
    static const float LIGHT_MAX = 0.90f;

    // Q6: ARGB32_Premultiplied channels are premultiplied — convert to RGB32
    // so the hue math sees straight colour values (opaque photos were fine,
    // transparent PNGs were wrong).
    QImage src = image.format() == QImage::Format_ARGB32_Premultiplied
        ? image.convertToFormat(QImage::Format_RGB32) : image;
    int w = src.width(), h = src.height();
    int step = qMax(1, qMax(w, h) / 64);

    double hueWeight[HUE_BINS] = {0};
    double hueSat[HUE_BINS] = {0};
    double hueLight[HUE_BINS] = {0};
    int hueCount[HUE_BINS] = {0};

    float maxSat = 0.0f;
    int keyR = 128, keyG = 128, keyB = 128;

    for (int y = 0; y < h; y += step) {
        const QRgb *row = reinterpret_cast<const QRgb *>(src.constScanLine(y));
        for (int x = 0; x < w; x += step) {
            QRgb px = row[x];
            int r = qRed(px), g = qGreen(px), b = qBlue(px);
            int mn = qMin(qMin(r, g), b);
            int mx = qMax(qMax(r, g), b);
            float sat = (mx == 0) ? 0.0f : (mx - mn) / (float)mx;
            float lgt = (mx + mn) / 510.0f;
            if (sat < SAT_THRESHOLD || lgt < LIGHT_MIN || lgt > LIGHT_MAX)
                continue;
            float hue = 0.0f;
            float delta = mx - mn;
            if (delta > 0) {
                if (mx == r)      hue = (g - b) / delta;
                else if (mx == g) hue = 2.0f + (b - r) / delta;
                else              hue = 4.0f + (r - g) / delta;
                hue /= 6.0f;
                if (hue < 0) hue += 1.0f;
            }
            int bin = qMin(int(hue * HUE_BINS), HUE_BINS - 1);
            double wgt = qMax(sat * 100.0, 1.0);
            hueWeight[bin] += wgt;
            hueSat[bin] += sat * wgt;
            hueLight[bin] += lgt * wgt;
            hueCount[bin]++;
            float score = sat * qMax(0.0f, lgt - 0.15f) * 1.5f;
            if (score > maxSat) { maxSat = score; keyR = r; keyG = g; keyB = b; }
        }
    }

    int best1 = 0, best2 = 0;
    double bestW1 = 0, bestW2 = 0;
    for (int i = 0; i < HUE_BINS; ++i) {
        if (hueWeight[i] > bestW1) { bestW2 = bestW1; best2 = best1; bestW1 = hueWeight[i]; best1 = i; }
        else if (hueWeight[i] > bestW2) { bestW2 = hueWeight[i]; best2 = i; }
    }

    QColor key(keyR, keyG, keyB);
    float hK, sK, lK;
    key.getHslF(&hK, &sK, &lK);

    auto binColor = [&](int bin, float defaultSat, float defaultLight) -> QColor {
        float h = (bin + 0.5f) / HUE_BINS;
        float s = (hueCount[bin] > 0) ? hueSat[bin] / hueWeight[bin] : defaultSat;
        float l = (hueCount[bin] > 0) ? hueLight[bin] / hueWeight[bin] : defaultLight;
        s = qBound(0.35f, s, 0.75f);
        l = qBound(0.35f, l, 0.70f);
        return QColor::fromHslF(h, s, l);
    };

    // Golden-angle shift (137.5° ≈ 0.38197 of a turn) — harmonious secondary hue (F4).
    static const float GOLDEN_TURN = 0.38197f;

    if (bestW1 < 1.0) {
        for (int m = 0; m < 6; ++m) {
            m_moodColorsA[m] = QColor(128, 128, 128);
            m_moodColorsB[m] = QColor(180, 180, 180);
        }
    } else {
        // Mood 0: Auto — top-2 weighted bins, golden-angle B fallback.
        QColor autoA = binColor(best1, 0.50f, 0.50f);
        float hueA = (best1 + 0.5f) / HUE_BINS;
        float hueB;
        if (bestW2 < 1.0) {
            hueB = fmod(hueA + GOLDEN_TURN, 1.0f);
        } else {
            hueB = (best2 + 0.5f) / HUE_BINS;
            float hD = hK - hueB;
            if (hD > 0.5f) hD -= 1.0f;
            if (hD < -0.5f) hD += 1.0f;
            hueB = fmod(hueB + hD * 0.2f + 1.0f, 1.0f);
        }
        float sB = qBound(0.30f, (autoA.hslSaturationF() + sK) * 0.45f, 0.65f);
        float lB = qBound(0.40f, autoA.lightnessF() + 0.15f, 0.78f);
        QColor autoB = QColor::fromHslF(hueB, sB, lB);
        float spread = qAbs(autoB.hslHueF() - autoA.hslHueF());
        if (spread > 0.5f) spread = 1.0f - spread;
        if (spread < 0.014f)
            autoB = QColor::fromHslF(fmod(hueA + GOLDEN_TURN, 1.0f), sB, lB);
        m_moodColorsA[0] = autoA;
        m_moodColorsB[0] = autoB;

        auto pickMoodBin = [&](const std::function<bool(int)> &pred) -> int {
            for (int i = 0; i < HUE_BINS; ++i)
                if (hueWeight[i] > 0 && pred(i)) return i;
            return -1;
        };

        // Mood 1: Soft — mid-sat, mid-light; B via golden angle.
        {
            int sb = pickMoodBin([&](int i){
                float s = hueSat[i]/hueWeight[i];
                float l = hueLight[i]/hueWeight[i];
                return s >= 0.25f && s <= 0.60f && l >= 0.35f && l <= 0.70f;
            });
            float h = (sb >= 0) ? ((sb + 0.5f) / HUE_BINS) : ((best1 + 0.5f) / HUE_BINS);
            float sV = qBound(0.28f, (sb >= 0) ? hueSat[sb]/hueWeight[sb] : 0.40f, 0.48f);
            float lV = qBound(0.38f, (sb >= 0) ? hueLight[sb]/hueWeight[sb] : 0.52f, 0.62f);
            m_moodColorsA[1] = QColor::fromHslF(h, sV, lV);
            m_moodColorsB[1] = QColor::fromHslF(fmod(h + GOLDEN_TURN, 1.0f),
                qBound(0.22f, sV*0.85f, 0.40f), qBound(0.48f, lV+0.10f, 0.72f));
        }

        // Mood 2: Vivid — highest sat×light.
        {
            int vb = best1;
            double vs = -1;
            for (int i = 0; i < HUE_BINS; ++i) {
                if (hueWeight[i] <= 0) continue;
                double sc = (hueSat[i]/hueWeight[i]) * (hueLight[i]/hueWeight[i]);
                if (sc > vs) { vs = sc; vb = i; }
            }
            float hV = (vb + 0.5f) / HUE_BINS;
            float sV = qBound(0.55f, (hueCount[vb] > 0) ? hueSat[vb]/hueWeight[vb] : 0.65f, 0.85f);
            float lV = qBound(0.40f, (hueCount[vb] > 0) ? hueLight[vb]/hueWeight[vb] : 0.55f, 0.68f);
            m_moodColorsA[2] = QColor::fromHslF(hV, sV, lV);
            float h2 = (best2 != vb && bestW2 > 1.0)
                ? ((best2 + 0.5f) / HUE_BINS) : fmod(hV + GOLDEN_TURN, 1.0f);
            m_moodColorsB[2] = QColor::fromHslF(h2,
                qBound(0.45f, sV*0.80f, 0.70f), qBound(0.45f, lV+0.10f, 0.72f));
        }

        // Mood 3: Warm — hues 0-60° (bins 0-3).
        {
            int wb = -1;
            for (int i = 0; i <= 3; ++i)
                if (hueWeight[i] > 0) { wb = i; break; }
            float hW = (wb >= 0) ? ((wb + 0.5f) / HUE_BINS) : 0.10f;
            float sW = qBound(0.40f, (wb >= 0 && hueCount[wb] > 0) ? hueSat[wb]/hueWeight[wb] : 0.55f, 0.72f);
            float lW = qBound(0.38f, (wb >= 0 && hueCount[wb] > 0) ? hueLight[wb]/hueWeight[wb] : 0.50f, 0.65f);
            m_moodColorsA[3] = QColor::fromHslF(hW, sW, lW);
            m_moodColorsB[3] = QColor::fromHslF(fmod(hW + GOLDEN_TURN, 1.0f),
                qBound(0.35f, sW*0.85f, 0.60f), qBound(0.42f, lW+0.12f, 0.72f));
        }

        // Mood 4: Cool — hues 180-270° (bins 12-17).
        {
            int cb = -1;
            for (int i = 12; i <= 17; ++i)
                if (hueWeight[i] > 0) { cb = i; break; }
            float hC = (cb >= 0) ? ((cb + 0.5f) / HUE_BINS) : 0.60f;
            float sC = qBound(0.40f, (cb >= 0 && hueCount[cb] > 0) ? hueSat[cb]/hueWeight[cb] : 0.50f, 0.72f);
            float lC = qBound(0.38f, (cb >= 0 && hueCount[cb] > 0) ? hueLight[cb]/hueWeight[cb] : 0.50f, 0.65f);
            m_moodColorsA[4] = QColor::fromHslF(hC, sC, lC);
            m_moodColorsB[4] = QColor::fromHslF(fmod(hC + GOLDEN_TURN, 1.0f),
                qBound(0.35f, sC*0.85f, 0.60f), qBound(0.42f, lC+0.12f, 0.72f));
        }

        // Mood 5: Deep — lowest-lightness bin.
        {
            int db = best1;
            double ml = 1.0;
            for (int i = 0; i < HUE_BINS; ++i) {
                if (hueWeight[i] <= 0) continue;
                float l = hueLight[i] / hueWeight[i];
                if (l < ml) { ml = l; db = i; }
            }
            float hD = (db + 0.5f) / HUE_BINS;
            float sD = qBound(0.35f, (hueCount[db] > 0) ? hueSat[db]/hueWeight[db] : 0.40f, 0.60f);
            float lD = qBound(0.20f, (hueCount[db] > 0) ? hueLight[db]/hueWeight[db] : 0.35f, 0.40f);
            m_moodColorsA[5] = QColor::fromHslF(hD, sD, lD);
            m_moodColorsB[5] = QColor::fromHslF(fmod(hD + GOLDEN_TURN, 1.0f),
                qBound(0.30f, sD*0.90f, 0.50f), qBound(0.30f, lD+0.12f, 0.50f));
        }
    }

    m_moodsComputed = true;
    computeMoodPalettesV2(image);
}

// ── V2: 3D RGB histogram mood palettes ────────────────────────────────────

namespace {
QColor wp_colorFromCentroid(double r, double g, double b)
{
    return QColor(qBound(0, (int)qRound(r), 255),
                  qBound(0, (int)qRound(g), 255),
                  qBound(0, (int)qRound(b), 255));
}
}

void WallpaperProcessor::computeMoodPalettesV2(const QImage &image)
{
    QMutexLocker lock(&m_moodMutex);
    static const int RGB_BINS = 8;
    static const float SAT_THRESHOLD = 0.12f;
    static const float LIGHT_MIN = 0.10f;
    static const float LIGHT_MAX = 0.92f;

    // Q6: un-premultiply before reading channels (see computeMoodPalettes).
    QImage src = image.format() == QImage::Format_ARGB32_Premultiplied
        ? image.convertToFormat(QImage::Format_RGB32) : image;
    int w = src.width(), h = src.height();
    int step = qMax(1, qMax(w, h) / 64);

    struct Bin { double weight = 0; double rSum = 0, gSum = 0, bSum = 0; int count = 0; };
    Bin bins[RGB_BINS][RGB_BINS][RGB_BINS];

    for (int y = 0; y < h; y += step) {
        const QRgb *row = reinterpret_cast<const QRgb *>(src.constScanLine(y));
        for (int x = 0; x < w; x += step) {
            QRgb px = row[x];
            int r = qRed(px), g = qGreen(px), b = qBlue(px);
            int mn = qMin(qMin(r, g), b);
            int mx = qMax(qMax(r, g), b);
            float sat = (mx == 0) ? 0.0f : (mx - mn) / (float)mx;
            float lgt = (mx + mn) / 510.0f;
            if (sat < SAT_THRESHOLD || lgt < LIGHT_MIN || lgt > LIGHT_MAX) continue;
            int ri = qMin(r * RGB_BINS / 256, RGB_BINS - 1);
            int gi = qMin(g * RGB_BINS / 256, RGB_BINS - 1);
            int bi = qMin(b * RGB_BINS / 256, RGB_BINS - 1);
            double wgt = sat * sat * (1.0 - qAbs(0.5 - lgt));
            bins[ri][gi][bi].weight += wgt;
            bins[ri][gi][bi].rSum += r * wgt;
            bins[ri][gi][bi].gSum += g * wgt;
            bins[ri][gi][bi].bSum += b * wgt;
            bins[ri][gi][bi].count++;
        }
    }

    std::vector<Centroid3D> centroids;
    for (int ri = 0; ri < RGB_BINS; ++ri)
        for (int gi = 0; gi < RGB_BINS; ++gi)
            for (int bi = 0; bi < RGB_BINS; ++bi) {
                const auto &bin = bins[ri][gi][bi];
                if (bin.weight < 0.5) continue;
                double rAvg = bin.rSum / bin.weight;
                double gAvg = bin.gSum / bin.weight;
                double bAvg = bin.bSum / bin.weight;
                double mnC = qMin(qMin(rAvg, gAvg), bAvg);
                double mxC = qMax(qMax(rAvg, gAvg), bAvg);
                double chroma = (mxC == 0) ? 0.0 : (mxC - mnC) / mxC;
                centroids.push_back({rAvg, gAvg, bAvg, bin.weight * chroma * chroma, ri, gi, bi, bin.count});
            }

    if (centroids.empty()) {
        for (int m = 0; m < 6; ++m) { m_moodColorsV2A[m] = QColor(128,128,128); m_moodColorsV2B[m] = QColor(180,180,180); }
        m_moodsComputed = true;
        return;
    }

    std::sort(centroids.begin(), centroids.end(),
        [](const Centroid3D &a, const Centroid3D &b) { return a.score > b.score; });

    std::vector<Centroid3D *> picks;
    picks.push_back(&centroids[0]);
    for (size_t i = 1; i < centroids.size() && picks.size() < 3; ++i) {
        bool ok = true;
        for (auto *pp : picks) {
            if (qAbs(centroids[i].ri - pp->ri) < 2 && qAbs(centroids[i].gi - pp->gi) < 2 && qAbs(centroids[i].bi - pp->bi) < 2) { ok = false; break; }
        }
        if (ok) picks.push_back(&centroids[i]);
    }

    QColor colors[3] = {
        wp_colorFromCentroid(picks[0]->r, picks[0]->g, picks[0]->b),
        picks.size() > 1 ? wp_colorFromCentroid(picks[1]->r, picks[1]->g, picks[1]->b) : wp_colorFromCentroid(picks[0]->r, picks[0]->g, picks[0]->b),
        picks.size() > 2 ? wp_colorFromCentroid(picks[2]->r, picks[2]->g, picks[2]->b) : wp_colorFromCentroid(picks[0]->r, picks[0]->g, picks[0]->b)
    };

    QColor softColors[3];
    for (int i = 0; i < 3; ++i) {
        float h, s, l;
        colors[i].getHslF(&h, &s, &l);
        s = qBound(0.25f, s * 0.60f, 0.55f);
        l = qBound(0.38f, l, 0.70f);
        softColors[i] = QColor::fromHslF(h, s, l);
    }

    auto pairByMaxContrast = [&]() -> QPair<QColor, QColor> {
        double bestDist = -1; int bestA = 0, bestB = 1;
        for (int i = 0; i < 3; ++i)
            for (int j = i+1; j < 3; ++j) {
                double dr = softColors[i].redF() - softColors[j].redF();
                double dg = softColors[i].greenF() - softColors[j].greenF();
                double db = softColors[i].blueF() - softColors[j].blueF();
                double dist = dr*dr + dg*dg + db*db;
                if (dist > bestDist) { bestDist = dist; bestA = i; bestB = j; }
            }
        return {softColors[bestA], softColors[bestB]};
    };

    { auto pp = pairByMaxContrast(); m_moodColorsV2A[0] = pp.first; m_moodColorsV2B[0] = pp.second; }
    { float h,s,l; softColors[0].getHslF(&h,&s,&l);
      m_moodColorsV2A[1] = QColor::fromHslF(h, qBound(0.20f, s*0.75f, 0.40f), qBound(0.40f, l*0.95f, 0.60f));
      m_moodColorsV2B[1] = QColor::fromHslF(fmod(h+1.0f/24.0f,1.0f), qBound(0.18f, s*0.70f, 0.35f), qBound(0.42f, l*1.05f, 0.65f)); }
    { int best=0; float maxS=0;
      for (int i=0;i<3;++i){ float h,s,l; softColors[i].getHslF(&h,&s,&l); if(s>maxS){maxS=s;best=i;} }
      float h,s,l; softColors[best].getHslF(&h,&s,&l);
      m_moodColorsV2A[2]=QColor::fromHslF(h, qBound(0.35f,s*1.15f,0.65f), qBound(0.40f,l,0.65f));
      m_moodColorsV2B[2]=QColor::fromHslF(fmod(h+1.0f/4.0f,1.0f), qBound(0.25f,s*0.70f,0.45f), qBound(0.38f,l+0.05f,0.70f)); }
    { int warmIdx=0; float warmestDist=1.0f;
      for (int i=0;i<3;++i){ float h,s,l; softColors[i].getHslF(&h,&s,&l); float d=qMin(qAbs(h-0.05f),qAbs(h-0.95f)); if(d<warmestDist){warmestDist=d;warmIdx=i;} }
      float h,s,l; softColors[warmIdx].getHslF(&h,&s,&l);
      m_moodColorsV2A[3]=QColor::fromHslF(h, qBound(0.30f,s*0.90f,0.50f), qBound(0.40f,l+0.02f,0.65f));
      m_moodColorsV2B[3]=QColor::fromHslF(fmod(h+1.0f/16.0f,1.0f), qBound(0.25f,s*0.75f,0.40f), qBound(0.45f,l+0.08f,0.72f)); }
    { int coolIdx=0; float coolestD=999;
      for (int i=0;i<3;++i){ float h,s,l; softColors[i].getHslF(&h,&s,&l); float d=qAbs(h-0.58f); if(d<coolestD){coolestD=d;coolIdx=i;} }
      float h,s,l; softColors[coolIdx].getHslF(&h,&s,&l);
      m_moodColorsV2A[4]=QColor::fromHslF(h, qBound(0.22f,s*0.80f,0.42f), qBound(0.42f,l+0.02f,0.68f));
      m_moodColorsV2B[4]=QColor::fromHslF(fmod(h-1.0f/18.0f+1.0f,1.0f), qBound(0.20f,s*0.70f,0.35f), qBound(0.45f,l+0.08f,0.72f)); }
    { int darkIdx=0; double minL=softColors[0].lightnessF();
      for (int i=1;i<3;++i){ if(softColors[i].lightnessF()<minL){minL=softColors[i].lightnessF();darkIdx=i;} }
      float h,s,l; softColors[darkIdx].getHslF(&h,&s,&l);
      m_moodColorsV2A[5]=QColor::fromHslF(h, qBound(0.20f,s*0.75f,0.38f), qBound(0.32f,l-0.05f,0.50f));
      m_moodColorsV2B[5]=QColor::fromHslF(fmod(h+0.5f,1.0f), qBound(0.20f,s*0.70f,0.35f), qBound(0.35f,l+0.08f,0.45f)); }

    m_moodsComputed = true;
}

QPair<QColor, QColor> WallpaperProcessor::extractHarmonizedColors(const QImage &image, int mood)
{
    QMutexLocker lock(&m_moodMutex);
    if (!m_moodsComputed)
        computeMoodPalettes(image);
    int m = qBound(0, mood, 5);
    if (m_useV2)
        return {m_moodColorsV2A[m], m_moodColorsV2B[m]};
    return {m_moodColorsA[m], m_moodColorsB[m]};
}

QPair<QColor, QColor> WallpaperProcessor::resolveMoodColors(const QImage &image, int mood, bool useV2)
{
    QMutexLocker lock(&m_moodMutex);
    if (!image.isNull() && !m_moodsComputed)
        computeMoodPalettes(image);
    int m = qBound(0, mood, 5);
    if (useV2)
        return {m_moodColorsV2A[m], m_moodColorsV2B[m]};
    return {m_moodColorsA[m], m_moodColorsB[m]};
}

// ── Mood palette accessors (i18n context unified to "WP", H5) ────────────

QString WallpaperProcessor::moodName(int index) const
{
    static const char *names[] = {
        QT_TRANSLATE_NOOP("WP", "Auto"), QT_TRANSLATE_NOOP("WP", "Soft"),
        QT_TRANSLATE_NOOP("WP", "Vivid"), QT_TRANSLATE_NOOP("WP", "Warm"),
        QT_TRANSLATE_NOOP("WP", "Cool"), QT_TRANSLATE_NOOP("WP", "Deep")
    };
    if (index < 0 || index >= 6) return {};
    return i18n(names[index]);
}

QString WallpaperProcessor::moodColorA(int index) const
{
    QMutexLocker lock(&m_moodMutex);   // B2: getters read what workers write
    if (index < 0 || index >= 6 || !m_moodsComputed) return {};
    return m_moodColorsA[index].name();
}

QString WallpaperProcessor::moodColorB(int index) const
{
    QMutexLocker lock(&m_moodMutex);
    if (index < 0 || index >= 6 || !m_moodsComputed) return {};
    return m_moodColorsB[index].name();
}

QString WallpaperProcessor::moodColorV2A(int index) const
{
    QMutexLocker lock(&m_moodMutex);
    if (index < 0 || index >= 6 || !m_moodsComputed) return {};
    return m_moodColorsV2A[index].name();
}

QString WallpaperProcessor::moodColorV2B(int index) const
{
    QMutexLocker lock(&m_moodMutex);
    if (index < 0 || index >= 6 || !m_moodsComputed) return {};
    return m_moodColorsV2B[index].name();
}

QString WallpaperProcessor::moodNameV2(int index) const
{
    static const char *names[] = {
        QT_TRANSLATE_NOOP("WP", "Dynamic"), QT_TRANSLATE_NOOP("WP", "Tonal"),
        QT_TRANSLATE_NOOP("WP", "Vibrant"), QT_TRANSLATE_NOOP("WP", "Ember"),
        QT_TRANSLATE_NOOP("WP", "Glacier"), QT_TRANSLATE_NOOP("WP", "Shadow")
    };
    if (index < 0 || index >= 6) return {};
    return i18n(names[index]);
}

// ── Blur core (S1/S2/S3): shared float kernel, range-clamped branch-free loops, ─
// adaptive dispatch — true Gaussian for σ ≤ 50, corrected O(n) box cascade above.
// All math in float/double, single dithered quantize at the end.

// Builds a normalized ±3σ Gaussian kernel once, shared by H and V passes.
struct GaussianKernel {
    std::vector<float> k;          // normalized weights
    std::vector<double> prefix;    // prefix sums of k, size k.size()+1, for edge folding
    int radius = 0;
};

static GaussianKernel buildGaussianKernel(double sigma)
{
    int kSize = qMax(3, (int)std::ceil(3.0 * sigma));
    if ((kSize & 1) == 0) ++kSize;
    GaussianKernel gk;
    gk.radius = kSize / 2;
    double sigmaSq2 = 2.0 * sigma * sigma;
    gk.k.resize(kSize);
    double sum = 0.0;
    for (int i = 0; i < kSize; ++i) {
        int x = i - gk.radius;
        double v = std::exp(-(double)(x * x) / sigmaSq2);
        gk.k[i] = (float)v;
        sum += v;
    }
    double invSum = 1.0 / sum;
    gk.prefix.resize(kSize + 1);
    gk.prefix[0] = 0.0;
    for (int i = 0; i < kSize; ++i) {
        gk.k[i] = (float)(gk.k[i] * invSum);
        gk.prefix[i + 1] = gk.prefix[i] + gk.k[i];
    }
    return gk;
}

// Range-clamped horizontal pass. Instead of qBound per tap (branch per tap, kills
// vectorization), the valid kernel range [kStart,kEnd] is computed once per pixel
// and the out-of-range kernel mass folds into the replicated edge pixel — the same
// replication semantics as qBound, but with a branch-free interior loop that
// auto-vectorizes. Output is byte-identical to the per-tap qBound version.
static void gaussianBlurH(float *dst, const float *src, int w, int h, const GaussianKernel &gk)
{
    const int kSize = (int)gk.k.size();
    const int radius = gk.radius;
    const float *k = gk.k.data();
    const double *pref = gk.prefix.data();
    for (int y = 0; y < h; ++y) {
        const float *sRow = src + (qsizetype)y * w * 4;
        float *dRow = dst + (qsizetype)y * w * 4;
        for (int x = 0; x < w; ++x) {
            int kStart = qMax(0, radius - x);
            int kEnd = qMin(kSize - 1, radius + (w - 1 - x));
            double c[4] = {0};
            // fold out-of-range kernel mass into replicated edge pixels
            double leftMass = pref[kStart];
            if (leftMass > 0.0) {
                const float *sp = sRow; // pixel 0
                c[0] += sp[0]*leftMass; c[1] += sp[1]*leftMass;
                c[2] += sp[2]*leftMass; c[3] += sp[3]*leftMass;
            }
            double rightMass = pref[kSize] - pref[kEnd + 1];
            if (rightMass > 0.0) {
                const float *sp = sRow + (w - 1) * 4;
                c[0] += sp[0]*rightMass; c[1] += sp[1]*rightMass;
                c[2] += sp[2]*rightMass; c[3] += sp[3]*rightMass;
            }
            const float *sp = sRow + (x + kStart - radius) * 4;
            for (int ki = kStart; ki <= kEnd; ++ki, sp += 4) {
                double kw = k[ki];
                c[0]+=sp[0]*kw; c[1]+=sp[1]*kw; c[2]+=sp[2]*kw; c[3]+=sp[3]*kw;
            }
            float *dp = dRow + x*4;
            dp[0]=(float)c[0]; dp[1]=(float)c[1]; dp[2]=(float)c[2]; dp[3]=(float)c[3];
        }
    }
}

// Range-clamped vertical pass, same edge-folding scheme, stride = w*4.
static void gaussianBlurV(float *dst, const float *src, int w, int h, const GaussianKernel &gk)
{
    const int kSize = (int)gk.k.size();
    const int radius = gk.radius;
    const float *k = gk.k.data();
    const double *pref = gk.prefix.data();
    const qsizetype stride = (qsizetype)w * 4;
    for (int x = 0; x < w; ++x) {
        const float *sBase = src + x * 4;
        float *dBase = dst + x * 4;
        for (int y = 0; y < h; ++y) {
            int kStart = qMax(0, radius - y);
            int kEnd = qMin(kSize - 1, radius + (h - 1 - y));
            double c[4] = {0};
            double leftMass = pref[kStart];
            if (leftMass > 0.0) {
                const float *sp = sBase;
                c[0] += sp[0]*leftMass; c[1] += sp[1]*leftMass;
                c[2] += sp[2]*leftMass; c[3] += sp[3]*leftMass;
            }
            double rightMass = pref[kSize] - pref[kEnd + 1];
            if (rightMass > 0.0) {
                const float *sp = sBase + (h - 1) * stride;
                c[0] += sp[0]*rightMass; c[1] += sp[1]*rightMass;
                c[2] += sp[2]*rightMass; c[3] += sp[3]*rightMass;
            }
            const float *sp = sBase + (y + kStart - radius) * stride;
            for (int ki = kStart; ki <= kEnd; ++ki, sp += stride) {
                double kw = k[ki];
                c[0]+=sp[0]*kw; c[1]+=sp[1]*kw; c[2]+=sp[2]*kw; c[3]+=sp[3]*kw;
            }
            float *dp = dBase + y*stride;
            dp[0]=(float)c[0]; dp[1]=(float)c[1]; dp[2]=(float)c[2]; dp[3]=(float)c[3];
        }
    }
}

// Corrected box radii (Jarosz/Kutskir). Outputs box *widths* (odd); caller uses
// radius = (width-1)/2. At N passes the cascade matches σ to ~0.04% avg error.
static void sigmaToBoxesN(int *boxes, int n, double sigma)
{
    double wi = std::sqrt((12.0 * sigma * sigma / n) + 1.0);
    int wl = (int)std::floor(wi);
    if ((wl & 1) == 0) --wl;
    int wu = wl + 2;
    double mi = (12.0 * sigma * sigma - n * wl * wl - 4.0 * n * wl - 3.0 * n) / (-4.0 * wl - 4.0);
    int m = (int)std::round(mi);
    for (int i = 0; i < n; ++i) boxes[i] = (i < m) ? wl : wu;
}

// O(n) horizontal box blur — sliding window, double accumulators, padded buffer.
// inValid = valid region of src; outValid = valid region after this pass.
// Writes rows [inValid, h-inValid) so the following V pass can read its window,
// but only cols [outValid, w-outValid) — the region the cascade still needs.
static void boxBlurHFloat(float *dst, const float *src, int w, int h, int r,
                          int inValid, int outValid)
{
    const int div = 2 * r + 1;
    const int xEnd = w - outValid;
    const int yEnd = h - inValid;
    for (int y = inValid; y < yEnd; ++y) {
        const float *s = src + (qsizetype)y * w * 4;
        float *d = dst + (qsizetype)y * w * 4;
        double acc[4] = {0};
        // window at x = outValid covers [outValid-r, outValid+r] = [inValid, ...)
        for (int x = outValid - r; x <= outValid + r; ++x) {
            const float *p = s + x * 4;
            acc[0] += p[0]; acc[1] += p[1]; acc[2] += p[2]; acc[3] += p[3];
        }
        // write + slide for all but the last column; the final column is written
        // without a trailing slide (the next window would read s[xEnd + r],
        // one past the previous pass's written region — discarded but OOB).
        for (int x = outValid; x < xEnd - 1; ++x) {
            float *dp = d + x * 4;
            dp[0] = (float)(acc[0] / div); dp[1] = (float)(acc[1] / div);
            dp[2] = (float)(acc[2] / div); dp[3] = (float)(acc[3] / div);
            // slide: remove s[x-r], add s[x+r+1]
            const float *op = s + (x - r) * 4;
            const float *ip = s + (x + r + 1) * 4;
            acc[0] += ip[0] - op[0]; acc[1] += ip[1] - op[1];
            acc[2] += ip[2] - op[2]; acc[3] += ip[3] - op[3];
        }
        if (xEnd > outValid) {
            float *dp = d + (xEnd - 1) * 4;
            dp[0] = (float)(acc[0] / div); dp[1] = (float)(acc[1] / div);
            dp[2] = (float)(acc[2] / div); dp[3] = (float)(acc[3] / div);
        }
    }
}

// O(n) vertical box blur — same padded-buffer scheme, stride = w*4.
static void boxBlurVFloat(float *dst, const float *src, int w, int h, int r,
                          int inValid, int outValid)
{
    const int div = 2 * r + 1;
    const qsizetype stride = (qsizetype)w * 4;
    const int xEnd = w - outValid;
    const int yEnd = h - outValid;
    for (int x = outValid; x < xEnd; ++x) {
        const float *s = src + x * 4;
        float *d = dst + x * 4;
        double acc[4] = {0};
        // window at y = outValid covers [outValid-r, outValid+r] = [inValid, ...)
        for (int y = outValid - r; y <= outValid + r; ++y) {
            const float *p = s + y * stride;
            acc[0] += p[0]; acc[1] += p[1]; acc[2] += p[2]; acc[3] += p[3];
        }
        // write + slide for all but the last row; final row written without
        // trailing slide (avoids the OOB read at s[yEnd + r]).
        for (int y = outValid; y < yEnd - 1; ++y) {
            float *dp = d + y * stride;
            dp[0] = (float)(acc[0] / div); dp[1] = (float)(acc[1] / div);
            dp[2] = (float)(acc[2] / div); dp[3] = (float)(acc[3] / div);
            const float *op = s + (y - r) * stride;
            const float *ip = s + (y + r + 1) * stride;
            acc[0] += ip[0] - op[0]; acc[1] += ip[1] - op[1];
            acc[2] += ip[2] - op[2]; acc[3] += ip[3] - op[3];
        }
        if (yEnd > outValid) {
            float *dp = d + (yEnd - 1) * stride;
            dp[0] = (float)(acc[0] / div); dp[1] = (float)(acc[1] / div);
            dp[2] = (float)(acc[2] / div); dp[3] = (float)(acc[3] / div);
        }
    }
}

static void imageToFloat(const QImage &img, float *buf)
{
    int w = img.width(), h = img.height();
    int bpl = img.bytesPerLine();
    for (int y = 0; y < h; ++y) {
        const uchar *row = img.constBits() + y * bpl;
        float *fRow = buf + y * w * 4;
        for (int x = 0; x < w; ++x) {
            fRow[x*4]=(float)row[x*4]; fRow[x*4+1]=(float)row[x*4+1];
            fRow[x*4+2]=(float)row[x*4+2]; fRow[x*4+3]=(float)row[x*4+3];
        }
    }
}

static void floatToImageDithered(const float *buf, QImage &img)
{
    static const uchar bayer[8][8] = {
        {  0, 48, 12, 60,  3, 51, 15, 63 }, { 32, 16, 44, 28, 35, 19, 47, 31 },
        {  8, 56,  4, 52, 11, 59,  7, 55 }, { 40, 24, 36, 20, 43, 27, 39, 23 },
        {  2, 50, 14, 62,  1, 49, 13, 61 }, { 34, 18, 46, 30, 33, 17, 45, 29 },
        { 10, 58,  6, 54,  9, 57,  5, 53 }, { 42, 26, 38, 22, 41, 25, 37, 21 }
    };
    int w = img.width(), h = img.height();
    int bpl = img.bytesPerLine();
    const float inv64 = 1.0f / 64.0f;
    for (int y = 0; y < h; ++y) {
        uchar *row = img.bits() + y * bpl;
        const float *fRow = buf + y * w * 4;
        const uchar *bRow = bayer[y & 7];
        for (int x = 0; x < w; ++x) {
            const float *fp = fRow + x * 4;
            float t = (float)bRow[x & 7] * inv64;
            for (int c = 0; c < 4; ++c) {
                float v = qBound(0.0f, fp[c], 255.0f);
                row[x*4 + c] = (uchar)qMin((int)(v + t), 255);
            }
        }
    }
}

static void boostSaturationFloat(float *buf, int nPix, double factor)
{
    float f = (float)factor;
    // BT.601 luma weights: perceptual desaturation. Arithmetic mean shifts hue
    // brightness asymmetrically (saturated red: mean 85 vs luma 76).
    for (int i = 0; i < nPix; ++i) {
        float *px = buf + i * 4;
        float b = px[0], g = px[1], r = px[2];
        float gray = 0.299f * r + 0.587f * g + 0.114f * b;
        px[0] = qBound(0.0f, gray + (b - gray) * f, 255.0f);
        px[1] = qBound(0.0f, gray + (g - gray) * f, 255.0f);
        px[2] = qBound(0.0f, gray + (r - gray) * f, 255.0f);
    }
}

// ── Float color grade (Phase 1) ─────────────────────────────────────────
// Shared by the background (stackBlur) and the foreground photo (gradedCopy).
// Operates on the float buffer BEFORE any 8-bit quantization; neutral params
// hit the fast path (zero cost). Buffer is B,G,R,A (QImage ARGB32 byte
// order): c==0 is BLUE, c==2 is RED. Warm = red up, blue down.
static void applyColorGradeFloat(float *buf, int nPix,
                                 double gamma, double warmth, double blackLift)
{
    const bool doGamma = std::abs(gamma - 1.0) > 0.001;
    const bool doWarm  = std::abs(warmth) > 0.001;
    const bool doLift  = blackLift > 0.001;
    if (!(doGamma || doWarm || doLift)) return;
    std::vector<float> glut;
    if (doGamma) {
        glut.resize(4096);
        for (int i = 0; i < 4096; ++i)
            glut[i] = 255.0f * std::pow((float)i / 4095.0f, (float)gamma);
    }
    const float wr = 1.0f + 0.15f * (float)warmth;
    const float wb = 1.0f - 0.15f * (float)warmth;
    const float lift = 255.0f * (float)blackLift;
    for (int i = 0; i < nPix; ++i) {
        float *px = buf + i * 4;
        for (int c = 0; c < 3; ++c) {
            float v = px[c];
            if (doGamma) v = glut[qBound(0, (int)(v / 255.0f * 4095.0f), 4095)];
            if (doWarm)  v *= (c == 0) ? wb : (c == 2) ? wr : 1.0f;
            if (doLift && v < lift) v = lift;
            px[c] = qBound(0.0f, v, 255.0f);
        }
    }
}

// Foreground photo grade (retro looks): sat (0 = B&W) + gamma/warmth/blackLift
// through the same float pipeline as the background, single dither at the end.
// Neutral fast path returns an implicit share — zero cost when no look is on.
static QImage gradedCopy(const QImage &src, double sat, double gamma, double warmth, double blackLift)
{
    if (std::abs(sat - 1.0) < 0.001 && std::abs(gamma - 1.0) < 0.001
        && std::abs(warmth) < 0.001 && blackLift < 0.001)
        return src;
    QImage work = src.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    std::vector<float> buf(size_t(work.width()) * work.height() * 4);
    imageToFloat(work, buf.data());
    boostSaturationFloat(buf.data(), work.width() * work.height(), sat);
    applyColorGradeFloat(buf.data(), work.width() * work.height(), gamma, warmth, blackLift);
    floatToImageDithered(buf.data(), work);
    return work;
}

void WallpaperProcessor::stackBlur(QImage &image, double sigma, double saturationFactor,
                                    double overlayOpacity, QRgb overlayColor, double brightness,
                                    double colorGamma, double colorWarmth, double colorBlackLift)
{
    if (sigma < 0.5 || image.isNull()) return;
    if (image.format() != QImage::Format_ARGB32_Premultiplied)
        image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    int w = image.width(), h = image.height();
    if (w < 1 || h < 1) return;
    int nPix = w * h;
    std::vector<float> buf1(nPix * 4);
    std::vector<float> buf2(nPix * 4);
    imageToFloat(image, buf1.data());

    // S1: adaptive dispatch. True Gaussian is exact but O(n·σ) — fast enough up to
    // σ=50 (kernel ≤301 taps). Above that, the corrected box cascade is O(n) with
    // ~0.04% error vs the Gaussian (radii chosen so N passes match σ, not a naive
    // 3-pass box — that is what banded on 2026-07-30).
    if (sigma <= 50.0) {
        GaussianKernel gk = buildGaussianKernel(sigma);
        gaussianBlurH(buf2.data(), buf1.data(), w, h, gk);
        gaussianBlurV(buf1.data(), buf2.data(), w, h, gk);
    } else {
        const int passes = (sigma < 100.0) ? 6 : 12;
        int widths[12];
        sigmaToBoxesN(widths, passes, sigma);
        int radii[12];
        int pad = 0;
        for (int p = 0; p < passes; ++p) { radii[p] = (widths[p] - 1) / 2; pad += radii[p]; }
        const int W = w + 2 * pad, H = h + 2 * pad;
        // Pad once with replicated edges (Gaussian-equivalent boundary condition),
        // then run all passes as pure valid convolution with a shrinking window.
        // This keeps the cascade equivalent to a single convolution of the box
        // kernel against the replicated input — interior AND border match the
        // true Gaussian (the per-pass-clamped version deviated ~13 levels at the
        // border because it re-replicated at every stage, widening the support).
        std::vector<float> pb1((qsizetype)W * H * 4), pb2((qsizetype)W * H * 4);
        for (int y = 0; y < H; ++y) {
            const int sy = qBound(0, y - pad, h - 1);
            float *dstRow = pb1.data() + (qsizetype)y * W * 4;
            const float *srcRow = buf1.data() + (qsizetype)sy * w * 4;
            const float *edgeL = srcRow;
            const float *edgeR = srcRow + (w - 1) * 4;
            for (int x = 0; x < pad; ++x) { std::memcpy(dstRow + x*4, edgeL, 4*sizeof(float)); }
            std::memcpy(dstRow + pad*4, srcRow, (qsizetype)w * 4 * sizeof(float));
            for (int x = 0; x < pad; ++x) { std::memcpy(dstRow + (pad + w + x)*4, edgeR, 4*sizeof(float)); }
        }
        int valid = 0;
        for (int p = 0; p < passes; ++p) {
            int inValid = valid;
            valid += radii[p];
            boxBlurHFloat(pb2.data(), pb1.data(), W, H, radii[p], inValid, valid);
            boxBlurVFloat(pb1.data(), pb2.data(), W, H, radii[p], inValid, valid);
        }
        // crop the valid region [pad, pad+w) × [pad, pad+h) back into buf1
        for (int y = 0; y < h; ++y)
            std::memcpy(buf1.data() + (qsizetype)y * w * 4,
                        pb1.data() + ((qsizetype)(y + pad) * W + pad) * 4,
                        (qsizetype)w * 4 * sizeof(float));
    }

    if (saturationFactor >= 0.0 && std::abs(saturationFactor - 1.0) > 0.001)
        boostSaturationFloat(buf1.data(), nPix, saturationFactor);

    if (overlayOpacity > 0.001f) {
        float fr=(float)((overlayColor>>16)&0xFF), fg=(float)((overlayColor>>8)&0xFF), fb=(float)(overlayColor&0xFF);
        float a=qBound(0.0, overlayOpacity, 1.0), ia=1.0f-a;
        for (int i=0;i<nPix;++i){ float *px=buf1.data()+i*4;
            px[0]=px[0]*ia+fb*a; px[1]=px[1]*ia+fg*a; px[2]=px[2]*ia+fr*a;
            // P2: premultiplied invariant — blended RGB must not exceed alpha.
            float A = px[3];
            px[0]=qMin(px[0], A); px[1]=qMin(px[1], A); px[2]=qMin(px[2], A);
        }
    }
    if (std::abs(brightness - 1.0) > 0.001) {
        float b=(float)brightness;
        for (int i=0;i<nPix;++i){ float *px=buf1.data()+i*4;
            px[0]=qMin(px[0]*b,255.0f); px[1]=qMin(px[1]*b,255.0f); px[2]=qMin(px[2]*b,255.0f); }
    }

    // ── Float color grade (Phase 1): gamma -> warmth -> blackLift -> clamp.
    // Lives in the float buffer BEFORE the single 8-bit dither (precision
    // invariant). Shared helper with the foreground photo (gradedCopy); the
    // neutral fast path inside it costs nothing for non-look renders.
    applyColorGradeFloat(buf1.data(), nPix, colorGamma, colorWarmth, colorBlackLift);
    floatToImageDithered(buf1.data(), image);
}


// ── Blur preset methods ──────────────────────────────────────────────────

void WallpaperProcessor::setBlurPresetIndex(int index)
{
    if (index < 0 || index >= blurPresetCount()) index = 0;
    if (index == static_cast<int>(BlurPresetId::Auto)) { computeSmartAutoAndApply(); return; }
    const auto &cfg = ::blurPresetConfig(index);
    if (cfg.isLocked()) { resetBlurToDefault(); return; }
    m_blurPresetId = QString::fromUtf8(cfg.id);
    m_blurPresetIndex = index;
    m_blurRadius   = qMax(0, (int)cfg.sigma);
    m_saturationFactor = cfg.satBoost;
    m_colorGamma     = cfg.gamma;
    m_colorWarmth    = cfg.warmth;
    m_colorBlackLift = cfg.blackLift;
    m_overlayOpacity   = cfg.overlayOpacity;
    m_overlayColor     = QColor::fromRgb(cfg.overlayColor);
    m_blurBrightness   = cfg.brightness;
    m_vignetteStrength = cfg.vignette;
    m_grainStrength    = cfg.grain;
    // Frame is owned by the preset: Reddit forces the built-in frame ON at its
    // width; every other preset means frame OFF (so no frame leaks between presets).
    m_photoFrame      = cfg.frameEnabled;
    m_photoFrameWidth = cfg.frameEnabled ? cfg.frameWidthPct : 0;
    // Holistic look fields: photoGrade rows (Kodachrome/Polaroid/Vintage/Tri-X/
    // Cool Film) own the texture state — procedural, no user assets. Ordinary
    // rows keep the user's current texture untouched.
    m_textureKind      = cfg.photoGrade ? cfg.textureKind : 0;
    m_textureColor     = cfg.photoGrade ? cfg.textureColor : 0;
    m_textureOverPhoto = cfg.photoGrade ? cfg.textureOverPhoto : false;
    m_textureOpacity   = cfg.photoGrade ? cfg.textureOpacity : 0.0;
    m_textureBlendMode = cfg.photoGrade ? qBound(0, cfg.textureBlendMode, 28) : WalltzDefaults::textureBlendMode;
    m_photoGrade       = cfg.photoGrade;
    Q_EMIT blurPresetIdChanged();
    Q_EMIT renderParamsChanged();
}

void WallpaperProcessor::resetBlurToDefault()
{
    m_blurPresetId = QStringLiteral("default");
    m_blurPresetIndex = 0;
    m_blurRadius   = WalltzDefaults::blurRadius;
    m_saturationFactor = WalltzDefaults::saturationFactor;
    m_colorGamma     = WalltzDefaults::colorGamma;
    m_colorWarmth    = WalltzDefaults::colorWarmth;
    m_colorBlackLift = WalltzDefaults::colorBlackLift;
    m_textureKind      = 0;
    m_textureColor     = 0;
    m_textureOpacity   = 0.0;
    m_textureBlendMode = WalltzDefaults::textureBlendMode;
    m_textureOverPhoto = false;
    m_photoGrade       = false;
    m_overlayOpacity   = WalltzDefaults::overlayOpacity;
    m_overlayColor     = Qt::black;
    m_blurBrightness   = WalltzDefaults::blurBrightness;
    m_vignetteStrength = WalltzDefaults::vignetteStrength;
    m_grainStrength    = WalltzDefaults::grainStrength;
    m_photoFrame   = false;    // Default = no frame at all (factory baseline)
    m_photoFrameWidth = 0;
    Q_EMIT blurPresetIdChanged();
    Q_EMIT renderParamsChanged();
}

void WallpaperProcessor::computeSmartAutoAndApply()
{
    if (!m_statsValid)
        m_imageStats = ImageStats{};
    m_blurPresetId = QStringLiteral("auto");
    m_blurPresetIndex = static_cast<int>(BlurPresetId::Auto);
    SmartAutoParams p = computeSmartAuto(m_imageStats);
    m_blurRadius       = qMax(0, (int)p.sigma);
    m_saturationFactor = p.satBoost;
    m_blurBrightness   = p.brightness;
    m_overlayOpacity   = p.overlayOpacity;
    m_overlayColor     = QColor::fromRgb(p.overlayColor);
    m_vignetteStrength = p.vignette;
    m_grainStrength    = p.grain;
    // Auto computes blur params only — frame must be OFF (no preset frame spec).
    m_photoFrame   = false;
    m_photoFrameWidth = 0;
    Q_EMIT blurPresetIdChanged();
    Q_EMIT renderParamsChanged();
}

QStringList WallpaperProcessor::blurPresetNames() const
{
    // H3: preset table is static — build the name list once.
    static const QStringList names = [] {
        QStringList n;
        n.reserve(blurPresetCount());
        for (int i = 0; i < blurPresetCount(); ++i)
            n.append(blurPresetName(i));
        return n;
    }();
    return names;
}

// ── gradient preset accessors ─────────────────────────────────────────────
int WallpaperProcessor::gradientPresetCount() const { return 12; }

QString WallpaperProcessor::gradientPresetName(int index) const
{
    if (index < 0 || index >= 12) return {};
    return i18n(s_presets[index].name);
}
QString WallpaperProcessor::gradientPresetColor1(int index) const
{
    if (index < 0 || index >= 12) return {};
    return QColor(s_presets[index].color1).name();
}
QString WallpaperProcessor::gradientPresetColor2(int index) const
{
    if (index < 0 || index >= 12) return {};
    return QColor(s_presets[index].color2).name();
}
double WallpaperProcessor::aspectRatioForMode(int mode) const
{
    if (mode < 0 || mode > 6) return 0.0;
    return s_aspectRatios[mode];
}

// ── Pattern QML accessors ────────────────────────────────────────────────

int WallpaperProcessor::geometricPatternCount() const { return GEOMETRIC_PATTERN_COUNT; }
QString WallpaperProcessor::geometricPatternName(int index) const
{
    if (index < 0 || index >= GEOMETRIC_PATTERN_COUNT) return {};
    return i18n(s_geometricNames[index]);
}
int WallpaperProcessor::motifPatternCount() const { return MOTIF_PATTERN_COUNT; }
QString WallpaperProcessor::motifPatternName(int index) const
{
    if (index < 0 || index >= MOTIF_PATTERN_COUNT) return {};
    return i18n(s_motifNames[index]);
}
int WallpaperProcessor::motifPatternCategory(int index) const
{
    if (index < 0 || index >= MOTIF_PATTERN_COUNT) return 0;
    return s_motifCategories[index];
}
QString WallpaperProcessor::motifCategoryName(int cat) const
{
    if (cat < 0 || cat >= 6) return {};
    return i18n(s_categoryNames[cat]);
}
QString WallpaperProcessor::svgGeoPatternName(int index) const
{
    if (index < 0 || index >= SVG_GEO_COUNT) return {};
    return i18n(s_svgGeoNames[index]);
}

QVariantList WallpaperProcessor::bgPatternMixMotifs() const
{
    QVariantList list;
    for (int i : m_bgPatternMixMotifs) list.append(i);
    return list;
}
void WallpaperProcessor::setBgPatternMixMotifs(const QVariantList &indices)
{
    m_bgPatternMixMotifs.clear();
    for (const auto &v : indices) m_bgPatternMixMotifs.append(v.toInt());
    Q_EMIT bgPatternMixMotifsChanged();
    Q_EMIT renderParamsChanged();
}
void WallpaperProcessor::toggleMixMotif(int index)
{
    if (m_bgPatternMixMotifs.contains(index)) m_bgPatternMixMotifs.removeAll(index);
    else                                      m_bgPatternMixMotifs.append(index);
    Q_EMIT bgPatternMixMotifsChanged();
    Q_EMIT renderParamsChanged();
}

// ── Unified pattern thumbnails (Q4) ──────────────────────────────────────
// One helper drives all three caches; filename embeds a schema version so
// stale thumbs from older builds are never served.

static QString walltzThumbFor(QHash<int, QString> &cache, const QString &prefix,
                              int index, int thumbSize,
                              const std::function<QImage(int, int)> &gen)
{
    auto it = cache.find(index);
    if (it != cache.end()) return it.value();
    QImage thumb = gen(index, thumbSize > 0 ? thumbSize : 60);
    if (thumb.isNull()) return {};
    static const QString tmpDir = [] {
        QString d = QDir::tempPath() + QStringLiteral("/walltz");
        QDir().mkpath(d);
        return d;
    }();
    // v2 schema bump: new rendering core + const tile generators.
    QString path = tmpDir + QStringLiteral("/%1_v2_%2.png").arg(prefix).arg(index);
    thumb.save(path, "PNG");
    QString url = QStringLiteral("file://") + path;
    cache.insert(index, url);
    return url;
}

QString WallpaperProcessor::geometricPatternThumbnail(int index, int thumbSize)
{
    if (index < 0 || index >= GEOMETRIC_PATTERN_COUNT) return {};
    return walltzThumbFor(m_geometricThumbnailCache, QStringLiteral("gth"), index, thumbSize,
        [this](int i, int s) { return generateGeometricTile(i, s, QColor(240,240,240), QColor(80,80,80), 1.0); });
}

QString WallpaperProcessor::svgGeoPatternThumbnail(int index, int thumbSize)
{
    if (index < 0 || index >= SVG_GEO_COUNT) return {};
    return walltzThumbFor(m_svgGeoThumbnailCache, QStringLiteral("sgth"), index, thumbSize,
        [this](int i, int s) { return generateSvgGeoTile(i, s, QColor(240,240,240), QColor(80,80,80), 1.0); });
}

QString WallpaperProcessor::motifPatternThumbnail(int index, int thumbSize)
{
    if (index < 0 || index >= MOTIF_PATTERN_COUNT) return {};
    return walltzThumbFor(m_motifThumbnailCache, QStringLiteral("mth"), index, thumbSize,
        [this](int i, int s) { return generateMotifTile(i, s, Qt::transparent, QColor(80,80,80), 1.0); });
}

// ── Overlay catalog (Phase 2: user assets, AppDataLocation/overlays) ────

// ── F2: named-parameter presets (QSettings-backed) ─────────────────────
// serializeParams/deserializeParams are the single source for both the
// persisted presets and the in-memory undo snapshot (F6) — one map, two
// uses, no drift.

static const char *kParamPresetGroup = "paramPresets";

QVariantMap WallpaperProcessor::serializeParams() const
{
    QVariantMap m;
    m.insert(QStringLiteral("blurMode"), m_blurMode);
    m.insert(QStringLiteral("bgGradientStyle"), m_bgGradientStyle);
    m.insert(QStringLiteral("bgGradientPreset"), m_bgGradientPreset);
    m.insert(QStringLiteral("gradientAngle"), m_gradientAngle);
    m.insert(QStringLiteral("blurRadius"), m_blurRadius);
    m.insert(QStringLiteral("saturationFactor"), m_saturationFactor);
    m.insert(QStringLiteral("colorGamma"), m_colorGamma);
    m.insert(QStringLiteral("colorWarmth"), m_colorWarmth);
    m.insert(QStringLiteral("colorBlackLift"), m_colorBlackLift);
    m.insert(QStringLiteral("textureKind"), m_textureKind);
    m.insert(QStringLiteral("textureColor"), int(m_textureColor));
    m.insert(QStringLiteral("textureOpacity"), m_textureOpacity);
    m.insert(QStringLiteral("textureBlendMode"), m_textureBlendMode);
    m.insert(QStringLiteral("textureOverPhoto"), m_textureOverPhoto);
    m.insert(QStringLiteral("photoGrade"), m_photoGrade);
    m.insert(QStringLiteral("overlayOpacity"), m_overlayOpacity);   // tint overlay (pre-existing)
    m.insert(QStringLiteral("overlayColor"), m_overlayColor.name());
    m.insert(QStringLiteral("blurBrightness"), m_blurBrightness);
    m.insert(QStringLiteral("bgZoom"), m_bgZoom);
    m.insert(QStringLiteral("bgBlurAngle"), m_bgBlurAngle);
    m.insert(QStringLiteral("autoMood"), m_autoMood);
    m.insert(QStringLiteral("useV2"), m_useV2);
    m.insert(QStringLiteral("vignetteStrength"), m_vignetteStrength);
    m.insert(QStringLiteral("grainStrength"), m_grainStrength);
    m.insert(QStringLiteral("caStrength"), m_caStrength);
    m.insert(QStringLiteral("photoFrame"), m_photoFrame);
    m.insert(QStringLiteral("photoFrameWidth"), m_photoFrameWidth);
    m.insert(QStringLiteral("fgZoom"), m_fgZoom);
    m.insert(QStringLiteral("pipZoom"), m_pipZoom);
    m.insert(QStringLiteral("bgColor"), m_bgColor.name());
    m.insert(QStringLiteral("autoColor"), m_autoColor);
    // ── Pattern params (B3: presets/undo were silently dropping these) ──
    m.insert(QStringLiteral("bgPatternEnabled"), m_bgPatternEnabled);
    m.insert(QStringLiteral("bgPatternType"), m_bgPatternType);
    m.insert(QStringLiteral("bgPatternColor"), m_bgPatternColor.name());
    m.insert(QStringLiteral("bgPatternScale"), m_bgPatternScale);
    m.insert(QStringLiteral("bgPatternRotation"), m_bgPatternRotation);
    m.insert(QStringLiteral("bgPatternSpacing"), m_bgPatternSpacing);
    m.insert(QStringLiteral("bgPatternRandomRotate"), m_bgPatternRandomRotate);
    m.insert(QStringLiteral("bgPatternJitter"), m_bgPatternJitter);
    m.insert(QStringLiteral("bgPatternGridAmplitude"), m_bgPatternGridAmplitude);
    m.insert(QStringLiteral("bgPatternMixEnabled"), m_bgPatternMixEnabled);
    QVariantList mix;
    for (int i : m_bgPatternMixMotifs) mix.append(i);
    m.insert(QStringLiteral("bgPatternMixMotifs"), mix);
    return m;
}

void WallpaperProcessor::deserializeParams(const QVariantMap &m)
{
    m_blurMode         = m.value(QStringLiteral("blurMode"), m_blurMode).toBool();
    m_bgGradientStyle  = m.value(QStringLiteral("bgGradientStyle"), m_bgGradientStyle).toInt();
    m_bgGradientPreset = m.value(QStringLiteral("bgGradientPreset"), m_bgGradientPreset).toInt();
    m_gradientAngle    = m.value(QStringLiteral("gradientAngle"), m_gradientAngle).toDouble();
    m_blurRadius       = m.value(QStringLiteral("blurRadius"), m_blurRadius).toInt();
    m_saturationFactor = m.value(QStringLiteral("saturationFactor"), m_saturationFactor).toDouble();
    m_colorGamma       = m.value(QStringLiteral("colorGamma"), m_colorGamma).toDouble();
    m_colorWarmth      = m.value(QStringLiteral("colorWarmth"), m_colorWarmth).toDouble();
    m_colorBlackLift   = m.value(QStringLiteral("colorBlackLift"), m_colorBlackLift).toDouble();
    m_textureKind      = m.value(QStringLiteral("textureKind"), m_textureKind).toInt();
    m_textureColor     = QRgb(m.value(QStringLiteral("textureColor"), int(m_textureColor)).toUInt());
    m_textureOpacity   = m.value(QStringLiteral("textureOpacity"), m_textureOpacity).toDouble();
    m_textureBlendMode = qBound(0, m.value(QStringLiteral("textureBlendMode"), m_textureBlendMode).toInt(), 28);
    m_textureOverPhoto = m.value(QStringLiteral("textureOverPhoto"), m_textureOverPhoto).toBool();
    m_photoGrade       = m.value(QStringLiteral("photoGrade"), m_photoGrade).toBool();
    m_overlayOpacity   = m.value(QStringLiteral("overlayOpacity"), m_overlayOpacity).toDouble();
    m_overlayColor     = QColor(m.value(QStringLiteral("overlayColor"), m_overlayColor.name()).toString());
    m_blurBrightness   = m.value(QStringLiteral("blurBrightness"), m_blurBrightness).toDouble();
    m_bgZoom           = m.value(QStringLiteral("bgZoom"), m_bgZoom).toDouble();
    m_bgBlurAngle      = m.value(QStringLiteral("bgBlurAngle"), m_bgBlurAngle).toDouble();
    m_autoMood         = m.value(QStringLiteral("autoMood"), m_autoMood).toInt();
    m_useV2            = m.value(QStringLiteral("useV2"), m_useV2).toBool();
    m_vignetteStrength = m.value(QStringLiteral("vignetteStrength"), m_vignetteStrength).toDouble();
    m_grainStrength    = m.value(QStringLiteral("grainStrength"), m_grainStrength).toDouble();
    m_caStrength       = m.value(QStringLiteral("caStrength"), m_caStrength).toDouble();
    m_photoFrame       = m.value(QStringLiteral("photoFrame"), m_photoFrame).toBool();
    m_photoFrameWidth  = m.value(QStringLiteral("photoFrameWidth"), m_photoFrameWidth).toInt();
    m_fgZoom           = m.value(QStringLiteral("fgZoom"), m_fgZoom).toDouble();
    m_pipZoom          = m.value(QStringLiteral("pipZoom"), m_pipZoom).toDouble();
    m_bgColor          = QColor(m.value(QStringLiteral("bgColor"), m_bgColor.name()).toString());
    m_autoColor        = m.value(QStringLiteral("autoColor"), m_autoColor).toBool();
    // ── Pattern params (B3) ──
    m_bgPatternEnabled    = m.value(QStringLiteral("bgPatternEnabled"), m_bgPatternEnabled).toBool();
    m_bgPatternType       = m.value(QStringLiteral("bgPatternType"), m_bgPatternType).toInt();
    m_bgPatternColor      = QColor(m.value(QStringLiteral("bgPatternColor"), m_bgPatternColor.name()).toString());
    m_bgPatternScale      = m.value(QStringLiteral("bgPatternScale"), m_bgPatternScale).toDouble();
    m_bgPatternRotation   = m.value(QStringLiteral("bgPatternRotation"), m_bgPatternRotation).toDouble();
    m_bgPatternSpacing    = m.value(QStringLiteral("bgPatternSpacing"), m_bgPatternSpacing).toDouble();
    m_bgPatternRandomRotate = m.value(QStringLiteral("bgPatternRandomRotate"), m_bgPatternRandomRotate).toBool();
    m_bgPatternJitter     = m.value(QStringLiteral("bgPatternJitter"), m_bgPatternJitter).toBool();
    m_bgPatternGridAmplitude = m.value(QStringLiteral("bgPatternGridAmplitude"), m_bgPatternGridAmplitude).toDouble();
    m_bgPatternMixEnabled = m.value(QStringLiteral("bgPatternMixEnabled"), m_bgPatternMixEnabled).toBool();
    m_bgPatternMixMotifs.clear();
    const QVariantList mix = m.value(QStringLiteral("bgPatternMixMotifs")).toList();
    for (const QVariant &v : mix) m_bgPatternMixMotifs.append(v.toInt());
    Q_EMIT renderParamsChanged();
}

void WallpaperProcessor::rememberState()
{
    m_undoSnapshot = serializeParams();
}

void WallpaperProcessor::restoreState()
{
    if (!m_undoSnapshot.isEmpty())
    deserializeParams(m_undoSnapshot);
}

QStringList WallpaperProcessor::paramPresetNames() const
{
    QSettings s;
    s.beginGroup(QLatin1String(kParamPresetGroup));
    QStringList keys = s.childGroups();
    s.endGroup();
    return keys;
}

void WallpaperProcessor::saveParamPreset(const QString &name)
{
    if (name.isEmpty()) return;
    QSettings s;
    s.beginGroup(QLatin1String(kParamPresetGroup));
    s.beginGroup(name);
    const QVariantMap m = serializeParams();
    for (auto it = m.constBegin(); it != m.constEnd(); ++it)
    s.setValue(it.key(), it.value());
    s.endGroup();
    s.endGroup();
    Q_EMIT paramPresetsChanged();
}

void WallpaperProcessor::applyParamPreset(const QString &name)
{
    QSettings s;
    s.beginGroup(QLatin1String(kParamPresetGroup));
    if (!s.childGroups().contains(name)) { s.endGroup(); return; }
    s.beginGroup(name);
    QVariantMap m;
    for (const QString &key : s.allKeys())
    m.insert(key, s.value(key));
    s.endGroup();
    s.endGroup();
    deserializeParams(m);
}

void WallpaperProcessor::deleteParamPreset(const QString &name)
{
    QSettings s;
    s.beginGroup(QLatin1String(kParamPresetGroup));
    s.remove(name);
    s.endGroup();
    Q_EMIT paramPresetsChanged();
}

// ── F1: set-as-wallpaper via XDG Desktop Portal (shell-agnostic) ─────────
// Save is the contract: the render always exists. Set is best-effort: one
// org.freedesktop.portal.Wallpaper.SetWallpaperFile call, no DE probing, and
// honest status either way (portal confirmed "set" or saved-to-Pictures).

namespace {
// QDBusConnection::connect on this Qt only accepts receiver+slot, so a
// one-shot relay carries the Response callback; parented to the processor and
// deleteLater'd after firing, so per-call state never touches processor state.
class PortalResponseRelay : public QObject
{
    Q_OBJECT
public:
    explicit PortalResponseRelay(std::function<void(uint, const QVariantMap &)> cb,
                                 QObject *parent = nullptr)
        : QObject(parent), m_cb(std::move(cb)) {}
public Q_SLOTS:
    void dispatch(uint response, const QVariantMap &results)
    {
        m_cb(response, results);
        deleteLater();
    }
private:
    std::function<void(uint, const QVariantMap &)> m_cb;
};
}

QString WallpaperProcessor::portalSetOn(int target)
{
    switch (target) {
    case TargetLockscreen: return QStringLiteral("lockscreen");
    case TargetBoth:       return QStringLiteral("both");
    default:               return QStringLiteral("background");
    }
}

void WallpaperProcessor::saveToPictures(const QString &path)
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    QDir().mkpath(dir);
    const QString dest = dir + QDir::separator() + QFileInfo(path).fileName();
    if (QFile::copy(path, dest))
        m_statusMessage = i18n("Saved to %1 — set it in your desktop's settings", dest);
    else
        m_statusMessage = i18n("Rendered: %1", path);
    Q_EMIT statusMessageChanged();
}

bool WallpaperProcessor::setAsWallpaper(const QString &path, int target)
{
    if (path.isEmpty() || !QFile::exists(path)) return false;

    QFile *file = new QFile(path);
    if (!file->open(QIODevice::ReadOnly)) {
        delete file;
        return false;
    }

    // handle_token becomes the last element of the Request object path, so it
    // must be a valid D-Bus object-path element ([A-Za-z0-9_] only).
    const QString token = QStringLiteral("walltz_%1_%2")
                              .arg(QCoreApplication::applicationPid())
                              .arg(QRandomGenerator::global()->generate());
    QVariantMap options;
    options.insert(QStringLiteral("handle_token"), token);
    options.insert(QStringLiteral("set-on"), portalSetOn(target));
    // The QML menu already IS the user's explicit choice; the portal's
    // confirmation dialog (shown when show-preview is unset/default-true)
    // would be a redundant second prompt. KDE backend honors this directly.
    options.insert(QStringLiteral("show-preview"), false);

    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.portal.Desktop"),
        QStringLiteral("/org/freedesktop/portal/desktop"),
        QStringLiteral("org.freedesktop.portal.Wallpaper"),
        QStringLiteral("SetWallpaperFile"));
    msg << QString()                                                        // parent_window (none)
        << QVariant::fromValue(QDBusUnixFileDescriptor(file->handle()))     // fd
        << options;                                                         // a{sv}

    QDBusPendingCall call = QDBusConnection::sessionBus().asyncCall(msg);
    auto *watcher = new QDBusPendingCallWatcher(call, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, file, path]() {
        watcher->deleteLater();
        const QDBusPendingReply<QDBusObjectPath> reply = *watcher;
        // The portal has copied the file by reply time; our fd is no longer needed.
        file->deleteLater();
        if (reply.isError()) {
            // No portal on this desktop — the saved file IS the deliverable.
            saveToPictures(path);
            return;
        }
        // Portal replied with a Request object; its Response signal (u, a{sv})
        // fires once: 0 = success, 1 = user cancelled, 2 = error.
        const QDBusObjectPath requestPath = reply.value();
        auto *relay = new PortalResponseRelay(
            [this, path](uint response, const QVariantMap &results) {
                if (response == 0) {
                    m_statusMessage = i18n("Set as wallpaper: %1", QFileInfo(path).fileName());
                } else if (response == 1) {
                    m_statusMessage = i18n("Wallpaper change cancelled");
                } else {
                    const QString why = results.value(QStringLiteral("error")).toString();
                    Q_EMIT errorOccurred(i18n("Could not set wallpaper: %1",
                                              why.isEmpty() ? QStringLiteral("unknown error") : why));
                    saveToPictures(path);
                }
                Q_EMIT statusMessageChanged();
            },
            this);
        QDBusConnection::sessionBus().connect(
            QStringLiteral("org.freedesktop.portal.Desktop"),
            requestPath.path(),
            QStringLiteral("org.freedesktop.portal.Request"),
            QStringLiteral("Response"),
            relay,
            SLOT(dispatch(uint, QVariantMap)));
    });
    return true;
}

void WallpaperProcessor::processAndSetWallpaper(const QString &sourcePath, int target)
{
    QString outPath;
    if (processSingleImage(sourcePath, outPath)) {
        if (!setAsWallpaper(outPath, target)) {
            Q_EMIT errorOccurred(i18n("Rendered but could not open the output file"));
            m_statusMessage = i18n("Rendered: %1", outPath);
            Q_EMIT statusMessageChanged();
        }
    } else {
        return;   // errorOccurred already emitted
    }
}

#include "WallpaperProcessor.moc"
