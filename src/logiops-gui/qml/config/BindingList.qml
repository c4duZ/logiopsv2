// Copyright 2019-2023 PixlOne — Adapted by c4duZ
//
// BindingList (BTN-04) — the synced button->current-binding list beside the
// device render. Each row (rowHeight 56) shows the button name (Label 14
// DemiBold), its current-binding summary (Body 16), and an action-type glyph.
// Hover/selection sync with the render by ControlID: hovering a row raises
// hoverControl(cid) and the parent echoes hoveredControlId so the matching
// hotspot highlights (and vice versa). Clicking a remappable row opens the
// reassign panel; non-remappable rows are muted and inert (BTN-01).
import QtQuick
import QtQuick.Controls
import logiops.gui

Item {
    id: root

    property var buttonsModel: null
    property int hoveredControlId: -1
    property int editingControlId: -1

    signal hoverControl(int controlId)
    signal activateControl(int row, int controlId)

    // Map an action type to its bundled glyph (Plan 01 glyph set).
    function glyphFor(type) {
        switch (type) {
        case "Keypress":          return "qrc:/logiops/gui/icons/keystroke.svg";
        case "ChangeDPI":
        case "CycleDPI":          return "qrc:/logiops/gui/icons/dpi.svg";
        case "ChangeHost":        return "qrc:/logiops/gui/icons/host.svg";
        case "ChangeProfile":     return "qrc:/logiops/gui/icons/profile.svg";
        case "ToggleSmartShift":  return "qrc:/logiops/gui/icons/smartshift.svg";
        case "ToggleHiresScroll": return "qrc:/logiops/gui/icons/hires.svg";
        default:                  return "qrc:/logiops/gui/icons/disabled.svg";
        }
    }

    ListView {
        id: view
        anchors.fill: parent
        clip: true
        model: root.buttonsModel
        spacing: 0
        boundsBehavior: Flickable.StopAtBounds

        delegate: Item {
            id: row
            required property int controlId
            required property bool remappable
            required property string buttonName
            required property string currentActionType
            required property string currentActionSummary
            required property int index

            width: ListView.view.width
            height: Theme.rowHeight

            readonly property bool isHovered: root.hoveredControlId === controlId
            readonly property bool isEditing: root.editingControlId === controlId

            Rectangle {
                anchors.fill: parent
                color: row.isEditing ? Theme.accentTint
                       : (row.isHovered ? Theme.hoverTint : "transparent")
                Behavior on color {
                    ColorAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic }
                }
            }

            Row {
                anchors.fill: parent
                anchors.leftMargin: Theme.spacingMd
                anchors.rightMargin: Theme.spacingMd
                spacing: Theme.spacingMd

                // Action-type glyph.
                Image {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 24; height: 24
                    sourceSize.width: 24; sourceSize.height: 24
                    source: root.glyphFor(row.currentActionType)
                    opacity: row.remappable ? 1.0 : 0.5
                }

                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 2
                    Text {
                        text: row.buttonName
                        font.pixelSize: Theme.labelSize
                        font.weight: Theme.weightMedium
                        color: row.remappable ? Theme.foreground : Theme.mutedForeground
                    }
                    Text {
                        text: row.currentActionSummary
                        font.pixelSize: Theme.bodySize
                        color: Theme.mutedForeground
                    }
                }
            }

            // Hairline divider between rows.
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Theme.hairline
            }

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: row.remappable ? Qt.PointingHandCursor : Qt.ArrowCursor
                onEntered: root.hoverControl(row.controlId)
                onExited: {
                    if (root.hoveredControlId === row.controlId)
                        root.hoverControl(-1);
                }
                onClicked: {
                    if (row.remappable)
                        root.activateControl(row.index, row.controlId);
                }
            }

            // Non-remappable rows explain themselves on hover (UI-SPEC copy).
            ToolTip {
                visible: !row.remappable && inertHover.containsMouse
                text: qsTr("This button can't be reassigned on this device.")
                delay: 300
            }
            MouseArea {
                id: inertHover
                anchors.fill: parent
                hoverEnabled: true
                enabled: !row.remappable
                acceptedButtons: Qt.NoButton
            }
        }
    }
}
