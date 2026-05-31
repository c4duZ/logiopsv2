// Copyright 2019-2023 PixlOne — Adapted by c4duZ
//
// SaveToolbar (CONF-01 / CONF-02) — the persistent persistence toolbar mounted in
// the DetailPane header, above the tabs. It is the phase's apply-live-then-Save UX:
//
//   - A warning-colored "Unsaved changes" pill bound to configState.dirty (appears
//     once any tab setter has applied-but-not-persisted a change).
//   - The phase-primary accent-filled "Save" CTA -> configState.save(). While
//     configState.saving is true it shows a BusyIndicator and is DISABLED (the
//     daemon holds its server_lock through the polkit prompt — the GUI must not
//     block and must not retry-storm, T-3-04-02).
//   - On configState.saved() a transient charging-colored "Saved" tick fades after
//     ~2s (motionBase).
//   - On configState.lastError the mapped inline error copy shows. The two paths
//     are the UI-SPEC strings: polkit-denied ("authorization was declined ...") vs
//     daemon-down ("daemon may be unavailable ..."). The mapping is done in C++
//     (ConfigState); the canonical strings are re-declared here so the copy is
//     auditable in the QML and QML can fall back if lastError is ever empty.
//   - A secondary/destructive "Restore defaults..." button opens the RestoreDialog.
//
// C++ (ConfigState) owns all state + the async Save; this renders only.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import logiops.gui

RowLayout {
    id: root

    // The active device's name + active profile, used by the RestoreDialog copy and
    // the ClearProfile target. Injected by DetailPane from the selected row.
    property string deviceName: ""
    property string activeProfile: ""

    // The two UI-SPEC Save error strings (Copywriting table). The C++ ConfigState
    // maps the D-Bus error to one of these and exposes it as lastError; we re-declare
    // them so the exact copy is visible/auditable in the QML.
    readonly property string authDeclinedError:
        qsTr("Couldn't save — authorization was declined. Your changes are still applied live; click Save to try again.")
    readonly property string daemonDownError:
        qsTr("Couldn't save to disk. The daemon may be unavailable — check that logid is running, then try again.")

    spacing: Theme.spacingMd

    // --- Unsaved-changes pill (warning) ---
    Rectangle {
        id: unsavedPill
        visible: configState.dirty
        implicitHeight: Theme.controlRowHeight - Theme.spacingSm
        implicitWidth: unsavedRow.implicitWidth + 2 * Theme.spacingMd
        radius: implicitHeight / 2
        color: Qt.rgba(Theme.warning.r, Theme.warning.g, Theme.warning.b, 0.16)

        RowLayout {
            id: unsavedRow
            anchors.centerIn: parent
            spacing: Theme.spacingXs
            Rectangle {
                width: 8; height: 8; radius: 4
                color: Theme.warning
                Layout.alignment: Qt.AlignVCenter
            }
            Text {
                text: qsTr("Unsaved changes")
                color: Theme.warning
                font.pixelSize: Theme.labelSize
                font.weight: Theme.weightMedium
            }
        }
    }

    // --- Transient "Saved" tick (charging), fades after ~2s ---
    Row {
        id: savedTick
        spacing: Theme.spacingXs
        opacity: 0
        visible: opacity > 0
        Text {
            text: "✓"
            color: Theme.charging
            font.pixelSize: Theme.bodySize
            font.weight: Theme.weightMedium
        }
        Text {
            text: qsTr("Saved")
            color: Theme.charging
            font.pixelSize: Theme.labelSize
            font.weight: Theme.weightMedium
        }
        Behavior on opacity {
            NumberAnimation { duration: Theme.motionBase; easing.type: Theme.motionEasing }
        }
        // Hold ~2s then fade out (UI-SPEC motion contract).
        Timer {
            id: savedHold
            interval: 2000
            onTriggered: savedTick.opacity = 0
        }
        Connections {
            target: configState
            function onSaved() { savedTick.opacity = 1; savedHold.restart() }
        }
    }

    // --- Inline Save error copy (mapped in C++; the exact strings are above) ---
    Text {
        id: errorText
        Layout.fillWidth: true
        visible: configState.lastError.length > 0
        // Prefer the C++-mapped lastError (already one of the two strings above);
        // keep wrapping for the long copy.
        text: configState.lastError
        color: Theme.destructive
        font.pixelSize: Theme.labelSize
        wrapMode: Text.WordWrap
        elide: Text.ElideRight
    }

    Item { Layout.fillWidth: true; visible: !errorText.visible }

    // --- Restore defaults... (secondary/destructive) ---
    Button {
        id: restoreBtn
        text: qsTr("Restore defaults…")
        flat: true
        contentItem: Text {
            text: restoreBtn.text
            font.pixelSize: Theme.labelSize
            font.weight: Theme.weightMedium
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            color: restoreBtn.hovered ? Theme.destructive : Theme.mutedForeground
        }
        background: Rectangle {
            radius: 8
            color: restoreBtn.hovered ? Theme.destructiveTint : "transparent"
        }
        onClicked: restoreDialog.open()
    }

    // --- Primary Save CTA (accent-filled), BusyIndicator while pending ---
    Button {
        id: saveBtn
        text: qsTr("Save")
        enabled: !configState.saving
        onClicked: configState.save()

        contentItem: Item {
            implicitWidth: saveLabel.implicitWidth + 2 * Theme.spacingMd
            implicitHeight: Theme.controlRowHeight - Theme.spacingSm
            Text {
                id: saveLabel
                anchors.centerIn: parent
                visible: !configState.saving
                text: saveBtn.text
                color: "#FFFFFF"
                font.pixelSize: Theme.labelSize
                font.weight: Theme.weightMedium
            }
            BusyIndicator {
                anchors.centerIn: parent
                visible: configState.saving
                running: configState.saving
                implicitWidth: 20; implicitHeight: 20
            }
        }
        background: Rectangle {
            radius: 8
            // Accent-filled primary CTA (the one phase-primary, UI-SPEC reserved
            // accent #3); dimmed while disabled/saving.
            color: Theme.accent
            opacity: saveBtn.enabled ? (saveBtn.hovered ? 0.92 : 1.0) : 0.5
        }
    }

    // --- Restore-defaults confirmation dialog (CONF-02) ---
    RestoreDialog {
        id: restoreDialog
        deviceName: root.deviceName
        activeProfile: root.activeProfile
    }
}
