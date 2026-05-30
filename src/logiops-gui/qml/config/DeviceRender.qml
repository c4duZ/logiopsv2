// Copyright 2019-2023 PixlOne — Adapted by c4duZ
//
// DeviceRender (BTN-01) — the clickable device picture. A per-model SVG with
// hotspots would slot in here; until per-model artwork exists we draw a GENERIC
// annotated mouse diagram and lay the button hotspots out around it. Hotspots are
// keyed by ControlID (NOT enumeration index — RESEARCH Pitfall 4) and synced with
// the binding list: hovering a hotspot raises hoverControl(cid); the parent
// echoes the active CID back via hoveredControlId so list<->render highlight in
// tandem. Non-remappable controls render muted + non-clickable with a tooltip.
//
// Hotspot data format (the per-PID sidecar a real artwork pack would ship):
//   [{ cid: <ControlID>, x: <0..1>, y: <0..1> }]  — x/y are fractional positions
// over the render. Here we synthesize positions from the model order around the
// generic silhouette so every enumerated button still gets a clickable hotspot.
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

    // Generic mouse silhouette + the hotspot overlay. The silhouette is a simple
    // rounded body so the tab is meaningful without per-model SVGs (BTN-01
    // "generic annotated fallback when no per-model artwork exists").
    Rectangle {
        id: stage
        anchors.centerIn: parent
        width: Math.min(parent.width, Theme.deviceRenderMax)
        height: Math.min(parent.height, width * 1.5)
        color: "transparent"

        // Generic mouse body.
        Rectangle {
            id: body
            anchors.centerIn: parent
            width: parent.width * 0.55
            height: parent.height * 0.8
            radius: width * 0.45
            color: Theme.secondary
            border.color: Theme.hairline
            border.width: 1
            // Center seam hinting left/right buttons.
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 1
                height: parent.height * 0.4
                y: parent.height * 0.04
                color: Theme.hairline
            }
        }

        Text {
            anchors.top: body.bottom
            anchors.topMargin: Theme.spacingSm
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("Generic device view")
            color: Theme.mutedForeground
            font.pixelSize: Theme.labelSize
        }

        // One hotspot per enumerated button, positioned around the silhouette.
        Repeater {
            model: root.buttonsModel
            delegate: Item {
                id: hotspot
                // Model roles (ButtonsModel::roleNames).
                required property int controlId
                required property bool remappable
                required property string buttonName
                required property string currentActionSummary
                required property int index

                // Synthesized fractional position: distribute hotspots vertically
                // along the body's right edge (a real artwork pack overrides this
                // via the per-PID {cid,x,y} sidecar).
                readonly property real fx: 0.62
                readonly property real fy: 0.18 + (index * 0.12)

                x: stage.width * fx
                y: stage.height * Math.min(fy, 0.9)
                width: Theme.hotspotMin
                height: Theme.hotspotMin

                readonly property bool isHovered: root.hoveredControlId === controlId
                readonly property bool isEditing: root.editingControlId === controlId

                // Visible marker (default subtle / hover tint / selected accent
                // ring / unsupported muted), per UI-SPEC hotspot states.
                Rectangle {
                    anchors.fill: parent
                    radius: width / 2
                    color: !hotspot.remappable
                           ? "transparent"
                           : (hotspot.isHovered ? Theme.hoverTint : "transparent")
                    border.width: hotspot.isEditing ? 2 : 1
                    border.color: !hotspot.remappable
                                  ? Theme.mutedForeground
                                  : (hotspot.isEditing ? Theme.accent
                                     : (hotspot.isHovered ? Theme.foreground : Theme.hairline))
                    opacity: hotspot.remappable ? 1.0 : 0.45
                    Behavior on color {
                        ColorAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic }
                    }

                    // CID label inside the marker.
                    Text {
                        anchors.centerIn: parent
                        text: hotspot.index + 1
                        font.pixelSize: Theme.labelSize
                        color: hotspot.remappable ? Theme.foreground : Theme.mutedForeground
                    }
                }

                // Invisible >=44px hit area centered on the marker (accessibility:
                // hotspots may render as small as hotspotMin 32 but MUST carry a
                // 44px target — UI-SPEC).
                MouseArea {
                    id: hit
                    anchors.centerIn: parent
                    width: Math.max(parent.width, 44)
                    height: Math.max(parent.height, 44)
                    hoverEnabled: true
                    enabled: hotspot.remappable
                    cursorShape: hotspot.remappable ? Qt.PointingHandCursor : Qt.ArrowCursor
                    onEntered: root.hoverControl(hotspot.controlId)
                    onExited: {
                        if (root.hoveredControlId === hotspot.controlId)
                            root.hoverControl(-1);
                    }
                    onClicked: root.activateControl(hotspot.index, hotspot.controlId)
                }

                // Unsupported / non-remappable: explain why (exact UI-SPEC copy).
                ToolTip {
                    visible: !hotspot.remappable && unsupportedHover.containsMouse
                    text: qsTr("This button can't be reassigned on this device.")
                    delay: 300
                }
                MouseArea {
                    id: unsupportedHover
                    anchors.fill: parent
                    hoverEnabled: true
                    enabled: !hotspot.remappable
                    acceptedButtons: Qt.NoButton
                }
            }
        }
    }
}
