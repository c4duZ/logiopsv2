// Copyright 2019-2023 PixlOne — Adapted by c4duZ
//
// ReassignPanel (BTN-02, BTN-03, HOST-01) — the non-modal categorized action
// picker. Header ("Reassign button" + the physical button name), then the
// CONTEXT-ordered categories: Keystroke / DPI (Change|Cycle) / Host switch /
// Profile switch / SmartShift toggle / Hi-res toggle / None (Disabled). The
// current action is marked with an accent dot. Apply is LIVE (no Save button) —
// each choice calls the matching buttonsModel two-step setter immediately. Host
// slots are driven by the device (buttonsModel.hostCount), NOT hardcoded.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import logiops.gui

Rectangle {
    id: root

    property var controller: null
    property var buttonsModel: null
    property int row: -1

    signal closed()

    color: Theme.secondary
    border.color: Theme.hairline
    border.width: 1

    // The current row's data (read live from the model so the accent dot tracks
    // reassigns). Guarded for row == -1 (panel closing).
    function roleData(roleOffset) {
        if (!buttonsModel || row < 0 || row >= buttonsModel.rowCount())
            return undefined;
        return buttonsModel.data(buttonsModel.index(row, 0), Qt.UserRole + roleOffset);
    }
    readonly property string buttonName: {
        var n = roleData(7); // ButtonNameRole = UserRole + 7
        return n === undefined ? "" : n;
    }
    readonly property string currentType: {
        var t = roleData(5); // CurrentActionTypeRole = UserRole + 5
        return t === undefined ? "None" : t;
    }
    readonly property int hostCount: buttonsModel ? buttonsModel.hostCount : 3

    // GEST-01: this button supports gestures (capability-gate the category —
    // hidden, not greyed, when false). GestureSupportRole = UserRole + 4.
    readonly property bool gestureSupport: {
        var g = roleData(4);
        return g === undefined ? false : g;
    }
    // The .../buttons/M object path the gesture model scopes to. ButtonPathRole
    // = UserRole + 8.
    readonly property string buttonPath: {
        var p = roleData(8);
        return p === undefined ? "" : p;
    }
    // The button-scoped GestureModel (Plan 02), owned by the controller and
    // re-pointed per button. Built lazily when the Gesture category opens.
    property var gestureModel: null

    // Local mode for the inline editors (which category is expanded).
    property string expanded: ""

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingLg
        spacing: Theme.spacingMd

        // --- Header ---
        RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text {
                    text: qsTr("Reassign button")
                    font.pixelSize: Theme.labelSize
                    color: Theme.mutedForeground
                }
                Text {
                    text: root.buttonName
                    font.pixelSize: Theme.headingSize
                    font.weight: Theme.weightMedium
                    color: Theme.foreground
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }
            // Close (deselect) — secondary, not accent.
            Button {
                text: qsTr("Close")
                flat: true
                onClicked: root.closed()
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.hairline }

        // --- Categorized action list (CONTEXT order). ---
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            ColumnLayout {
                width: parent.width
                spacing: Theme.spacingSm

                // 1) Keystroke (BTN-02).
                CategoryRow {
                    label: qsTr("Keystroke")
                    glyph: "qrc:/logiops/gui/icons/keystroke.svg"
                    active: root.currentType === "Keypress"
                    onChosen: root.expanded = (root.expanded === "Keypress" ? "" : "Keypress")
                }
                KeyCaptureField {
                    Layout.fillWidth: true
                    visible: root.expanded === "Keypress"
                    onKeysCaptured: function (names) {
                        if (root.buttonsModel && names.length > 0)
                            root.buttonsModel.setKeypress(root.row, names);
                    }
                }

                // 2) DPI — Change / Cycle (BTN-03).
                CategoryRow {
                    label: qsTr("DPI — Change")
                    glyph: "qrc:/logiops/gui/icons/dpi.svg"
                    active: root.currentType === "ChangeDPI"
                    onChosen: root.expanded = (root.expanded === "ChangeDPI" ? "" : "ChangeDPI")
                }
                RowLayout {
                    Layout.fillWidth: true
                    visible: root.expanded === "ChangeDPI"
                    SpinBox {
                        id: dpiChange
                        from: -1000; to: 1000; stepSize: 50; value: 0
                    }
                    Button {
                        text: qsTr("Apply")
                        onClicked: if (root.buttonsModel) root.buttonsModel.setChangeDpi(root.row, dpiChange.value)
                    }
                }
                CategoryRow {
                    label: qsTr("DPI — Cycle")
                    glyph: "qrc:/logiops/gui/icons/dpi.svg"
                    active: root.currentType === "CycleDPI"
                    onChosen: if (root.buttonsModel) root.buttonsModel.setCycleDpi(root.row, [])
                }

                // 3) Host switch (HOST-01) — numbered slots 1..N + next/prev.
                CategoryRow {
                    label: qsTr("Host switch")
                    glyph: "qrc:/logiops/gui/icons/host.svg"
                    active: root.currentType === "ChangeHost"
                    onChosen: root.expanded = (root.expanded === "ChangeHost" ? "" : "ChangeHost")
                }
                Flow {
                    Layout.fillWidth: true
                    visible: root.expanded === "ChangeHost"
                    spacing: Theme.spacingSm

                    Repeater {
                        // Slot count is device-driven via hostCount (HOST-01).
                        model: root.hostCount
                        delegate: Button {
                            required property int index
                            text: qsTr("Host %1").arg(index + 1)
                            onClicked: if (root.buttonsModel)
                                root.buttonsModel.setChangeHost(root.row, "" + (index + 1))
                        }
                    }
                    Button {
                        text: qsTr("Next")
                        onClicked: if (root.buttonsModel) root.buttonsModel.setChangeHost(root.row, "next")
                    }
                    Button {
                        text: qsTr("Previous")
                        onClicked: if (root.buttonsModel) root.buttonsModel.setChangeHost(root.row, "prev")
                    }
                }

                // 4) Profile switch (BTN-03).
                CategoryRow {
                    label: qsTr("Profile switch")
                    glyph: "qrc:/logiops/gui/icons/profile.svg"
                    active: root.currentType === "ChangeProfile"
                    onChosen: root.expanded = (root.expanded === "ChangeProfile" ? "" : "ChangeProfile")
                }
                RowLayout {
                    Layout.fillWidth: true
                    visible: root.expanded === "ChangeProfile"
                    TextField {
                        id: profileName
                        Layout.fillWidth: true
                        placeholderText: qsTr("Profile name")
                    }
                    Button {
                        text: qsTr("Apply")
                        enabled: profileName.text.length > 0
                        onClicked: if (root.buttonsModel)
                            root.buttonsModel.setChangeProfile(root.row, profileName.text)
                    }
                }

                // 5) SmartShift toggle (BTN-03, param-less).
                CategoryRow {
                    label: qsTr("SmartShift toggle")
                    glyph: "qrc:/logiops/gui/icons/smartshift.svg"
                    active: root.currentType === "ToggleSmartShift"
                    onChosen: if (root.buttonsModel) root.buttonsModel.setToggleSmartShift(root.row)
                }

                // 6) Hi-res toggle (BTN-03, param-less).
                CategoryRow {
                    label: qsTr("Hi-res toggle")
                    glyph: "qrc:/logiops/gui/icons/hires.svg"
                    active: root.currentType === "ToggleHiresScroll"
                    onChosen: if (root.buttonsModel) root.buttonsModel.setToggleHiresScroll(root.row)
                }

                // 7) Gesture (GEST-01..04) — capability-gated; placed after
                // "Hi-res toggle", before "Disabled" (UI-SPEC §Entry point).
                // Hidden (not greyed) when the button has no gesture support.
                CategoryRow {
                    label: qsTr("Gesture")
                    glyph: "qrc:/logiops/gui/icons/gesture.svg"
                    visible: root.gestureSupport
                    active: root.currentType === "Gesture"
                    onChosen: {
                        if (root.expanded === "Gesture") {
                            root.expanded = "";
                            return;
                        }
                        // Pitfall 3 sequence: SetAction("Gesture") FIRST so the daemon
                        // builds the GestureAction (and its gestures map) before any
                        // SetGesture runs; THEN build the button-scoped model + open.
                        if (root.buttonsModel)
                            root.buttonsModel.setAction(root.row, "Gesture");
                        if (root.controller && root.buttonPath.length > 0)
                            root.gestureModel = root.controller.gestureModelForButton(root.buttonPath);
                        root.expanded = "Gesture";
                    }
                }
                GestureBuilder {
                    Layout.fillWidth: true
                    visible: root.expanded === "Gesture"
                    gestureModel: root.gestureModel
                    // §Motion: 220ms height/opacity reveal (OutCubic).
                    opacity: visible ? 1.0 : 0.0
                    Behavior on opacity {
                        NumberAnimation { duration: Theme.motionAdd; easing.type: Easing.OutCubic }
                    }
                    // Reuse the EXACT button-reassignment action chooser for the
                    // gesture's per-direction action (no parallel picker): expand the
                    // Keystroke category scoped to "the action this direction fires".
                    // The chosen keystroke is forwarded to the gesture model.
                    onChooseActionRequested: function (direction) {
                        root.expanded = "Keypress";
                    }
                }

                // 8) None (Disabled) — clears the binding.
                CategoryRow {
                    label: qsTr("Disabled")
                    glyph: "qrc:/logiops/gui/icons/disabled.svg"
                    active: root.currentType === "None"
                    onChosen: if (root.buttonsModel) root.buttonsModel.clearAction(root.row)
                }
            }
        }
    }

    // Inline component: one category row with glyph, label, and an accent dot when
    // it is the current action. Kept local so the panel stays one file.
    component CategoryRow: Item {
        property string label: ""
        property string glyph: ""
        property bool active: false
        signal chosen()

        Layout.fillWidth: true
        implicitHeight: Theme.controlRowHeight

        Rectangle {
            anchors.fill: parent
            color: hover.containsMouse ? Theme.hoverTint : "transparent"
            radius: 6
        }
        Row {
            anchors.left: parent.left
            anchors.leftMargin: Theme.spacingSm
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.spacingSm
            Image {
                anchors.verticalCenter: parent.verticalCenter
                width: 20; height: 20
                sourceSize.width: 20; sourceSize.height: 20
                source: glyph
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: label
                font.pixelSize: Theme.bodySize
                color: Theme.foreground
            }
        }
        // Accent dot marks the current action (UI-SPEC).
        Rectangle {
            anchors.right: parent.right
            anchors.rightMargin: Theme.spacingSm
            anchors.verticalCenter: parent.verticalCenter
            width: 8; height: 8; radius: 4
            color: Theme.accent
            visible: active
        }
        MouseArea {
            id: hover
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: chosen()
        }
    }
}
