// Copyright 2019-2023 PixlOne — Adapted by c4duZ
//
// ConfigTabs: the StackLayout host for the four config tabs (UI-01). It receives
// the active tab index and the per-device DeviceController and shows exactly one
// tab body at a time, cross-fading on switch (motionFast, UI-SPEC). Wave 2 (Plans
// 02 Buttons / 03 Pointer+Scroll / 04 Profiles) replaces each placeholder Item
// with the real tab content; the placeholders keep the shell compiling and
// renderable standalone now.
//
// Capability omission is decided by DetailPane (which owns the TabBar) and passed
// down as the `tabKeys` list, so the StackLayout indices stay in lock-step with
// the visible TabButtons even when a whole tab is omitted (UI-SPEC: omit, never
// show-disabled, an absent capability).
import QtQuick
import QtQuick.Layouts
import logiops.gui

Item {
    id: root

    // The active tab index (bound to the TabBar in DetailPane).
    property int currentIndex: 0
    // The per-device controller (capabilities + live values). May be null when no
    // device is selected.
    property var controller: null
    // The ordered keys of the currently VISIBLE tabs (e.g. ["buttons","pointer",
    // "scroll","profiles"] minus any omitted). DetailPane computes this from the
    // controller's capability flags so both views agree on indices.
    property var tabKeys: ["buttons", "pointer", "scroll", "profiles"]

    function keyAt(i) {
        return (i >= 0 && i < tabKeys.length) ? tabKeys[i] : "";
    }

    StackLayout {
        id: stack
        anchors.fill: parent
        currentIndex: root.currentIndex

        // One body per visible tab. Wave 2 fills these in; "buttons" now loads the
        // real ButtonsTab (Plan 02), the rest stay placeholders until their plans.
        Repeater {
            model: root.tabKeys
            delegate: Item {
                id: tabBody
                required property string modelData
                required property int index

                // Cross-fade content on tab switch (UI-SPEC motionFast).
                opacity: stack.currentIndex === index ? 1 : 0
                Behavior on opacity {
                    NumberAnimation { duration: Theme.motionFast; easing.type: Theme.motionEasing }
                }

                // Real content for implemented tabs; placeholder label otherwise.
                Loader {
                    anchors.fill: parent
                    active: tabBody.modelData === "buttons"
                    sourceComponent: tabBody.modelData === "buttons" ? buttonsTabComponent : null
                }

                Text {
                    anchors.centerIn: parent
                    visible: tabBody.modelData !== "buttons"
                    text: {
                        switch (tabBody.modelData) {
                        case "pointer":  return qsTr("Pointer");
                        case "scroll":   return qsTr("Scroll");
                        case "profiles": return qsTr("Profiles");
                        default:         return tabBody.modelData;
                        }
                    }
                    color: Theme.mutedForeground
                    font.pixelSize: Theme.bodySize
                }
            }
        }
    }

    // The Buttons tab content (BTN-01..04, HOST-01). Instantiated lazily by the
    // Loader above so non-Buttons tabs cost nothing.
    Component {
        id: buttonsTabComponent
        ButtonsTab {
            controller: root.controller
            buttonsModel: deviceControllerFactory.buttonsModel
        }
    }
}
