#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 kirijin <avel.ronin@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
# Verify walltz features are correctly wired
# Run from host (not inside distrobox)
set -euo pipefail
ROOT="/var/home/pavel/src/walltz/walltz"
PASS=0; FAIL=0
ok() { msg="  [PASS] $1"; PASS=$((PASS+1)); echo "$msg"; }
fail() { msg="  [FAIL] $1"; FAIL=$((FAIL+1)); echo "$msg"; }

echo "=== walltz feature verification ==="

# ── Build (run via distrobox) ──
echo "--- Build ---"
BUILD_OUTPUT=$(distrobox enter walltz-dev -- bash -c "cd '${ROOT}/build' && make -j\$(nproc) 2>&1") || true
if echo "$BUILD_OUTPUT" | grep -q "Built target walltz"; then
    ok "Build succeeds"
else
    echo "   Output: $BUILD_OUTPUT" | tail -5
    fail "Build failed"; exit 1
fi

# ── 1. ThemedIcon (MultiEffect theming) ──
echo "--- 1. ThemedIcon ---"
grep -q 'MultiEffect {' "${ROOT}/src/ThemedIcon.qml" && ok "MultiEffect standalone block" \
  || fail "No standalone MultiEffect"
grep -q 'visible: false' "${ROOT}/src/ThemedIcon.qml" && ok "Source Image hidden (visible: false)" \
  || fail "Source Image not hidden"
grep -q 'colorization: 1.0' "${ROOT}/src/ThemedIcon.qml" && ok "colorization: 1.0 (double)" \
  || fail "colorization not 1.0"
grep -q 'anchors.centerIn: parent' "${ROOT}/src/ThemedIcon.qml" && ok "Icon centered, not fill" \
  || fail "Icon not centerIn"
grep -q 'iconSize' "${ROOT}/src/ThemedIcon.qml" && ok "Icon size uses iconSize property" \
  || fail "Icon size not 24"

# ── 2. Swap button ──
echo "--- 2. Swap button ---"
grep -q 'source: "qrc:/icons/swap.svg"' "${ROOT}/src/Main.qml" && ok "Swap icon bound" \
  || fail "Swap icon missing"
grep -q 'processor.aspectMode = 0' "${ROOT}/src/Main.qml" && ok "Swap resets aspectMode" \
  || fail "Swap doesn't reset aspect mode"

# ── 3. Crossfade preview ──
echo "--- 3. Crossfade preview ---"
grep -q 'id: previewA' "${ROOT}/src/Main.qml" && ok "previewA layer exists" \
  || fail "Missing previewA"
grep -q 'id: previewB' "${ROOT}/src/Main.qml" && ok "previewB layer exists" \
  || fail "Missing previewB"
grep -q 'SequentialAnimation' "${ROOT}/src/Main.qml" && ok "SequentialAnimation crossfade" \
  || fail "No SequentialAnimation"
grep -q 'previewA.source = previewB.source' "${ROOT}/src/Main.qml" && ok "Post-fade: B copied to A" \
  || fail "Post-fade copy missing"
grep -q 'crossfadePreview' "${ROOT}/src/Main.qml" && ok "crossfadePreview function" \
  || fail "crossfadePreview missing"
! grep -q 'ParallelAnimation' "${ROOT}/src/Main.qml" && ok "No old ParallelAnimation" \
  || fail "ParallelAnimation still present"

# ── 4. Drop zone + ghost outline ──
echo "--- 4. Drop zone ---"
grep -q 'DropArea' "${ROOT}/src/Main.qml" && ok "DropArea present" \
  || fail "DropArea missing"
grep -qF 'border.color: dropArea.fileCount === 0' "${ROOT}/src/Main.qml" && ok "Conditional ghost border" \
  || fail "Ghost border not conditional"
grep -qF '"transparent"' "${ROOT}/src/Main.qml" && ok "Transparent fill when empty" \
  || fail "No transparent fill"

# ── 5. Braille spinner (not old LoadingPlaceholder) ──
echo "--- 5. Loading spinner ---"
grep -q '"⢸"' "${ROOT}/src/Main.qml" && ok "Braille spinner frames (literal Unicode)" \
  || fail "Braille frames missing"
grep -q 'visible: processor.busy' "${ROOT}/src/Main.qml" && ok "Spinner tied to processor.busy" \
  || fail "Spinner not tied to busy"
! grep -q 'Kirigami.LoadingPlaceholder' "${ROOT}/src/Main.qml" && ok "No old LoadingPlaceholder" \
  || fail "LoadingPlaceholder still present"

# ── 6. Blur (stackBlur + boxBlur O(n)) ──
echo "--- 6. Gaussian blur ---"
grep -q 'stackBlur' "${ROOT}/src/WallpaperProcessor.cpp" && ok "stackBlur implementation" \
  || fail "stackBlur missing"
