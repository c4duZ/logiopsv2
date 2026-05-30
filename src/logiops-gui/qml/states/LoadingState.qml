// Copyright 2019-2023 PixlOne — Adapted by c4duZ
//
// Loading full-window state. Holds until the first Enumerate completes so the
// empty state never flashes during initial enumeration (UI-SPEC).
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

        BusyIndicator {
            running: true
            Layout.alignment: Qt.AlignHCenter
        }
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Looking for your devices…")
            color: Theme.foreground
            font.pixelSize: Theme.displaySize
            font.weight: Theme.weightMedium
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
    }
}
