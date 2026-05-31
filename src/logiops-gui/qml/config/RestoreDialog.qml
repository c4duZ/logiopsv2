// Copyright 2019-2023 PixlOne — Adapted by c4duZ
//
// RestoreDialog (CONF-02 / T-3-04-04) — the modal confirmation gate for restoring a
// device to defaults. ClearProfile wipes the active profile's buttons/pointer/scroll
// config, so it MUST be confirmed (naming the device + "can't be undone"). The
// destructive-styled confirm button calls configState.restoreDefaults(activeProfile)
// (.Device.ClearProfile on the selected device, set via configState.setDevicePath in
// DetailPane); Cancel dismisses with no side effect. Acts on the ACTIVE profile only.
//
// Exact UI-SPEC Copywriting strings. C++ owns the D-Bus call; this renders only.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import logiops.gui

Dialog {
    id: root

    property string deviceName: ""
    property string activeProfile: ""

    modal: true
    anchors.centerIn: parent
    width: Math.min(Theme.dialogMaxWidth, parent ? parent.width - 2 * Theme.spacingLg : Theme.dialogMaxWidth)
    padding: Theme.spacingLg

    title: qsTr("Restore default settings?")

    // Fade in/out per the UI-SPEC motion contract.
    enter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.motionBase; easing.type: Easing.OutCubic } }
    exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Theme.motionBase; easing.type: Easing.OutCubic } }

    background: Rectangle {
        color: Theme.secondary
        radius: 10
        border.width: 1
        border.color: Theme.hairline
    }

    header: ColumnLayout {
        spacing: 0
        // Destructive accent strip (UI-SPEC: destructiveTint dialog accent).
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 4
            radius: 2
            color: Theme.destructive
        }
        Text {
            Layout.fillWidth: true
            Layout.margins: Theme.spacingLg
            Layout.bottomMargin: 0
            text: root.title
            font.pixelSize: Theme.headingSize
            font.weight: Theme.weightMedium
            color: Theme.foreground
            wrapMode: Text.WordWrap
        }
    }

    contentItem: Text {
        text: qsTr("This resets %1's buttons, pointer, and scroll settings to their defaults. This can't be undone.")
                .arg(root.deviceName.length > 0 ? root.deviceName : qsTr("this device"))
        color: Theme.mutedForeground
        font.pixelSize: Theme.bodySize
        wrapMode: Text.WordWrap
    }

    footer: DialogButtonBox {
        padding: Theme.spacingLg
        spacing: Theme.spacingSm
        background: Rectangle { color: "transparent" }

        // Cancel (secondary).
        Button {
            text: qsTr("Cancel")
            flat: true
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }
        // Restore defaults (destructive confirm).
        Button {
            id: confirmBtn
            text: qsTr("Restore defaults")
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            contentItem: Text {
                text: confirmBtn.text
                color: "#FFFFFF"
                font.pixelSize: Theme.labelSize
                font.weight: Theme.weightMedium
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                radius: 8
                color: Theme.destructive
                opacity: confirmBtn.hovered ? 0.92 : 1.0
            }
        }
    }

    onAccepted: configState.restoreDefaults(root.activeProfile)
}
