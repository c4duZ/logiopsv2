// Copyright 2019-2023 PixlOne — Adapted by c4duZ
//
// PointerTab (DPI-01 / DPI-02 / DPI-03) — the "Pointer" tab. A device-bounded DPI
// sensitivity slider (DPI-01) with a tabular-stable numeric readout, above the
// DpiCycleEditor labeled preset list (DPI-02 / DPI-03).
//
// The slider bounds come from the device (controller.dpiMin/dpiMax/dpiStep — never
// hardcoded). onMoved calls controller.setDpi(value) — fired on move-end, NOT per
// pixel, to avoid a D-Bus call storm (T-3-03-02). No GUI-side rounding: the daemon
// snaps via getClosestDPI and the controller re-reads the snapped value.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import logiops.gui

Item {
    id: root
    // The per-device controller (capabilities + live values), injected by ConfigTabs.
    property var controller: null

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: root.width
            spacing: Theme.spacingLg

            // Outer padding column.
            anchors.left: parent.left
            anchors.right: parent.right

            // --- Section header ---
            Text {
                Layout.topMargin: Theme.spacingLg
                Layout.leftMargin: Theme.spacingLg
                text: qsTr("Pointer")
                font.pixelSize: Theme.headingSize
                font.weight: Theme.weightMedium
                color: Theme.foreground
            }

            // --- DPI sensitivity slider (DPI-01) ---
            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.spacingLg
                Layout.rightMargin: Theme.spacingLg
                spacing: Theme.spacingSm

                Text {
                    text: qsTr("Sensitivity (DPI)")
                    font.pixelSize: Theme.labelSize
                    font.weight: Theme.weightMedium
                    color: Theme.foreground
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingMd

                    Slider {
                        id: dpiSlider
                        Layout.fillWidth: true
                        from: root.controller ? root.controller.dpiMin : 0
                        to: root.controller ? root.controller.dpiMax : 16000
                        stepSize: root.controller && root.controller.dpiStep > 0
                                  ? root.controller.dpiStep : 50
                        value: root.controller ? root.controller.dpi : 0
                        // Fire on move-END only, not per pixel (T-3-03-02). No
                        // GUI-side rounding — the daemon snaps via getClosestDPI.
                        onMoved: if (root.controller) root.controller.setDpi(value)

                        // Unfilled track = hairline; filled portion = accent.
                        background: Rectangle {
                            x: dpiSlider.leftPadding
                            y: dpiSlider.topPadding + dpiSlider.availableHeight / 2 - height / 2
                            width: dpiSlider.availableWidth
                            height: 4
                            radius: 2
                            color: Theme.hairline
                            Rectangle {
                                width: dpiSlider.visualPosition * parent.width
                                height: parent.height
                                color: Theme.accent
                                radius: 2
                            }
                        }
                        // Thumb = accent with a focus ring on keyboard focus.
                        handle: Rectangle {
                            x: dpiSlider.leftPadding + dpiSlider.visualPosition
                               * (dpiSlider.availableWidth - width)
                            y: dpiSlider.topPadding + dpiSlider.availableHeight / 2 - height / 2
                            width: 18
                            height: 18
                            radius: 9
                            color: Theme.accent
                            border.color: dpiSlider.visualFocus ? Theme.accent : "transparent"
                            border.width: dpiSlider.visualFocus ? 3 : 0
                            opacity: dpiSlider.visualFocus ? 1.0 : 0.95
                        }
                    }

                    // Live numeric readout (Body 16), tabular-stable width via
                    // TextMetrics so sliding values do not reflow (DPI unit shown).
                    TextMetrics {
                        id: dpiMetrics
                        font.pixelSize: Theme.bodySize
                        text: "00000 DPI"
                    }
                    Text {
                        Layout.preferredWidth: dpiMetrics.width
                        horizontalAlignment: Text.AlignRight
                        text: (root.controller ? root.controller.dpi : 0) + qsTr(" DPI")
                        font.pixelSize: Theme.bodySize
                        color: Theme.foreground
                    }
                }
            }

            // --- DPI-cycle preset editor (DPI-02 / DPI-03) ---
            DpiCycleEditor {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.spacingLg
                Layout.rightMargin: Theme.spacingLg
                Layout.bottomMargin: Theme.spacingLg
                controller: root.controller
            }
        }
    }
}
