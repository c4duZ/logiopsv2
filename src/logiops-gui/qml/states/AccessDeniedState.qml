// Copyright 2019-2023 PixlOne — Adapted by c4duZ
//
// Access-denied onboarding full-window state: connected to the bus but the
// D-Bus policy rejected the call (user not in the logiops group). DISTINCT from
// daemon-down (CONTEXT.md: must be a separate onboarding, not a generic error).
// Shows the usermod command block + a working "Copy command" CTA
// (daemon.copyUsermodCommand) and the re-login note.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import logiops.gui

Rectangle {
    color: Theme.dominant

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - 2 * Theme.spacingLg, Theme.stateMaxWidth)
        spacing: Theme.spacingLg

        Text {
            Layout.fillWidth: true
            text: qsTr("You're not in the logiops group yet")
            color: Theme.foreground
            font.pixelSize: Theme.displaySize
            font.weight: Theme.weightMedium
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }
        Text {
            Layout.fillWidth: true
            text: qsTr("To control your devices, add your user to the logiops group and sign back in.")
            color: Theme.mutedForeground
            font.pixelSize: Theme.bodySize
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }

        // Monospace command block.
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: cmd.implicitHeight + 2 * Theme.spacingMd
            color: Theme.secondary
            border.color: Theme.hairline
            border.width: 1
            radius: 6
            Text {
                id: cmd
                anchors.fill: parent
                anchors.margins: Theme.spacingMd
                text: "sudo usermod -aG logiops $USER"
                textFormat: Text.PlainText
                color: Theme.foreground
                font.pixelSize: Theme.labelSize
                font.family: "monospace"
                verticalAlignment: Text.AlignVCenter
                wrapMode: Text.WrapAnywhere
            }
        }

        Button {
            id: copyBtn
            Layout.alignment: Qt.AlignHCenter
            // Label briefly swaps to "Copied" then reverts (~1.5s, UI-SPEC).
            property bool copied: false
            text: copied ? qsTr("Copied") : qsTr("Copy command")
            Accessible.name: text
            onClicked: {
                daemon.copyUsermodCommand();
                copied = true;
                revert.restart();
            }
            Timer { id: revert; interval: 1500; onTriggered: copyBtn.copied = false }

            contentItem: Text {
                text: copyBtn.text
                color: "white"
                font.pixelSize: Theme.labelSize
                font.weight: Theme.weightMedium
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                implicitHeight: 40
                implicitWidth: 160
                radius: 6
                color: Theme.accent
                opacity: copyBtn.down ? 0.85 : 1.0
            }
        }

        Text {
            Layout.fillWidth: true
            text: qsTr("Then log out and back in (or reboot) for it to take effect.")
            color: Theme.mutedForeground
            font.pixelSize: Theme.labelSize
            font.weight: Theme.weightMedium
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }
    }
}
