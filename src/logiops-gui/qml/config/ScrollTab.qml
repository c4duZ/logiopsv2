// Copyright 2019-2023 PixlOne — Adapted by c4duZ
//
// ScrollTab (SCR-01 / SCR-02 / SCR-03) — the "Scroll" tab. Three capability-gated
// sections, each wrapped with `visible` bound to the controller's introspected
// capability flag and laid out so an absent feature's whole section is HIDDEN
// (mirroring the daemon's UnsupportedFeature), never shown disabled:
//   1) SmartShift (SCR-01): on/off Switch (accent when ON) + threshold + torque
//      sliders; sliders greyed/disabled while OFF; torque slider gated on hasTorque.
//   2) Hi-res scroll (SCR-02): on/off Switch + invert-direction CheckBox.
//   3) Thumbwheel (SCR-03): divert + invert Switches + a TAP simple-action mapping
//      reusing the categorized action-picker structure (the simple-action path
//      .ThumbWheel.SetTap; left/right are gesture-typed and deferred to Phase 4).
//
// All toggles use the accent ON state per the UI-SPEC reserved-accent list. The
// tab itself is only shown by DetailPane when at least one of these capabilities
// is present; here each SECTION is independently gated.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import logiops.gui

Item {
    id: root
    // The per-device controller (capabilities + live values), injected by ConfigTabs.
    property var controller: null

    readonly property bool hasSmartShift: controller ? controller.hasSmartShift : false
    readonly property bool hasHires: controller ? controller.hasHires : false
    readonly property bool hasThumbwheel: controller ? controller.hasThumbwheel : false
    readonly property bool hasTorque: controller ? controller.hasTorque : false

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: root.width
            spacing: Theme.spacingLg

            Text {
                Layout.topMargin: Theme.spacingLg
                Layout.leftMargin: Theme.spacingLg
                text: qsTr("Scroll")
                font.pixelSize: Theme.headingSize
                font.weight: Theme.weightMedium
                color: Theme.foreground
            }

            // ---------------------------------------------------------------
            // 1) SmartShift (SCR-01) — hidden entirely when unsupported.
            // ---------------------------------------------------------------
            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.spacingLg
                Layout.rightMargin: Theme.spacingLg
                spacing: Theme.spacingSm
                visible: root.hasSmartShift

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: qsTr("SmartShift")
                        font.pixelSize: Theme.labelSize
                        font.weight: Theme.weightMedium
                        color: Theme.foreground
                        Layout.fillWidth: true
                    }
                    // on/off toggle (accent when ON).
                    Switch {
                        id: smartShiftSwitch
                        checked: root.controller ? root.controller.smartShiftActive : false
                        onToggled: if (root.controller)
                            root.controller.setSmartShiftActive(checked)
                    }
                }

                // Threshold slider — greyed/disabled while OFF.
                RowLayout {
                    Layout.fillWidth: true
                    enabled: smartShiftSwitch.checked
                    opacity: enabled ? 1.0 : 0.4
                    Text {
                        text: qsTr("Threshold")
                        font.pixelSize: Theme.labelSize
                        color: Theme.foreground
                        Layout.preferredWidth: 96
                    }
                    Slider {
                        id: thresholdSlider
                        Layout.fillWidth: true
                        from: 0; to: 255; stepSize: 1
                        value: root.controller ? root.controller.smartShiftThreshold : 0
                        onMoved: if (root.controller)
                            root.controller.setSmartShiftThreshold(Math.round(value))
                    }
                    Text {
                        text: Math.round(thresholdSlider.value)
                        font.pixelSize: Theme.bodySize
                        color: Theme.foreground
                        Layout.preferredWidth: 40
                        horizontalAlignment: Text.AlignRight
                    }
                }

                // Torque slider — gated on hasTorque (TorqueSupport).
                RowLayout {
                    Layout.fillWidth: true
                    visible: root.hasTorque
                    enabled: smartShiftSwitch.checked
                    opacity: enabled ? 1.0 : 0.4
                    Text {
                        text: qsTr("Torque")
                        font.pixelSize: Theme.labelSize
                        color: Theme.foreground
                        Layout.preferredWidth: 96
                    }
                    Slider {
                        id: torqueSlider
                        Layout.fillWidth: true
                        from: 0; to: 100; stepSize: 1
                        value: root.controller ? root.controller.smartShiftTorque : 0
                        onMoved: if (root.controller)
                            root.controller.setSmartShiftTorque(Math.round(value))
                    }
                    Text {
                        text: Math.round(torqueSlider.value)
                        font.pixelSize: Theme.bodySize
                        color: Theme.foreground
                        Layout.preferredWidth: 40
                        horizontalAlignment: Text.AlignRight
                    }
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: Theme.hairline }
            }

            // ---------------------------------------------------------------
            // 2) Hi-res scroll (SCR-02) — hidden entirely when unsupported.
            // ---------------------------------------------------------------
            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.spacingLg
                Layout.rightMargin: Theme.spacingLg
                spacing: Theme.spacingSm
                visible: root.hasHires

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: qsTr("Hi-res scroll")
                        font.pixelSize: Theme.labelSize
                        font.weight: Theme.weightMedium
                        color: Theme.foreground
                        Layout.fillWidth: true
                    }
                    Switch {
                        id: hiresSwitch
                        checked: root.controller ? root.controller.hiresOn : false
                        onToggled: if (root.controller) root.controller.setHiresOn(checked)
                    }
                }

                CheckBox {
                    text: qsTr("Invert scroll direction")
                    checked: root.controller ? root.controller.hiresInvert : false
                    onToggled: if (root.controller) root.controller.setHiresInvert(checked)
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: Theme.hairline }
            }

            // ---------------------------------------------------------------
            // 3) Thumbwheel (SCR-03) — hidden entirely when unsupported.
            // ---------------------------------------------------------------
            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.spacingLg
                Layout.rightMargin: Theme.spacingLg
                Layout.bottomMargin: Theme.spacingLg
                spacing: Theme.spacingSm
                visible: root.hasThumbwheel

                Text {
                    text: qsTr("Thumbwheel")
                    font.pixelSize: Theme.labelSize
                    font.weight: Theme.weightMedium
                    color: Theme.foreground
                }

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: qsTr("Divert to gestures")
                        font.pixelSize: Theme.labelSize
                        color: Theme.foreground
                        Layout.fillWidth: true
                    }
                    Switch {
                        id: divertSwitch
                        checked: root.controller ? root.controller.thumbDivert : false
                        onToggled: if (root.controller)
                            root.controller.setThumbDivert(checked)
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: qsTr("Invert direction")
                        font.pixelSize: Theme.labelSize
                        color: Theme.foreground
                        Layout.fillWidth: true
                    }
                    Switch {
                        id: thumbInvertSwitch
                        checked: root.controller ? root.controller.thumbInvert : false
                        onToggled: if (root.controller)
                            root.controller.setThumbInvert(checked)
                    }
                }

                // Tap action mapping (SCR-03) — reuses the categorized action-picker
                // structure (same glyphs/rows as the Buttons ReassignPanel),
                // restricted to the simple actions that work without a button
                // context via .ThumbWheel.SetTap (VERIFIED simple action works).
                Text {
                    Layout.topMargin: Theme.spacingSm
                    text: qsTr("Tap action")
                    font.pixelSize: Theme.labelSize
                    color: Theme.mutedForeground
                }

                Repeater {
                    model: [
                        { type: "ToggleSmartShift", label: qsTr("SmartShift toggle"),
                          glyph: "qrc:/logiops/gui/icons/smartshift.svg" },
                        { type: "ToggleHiresScroll", label: qsTr("Hi-res toggle"),
                          glyph: "qrc:/logiops/gui/icons/hires.svg" },
                        { type: "None", label: qsTr("Disabled"),
                          glyph: "qrc:/logiops/gui/icons/disabled.svg" }
                    ]
                    delegate: Item {
                        required property var modelData
                        Layout.fillWidth: true
                        implicitHeight: Theme.controlRowHeight

                        Rectangle {
                            anchors.fill: parent
                            color: tapHover.containsMouse ? Theme.hoverTint : "transparent"
                            radius: 6
                        }
                        Row {
                            anchors.left: parent.left
                            anchors.leftMargin: Theme.spacingSm
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: Theme.spacingSm
                            Image {
                                anchors.verticalCenter: parent.verticalCenter
                                width: 20; height: 20
                                sourceSize.width: 20; sourceSize.height: 20
                                source: modelData.glyph
                            }
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: modelData.label
                                font.pixelSize: Theme.bodySize
                                color: Theme.foreground
                            }
                        }
                        MouseArea {
                            id: tapHover
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            // Simple-action assignment to the thumbwheel tap (SCR-03).
                            onClicked: if (root.controller)
                                root.controller.setThumbTap(modelData.type)
                        }
                    }
                }
            }
        }
    }
}
