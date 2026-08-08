#!/bin/bash
# SPDX-FileCopyrightText: 2026 kirijin <avel.ronin@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
# Build Walltz as an AppImage
# Usage: bash scripts/build-appimage.sh
# Requires: docker or podman (runs build in walltz-dev container)

set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${PROJECT_DIR}/build-appimage"
APPDIR="${BUILD_DIR}/AppDir"
VERSION="$(grep 'set(VERSION' "${PROJECT_DIR}/CMakeLists.txt" | sed 's/.*"\(.*\)".*/\1/')"

echo "=== Building Walltz AppImage (v${VERSION}) ==="
echo "  Project: $PROJECT_DIR"

# Step 1: Build the app inside the dev container
mkdir -p "${BUILD_DIR}"
distrobox enter walltz-dev -- bash -c "
    cd '${PROJECT_DIR}/build-appimage' || exit 1
    cmake '${PROJECT_DIR}' \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DCMAKE_SKIP_RPATH=ON \
        -DCMAKE_SKIP_INSTALL_RPATH=ON
    make -j\$(nproc)
    DESTDIR='${APPDIR}' make install
"

# Step 2: Download linuxdeploy if not cached
LINUXDEPLOY="${BUILD_DIR}/linuxdeploy-x86_64.AppImage"
if [ ! -f "$LINUXDEPLOY" ]; then
    echo "  Downloading linuxdeploy..."
    curl -sLo "$LINUXDEPLOY" \
        "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
    chmod +x "$LINUXDEPLOY"
fi

LINUXDEPLOY_QT="${BUILD_DIR}/linuxdeploy-plugin-qt-x86_64.AppImage"
if [ ! -f "$LINUXDEPLOY_QT" ]; then
    echo "  Downloading linuxdeploy-qt-plugin..."
    curl -sLo "$LINUXDEPLOY_QT" \
        "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage"
    chmod +x "$LINUXDEPLOY_QT"
fi

# Step 3: Run linuxdeploy to bundle dependencies
export LDAI_OUTPUT="${BUILD_DIR}/Walltz-${VERSION}-x86_64.AppImage"
export QML_SOURCES_PATHS="${PROJECT_DIR}/src"

# Copy icon into AppDir for linuxdeploy's icon detection
ICON_DEST="${APPDIR}/usr/share/icons/hicolor/scalable/apps/org.walltz.walltz.svg"
if [ -f "$ICON_DEST" ]; then
    mkdir -p "${APPDIR}/usr/share/icons/hicolor/256x256/apps"
    # linuxdeploy prefers PNG icons; convert SVG if rsvg-convert is available
    if command -v rsvg-convert &>/dev/null; then
        rsvg-convert -w 256 -h 256 "$ICON_DEST" \
            -o "${APPDIR}/usr/share/icons/hicolor/256x256/apps/org.walltz.walltz.png"
    fi
fi

echo "  Running linuxdeploy..."
# Set up environment for linuxdeploy
export LDAI_OUTPUT="${LDAI_OUTPUT}"
export APPIMAGE_EXTRACT_AND_RUN=1

"${LINUXDEPLOY}" --appdir="${APPDIR}" \
    --desktop-file="${APPDIR}/usr/share/applications/org.walltz.walltz.desktop" \
    --icon-file="${APPDIR}/usr/share/icons/hicolor/256x256/apps/org.walltz.walltz.png" \
    --output=appimage 2>&1 | tee "${BUILD_DIR}/linuxdeploy.log"

echo ""
echo "=== DONE ==="
echo "  AppImage: ${LDAI_OUTPUT}"
ls -lh "${LDAI_OUTPUT}" 2>/dev/null || echo "  (check build-appimage/ for output)"
