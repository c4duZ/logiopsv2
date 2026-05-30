// Copyright 2019-2023 PixlOne — Adapted by c4duZ
//
// KeyCaptureField (BTN-02) — the live key-capture widget plus a manual fallback.
// "Press keys now" puts an Item into focus and accumulates modifiers + a final
// key; an accent focus ring + tabular-stable readout shows the captured combo.
// Esc cancels (KeyNameMapper maps Escape -> empty, the cancel sentinel). On a
// real key it calls keyNames.comboToEvdev(modifiers, key) -> evdev KEY_* names and
// emits keysCaptured(names), which the panel forwards to buttonsModel.setKeypress.
// Beneath, a manual modifier-checkbox + key-combo fallback covers anything the
// live capture can't grab (compositor-reserved combos, etc.).
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import logiops.gui

ColumnLayout {
    id: root
    spacing: Theme.spacingSm

    // Emits the ordered evdev KEY_* names (modifiers..., key).
    signal keysCaptured(var names)

    property bool capturing: false
    property var captured: []          // human labels for the readout
    property var capturedModifiers: [] // Qt modifier key codes
    property int capturedKey: 0        // Qt key code of the final (non-modifier) key

    function modifierName(qtKey) {
        switch (qtKey) {
        case Qt.Key_Control: return "Ctrl";
        case Qt.Key_Shift:   return "Shift";
        case Qt.Key_Alt:     return "Alt";
        case Qt.Key_Meta:    return "Meta";
        default:             return "";
        }
    }
    function isModifier(qtKey) {
        return qtKey === Qt.Key_Control || qtKey === Qt.Key_Shift
            || qtKey === Qt.Key_Alt || qtKey === Qt.Key_Meta;
    }

    // --- Live capture surface. ---
    Item {
        id: captureSurface
        Layout.fillWidth: true
        implicitHeight: Theme.controlRowHeight
        focus: root.capturing
        activeFocusOnTab: true

        Rectangle {
            anchors.fill: parent
            radius: 6
            color: Theme.dominant
            border.width: root.capturing ? 2 : 1
            border.color: root.capturing ? Theme.accent : Theme.hairline
        }

        Text {
            anchors.centerIn: parent
            // Tabular-stable readout: prompt while idle, live combo while capturing.
            text: root.capturing
                  ? (root.captured.length > 0
                     ? root.captured.join(" + ")
                     : qsTr("Press the keys you want to assign"))
                  : qsTr("Press keys now")
            color: root.capturing ? Theme.foreground : Theme.mutedForeground
            font.pixelSize: Theme.bodySize
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                root.capturing = true;
                root.captured = [];
                root.capturedModifiers = [];
                root.capturedKey = 0;
                captureSurface.forceActiveFocus();
            }
        }

        Keys.onPressed: function (event) {
            if (!root.capturing)
                return;
            event.accepted = true;

            if (event.key === Qt.Key_Escape) {
                // Esc cancels the capture (KeyNameMapper maps it to empty).
                root.capturing = false;
                root.captured = [];
                root.capturedModifiers = [];
                root.capturedKey = 0;
                return;
            }

            if (root.isModifier(event.key)) {
                if (root.capturedModifiers.indexOf(event.key) === -1) {
                    var m = root.capturedModifiers.slice();
                    m.push(event.key);
                    root.capturedModifiers = m;
                }
            } else {
                root.capturedKey = event.key;
            }

            // Rebuild the human readout (modifiers then key).
            var labels = [];
            for (var i = 0; i < root.capturedModifiers.length; ++i)
                labels.push(root.modifierName(root.capturedModifiers[i]));
            if (root.capturedKey !== 0) {
                var keyName = keyNames.toEvdevName(root.capturedKey);
                labels.push(keyName.length > 0 ? keyName.replace("KEY_", "") : "?");
            }
            root.captured = labels;
        }

        Keys.onReleased: function (event) {
            if (!root.capturing || root.capturedKey === 0)
                return;
            event.accepted = true;
            // A non-modifier key was captured -> finalize via the C++ mapper.
            var names = keyNames.comboToEvdev(root.capturedModifiers, root.capturedKey);
            if (names.length > 0)
                root.keysCaptured(names);
            root.capturing = false;
        }
    }

    Text {
        text: qsTr("Press Esc to cancel")
        font.pixelSize: Theme.labelSize
        color: Theme.mutedForeground
        visible: root.capturing
    }

    // --- Manual fallback: modifier checkboxes + a single key field. ---
    Text {
        text: qsTr("Or set it manually")
        font.pixelSize: Theme.labelSize
        color: Theme.mutedForeground
    }
    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.spacingSm
        CheckBox { id: mCtrl;  text: qsTr("Ctrl") }
        CheckBox { id: mShift; text: qsTr("Shift") }
        CheckBox { id: mAlt;   text: qsTr("Alt") }
        CheckBox { id: mMeta;  text: qsTr("Meta") }
    }
    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.spacingSm
        TextField {
            id: manualKey
            Layout.fillWidth: true
            placeholderText: qsTr("Key (e.g. T)")
            maximumLength: 1
        }
        Button {
            text: qsTr("Assign")
            enabled: manualKey.text.length === 1
            onClicked: {
                var mods = [];
                if (mCtrl.checked)  mods.push(Qt.Key_Control);
                if (mShift.checked) mods.push(Qt.Key_Shift);
                if (mAlt.checked)   mods.push(Qt.Key_Alt);
                if (mMeta.checked)  mods.push(Qt.Key_Meta);
                // Map the typed character to a Qt key code (A..Z / 0..9).
                var ch = manualKey.text.toUpperCase().charCodeAt(0);
                var names = keyNames.comboToEvdev(mods, ch);
                if (names.length > 0)
                    root.keysCaptured(names);
            }
        }
    }
}
