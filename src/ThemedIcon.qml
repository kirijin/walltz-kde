// SPDX-FileCopyrightText: 2026 kirijin <avel.ronin@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Effects
import org.kde.kirigami as Kirigami

Item {
    property alias source: icon.source
    property color colorType: Kirigami.Theme.textColor
    property int iconSize: 24

    implicitWidth: iconSize
    implicitHeight: iconSize

    Image {
        id: icon
        anchors.centerIn: parent
        width: iconSize
        height: iconSize
        fillMode: Image.PreserveAspectFit
        // opacity:0 keeps the texture alive for MultiEffect while invisible.
        // visible:false causes Qt to skip texture allocation, giving MultiEffect
        // a stale/empty source during layout animations → glitch.
        visible: true
        opacity: 0.0
    }

    MultiEffect {
        source: icon
        anchors.centerIn: parent
        width: iconSize
        height: iconSize
        colorization: 1.0
        colorizationColor: parent.colorType
    }
}
