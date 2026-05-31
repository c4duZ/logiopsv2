// Copyright 2019-2023 PixlOne — Adapted by c4duZ
//
// ProfilesTab (PROF-01) — the "Profiles" tab. A list of manual profiles rendered as
// pills: the active profile = accentTint pill + accent text (UI-SPEC reserved-accent
// list); selecting any pill switches the active profile LIVE (profilesModel
// .switchProfile). A "+ New profile" secondary button reveals an inline name field
// that creates + switches in one call (createProfile -> SetProfile). Profile names
// are inline-editable (double-click a pill, Label 14) via renameProfile. The empty
// state shows the exact UI-SPEC copy.
//
// The model is deviceControllerFactory.profilesModel (a ProfilesModel over
// .Device.GetProfiles); C++ owns all marshalling. Manual profiles only this phase.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import logiops.gui

Item {
    id: root
    // The per-device controller (unused directly here, kept for symmetry with the
    // other tabs / future per-profile capability gating).
    property var controller: null
    // The per-device profiles model (injected by ConfigTabs).
    property var profilesModel: deviceControllerFactory.profilesModel

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: root.width
            spacing: Theme.spacingLg

            // --- Section header ---
            Text {
                Layout.topMargin: Theme.spacingLg
                Layout.leftMargin: Theme.spacingLg
                text: qsTr("Profiles")
                font.pixelSize: Theme.headingSize
                font.weight: Theme.weightMedium
                color: Theme.foreground
            }

            // --- Live-apply helper (subtle, optional UI-SPEC copy) ---
            Text {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.spacingLg
                Layout.rightMargin: Theme.spacingLg
                text: qsTr("Changes apply instantly. Save to keep them after a restart.")
                color: Theme.mutedForeground
                font.pixelSize: Theme.labelSize
                wrapMode: Text.WordWrap
            }

            // --- Empty state (exact UI-SPEC copy: no profiles) ---
            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.spacingLg
                Layout.rightMargin: Theme.spacingLg
                spacing: Theme.spacingSm
                visible: !root.profilesModel || root.profilesModel.rowCount() === 0

                Text {
                    text: qsTr("No profiles yet")
                    font.pixelSize: Theme.headingSize
                    font.weight: Theme.weightMedium
                    color: Theme.foreground
                }
                Text {
                    Layout.fillWidth: true
                    text: qsTr("Create a profile to save a set of settings you can switch between.")
                    color: Theme.mutedForeground
                    font.pixelSize: Theme.bodySize
                    wrapMode: Text.WordWrap
                }
            }

            // --- Profile pills ---
            Flow {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.spacingLg
                Layout.rightMargin: Theme.spacingLg
                spacing: Theme.spacingSm

                Repeater {
                    model: root.profilesModel
                    delegate: Rectangle {
                        id: pill
                        required property int index
                        required property string name
                        required property bool isActive
                        required property bool isDefault

                        implicitHeight: Theme.controlRowHeight
                        implicitWidth: Math.max(96, pillRow.implicitWidth + 2 * Theme.spacingMd)
                        radius: implicitHeight / 2
                        // Active pill = accentTint bg per the UI-SPEC reserved-accent list.
                        color: isActive ? Theme.accentTint
                                        : (pillHover.hovered ? Theme.hoverTint : "transparent")
                        border.width: isActive ? 0 : 1
                        border.color: Theme.hairline

                        Behavior on color {
                            ColorAnimation { duration: Theme.motionFast; easing.type: Theme.motionEasing }
                        }

                        HoverHandler { id: pillHover }
                        TapHandler {
                            // Single tap switches the active profile live.
                            onTapped: if (root.profilesModel && !pill.isActive)
                                root.profilesModel.switchProfile(pill.name)
                            // Double-tap enters inline rename.
                            onDoubleTapped: renameLoader.active = true
                        }

                        RowLayout {
                            id: pillRow
                            anchors.centerIn: parent
                            spacing: Theme.spacingXs

                            Text {
                                text: pill.name
                                visible: !renameLoader.active
                                font.pixelSize: Theme.labelSize
                                font.weight: Theme.weightMedium
                                // On accentTint, accent text for the active pill.
                                color: pill.isActive ? Theme.accent : Theme.foreground
                            }

                            // Inline rename editor (double-tap reveals it, Label 14).
                            Loader {
                                id: renameLoader
                                active: false
                                visible: active
                                sourceComponent: TextField {
                                    text: pill.name
                                    font.pixelSize: Theme.labelSize
                                    Component.onCompleted: { forceActiveFocus(); selectAll() }
                                    onEditingFinished: {
                                        if (root.profilesModel && text.length > 0 && text !== pill.name)
                                            root.profilesModel.renameProfile(pill.name, text)
                                        renameLoader.active = false
                                    }
                                    Keys.onEscapePressed: renameLoader.active = false
                                }
                            }

                            // Default marker (muted).
                            Text {
                                visible: pill.isDefault && !renameLoader.active
                                text: qsTr("(default)")
                                font.pixelSize: Theme.labelSize
                                color: Theme.mutedForeground
                            }

                            // Destructive remove affordance (never for the last profile).
                            ToolButton {
                                id: removePill
                                visible: !renameLoader.active && root.profilesModel
                                         && root.profilesModel.rowCount() > 1
                                text: "✕"
                                ToolTip.visible: hovered
                                ToolTip.text: qsTr("Remove profile")
                                contentItem: Text {
                                    text: removePill.text
                                    font.pixelSize: Theme.labelSize
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                    color: removePill.hovered ? Theme.destructive : Theme.mutedForeground
                                }
                                background: null
                                onClicked: if (root.profilesModel)
                                    root.profilesModel.removeProfile(pill.name)
                            }
                        }
                    }
                }
            }

            // --- "+ New profile" secondary button + inline name field ---
            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.spacingLg
                Layout.rightMargin: Theme.spacingLg
                Layout.bottomMargin: Theme.spacingLg
                spacing: Theme.spacingSm

                Button {
                    id: newProfileBtn
                    text: qsTr("+ New profile")
                    flat: true
                    visible: !newProfileRow.visible
                    onClicked: { newProfileRow.visible = true; newProfileField.forceActiveFocus() }
                }

                RowLayout {
                    id: newProfileRow
                    visible: false
                    Layout.fillWidth: true
                    spacing: Theme.spacingSm

                    function commit() {
                        if (root.profilesModel && newProfileField.text.length > 0)
                            root.profilesModel.createProfile(newProfileField.text)
                        newProfileField.text = ""
                        newProfileRow.visible = false
                    }

                    TextField {
                        id: newProfileField
                        Layout.fillWidth: true
                        placeholderText: qsTr("Profile name")
                        font.pixelSize: Theme.bodySize
                        onAccepted: newProfileRow.commit()
                        Keys.onEscapePressed: { text = ""; newProfileRow.visible = false }
                    }
                    Button {
                        text: qsTr("Create")
                        onClicked: newProfileRow.commit()
                    }
                    Button {
                        text: qsTr("Cancel")
                        flat: true
                        onClicked: { newProfileField.text = ""; newProfileRow.visible = false }
                    }
                }
            }
        }
    }
}
