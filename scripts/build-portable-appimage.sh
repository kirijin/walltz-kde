#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 kirijin <avel.ronin@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
# Build a portable Walltz AppImage in a clean stock Fedora container
# This produces an AppImage with stock Fedora Qt6 that works on Fedora 44+
set -euo pipefail

PROJECT_DIR="$(realpath "$(dirname "$0")/..")"
BUILD_DIR="${PROJECT_DIR}/build-appimage"
CONTAINER_TAG="walltz-builder:fedora44"

cd "${PROJECT_DIR}"

echo "=== Step 1: Build container image ==="
podman build -t "${CONTAINER_TAG}" -f- . <<'DOCKERFILE'
FROM fedora:44
RUN dnf install -y \
    cmake ninja-build gcc-c++ \
    qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtsvg-devel qt6-qtwayland-devel \
    qt6-qtwebsockets-devel \
    kf6-kirigami-devel kf6-ki18n-devel kf6-kcoreaddons-devel \
    kf6-kiconthemes-devel kf6-kconfigwidgets-devel \
    kf6-kguiaddons-devel kf6-kcolorscheme-devel kf6-karchive-devel \
    extra-cmake-modules \
    kf6-breeze-icons \
    file fuse3-libs fuse3 which \
    && dnf clean all
DOCKERFILE

echo "=== Step 2: Build + package AppImage in one pass ==="
mkdir -p "${BUILD_DIR}"

podman run --rm -i \
    --privileged \
    -v "${PROJECT_DIR}:/src:Z" \
    -v "${BUILD_DIR}:/build:Z" \
    -w /src \
    "${CONTAINER_TAG}" \
    bash <<'BUILDSCRIPT'
set -euo pipefail

cd /src
echo "--- Building walltz ---"
mkdir -p /tmp/build && cd /tmp/build
cmake /src -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=/usr -GNinja
ninja -j$(nproc)
DESTDIR=/build/AppDir ninja install
cd /src

echo "--- Downloading linuxdeploy ---"
curl -sL -o /tmp/linuxdeploy-x86_64.AppImage \
    "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
curl -sL -o /tmp/linuxdeploy-plugin-qt-x86_64.AppImage \
    "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage"
chmod +x /tmp/linuxdeploy*.AppImage

echo "--- Running linuxdeploy ---"
export LDAI_DIR="/build/AppDir"
export UPDINFO=""

# Create qt.conf for relative path resolution
mkdir -p /build/AppDir/usr/plugins /build/AppDir/usr/qml

# NO_STRIP=1 avoids strip errors on Fedora 44's .relr.dyn sections
NO_STRIP=1 /tmp/linuxdeploy-x86_64.AppImage \
    --appdir /build/AppDir \
    --plugin qt \
    --output appimage \
    --desktop-file /build/AppDir/usr/share/applications/org.walltz.walltz.desktop \
    -v0 2>&1 || true
echo "--- Result ---"
APPIMAGE=$(ls /build/Walltz-*.AppImage 2>/dev/null | head -1)
if [ -z "$APPIMAGE" ]; then
    echo "linuxdeploy output plugin failed — running appimagetool directly"
    wget -q "https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage" -O /tmp/appimagetool-x86_64.AppImage
    chmod +x /tmp/appimagetool-x86_64.AppImage
    /tmp/appimagetool-x86_64.AppImage /build/AppDir
    APPIMAGE=$(ls /build/Walltz-*.AppImage 2>/dev/null | head -1)
fi
ls -lh "$APPIMAGE"
BUILDSCRIPT

echo ""
echo "=== Step 3: Verify ==="
ls -lh "${BUILD_DIR}/Walltz-0.1.0-x86_64.AppImage" 2>/dev/null \
  && echo "AppImage built: ${BUILD_DIR}/Walltz-0.1.0-x86_64.AppImage" \
  || echo "No AppImage produced — check build log"
