// Copyright 2019-2023 PixlOne — Adapted by c4duZ
//
// Daemon-not-running full-window state: the bus name has no owner. DISTINCT from
// empty and access-denied. The "Retry connection" CTA calls daemon.retry()
// (C++ reconnect; no app restart — DEV-03/CONF-03).
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
            text: qsTr("Can't reach the logid service")
            color: Theme.foreground
            font.pixelSize: Theme.displaySize
            font.weight: Theme.weightMedium
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }
        Text {
            Layout.fillWidth: true
            text: qsTr("The logid background service isn't running. Start it, then retry.")
            color: Theme.mutedForeground
            font.pixelSize: Theme.bodySize
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }
        Text {
            Layout.fillWidth: true
            text: qsTr("Try: sudo systemctl start logid")
            textFormat: Text.PlainText
            color: Theme.mutedForeground
            font.pixelSize: Theme.labelSize
            font.weight: Theme.weightMedium
            font.family: "monospace"
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }

        Button {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: Theme.spacingMd
            text: qsTr("Retry connection")
            Accessible.name: text
            onClicked: daemon.retry()

            contentItem: Text {
                text: parent.text
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
                opacity: parent.down ? 0.85 : 1.0
            }
        }
    }
}
