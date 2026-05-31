// Copyright 2019-2023 PixlOne — Adapted by c4duZ
//
// Theme singleton: the UI-SPEC design tokens resolved as a light/dark PAIR at
// runtime. The target is Qt 6.4.2, which has NO Application.styleHints.colorScheme
// (that is Qt 6.5+, RESEARCH Pitfall 5 / A5). We detect dark mode from the system
// palette's window-color lightness and react to palette changes via SystemPalette.
pragma Singleton
import QtQuick

QtObject {
    id: theme

    // System palette drives dark detection AND native accent feel. SystemPalette
    // emits its change signals when the desktop theme flips, so `dark` re-resolves
    // and every binding below repaints (no app restart, no manual toggle).
    readonly property SystemPalette _sys: SystemPalette { colorGroup: SystemPalette.Active }

    // window lightness < 0.5 => dark. The UI-SPEC mandates exactly this approach
    // for the 6.4.2 target.
    readonly property bool dark: _sys.window.hslLightness < 0.5

    // --- Spacing scale (8-point, all multiples of 4). UI-SPEC. ---
    readonly property int spacingXs: 4
    readonly property int spacingSm: 8
    readonly property int spacingMd: 16
    readonly property int spacingLg: 24
    readonly property int spacingXl: 32
    readonly property int spacing2xl: 48
    readonly property int spacing3xl: 64

    // --- Layout tokens. ---
    readonly property int rowHeight: 56        // 7x8, deliberate >=44px hit target.
    readonly property int iconContainer: 40
    readonly property int sidebarWidth: 280
    readonly property int sidebarMin: 240
    readonly property int sidebarMax: 360
    readonly property int windowWidth: 960
    readonly property int windowHeight: 640
    readonly property int windowMinWidth: 720
    readonly property int windowMinHeight: 480
    readonly property int stateMaxWidth: 480   // centered full-pane state content.

    // --- Phase 3 layout tokens (UI-SPEC "New layout tokens"). ADD only. ---
    readonly property int tabBarHeight: 48     // top tab strip (>=44px hit target).
    readonly property int controlRowHeight: 40 // slider / toggle / combo rows.
    readonly property int actionPanelWidth: 360 // non-modal reassign side panel.
    readonly property int actionPanelMin: 320
    readonly property int deviceRenderMax: 420 // caps the SVG device-art column.
    readonly property int hotspotMin: 32       // min hotspot (>=44 effective via padding).
    readonly property int dialogMaxWidth: 440  // confirm dialogs.

    // --- Phase 4 layout token (UI-SPEC "New layout tokens"). ADD only. ---
    // Each of the 4 cardinal cells in the gesture direction cross. >=44px hit
    // target; the corners + center of the 3x3 grid are inert.
    readonly property int gestureDirCell: 44

    // --- Typography (exactly 4 sizes, exactly 2 weights). UI-SPEC. ---
    readonly property int displaySize: 28
    readonly property int headingSize: 20
    readonly property int bodySize: 16
    readonly property int labelSize: 14
    readonly property int weightRegular: 400   // Font.Normal
    readonly property int weightMedium: 600    // Font.DemiBold

    // --- Color roles, resolved per mode. UI-SPEC light/dark pairs. ---
    readonly property color dominant: dark ? "#1C1E22" : "#F5F6F8"          // canvas / detail pane
    readonly property color secondary: dark ? "#26292E" : "#FFFFFF"         // sidebar / cards
    readonly property color hairline: dark ? "#34383F" : "#E3E6EB"          // 1px dividers
    readonly property color accent: dark ? "#4C92FF" : "#2D7DF6"            // selection / focus / CTA
    readonly property color foreground: dark ? "#ECEEF1" : "#1B1D21"        // primary text
    readonly property color mutedForeground: dark ? "#9AA1AC" : "#6B7280"   // sub-line / "—" / badges
    readonly property color charging: dark ? "#46C46A" : "#2BA84A"          // charging / healthy battery
    readonly property color warning: dark ? "#F0B252" : "#E8A23D"           // low battery / unsaved
    // Phase 3 destructive semantic (UI-SPEC "New semantic token"). Destructive
    // actions ONLY — restore-defaults / remove-preset / discard. ADD only.
    readonly property color destructive: dark ? "#E06363" : "#D14343"

    // 12% accent tint for the selected-row background (text stays foreground for
    // contrast, per UI-SPEC accent contrast note).
    readonly property color accentTint: Qt.rgba(accent.r, accent.g, accent.b, 0.12)
    // ~6% foreground overlay for hover.
    readonly property color hoverTint: Qt.rgba(foreground.r, foreground.g, foreground.b, 0.06)
    // 12% destructive tint for destructive dialog accent strips / icon bg.
    readonly property color destructiveTint: Qt.rgba(destructive.r, destructive.g, destructive.b, 0.12)

    // --- Phase 4 semantic alias (UI-SPEC "New tokens"). Zero new hues. ---
    // The small accent dot on a direction cell whose gesture is not "Nothing", so
    // all 4 directions read their configured/empty state at a glance. A NAMED
    // ALIAS of `accent` for executor clarity — no new color is introduced.
    readonly property color gestureConfigured: accent

    // --- Motion budget: 120-250ms, ease-out, no overshoot. UI-SPEC. ---
    readonly property int motionFast: 150
    readonly property int motionBase: 200
    readonly property int motionAdd: 220
    readonly property int motionEasing: Easing.OutCubic  // qualified at use sites
}