grep -q 'boxBlurH' "${ROOT}/src/WallpaperProcessor.cpp" && ok "boxBlurH (parallel O(n))" \
  || fail "boxBlurH missing"
grep -q 'boxBlurV' "${ROOT}/src/WallpaperProcessor.cpp" && ok "boxBlurV (parallel O(n))" \
  || fail "boxBlurV missing"
grep -q 'sigmaToBoxes' "${ROOT}/src/WallpaperProcessor.cpp" && ok "sigmaToBoxes (3-pass Gaussian)" \
  || fail "sigmaToBoxes missing"
grep -q 'QtConcurrent::blockingMap' "${ROOT}/src/WallpaperProcessor.cpp" \
  && ok "QtConcurrent parallel blur" || fail "QtConcurrent missing"

# ── 7. Chromatic Aberration ──
echo "--- 7. Chromatic aberration ---"
grep -q 'caStrength' "${ROOT}/src/WallpaperProcessor.h" && ok "caStrength Q_PROPERTY" \
  || fail "caStrength not in header"
grep -q 'maxShift = m_caStrength' "${ROOT}/src/WallpaperProcessor.cpp" && ok "CA maxShift computed" \
  || fail "CA maxShift missing"

# ── 8. Vignette + Grain ──
echo "--- 8. Vignette & Grain ---"
grep -q 'vignetteStrength' "${ROOT}/src/WallpaperProcessor.h" && ok "vignetteStrength Q_PROPERTY" \
  || fail "vignetteStrength not in header"
grep -q 'grainStrength' "${ROOT}/src/WallpaperProcessor.h" && ok "grainStrength Q_PROPERTY" \
  || fail "grainStrength not in header"
grep -q 'QRadialGradient' "${ROOT}/src/WallpaperProcessor.cpp" && ok "Vignette uses radial gradient" \
  || fail "Vignette radial gradient missing"
grep -q 'SoftLight' "${ROOT}/src/WallpaperProcessor.cpp" && ok "Grain uses SoftLight compositing" \
  || fail "Grain SoftLight missing"

# ── 9. Background rotation ──
echo "--- 9. Background rotation ---"
grep -q 'bgBlurAngle' "${ROOT}/src/WallpaperProcessor.h" && ok "bgBlurAngle Q_PROPERTY" \
  || fail "bgBlurAngle not in header"
grep -q 'onBgBlurAngleChanged' "${ROOT}/src/Main.qml" && ok "bgBlurAngle wired in QML" \
  || fail "bgBlurAngle callback missing"

# ── 10. Mood palettes (V1 + V2) ──
echo "--- 10. Mood palettes ---"
grep -q 'computeMoodPalettes' "${ROOT}/src/WallpaperProcessor.cpp" && ok "computeMoodPalettes V1" \
  || fail "computeMoodPalettes missing"
grep -q 'computeMoodPalettesV2' "${ROOT}/src/WallpaperProcessor.cpp" && ok "computeMoodPalettes V2" \
  || fail "computeMoodPalettesV2 missing"
grep -q 'moodColorV2A' "${ROOT}/src/WallpaperProcessor.cpp" && ok "moodColorV2A accessor" \
  || fail "moodColorV2A missing"
grep -q 'moodNameV2' "${ROOT}/src/WallpaperProcessor.cpp" && ok "moodNameV2 accessor" \
  || fail "moodNameV2 missing"
! grep -q 'static const char.*fallback' "${ROOT}/src/WallpaperProcessor.cpp" && ok "No stale fallback hex colors" \
  || fail "Stale fallback hex colors still present"

# ── 11. Gradient presets ──
echo "--- 11. Gradient presets ---"
grep -q 'QT_TRANSLATE_NOOP' "${ROOT}/src/WallpaperProcessor.cpp" && ok "Gradient names i18n-marked" \
  || fail "Gradient names not i18n-marked"

# ── 12. Photo frame ──
echo "--- 12. Photo frame ---"
grep -q 'Q_PROPERTY.*photoFrameWidth' "${ROOT}/src/WallpaperProcessor.h" && ok "photoFrameWidth Q_PROPERTY" \
  || fail "photoFrameWidth not in header"
grep -q 'FRAME_RADIUS' "${ROOT}/src/WallpaperProcessor.cpp" && ok "FRAME_RADIUS constant" \
  || fail "FRAME_RADIUS missing"
grep -q 'Antialiasing' "${ROOT}/src/WallpaperProcessor.cpp" && ok "Antialiasing for smooth corners" \
  || fail "Antialiasing missing"
