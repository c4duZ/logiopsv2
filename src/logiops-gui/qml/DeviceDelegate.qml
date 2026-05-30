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

    // Live battery roles (per-role bindings — a battery tick repaints ONLY the
    // battery block, never moves/re-sorts the row; no-flicker contract UI-SPEC).
    readonly property bool batteryKnown: model.batteryKnown
    readonly property bool charging: model.charging
    // T-02-15: clamp a spoofed/buggy out-of-range percentage to 0..100.
    readonly property int batteryPercent: Math.max(0, Math.min(100, model.batteryPercent))
    // Threshold color: >20% Success green, <=20% Warning amber (UI-SPEC). The
    // charging bolt itself is always green regardless of level (handled below).
    readonly property color batteryColor: batteryPercent > 20 ? Theme.charging : Theme.warning

    // Accessible: announce the device + live connection AND battery state to
    // Orca/AT-SPI; updates on every battery/connection change.
    Accessible.role: Accessible.ListItem
    Accessible.name: model.name + ", " +
        (batteryKnown
            ? (batteryPercent + qsTr(" percent") + (charging ? qsTr(", charging") : ""))
            : qsTr("battery unknown")) + ", " +
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
                text: model.modelName
                textFormat: Text.PlainText
                color: Theme.mutedForeground
                font.pixelSize: Theme.labelSize
                font.weight: Theme.weightMedium
                elide: Text.ElideRight
                visible: model.modelName.length > 0 && model.modelName !== model.name
            }
        }

        // Battery block (UI-SPEC anatomy 4): right-aligned battery glyph +
        // numeric % (tabular, stable width) + charging bolt; threshold colors;
        // "—" muted when unknown. Per-role bindings only -> a tick repaints just
        // this block (no row move/re-sort, no-flicker contract).
        RowLayout {
            spacing: Theme.spacingXs

            // --- Known battery: glyph + fill + % ---
            // Battery glyph drawn as primitives so its outline/fill tints to the
            // threshold color without a runtime ColorOverlay dependency.
            Item {
                Layout.preferredWidth: 22
                Layout.preferredHeight: 12
                visible: delegate.batteryKnown
                // Outline body.
                Rectangle {
                    id: battBody
                    anchors { left: parent.left; verticalCenter: parent.verticalCenter }
                    width: 18; height: 11; radius: 2
                    color: "transparent"
                    border.width: 1.4
                    border.color: delegate.batteryColor
                    Behavior on border.color { ColorAnimation { duration: Theme.motionBase; easing.type: Theme.motionEasing } }
                    // Fill proportional to % (clamped), inset 1.5px.
                    Rectangle {
                        anchors { left: parent.left; verticalCenter: parent.verticalCenter; leftMargin: 1.5 }
                        height: parent.height - 3
                        width: (parent.width - 3) * (delegate.batteryPercent / 100)
                        radius: 1
                        color: delegate.batteryColor
                        Behavior on color { ColorAnimation { duration: Theme.motionBase; easing.type: Theme.motionEasing } }
                        Behavior on width { NumberAnimation { duration: Theme.motionBase; easing.type: Theme.motionEasing } }
                    }
                }
                // Positive terminal nub.
                Rectangle {
                    anchors { left: battBody.right; verticalCenter: parent.verticalCenter }
                    width: 2; height: 5; radius: 1
                    color: delegate.batteryColor
                    Behavior on color { ColorAnimation { duration: Theme.motionBase; easing.type: Theme.motionEasing } }
                }
                // Charging bolt overlay — always green (UI-SPEC), level-independent.
                Image {
                    anchors.centerIn: parent
                    width: 12; height: 12
                    sourceSize.width: 12; sourceSize.height: 12
                    source: "qrc:/logiops/gui/icons/charging.svg"
                    fillMode: Image.PreserveAspectFit
                    visible: delegate.charging
                }
            }
            Text {
                id: pctText
                visible: delegate.batteryKnown
                text: delegate.batteryPercent + "%"
                color: delegate.batteryColor
                font.pixelSize: Theme.labelSize
                font.weight: Theme.weightMedium
                // Stable width (no horizontal jitter as the number ticks) without
                // relying on font.features — that is Qt 6.7+ and the target is
                // 6.4.2 (UI-SPEC permits the fallback). Reserve the widest case
                // ("100%") and right-align so 1-/2-/3-digit values don't shift.
                // TextMetrics is not an Item, so it has no `parent` — reference the
                // enclosing Text by id (parent.font was undefined → QFont warning).
                Layout.preferredWidth: metrics.advanceWidth
                horizontalAlignment: Text.AlignRight
                TextMetrics { id: metrics; font: pctText.font; text: "100%" }
                Behavior on color { ColorAnimation { duration: Theme.motionBase; easing.type: Theme.motionEasing } }
            }

            // --- Unknown battery: "—" muted, no fill, no bolt, no warning color ---
            Text {
                visible: !delegate.batteryKnown
                text: "—"
                color: Theme.mutedForeground
                font.pixelSize: Theme.labelSize
                font.weight: Theme.weightMedium
            }
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
