pragma Singleton
import QtQuick

// The app's palettes and styles. The palette (`name`) sets the colours: Graphite (neutral near-black,
// amber accent) is the default, and each dark palette has a light counterpart. The style sets the
// shape and type of the controls. Any style goes with any palette; an unknown name falls back to
// Graphite. Every token is bound, so switching restyles the window live.
// Use these tokens in every view; text greys are solid colours (text2/text3), never opacity.
QtObject {
    property string name: "graphite"
    property string style: "graphite"
    // Smaller controls and bars, with any style
    property bool compact: false
    // The UI font family (the `font` setting); empty, or not installed, uses the style's font
    property string fontChoice: ""

    // Menu order within each group; the same position in the other list is the counterpart, and a
    // pair shares its label. Paper, Mist and Sage keep their original keys so saved settings still load.
    readonly property var darkNames: ["graphite", "slate", "plum", "sage-dark", "lavender", "azure", "steel"]
    readonly property var lightNames: ["paper", "mist", "plum-light", "sage", "lavender-light", "azure-light", "steel-light"]
    readonly property bool light: lightNames.includes(name)

    // The palette on the other side (dark <-> light) of `palette`.
    function counterpart(palette, toLight) {
        const from = toLight ? darkNames : lightNames
        const to = toLight ? lightNames : darkNames
        const index = from.indexOf(palette)
        return index >= 0 ? to[index] : palette
    }

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
        "sage-dark": darkPalette({
            label: "Sage",
            chrome: "#080A08", sunken: "#0A0C0A", background: "#0C0F0C", surface: "#131713",
            popup: "#191E19", raised: "#1C221C", raisedHover: "#252C25", raisedPressed: "#2B332B",
            border: "#252C25", controlBorder: "#2E372E", controlBorderHover: "#3D493D",
            text: "#EAF0EA", text2: "#A3ADA3", text3: "#828D82", disabledText: "#5A635A",
            accent: "#5CC48A", accentHover: "#78D09F", accentInk: "#06140C", accentSoft: "#245CC48A",
            accentLine: "#995CC48A", accentText: "#80D4A5"
        }),
        "lavender": darkPalette({
            label: "Lavender",
            chrome: "#0F0D13", sunken: "#0F0D13", background: "#141218", surface: "#1D1B20",
            popup: "#2B2930", raised: "#36343B", raisedHover: "#403D46", raisedPressed: "#48454E",
            videoBackground: "#0F0D13",
            border: "#49454F", controlBorder: "#938F99", controlBorderHover: "#CAC4D0",
            text: "#E6E0E9", text2: "#CAC4D0", text3: "#938F99", disabledText: "#605D64",
            accent: "#D0BCFF", accentHover: "#DCCBFF", accentInk: "#381E72", accentSoft: "#4A4458",
            accentLine: "#664A4458", accentText: "#E8DEF8"
        }),
        "azure": darkPalette({
            label: "Azure",
            chrome: "#1C1C1C", sunken: "#1F1F1F", background: "#202020", surface: "#282828",
            fieldFill: "#2B2B2B", fieldBottom: "#9A9A9A", videoBackground: "#1A1A1A",
            popup: "#2C2C2C", raised: "#2D2D2D", raisedHover: "#323232", raisedPressed: "#272727",
            border: "#333333", controlBorder: "#383838", controlBorderHover: "#454545",
            text: "#FFFFFF", text2: "#C5C5C5", text3: "#9E9E9E", disabledText: "#5D5D5D",
            accent: "#60CDFF", accentHover: "#5DB9E6", accentInk: "#000000", accentSoft: "#2660CDFF",
            accentLine: "#9960CDFF", accentText: "#99EBFF"
        }),
        "steel": darkPalette({
            label: "Steel",
            chrome: "#2E2E32", sunken: "#1D1D20", background: "#222226", surface: "#2E2E32",
            popup: "#36363A", raised: "#3A3A3E", raisedHover: "#434347", raisedPressed: "#4C4C50",
            border: "#151517", controlBorder: "#3F3F43", controlBorderHover: "#55555A",
            text: "#FFFFFF", text2: "#C0C0C3", text3: "#9A9A9E", disabledText: "#5E5E62",
            accent: "#3584E4", accentHover: "#4A91E7", accentInk: "#FFFFFF", accentSoft: "#333584E4",
            accentLine: "#993584E4", accentText: "#81B6F4"
        }),
        // The light palettes are soft tinted greys rather than white, so they aren't glaring next to the video.
        "paper": lightPalette({
            label: "Graphite",
            chrome: "#EDEBE6", sunken: "#D6D3CC", background: "#E4E1DB", surface: "#EDEBE6",
            popup: "#F4F2EE", raised: "#E6E3DD", raisedHover: "#DCD9D2", raisedPressed: "#D2CEC6",
            border: "#D5D1C9", controlBorder: "#C7C2B9", controlBorderHover: "#A9A398",
            text: "#1E1C19", text2: "#4F4B45", text3: "#5E5A53", disabledText: "#9C978E",
            accent: "#D0850C", accentHover: "#DC9628", accentInk: "#1A1206", accentSoft: "#2ED0850C",
            accentLine: "#B3D0850C", accentText: "#7A4900"
        }),
        "mist": lightPalette({
            label: "Slate",
            chrome: "#E9ECEF", sunken: "#CFD4DA", background: "#DDE1E6", surface: "#E9ECEF",
            popup: "#F1F3F5", raised: "#E1E5E9", raisedHover: "#D6DBE0", raisedPressed: "#CBD1D7",
            border: "#CDD2D8", controlBorder: "#BEC4CC", controlBorderHover: "#9CA4AE",
            text: "#181B1F", text2: "#474D55", text3: "#555C65", disabledText: "#959CA5",
            accent: "#5A93E3", accentHover: "#6FA2E8", accentInk: "#06111F", accentSoft: "#2E5A93E3",
            accentLine: "#B35A93E3", accentText: "#1F54A3"
        }),
        "plum-light": lightPalette({
            label: "Plum",
            chrome: "#ECE8EF", sunken: "#D3CDD9", background: "#E2DDE6", surface: "#ECE8EF",
            popup: "#F3F1F5", raised: "#E4E0E8", raisedHover: "#DAD5DF", raisedPressed: "#CFC9D5",
            border: "#D2CCD8", controlBorder: "#C4BDCB", controlBorderHover: "#A59CAE",
            text: "#1D1920", text2: "#4C4552", text3: "#5B5461", disabledText: "#9A93A1",
            accent: "#9A6ADB", accentHover: "#A77CE0", accentInk: "#140A22", accentSoft: "#2E9A6ADB",
            accentLine: "#B39A6ADB", accentText: "#5B2E9E"
        }),
        "sage": lightPalette({
            label: "Sage",
            chrome: "#E8EBE6", sunken: "#CDD3CB", background: "#DCE1DA", surface: "#E8EBE6",
            popup: "#F0F2EE", raised: "#E0E4DD", raisedHover: "#D5DAD2", raisedPressed: "#CAD0C6",
            border: "#CCD2C9", controlBorder: "#BCC3B9", controlBorderHover: "#9AA396",
            text: "#181C17", text2: "#464D44", text3: "#545C52", disabledText: "#939B90",
            accent: "#46AD70", accentHover: "#58B981", accentInk: "#06140C", accentSoft: "#2E46AD70",
            accentLine: "#B346AD70", accentText: "#1F633A"
        }),
        "lavender-light": lightPalette({
            label: "Lavender",
            chrome: "#F3EDF7", sunken: "#ECE6F0", background: "#FEF7FF", surface: "#F7F2FA",
            popup: "#F3EDF7", raised: "#ECE6F0", raisedHover: "#E3DDE7", raisedPressed: "#DAD4DE",
            videoBackground: "#E6E0E9",
            border: "#CAC4D0", controlBorder: "#79747E", controlBorderHover: "#1D1B20",
            text: "#1D1B20", text2: "#49454F", text3: "#5F5B66", disabledText: "#A8A3AE",
            accent: "#6750A4", accentHover: "#7965AF", accentInk: "#FFFFFF", accentSoft: "#E8DEF8",
            accentLine: "#666750A4", accentText: "#4F378B"
        }),
        "azure-light": lightPalette({
            label: "Azure",
            chrome: "#EEEEEE", sunken: "#E6E6E6", background: "#F3F3F3", surface: "#F3F3F3",
            popup: "#F9F9F9", raised: "#FBFBFB", raisedHover: "#F6F6F6", raisedPressed: "#F0F0F0",
            fieldFill: "#FBFBFB", fieldBottom: "#868686", videoBackground: "#E4E4E4",
            border: "#E0E0E0", controlBorder: "#DADADA", controlBorderHover: "#C2C2C2",
            text: "#1A1A1A", text2: "#454545", text3: "#616161", disabledText: "#A0A0A0",
            accent: "#005FB8", accentHover: "#1A6FC2", accentInk: "#FFFFFF", accentSoft: "#1F005FB8",
            accentLine: "#99005FB8", accentText: "#003E92"
        }),
        "steel-light": lightPalette({
            label: "Steel",
            chrome: "#EBEBED", sunken: "#E6E6E8", background: "#FAFAFB", surface: "#FFFFFF",
            popup: "#FFFFFF", raised: "#E6E6E7", raisedHover: "#DEDEE0", raisedPressed: "#D3D3D5",
            border: "#DCDCDE", controlBorder: "#D0D0D3", controlBorderHover: "#B0B0B5",
            text: "#333338", text2: "#5E5E64", text3: "#6A6A70", disabledText: "#A8A8AD",
            accent: "#3584E4", accentHover: "#4A91E7", accentInk: "#FFFFFF", accentSoft: "#263584E4",
            accentLine: "#993584E4", accentText: "#0461BE"
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

    // Other design languages' shapes. `outlined` draws buttons and fields as outlines on the surface
    // below them. Fonts are tried in order; one that isn't installed falls through to Geist.
    readonly property var styleNames: ["graphite", "material", "fluent", "adwaita"]
    readonly property var styles: ({
        "graphite": {
            label: "Graphite", hint: "The app's own style",
            fonts: ["Geist"], monoFonts: ["Geist Mono"], fontSize: 13, buttonWeight: Font.Medium,
            radius: 6, popupRadius: 6, controlHeight: 32, compactHeight: 26, pill: false,
            buttonBorder: true, bottomStroke: false, outlined: false
        },
        "material": {
            label: "Material", hint: "Material Design 3: outlined pill buttons, rounder popups",
            fonts: ["Roboto", "Roboto Flex", "Segoe UI"], monoFonts: ["Roboto Mono", "Geist Mono"],
            fontSize: 14, buttonWeight: Font.Medium,
            radius: 4, popupRadius: 16, controlHeight: 40, compactHeight: 32, pill: true,
            buttonBorder: true, bottomStroke: false, outlined: true
        },
        "fluent": {
            label: "Fluent", hint: "Windows 11: small corners, Segoe UI, a darker bottom edge on controls",
            fonts: ["Segoe UI Variable Text", "Segoe UI Variable", "Segoe UI"],
            monoFonts: ["Cascadia Mono", "Consolas", "Geist Mono"], fontSize: 13, buttonWeight: Font.Normal,
            radius: 4, popupRadius: 8, controlHeight: 32, compactHeight: 26, pill: false,
            buttonBorder: true, bottomStroke: true, outlined: false
        },
        "adwaita": {
            label: "Adwaita", hint: "GNOME / libadwaita: filled borderless buttons, bold labels, round popovers",
            fonts: ["Adwaita Sans", "Cantarell", "Inter", "Ubuntu", "Noto Sans"],
            monoFonts: ["Adwaita Mono", "Source Code Pro", "Geist Mono"], fontSize: 13, buttonWeight: Font.Bold,
            radius: 6, popupRadius: 12, controlHeight: 34, compactHeight: 28, pill: false,
            buttonBorder: false, bottomStroke: false, outlined: false
        }
    })

    readonly property var currentStyle: styles[style] ?? styles["graphite"]
    readonly property var current: palettes[name] ?? palettes["graphite"]

    // Surfaces, darkest to lightest (in the dark palettes)
    readonly property color chrome: current.chrome
    readonly property color sunken: current.sunken
    // Optional per palette (the fields' fill and bottom edge, the video area); an outlined style
    // leaves buttons and fields unfilled
    // Fully transparent, but in the hover colour: fading from "transparent" (clear black) would
    // pass through a dark grey on the way.
    readonly property color clear: Qt.rgba(raisedHover.r, raisedHover.g, raisedHover.b, 0)
    readonly property color buttonFill: currentStyle.outlined ? clear : current.raised
    readonly property color fieldFill: currentStyle.outlined ? "transparent" : (current.fieldFill ?? current.sunken)
    readonly property color fieldBottom: current.fieldBottom ?? current.controlBorderHover
    readonly property color videoBackground: current.videoBackground ?? current.sunken
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

    // Shape and type, from the style
    readonly property int radius: currentStyle.radius
    readonly property int popupRadius: currentStyle.popupRadius
    readonly property int controlHeight: compact ? currentStyle.compactHeight : currentStyle.controlHeight
    readonly property bool pill: currentStyle.pill              // buttons and segmented tracks
    readonly property bool buttonBorder: currentStyle.buttonBorder
    readonly property bool bottomStroke: currentStyle.bottomStroke
    readonly property int fontSize: compact ? Math.min(currentStyle.fontSize, 13) : currentStyle.fontSize

    // Bars and panels
    readonly property int barHeight: compact ? 38 : 48            // the top bar
    readonly property int statusHeight: compact ? 28 : 34
    readonly property int panelPadding: compact ? 8 : 12          // the controls under the video
    readonly property int panelSpacing: compact ? 6 : 10
    readonly property int buttonWeight: currentStyle.buttonWeight

    readonly property var installedFonts: Qt.fontFamilies()
    function firstInstalled(families, fallback) {
        return families.find(family => installedFonts.includes(family)) ?? fallback
    }
    readonly property string styleFont: firstInstalled(currentStyle.fonts, "Geist")
    readonly property string font: fontChoice && installedFonts.includes(fontChoice) ? fontChoice : styleFont
    readonly property string monoFont: firstInstalled(currentStyle.monoFonts, "Geist Mono")

    function cornerFor(height) {
        return pill ? height / 2 : radius
    }
}
