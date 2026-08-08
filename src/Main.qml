// SPDX-FileCopyrightText: 2026 kirijin <avel.ronin@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import QtQuick.Effects
import org.kde.kirigami as Kirigami
import org.walltz.processor 1.0

Kirigami.ApplicationWindow {
    id: root

    width: 720
    height: 720
    minimumWidth: leftColumn.Layout.minimumWidth 
                   + rightColumn.Layout.minimumWidth 
                   + mainPage.leftPadding + mainPage.rightPadding
                   + Kirigami.Units.smallSpacing * 2 + Kirigami.Units.largeSpacing
    minimumHeight: 600

    maximumWidth: minimumWidth

    Component.onCompleted: width = minimumWidth

    title: i18nc("@title:window", "Walltz")

    pageStack.initialPage: Kirigami.Page {
        id: mainPage

        title: i18n("Walltz")

        actions: [
            Kirigami.Action {
                text: i18n("Walltz it")
                icon.name: "document-save"
                displayHint: Kirigami.DisplayHint.KeepVisible
                enabled: dropArea.fileCount > 0
                         && widthInput.length > 0 && heightInput.length > 0
                         && !processor.busy
                onTriggered: processor.processQueue(dropArea.filePaths)
            },
            Kirigami.Action {
                text: i18n("Set as wallpaper")
                icon.name: "preferences-desktop-wallpaper"
                displayHint: Kirigami.DisplayHint.KeepVisible
                enabled: dropArea.fileCount === 1
                         && widthInput.length > 0 && heightInput.length > 0
                         && !processor.busy
                Kirigami.Action {
                    text: i18n("Desktop")
                    onTriggered: processor.processAndSetWallpaper(dropArea.filePaths[0], 0)
                }
                Kirigami.Action {
                    text: i18n("Lockscreen")
                    onTriggered: processor.processAndSetWallpaper(dropArea.filePaths[0], 1)
                }
                Kirigami.Action {
                    text: i18n("Both")
                    onTriggered: processor.processAndSetWallpaper(dropArea.filePaths[0], 2)
                }
            },
            Kirigami.Action {
                text: i18n("Undo tweaks")
                icon.name: "edit-undo"
                enabled: dropArea.fileCount > 0
                onTriggered: {
                    processor.restoreState()
                    previewDebounce.restart()
                }
            },
            Kirigami.Action {
                text: i18n("Presets")
                icon.name: "bookmarks"
                displayHint: Kirigami.DisplayHint.KeepVisible
                onTriggered: presetDialog.open()
            },
            Kirigami.Action {
                text: i18n("Quit")
                icon.name: "application-exit"
                shortcut: "Ctrl+Q"
                onTriggered: Qt.quit()
            }
        ]

        Kirigami.Theme.colorSet: Kirigami.Theme.View

        Component.onCompleted: {
            processor.detectScreenSize();
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: Kirigami.Units.smallSpacing
            anchors.margins: Kirigami.Units.smallSpacing

            // === Drop area / preview ===
            Item {
                id: previewBox
                Layout.fillWidth: true
                Layout.leftMargin: Kirigami.Units.gridUnit
                Layout.rightMargin: Kirigami.Units.gridUnit
                Layout.minimumHeight: 100

                readonly property double _ar: processor.targetWidth / Math.max(1, processor.targetHeight)
                Layout.preferredHeight: root.height * 0.40

                readonly property color _canvasColor: Kirigami.Theme.colorScheme === Kirigami.Theme.Dark
                    ? Qt.darker(Kirigami.Theme.backgroundColor, 2.5)
                    : Qt.darker(Kirigami.Theme.backgroundColor, 3.0)
                Rectangle {
                    id: dropZone
                    anchors.centerIn: parent
                    layer.enabled: true
                    layer.effect: MultiEffect {
                        shadowEnabled: true
                        shadowColor: Qt.rgba(0, 0, 0, 0.4)
                        shadowBlur: 16
                        shadowHorizontalOffset: 0
                        shadowVerticalOffset: 4
                        shadowOpacity: 0.7
                    }

                    readonly property double _scale: Math.min(
                        parent.width / previewBox._ar,
                        parent.height
                    )
                    width: previewBox._ar * _scale
                    height: _scale

                    radius: Kirigami.Units.cornerRadius
                    color: dropArea.containsDrag ? Kirigami.Theme.highlightColor
                          : (dropArea.fileCount === 0 ? "transparent" : previewBox._canvasColor)
                    border.color: dropArea.fileCount === 0 ? Kirigami.Theme.disabledTextColor : "transparent"
                    border.width: dropArea.fileCount === 0 ? 1 : 0
                    Behavior on color { ColorAnimation { duration: 150 } }

                    DropArea {
                        id: dropArea
                        anchors.fill: parent
                        property int fileCount: 0
                        property var filePaths: []
                        property var fileList: []  // [{path, previewUrl}]

                        onDropped: function (drop) {
                            if (drop.hasUrls && drop.urls.length > 0) {
                                // F6: anchor the undo state before any tweaking
                                processor.rememberState();
                                var entries = [];
                                var paths = [];
                                for (var i = 0; i < drop.urls.length; ++i) {
                                    var urlStr = drop.urls[i].toString();
                                    if (urlStr.startsWith("file://"))
                                        urlStr = decodeURIComponent(urlStr.substring(7));
                                    if (urlStr.length > 0) {
                                        paths.push(urlStr);
                                        var pv = processor.generatePreview(urlStr);
                                        entries.push({path: urlStr, previewUrl: pv});
                                    }
                                }
                                fileList = entries;
                                filePaths = paths;
                                fileCount = paths.length;
                                if (fileCount > 0)
                                    previewContainer._pendingRenders = fileCount
                                previewList.model = fileList;
                                if (fileList.length > 0)
                                    previewA.source = fileList[0].previewUrl;
                                drop.accept();
                            }
                        }
                        onEntered: function (drag) { if (drag.hasUrls) drag.accept(); }
                    }

                    Item {
                        id: previewContainer
                        anchors.fill: parent
                        visible: dropArea.fileCount > 0

                        property int _pendingRenders: 0
                        readonly property bool renderInProgress: _pendingRenders > 0

                        Image {
                            id: previewA
                            anchors.fill: parent
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                            asynchronous: true
                            opacity: 1.0
                        }

                        Image {
                            id: previewB
                            anchors.fill: parent
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                            asynchronous: true
                            opacity: 0.0
                        }

                        // ── Braille loading indicator ──
                        Rectangle {
                            id: brailleBox
                            anchors.centerIn: parent
                            width: 80; height: 80
                            radius: 12
                            color: Qt.rgba(0, 0, 0, 0.55)
                            visible: previewContainer.renderInProgress

                            property int _idx: 0
                            readonly property var _chars: [
                                "\u280B", "\u2819", "\u2839", "\u2838", "\u283C",
                                "\u2834", "\u2826", "\u2827", "\u2807", "\u280F"
                            ]

                            Text {
                                anchors.centerIn: parent
                                text: brailleBox._chars[brailleBox._idx]
                                font.pixelSize: 28
                                font.family: "monospace"
                                color: "white"
                            }

                            Timer {
                                interval: 100; running: brailleBox.visible; repeat: true
                                onTriggered: {
                                    brailleBox._idx = (brailleBox._idx + 1) % brailleBox._chars.length
                                }
                            }
                        }

                        // ── Crossfade animation ──
                        SequentialAnimation {
                            id: fadeAnim
                            NumberAnimation { target: previewB; property: "opacity"; to: 1.0; duration: 200 }
                            ScriptAction {
                                script: {
                                    previewA.source = previewB.source;
                                    previewB.opacity = 0.0;
                                    previewB.source = "";
                                }
                            }
                        }
                    }

                    Item {
                        anchors.centerIn: parent
                        visible: dropArea.fileCount === 0 && !dropArea.containsDrag

                        readonly property var _frames: [
                            "⠋", "⠙", "⠹", "⠸", "⠼",
                            "⠴", "⠦", "⠧", "⠇", "⠏"
                        ]
                        property int _frame: 0

                        Timer {
                            interval: 100; repeat: true
                            running: parent.visible
                            onTriggered: {
                                parent._frame = (parent._frame + 1) % parent._frames.length
                            }
                        }

                        ColumnLayout {
                            anchors.centerIn: parent
                            spacing: Kirigami.Units.smallSpacing

                            Controls.Label {
                                Layout.alignment: Qt.AlignHCenter
                                text: parent.parent._frames[parent.parent._frame]
                                font.pixelSize: Kirigami.Theme.defaultFont.pixelSize * 2
                                color: Kirigami.Theme.disabledTextColor
                            }
                            Controls.Label {
                                Layout.alignment: Qt.AlignHCenter
                                text: i18n("Drop image(s) here")
                                color: Kirigami.Theme.disabledTextColor
                            }
                        }
                    }

                    Controls.Label {
                        id: moreFilesLabel
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: Kirigami.Units.smallSpacing
                        anchors.horizontalCenter: parent.horizontalCenter
                        visible: dropArea.fileCount > 1
                        text: i18n("+ %1 more files", dropArea.fileCount - 1)
                        color: Kirigami.Theme.disabledTextColor
                    }

                    // Processing overlay
                    Rectangle {
                        anchors.fill: parent
                        visible: processor.busy
                        color: Qt.rgba(0, 0, 0, 0.55)
                        radius: parent.radius
                        z: 10

                        ColumnLayout {
                            anchors.centerIn: parent
                            spacing: Kirigami.Units.smallSpacing

                            Kirigami.Heading {
                                text: i18n("Processing\u2026")
                                color: "white"
                                Layout.alignment: Qt.AlignHCenter
                            }

                            Controls.ProgressBar {
                                // Determinate when a real queue is running (H6),
                                // indeterminate only as a fallback.
                                indeterminate: processor.queueSize === 0
                                from: 0
                                to: processor.queueSize
                                value: processor.queueProgress
                                Layout.fillWidth: true
                                Layout.preferredWidth: 200
                            }
                        }
                    }
                }
            }

            // === Resolution row ===
            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                Item { Layout.fillWidth: true }

                // Braille processing indicator
                Controls.Label {
                    id: processingIndicator
                    visible: processor.busy
                    text: "⢸"
                    font.pixelSize: Kirigami.Theme.defaultFont.pixelSize
                    Layout.alignment: Qt.AlignVCenter
                    Timer {
                        interval: 80
                        running: processor.busy
                        repeat: true
                        property int frame: 0
                        readonly property var frames: [
                            "⢸", "⢹", "⢺", "⢻",
                            "⢼", "⢽", "⢾", "⢿"
                        ]
                        onTriggered: {
                            frame = (frame + 1) % frames.length;
                            parent.text = frames[frame];
                        }
                    }
                }

                Controls.TextField {
                    id: widthInput
                    Layout.preferredWidth: 80
                    inputMethodHints: Qt.ImhDigitsOnly
                    placeholderText: i18n("Width")
                    text: processor.targetWidth
                    validator: IntValidator { bottom: 1; top: 14999 }
                    onTextChanged: {
                        var v = parseInt(text);
                        if (!isNaN(v) && v > 0) processor.targetWidth = v;
                    }
                }
                Controls.ToolButton {
                    id: swapBtn
                    display: Controls.AbstractButton.IconOnly
                    contentItem: ThemedIcon { source: "qrc:/icons/swap.svg" }
                    hoverEnabled: true
                    Controls.ToolTip.text: i18n("Swap width and height")
                    Controls.ToolTip.visible: swapBtn.hovered
                    Controls.ToolTip.delay: 400
                    onClicked: {
                        processor.aspectMode = 0;  // disable auto-recalc before swap
                        var tmp = processor.targetWidth;
                        processor.targetWidth = processor.targetHeight;
                        processor.targetHeight = tmp;
                    }
                }
                Controls.TextField {
                    id: heightInput
                    Layout.preferredWidth: 80
                    inputMethodHints: Qt.ImhDigitsOnly
                    placeholderText: i18n("Height")
                    text: processor.targetHeight
                    validator: IntValidator { bottom: 1; top: 14999 }
                    onTextChanged: {
                        var v = parseInt(text);
                        if (!isNaN(v) && v > 0) processor.targetHeight = v;
                    }
                }

                Controls.ToolButton {
                    id: resetResBtn
                    text: i18n("Detect")
                    display: Controls.AbstractButton.IconOnly
                    contentItem: ThemedIcon { source: "qrc:/icons/reset-resolution.svg" }
                    hoverEnabled: true
                    Controls.ToolTip.text: i18n("Reset to screen resolution (%1\u00D7%2)",
                                       processor.screenWidth, processor.screenHeight)
                    Controls.ToolTip.visible: resetResBtn.hovered
                    onClicked: {
                        processor.aspectMode = 0;
                        processor.detectScreenSize();
                    }
                }
                Controls.ToolButton {
                    id: resetEffectsBtn
                    text: i18n("Effects")
                    display: Controls.AbstractButton.IconOnly
                    contentItem: ThemedIcon { source: "qrc:/icons/reset-effects.svg" }
                    hoverEnabled: true
                    Controls.ToolTip.text: i18n("Reset all effects")
                    Controls.ToolTip.visible: resetEffectsBtn.hovered
                    Controls.ToolTip.delay: 400
                    onClicked: {
                        processor.vignetteStrength = 0.0
                        processor.grainStrength = 0.0
                        processor.blurRadius = processor.defaultBlurRadius()
                        processor.saturationFactor = processor.defaultSaturation()
                        processor.bgZoom = processor.defaultBgZoom()
                        processor.bgBlurAngle = processor.defaultBgBlurAngle()
                        processor.gradientAngle = processor.defaultGradientAngle()
                        processor.caStrength = 0.0
                        processor.fgZoom = processor.defaultFgZoom()
                        processor.pipZoom = processor.defaultPipZoom()
                        processor.photoFrameWidth = processor.defaultPhotoFrameWidth()
                        processor.photoFrame = false
                        previewDebounce.restart()
                    }
                }

                Item { Layout.fillWidth: true }
            }

            // === Aspect ratio preset ===
            Controls.ButtonGroup { id: ratioGroup }

            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing
                Layout.bottomMargin: Kirigami.Units.smallSpacing

                Item { Layout.fillWidth: true }

                RowLayout {
                id: ratioRow
                spacing: Kirigami.Units.smallSpacing

                    // Generic zone
                    Controls.Button {
                        text: i18n("Free")
                        checkable: true
                        implicitWidth: Kirigami.Units.gridUnit * 4
                        highlighted: checked
                        Controls.ButtonGroup.group: ratioGroup
                        checked: processor.aspectMode === 0
                        onClicked: processor.aspectMode = 0
                    }
                    Controls.Button {
                        text: "1:1"
                        checkable: true
                        implicitWidth: Kirigami.Units.gridUnit * 4
                        highlighted: checked
                        Controls.ButtonGroup.group: ratioGroup
                        checked: processor.aspectMode === 1
                        onClicked: processor.aspectMode = 1
                    }
                    Controls.Button {
                        text: "4:3"
                        checkable: true
                        implicitWidth: Kirigami.Units.gridUnit * 4
                        highlighted: checked
                        Controls.ButtonGroup.group: ratioGroup
                        checked: processor.aspectMode === 2
                        onClicked: processor.aspectMode = 2
                    }
                    Controls.Button {
                        text: "16:10"
                        checkable: true
                        implicitWidth: Kirigami.Units.gridUnit * 4
                        highlighted: checked
                        Controls.ButtonGroup.group: ratioGroup
                        checked: processor.aspectMode === 4
                        onClicked: processor.aspectMode = 4
                    }

                    // Wide zone
                    Controls.Button {
                        text: "16:9"
                        checkable: true
                        implicitWidth: Kirigami.Units.gridUnit * 4
                        highlighted: checked
                        Controls.ButtonGroup.group: ratioGroup
                        checked: processor.aspectMode === 3
                        onClicked: processor.aspectMode = 3
                    }
                    Controls.Button {
                        text: "21:9"
                        checkable: true
                        implicitWidth: Kirigami.Units.gridUnit * 4
                        highlighted: checked
                        Controls.ButtonGroup.group: ratioGroup
                        checked: processor.aspectMode === 5
                        onClicked: processor.aspectMode = 5
                    }
                    Controls.Button {
                        text: "32:9"
                        checkable: true
                        implicitWidth: Kirigami.Units.gridUnit * 4
                        highlighted: checked
                        Controls.ButtonGroup.group: ratioGroup
                        checked: processor.aspectMode === 6
                        onClicked: processor.aspectMode = 6
                    }
                }

                Item { Layout.fillWidth: true }
            }

            // ── Two-column split (main + effects) ──
            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.largeSpacing

                ColumnLayout {
                    id: leftColumn
                    Layout.fillWidth: true
                    Layout.minimumWidth: Kirigami.Units.gridUnit * 28
                    Layout.preferredWidth: Kirigami.Units.gridUnit * 28
                    Layout.alignment: Qt.AlignTop
                    spacing: Kirigami.Units.smallSpacing

            // ── Mode toggle (always visible) ──
            Controls.ButtonGroup { id: modeGroup }

            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing
                Layout.bottomMargin: Kirigami.Units.smallSpacing

                Item { Layout.fillWidth: true }

                Controls.Button {
                    text: i18n("Blur")
                    checkable: true
                    implicitWidth: Kirigami.Units.gridUnit * 7
                    highlighted: checked
                    checked: processor.blurMode
                    onClicked: {
                        processor.blurMode = true
                        processor.bgPatternEnabled = false
                    }
                    Controls.ButtonGroup.group: modeGroup
                }
                Controls.Button {
                    text: i18n("Colour")
                    checkable: true
                    implicitWidth: Kirigami.Units.gridUnit * 7
                    highlighted: checked
                    checked: !processor.blurMode && !processor.bgPatternEnabled
                    onClicked: {
                        processor.blurMode = false
                        processor.bgPatternEnabled = false
                    }
                    Controls.ButtonGroup.group: modeGroup
                }
                Controls.Button {
                    text: i18n("Pattern")
                    checkable: true
                    implicitWidth: Kirigami.Units.gridUnit * 7
                    highlighted: checked
                    checked: !processor.blurMode && processor.bgPatternEnabled
                    onClicked: {
                        processor.blurMode = false
                        processor.bgPatternEnabled = true
                    }
                    Controls.ButtonGroup.group: modeGroup
                }

                Item { Layout.fillWidth: true }
            }

            // ── Blur preset buttons (visible when Blur mode active) ──
            RowLayout {
                visible: processor.blurMode
                Layout.fillWidth: true
                Layout.bottomMargin: Kirigami.Units.smallSpacing

                Item { Layout.fillWidth: true }

                Controls.ButtonGroup { id: blurPresetGroup }

                GridLayout {
                    columns: 5
                    columnSpacing: Kirigami.Units.smallSpacing
                    rowSpacing: Kirigami.Units.smallSpacing

                    Repeater {
                        model: processor.blurPresetNames

                        Controls.Button {
                            required property int index
                            required property string modelData

                            text: modelData
                            checkable: true
                            implicitWidth: Kirigami.Units.gridUnit * 7
                            highlighted: checked
                            Layout.fillWidth: true
                            Layout.maximumWidth: Kirigami.Units.gridUnit * 7
                            Layout.alignment: Qt.AlignHCenter
                            Controls.ButtonGroup.group: blurPresetGroup
                            checked: processor.blurPresetIndex === index
                            onClicked: {
                                processor.blurPresetIndex = index
                                previewDebounce.restart()
                            }
                        }
                    }
                }

                Item { Layout.fillWidth: true }
            }

            // ── Colour controls (outside accordion, above sliders) ──
            Controls.ButtonGroup { id: fillGroup }

            Timer {
                id: moodPreviewTimer
                interval: 200
                onTriggered: {
                    // Cancel the slower previewDebounce — we handle the update now
                    previewDebounce.stop()
                    if (dropArea.fileCount > 0 && dropArea.filePaths.length > 0) {
                        var url = processor.generatePreview(dropArea.filePaths[0])
                        if (url.length > 0)
                            crossfadePreview(url + "?t=" + Date.now())
                    }
                }
            }

            // Style toggle: Solid / Gradient / Auto
            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing
                visible: !processor.blurMode
                Layout.bottomMargin: Kirigami.Units.smallSpacing

                Item { Layout.fillWidth: true }

                Controls.Button {
                    text: i18n("Solid")
                    checkable: true
                    implicitWidth: Kirigami.Units.gridUnit * 7
                    highlighted: checked
                    checked: processor.bgGradientStyle === 0
                    onClicked: processor.bgGradientStyle = 0
                    Controls.ButtonGroup.group: fillGroup
                }
                Controls.Button {
                    text: i18n("Gradient")
                    checkable: true
                    implicitWidth: Kirigami.Units.gridUnit * 7
                    highlighted: checked
                    checked: processor.bgGradientStyle === 1
                    onClicked: processor.bgGradientStyle = 1
                    Controls.ButtonGroup.group: fillGroup
                }
                Controls.Button {
                    text: i18n("Mood")
                    checkable: true
                    implicitWidth: Kirigami.Units.gridUnit * 7
                    highlighted: checked
                    checked: processor.bgGradientStyle === 2
                    onClicked: processor.bgGradientStyle = 2
                    Controls.ButtonGroup.group: fillGroup
                }

                Item { Layout.fillWidth: true }
            }

            // Solid colour picker
            RowLayout {
                visible: !processor.blurMode && processor.bgGradientStyle === 0
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing
                Layout.bottomMargin: Kirigami.Units.smallSpacing

                Item { Layout.fillWidth: true }

                // Auto — star indicator
                Rectangle {
                    id: autoColorRect
                    implicitWidth: 28; implicitHeight: 28
                    radius: 4
                    border.width: processor.autoColor ? 2 : 1
                    border.color: processor.autoColor
                                   ? Kirigami.Theme.highlightColor
                                   : Kirigami.Theme.disabledTextColor
                    color: processor.autoColor && dropArea.fileCount > 0
                           ? processor.backgroundColor
                           : "transparent"

                    Controls.Label {
                        anchors.centerIn: parent
                        text: "\u2605"
                        font.pixelSize: Kirigami.Theme.defaultFont.pixelSize
                        color: autoColorRect.border.color
                    }

                    Controls.Button {
                        anchors.fill: parent
                        opacity: 0
                        onClicked: processor.autoColor = true
                    }
                }

                // Visual spacer
                Item {
                    implicitWidth: Kirigami.Units.smallSpacing
                    implicitHeight: 1
                }

                // Preset swatches
                Repeater {
                    model: [
                        "#d4c5a9","#b8b8c8","#a3c4a8",
                        "#a8c5d4","#c9b8b8","#b8b8d4","#888888","#555555","#333333"
                    ]

                    Rectangle {
                        required property string modelData

                        implicitWidth: 28; implicitHeight: 28
                        radius: 4
                        border.width: (!processor.autoColor
                                       && processor.backgroundColor.toString().toUpperCase() === modelData.toUpperCase())
                                      ? 2 : 1
                        border.color: (!processor.autoColor
                                       && processor.backgroundColor.toString().toUpperCase() === modelData.toUpperCase())
                                      ? Kirigami.Theme.highlightColor
                                      : Kirigami.Theme.textColor
                        color: modelData

                        Controls.Button {
                            anchors.fill: parent
                            opacity: 0
                            onClicked: {
                                processor.autoColor = false
                                processor.backgroundColor = modelData
                            }
                        }
                    }
                }

                // More button
                Controls.ToolButton {
                    text: "+"
                    implicitWidth: 28; implicitHeight: 28
                    font.bold: true
                    onClicked: colorDialog.open()
                    Controls.ToolTip.text: i18n("More colors\u2026")
                    Controls.ToolTip.visible: hovered
                    Controls.ToolTip.delay: 400
                }

                Item { Layout.fillWidth: true }
            }

            // Gradient preset picker (Gradient mode) — 6-col grid
            GridLayout {
                id: gradientGrid
                visible: !processor.blurMode && processor.bgGradientStyle === 1
                Layout.fillWidth: false
                Layout.alignment: Qt.AlignHCenter
                columns: 6
                columnSpacing: Kirigami.Units.smallSpacing
                rowSpacing: Kirigami.Units.smallSpacing
                Layout.bottomMargin: Kirigami.Units.smallSpacing

                Repeater {
                    model: processor.gradientPresetCount()

                    Rectangle {
                        id: presetDelegate
                        required property int index

                        Layout.preferredWidth: 56
                        Layout.preferredHeight: 40
                        radius: Kirigami.Units.cornerRadius
                        border.width: processor.bgGradientPreset === index ? 2 : 1
                        border.color: processor.bgGradientPreset === index
                                       ? Kirigami.Theme.highlightColor
                                       : Kirigami.Theme.textColor

                        gradient: Gradient {
                            GradientStop { position: 0.0; color: processor.gradientPresetColor1(presetDelegate.index) }
                            GradientStop { position: 1.0; color: processor.gradientPresetColor2(presetDelegate.index) }
                        }

                        Controls.Button {
                            anchors.fill: parent
                            opacity: 0
                            onClicked: processor.bgGradientPreset = index
                        }

                        Controls.Label {
                            anchors.bottom: parent.bottom
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.bottomMargin: 2
                            text: String(presetDelegate.index + 1)
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                            color: Kirigami.Theme.textColor
                            style: Text.Outline
                            styleColor: Kirigami.Theme.backgroundColor
                        }
                    }
                }
            }

            // Mood palette V1 (Auto mode)
            Controls.ButtonGroup { id: moodGroup }

            RowLayout {
                visible: !processor.blurMode && processor.bgGradientStyle === 2
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                Repeater {
                    model: 6
                    delegate: Controls.Button {
                        text: processor.moodName(index)
                        checkable: true
                        implicitWidth: Kirigami.Units.gridUnit * 7
                        highlighted: checked
                        Controls.ButtonGroup.group: moodGroup
                        checked: !processor.useV2 && processor.autoMood === index
                        onClicked: {
                            if (processor.autoMood === index && !processor.useV2)
                                return
                            processor.autoMood = index
                            processor.useV2 = false
                            moodPreviewTimer.restart()
                        }
                        Layout.fillWidth: true
                    }
                }
            }

            // Mood palette V2 (Auto mode)
            RowLayout {
                visible: !processor.blurMode && processor.bgGradientStyle === 2
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing
                Layout.bottomMargin: Kirigami.Units.smallSpacing

                Repeater {
                    model: 6
                    delegate: Controls.Button {
                        text: processor.moodNameV2(index)
                        checkable: true
                        implicitWidth: Kirigami.Units.gridUnit * 7
                        highlighted: checked
                        checked: processor.useV2 && processor.autoMood === index
                        onClicked: {
                            if (processor.autoMood === index && processor.useV2)
                                return
                            processor.autoMood = index
                            processor.useV2 = true
                            moodPreviewTimer.restart()
                        }
                        Layout.fillWidth: true
                    }
                }
            }

            // ── Pattern controls (visible when Pattern mode is active) ──
            // Visibility is !processor.blurMode && processor.bgPatternEnabled
            ColumnLayout {
                id: patternControls
                visible: !processor.blurMode && processor.bgPatternEnabled
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing
                property int patternCatIndex: 0

                // Category tabs — 2 rows of 4
                Controls.ButtonGroup { id: patternCatGroup }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 4
                    rowSpacing: Kirigami.Units.smallSpacing
                    columnSpacing: Kirigami.Units.smallSpacing
                    Layout.bottomMargin: Kirigami.Units.smallSpacing

                    Repeater {
                        model: 8  // geometric + tiled + 6 motif categories

                        Controls.Button {
                            required property int index

                            text: {
                                if (index === 0) return i18n("Geometric")
                                if (index === 1) return i18n("Tiled")
                                return processor.motifCategoryName(index - 2)
                            }
                            checkable: true
                            highlighted: checked
                            Layout.fillWidth: true
                            Layout.maximumWidth: Kirigami.Units.gridUnit * 7
                            Controls.ButtonGroup.group: patternCatGroup
                            checked: {
                                if (index === 0)
                                    return patternControls.patternCatIndex === 0
                                return patternControls.patternCatIndex === index
                            }
                            onClicked: {
                                patternControls.patternCatIndex = index
                            }
                            Layout.alignment: Qt.AlignHCenter
                        }
                    }
                }

                // Pattern grid + Mix mode header
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing
                    Layout.leftMargin: Kirigami.Units.smallSpacing
                    Layout.rightMargin: Kirigami.Units.smallSpacing

                    // Pattern-mode toggle row
                    RowLayout {
                        Layout.fillWidth: false
                        Layout.alignment: Qt.AlignHCenter
                        spacing: Kirigami.Units.smallSpacing
                        Layout.bottomMargin: Kirigami.Units.smallSpacing

                        Controls.Switch {
                            id: tiltSwitch
                            text: i18n("Tilt")
                            checked: processor.bgPatternRandomRotate
                            onCheckedChanged: {
                                processor.bgPatternRandomRotate = checked
                                previewDebounce.restart()
                            }
                        }

                        Controls.Switch {
                            id: organicSwitch
                            text: i18n("Organic")
                            checked: processor.bgPatternJitter
                            onCheckedChanged: {
                                processor.bgPatternJitter = checked
                                previewDebounce.restart()
                            }
                        }

                        Controls.Switch {
                            id: mixSwitch
                            text: i18n("Mix")
                            checked: processor.bgPatternMixEnabled
                            onCheckedChanged: {
                                processor.bgPatternMixEnabled = checked
                                previewDebounce.restart()
                            }
                        }
                        Controls.Label {
                            text: i18n("(%1 selected)", processor.mixMotifCount)
                            visible: mixSwitch.checked
                            color: Kirigami.Theme.disabledTextColor
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        }
                    }

                    Flickable {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 188  // 3 rows of 52px tiles + margins
                        contentHeight: patternGrid.height
                        clip: true
                        flickableDirection: Flickable.VerticalFlick

                        Grid {
                            id: patternGrid
                            spacing: Kirigami.Units.smallSpacing
                            columns: Math.max(1, Math.floor(parent.width / (52 + Kirigami.Units.smallSpacing)))
                            anchors.horizontalCenter: parent.horizontalCenter

                            // SVG geometric primitives (catIndex === 0)
                            Repeater {
                                id: svgGeoRepeater
                                model: patternControls.patternCatIndex === 0
                                       ? processor.svgGeoPatternCount() : 0

                                delegate: Rectangle {
                                    required property int index
                                    readonly property int typeIdx: processor.svgGeoOffset() + index

                                    implicitWidth: 52
                                    implicitHeight: 52
                                    radius: Kirigami.Units.cornerRadius
                                    border.width: {
                                        if (mixSwitch.checked) {
                                            var mixList = processor.bgPatternMixMotifs
                                            return mixList.indexOf(typeIdx) >= 0 ? 3 : 0
                                        }
                                        return processor.bgPatternType === typeIdx ? 3 : 1
                                    }
                                    border.color: {
                                        if (mixSwitch.checked) {
                                            var mixList = processor.bgPatternMixMotifs
                                            return mixList.indexOf(typeIdx) >= 0
                                                   ? Kirigami.Theme.highlightColor
                                                   : Kirigami.Theme.textColor
                                        }
                                        return processor.bgPatternType === typeIdx
                                               ? Kirigami.Theme.highlightColor
                                               : Kirigami.Theme.textColor
                                    }
                                    opacity: mixSwitch.checked ? (function() {
                                        var mixList = processor.bgPatternMixMotifs
                                        return mixList.indexOf(typeIdx) >= 0 ? 1.0 : 0.4
                                    })() : 1.0

                                    Image {
                                        anchors.fill: parent
                                        anchors.margins: 2
                                        source: processor.svgGeoPatternThumbnail(index, 48)
                                        fillMode: Image.PreserveAspectFit
                                        cache: false
                                        sourceSize.width: 48
                                        sourceSize.height: 48
                                    }

                                    Controls.Button {
                                        anchors.fill: parent
                                        opacity: 0
                                        onClicked: {
                                            if (mixSwitch.checked) {
                                                processor.toggleMixMotif(typeIdx)
                                            } else {
                                                processor.bgPatternType = typeIdx
                                            }
                                            previewDebounce.restart()
                                        }
                                    }
                                }
                            }

                            // Tiled patterns (catIndex === 1 — old geometric)
                            Repeater {
                                id: tiledRepeater
                                model: patternControls.patternCatIndex === 1
                                       ? processor.geometricPatternCount() : 0

                                delegate: Rectangle {
                                    required property int index

                                    implicitWidth: 52
                                    implicitHeight: 52
                                    radius: Kirigami.Units.cornerRadius
                                    border.width: {
                                        if (mixSwitch.checked) {
                                            var mixList = processor.bgPatternMixMotifs
                                            return mixList.indexOf(index) >= 0 ? 3 : 0
                                        }
                                        return processor.bgPatternType === index ? 3 : 1
                                    }
                                    border.color: {
                                        if (mixSwitch.checked) {
                                            var mixList = processor.bgPatternMixMotifs
                                            return mixList.indexOf(index) >= 0
                                                   ? Kirigami.Theme.highlightColor
                                                   : Kirigami.Theme.textColor
                                        }
                                        return processor.bgPatternType === index
                                               ? Kirigami.Theme.highlightColor
                                               : Kirigami.Theme.textColor
                                    }
                                    opacity: mixSwitch.checked ? (function() {
                                        var mixList = processor.bgPatternMixMotifs
                                        return mixList.indexOf(index) >= 0 ? 1.0 : 0.4
                                    })() : 1.0

                                    Image {
                                        anchors.fill: parent
                                        anchors.margins: 2
                                        source: processor.geometricPatternThumbnail(index, 48)
                                        fillMode: Image.PreserveAspectFit
                                        cache: false
                                        sourceSize.width: 48
                                        sourceSize.height: 48
                                    }

                                    Controls.Button {
                                        anchors.fill: parent
                                        opacity: 0
                                        onClicked: {
                                            if (mixSwitch.checked) {
                                                processor.toggleMixMotif(index)
                                            } else {
                                                processor.bgPatternType = index
                                            }
                                            previewDebounce.restart()
                                        }
                                    }
                                }
                            }

                            // Motif patterns (catIndex >= 2)
                            Repeater {
                                id: motifRepeater

                                model: patternControls.patternCatIndex >= 2
                                       ? (function() {
                                           var cat = patternControls.patternCatIndex - 2
                                           var result = []
                                           for (var i = 0; i < processor.motifPatternCount(); i++) {
                                               if (processor.motifPatternCategory(i) === cat)
                                                   result.push(i)
                                           }
                                           return result
                                       })() : []

                                delegate: Rectangle {
                                    required property int modelData

                                    implicitWidth: 52
                                    implicitHeight: 52
                                    radius: Kirigami.Units.cornerRadius
                                    border.width: {
                                        var typeIdx = processor.motifOffset() + modelData
                                        if (mixSwitch.checked) {
                                            var mixList = processor.bgPatternMixMotifs
                                            return mixList.indexOf(typeIdx) >= 0 ? 3 : 0
                                        }
                                        return processor.bgPatternType === typeIdx ? 3 : 1
                                    }
                                    border.color: {
                                        if (mixSwitch.checked) {
                                            var mixList = processor.bgPatternMixMotifs
                                            return mixList.indexOf(processor.motifOffset() + modelData) >= 0
                                                   ? Kirigami.Theme.highlightColor
                                                   : Kirigami.Theme.textColor
                                        }
                                        var typeIdx = processor.motifOffset() + modelData
                                        return processor.bgPatternType === typeIdx
                                               ? Kirigami.Theme.highlightColor
                                               : Kirigami.Theme.textColor
                                    }
                                    opacity: mixSwitch.checked ? (function() {
                                        var mixList = processor.bgPatternMixMotifs
                                        return mixList.indexOf(processor.motifOffset() + modelData) >= 0 ? 1.0 : 0.4
                                    })() : 1.0

                                    Image {
                                        anchors.fill: parent
                                        anchors.margins: 2
                                        source: processor.motifPatternThumbnail(modelData, 48)
                                        fillMode: Image.PreserveAspectFit
                                        cache: false
                                        sourceSize.width: 48
                                        sourceSize.height: 48
                                    }

                                    Controls.Button {
                                        anchors.fill: parent
                                        opacity: 0
                                        onClicked: {
                                            var typeIdx = processor.motifOffset() + modelData
                                            if (mixSwitch.checked) {
                                                processor.toggleMixMotif(typeIdx)
                                            } else {
                                                processor.bgPatternType = typeIdx
                                            }
                                            previewDebounce.restart()
                                        }
                                    }
                                }
                            }
                        }
                    }
                }


            }

            }  // end of leftColumn

            // ── Elegant vertical separator ──
            Rectangle {
                Layout.fillHeight: true
                Layout.topMargin: Kirigami.Units.smallSpacing
                Layout.preferredWidth: 1
                color: Kirigami.Theme.disabledTextColor
                opacity: 0.25
            }

            // ── Right column: effects ──
            ColumnLayout {
                id: rightColumn
                Layout.preferredWidth: Kirigami.Units.gridUnit * 14
                Layout.minimumWidth: Kirigami.Units.gridUnit * 14
                Layout.maximumWidth: Kirigami.Units.gridUnit * 14
                Layout.alignment: Qt.AlignTop
                Layout.topMargin: Kirigami.Units.largeSpacing
                spacing: Kirigami.Units.smallSpacing

                Controls.Label {
                    text: i18n("Effects")
                    font.bold: true
                    color: Kirigami.Theme.textColor
                    horizontalAlignment: Text.AlignHCenter
                    Layout.fillWidth: true
                }

                GridLayout {
                    Layout.fillWidth: true
                    Layout.bottomMargin: Kirigami.Units.smallSpacing
                    columns: 2
                    columnSpacing: Kirigami.Units.smallSpacing
                    rowSpacing: 2

                    Controls.ToolButton {
                        display: Controls.AbstractButton.IconOnly
                        contentItem: ThemedIcon { source: "qrc:/icons/vignette.svg" }
                        Controls.ToolTip.text: i18n("Reset Vignette")
                        Controls.ToolTip.visible: hovered
                        Controls.ToolTip.delay: 400
                        onClicked: {
                            processor.vignetteStrength = 0.0
                            previewDebounce.restart()
                        }
                    }
                    Controls.Slider {
                        Layout.fillWidth: true
                        from: 0; to: 1.0; stepSize: 0.05
                        value: processor.vignetteStrength
                        Controls.ToolTip.text: i18n("%1%", Math.round(processor.vignetteStrength * 100))
                        Controls.ToolTip.visible: hovered
                        Controls.ToolTip.delay: 400
                        onMoved: {
                            processor.vignetteStrength = value
                            previewDebounce.restart()
                        }
                    }

                    Controls.ToolButton {
                        display: Controls.AbstractButton.IconOnly
                        contentItem: ThemedIcon { source: "qrc:/icons/grain.svg" }
                        Controls.ToolTip.text: i18n("Reset Grain")
                        Controls.ToolTip.visible: hovered
                        Controls.ToolTip.delay: 400
                        onClicked: {
                            processor.grainStrength = 0.0
                            previewDebounce.restart()
                        }
                    }
                    Controls.Slider {
                        Layout.fillWidth: true
                        from: 0; to: 1.0; stepSize: 0.05
                        value: processor.grainStrength
                        Controls.ToolTip.text: i18n("%1%", Math.round(processor.grainStrength * 100))
                        Controls.ToolTip.visible: hovered
                        Controls.ToolTip.delay: 400
                        onMoved: {
                            processor.grainStrength = value
                            previewDebounce.restart()
                        }
                    }

                    Controls.ToolButton {
                        display: Controls.AbstractButton.IconOnly
                        contentItem: ThemedIcon { source: "qrc:/icons/chromatic-aberration.svg" }
                        Controls.ToolTip.text: i18n("Reset Chromatic Aberration")
                        Controls.ToolTip.visible: hovered
                        Controls.ToolTip.delay: 400
                        onClicked: {
                            processor.caStrength = 0.0
                            previewDebounce.restart()
                        }
                    }
                    Controls.Slider {
                        Layout.fillWidth: true
                        from: 0; to: 1.0; stepSize: 0.05
                        value: processor.caStrength
                        Controls.ToolTip.text: i18n("%1%", Math.round(processor.caStrength * 100))
                        Controls.ToolTip.visible: hovered
                        Controls.ToolTip.delay: 400
                        onMoved: {
                            processor.caStrength = value
                            previewDebounce.restart()
                        }
                    }

                    Controls.ToolButton {
                        display: Controls.AbstractButton.IconOnly
                        contentItem: ThemedIcon { source: "qrc:/icons/frame.svg" }
                        Controls.ToolTip.text: i18n("Reset Photo Frame")
                        Controls.ToolTip.visible: hovered
                        Controls.ToolTip.delay: 400
                        onClicked: {
                            processor.photoFrameWidth = 5
                            processor.photoFrame = true
                            previewDebounce.restart()
                        }
                    }
                    Controls.Slider {
                        id: frameWidthSlider
                        Layout.fillWidth: true
                        from: 0; to: 25; stepSize: 1
                        value: processor.photoFrameWidth
                        Controls.ToolTip.text: processor.photoFrameWidth === 0
                                      ? i18n("Off")
                                      : i18n("%1%", processor.photoFrameWidth)
                        Controls.ToolTip.visible: hovered
                        Controls.ToolTip.delay: 400
                        onMoved: {
                            processor.photoFrameWidth = value
                            if (value > 0) {
                                processor.photoFrame = true
                            } else {
                                processor.photoFrame = false
                            }
                            previewDebounce.restart()
                        }
                    }
                }


                Controls.Label {
                    text: i18n("Background")
                    font.bold: true
                    color: Kirigami.Theme.textColor
                    horizontalAlignment: Text.AlignHCenter
                    Layout.fillWidth: true
                    visible: processor.blurMode
                }

                // ── right pane blur sliders ──

                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: Kirigami.Units.smallSpacing
                    rowSpacing: 2
                    visible: processor.blurMode

                    Controls.ToolButton {
                        display: Controls.AbstractButton.IconOnly
                        contentItem: ThemedIcon { source: "qrc:/icons/blur.svg" }
                        Controls.ToolTip.text: i18n("Reset Blur")
                        Controls.ToolTip.visible: hovered
                        Controls.ToolTip.delay: 400
                        onClicked: {
                            processor.blurRadius = processor.defaultBlurRadius()
                            previewDebounce.restart()
                        }
                    }
                    Controls.Slider {
                        id: blurSlider
                        Layout.fillWidth: true
                        from: 0; to: 120; stepSize: 1
                        value: processor.blurRadius
                        Controls.ToolTip.text: processor.blurRadius === 0
                                      ? i18n("Auto")
                                      : i18n("%1 px", processor.blurRadius)
                        Controls.ToolTip.visible: hovered
                        Controls.ToolTip.delay: 400
                        onMoved: processor.blurRadius = value
                    }

                    Controls.ToolButton {
                        display: Controls.AbstractButton.IconOnly
                        contentItem: ThemedIcon { source: "qrc:/icons/saturation.svg" }
                        Controls.ToolTip.text: i18n("Reset Saturation")
                        Controls.ToolTip.visible: hovered
                        Controls.ToolTip.delay: 400
                        onClicked: {
                            processor.saturationFactor = processor.defaultSaturation()
                            previewDebounce.restart()
                        }
                    }
                    Controls.Slider {
                        id: satSlider
                        Layout.fillWidth: true
                        from: 0; to: 30; stepSize: 1
                        value: processor.saturationFactor * 10
                        Controls.ToolTip.text: i18n("%1×", processor.saturationFactor.toFixed(1))
                        Controls.ToolTip.visible: hovered
                        Controls.ToolTip.delay: 400
                        onMoved: processor.saturationFactor = value / 10.0
                    }

                    Controls.ToolButton {
                        display: Controls.AbstractButton.IconOnly
                        contentItem: ThemedIcon { source: "qrc:/icons/rotation.svg" }
                        Controls.ToolTip.text: i18n("Reset Rotation")
                        Controls.ToolTip.visible: hovered
                        Controls.ToolTip.delay: 400
                        onClicked: {
                            processor.bgBlurAngle = processor.defaultBgBlurAngle()
                            previewDebounce.restart()
                        }
                    }
                    Controls.Slider {
                        id: bgRotSlider
                        Layout.fillWidth: true
                        from: 0; to: 360; stepSize: 1
                        value: processor.bgBlurAngle
                        Controls.ToolTip.text: i18n("%1°", processor.bgBlurAngle)
                        Controls.ToolTip.visible: hovered
                        Controls.ToolTip.delay: 400
                        onMoved: processor.bgBlurAngle = value
                    }
                }

                Controls.Label {
                    text: i18n("Zoom")
                    font.bold: true
                    color: Kirigami.Theme.textColor
                    horizontalAlignment: Text.AlignHCenter
                    Layout.fillWidth: true
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: Kirigami.Units.smallSpacing
                    rowSpacing: 2

                    // Background zoom (blur-mode background only)
                    Controls.ToolButton {
                        visible: processor.blurMode
                        display: Controls.AbstractButton.IconOnly
                        contentItem: ThemedIcon { source: "qrc:/icons/zoom.svg" }
                        Controls.ToolTip.text: i18n("Reset Background Zoom")
                        Controls.ToolTip.visible: hovered
                        Controls.ToolTip.delay: 400
                        onClicked: {
                            processor.bgZoom = processor.defaultBgZoom()
                            previewDebounce.restart()
                        }
                    }
                    Controls.Slider {
                        id: zoomSlider
                        visible: processor.blurMode
                        Layout.fillWidth: true
                        from: 5; to: 30; stepSize: 1
                        value: Math.round(processor.bgZoom * 10)
                        Controls.ToolTip.text: i18n("%1%", Math.round(processor.bgZoom * 100))
                        Controls.ToolTip.visible: hovered
                        Controls.ToolTip.delay: 400
                        onMoved: processor.bgZoom = value / 10.0
                    }

                    // Foreground zoom (rect-scaling, whole picture incl. frame)
                    Controls.ToolButton {
                        display: Controls.AbstractButton.IconOnly
                        contentItem: ThemedIcon { source: "qrc:/icons/zoom-center.svg" }
                        Controls.ToolTip.text: i18n("Reset Picture Zoom")
                        Controls.ToolTip.visible: hovered
                        Controls.ToolTip.delay: 400
                        onClicked: {
                            processor.fgZoom = processor.defaultFgZoom()
                            previewDebounce.restart()
                        }
                    }
                    Controls.Slider {
                        Layout.fillWidth: true
                        from: processor.fgZoomMin; to: processor.fgZoomMax; stepSize: 0.01
                        value: processor.fgZoom
                        Controls.ToolTip.text: i18n("%1%", Math.round(processor.fgZoom * 100))
                        Controls.ToolTip.visible: hovered
                        Controls.ToolTip.delay: 400
                        onMoved: {
                            processor.fgZoom = value
                            previewDebounce.restart()
                        }
                    }

                    // PiP zoom (content magnify inside fixed rect, margins stay put)
                    Controls.ToolButton {
                        display: Controls.AbstractButton.IconOnly
                        contentItem: ThemedIcon { source: "qrc:/icons/pip.svg" }
                        Controls.ToolTip.text: i18n("Reset PiP Zoom")
                        Controls.ToolTip.visible: hovered
                        Controls.ToolTip.delay: 400
                        onClicked: {
                            processor.pipZoom = processor.defaultPipZoom()
                            previewDebounce.restart()
                        }
                    }
                    Controls.Slider {
                        Layout.fillWidth: true
                        from: 1.0; to: 4.0; stepSize: 0.05
                        value: processor.pipZoom
                        Controls.ToolTip.text: i18n("%1%", Math.round(processor.pipZoom * 100))
                        Controls.ToolTip.visible: hovered
                        Controls.ToolTip.delay: 400
                        onMoved: {
                            processor.pipZoom = value
                            previewDebounce.restart()
                        }
                    }
                }

                Controls.Label {
                    text: i18n("Page")
                    font.bold: true
                    color: Kirigami.Theme.textColor
                    horizontalAlignment: Text.AlignHCenter
                    Layout.fillWidth: true
                    visible: !processor.blurMode && dropArea.fileList.length > 0
                }

                GridLayout {
                    Layout.fillWidth: true
                    visible: !processor.blurMode && dropArea.fileList.length > 0
                    Layout.bottomMargin: Kirigami.Units.smallSpacing
                    columns: 2
                    columnSpacing: Kirigami.Units.smallSpacing
                    rowSpacing: 2

                    // Gradient Angle
                    Controls.ToolButton {
                        visible: !processor.blurMode && processor.bgGradientStyle > 0
                        display: Controls.AbstractButton.IconOnly
                        contentItem: ThemedIcon { source: "qrc:/icons/angle.svg" }
                        Controls.ToolTip.text: i18n("Reset Angle")
                        Controls.ToolTip.visible: hovered
                        Controls.ToolTip.delay: 400
                        onClicked: {
                            processor.gradientAngle = processor.defaultGradientAngle()
                            previewDebounce.restart()
                        }
                    }
                    Controls.Slider {
                        visible: !processor.blurMode && processor.bgGradientStyle > 0
                        Layout.fillWidth: true
                        from: 0; to: 360; stepSize: 1
                        value: processor.gradientAngle
                        Controls.ToolTip.text: i18n("%1°", processor.gradientAngle)
                        Controls.ToolTip.visible: hovered
                        Controls.ToolTip.delay: 400
                        onMoved: processor.gradientAngle = value
                    }

                    // Pattern Scale
                    Controls.ToolButton {
                        visible: !processor.blurMode && processor.bgPatternEnabled
                        display: Controls.AbstractButton.IconOnly
                        contentItem: ThemedIcon { source: "qrc:/icons/zoom.svg" }
                        Controls.ToolTip.text: i18n("Reset Scale")
                        Controls.ToolTip.visible: hovered
                        Controls.ToolTip.delay: 400
                        onClicked: {
                            processor.bgPatternScale = 1.0
                            previewDebounce.restart()
                        }
                    }
                    Controls.Slider {
                        visible: !processor.blurMode && processor.bgPatternEnabled
                        Layout.fillWidth: true
                        from: 3; to: 30; stepSize: 1
                        value: Math.round(processor.bgPatternScale * 10)
                        Controls.ToolTip.text: i18n("%1%", Math.round(processor.bgPatternScale * 100))
                        Controls.ToolTip.visible: hovered
                        Controls.ToolTip.delay: 400
                        onMoved: {
                            processor.bgPatternScale = value / 10.0
                            previewDebounce.restart()
                        }
                    }

                    // Pattern Spacing
                    Controls.ToolButton {
                        visible: !processor.blurMode && processor.bgPatternEnabled && patternControls.patternCatIndex !== 1
                        display: Controls.AbstractButton.IconOnly
                        contentItem: ThemedIcon { source: "qrc:/icons/zoom.svg" }
                        Controls.ToolTip.text: i18n("Reset Spacing")
                        Controls.ToolTip.visible: hovered
                        Controls.ToolTip.delay: 400
                        onClicked: {
                            processor.bgPatternSpacing = 0.0
                            previewDebounce.restart()
                        }
                    }
                    Controls.Slider {
                        visible: !processor.blurMode && processor.bgPatternEnabled && patternControls.patternCatIndex !== 1
                        Layout.fillWidth: true
                        from: 0; to: 20; stepSize: 1
                        value: Math.round(processor.bgPatternSpacing * 10)
                        Controls.ToolTip.text: i18n("%1% gap", Math.round(processor.bgPatternSpacing * 100))
                        Controls.ToolTip.visible: hovered
                        Controls.ToolTip.delay: 400
                        onMoved: {
                            processor.bgPatternSpacing = value / 10.0
                            previewDebounce.restart()
                        }
                    }
                }
            }  // end of rightColumn
        }  // end of two-column RowLayout

            // ── File list ──
            Controls.Label {
                visible: dropArea.fileCount > 1
                text: i18n("Files to process:")
                color: Kirigami.Theme.disabledTextColor
            }
            ListView {
                id: previewList
                visible: dropArea.fileCount > 1
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(count * 40, 120)
                model: []
                delegate: RowLayout {
                    id: fileDelegate
                    width: parent ? parent.width : 0
                    spacing: Kirigami.Units.smallSpacing

                    required property var modelData

                    Image {
                        source: modelData ? modelData.previewUrl : ""
                        sourceSize.width: 32; sourceSize.height: 32
                        fillMode: Image.PreserveAspectFit
                        Layout.preferredWidth: 32; Layout.preferredHeight: 32
                        visible: status === Image.Ready
                    }
                    Controls.Label {
                        text: modelData ? modelData.path.toString().split("/").pop() : ""
                        elide: Text.ElideMiddle
                        Layout.fillWidth: true
                    }
                }
            }

            // === Status / InlineMessage ===
            Kirigami.InlineMessage {
                id: statusMessage
                Layout.fillWidth: true
                showCloseButton: true
                visible: false
            }

            Item { Layout.fillHeight: true }
        }
    }

    // Color dialog
    Kirigami.Dialog {
        id: colorDialog
        title: i18n("Pick background color")
        preferredWidth: Kirigami.Units.gridUnit * 18
        standardButtons: Kirigami.Dialog.Ok | Kirigami.Dialog.Cancel

        GridLayout {
            columns: 8
            columnSpacing: 4; rowSpacing: 4
            property var colors: [
                "#ffffff","#f5f0eb","#e8e0d8","#d4c9be","#b8ada0","#8a8078","#5a5048","#3a3028",
                "#e0d8d0","#d0c8d4","#d0d4c8","#c8d4d8","#d4c8c8","#c8c8d4","#a0a098","#686868",
                "#f0e8dc","#dcd0c4","#c8c0b4","#b8b098","#a8a898","#989888","#787870","#585850"
            ]
            Repeater {
                model: parent.colors
                Rectangle {
                    width: 32; height: 32; radius: 4
                    border.width: 1; border.color: Kirigami.Theme.textColor
                    color: modelData
                    Controls.Button {
                        anchors.fill: parent; opacity: 0
                        onClicked: {
                            processor.autoColor = false;
                            processor.backgroundColor = modelData;
                        }
                    }
                }
            }
        }
    }

    // Presets dialog (F2 — named parameter presets)
    Kirigami.Dialog {
        id: presetDialog
        title: i18n("Parameter presets")
        preferredWidth: Kirigami.Units.gridUnit * 20

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            Controls.TextField {
                id: presetNameField
                Layout.fillWidth: true
                placeholderText: i18n("New preset name")
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing
                Controls.Button {
                    text: i18n("Save current")
                    Layout.fillWidth: true
                    onClicked: {
                        var n = presetNameField.text.trim()
                        if (n.length > 0) {
                            processor.saveParamPreset(n)
                            presetNameField.text = ""
                        }
                    }
                }
                Controls.Button {
                    text: i18n("Delete selected")
                    Layout.fillWidth: true
                    enabled: presetList.currentIndex >= 0
                    onClicked: {
                        if (presetList.currentIndex >= 0)
                            processor.deleteParamPreset(presetList.model[presetList.currentIndex])
                    }
                }
            }

            ListView {
                id: presetList
                Layout.fillWidth: true
                Layout.preferredHeight: 160
                clip: true
                model: processor.paramPresetNames

                delegate: Controls.ItemDelegate {
                    required property string modelData
                    width: presetList.width
                    text: modelData
                    highlighted: presetList.currentIndex === index
                    onClicked: {
                        presetList.currentIndex = index
                        processor.applyParamPreset(modelData)
                    }
                }
            }
        }
    }

    // Processor
    WallpaperProcessor {
        id: processor

        onStatusMessageChanged: {
            if (processor.statusMessage.length > 0) {
                statusMessage.type = Kirigami.MessageType.Information;
                statusMessage.text = processor.statusMessage;
                statusMessage.visible = true;
                statusMessageTimer.restart();
            }
        }
        onErrorOccurred: function (msg) {
            statusMessage.type = Kirigami.MessageType.Error;
            statusMessage.text = msg;
            statusMessage.visible = true;
            statusMessageTimer.stop();  // persist errors
        }
        onProcessingFinished: {
            if (processor.outputPath.length > 0) {
                crossfadePreview("file://" + processor.outputPath);
            }
            statusMessage.type = Kirigami.MessageType.Positive;
            statusMessage.text = i18n("Done");
            statusMessage.visible = true;
            statusMessageTimer.restart();
        }
    }

    // Status auto-hide timer
    Timer {
        id: statusMessageTimer
        interval: 4000
        onTriggered: statusMessage.visible = false
    }

    function refreshPreviews() {
        var list = dropArea.fileList;
        if (list.length === 0) return;
        previewContainer._pendingRenders = list.length;
        var cacheBust = "?t=" + Date.now();
        var entries = [];
        for (var i = 0; i < list.length; ++i) {
            if (list[i].path) {
                var url = processor.generatePreview(list[i].path);
                if (url.length > 0)
                    url += cacheBust;
                entries.push({path: list[i].path, previewUrl: url});
            }
        }
        dropArea.fileList = entries;
        previewList.model = dropArea.fileList;
        if (dropArea.fileList.length > 0)
            crossfadePreview(dropArea.fileList[0].previewUrl);
    }

    // Live preview update on tweak changes (debounced)
    Timer {
        id: previewDebounce
        interval: 300
        repeat: false
        onTriggered: refreshPreviews()
    }

    function crossfadePreview(newUrl) {
        if (!previewA.source.toString() || previewA.source.toString() === "") {
            previewA.source = newUrl;
            previewA.opacity = 1.0;
            return;
        }
        // Strip cache-bust for URL comparison — same path = same file
        var oldUrl = previewA.source.toString().replace(/\?t=\d+$/, "");
        if (oldUrl === newUrl) {
            // Touch source to force QML image cache refresh
            previewA.source = "";
            previewA.source = newUrl;
            return;
        }
        fadeAnim.stop();
        previewA.opacity = 1.0;
        previewB.opacity = 0.0;
        previewB.source = "";
        previewB.source = newUrl;
        fadeAnim.start();
    }

    Connections {
        target: processor
        function onPreviewReady(sourcePath, previewUrl) {
            // Update the matching entry in fileList
            var list = dropArea.fileList
            for (var i = 0; i < list.length; ++i) {
                if (list[i].path === sourcePath) {
                    list[i].previewUrl = previewUrl
                    break
                }
            }
            dropArea.fileList = list
            if (--previewContainer._pendingRenders < 0)
                previewContainer._pendingRenders = 0
            // If this is the first image's preview, crossfade immediately
            if (dropArea.fileList.length > 0 && dropArea.fileList[0].path === sourcePath)
                crossfadePreview(previewUrl)
        }
        // Single aggregate signal drives the live preview (Q2) — one handler
        // replaces the previous 28 per-parameter handlers.
        function onRenderParamsChanged() {
            previewDebounce.restart();
            // Keep the width/height fields in sync when the backend recalculates
            // them (aspect-ratio presets, screen detect). Guard against clobbering
            // an in-progress edit (Q16).
            if (!widthInput.activeFocus)
                widthInput.text = processor.targetWidth;
            if (!heightInput.activeFocus)
                heightInput.text = processor.targetHeight;
        }
    }
}
