// Copyright 2019-2023 PixlOne — Adapted by c4duZ
//
// The Phase 3 tabbed detail pane (UI-01). A persistent header (device icon, name,
// model sub-line, live connection/battery line) sits ABOVE a sliding TabBar
// (Buttons / Pointer / Scroll / Profiles) hosting a cross-fading ConfigTabs
// StackLayout. Capability-absent tabs are OMITTED entirely (UI-SPEC), gated on the
// per-device DeviceController (deviceControllerFactory.controller). Wave 2 fills
// the four tab placeholders. C++ owns all data + capability discovery; this is
// pure rendering.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import logiops.gui

Rectangle {
    id: pane
    color: Theme.dominant

    property int currentIndex: -1

    readonly property bool hasSelection: currentIndex >= 0 && currentIndex < deviceModel.rowCount()
    readonly property var controller: deviceControllerFactory.controller

    // "Select a device" quiet prompt when nothing is selected (not a full error).
    Text {
        anchors.centerIn: parent
        visible: !pane.hasSelection
        text: qsTr("Select a device")
        color: Theme.mutedForeground
        font.pixelSize: Theme.bodySize
    }

    // Hidden Repeater bound to the model lets us read the selected row's roles
    // declaratively (C++ owns the data; this is pure rendering).
    Repeater {
        id: rows
        model: deviceModel
        // The hidden delegates instantiate asynchronously: when a device is
        // auto-selected, currentIndex can reach a valid row BEFORE this Repeater
        // has built its Item for that row, so selectedItem() (and thus the path
        // handed to the DeviceController factory) would resolve empty and the
        // per-device controller would never be built. Re-run the path sync once
        // the delegates exist so capability discovery fires on the real path.
        onCountChanged: pane.syncController()
        delegate: Item {
            readonly property bool current: index === pane.currentIndex
            readonly property string rPath: model.path
            readonly property string rName: model.name
            readonly property string rModel: model.modelName
            readonly property int rKind: model.deviceKind
            readonly property int rConn: model.connectionState
            readonly property bool rBattKnown: model.batteryKnown
            readonly property bool rCharging: model.charging
            // T-02-15: clamp out-of-range percentage to 0..100.
            readonly property int rBattPercent: Math.max(0, Math.min(100, model.batteryPercent))
            visible: false
        }
    }

    function selectedItem() {
        for (var i = 0; i < rows.count; ++i) {
            var it = rows.itemAt(i);
            if (it && it.current)
                return it;
        }
        return null;
    }

    // Drive the per-device DeviceController off the selected row's path. C++
    // (the factory) builds + introspects a fresh controller per device; QML only
    // hands it the path on selection change.
    function syncController() {
        var it = pane.selectedItem();
        var path = it ? it.rPath : "";
        deviceControllerFactory.selectDevice(path);
        // Point the global ConfigState's restore target at the selected device
        // (.Device.ClearProfile for Restore-defaults, CONF-02).
        configState.setDevicePath(path);
    }
    onCurrentIndexChanged: syncController()
    Component.onCompleted: syncController()

    // --- Visible-tab gating (UI-SPEC: omit, never show-disabled, an absent
    // capability). The Buttons tab needs remappable buttons; Pointer needs DPI;
    // Scroll needs any of smartshift / hires / thumbwheel; Profiles is always
    // available. Both the TabBar and ConfigTabs read this single source so their
    // indices stay in lock-step. ---
    readonly property var tabKeys: {
        var keys = [];
        var c = pane.controller;
        if (c && c.hasButtons) keys.push("buttons");
        if (c && c.hasDpi) keys.push("pointer");
        if (c && (c.hasSmartShift || c.hasHires || c.hasThumbwheel)) keys.push("scroll");
        keys.push("profiles");
        return keys;
    }

    function tabLabel(key) {
        switch (key) {
        case "buttons":  return qsTr("Buttons");
        case "pointer":  return qsTr("Pointer");
        case "scroll":   return qsTr("Scroll");
        case "profiles": return qsTr("Profiles");
        default:         return key;
        }
    }

    ColumnLayout {
        id: content
        visible: pane.hasSelection
        anchors.fill: parent
        anchors.margins: Theme.spacingLg
        spacing: Theme.spacingMd

        // --- Persistent header: icon + name + model sub-line + connection chip. ---
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingMd

            Image {
                Layout.preferredWidth: Theme.iconContainer
                Layout.preferredHeight: Theme.iconContainer
                sourceSize.width: Theme.iconContainer; sourceSize.height: Theme.iconContainer
                source: {
                    var it = pane.selectedItem();
                    return (it && it.rKind === 1)
                        ? "qrc:/logiops/gui/icons/keyboard.svg"
                        : "qrc:/logiops/gui/icons/mouse.svg";
                }
                fillMode: Image.PreserveAspectFit
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingXs
                Text {
                    text: { var it = pane.selectedItem(); return it ? it.rName : ""; }
                    textFormat: Text.PlainText
                    color: Theme.foreground
                    font.pixelSize: Theme.displaySize
                    font.weight: Theme.weightMedium
                }
                Text {
                    text: { var it = pane.selectedItem(); return it ? it.rModel : ""; }
                    textFormat: Text.PlainText
                    color: Theme.mutedForeground
                    font.pixelSize: Theme.labelSize
                    font.weight: Theme.weightMedium
                }
            }

            // Live connection / battery chip (reuses the Phase 2 threshold colors).
            Text {
                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                text: {
                    var it = pane.selectedItem();
                    if (!it) return "";
                    if (it.rConn === 2) return qsTr("Offline");
                    if (it.rConn === 1) return qsTr("Sleeping");
                    if (!it.rBattKnown) return qsTr("Connected");
                    return it.rBattPercent + "%" + (it.rCharging ? qsTr(" — charging") : "");
                }
                color: {
                    var it = pane.selectedItem();
                    if (!it || it.rConn === 2 || it.rConn === 1 || !it.rBattKnown) return Theme.mutedForeground;
                    return it.rBattPercent > 20 ? Theme.charging : Theme.warning;
                }
                font.pixelSize: Theme.labelSize
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.hairline }

        // --- Persistent persistence toolbar (CONF-01 / CONF-02): the unsaved pill,
        // the async polkit-gated Save CTA, and the Restore-defaults entry. Sits
        // ABOVE the tabs so it is always reachable while editing any tab (UI-SPEC). ---
        SaveToolbar {
            Layout.fillWidth: true
            deviceName: { var it = pane.selectedItem(); return it ? it.rName : ""; }
            activeProfile: deviceControllerFactory.profilesModel
                ? deviceControllerFactory.profilesModel.activeProfile : ""
        }

        // --- Sliding TabBar (UI-SPEC: tabBarHeight 48, 2px accent underline that
        // slides between tabs, motionBase/OutCubic; selected = accent DemiBold,
        // unselected = mutedForeground -> foreground on hover). ---
        TabBar {
            id: tabBar
            Layout.fillWidth: true
            implicitHeight: Theme.tabBarHeight
            spacing: 0
            background: Rectangle { color: "transparent" }

            // Clamp selection when the visible tab set shrinks (capability change).
            Connections {
                target: pane
                function onTabKeysChanged() {
                    if (tabBar.currentIndex >= pane.tabKeys.length)
                        tabBar.currentIndex = Math.max(0, pane.tabKeys.length - 1);
                }
            }

            Repeater {
                model: pane.tabKeys
                delegate: TabButton {
                    id: tabButton
                    required property string modelData
                    required property int index
                    readonly property bool selected: tabBar.currentIndex === index
                    height: Theme.tabBarHeight

                    contentItem: Text {
                        text: pane.tabLabel(tabButton.modelData)
                        font.pixelSize: Theme.labelSize
                        font.weight: tabButton.selected ? Theme.weightMedium : Theme.weightRegular
                        color: tabButton.selected
                            ? Theme.accent
                            : (tabButton.hovered ? Theme.foreground : Theme.mutedForeground)
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: tabButton.hovered && !tabButton.selected ? Theme.hoverTint : "transparent"
                    }
                }
            }

            // 2px accent underline that SLIDES between tabs (motionBase/OutCubic).
            Rectangle {
                height: 2
                color: Theme.accent
                y: tabBar.height - height
                readonly property var curItem: tabBar.contentItem.children.length > tabBar.currentIndex
                    ? tabBar.contentItem.children[tabBar.currentIndex] : null
                x: curItem ? curItem.x : 0
                width: curItem ? curItem.width : 0
                visible: pane.tabKeys.length > 0
                Behavior on x { NumberAnimation { duration: Theme.motionBase; easing.type: Easing.OutCubic } }
                Behavior on width { NumberAnimation { duration: Theme.motionBase; easing.type: Easing.OutCubic } }
            }
        }

        // Hairline track under the tab strip.
        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.hairline }

        // --- Cross-fading tab content host. ---
        ConfigTabs {
            id: tabs
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabBar.currentIndex
            controller: pane.controller
            tabKeys: pane.tabKeys
        }

        // Cross-fade the whole populated body on selection change (UI-SPEC).
        opacity: pane.hasSelection ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: Theme.motionFast; easing.type: Theme.motionEasing } }
    }
}
