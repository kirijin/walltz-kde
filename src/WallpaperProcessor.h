// SPDX-FileCopyrightText: 2026 kirijin <avel.ronin@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef WALLPAPERPROCESSOR_H
#define WALLPAPERPROCESSOR_H

#include <QObject>
#include <QImage>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QColor>
#include <QVariantList>
#include <QPair>
#include <QAtomicInt>
#include <QPointer>
#include <QMutex>
#include <QtConcurrent>
#include <QPainterPath>
#include <QFutureWatcher>

#include "blur_presets.h"
#include "WallpaperAnalyzer.h"

class QWindow;

// ── Factory defaults (single source of truth) ─────────────────────────────
// These are the one canonical copy. blur_presets.cpp's "default" row and
// Main.qml reset buttons derive from these via the accessors below.
namespace WalltzDefaults {
    inline constexpr int    blurRadius       = 90;    // px; 0 = auto
    inline constexpr double saturationFactor = 1.8;
    inline constexpr double blurBrightness   = 1.0;
    inline constexpr double colorGamma       = 1.0;   // per-channel curve (float stage)
    inline constexpr double colorWarmth      = 0.0;   // -1..1, R/B balance (positive = warm)
    inline constexpr double colorBlackLift   = 0.0;   // shadow floor 0..1 (faded-film look)
    inline constexpr int    textureBlendMode = 13;    // QPainter::CompositionMode_Multiply
    inline constexpr double overlayOpacity   = 0.0;
    inline constexpr double bgZoom           = 1.0;
    inline constexpr double bgBlurAngle      = 0.0;
    inline constexpr double gradientAngle    = 45.0;
    inline constexpr double fgZoom           = 0.8;   // 1.0 = golden-rect ceiling
    inline constexpr double pipZoom          = 1.0;
    inline constexpr double vignetteStrength = 0.0;
    inline constexpr double grainStrength    = 0.0;
    inline constexpr double caStrength       = 0.0;
    inline constexpr int    photoFrameWidth  = 0;     // % of min dim; 5 = golden ρ
    inline constexpr int    blurRadiusMax    = 120;
    // Composition
    inline constexpr double canvasMarginRho  = 0.05;  // 5% margin each side
    inline constexpr double minZoom          = 0.5;   // picture shrinks to half golden
}

/// Category of a background pattern, decoded from the flat integer id.
enum class PatternKind { Geometric, SvgGeo, Motif };

/// A decoded pattern reference. The flat `bgPatternType` int encodes the
/// category in ranges (0-49 geo, 50-99 svg, 100+ motif); PatternRef makes the
/// category explicit so no consumer re-derives it with qBound chains.
struct PatternRef {
    PatternKind kind = PatternKind::Geometric;
    int index = 0;             // 0-based within its kind
    int flat = 0;              // original flat bgPatternType value
};

