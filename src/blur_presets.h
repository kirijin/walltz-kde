#ifndef BLUR_PRESETS_H
#define BLUR_PRESETS_H

#include <QString>
#include <QColor>

/// Parameter set for a named blur preset.
/// Each entry drives the existing pipeline (sigma, sat, dither) + overlay + brightness.
struct BlurConfig {
    const char *id;          // "default", "apple", "gnome", "mica", "acrylic", "reddit"
    const char *displayName; // "Default", "Apple", "GNOME", "Mica", "Acrylic", "Reddit"

    double sigma        = 47.5;
    double satBoost     = 1.8;
    double brightness   = 1.0;
    double overlayOpacity = 0.0;   // 0-1, 0 = no tint overlay
    QRgb  overlayColor  = 0;       // sRGB, unused when overlayOpacity == 0
    double vignette     = 0.0;
    double grain        = 0.0;

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