grep -q 'std::min(W, H) / 500.0' "${ROOT}/src/WallpaperProcessor.cpp" && ok "Frame width proportional to output" \
  || fail "Frame width not proportional"

# ── 13. Aspect mode ratios (single source of truth) ──
echo "--- 13. Aspect ratios ---"
grep -q 's_aspectRatios' "${ROOT}/src/WallpaperProcessor.h" && ok "Single source-of-truth ratios" \
  || fail "s_aspectRatios missing"
HAVE_RATIOS=$(grep -c 'static const double ratios' "${ROOT}/src/WallpaperProcessor.cpp" || true)
[ "$HAVE_RATIOS" -eq 0 ] && ok "No duplicated ratios array in .cpp" \
  || fail "Duplicated ratios array still in .cpp"

# ── 14. Downscale DRY (limitImageSize helper) ──
echo "--- 14. DRY downscale ---"
grep -q 'limitImageSize' "${ROOT}/src/WallpaperProcessor.h" && ok "limitImageSize declared" \
  || fail "limitImageSize not declared"
DUPE_COUNT=$(grep -c 'imgW = imgW \* 2 / 5' "${ROOT}/src/WallpaperProcessor.cpp" || true)
[ "$DUPE_COUNT" -eq 1 ] && ok "Downscale code appears only once (in limitImageSize)" \
  || fail "Downscale code still duplicated ($DUPE_COUNT occurrences)"

# ── 15. boostSaturation endian-safe ──
echo "--- 15. Saturation boost ---"
grep -q 'qRed(px)' "${ROOT}/src/WallpaperProcessor.cpp" && ok "Saturation uses qRed/qGreen/qBlue (endian-safe)" \
  || fail "Saturation still uses byte-offset access"
! grep -q '// B,G,R,A on little-endian' "${ROOT}/src/WallpaperProcessor.cpp" && ok "No x86_64-specific comment" \
  || fail "Endian-specific comment still present"

# ── 16. Reset effects button ──
echo "--- 16. Reset effects ---"
grep -q 'Reset all effects' "${ROOT}/src/Main.qml" && ok "Reset effects button (tooltip)" \
  || fail "Reset effects missing"

# ── 17. CollapsibleSection ──
echo "--- 17. Collapsible sections ---"
grep -q 'property bool expanded' "${ROOT}/src/CollapsibleSection.qml" && ok "CollapsibleSection expanded property" \
  || fail "CollapsibleSection missing"

# ── 18. No stale components ──
echo "--- 18. Stale component audit ---"
! grep -q 'Controls.SpinBox' "${ROOT}/src/Main.qml" && ok "No SpinBox inputs" \
  || fail "SpinBox still present"
! grep -q 'Kirigami.Separator' "${ROOT}/src/Main.qml" && ok "No Kirigami.Separator" \
  || fail "Separator still present"
! grep -q 'wp_colorFromCentroid' "${ROOT}/src/WallpaperProcessor.h" && ok "wp_colorFromCentroid not in header" \
  || fail "wp_colorFromCentroid leaks to header"

# ── 19. Connections merged ──
echo "--- 19. Connections ---"
CONN_COUNT=$(grep -c 'Connections {' "${ROOT}/src/Main.qml" || true)
[ "$CONN_COUNT" -eq 1 ] && ok "Single Connections block (merged)" \
  || fail "Multiple Connections blocks ($CONN_COUNT)"

# ── 20. CMakeLists flags ──
echo "--- 20. CMake build flags ---"
grep -q 'march=native' "${ROOT}/src/CMakeLists.txt" && ok "-march=native SIMD" \
  || fail "-march=native missing"
grep -q 'INTERPROCEDURAL_OPTIMIZATION' "${ROOT}/src/CMakeLists.txt" && ok "LTO enabled" \
  || fail "LTO missing"
grep -q 'Qt6::Concurrent' "${ROOT}/src/CMakeLists.txt" && ok "Qt6::Concurrent linked" \
  || fail "Qt6::Concurrent missing"

# ── 21. Packaging files ──
echo "--- 21. Packaging ---"
[ -f "${ROOT}/flatpak/org.walltz.walltz.yml" ] && ok "Flatpak manifest exists" \
  || fail "Flatpak manifest missing"
[ -f "${ROOT}/scripts/build-appimage.sh" ] && ok "AppImage build script exists" \
  || fail "AppImage build script missing"
[ -f "${ROOT}/COPYING" ] && ok "LICENSE file (COPYING)" \
  || fail "LICENSE file missing"
[ -x "${ROOT}/scripts/build-appimage.sh" ] && ok "AppImage script executable" \
  || fail "AppImage script not executable"

echo
echo "Result: $PASS passed, $FAIL failed"
[ "$FAIL" -eq 0 ] && echo "All checks OK" || exit 1
