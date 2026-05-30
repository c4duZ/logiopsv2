// Copyright 2019-2023 PixlOne — Adapted by c4duZ
//
// The app shell. A single ApplicationWindow that switches its whole content on
// the C++ screen-state enum (daemon.screenState). The four full-window states
// (loading / empty / daemon-down / access-denied) take over the entire window —
// they describe a connection/population condition, not a per-device one (UI-SPEC
// layout contract). Only the Populated state shows the sidebar+detail SplitView.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import logiops.gui

ApplicationWindow {
    id: window
    width: Theme.windowWidth
    height: Theme.windowHeight
    minimumWidth: Theme.windowMinWidth
    minimumHeight: Theme.windowMinHeight
    visible: true
    title: qsTr("logiops")
    color: Theme.dominant

    // Index of the currently selected device row (single-select). C++ owns the
    // model; this is pure view selection state, auto-set to the first row when
    // the list populates (UI-SPEC: detail pane is never blank with >=1 device).
    property int currentIndex: -1

    // DaemonConnection.ScreenState enum values (mirrors the C++ Q_ENUM order:
    // Loading=0, Populated=1, Empty=2, DaemonDown=3, AccessDenied=4).
    readonly property int stLoading: 0
    readonly property int stPopulated: 1
    readonly property int stEmpty: 2
    readonly property int stDaemonDown: 3
    readonly property int stAccessDenied: 4

    StackLayout {
        anchors.fill: parent
        // The screen-state enum directly indexes the stack, so each state is a
        // DISTINCT view driven by C++ (CONTEXT.md: states must be separate).
        currentIndex: daemon.screenState

        LoadingState {}              // 0 Loading

        // 1 Populated — the sidebar + detail shell.
        SplitView {
            orientation: Qt.Horizontal

            DeviceList {
                id: sidebar
                SplitView.preferredWidth: Theme.sidebarWidth
                SplitView.minimumWidth: Theme.sidebarMin
                SplitView.maximumWidth: Theme.sidebarMax
                currentIndex: window.currentIndex
                onDeviceSelected: function(row) { window.currentIndex = row; }
            }

            DetailPane {
                SplitView.fillWidth: true
                currentIndex: window.currentIndex
            }
        }

        EmptyState {}                // 2 Empty
        DaemonDownState {}           // 3 DaemonDown
        AccessDeniedState {}         // 4 AccessDenied
    }

    // Auto-select the first device when the list becomes populated, and keep the
    // selection valid as rows insert/remove (selection moves to a remaining row).
    Connections {
        target: deviceModel
        function onRowsInserted() {
            if (window.currentIndex < 0 && deviceModel.rowCount() > 0)
                window.currentIndex = 0;
        }
        function onRowsRemoved() {
            if (window.currentIndex >= deviceModel.rowCount())
                window.currentIndex = deviceModel.rowCount() - 1;
        }
        function onModelReset() {
            window.currentIndex = deviceModel.rowCount() > 0 ? 0 : -1;
        }
    }
}
