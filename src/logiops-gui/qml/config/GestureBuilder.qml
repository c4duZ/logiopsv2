// Copyright 2024 c4duZ — https://github.com/c4duZ/logiopsv2
//
// GestureBuilder (GEST-01..04) — the guided gesture builder, rendered inline in
// ReassignPanel's "Gesture" category. Per the Phase 4 UI-SPEC: a 4-cardinal
// direction cross -> plain-language mode pills -> action sub-section ->
// granularity slider with a human readout -> a first-class live preview card,
// plus a destructive-lite "Clear". It binds the injected `gestureModel` (Plan 02)
// for ALL state and copy — QML renders only; there is ZERO business logic here.
// The preview sentence, granularity readout, and mode labels all come from the
// C++ model. Every edit live-applies via the model and dirties the inherited
// ConfigState (no Save button here — the window toolbar Save is the only path).
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import logiops.gui

ColumnLayout {
    id: root

    // The button-scoped gesture brain (Plan 02), handed in by ReassignPanel from
    // the controller. May be null while the model is being built; every binding
    // guards for that.
    property var gestureModel: null

    // The action-picker reuse hook: ReassignPanel passes a callback that opens its
    // existing categorized action chooser scoped to "the action this direction
    // fires" (we do NOT build a parallel picker). Signature: chooseAction(direction).
    signal chooseActionRequested(string direction)

    spacing: Theme.spacingMd

    // The active direction the body edits, mirrored from the model so the body
    // re-binds when a cell is chosen.
    readonly property string activeDirection: gestureModel ? gestureModel.activeDirection : "up"
    // The active direction's current plain-language mode (drives progressive
    // disclosure). Empty until a mode is chosen. Bound to the NOTIFYable model
    // property (NOT the bare modeOf() method) so the disclosure sections re-bind
    // when the mode of the current direction changes, not only on direction switch
    // (WR-02). The model emits previewChanged on setMode/setActiveDirection.
    readonly property string activeMode: gestureModel ? gestureModel.activeMode : ""

    // Capitalized human label for a direction ("Up"/"Down"/"Left"/"Right").
    function dirLabel(d) {
        if (d === "up") return qsTr("Up");
        if (d === "down") return qsTr("Down");
        if (d === "left") return qsTr("Left");
        if (d === "right") return qsTr("Right");
        return d;
    }
    // Arrow glyph rotation per direction (the single arrow asset points up = 0).
    function dirRotation(d) {
        if (d === "down") return 180;
        if (d === "left") return 270;
        if (d === "right") return 90;
        return 0;
    }

    // Builder intro helper (UI-SPEC §Copywriting), muted.
    Text {
        Layout.fillWidth: true
        text: qsTr("Pick a direction, then choose what it does.")
        font.pixelSize: Theme.labelSize
        color: Theme.mutedForeground
        wrapMode: Text.WordWrap
    }

    // ----------------------------------------------------------------------
    // 1 — DIRECTION CROSS (§1, GEST-01). A 3x3 grid; only the 4 edge-center
    // cells are interactive at gestureDirCell 44px; corners empty; center holds
    // the mouse glyph to anchor the spatial metaphor.
    // ----------------------------------------------------------------------
    GridLayout {
        Layout.alignment: Qt.AlignHCenter
        columns: 3
        rowSpacing: Theme.spacingXs
        columnSpacing: Theme.spacingXs

        // Row 0: empty, up, empty
        Item { width: Theme.gestureDirCell; height: Theme.gestureDirCell }
        DirCell { direction: "up" }
        Item { width: Theme.gestureDirCell; height: Theme.gestureDirCell }
        // Row 1: left, center(mouse), right
        DirCell { direction: "left" }
        Item {
            width: Theme.gestureDirCell; height: Theme.gestureDirCell
            Image {
                anchors.centerIn: parent
                width: 22; height: 22
                sourceSize.width: 22; sourceSize.height: 22
                source: "qrc:/logiops/gui/icons/mouse.svg"
                opacity: 0.5
            }
        }
        DirCell { direction: "right" }
        // Row 2: empty, down, empty
        Item { width: Theme.gestureDirCell; height: Theme.gestureDirCell }
        DirCell { direction: "down" }
        Item { width: Theme.gestureDirCell; height: Theme.gestureDirCell }
    }

    // Active-direction label (Heading 20) names the current target (§1).
    Text {
        Layout.fillWidth: true
        Layout.topMargin: Theme.spacingSm
        text: root.dirLabel(root.activeDirection)
        font.pixelSize: Theme.headingSize
        font.weight: Theme.weightMedium
        color: Theme.foreground
    }

    // ----------------------------------------------------------------------
    // The builder BODY below cross-fades on direction switch (§1). We re-key it
    // on activeDirection so the opacity animation re-runs.
    // ----------------------------------------------------------------------
    ColumnLayout {
        id: body
        Layout.fillWidth: true
        spacing: Theme.spacingMd

        // Cross-fade the whole body when the direction changes (motionFast 150ms).
        opacity: 1.0
        Connections {
            target: root
            function onActiveDirectionChanged() { bodyFade.restart(); }
        }
        SequentialAnimation {
            id: bodyFade
            NumberAnimation { target: body; property: "opacity"; to: 0.0; duration: Theme.motionFast / 2; easing.type: Easing.OutCubic }
            NumberAnimation { target: body; property: "opacity"; to: 1.0; duration: Theme.motionFast / 2; easing.type: Easing.OutCubic }
        }

        // --- 2 — MODE PILLS (§2, GEST-01/03). Four single-select pills in the
        // LOCKED order/labels straight from the model (single source of truth). ---
        Text {
            text: qsTr("Mode")
            font.pixelSize: Theme.labelSize
            font.weight: Theme.weightMedium
            color: Theme.foreground
        }
        // Mode options as a full-width vertical list. A Flow (positioner) inside a
        // ColumnLayout collapses to height 0 — the Layout never allocates vertical
        // space for the positioner's wrapped content. A ColumnLayout of fillWidth
        // pills lays out reliably and reads better for the long mode labels.
        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingSm
            Repeater {
                id: pillRepeater
                model: root.gestureModel ? root.gestureModel.plainModes() : []
                delegate: ModePill {
                    required property string modelData
                    Layout.fillWidth: true
                    Layout.preferredHeight: Theme.controlRowHeight
                    label: modelData
                    selected: root.activeMode === modelData
                    onChosen: if (root.gestureModel) root.gestureModel.setMode(root.activeDirection, modelData)
                }
            }
        }

        // Progressive disclosure (§2): which sub-sections this mode needs.
        readonly property bool showsAction: root.activeMode === qsTr("Repeat while moving")
                                            || root.activeMode === qsTr("Do once when moved far enough")
        readonly property bool showsGranularity: showsAction
                                                 || root.activeMode === qsTr("Adjust proportionally")

        // --- 3 — ACTION SUB-SECTION (§3, GEST-01). Reuses the SAME categorized
        // chooser as button reassignment (no parallel picker). ---
        ColumnLayout {
            Layout.fillWidth: true
            visible: body.showsAction
            spacing: Theme.spacingXs
            Text {
                text: qsTr("Action")
                font.pixelSize: Theme.labelSize
                font.weight: Theme.weightMedium
                color: Theme.foreground
            }
            Button {
                Layout.fillWidth: true
                text: qsTr("Choose what this direction does.")
                onClicked: root.chooseActionRequested(root.activeDirection)
            }
        }

        // --- 4 — GRANULARITY SLIDER (§4, GEST-02). Reuses the Pointer-tab slider
        // visual; the readout is the model's human string (no raw HID++ number). ---
        ColumnLayout {
            Layout.fillWidth: true
            visible: body.showsGranularity
            spacing: Theme.spacingXs
            Text {
                text: qsTr("Granularity")
                font.pixelSize: Theme.labelSize
                font.weight: Theme.weightMedium
                color: Theme.foreground
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingMd
                Slider {
                    id: granSlider
                    Layout.fillWidth: true
                    from: 0
                    to: 200
                    stepSize: 1
                    // Fire on move-END only (mirror PointerTab — avoids a D-Bus storm).
                    onPressedChanged: if (!pressed && root.gestureModel)
                        root.gestureModel.setGranularity(root.activeDirection, Math.round(value))
                    background: Rectangle {
                        x: granSlider.leftPadding
                        y: granSlider.topPadding + granSlider.availableHeight / 2 - height / 2
                        width: granSlider.availableWidth
                        height: 4
                        radius: 2
                        color: Theme.hairline
                        Rectangle {
                            width: granSlider.visualPosition * parent.width
                            height: parent.height
                            color: Theme.accent
                            radius: 2
                        }
                    }
                    handle: Rectangle {
                        x: granSlider.leftPadding + granSlider.visualPosition
                           * (granSlider.availableWidth - width)
                        y: granSlider.topPadding + granSlider.availableHeight / 2 - height / 2
                        width: 18
                        height: 18
                        radius: 9
                        color: Theme.accent
                        border.color: granSlider.visualFocus ? Theme.accent : "transparent"
                        border.width: granSlider.visualFocus ? 3 : 0
                    }
                }
                // Human readout from the model (Label 14 Regular, muted),
                // tabular-stable width via TextMetrics (Qt 6.4.2 — no font.features).
                TextMetrics {
                    id: granMetrics
                    font.pixelSize: Theme.labelSize
                    text: "~1 step per long swipe of movement"
                }
                Text {
                    Layout.preferredWidth: granMetrics.width
                    horizontalAlignment: Text.AlignRight
                    text: root.gestureModel ? root.gestureModel.granularityReadout : ""
                    font.pixelSize: Theme.labelSize
                    color: Theme.mutedForeground
                    elide: Text.ElideRight
                }
            }
            Text {
                Layout.fillWidth: true
                text: qsTr("Lower means each small movement counts as one step. "
                           + "Higher means you move farther per step.")
                font.pixelSize: Theme.labelSize
                color: Theme.mutedForeground
                wrapMode: Text.WordWrap
            }
        }
    }

    // ----------------------------------------------------------------------
    // 5 — PREVIEW CARD (§5, GEST-04). Full-width accentTint surface; the Body-16
    // foreground sentence comes straight from gestureModel.previewSentence and
    // cross-fades on change. The legibility centerpiece.
    // ----------------------------------------------------------------------
    Rectangle {
        Layout.fillWidth: true
        Layout.topMargin: Theme.spacingSm
        color: Theme.accentTint
        border.color: Theme.hairline
        border.width: 1
        radius: 8
        implicitHeight: previewRow.implicitHeight + Theme.spacingLg * 2

        RowLayout {
            id: previewRow
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.margins: Theme.spacingLg
            spacing: Theme.spacingSm

            Image {
                Layout.alignment: Qt.AlignTop
                width: 20; height: 20
                sourceSize.width: 20; sourceSize.height: 20
                source: "qrc:/logiops/gui/icons/gesture.svg"
            }
            Text {
                id: previewText
                Layout.fillWidth: true
                text: root.gestureModel ? root.gestureModel.previewSentence : ""
                font.pixelSize: Theme.bodySize
                color: Theme.foreground
                wrapMode: Text.WordWrap
                // §5: cross-fade on change (no layout jump — the card height
                // tracks the wrapped text). Pulse opacity whenever the bound
                // sentence updates.
                onTextChanged: previewFade.restart()
                SequentialAnimation {
                    id: previewFade
                    NumberAnimation { target: previewText; property: "opacity"; from: 0.4; to: 1.0; duration: Theme.motionFast; easing.type: Easing.OutCubic }
                }
                // Accessibility: AT-SPI announces the live sentence (§Accessibility).
                Accessible.role: Accessible.StaticText
                Accessible.name: text
            }
        }
    }

    // ----------------------------------------------------------------------
    // 6 — CLEAR (§6). The ONLY destructive-colored element: sets the active
    // direction to "Nothing" via the model. Not a confirmation dialog (low-stakes,
    // live-applied; the Unsaved pill + Save is the safety net).
    // ----------------------------------------------------------------------
    ToolButton {
        id: clearBtn
        Layout.alignment: Qt.AlignRight
        text: qsTr("Clear")
        ToolTip.visible: hovered
        ToolTip.text: qsTr("Set this direction to do nothing")
        contentItem: Text {
            text: clearBtn.text
            font.pixelSize: Theme.labelSize
            color: clearBtn.hovered ? Theme.destructive : Theme.mutedForeground
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            radius: 6
            color: clearBtn.hovered ? Theme.destructiveTint : "transparent"
        }
        onClicked: if (root.gestureModel) root.gestureModel.setMode(root.activeDirection, qsTr("Nothing"))
    }

    // ======================================================================
    // Inline component: one interactive direction cell (§1 cell states).
    // ======================================================================
    component DirCell: Item {
        id: cell
        property string direction: "up"
        readonly property bool isActive: root.activeDirection === direction
        readonly property bool isConfigured: root.gestureModel
                                             ? root.gestureModel.isConfigured(direction) : false

        width: Theme.gestureDirCell
        height: Theme.gestureDirCell

        activeFocusOnTab: true
        Keys.onReturnPressed: cell.choose()
        Keys.onSpacePressed: cell.choose()
        function choose() {
            if (root.gestureModel)
                root.gestureModel.activeDirection = direction;
        }

        Rectangle {
            anchors.fill: parent
            radius: 6
            color: cell.isActive ? Theme.accentTint
                                  : (cellHover.containsMouse ? Theme.hoverTint : "transparent")
            // Active = 2px accent ring; keyboard focus = 2px accent ring; else hairline.
            border.color: (cell.isActive || cell.activeFocus) ? Theme.accent : Theme.hairline
            border.width: (cell.isActive || cell.activeFocus) ? 2 : 1
            Behavior on color { ColorAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic } }
        }
        // The direction arrow (single asset rotated), accent when active.
        Image {
            anchors.centerIn: parent
            width: 20; height: 20
            sourceSize.width: 20; sourceSize.height: 20
            source: "qrc:/logiops/gui/icons/arrow.svg"
            rotation: root.dirRotation(cell.direction)
            // accent tint on the active cell (ImageColorOverlay not on 6.4.2 base —
            // approximate with opacity; the ring + label already carry the state).
            opacity: cell.isActive ? 1.0 : 0.85
        }
        // Configured dot (gestureConfigured -> accent) when this direction's
        // gesture is not "Nothing" (§1, state never color-only — paired with the
        // mode summary in the preview).
        Rectangle {
            visible: cell.isConfigured
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: 4
            width: 7; height: 7; radius: 4
            color: Theme.gestureConfigured
        }
        MouseArea {
            id: cellHover
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: cell.choose()
        }
        Accessible.role: Accessible.Button
        Accessible.name: root.dirLabel(cell.direction)
    }

    // ======================================================================
    // Inline component: one mode pill (§2). Selected = accentTint + accent text;
    // unselected = hairline border + foreground; hoverTint on hover.
    // ======================================================================
    component ModePill: Rectangle {
        id: pill
        property string label: ""
        property bool selected: false
        signal chosen()

        implicitHeight: Theme.controlRowHeight
        implicitWidth: pillText.implicitWidth + Theme.spacingMd * 2
        radius: 8
        color: selected ? Theme.accentTint
                         : (pillHover.containsMouse ? Theme.hoverTint : "transparent")
        border.color: selected ? Theme.accent : Theme.hairline
        border.width: selected ? 2 : 1
        Behavior on color { ColorAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic } }

        activeFocusOnTab: true
        Keys.onReturnPressed: pill.chosen()
        Keys.onSpacePressed: pill.chosen()

        Text {
            id: pillText
            anchors.centerIn: parent
            text: pill.label
            font.pixelSize: Theme.bodySize
            font.weight: pill.selected ? Theme.weightMedium : Theme.weightRegular
            color: pill.selected ? Theme.accent : Theme.foreground
        }
        // Keyboard focus ring (2px accent) when focused but not selected.
        Rectangle {
            visible: pill.activeFocus && !pill.selected
            anchors.fill: parent
            radius: parent.radius
            color: "transparent"
            border.color: Theme.accent
            border.width: 2
        }
        MouseArea {
            id: pillHover
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: pill.chosen()
        }
        Accessible.role: Accessible.RadioButton
        Accessible.name: pill.label
    }
}
