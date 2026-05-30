// Copyright 2019-2023 PixlOne — Adapted by c4duZ
//
// Empty full-window state: connected to the daemon, zero devices. DISTINCT from
// daemon-down and access-denied (CONTEXT.md requires separate, clear messages).
import QtQuick
import QtQuick.Layouts
import logiops.gui

Rectangle {
    color: Theme.dominant

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - 2 * Theme.spacingLg, Theme.stateMaxWidth)
        spacing: Theme.spacingLg

        Image {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 72
            Layout.preferredHeight: 72
            sourceSize.width: 72; sourceSize.height: 72
            source: "qrc:/logiops/gui/icons/mouse.svg"
            opacity: 0.5
            fillMode: Image.PreserveAspectFit
        }
        Text {
            Layout.fillWidth: true
            text: qsTr("No Logitech devices found")
            color: Theme.foreground
            font.pixelSize: Theme.displaySize
            font.weight: Theme.weightMedium
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }
        Text {
            Layout.fillWidth: true
            text: qsTr("Connect a Logitech mouse or keyboard — or turn on a paired one — and it'll show up here automatically.")
            color: Theme.mutedForeground
            font.pixelSize: Theme.bodySize
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }
    }
}
