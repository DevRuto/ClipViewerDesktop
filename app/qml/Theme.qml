pragma Singleton
import QtQuick

// The Graphite palette: neutral near-black surfaces, one amber accent, 6 px corners, Geist.
// Use these tokens in every view; text greys are solid colours (text2/text3), never opacity.
QtObject {
    // Surfaces, darkest to lightest
    readonly property color chrome: "#08090A"
    readonly property color sunken: "#0A0B0C"
    readonly property color background: "#0C0D0F"
    readonly property color surface: "#141518"
    readonly property color popup: "#1A1C1F"
    readonly property color raised: "#1D1F23"
    readonly property color raisedHover: "#25282D"
    readonly property color raisedPressed: "#2B2E34"

    // Lines
    readonly property color border: "#25282D"
    readonly property color controlBorder: "#2E3238"
    readonly property color controlBorderHover: "#3D424A"

    // Text: primary, secondary (labels), tertiary (hints, section titles); all >= 4.5:1 on surfaces
    readonly property color text: "#ECEEF0"
    readonly property color text2: "#A3A8B0"
    readonly property color text3: "#80868F"
    readonly property color disabledText: "#5C6168"

    // Accent
    readonly property color accent: "#F2A93B"
    readonly property color accentHover: "#F6BA5E"
    readonly property color accentInk: "#1A1206"   // text on an accent fill
    readonly property color accentSoft: "#1FF2A93B"
    readonly property color accentLine: "#99F2A93B"
    readonly property color accentText: "#F5BD66"

    // Status
    readonly property color danger: "#E5484D"
    readonly property color dangerSoft: "#26E5484D"
    readonly property color dangerLine: "#80E5484D"
    readonly property color warning: "#E0A340"
    readonly property color warningSoft: "#24D9972B"
    readonly property color warningLine: "#80D9972B"

    readonly property int radius: 6
    readonly property string font: "Geist"
    readonly property string monoFont: "Geist Mono"
}
