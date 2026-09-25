pragma Singleton
import QtQuick

// The app's palettes: Graphite (neutral near-black, amber accent) is the default. There are three dark
// and three light ones, each with its own tint and accent. `name` picks one; an unknown name falls
// back to Graphite. Every token is bound to the current palette, so switching restyles the window live.
// Use these tokens in every view; text greys are solid colours (text2/text3), never opacity.
QtObject {
    property string name: "graphite"

    // Menu order within each group
    readonly property var darkNames: ["graphite", "slate", "plum"]
    readonly property var lightNames: ["paper", "mist", "sage"]
    readonly property var palettes: ({
        "graphite": darkPalette({
            label: "Graphite",
            chrome: "#08090A", sunken: "#0A0B0C", background: "#0C0D0F", surface: "#141518",
            popup: "#1A1C1F", raised: "#1D1F23", raisedHover: "#25282D", raisedPressed: "#2B2E34",
            border: "#25282D", controlBorder: "#2E3238", controlBorderHover: "#3D424A",
            text: "#ECEEF0", text2: "#A3A8B0", text3: "#80868F", disabledText: "#5C6168",
            accent: "#F2A93B", accentHover: "#F6BA5E", accentInk: "#1A1206", accentSoft: "#1FF2A93B",
            accentLine: "#99F2A93B", accentText: "#F5BD66"
        }),
        "slate": darkPalette({
            label: "Slate",
            chrome: "#080B10", sunken: "#0A0D12", background: "#0D1117", surface: "#141A22",
            popup: "#1A2029", raised: "#1D242E", raisedHover: "#262F3B", raisedPressed: "#2D3744",
            border: "#262F3B", controlBorder: "#303A47", controlBorderHover: "#404C5C",
            text: "#E8EDF3", text2: "#A0ABB9", text3: "#8390A0", disabledText: "#58616D",
            accent: "#5B9BF5", accentHover: "#7BAFF8", accentInk: "#06111F", accentSoft: "#245B9BF5",
            accentLine: "#995B9BF5", accentText: "#86B6F9"
        }),
        "plum": darkPalette({
            label: "Plum",
            chrome: "#0A080D", sunken: "#0C0A0F", background: "#0F0C13", surface: "#17131C",
            popup: "#1D1823", raised: "#211B27", raisedHover: "#2A2332", raisedPressed: "#312939",
            border: "#2A2332", controlBorder: "#342B3D", controlBorderHover: "#45394F",
            text: "#EFEAF3", text2: "#ADA4B6", text3: "#8B8195", disabledText: "#625A6B",
            accent: "#B48CF2", accentHover: "#C4A4F5", accentInk: "#140A22", accentSoft: "#24B48CF2",
            accentLine: "#99B48CF2", accentText: "#C7A8F7"
        }),
        // The light palettes are soft tinted greys rather than white, so they aren't glaring next to the video.
        "paper": lightPalette({
            label: "Paper",
            chrome: "#EDEBE6", sunken: "#D6D3CC", background: "#E4E1DB", surface: "#EDEBE6",
            popup: "#F4F2EE", raised: "#E6E3DD", raisedHover: "#DCD9D2", raisedPressed: "#D2CEC6",
            border: "#D5D1C9", controlBorder: "#C7C2B9", controlBorderHover: "#A9A398",
            text: "#1E1C19", text2: "#4F4B45", text3: "#5E5A53", disabledText: "#9C978E",
            accent: "#D0850C", accentHover: "#DC9628", accentInk: "#1A1206", accentSoft: "#2ED0850C",
            accentLine: "#B3D0850C", accentText: "#7A4900"
        }),
        "mist": lightPalette({
            label: "Mist",
            chrome: "#E9ECEF", sunken: "#CFD4DA", background: "#DDE1E6", surface: "#E9ECEF",
            popup: "#F1F3F5", raised: "#E1E5E9", raisedHover: "#D6DBE0", raisedPressed: "#CBD1D7",
            border: "#CDD2D8", controlBorder: "#BEC4CC", controlBorderHover: "#9CA4AE",
            text: "#181B1F", text2: "#474D55", text3: "#555C65", disabledText: "#959CA5",
            accent: "#5A93E3", accentHover: "#6FA2E8", accentInk: "#06111F", accentSoft: "#2E5A93E3",
            accentLine: "#B35A93E3", accentText: "#1F54A3"
        }),
        "sage": lightPalette({
            label: "Sage",
            chrome: "#E8EBE6", sunken: "#CDD3CB", background: "#DCE1DA", surface: "#E8EBE6",
            popup: "#F0F2EE", raised: "#E0E4DD", raisedHover: "#D5DAD2", raisedPressed: "#CAD0C6",
            border: "#CCD2C9", controlBorder: "#BCC3B9", controlBorderHover: "#9AA396",
            text: "#181C17", text2: "#464D44", text3: "#545C52", disabledText: "#939B90",
            accent: "#46AD70", accentHover: "#58B981", accentInk: "#06140C", accentSoft: "#2E46AD70",
            accentLine: "#B346AD70", accentText: "#1F633A"
        })
    })

    // The status colours are shared within the dark and the light palettes.
    function darkPalette(p) {
        return Object.assign({
            danger: "#E5484D", dangerSoft: "#26E5484D", dangerLine: "#80E5484D",
            warning: "#E0A340", warningSoft: "#24D9972B", warningLine: "#80D9972B"
        }, p)
    }
    function lightPalette(p) {
        return Object.assign({
            danger: "#C8302F", dangerSoft: "#1FC8302F", dangerLine: "#80C8302F",
            warning: "#8A5700", warningSoft: "#26E0A340", warningLine: "#99C98A1F"
        }, p)
    }

    readonly property var current: palettes[name] ?? palettes["graphite"]

    // Surfaces, darkest to lightest (in the dark palettes)
    readonly property color chrome: current.chrome
    readonly property color sunken: current.sunken
    readonly property color background: current.background
    readonly property color surface: current.surface
    readonly property color popup: current.popup
    readonly property color raised: current.raised
    readonly property color raisedHover: current.raisedHover
    readonly property color raisedPressed: current.raisedPressed

    // Lines
    readonly property color border: current.border
    readonly property color controlBorder: current.controlBorder
    readonly property color controlBorderHover: current.controlBorderHover

    // Text: primary, secondary (labels), tertiary (hints, section titles); all >= 4.5:1 on surfaces
    readonly property color text: current.text
    readonly property color text2: current.text2
    readonly property color text3: current.text3
    readonly property color disabledText: current.disabledText

    // Accent
    readonly property color accent: current.accent
    readonly property color accentHover: current.accentHover
    readonly property color accentInk: current.accentInk   // text on an accent fill
    readonly property color accentSoft: current.accentSoft
    readonly property color accentLine: current.accentLine
    readonly property color accentText: current.accentText

    // Status
    readonly property color danger: current.danger
    readonly property color dangerSoft: current.dangerSoft
    readonly property color dangerLine: current.dangerLine
    readonly property color warning: current.warning
    readonly property color warningSoft: current.warningSoft
    readonly property color warningLine: current.warningLine

    readonly property int radius: 6
    readonly property string font: "Geist"
    readonly property string monoFont: "Geist Mono"
}
