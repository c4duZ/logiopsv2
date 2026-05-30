// Copyright 2019-2023 PixlOne — Adapted by c4duZ
//
// DpiCycleEditor (DPI-02 / DPI-03) — the labeled DPI-cycle preset list. Each row
// is a preset = an editable DPI VALUE (SpinBox, bounded by the device) + an
// editable LABEL (TextField), with a destructive per-row remove affordance. A
// "+ Add preset" secondary button appends a row; an empty list shows the exact
// helper copy. Presets are device/profile-scoped (option-a, Task 0): every edit
// calls a DeviceController preset mutator which pushes the FULL {value,label} list
// to the daemon via .DPI.SetPresets (values persist device-scoped, labels in the
// same schema list). Reordering is out of scope this phase.
//
// The model is deviceController.dpiPresets (a list of {value,label} maps). The
// active preset chip (the one matching the current live DPI) uses accentTint bg +
// accent text per the UI-SPEC reserved-accent list.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import logiops.gui

ColumnLayout {
    id: root

    // The per-device controller (provides dpiPresets + the preset mutators and
    // dpiMin/dpiMax/dpiStep bounds). May be null when no device is selected.
    property var controller: null

    readonly property var presets: controller ? controller.dpiPresets : []
    readonly property int currentDpi: controller ? controller.dpi : 0

    spacing: Theme.spacingSm

    // --- Section header ---
    Text {
        text: qsTr("DPI cycle presets")
        font.pixelSize: Theme.labelSize
        font.weight: Theme.weightMedium
        color: Theme.foreground
    }

    // --- Empty state (exact UI-SPEC copy) ---
    Text {
        Layout.fillWidth: true
        visible: root.presets.length === 0
        text: qsTr("No DPI presets yet. Add one to cycle through sensitivities.")
        color: Theme.mutedForeground
        font.pixelSize: Theme.bodySize
        wrapMode: Text.WordWrap
    }

    // --- Preset rows ---
    Repeater {
        model: root.presets
        delegate: Rectangle {
            id: presetRow
            required property int index
            required property var modelData

            readonly property int presetValue: modelData.value
            readonly property string presetLabel: modelData.label
            // The active preset = the one whose value matches the live DPI.
            readonly property bool active: presetValue === root.currentDpi

            Layout.fillWidth: true
            implicitHeight: Theme.controlRowHeight + Theme.spacingSm
            radius: 8
            // Active chip: accentTint bg per the UI-SPEC reserved-accent list.
            color: active ? Theme.accentTint : "transparent"
            border.width: active ? 0 : 1
            border.color: Theme.hairline

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.spacingMd
                anchors.rightMargin: Theme.spacingSm
                spacing: Theme.spacingSm

                // DPI value (bounded by the device; daemon snaps on apply).
                SpinBox {
                    id: valueSpin
                    from: root.controller ? root.controller.dpiMin : 0
                    to: root.controller ? root.controller.dpiMax : 16000
                    stepSize: root.controller && root.controller.dpiStep > 0
                              ? root.controller.dpiStep : 50
                    value: presetRow.presetValue
                    editable: true
                    onValueModified: if (root.controller)
                        root.controller.setPresetValue(presetRow.index, value)
                }

                // Editable label (Label 14).
                TextField {
                    id: labelField
                    Layout.fillWidth: true
                    text: presetRow.presetLabel
                    placeholderText: qsTr("Label")
                    font.pixelSize: Theme.labelSize
                    color: presetRow.active ? Theme.accent : Theme.foreground
                    onEditingFinished: if (root.controller)
                        root.controller.setPresetLabel(presetRow.index, text)
                }

                // Destructive per-row remove affordance.
                ToolButton {
                    id: removeBtn
                    text: "✕" // ✕
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Remove preset")
                    contentItem: Text {
                        text: removeBtn.text
                        font.pixelSize: Theme.bodySize
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        // destructive on hover/confirm (UI-SPEC).
                        color: removeBtn.hovered ? Theme.destructive : Theme.mutedForeground
                    }
                    onClicked: if (root.controller)
                        root.controller.removePreset(presetRow.index)
                }
            }
        }
    }

    // --- "+ Add preset" secondary button (NOT accent). ---
    Button {
        id: addBtn
        text: qsTr("+ Add preset")
        flat: true
        onClicked: if (root.controller) {
            // Seed the new preset at the current live DPI (a sensible default the
            // controller clamps to [dpiMin,dpiMax]); empty label for the user to fill.
            root.controller.addPreset(root.currentDpi > 0 ? root.currentDpi
                                                          : root.controller.dpiMin,
                                      "")
        }
    }
}
