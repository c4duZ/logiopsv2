// Copyright 2019-2023 PixlOne — Adapted by c4duZ
//
// PointerTab — Phase 3 Wave 1 PLACEHOLDER. The tabbed shell (Plan 01) ships this empty
// tab body so the StackLayout compiles and renders standalone; Wave 2 (Plan 03)
// replaces it with the real content, binding to the DeviceController passed in.
import QtQuick
import logiops.gui

Item {
    id: root
    // The per-device controller (capabilities + live values), injected by ConfigTabs.
    property var controller: null

    Text {
        anchors.centerIn: parent
        text: qsTr("Pointer")
        color: Theme.mutedForeground
        font.pixelSize: Theme.bodySize
    }
}
