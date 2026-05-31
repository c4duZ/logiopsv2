// Copyright 2019-2023 PixlOne — Adapted by c4duZ
//
// ButtonsTab (BTN-01..04, HOST-01) — the headline interaction of Phase 3:
// "click a button, reassign it." Two synced views side by side (stacked
// vertically below windowMinWidth): a clickable device render (DeviceRender) and
// a button->binding list (BindingList), with a non-modal reassign panel
// (ReassignPanel) that slides in when a remappable hotspot/row is chosen. Hover
// and selection are kept in sync across the render and the list by ControlID
// (NOT enumeration index — RESEARCH Pitfall 4). All apply is LIVE via
// buttonsModel; QML renders only.
import QtQuick
import QtQuick.Layouts
import logiops.gui

Item {
    id: root

    // The per-device controller (capabilities + hostCount source), injected by
    // ConfigTabs.
    property var controller: null
    // The per-device Buttons model (.Buttons.Enumerate rows + two-step setAction),
    // injected by ConfigTabs from the factory.
    property var buttonsModel: null

    // --- Shared selection / hover state (CID-keyed so render and list agree). ---
    // -1 means "no hotspot is hovered / being edited".
    property int hoveredControlId: -1
    property int editingRow: -1
    property int editingControlId: -1

    function openReassign(row, controlId) {
        root.editingRow = row;
        root.editingControlId = controlId;
    }
    function closeReassign() {
        root.editingRow = -1;
        root.editingControlId = -1;
    }

    // Empty state: device reports no remappable buttons at all (UI-SPEC copy).
    readonly property bool hasAnyRemappable: {
        if (!buttonsModel)
            return false;
        for (var i = 0; i < buttonsModel.count; ++i) {
            var idx = buttonsModel.index(i, 0);
            if (buttonsModel.data(idx, 0x0103)) // RemappableRole (UserRole+3)
                return true;
        }
        return false;
    }

    Text {
        anchors.centerIn: parent
        visible: buttonsModel && buttonsModel.count === 0
        text: qsTr("This device has no reassignable buttons.")
        color: Theme.mutedForeground
        font.pixelSize: Theme.bodySize
    }

    // --- The two synced views + the reassign panel. ---
    RowLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingLg
        spacing: Theme.spacingXl
        visible: buttonsModel && buttonsModel.count > 0

        // Left: device render with clickable hotspots (BTN-01).
        DeviceRender {
            id: render
            Layout.preferredWidth: Theme.deviceRenderMax
            Layout.maximumWidth: Theme.deviceRenderMax
            Layout.fillHeight: true
            buttonsModel: root.buttonsModel
            hoveredControlId: root.hoveredControlId
            editingControlId: root.editingControlId
            onHoverControl: function (controlId) { root.hoveredControlId = controlId; }
            onActivateControl: function (row, controlId) { root.openReassign(row, controlId); }
        }

        // Middle: the synced binding list (BTN-04).
        BindingList {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            buttonsModel: root.buttonsModel
            hoveredControlId: root.hoveredControlId
            editingControlId: root.editingControlId
            onHoverControl: function (controlId) { root.hoveredControlId = controlId; }
            onActivateControl: function (row, controlId) { root.openReassign(row, controlId); }
        }

        // Right: non-modal reassign panel (BTN-02/03, HOST-01). Width animates in
        // from 0 -> actionPanelWidth so the render stays visible (UI-SPEC).
        ReassignPanel {
            id: panel
            Layout.fillHeight: true
            Layout.preferredWidth: root.editingRow >= 0 ? Theme.actionPanelWidth : 0
            clip: true
            visible: Layout.preferredWidth > 0
            controller: root.controller
            buttonsModel: root.buttonsModel
            row: root.editingRow
            onClosed: root.closeReassign()
            Behavior on Layout.preferredWidth {
                NumberAnimation { duration: Theme.motionAdd; easing.type: Easing.OutCubic }
            }
        }
    }
}
