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
//
// Responsive (260531-fye): a tiling WM / XWayland can force the window below the
// 720px min, at which point the render + 360px reassign panel sitting side by
// side overflow and the panel's controls clip off-screen (unreachable). When the
// tab is `narrow` (width < windowMinWidth) AND a row is being edited, the
// ReassignPanel becomes a FULL-WIDTH overlay covering the render+list instead of
// a side panel, so it is always fully on-screen and its Close button reachable.
// The wide (side-by-side) layout is unchanged.
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

    // Responsive breakpoint: below the window min, the render + side panel no
    // longer both fit, so the reassign panel switches to a full-width overlay.
    readonly property bool narrow: root.width < Theme.windowMinWidth
    // True when the panel must take over the whole tab (narrow AND editing).
    readonly property bool panelOverlay: root.narrow && root.editingRow >= 0

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

        // Left: device render with clickable hotspots (BTN-01). Collapsed to 0
        // width while the panel is a full-tab overlay (narrow + editing) so the
        // panel owns the whole tab and nothing renders behind/beside it.
        DeviceRender {
            id: render
            Layout.preferredWidth: root.panelOverlay ? 0 : Theme.deviceRenderMax
            Layout.maximumWidth: root.panelOverlay ? 0 : Theme.deviceRenderMax
            Layout.fillHeight: true
            visible: Layout.preferredWidth > 0
            buttonsModel: root.buttonsModel
            hoveredControlId: root.hoveredControlId
            editingControlId: root.editingControlId
            onHoverControl: function (controlId) { root.hoveredControlId = controlId; }
            onActivateControl: function (row, controlId) { root.openReassign(row, controlId); }
            Behavior on Layout.preferredWidth {
                NumberAnimation { duration: Theme.motionAdd; easing.type: Easing.OutCubic }
            }
        }

        // Middle: the synced binding list (BTN-04). Hidden while the panel is a
        // full-tab overlay so the panel is the only thing on screen.
        BindingList {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !root.panelOverlay
            buttonsModel: root.buttonsModel
            hoveredControlId: root.hoveredControlId
            editingControlId: root.editingControlId
            onHoverControl: function (controlId) { root.hoveredControlId = controlId; }
            onActivateControl: function (row, controlId) { root.openReassign(row, controlId); }
        }

        // Right: non-modal reassign panel (BTN-02/03, HOST-01).
        // Wide: width animates 0 -> actionPanelWidth (360) so the render stays
        //   visible beside it (UI-SPEC).
        // Narrow + editing: fillWidth so the panel takes the whole tab as an
        //   overlay (its controls can never clip off-window).
        ReassignPanel {
            id: panel
            Layout.fillHeight: true
            Layout.fillWidth: root.panelOverlay
            // In overlay mode width is driven by fillWidth, so the preferred
            // width is only the side-panel reveal value (0 when not editing).
            Layout.preferredWidth: root.panelOverlay
                ? 0
                : (root.editingRow >= 0 ? Theme.actionPanelWidth : 0)
            clip: true
            visible: root.editingRow >= 0
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
