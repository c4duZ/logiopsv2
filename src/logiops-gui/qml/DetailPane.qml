// Copyright 2019-2023 PixlOne — Adapted by c4duZ
//
// The read-only detail pane (UI-SPEC: this phase shows status only — NO config
// controls, those are Fase 3). Large device icon, Display-size name, model
// sub-line, and a Connection section reflecting the live connectionState. The
// Battery section is a stub label pending Plan 05. Content cross-fades on select.
import QtQuick
import QtQuick.Layouts
import logiops.gui

Rectangle {
    id: pane
    color: Theme.dominant

    property int currentIndex: -1

    // Pull the selected row's roles via a single-item DelegateModel-free lookup.
    readonly property bool hasSelection: currentIndex >= 0 && currentIndex < deviceModel.rowCount()

    // "Select a device" quiet prompt when nothing is selected (not a full error).
    Text {
        anchors.centerIn: parent
        visible: !pane.hasSelection
        text: qsTr("Select a device")
        color: Theme.mutedForeground
        font.pixelSize: Theme.bodySize
    }

    // A hidden Repeater bound to the model lets us read the selected row's roles
    // declaratively (C++ owns the data; this is pure rendering).
    Repeater {
        id: rows
        model: deviceModel
        delegate: Item {
            readonly property bool current: index === pane.currentIndex
            readonly property string rName: model.name
            readonly property string rModel: model.model
            readonly property int rKind: model.deviceKind
            readonly property int rConn: model.connectionState
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

    ColumnLayout {
        id: content
        visible: pane.hasSelection
        anchors.fill: parent
        anchors.margins: Theme.spacingLg
        spacing: Theme.spacingMd

        // Header block: large icon + name + model sub-line.
        Image {
            Layout.preferredWidth: 64
            Layout.preferredHeight: 64
            sourceSize.width: 64; sourceSize.height: 64
            source: {
                var it = pane.selectedItem();
                return (it && it.rKind === 1)
                    ? "qrc:/logiops/gui/icons/keyboard.svg"
                    : "qrc:/logiops/gui/icons/mouse.svg";
            }
            fillMode: Image.PreserveAspectFit
        }
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

        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.hairline; Layout.topMargin: Theme.spacingMd }

        // Connection section.
        Text {
            text: qsTr("Connection")
            color: Theme.foreground
            font.pixelSize: Theme.headingSize
            font.weight: Theme.weightMedium
        }
        Text {
            text: {
                var it = pane.selectedItem();
                if (!it) return "";
                switch (it.rConn) {
                case 1: return qsTr("Sleeping");
                case 2: return qsTr("Offline");
                case 3: return qsTr("Unknown");
                default: return qsTr("Connected");
                }
            }
            color: Theme.mutedForeground
            font.pixelSize: Theme.bodySize
        }

        // Battery section — STUB pending Plan 05 (battery display deferred).
        Text {
            text: qsTr("Battery")
            color: Theme.foreground
            font.pixelSize: Theme.headingSize
            font.weight: Theme.weightMedium
            Layout.topMargin: Theme.spacingMd
        }
        Text {
            text: "—"
            color: Theme.mutedForeground
            font.pixelSize: Theme.bodySize
        }

        Item { Layout.fillHeight: true }

        // Cross-fade content on selection change (150ms, UI-SPEC).
        opacity: pane.hasSelection ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: Theme.motionFast; easing.type: Theme.motionEasing } }
    }
}
