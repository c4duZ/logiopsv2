// Copyright 2024 c4duZ — https://github.com/c4duZ/logiopsv2
//
// GesturePicker (260531-gmt) — the predefined Options+-style action chooser for a
// gesture direction. Renders the `gestureActions` C++ provider's categorized list
// (Media / Brightness / Desktops & Windows / Edit & Navigation / System) as a
// scrollable, WIDTH-CONSTRAINED column of rows grouped by category, plus a final
// "Custom keystroke…" advanced item that falls back to the raw key capture.
//
// QML renders only: the action labels + key lists come from the gestureActions
// context property; this component emits the user's choice and the OWNER
// (ReassignPanel via GestureBuilder) dispatches it through the existing C++
// gestureModel.setGestureKeypress. Nothing here clips in the 360px panel — every
// row is fillWidth with wrapping/eliding text and any ScrollView pins
// contentWidth to availableWidth.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import logiops.gui

ColumnLayout {
    id: root
    spacing: Theme.spacingSm

    // A predefined action was chosen: keys is the ordered evdev KEY_* combo (a
    // QStringList from the provider), label is the human name (for the readout).
    signal actionPicked(var keys, string label)
    // The advanced "Custom keystroke…" item was chosen: the owner opens the raw
    // KeyCaptureField path (unchanged power-user capture).
    signal customRequested()

    // The categorized provider data (read once from the context property). Guarded
    // for the (test/offscreen) case where the property is absent.
    readonly property var categoryData: (typeof gestureActions !== "undefined" && gestureActions)
                                        ? gestureActions.categories() : []

    Text {
        Layout.fillWidth: true
        text: qsTr("Pick an action for this direction.")
        font.pixelSize: Theme.labelSize
        color: Theme.mutedForeground
        wrapMode: Text.WordWrap
    }

    // The categorized list, height-capped and scrollable so the full set never
    // pushes the panel taller than the viewport. contentWidth: availableWidth
    // keeps the rows inside the panel (no horizontal clip).
    ScrollView {
        id: pickScroll
        Layout.fillWidth: true
        Layout.preferredHeight: Math.min(pickColumn.implicitHeight, 280)
        clip: true
        contentWidth: availableWidth

        ColumnLayout {
            id: pickColumn
            width: pickScroll.availableWidth
            spacing: Theme.spacingSm

            Repeater {
                model: root.categoryData
                delegate: ColumnLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: Theme.spacingXs

                    // Category header (muted Label).
                    Text {
                        Layout.fillWidth: true
                        Layout.topMargin: Theme.spacingXs
                        text: modelData.name
                        font.pixelSize: Theme.labelSize
                        font.weight: Theme.weightMedium
                        color: Theme.mutedForeground
                        wrapMode: Text.WordWrap
                    }
                    // One selectable row per predefined action.
                    Repeater {
                        model: modelData.actions
                        delegate: ActionRow {
                            required property var modelData
                            Layout.fillWidth: true
                            label: modelData.label
                            onChosen: root.actionPicked(modelData.keys, modelData.label)
                        }
                    }
                }
            }
        }
    }

    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.hairline }

    // Advanced fallback: open the raw KeyCaptureField (the existing power-user
    // path) to capture an arbitrary combo not in the curated set.
    ActionRow {
        Layout.fillWidth: true
        label: qsTr("Custom keystroke…")
        muted: true
        onChosen: root.customRequested()
    }

    // ======================================================================
    // Inline component: one selectable action row. fillWidth + eliding label so
    // a long action name never overflows the panel.
    // ======================================================================
    component ActionRow: Item {
        id: actRow
        property string label: ""
        property bool muted: false
        signal chosen()

        implicitHeight: Theme.controlRowHeight

        activeFocusOnTab: true
        Keys.onReturnPressed: actRow.chosen()
        Keys.onSpacePressed: actRow.chosen()

        Rectangle {
            anchors.fill: parent
            radius: 6
            color: rowHover.containsMouse ? Theme.hoverTint : "transparent"
            border.color: actRow.activeFocus ? Theme.accent : "transparent"
            border.width: actRow.activeFocus ? 2 : 0
        }
        Text {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: Theme.spacingSm
            anchors.rightMargin: Theme.spacingSm
            text: actRow.label
            font.pixelSize: Theme.bodySize
            color: actRow.muted ? Theme.mutedForeground : Theme.foreground
            elide: Text.ElideRight
        }
        MouseArea {
            id: rowHover
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: actRow.chosen()
        }
        Accessible.role: Accessible.Button
        Accessible.name: actRow.label
    }
}