// ── Render-parameter snapshot ─────────────────────────────────────────────
// Captured by value so a background thread can render without touching the
// processor. All mood/gradient colors are PRE-RESOLVED into the snapshot so
// the render core never reads processor members.
struct RenderSnapshot {
    // Target canvas
    int W = 1920, H = 1080;
    // Background mode
    bool blurMode = true;
    int  bgGradientStyle = 0;          // 0=Solid, 1=Preset, 2=Auto(mood)
    // Blur
    double bgZoom = WalltzDefaults::bgZoom;
    double bgBlurAngle = WalltzDefaults::bgBlurAngle;
    int    blurRadius = WalltzDefaults::blurRadius;
    double saturationFactor = WalltzDefaults::saturationFactor;
    double overlayOpacity = WalltzDefaults::overlayOpacity;
    QRgb   overlayColor = 0xff000000;
    double blurBrightness = WalltzDefaults::blurBrightness;
    // Solid / gradient background
    bool   autoColor = true;
    QRgb   bgColor = 0xffffffff;
    int    bgGradientPreset = 0;
    double gradientAngle = WalltzDefaults::gradientAngle;
    // Pre-resolved gradient endpoints (auto/mood mode); resolved on main thread
    QRgb   moodColorA = 0xff808080;
    QRgb   moodColorB = 0xffb4b4b4;
    // Patterns
    bool   bgPatternEnabled = false;
    int    bgPatternType = 0;
    QRgb   bgPatternColor = 0xff787878;
    double bgPatternScale = 1.0;
    double bgPatternRotation = 0.0;
    double bgPatternSpacing = 0.0;
    bool   bgPatternRandomRotate = false;
    bool   bgPatternJitter = false;
    double bgPatternGridAmplitude = 0.20;
    bool   bgPatternMixEnabled = false;
    QList<int> bgPatternMixMotifs;
    // Effects
    double vignetteStrength = WalltzDefaults::vignetteStrength;
    double grainStrength = WalltzDefaults::grainStrength;
    double caStrength = WalltzDefaults::caStrength;
    double colorGamma = WalltzDefaults::colorGamma;
    double colorWarmth = WalltzDefaults::colorWarmth;
    double colorBlackLift = WalltzDefaults::colorBlackLift;
    QImage textureImage;                  // resolved main-thread; null = no overlay
    double textureOpacity = 0.0;
    int    textureBlendMode = WalltzDefaults::textureBlendMode;
    bool   textureOverPhoto = false;      // draw the texture ON TOP of the photo
    bool   photoGrade = false;            // apply sat+grade to the foreground photo
    bool   photoFrame = false;
    int    photoFrameWidth = WalltzDefaults::photoFrameWidth;
    double fgZoom = WalltzDefaults::fgZoom;
    double pipZoom = WalltzDefaults::pipZoom;
    // Mood selection (used by the worker queue to resolve per-image colors)
    int    autoMood = 0;
    bool   useV2 = false;
    // Source image (implicitly shared — cheap, no detach)
    QImage sourceImage;
};

