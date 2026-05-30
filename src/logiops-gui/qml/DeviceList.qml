// Copyright 2019-2023 PixlOne — Adapted by c4duZ
//
// The sidebar device list: a ListView bound to the C++ DeviceModel. Keyboard
// operable, single-select, with fade+slide add/remove transitions (UI-SPEC).
// Sort order is owned by C++ (stable nickname sort) — this view never re-sorts.
import QtQuick
import QtQuick.Controls
import logiops.gui

Rectangle {
    id: root
    color: Theme.secondary

    // Selected row, driven from Main; emits deviceSelected when the user picks one.
    property int currentIndex: -1
    signal deviceSelected(int row)

    // 1px hairline between the sidebar and the detail pane.
    Rectangle {
        anchors { top: parent.top; bottom: parent.bottom; right: parent.right }
        width: 1
        color: Theme.hairline
    }

    ListView {
        id: list
        anchors.fill: parent
        anchors.rightMargin: 1
        model: deviceModel
        currentIndex: root.currentIndex
        keyNavigationEnabled: true
        focus: true
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        delegate: DeviceDelegate {
            width: ListView.view.width
            selected: index === root.currentIndex
            onClicked: root.deviceSelected(index)
        }

        // Keyboard activation: Enter/Space select the focused row.
        Keys.onReturnPressed: root.deviceSelected(list.currentIndex)
        Keys.onSpacePressed: root.deviceSelected(list.currentIndex)

        // Add: fade 0->1 + slide down 8px->0 over 220ms ease-out (UI-SPEC).
        add: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.motionAdd; easing.type: Theme.motionEasing }
            NumberAnimation { property: "y"; from: -8; duration: Theme.motionAdd; easing.type: Theme.motionEasing }
        }
        // Remove: fade 1->0 over 200ms (UI-SPEC).
        remove: Transition {
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Theme.motionBase; easing.type: Theme.motionEasing }
        }
        // Existing rows ease to their new position so an insert/remove doesn't jump.
        displaced: Transition {
            NumberAnimation { properties: "y"; duration: Theme.motionBase; easing.type: Theme.motionEasing }
        }
    }
}
