// SPDX-FileCopyrightText: 2026 kirijin <avel.ronin@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: root

    property string title
    property bool expanded: false
    property bool collapsible: true
    default property alias content: contentColumn.data

    spacing: 0

    // ── Header ──
    Item {
        id: headerArea
        Layout.fillWidth: true
        implicitHeight: headerText.implicitHeight + Kirigami.Units.smallSpacing * 2

        readonly property bool _hovered: headerMouse.containsMouse || headerMouse.pressed

        Rectangle {
            anchors.fill: parent
            radius: Kirigami.Units.cornerRadius
            color: headerArea._hovered
                   ? Kirigami.Theme.hoverColor
                   : "transparent"
            Behavior on color { ColorAnimation { duration: 100 } }
        }

        RowLayout {
            anchors.centerIn: parent
            spacing: Kirigami.Units.smallSpacing

            Controls.Label {
                text: root.expanded ? "▼" : "▶"
                font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                color: Kirigami.Theme.textColor
                opacity: 0.6
            }

            Controls.Label {
                id: headerText
                text: root.title
                font.weight: Font.DemiBold
                color: Kirigami.Theme.textColor
            }
        }

        MouseArea {
            id: headerMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                if (root.collapsible) {
                    root.expanded = !root.expanded;
                }
            }
        }
    }

    // ── Content with collapse animation ──
    Item {
        id: contentWrapper
        Layout.fillWidth: true
        clip: true

        // _animH is the animated height — separate from implicitHeight so we
        // capture the settled content height *after* layout, then animate to a
        // fixed target. Binding directly to implicitHeight causes the animation
        // target to move as content lays out mid-animation → bounce.
        property real _animH: 0

        implicitHeight: contentColumn.implicitHeight + (contentColumn.implicitHeight > 0 ? Kirigami.Units.smallSpacing : 0)
        height: _animH
        visible: root.expanded || _animH > 0

        Behavior on _animH {
            NumberAnimation {
                duration: 150
                easing.type: Easing.OutQuad
            }
        }

        ColumnLayout {
            id: contentColumn
            anchors.left: parent.left
            anchors.right: parent.right
            spacing: Kirigami.Units.smallSpacing
        }

        Component.onCompleted: {
            if (root.expanded)
                _animH = contentColumn.implicitHeight
                    + (contentColumn.implicitHeight > 0 ? Kirigami.Units.smallSpacing : 0)
        }

        Connections {
            target: root
            function onExpandedChanged() {
                if (root.expanded) {
                    // Defer one cycle so content layout settles, then
                    // animate to the captured height — no moving target.
                    Qt.callLater(function() {
                        var h = contentColumn.implicitHeight
                            + (contentColumn.implicitHeight > 0 ? Kirigami.Units.smallSpacing : 0)
                        if (h > 0)
                            contentWrapper._animH = h
                    })
                } else {
                    contentWrapper._animH = 0
                }
            }
        }
    }
}