class WallpaperProcessor : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int targetWidth READ targetWidth WRITE setTargetWidth NOTIFY renderParamsChanged)
    Q_PROPERTY(int targetHeight READ targetHeight WRITE setTargetHeight NOTIFY renderParamsChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(QString outputPath READ outputPath NOTIFY outputPathChanged)
    Q_PROPERTY(bool blurMode READ blurMode WRITE setBlurMode NOTIFY renderParamsChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor NOTIFY renderParamsChanged)
    Q_PROPERTY(bool autoColor READ autoColor WRITE setAutoColor NOTIFY renderParamsChanged)
    Q_PROPERTY(int queueSize READ queueSize NOTIFY queueChanged)
    Q_PROPERTY(int queueProgress READ queueProgress NOTIFY queueProgressChanged)
    Q_PROPERTY(int screenWidth READ screenWidth NOTIFY screenWidthChanged)
    Q_PROPERTY(int screenHeight READ screenHeight NOTIFY screenHeightChanged)
    Q_PROPERTY(double windowDpr READ windowDpr NOTIFY windowDprChanged)
    Q_PROPERTY(bool keepAbove READ keepAbove NOTIFY keepAboveChanged)
    Q_PROPERTY(int aspectMode READ aspectMode WRITE setAspectMode NOTIFY renderParamsChanged)
    Q_PROPERTY(int blurRadius READ blurRadius WRITE setBlurRadius NOTIFY renderParamsChanged)
    Q_PROPERTY(double saturationFactor READ saturationFactor WRITE setSaturationFactor NOTIFY renderParamsChanged)
    Q_PROPERTY(double overlayOpacity READ overlayOpacity WRITE setOverlayOpacity NOTIFY renderParamsChanged)
    Q_PROPERTY(QColor overlayColor READ overlayColor WRITE setOverlayColor NOTIFY renderParamsChanged)
    Q_PROPERTY(double blurBrightness READ blurBrightness WRITE setBlurBrightness NOTIFY renderParamsChanged)
    Q_PROPERTY(QString blurPresetId READ blurPresetId NOTIFY blurPresetIdChanged)
    Q_PROPERTY(QStringList blurPresetNames READ blurPresetNames CONSTANT)
    Q_PROPERTY(int blurPresetIndex READ blurPresetIndex WRITE setBlurPresetIndex NOTIFY blurPresetIdChanged)
    Q_PROPERTY(int bgGradientStyle READ bgGradientStyle WRITE setBgGradientStyle NOTIFY renderParamsChanged)
    Q_PROPERTY(int bgGradientPreset READ bgGradientPreset WRITE setBgGradientPreset NOTIFY renderParamsChanged)
    Q_PROPERTY(double gradientAngle READ gradientAngle WRITE setGradientAngle NOTIFY renderParamsChanged)
    Q_PROPERTY(double bgZoom READ bgZoom WRITE setBgZoom NOTIFY renderParamsChanged)
    Q_PROPERTY(double bgBlurAngle READ bgBlurAngle WRITE setBgBlurAngle NOTIFY renderParamsChanged)
    Q_PROPERTY(int autoMood READ autoMood WRITE setAutoMood NOTIFY renderParamsChanged)
    Q_PROPERTY(bool useV2 READ useV2 WRITE setUseV2 NOTIFY renderParamsChanged)
    Q_PROPERTY(double vignetteStrength READ vignetteStrength WRITE setVignetteStrength NOTIFY renderParamsChanged)
    Q_PROPERTY(double grainStrength READ grainStrength WRITE setGrainStrength NOTIFY renderParamsChanged)
    Q_PROPERTY(double caStrength READ caStrength WRITE setCaStrength NOTIFY renderParamsChanged)
    Q_PROPERTY(QString texturePath READ texturePath WRITE setTexturePath NOTIFY renderParamsChanged)
    Q_PROPERTY(double textureOpacity READ textureOpacity WRITE setTextureOpacity NOTIFY renderParamsChanged)
    Q_PROPERTY(bool photoFrame READ photoFrame WRITE setPhotoFrame NOTIFY renderParamsChanged)
    Q_PROPERTY(int photoFrameWidth READ photoFrameWidth WRITE setPhotoFrameWidth NOTIFY renderParamsChanged)
    Q_PROPERTY(double fgZoom READ fgZoom WRITE setFgZoom NOTIFY renderParamsChanged)
    Q_PROPERTY(double pipZoom READ pipZoom WRITE setPipZoom NOTIFY renderParamsChanged)
    Q_PROPERTY(double fgZoomMin READ fgZoomMin NOTIFY fgZoomBoundsChanged)
    Q_PROPERTY(double fgZoomMax READ fgZoomMax NOTIFY fgZoomBoundsChanged)
    Q_PROPERTY(bool bgPatternEnabled READ bgPatternEnabled WRITE setBgPatternEnabled NOTIFY renderParamsChanged)
    Q_PROPERTY(int bgPatternType READ bgPatternType WRITE setBgPatternType NOTIFY renderParamsChanged)
    Q_PROPERTY(QColor bgPatternColor READ bgPatternColor WRITE setBgPatternColor NOTIFY renderParamsChanged)
    Q_PROPERTY(double bgPatternScale READ bgPatternScale WRITE setBgPatternScale NOTIFY renderParamsChanged)
    Q_PROPERTY(double bgPatternRotation READ bgPatternRotation WRITE setBgPatternRotation NOTIFY renderParamsChanged)
    Q_PROPERTY(double bgPatternSpacing READ bgPatternSpacing WRITE setBgPatternSpacing NOTIFY renderParamsChanged)
    Q_PROPERTY(bool bgPatternRandomRotate READ bgPatternRandomRotate WRITE setBgPatternRandomRotate NOTIFY renderParamsChanged)
    Q_PROPERTY(bool bgPatternJitter READ bgPatternJitter WRITE setBgPatternJitter NOTIFY renderParamsChanged)
    Q_PROPERTY(double bgPatternGridAmplitude READ bgPatternGridAmplitude WRITE setBgPatternGridAmplitude NOTIFY renderParamsChanged)
    Q_PROPERTY(bool bgPatternMixEnabled READ bgPatternMixEnabled WRITE setBgPatternMixEnabled NOTIFY renderParamsChanged)
    Q_PROPERTY(QVariantList bgPatternMixMotifs READ bgPatternMixMotifs NOTIFY bgPatternMixMotifsChanged)
    Q_PROPERTY(int mixMotifCount READ mixMotifCount NOTIFY bgPatternMixMotifsChanged)

    // ── Named-parameter presets (F2) ──
    Q_PROPERTY(QStringList paramPresetNames READ paramPresetNames NOTIFY paramPresetsChanged)

public:
    explicit WallpaperProcessor(QObject *parent = nullptr);

    // ── Getters ──
    int targetWidth() const { return m_targetWidth; }
    int targetHeight() const { return m_targetHeight; }
    QString statusMessage() const { return m_statusMessage; }
    QString outputPath() const { return m_outputPath; }
    bool blurMode() const { return m_blurMode; }
    bool busy() const { return m_busy; }
    QColor backgroundColor() const { return m_bgColor; }
    bool autoColor() const { return m_autoColor; }
    int queueSize() const { return m_queue.size(); }
    int queueProgress() const { return m_queueProgress.loadRelaxed(); }
    int screenWidth() const { return m_screenWidth; }
    int screenHeight() const { return m_screenHeight; }
    bool keepAbove() const { return m_keepAbove; }
    double windowDpr() const { return m_windowDpr; }
    int aspectMode() const { return m_aspectMode; }
    int blurRadius() const { return m_blurRadius; }
    double saturationFactor() const { return m_saturationFactor; }
    double colorGamma() const { return m_colorGamma; }
    double colorWarmth() const { return m_colorWarmth; }
    double colorBlackLift() const { return m_colorBlackLift; }
    double overlayOpacity() const { return m_overlayOpacity; }
    QColor overlayColor() const { return m_overlayColor; }
    double blurBrightness() const { return m_blurBrightness; }
    QString blurPresetId() const { return m_blurPresetId; }
    QStringList blurPresetNames() const;
    int blurPresetIndex() const { return m_blurPresetIndex; }
    void setBlurPresetIndex(int index);
    int bgGradientStyle() const { return m_bgGradientStyle; }
    int bgGradientPreset() const { return m_bgGradientPreset; }
    double gradientAngle() const { return m_gradientAngle; }
    double bgZoom() const { return m_bgZoom; }
    double bgBlurAngle() const { return m_bgBlurAngle; }
    int autoMood() const { return m_autoMood; }
    bool useV2() const { return m_useV2; }
    double vignetteStrength() const { return m_vignetteStrength; }
    double grainStrength() const { return m_grainStrength; }
    double caStrength() const { return m_caStrength; }
    QString texturePath() const { return m_texturePath; }
    double textureOpacity() const { return m_textureOpacity; }
    int textureBlendMode() const { return m_textureBlendMode; }
    bool photoGrade() const { return m_photoGrade; }
    bool photoFrame() const { return m_photoFrame; }
    int photoFrameWidth() const { return m_photoFrameWidth; }
    double fgZoom() const { return m_fgZoom; }
    void setFgZoom(double z);
    double pipZoom() const { return m_pipZoom; }
    void setPipZoom(double z);
    double fgZoomMin() const { return m_fgZoomMin; }
    double fgZoomMax() const { return m_fgZoomMax; }
    bool bgPatternEnabled() const { return m_bgPatternEnabled; }
    int bgPatternType() const { return m_bgPatternType; }
    QColor bgPatternColor() const { return m_bgPatternColor; }
    double bgPatternScale() const { return m_bgPatternScale; }
    double bgPatternRotation() const { return m_bgPatternRotation; }
    double bgPatternSpacing() const { return m_bgPatternSpacing; }
    bool bgPatternRandomRotate() const { return m_bgPatternRandomRotate; }
    bool bgPatternJitter() const { return m_bgPatternJitter; }
    double bgPatternGridAmplitude() const { return m_bgPatternGridAmplitude; }
    bool bgPatternMixEnabled() const { return m_bgPatternMixEnabled; }
    QVariantList bgPatternMixMotifs() const;
    int mixMotifCount() const { return m_bgPatternMixMotifs.size(); }

    // ── Setters ──
    void setTargetWidth(int w);
    void setTargetHeight(int h);
    void setBlurMode(bool blur);
    void setBackgroundColor(const QColor &c);
    void setAutoColor(bool autoC);
    void setBlurRadius(int r);
    void setSaturationFactor(double f);
    void setColorGamma(double g);
    void setColorWarmth(double w);
    void setColorBlackLift(double l);
    void setOverlayOpacity(double o);
    void setOverlayColor(const QColor &c);
    void setBlurBrightness(double b);
    void setBgGradientStyle(int s);
    void setBgGradientPreset(int p);
    void setGradientAngle(double a);
    void setBgZoom(double z);
    void setBgBlurAngle(double a);
    void setAutoMood(int m);
    void setUseV2(bool v2);
    void setVignetteStrength(double s);
    void setGrainStrength(double s);
    void setCaStrength(double s);
    void setTexturePath(const QString &path);
    void setTextureOpacity(double o);
    void setTextureBlendMode(int m);
    void setPhotoFrame(bool on);
    void setPhotoFrameWidth(int w);
    void setBgPatternEnabled(bool on);
    void setBgPatternType(int t);
    void setBgPatternColor(const QColor &c);
    void setBgPatternScale(double s);
    void setBgPatternRotation(double a);
    void setBgPatternSpacing(double s);
    void setBgPatternRandomRotate(bool on);
    void setBgPatternJitter(bool on);
    void setBgPatternGridAmplitude(double v);
    void setBgPatternMixEnabled(bool on);
    Q_INVOKABLE void setBgPatternMixMotifs(const QVariantList &indices);
    Q_INVOKABLE void toggleMixMotif(int index);
    Q_INVOKABLE void resetBlurToDefault();

    /// Generate a small processed preview — returns last-good file:// URL.
    Q_INVOKABLE QString generatePreview(const QString &sourcePath);

    /// Gradient preset access
    Q_INVOKABLE int gradientPresetCount() const;
    Q_INVOKABLE QString gradientPresetName(int index) const;
    Q_INVOKABLE QString gradientPresetColor1(int index) const;
    Q_INVOKABLE QString gradientPresetColor2(int index) const;
    Q_INVOKABLE double aspectRatioForMode(int mode) const;

    /// Overlay catalog (user assets in AppDataLocation/overlays).
    Q_INVOKABLE QStringList textureCatalog();
    Q_INVOKABLE QString textureCatalogDir() const;

    /// Mood palette access
    Q_INVOKABLE int moodCount() const { return 6; }
    Q_INVOKABLE QString moodName(int index) const;
    Q_INVOKABLE QString moodColorA(int index) const;
    Q_INVOKABLE QString moodColorB(int index) const;
    Q_INVOKABLE QString moodNameV2(int index) const;
    Q_INVOKABLE QString moodColorV2A(int index) const;
    Q_INVOKABLE QString moodColorV2B(int index) const;

    // ── Pattern QML accessors ──
    Q_INVOKABLE int geometricPatternCount() const;
    Q_INVOKABLE QString geometricPatternName(int index) const;
    Q_INVOKABLE QString geometricPatternThumbnail(int index, int thumbSize = 60);
    Q_INVOKABLE int motifPatternCount() const;
    Q_INVOKABLE QString motifPatternName(int index) const;
    Q_INVOKABLE int motifPatternCategory(int index) const;
    Q_INVOKABLE QString motifCategoryName(int cat) const;
    Q_INVOKABLE int motifOffset() const { return MOTIF_OFFSET; }
    Q_INVOKABLE QString motifPatternThumbnail(int index, int thumbSize = 60);
    Q_INVOKABLE int svgGeoPatternCount() const { return SVG_GEO_COUNT; }
    Q_INVOKABLE int svgGeoOffset() const { return SVG_GEO_OFFSET; }
    Q_INVOKABLE QString svgGeoPatternName(int index) const;
    Q_INVOKABLE QString svgGeoPatternThumbnail(int index, int thumbSize = 60);

    // ── Factory-default accessors for QML reset buttons (Q5) ──
    Q_INVOKABLE int defaultBlurRadius() const       { return WalltzDefaults::blurRadius; }
    Q_INVOKABLE double defaultSaturation() const    { return WalltzDefaults::saturationFactor; }
    Q_INVOKABLE double defaultBrightness() const    { return WalltzDefaults::blurBrightness; }
    Q_INVOKABLE double defaultBgZoom() const        { return WalltzDefaults::bgZoom; }
    Q_INVOKABLE double defaultBgBlurAngle() const   { return WalltzDefaults::bgBlurAngle; }
    Q_INVOKABLE double defaultGradientAngle() const { return WalltzDefaults::gradientAngle; }
    Q_INVOKABLE double defaultFgZoom() const        { return WalltzDefaults::fgZoom; }
    Q_INVOKABLE double defaultPipZoom() const       { return WalltzDefaults::pipZoom; }
    Q_INVOKABLE int defaultPhotoFrameWidth() const  { return WalltzDefaults::photoFrameWidth; }

    // ── Named-parameter presets (F2) ──
    QStringList paramPresetNames() const;
    Q_INVOKABLE void saveParamPreset(const QString &name);
    Q_INVOKABLE void applyParamPreset(const QString &name);
    Q_INVOKABLE void deleteParamPreset(const QString &name);
    Q_INVOKABLE void rememberState();     // F6: snapshot all params (undo anchor)
    Q_INVOKABLE void restoreState();      // F6: restore to last rememberState()

    // ── Set-as-wallpaper (F1) ──
    enum WallpaperTarget { TargetDesktop = 0, TargetLockscreen = 1, TargetBoth = 2 };
    Q_INVOKABLE bool setAsWallpaper(const QString &path, int target = TargetDesktop);
    Q_INVOKABLE void processAndSetWallpaper(const QString &sourcePath, int target = TargetDesktop);

    /// Render one image synchronously to "<name>.wp.png" (used by CLI F3).
    bool processSingleImage(const QString &sourcePath, QString &outPath);

public Q_SLOTS:
    void detectScreenSize();
    void setWindow(QWindow *window);
    void setKeepAbove(bool keep);
    void setAspectMode(int mode);
    void processImage(const QString &sourcePath);
    void processQueue(const QStringList &paths);
    void cancelProcessing();
    void updateScreenSize(int w, int h);

private Q_SLOTS:
    void startQueue();
    void handleQueueResult(int index, const QString &outPath);
    void finishQueue();
    void detectFromWindow();
    void pollDpr();

Q_SIGNALS:
    /// Aggregate: emitted by every render-affecting setter (Q2). QML hooks one handler.
    void renderParamsChanged();
    void statusMessageChanged();
    void outputPathChanged();
    void busyChanged();
    void queueChanged();
    void queueProgressChanged();
    void screenWidthChanged();
    void screenHeightChanged();
    void keepAboveChanged();
    void windowDprChanged();
    void blurPresetIdChanged();
    void fgZoomBoundsChanged();
    void bgPatternMixMotifsChanged();
    void textureCatalogChanged();
    void paramPresetsChanged();
    void processingStarted();
    void processingFinished();
    void errorOccurred(const QString &message);
    void previewReady(const QString &sourcePath, const QString &previewUrl);

private:
    int m_targetWidth = 1920;
    int m_targetHeight = 1080;
    int m_screenWidth = 1920;
    int m_screenHeight = 1080;
    QString m_statusMessage;
    QString m_outputPath;
    bool m_blurMode = true;
    bool m_busy = false;
    QColor m_bgColor = Qt::white;
    bool m_autoColor = true;
    QPointer<QWindow> m_window;              // B7: null-safe on window destruction
    double m_windowDpr = 1.0;
    bool m_keepAbove = false;
    int m_aspectMode = 0;
    double m_aspectRatio = 0.0;
    int m_detectAttempt = 0;
    int m_dprStableCount = 0;   // bounded DPR poll self-terminates (Q11)

    // ── Async queue (worker pool, B8) ──
    QStringList m_queue;
    QAtomicInt m_queueProgress{0};
    QAtomicInt m_cancelRequested{0};
    QFutureWatcher<QPair<int, QString>> m_queueWatcher;
    RenderSnapshot m_queueSnapshot;      // params captured on main thread
    QVariantMap m_undoSnapshot;          // F6: undo anchor (rememberState/restoreState)

    int m_blurRadius = WalltzDefaults::blurRadius;
    double m_saturationFactor = WalltzDefaults::saturationFactor;
    double m_colorGamma = WalltzDefaults::colorGamma;
    double m_colorWarmth = WalltzDefaults::colorWarmth;
    double m_colorBlackLift = WalltzDefaults::colorBlackLift;
    QString m_blurPresetId = QStringLiteral("default");
    int m_blurPresetIndex = 0;
    double m_overlayOpacity = WalltzDefaults::overlayOpacity;
    QColor m_overlayColor = Qt::black;
    double m_blurBrightness = WalltzDefaults::blurBrightness;
    int m_bgGradientStyle = 0;
    int m_bgGradientPreset = 0;
    double m_gradientAngle = WalltzDefaults::gradientAngle;
    double m_bgZoom = WalltzDefaults::bgZoom;
    double m_bgBlurAngle = WalltzDefaults::bgBlurAngle;
    int m_autoMood = 0;
    bool m_useV2 = false;
    double m_vignetteStrength = WalltzDefaults::vignetteStrength;
    double m_grainStrength = WalltzDefaults::grainStrength;
    double m_caStrength = WalltzDefaults::caStrength;
    bool m_photoFrame = false;
    int m_photoFrameWidth = WalltzDefaults::photoFrameWidth;
    double m_fgZoom = WalltzDefaults::fgZoom;
    double m_pipZoom = WalltzDefaults::pipZoom;
    double m_fgZoomMin = WalltzDefaults::minZoom;
    double m_fgZoomMax = 1.0;
    QColor m_moodColorsA[6];
    QColor m_moodColorsB[6];
    QColor m_moodColorsV2A[6];
    QColor m_moodColorsV2B[6];
    bool m_moodsComputed = false;
    // Recursive: computeMoodPalettes() calls computeMoodPalettesV2() internally.
    mutable QRecursiveMutex m_moodMutex;   // guards the four palette arrays + flag

    // ── Pattern parameters ──
    bool m_bgPatternEnabled = false;
    int m_bgPatternType = 0;
    QColor m_bgPatternColor = QColor(120, 120, 120);
    double m_bgPatternScale = 1.0;
    double m_bgPatternRotation = 0.0;
    double m_bgPatternSpacing = 0.0;
    bool m_bgPatternRandomRotate = false;
    bool m_bgPatternJitter = false;
    double m_bgPatternGridAmplitude = 0.20;
    bool m_bgPatternMixEnabled = false;
    QList<int> m_bgPatternMixMotifs;

    // ── Smart Auto state ──
    ImageStats m_imageStats;
    bool m_statsValid = false;
    void computeSmartAutoAndApply();

    // ── Async preview state ──
    QString m_lastPreviewUrl;
    QAtomicInt m_nextRenderId{1};
    int m_currentRenderId = 0;   // main-thread only (documented invariant, B10)

    // ── Caches ──
    mutable QHash<int, QString> m_geometricThumbnailCache;
    mutable QHash<int, QString> m_motifThumbnailCache;
    mutable QHash<int, QString> m_svgGeoThumbnailCache;
    QHash<QString, QImage> m_renderTileCache;   // Q14: rendered pattern tiles

    // ── Overlay state (Phase 2: user-asset texture overlay) ──
    QString m_texturePath;                       // empty = off
    double  m_textureOpacity = 0.0;
    int     m_textureBlendMode = WalltzDefaults::textureBlendMode;
    QImage  m_textureLoaded;                     // resolved on the main thread
    QStringList m_textureCatalog;                // cached scan of the overlays dir
    bool    m_textureOverPhoto = false;
    bool    m_photoGrade = false;                // photo look active: grade the photo

    /// Build a fully-resolved snapshot from current member state + source.
    /// Must be called on the main thread (reads members, resolves mood colors).
    RenderSnapshot captureSnapshot(const QImage &src, int W, int H);

    void computeMoodPalettes(const QImage &image);
    void computeMoodPalettesV2(const QImage &image);
    QPair<QColor, QColor> extractHarmonizedColors(const QImage &image, int mood = 0);
    /// Thread-safe mood resolution for worker tasks (locks m_moodMutex).
    QPair<QColor, QColor> resolveMoodColors(const QImage &image, int mood, bool useV2);

    // ── Param serialization (shared by presets F2 and undo F6) ──
    QVariantMap serializeParams() const;
    void deserializeParams(const QVariantMap &m);

    struct Centroid3D {
        double r, g, b;
        double score;
        int ri, gi, bi;
        int count;
    };

    static QImage limitImageSize(const QImage &src, int maxW, int maxH);

    /// Decode a flat bgPatternType into a PatternRef (Q3). Single decode site.
    static PatternRef decodePatternType(int flat);

    /// Map a WallpaperTarget to the portal's set-on value ("background" | "lockscreen" | "both").
    static QString portalSetOn(int target);

    /// Copy the rendered file to ~/Pictures and report the path (fallback when
    /// no portal is available). Emits statusMessageChanged.
    void saveToPictures(const QString &path);

    /// Static adaptive pattern overlay used by renderCore (thread-safe).
    static void renderAdaptivePatternStatic(QImage &output, const RenderSnapshot &rs,
                                            QHash<QString, QImage> *tileCache = nullptr);

public:
    /// True separable Gaussian blur + float saturation/overlay/brightness.
    /// Replaces the old O(n) box cascade (which caused gradient banding).
    /// colorGamma/warmth/blackLift are the float color-grade stage (Phase 1):
    /// applied gamma -> warmth -> blackLift -> clamp inside the float buffer,
    /// before the single 8-bit dither. All three default neutral (no-op).
    static void stackBlur(QImage &image, double sigma,
                          double saturationFactor = 0.0,
                          double overlayOpacity = 0.0,
                          QRgb overlayColor = 0,
                          double brightness = 1.0,
                          double colorGamma = 1.0,
                          double colorWarmth = 0.0,
                          double colorBlackLift = 0.0);

    /// Unified render core (Q1). Thread-safe: reads only the snapshot.
    /// Public so it can be exercised by tests and embedded callers.
    static QImage renderCore(const RenderSnapshot &rs,
                             double *outMinZoom = nullptr,
                             double *outMaxZoom = nullptr,
                             QHash<QString, QImage> *tileCache = nullptr);

    static constexpr double s_aspectRatios[7] = {0.0, 1.0, 4.0/3.0, 16.0/9.0, 16.0/10.0, 21.0/9.0, 32.0/9.0};

    struct GradientPreset {
        const char *name;
        QRgb color1;
        QRgb color2;
    };
    static const GradientPreset s_presets[12];

    static constexpr int GEOMETRIC_PATTERN_COUNT = 8;
    static constexpr int MOTIF_OFFSET = 100;
    static constexpr int MOTIF_PATTERN_COUNT = 88;
    static constexpr int SVG_GEO_OFFSET = 50;
    static constexpr int SVG_GEO_COUNT = 16;

    // Pattern tile generators — static (no member state; Q2 dedup). The
    // thumbnail and adaptive-pattern paths both call these; the old `make*Tile`
    // statics were byte-identical duplicates and are gone.
    static QImage generateGeometricTile(int type, int tileSize, const QColor &bg, const QColor &fg, double scale);
    static QImage generateSvgGeoTile(int index, int tileSize, const QColor &bg, const QColor &fg, double scale);
    static QImage generateMotifTile(int motifIndex, int tileSize, const QColor &bg, const QColor &fg, double scale);

    static const char *s_geometricNames[GEOMETRIC_PATTERN_COUNT];
    static const char *s_svgGeoNames[SVG_GEO_COUNT];
    static const char *s_motifNames[MOTIF_PATTERN_COUNT];
    static const int s_motifCategories[MOTIF_PATTERN_COUNT];
    static const char *s_motifSvgFiles[MOTIF_PATTERN_COUNT];
    static const char *s_categoryNames[6];
};

#endif // WALLPAPERPROCESSOR_H
