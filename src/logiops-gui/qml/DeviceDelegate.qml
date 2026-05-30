// Copyright 2019-2023 PixlOne — Adapted by c4duZ
//
// One 56px device row (UI-SPEC anatomy): [icon 40px] [name + model sub-line]
// [battery PLACEHOLDER "—" until Plan 05] [status badge when offline/sleeping].
// Per-role bindings so a single field repaints without touching the row
// (no-flicker). Selection = 3px accent left bar + 12% accent tint; keyboard
// focus = 2px accent ring; hover = 6% foreground tint.
import QtQuick
import QtQuick.Layouts
import logiops.gui

Item {
    id: delegate
    height: Theme.rowHeight

    // ConnectionState enum (DeviceModel: Online=0, Sleeping=1, Offline=2, Unknown=3).
    property bool selected: false
    signal clicked()

    readonly property int connState: model.connectionState
    readonly property bool isOffline: connState === 2
    readonly property bool isSleeping: connState === 1
    // Dim per state: offline 55%, sleeping 70%, otherwise full (UI-SPEC).
    readonly property real stateOpacity: isOffline ? 0.55 : (isSleeping ? 0.70 : 1.0)

    // Accessible: announce the device + live connection state to Orca/AT-SPI.
    Accessible.role: Accessible.ListItem
    Accessible.name: model.name + ", " +
        (isOffline ? qsTr("offline") : isSleeping ? qsTr("sleeping") : qsTr("online"))
    Accessible.selectable: true
    Accessible.selected: delegate.selected

    // Selection tint background.
    Rectangle {
        anchors.fill: parent
        color: delegate.selected ? Theme.accentTint
             : hover.hovered ? Theme.hoverTint : "transparent"
        Behavior on color { ColorAnimation { duration: Theme.motionFast; easing.type: Theme.motionEasing } }
    }
    // 3px accent left bar on the selected row.
    Rectangle {
        anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
        width: 3
        color: Theme.accent
        visible: delegate.selected
    }
    // 1px hairline divider between rows (not after the last is handled by ListView clip).
    Rectangle {
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: 1
        color: Theme.hairline
    }
    // 2px accent focus ring (keyboard focus, independent of hover/selection).
    Rectangle {
        anchors.fill: parent
        anchors.margins: 1
        color: "transparent"
        border.width: 2
        border.color: Theme.accent
        visible: delegate.activeFocus || (ListView.isCurrentItem && delegate.ListView.view.activeFocus)
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.spacingMd
        anchors.rightMargin: Theme.spacingMd
        spacing: Theme.spacingMd
        opacity: delegate.stateOpacity
        Behavior on opacity { NumberAnimation { duration: Theme.motionBase; easing.type: Theme.motionEasing } }

        // Device icon (40px container, ~28px glyph) tinted per state.
        Item {
            Layout.preferredWidth: Theme.iconContainer
            Layout.preferredHeight: Theme.iconContainer
            Image {
                anchors.centerIn: parent
                width: 28; height: 28
                sourceSize.width: 28; sourceSize.height: 28
                // deviceKind: Mouse=0, Keyboard=1, Unknown=2 -> mouse glyph fallback.
                source: model.deviceKind === 1
                    ? "qrc:/logiops/gui/icons/keyboard.svg"
                    : "qrc:/logiops/gui/icons/mouse.svg"
                fillMode: Image.PreserveAspectFit
            }
        }

        // Name + model sub-line column.
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0
            Text {
                Layout.fillWidth: true
                text: model.name
                // T-02-14: render device-supplied strings as PlainText (never RichText).
                textFormat: Text.PlainText
                color: Theme.foreground
                font.pixelSize: Theme.bodySize
                font.weight: Theme.weightRegular
                elide: Text.ElideRight
            }
            Text {
                Layout.fillWidth: true
                text: model.model
                textFormat: Text.PlainText
                color: Theme.mutedForeground
                font.pixelSize: Theme.labelSize
                font.weight: Theme.weightMedium
                elide: Text.ElideRight
                visible: model.model.length > 0 && model.model !== model.name
            }
        }

        // Battery block PLACEHOLDER — Plan 05 wires real battery. Show "—" muted.
        Text {
            text: "—"
            color: Theme.mutedForeground
            font.pixelSize: Theme.labelSize
            font.weight: Theme.weightMedium
        }

        // Status badge: only when not online-awake (Offline / Sleeping). Text
        // carries the meaning (non-color signaling, UI-SPEC accessibility).
        Rectangle {
            visible: delegate.isOffline || delegate.isSleeping
            color: Theme.hoverTint
            radius: 4
            Layout.preferredHeight: 20
            Layout.preferredWidth: badge.implicitWidth + 2 * Theme.spacingSm
            Text {
                id: badge
                anchors.centerIn: parent
                text: delegate.isOffline ? qsTr("Offline") : qsTr("Sleeping")
                color: Theme.mutedForeground
                font.pixelSize: Theme.labelSize
                font.weight: Theme.weightMedium
            }
        }
    }

    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
    TapHandler { onTapped: delegate.clicked() }
}
