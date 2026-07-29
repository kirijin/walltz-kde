#ifndef WALLPAPERPROCESSOR_H
#define WALLPAPERPROCESSOR_H

#include <QObject>
#include <QImage>
#include <QSize>
#include <QString>
#include <QColor>
#include <QStringList>
#include <QVariantList>
#include <QPair>
#include <QtConcurrent>
#include <QPainterPath>

class QWindow;

class WallpaperProcessor : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int targetWidth READ targetWidth WRITE setTargetWidth NOTIFY targetWidthChanged)
    Q_PROPERTY(int targetHeight READ targetHeight WRITE setTargetHeight NOTIFY targetHeightChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(QString outputPath READ outputPath NOTIFY outputPathChanged)
    Q_PROPERTY(bool blurMode READ blurMode WRITE setBlurMode NOTIFY blurModeChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor NOTIFY backgroundColorChanged)
    Q_PROPERTY(bool autoColor READ autoColor WRITE setAutoColor NOTIFY autoColorChanged)
    Q_PROPERTY(int queueSize READ queueSize NOTIFY queueChanged)
    Q_PROPERTY(int queueProgress READ queueProgress NOTIFY queueProgressChanged)
    Q_PROPERTY(int screenWidth READ screenWidth NOTIFY screenWidthChanged)
    Q_PROPERTY(int screenHeight READ screenHeight NOTIFY screenHeightChanged)
    Q_PROPERTY(double windowDpr READ windowDpr NOTIFY windowDprChanged)
    Q_PROPERTY(bool keepAbove READ keepAbove NOTIFY keepAboveChanged)
    Q_PROPERTY(int aspectMode READ aspectMode WRITE setAspectMode NOTIFY aspectModeChanged)
    // ── New tweakable parameters ──
    Q_PROPERTY(int blurRadius READ blurRadius WRITE setBlurRadius NOTIFY blurRadiusChanged)
    Q_PROPERTY(double saturationFactor READ saturationFactor WRITE setSaturationFactor NOTIFY saturationFactorChanged)
    Q_PROPERTY(int bgGradientStyle READ bgGradientStyle WRITE setBgGradientStyle NOTIFY bgGradientStyleChanged)
    Q_PROPERTY(int bgGradientPreset READ bgGradientPreset WRITE setBgGradientPreset NOTIFY bgGradientPresetChanged)
    Q_PROPERTY(double gradientAngle READ gradientAngle WRITE setGradientAngle NOTIFY gradientAngleChanged)
    Q_PROPERTY(double bgZoom READ bgZoom WRITE setBgZoom NOTIFY bgZoomChanged)
    Q_PROPERTY(double bgBlurAngle READ bgBlurAngle WRITE setBgBlurAngle NOTIFY bgBlurAngleChanged)
    Q_PROPERTY(int autoMood READ autoMood WRITE setAutoMood NOTIFY autoMoodChanged)
    Q_PROPERTY(bool useV2 READ useV2 WRITE setUseV2 NOTIFY useV2Changed)
    Q_PROPERTY(double vignetteStrength READ vignetteStrength WRITE setVignetteStrength NOTIFY vignetteStrengthChanged)
    Q_PROPERTY(double grainStrength READ grainStrength WRITE setGrainStrength NOTIFY grainStrengthChanged)
    Q_PROPERTY(double caStrength READ caStrength WRITE setCaStrength NOTIFY caStrengthChanged)
    Q_PROPERTY(bool photoFrame READ photoFrame WRITE setPhotoFrame NOTIFY photoFrameChanged)
    Q_PROPERTY(int photoFrameWidth READ photoFrameWidth WRITE setPhotoFrameWidth NOTIFY photoFrameWidthChanged)
    // ── Pattern properties ──
    Q_PROPERTY(bool bgPatternEnabled READ bgPatternEnabled WRITE setBgPatternEnabled NOTIFY bgPatternEnabledChanged)
    Q_PROPERTY(int bgPatternType READ bgPatternType WRITE setBgPatternType NOTIFY bgPatternTypeChanged)
    Q_PROPERTY(QColor bgPatternColor READ bgPatternColor WRITE setBgPatternColor NOTIFY bgPatternColorChanged)
    Q_PROPERTY(double bgPatternScale READ bgPatternScale WRITE setBgPatternScale NOTIFY bgPatternScaleChanged)
    Q_PROPERTY(double bgPatternRotation READ bgPatternRotation WRITE setBgPatternRotation NOTIFY bgPatternRotationChanged)
    Q_PROPERTY(double bgPatternSpacing READ bgPatternSpacing WRITE setBgPatternSpacing NOTIFY bgPatternSpacingChanged)
    Q_PROPERTY(bool bgPatternRandomRotate READ bgPatternRandomRotate WRITE setBgPatternRandomRotate NOTIFY bgPatternRandomRotateChanged)
    Q_PROPERTY(bool bgPatternJitter READ bgPatternJitter WRITE setBgPatternJitter NOTIFY bgPatternJitterChanged)
    Q_PROPERTY(double bgPatternGridAmplitude READ bgPatternGridAmplitude WRITE setBgPatternGridAmplitude NOTIFY bgPatternGridAmplitudeChanged)
    Q_PROPERTY(bool bgPatternMixEnabled READ bgPatternMixEnabled WRITE setBgPatternMixEnabled NOTIFY bgPatternMixEnabledChanged)

public:
    explicit WallpaperProcessor(QObject *parent = nullptr);

    // ── Existing getters ──
    int targetWidth() const { return m_targetWidth; }
    int targetHeight() const { return m_targetHeight; }
    QString statusMessage() const { return m_statusMessage; }
    QString outputPath() const { return m_outputPath; }
    bool blurMode() const { return m_blurMode; }
    bool busy() const { return m_busy; }
    QColor backgroundColor() const { return m_bgColor; }
    bool autoColor() const { return m_autoColor; }
    int queueSize() const { return m_queue.size(); }
    int queueProgress() const { return m_queueProgress; }
    int screenWidth() const { return m_screenWidth; }
    int screenHeight() const { return m_screenHeight; }
    bool keepAbove() const { return m_keepAbove; }
    double windowDpr() const { return m_windowDpr; }
    int aspectMode() const { return m_aspectMode; }
    // ── New getters ──
    int blurRadius() const { return m_blurRadius; }
    double saturationFactor() const { return m_saturationFactor; }
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
    bool photoFrame() const { return m_photoFrame; }
    int photoFrameWidth() const { return m_photoFrameWidth; }
    // ── Pattern getters ──
    bool bgPatternEnabled() const { return m_bgPatternEnabled; }
    int bgPatternType() const { return m_bgPatternType; }
    QColor bgPatternColor() const { return m_bgPatternColor; }
    double bgPatternScale() const { return m_bgPatternScale; }
    double bgPatternRotation() const { return m_bgPatternRotation; }
    double bgPatternSpacing() const { return m_bgPatternSpacing; }
    bool bgPatternRandomRotate() const { return m_bgPatternRandomRotate; }
    bool bgPatternJitter() const { return m_bgPatternJitter; }
    bool bgPatternMixEnabled() const { return m_bgPatternMixEnabled; }
    double bgPatternGridAmplitude() const { return m_bgPatternGridAmplitude; }

    // ── Existing setters ──
    void setTargetWidth(int w);
    void setTargetHeight(int h);
    void setBlurMode(bool blur);
    void setBackgroundColor(const QColor &c);
    void setAutoColor(bool autoC);
    // ── New setters ──
    void setBlurRadius(int r);
    void setSaturationFactor(double f);
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
    void setPhotoFrame(bool on);
    void setPhotoFrameWidth(int w);
    // ── Pattern setters ──
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

    /// Generate a small processed preview (400px max) — returns file:// URL
    Q_INVOKABLE QString generatePreview(const QString &sourcePath);

    /// Gradient preset access
    Q_INVOKABLE int gradientPresetCount() const;
    Q_INVOKABLE QString gradientPresetName(int index) const;
    Q_INVOKABLE QString gradientPresetColor1(int index) const;
    Q_INVOKABLE QString gradientPresetColor2(int index) const;
    Q_INVOKABLE double aspectRatioForMode(int mode) const;

    /// Mood palette access (auto-gradient variants)
    Q_INVOKABLE int moodCount() const { return 6; }
    Q_INVOKABLE QString moodName(int index) const;
    Q_INVOKABLE QString moodColorA(int index) const;
    Q_INVOKABLE QString moodColorB(int index) const;

    /// V2 mood palette access (3D RGB histogram — second row)
    Q_INVOKABLE QString moodNameV2(int index) const;
    Q_INVOKABLE QString moodColorV2A(int index) const;
    Q_INVOKABLE QString moodColorV2B(int index) const;

    // ── Pattern QML accessors ──
    /// Total number of geometric pattern types
    Q_INVOKABLE int geometricPatternCount() const;
    /// Name of a geometric pattern type (for UI labels)
    Q_INVOKABLE QString geometricPatternName(int index) const;
    /// Generate a small thumbnail for a geometric pattern (returns file:// URL)
    Q_INVOKABLE QString geometricPatternThumbnail(int index, int thumbSize = 60) const;

    /// Total number of motif (icon) pattern types
    Q_INVOKABLE int motifPatternCount() const;
    /// Name of a motif pattern
    Q_INVOKABLE QString motifPatternName(int index) const;
    /// Category of a motif: 0=animal, 1=critter, 2=nature, 3=music, 4=celestial, 5=whimsical
    Q_INVOKABLE int motifPatternCategory(int index) const;
    /// Category name for UI tabs
    Q_INVOKABLE QString motifCategoryName(int cat) const;
    /// Offset for motif pattern types (add to motif index to get bgPatternType value)
    Q_INVOKABLE int motifOffset() const { return MOTIF_OFFSET; }
    /// Generate a small thumbnail for a motif pattern (returns file:// URL)
    Q_INVOKABLE QString motifPatternThumbnail(int index, int thumbSize = 60) const;

    // ── SVG geometric primitive accessors ──
    Q_INVOKABLE int svgGeoPatternCount() const { return SVG_GEO_COUNT; }
    Q_INVOKABLE int svgGeoOffset() const { return SVG_GEO_OFFSET; }
    Q_INVOKABLE QString svgGeoPatternName(int index) const;
    Q_INVOKABLE QString svgGeoPatternThumbnail(int index, int thumbSize = 60) const;

    /// Get the currently selected mix motif indices (for QML to highlight)
    Q_PROPERTY(QVariantList bgPatternMixMotifs READ bgPatternMixMotifs
               NOTIFY bgPatternMixMotifsChanged)
    Q_INVOKABLE QVariantList bgPatternMixMotifs() const;
    /// Number of patterns currently in the mix (reliable for QML labels)
    Q_PROPERTY(int mixMotifCount READ mixMotifCount NOTIFY bgPatternMixMotifsChanged)
    Q_INVOKABLE int mixMotifCount() const { return m_bgPatternMixMotifs.size(); }
    /// Set mix motif indices
    Q_INVOKABLE void setBgPatternMixMotifs(const QVariantList &indices);
    /// Toggle a motif in the mix selection
    Q_INVOKABLE void toggleMixMotif(int index);

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
    void processNext();
    void detectFromWindow();
    void pollDpr();

Q_SIGNALS:
    void targetWidthChanged();
    void targetHeightChanged();
    void statusMessageChanged();
    void outputPathChanged();
    void blurModeChanged();
    void backgroundColorChanged();
    void autoColorChanged();
    void queueChanged();
    void queueProgressChanged();
    void busyChanged();
    void screenWidthChanged();
    void screenHeightChanged();
    void keepAboveChanged();
    void aspectModeChanged();
    void windowDprChanged();
    void processingStarted();
    void processingFinished();
    void errorOccurred(const QString &message);
    // ── New signals ──
    void blurRadiusChanged();
    void saturationFactorChanged();
    void bgGradientStyleChanged();
    void bgGradientPresetChanged();
    void gradientAngleChanged();
    void bgZoomChanged();
    void bgBlurAngleChanged();
    void autoMoodChanged();
    void useV2Changed();
    void vignetteStrengthChanged();
    void grainStrengthChanged();
    void caStrengthChanged();
    void photoFrameChanged();
    void photoFrameWidthChanged();
    // ── Pattern signals ──
    void bgPatternEnabledChanged();
    void bgPatternTypeChanged();
    void bgPatternColorChanged();
    void bgPatternScaleChanged();
    void bgPatternRotationChanged();
    void bgPatternSpacingChanged();
    void bgPatternRandomRotateChanged();
    void bgPatternJitterChanged();
    void bgPatternGridAmplitudeChanged();
    void bgPatternMixEnabledChanged();
    void bgPatternMixMotifsChanged();

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
    bool m_cancelRequested = false;
    QWindow *m_window = nullptr;
    double m_windowDpr = 1.0;
    bool m_keepAbove = false;
    int m_aspectMode = 0;
    double m_aspectRatio = 0.0;
    int m_detectAttempt = 0;

    QStringList m_queue;
    int m_queueProgress = 0;
    int m_currentIndex = 0;

    // ── New tweakable parameters ──
    int m_blurRadius = 0;          // 0 = auto (adaptive 0.051×H), 1–120 = manual
    double m_saturationFactor = 1.8;
    int m_bgGradientStyle = 0;      // 0 = Solid, 1 = Preset, 2 = Auto
    int m_bgGradientPreset = 0;     // index into s_presets[]
    double m_gradientAngle = 45.0;   // degrees (0 = horizontal, 45 = diagonal ↘)
    double m_bgZoom = 1.0;          // background zoom multiplier (0.5–3.0, 1.0 = fill)
    double m_bgBlurAngle = 0.0;     // blur background rotation (degrees, 0 = normal)
    int m_autoMood = 0;             // 0=Auto, 1=Soft, 2=Vivid, 3=Warm, 4=Cool, 5=Deep
    bool m_useV2 = false;           // use V2 (3D RGB histogram) instead of V1
    double m_vignetteStrength = 0.0; // vignette: 0 = off, 1 = max
    double m_grainStrength = 0.0;    // grain: 0 = off, 1 = max
    double m_caStrength = 0.0;       // chromatic aberration: 0 = off, 1 = max
    bool m_photoFrame = false;
    int m_photoFrameWidth = 0;
    QColor m_moodColorsA[6];        // cached mood gradient color A (index = mood)
    QColor m_moodColorsB[6];        // cached mood gradient color B
    QColor m_moodColorsV2A[6];      // V2 mood gradient color A (second row)
    QColor m_moodColorsV2B[6];      // V2 mood gradient color B
    bool m_moodsComputed = false;   // true after computeAllMoods()

    // ── Pattern parameters ──
    bool m_bgPatternEnabled = false;
    int m_bgPatternType = 0;         // 0..7 = geometric, 50..65 = SVG geo, 100..137 = motif, 150..157 = texture
    QColor m_bgPatternColor = QColor(120, 120, 120); // pattern foreground
    double m_bgPatternScale = 1.0;   // 0.3 – 3.0
    double m_bgPatternRotation = 0.0; // degrees (per-tile random now)
    double m_bgPatternSpacing = 0.0;  // extra gap between tiles, 0.0-2.0
    bool m_bgPatternRandomRotate = false;  // random per-tile rotation off by default
    bool m_bgPatternJitter = false;       // random grid offset jitter
    double m_bgPatternGridAmplitude = 0.20;  // sine-wave jitter amplitude (0.0-0.5)
    bool m_bgPatternMixEnabled = false;
    QList<int> m_bgPatternMixMotifs;  // indices into motif list for mix mode

    QImage m_blurBuf;               // pre-allocated temp buffer for blur passes

    // ── Cached pattern thumbnails ──
    mutable QHash<int, QString> m_geometricThumbnailCache;
    mutable QHash<int, QString> m_motifThumbnailCache;
    mutable QHash<int, QString> m_svgGeoThumbnailCache;

    bool processSingleImage(const QString &sourcePath, QString &outPath);
    QImage renderWallpaper(const QImage &src, int W, int H);
    QColor extractAverageColor(const QImage &image);
    QPair<QColor, QColor> extractHarmonizedColors(const QImage &image, int mood = 0);
    void computeMoodPalettes(const QImage &image);
    void computeMoodPalettesV2(const QImage &image);

    /// 3D RGB histogram centroid (for V2)
    struct Centroid3D {
        double r, g, b;
        double score;
        int ri, gi, bi;
        int count;
    };

    /// Scale source image down if both dimensions exceed the target (2/5 iteration)
    static QImage limitImageSize(const QImage &src, int maxW, int maxH);

    /// Gaussian blur via 3-pass box blur approximation (O(n), radius-independent)
    static void stackBlur(QImage &image, double sigma);
    /// Multi-threaded horizontal box blur pass
    static void boxBlurH(QImage &dst, const QImage &src, int radius);
    /// Multi-threaded vertical box blur pass
    static void boxBlurV(QImage &dst, const QImage &src, int radius);
    static void boostSaturation(QImage &image, double factor);
    /// Ensure noise texture is allocated (lazy init)
    static void ensureNoiseTexture(int w, int h);
    static QImage s_noiseTexture;       // shared pre-generated noise

    /// Aspect ratios indexed by mode. 0=Free (0.0), 1=1:1, 2=4:3, 3=16:9, 4=16:10, 5=21:9, 6=32:9
    static constexpr double s_aspectRatios[7] = {0.0, 1.0, 4.0/3.0, 16.0/9.0, 16.0/10.0, 21.0/9.0, 32.0/9.0};

    /// Gradient preset data
    struct GradientPreset {
        const char *name;   // i18n key
        QRgb color1;
        QRgb color2;
    };
    static const GradientPreset s_presets[12];

    // ── Pattern constants ──
    static constexpr int GEOMETRIC_PATTERN_COUNT = 8;
    static constexpr int MOTIF_OFFSET = 100;
    static constexpr int MOTIF_PATTERN_COUNT = 88;
    static constexpr int SVG_GEO_OFFSET = 50;
    static constexpr int SVG_GEO_COUNT = 16;

    // ── Pattern generation ──
    /// Main entry: render a pattern onto the full output canvas
    void renderPatternBackground(QPainter &p, int W, int H);

    /// Generate a tile for a geometric pattern
    QImage generateGeometricTile(int type, int tileSize, const QColor &bg, const QColor &fg, double scale);

    /// Generate a tile for an SVG geometric primitive
    QImage generateSvgGeoTile(int index, int tileSize, const QColor &bg, const QColor &fg, double scale);

    /// Generate a tile for a motif (icon) pattern
    QImage generateMotifTile(int motifIndex, int tileSize, const QColor &bg, const QColor &fg, double scale);

    /// Generate a tile for mixed motifs
    QImage generateMixedTile(const QList<int> &motifIndices, int tileSize, const QColor &bg, const QColor &fg, double scale);

    /// Build a QPainterPath for a motif icon (normalized to 100x100 viewport)
    static QPainterPath buildMotifPath(int index);

    /// Render a single icon path onto a canvas at a given position and size
    static void renderIcon(QPainter &p, const QPainterPath &path,
                           double cx, double cy, double size, const QColor &color);

    /// Generate a thumbnail image for a pattern type (used in UI)
    QImage generatePatternThumbnail(int type, int thumbSize) const;

    /// Render a single motif icon to a small image for thumbnails
    QImage renderMotifIcon(int index, int size) const;

    /// Render gradient or solid background (shared by pattern and gradient modes)
    void renderGradientOrSolid(QPainter &p, QImage &output, const QImage &src, int W, int H);

    /// Render pattern tiles on top of background, sampling per-tile for adaptive coloring
    void renderAdaptivePattern(QPainter &p, const QImage &background, int W, int H);

    /// Helper: tile an image across the output
    static void tileImage(QPainter &p, const QImage &tile, int W, int H);

    /// Geometric pattern names
    static const char *s_geometricNames[GEOMETRIC_PATTERN_COUNT];
    /// SVG geometric primitive names
    static const char *s_svgGeoNames[SVG_GEO_COUNT];
    /// Motif pattern names (in categories: animals, critters, nature, music, celestial, whimsical)
    static const char *s_motifNames[MOTIF_PATTERN_COUNT];
    /// Motif category mapping (0=animals, 1=nature, 2=music, 3=celestial, 4=whimsical)
    static const int s_motifCategories[MOTIF_PATTERN_COUNT];
    /// SVG resource filenames for each motif (without extension)
    static const char *s_motifSvgFiles[MOTIF_PATTERN_COUNT];
    /// Category names
    static const char *s_categoryNames[6];
};

#endif // WALLPAPERPROCESSOR_H
