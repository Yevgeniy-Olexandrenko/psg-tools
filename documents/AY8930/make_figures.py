"""Generates the SVG figures of the AY8930 datasheet (documents/AY8930.md).

Run from any directory:  python make_figures.py
The figures go to the 'figures' folder next to this script. The waveforms
come from the same rules as the psg-tools emulator (AY89XXPsg.cpp).
"""

import os
import re

OUT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "figures")

FONT = "Helvetica, Arial, sans-serif"

# The figures use color tokens. save() turns them into CSS classes of one SVG file:
# the light colors by default, the dark colors under prefers-color-scheme: dark.
# The colors follow the GitHub themes. The background is solid in both themes,
# so a figure stays readable if the page and the figure get different themes.
BACKGROUND = "@background@"
PAPER = "@paper@"
INK = "@ink@"
GREY = "@grey@"
DARK = "@dark@"
ACCENT = "@accent@"

LIGHT = {BACKGROUND: "#FFFFFF", PAPER: "#FFFFFF", INK: "#1F2328", GREY: "#E6E9ED",
         DARK: "#59636E", ACCENT: "#CF222E"}
DARK_THEME = {BACKGROUND: "#0D1117", PAPER: "#0D1117", INK: "#E6EDF3", GREY: "#30363D",
              DARK: "#9198A1", ACCENT: "#FF7B72"}


def themed(content):
    """Replaces the fill and stroke tokens of each element with classes, f-<token>
    and s-<token>, and returns the style sheet of the two themes."""
    def element(match):
        tag = match.group(0)
        classes = []

        def take(attr):
            classes.append(("f-" if attr.group(1) == "fill" else "s-") + attr.group(2))
            return ""

        tag = re.sub(r' (fill|stroke)="@(\w+)@"', take, tag)
        if classes:
            tag = tag.replace(" ", ' class="%s" ' % " ".join(classes), 1)
        return tag

    content = re.sub(r"<[a-z]+[^>]*>", element, content)

    def rules(palette, indent):
        lines = []
        for token, color in palette.items():
            name = token.strip("@")
            lines.append("%s.f-%s { fill: %s; } .s-%s { stroke: %s; }" % (indent, name, color, name, color))
        return "\n".join(lines)

    style = ("<style>\n%s\n  @media (prefers-color-scheme: dark) {\n%s\n  }\n</style>\n"
             % (rules(LIGHT, "  "), rules(DARK_THEME, "    ")))
    return style, content

# DAC levels of the psg-tools emulator for the AY8930 (YM_DAC_TABLE in AY89XXPsg.cpp)
DAC_LEVELS = [
    0.0, 0.0,
    0.00465400167849, 0.00772106507973,
    0.01095597772180, 0.01396200503550,
    0.01699855039290, 0.02001983672850,
    0.02436865796900, 0.02969405661100,
    0.03506523231860, 0.04039063096060,
    0.04853894865340, 0.05833524071110,
    0.06805523765930, 0.07777523460750,
    0.09251544975970, 0.11108567940800,
    0.12974746318800, 0.14848554207700,
    0.17666895552000, 0.21155107957600,
    0.24638742656600, 0.28110170138100,
    0.33373006790300, 0.40042725261300,
    0.46738384069600, 0.53443198291000,
    0.63517204547200, 0.75800717174000,
    0.87992675669500, 1.0,
]

# Envelope shapes of the emulator: (step, start level) of the first and the second segment
SD, SU, HOLD = -1, +1, 0
TOP, BOTTOM = 31, 0
ENVELOPES = [
    ((SD, TOP), (HOLD, BOTTOM)), ((SD, TOP), (HOLD, BOTTOM)),
    ((SD, TOP), (HOLD, BOTTOM)), ((SD, TOP), (HOLD, BOTTOM)),
    ((SU, BOTTOM), (HOLD, BOTTOM)), ((SU, BOTTOM), (HOLD, BOTTOM)),
    ((SU, BOTTOM), (HOLD, BOTTOM)), ((SU, BOTTOM), (HOLD, BOTTOM)),
    ((SD, TOP), (SD, TOP)), ((SD, TOP), (HOLD, BOTTOM)),
    ((SD, TOP), (SU, BOTTOM)), ((SD, TOP), (HOLD, TOP)),
    ((SU, BOTTOM), (SU, BOTTOM)), ((SU, BOTTOM), (HOLD, TOP)),
    ((SU, BOTTOM), (SD, TOP)), ((SU, BOTTOM), (HOLD, BOTTOM)),
]

# High steps of the 32-step tone cycle for the duty codes 0-8
DUTY_HIGH_STEPS = [1, 2, 4, 8, 16, 24, 28, 30, 31]
DUTY_PERCENT = ["3.125", "6.25", "12.5", "25", "50", "75", "87.5", "93.75", "96.875"]


def envelope_levels(shape, steps):
    """The envelope levels of 'steps' periods after a write of the shape register."""
    segments = ENVELOPES[shape]
    segment = 0
    level = segments[0][1]
    levels = [level]
    for _ in range(steps - 1):
        level += segments[segment][0]
        if level < BOTTOM or level > TOP:
            segment ^= 1
            level = segments[segment][1]
        levels.append(level)
    return levels


def esc(text):
    """Escapes the text for XML; '>=' and '<-' become the math symbols."""
    text = text.replace("&", "&amp;").replace(">=", "\x01").replace("<-", "\x02")
    text = text.replace("<", "&lt;").replace(">", "&gt;")
    return text.replace("\x01", "&#8805;").replace("\x02", "&#8592;")


class Svg:
    def __init__(self, width, height):
        self.width = width
        self.height = height
        self.items = []

    def line(self, x1, y1, x2, y2, color=INK, width=1.5, dash=None, arrow=False):
        extra = ' stroke-dasharray="%s"' % dash if dash else ""
        if arrow:
            extra += ' marker-end="url(#arrow)"'
        self.items.append('<line x1="%g" y1="%g" x2="%g" y2="%g" stroke="%s" stroke-width="%g"%s/>'
                          % (x1, y1, x2, y2, color, width, extra))

    def polyline(self, points, color=INK, width=1.5, dash=None, arrow=False, fill="none"):
        extra = ' stroke-dasharray="%s"' % dash if dash else ""
        if arrow:
            extra += ' marker-end="url(#arrow)"'
        pts = " ".join("%g,%g" % p for p in points)
        self.items.append('<polyline points="%s" fill="%s" stroke="%s" stroke-width="%g" '
                          'stroke-linejoin="round"%s/>' % (pts, fill, color, width, extra))

    def rect(self, x, y, w, h, fill=PAPER, color=INK, width=1.5, radius=0):
        self.items.append('<rect x="%g" y="%g" width="%g" height="%g" rx="%g" fill="%s" '
                          'stroke="%s" stroke-width="%g"/>' % (x, y, w, h, radius, fill, color, width))

    def circle(self, x, y, r, fill=PAPER, color=INK, width=1.5):
        self.items.append('<circle cx="%g" cy="%g" r="%g" fill="%s" stroke="%s" stroke-width="%g"/>'
                          % (x, y, r, fill, color, width))

    def text(self, x, y, text, size=13, anchor="start", weight="normal", color=INK,
             overline=False, italic=False):
        extra = ' text-decoration="overline"' if overline else ""
        if italic:
            extra += ' font-style="italic"'
        self.items.append('<text x="%g" y="%g" font-family="%s" font-size="%g" text-anchor="%s" '
                          'font-weight="%s" fill="%s"%s>%s</text>'
                          % (x, y, FONT, size, anchor, weight, color, extra, esc(text)))

    def box(self, x, y, w, h, lines, size=13, fill=PAPER, weight="bold"):
        """A block with centered lines of text."""
        self.rect(x, y, w, h, fill=fill)
        top = y + h / 2 - (len(lines) - 1) * (size + 3) / 2 + size * 0.35
        for i, line in enumerate(lines):
            self.text(x + w / 2, top + i * (size + 3), line, size=size, anchor="middle",
                      weight=weight if i == 0 else "normal")

    def xor(self, x, y, r=11):
        self.circle(x, y, r)
        self.line(x - r, y, x + r, y, width=1.2)
        self.line(x, y - r, x, y + r, width=1.2)

    def save(self, name):
        """Writes the figure with the colors of the light and the dark theme."""
        body = ('<defs><marker id="arrow" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" '
                'markerHeight="7" orient="auto-start-reverse"><path d="M0,0 L10,5 L0,10 z" fill="%s"/>'
                '</marker></defs>\n<rect width="100%%" height="100%%" fill="%s"/>\n'
                % (INK, BACKGROUND)) + "\n".join(self.items) + "\n"
        style, body = themed(body)
        text = ('<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d">\n'
                % (self.width, self.height, self.width, self.height)) + style + body + "</svg>\n"
        path = os.path.join(OUT_DIR, name)
        with open(path, "w", encoding="ascii", newline="\n") as f:
            f.write(text)
        print("wrote", path)


def title(svg, text, y=26):
    svg.text(svg.width / 2, y, text, size=15, anchor="middle", weight="bold")


# ----------------------------------------------------------------------------

def figure_pinout():
    left = [("VSS (GND)", False), ("NC", False), ("ANALOG CHANNEL B", False),
            ("ANALOG CHANNEL A", False), ("NC", False)] + \
           [("IOB%d" % i, False) for i in range(7, -1, -1)] + \
           [("IOA%d" % i, False) for i in range(7, 0, -1)]
    right = [("IOA0", False), ("CLOCK", False), ("RESET", True), ("A9", True), ("A8", False),
             ("SELECT", True), ("BDIR", False), ("BC2 (NC)", False), ("BC1", False)] + \
            [("DA%d" % i, False) for i in range(7, -1, -1)] + \
            [("ANALOG CHANNEL C", False), ("NC", False), ("VCC (+5V)", False)]
    pitch = 26
    svg = Svg(760, 620)
    title(svg, "AY8930 PIN CONFIGURATION: 40-LEAD PDIP, TOP VIEW")
    x0, x1, y0 = 290, 470, 60
    height = 20 * pitch + 20
    svg.rect(x0, y0, x1 - x0, height, fill=GREY)
    svg.items.append('<path d="M%g,%g a18,18 0 0,0 36,0" fill="%s" stroke="%s" stroke-width="1.5"/>'
                     % ((x0 + x1) / 2 - 18, y0, BACKGROUND, INK))
    svg.text((x0 + x1) / 2, y0 + height / 2, "AY8930", size=22, anchor="middle", weight="bold")
    for i in range(20):
        y = y0 + 22 + i * pitch
        name, bar = left[i]
        svg.line(x0 - 26, y, x0, y)
        svg.text(x0 + 8, y + 4, str(i + 1), size=12)
        svg.text(x0 - 32, y + 4, name, size=12, anchor="end", overline=bar)
        pin = 40 - i
        name, bar = right[pin - 21]
        svg.line(x1, y, x1 + 26, y)
        svg.text(x1 - 8, y + 4, str(pin), size=12, anchor="end")
        svg.text(x1 + 32, y + 4, name, size=12, overline=bar)
    svg.save("pinout.svg")


def figure_block_diagram():
    svg = Svg(1000, 510)
    title(svg, "AY8930 BLOCK DIAGRAM")
    # bus interface
    svg.box(110, 60, 170, 70, ["BUS CONTROL", "DECODE"])
    svg.box(110, 160, 170, 90, ["ADDRESS LATCH", "AND CHIP SELECT", "(DA7-DA4 = 0000)"])
    svg.box(110, 420, 170, 70, ["CLOCK DIVIDER", "(1 OR 2)"])
    for y, name in ((85, "BDIR"), (110, "BC1")):
        svg.line(70, y, 110, y, arrow=True)
        svg.text(66, y + 4, name, size=12, anchor="end")
    for y, name, bar in ((185, "DA7-DA0", False), (210, "A8", False), (235, "A9", True)):
        svg.line(70, y, 110, y, arrow=True)
        svg.text(66, y + 4, name, size=12, anchor="end", overline=bar)
    for y, name, bar in ((445, "CLOCK", False), (470, "SELECT", True)):
        svg.line(70, y, 110, y, arrow=True)
        svg.text(66, y + 4, name, size=12, anchor="end", overline=bar)
    svg.line(280, 95, 330, 95, arrow=True)
    svg.line(280, 205, 330, 205, arrow=True)
    # register array
    svg.rect(330, 60, 180, 250, fill=GREY)
    svg.text(420, 90, "REGISTER ARRAY", size=13, anchor="middle", weight="bold")
    svg.box(345, 105, 150, 80, ["BANK A", "16 x 8 BITS", "R0A-RFA"], size=12)
    svg.box(345, 200, 150, 80, ["BANK B", "11 x 8 BITS", "R0B-RAB"], size=12)
    svg.text(420, 300, "R13 SELECTS MODE AND BANK", size=10, anchor="middle")
    # i/o ports
    svg.box(345, 345, 150, 56, ["I/O PORTS", "A AND B"], size=12)
    svg.line(420, 310, 420, 345, arrow=True)
    svg.line(345, 362, 300, 362)
    svg.text(296, 366, "IOA7-IOA0", size=12, anchor="end")
    svg.line(345, 386, 300, 386)
    svg.text(296, 390, "IOB7-IOB0", size=12, anchor="end")
    # generators
    svg.box(570, 60, 180, 64, ["TONE GENERATORS", "A, B, C", "(PERIOD, DUTY CYCLE)"], size=12)
    svg.box(570, 150, 180, 64, ["NOISE GENERATOR", "17-BIT LFSR,", "AND / OR MASKS"], size=12)
    svg.box(570, 240, 180, 64, ["ENVELOPE", "GENERATORS A, B, C", "(32 STEPS)"], size=12)
    for y in (92, 182, 272):
        svg.line(510, y, 570, y, arrow=True)
    # clock to generators
    svg.polyline([(280, 455), (540, 455), (540, 110), (570, 110)], arrow=True)
    svg.line(540, 200, 570, 200, arrow=True)
    svg.line(540, 290, 570, 290, arrow=True)
    for y in (200, 290):
        svg.circle(540, y, 3, fill=INK)
    svg.text(410, 449, "fCLK", size=11, anchor="middle", italic=True)
    # mixers, amplitude, dac
    svg.box(800, 60, 160, 64, ["MIXERS", "A, B, C"], size=12)
    svg.box(800, 180, 160, 64, ["AMPLITUDE", "CONTROL"], size=12)
    svg.box(800, 300, 160, 64, ["D/A CONVERTERS", "A, B, C (5 BITS)"], size=12)
    svg.line(750, 92, 800, 92, arrow=True)
    svg.polyline([(750, 182), (775, 182), (775, 110), (800, 110)], arrow=True)
    svg.polyline([(750, 272), (775, 272), (775, 222), (800, 222)], arrow=True)
    svg.polyline([(480, 60), (480, 44), (980, 44), (980, 212), (960, 212)], arrow=True)
    svg.text(760, 38, "AMPLITUDE REGISTERS R8A-RAA", size=10, anchor="middle")
    svg.line(880, 124, 880, 180, arrow=True)
    svg.text(886, 156, "GATE", size=10)
    svg.line(880, 244, 880, 300, arrow=True)
    svg.text(886, 276, "LEVEL", size=10)
    for i, name in enumerate("ABC"):
        x = 840 + i * 40
        svg.line(x, 364, x, 410, arrow=True)
        svg.text(x, 426, name, size=12, anchor="middle", weight="bold")
    svg.text(880, 446, "ANALOG CHANNELS", size=11, anchor="middle")
    svg.save("block_diagram.svg")


def figure_duty_cycles():
    step = 9
    x0, y0, row = 200, 80, 44
    svg = Svg(820, y0 + 9 * row + 20)
    title(svg, "TONE OUTPUT FOR THE DUTY CYCLE CODES (EXPANDED MODE)")
    svg.text(40, y0 - 18, "R6B-R8B", size=12, weight="bold")
    svg.text(120, y0 - 18, "DUTY CYCLE", size=12, weight="bold")
    period = 32 * step
    svg.line(x0, y0 - 30, x0 + period, y0 - 30, color=DARK, width=1, arrow=True)
    svg.line(x0 + period, y0 - 30, x0, y0 - 30, color=DARK, width=1, arrow=True)
    svg.text(x0 + period / 2, y0 - 36, "ONE TONE PERIOD = 32 DUTY STEPS = 8 x TP CLOCK CYCLES",
             size=11, anchor="middle", color=DARK)
    for code, high in enumerate(DUTY_HIGH_STEPS):
        top = y0 + code * row
        low = top + 26
        label = "%s%s" % (format(code, "04b"), "" if code < 8 else " (8-15)")
        svg.text(40, low - 6, label, size=12)
        svg.text(120, low - 6, DUTY_PERCENT[code] + " %", size=12)
        points = [(x0, low)]
        for p in range(2):
            start = x0 + p * period
            points += [(start, low), (start, top), (start + high * step, top),
                       (start + high * step, low), (start + period, low)]
        svg.polyline(points)
        for p in range(3):
            svg.line(x0 + p * period, top - 4, x0 + p * period, low + 4, color=DARK, width=0.8, dash="3,3")
    svg.save("duty_cycles.svg")


def lfsr(svg, x, y, w, cells, h=36):
    svg.rect(x, y, w, h, fill=GREY)
    cw = w / len(cells)
    for i, c in enumerate(cells):
        if i:
            svg.line(x + i * cw, y, x + i * cw, y + h, width=1)
        svg.text(x + (i + 0.5) * cw, y + h / 2 + 4, c, size=12, anchor="middle")
    return cw


def figure_noise_compat():
    svg = Svg(860, 270)
    title(svg, "NOISE GENERATOR: COMPATIBILITY MODE")
    svg.text(20, 94, "fCLK / 8", size=12, italic=True)
    svg.line(80, 90, 110, 90, arrow=True)
    svg.box(110, 60, 180, 60, ["NOISE PERIOD COUNTER", "R6 (5 BITS)"], size=12)
    svg.line(290, 90, 330, 90, arrow=True)
    svg.box(330, 65, 60, 50, ["/ 2"], size=13)
    svg.line(390, 90, 450, 90, arrow=True)
    svg.text(420, 82, "SHIFT", size=10, anchor="middle")
    cells = ["B16", "...", "B4", "B3", "B2", "B1", "B0"]
    cw = lfsr(svg, 450, 72, 340, cells)
    svg.text(620, 64, "17-BIT SHIFT REGISTER (RIGHT SHIFT)", size=11, anchor="middle")
    b0 = 450 + 6.5 * cw
    b2 = 450 + 4.5 * cw
    svg.line(790, 90, 830, 90, arrow=True)
    svg.text(830, 128, "TO MIXERS", size=11, anchor="end")
    svg.polyline([(b0, 108), (b0, 190), (612, 190)], arrow=True)
    svg.polyline([(b2, 108), (b2, 170), (600, 170), (600, 178)], arrow=True)
    svg.xor(600, 190)
    svg.polyline([(589, 190), (430, 190), (430, 100), (450, 100)], arrow=True)
    svg.text(600, 222, "B16 <- B0 XOR B2", size=11, anchor="middle")
    svg.text(110, 250, "The register shifts once for every 2 x R6 counts: fSHIFT = fCLK / (16 x NP). "
             "The output is B0.", size=11)
    svg.save("noise_compat.svg")


def figure_noise_expanded():
    svg = Svg(900, 470)
    title(svg, "NOISE GENERATOR: EXPANDED MODE")
    svg.text(20, 94, "fCLK / 8", size=12, italic=True)
    svg.line(80, 90, 110, 90, arrow=True)
    svg.box(110, 60, 170, 60, ["NOISE PERIOD COUNTER", "R6A (8 BITS)"], size=12)
    svg.line(280, 90, 330, 90, arrow=True)
    svg.text(305, 82, "COUNT", size=10, anchor="middle")
    svg.box(330, 60, 170, 60, ["NOISE VALUE COUNTER", "(COUNTS UP)"], size=12)
    svg.line(500, 90, 560, 90, arrow=True)
    svg.box(560, 60, 110, 60, ["COMPARE", "VALUE >= V"], size=12)
    svg.line(670, 90, 740, 90, arrow=True)
    svg.text(705, 82, "REACHED", size=10, anchor="middle")
    svg.box(740, 65, 50, 50, ["T"], size=14)
    svg.line(790, 90, 870, 90, arrow=True)
    svg.text(870, 76, "TO MIXERS", size=11, anchor="end")
    # reset of the value counter
    svg.polyline([(705, 90), (705, 140), (415, 140), (415, 120)], arrow=True)
    svg.text(560, 155, "CLEAR VALUE", size=10, anchor="middle")
    # masks
    svg.box(540, 190, 150, 50, ["OR MASK", "RAB"], size=12)
    svg.box(540, 280, 150, 50, ["AND MASK", "R9B"], size=12)
    svg.line(615, 190, 615, 120, arrow=True)
    svg.text(622, 174, "V", size=12, italic=True)
    svg.line(615, 280, 615, 240, arrow=True)
    # lfsr
    cells = ["B16", "...", "B8", "B7", "...", "B2", "B1", "B0"]
    cw = lfsr(svg, 110, 360, 400, cells)
    svg.text(310, 352, "17-BIT SHIFT REGISTER (RIGHT SHIFT)", size=11, anchor="middle")
    svg.polyline([(470, 360), (470, 305), (540, 305)], arrow=True)
    svg.text(476, 340, "B7-B0", size=10)
    # shift enable
    svg.polyline([(720, 90), (720, 378), (510, 378)], arrow=True)
    svg.text(726, 300, "SHIFT", size=10)
    # feedback
    b0 = 110 + 7.5 * cw
    b2 = 110 + 5.5 * cw
    svg.polyline([(b0, 396), (b0, 448), (262, 448)], arrow=True)
    svg.polyline([(b2, 396), (b2, 426), (250, 426), (250, 436)], arrow=True)
    svg.xor(250, 448)
    svg.polyline([(239, 448), (90, 448), (90, 370), (110, 370)], arrow=True)
    svg.text(130, 464, "B16 <- B0 XOR B2", size=11)
    svg.save("noise_expanded.svg")


def figure_mixer():
    svg = Svg(900, 300)
    title(svg, "MIXER OF ONE CHANNEL (R7: 1 = DISABLED)")

    def gate(x, y, label, w=46, h=56):
        svg.rect(x, y, w, h)
        svg.text(x + w / 2, y + 20, label, size=14, anchor="middle", weight="bold")

    def bubble_in(x, y):
        svg.circle(x - 5, y, 5)

    for panel, (name, first, last) in enumerate((("AY-3-8910 / YM2149", ">=1", "&"),
                                                 ("AY8930 (BOTH MODES)", "&", ">=1"))):
        ox = 30 + panel * 450
        svg.text(ox + 200, 64, name, size=13, anchor="middle", weight="bold")
        gate(ox + 150, 90, first)
        gate(ox + 150, 180, first)
        gate(ox + 270, 132, last)
        inputs = (("TONE OUTPUT", 102), ("R7 TONE BIT", 134), ("NOISE OUTPUT", 192), ("R7 NOISE BIT", 224))
        for label, y in inputs:
            svg.text(ox + 100, y + 4, label, size=11, anchor="end")
            if panel and "R7" in label:
                svg.line(ox + 105, y, ox + 140, y)
                bubble_in(ox + 150, y)
            else:
                svg.line(ox + 105, y, ox + 150, y)
        svg.polyline([(ox + 196, 118), (ox + 235, 118), (ox + 235, 146), (ox + 270, 146)])
        svg.polyline([(ox + 196, 208), (ox + 235, 208), (ox + 235, 176), (ox + 270, 176)])
        svg.line(ox + 316, 160, ox + 360, 160, arrow=True)
        svg.text(ox + 364, 164, "GATE", size=11)
    svg.text(255, 270, "Tone and noise disabled: gate = 1,", size=12, anchor="middle")
    svg.text(255, 286, "the channel outputs its amplitude.", size=12, anchor="middle")
    svg.text(705, 270, "Tone and noise disabled: gate = 0,", size=12, anchor="middle", color=ACCENT)
    svg.text(705, 286, "the channel is silent, the envelope too.", size=12, anchor="middle", color=ACCENT)
    svg.save("mixer.svg")


def figure_envelope_shapes():
    rows = [("0", "0", "x", "x", 0), ("0", "1", "x", "x", 4), ("1", "0", "0", "0", 8),
            ("1", "0", "0", "1", 9), ("1", "0", "1", "0", 10), ("1", "0", "1", "1", 11),
            ("1", "1", "0", "0", 12), ("1", "1", "0", "1", 13), ("1", "1", "1", "0", 14),
            ("1", "1", "1", "1", 15)]
    step = 4
    x0, y0, row, amp = 230, 110, 54, 36
    svg = Svg(x0 + 96 * step + 40, y0 + len(rows) * row + 40)
    title(svg, "ENVELOPE SHAPE / CYCLE CONTROL (R13, R4B, R5B: BITS B3-B0)")
    heads = ["CONT.", "ATT.", "ALT.", "HOLD"]
    for i, h in enumerate(heads):
        svg.text(40 + i * 42, y0 - 30, h, size=11, anchor="middle", weight="bold")
        svg.text(40 + i * 42, y0 - 48, "B%d" % (3 - i), size=11, anchor="middle", color=DARK)
    svg.text(x0 + 48 * step, y0 - 30, "ENVELOPE LEVEL, 0-31 (THREE CYCLES)", size=11, anchor="middle",
             weight="bold")
    for r, (c, a, al, h, shape) in enumerate(rows):
        base = y0 + r * row + amp
        for i, bit in enumerate((c, a, al, h)):
            svg.text(40 + i * 42, base - 10, bit, size=13, anchor="middle")
        levels = envelope_levels(shape, 96)
        points = []
        for i, level in enumerate(levels):
            y = base - level * amp / 31.0
            points += [(x0 + i * step, y), (x0 + (i + 1) * step, y)]
        svg.polyline(points, width=1.3)
        svg.line(x0, base + 1, x0 + 96 * step, base + 1, color=DARK, width=0.6)
        if r < len(rows) - 1:
            svg.line(20, base + 9, x0 + 96 * step + 20, base + 9, color=GREY, width=1)
    bottom = y0 + len(rows) * row
    for i in range(4):
        svg.line(x0 + i * 32 * step, y0 - 8, x0 + i * 32 * step, bottom - 10, color=DARK, width=0.8, dash="3,3")
    dimension(svg, x0, x0 + 32 * step, bottom + 2, "ONE CYCLE = 32 STEPS = 256 x EP CLOCK CYCLES")
    svg.save("envelope_shapes.svg")


def dimension(svg, x1, x2, y, label):
    """A double arrow from x1 to x2 with the label under it."""
    svg.line(x1, y - 6, x1, y + 6, color=DARK, width=0.8)
    svg.line(x2, y - 6, x2, y + 6, color=DARK, width=0.8)
    svg.line(x1, y, x2, y, color=DARK, width=1, arrow=True)
    svg.line(x2, y, x1, y, color=DARK, width=1, arrow=True)
    svg.text(x1, y + 18, label, size=11, color=DARK)


def figure_envelope_detail():
    step = 8
    x0, amp = 130, 124
    svg = Svg(x0 + 64 * step + 40, 420)
    title(svg, "DETAIL OF TWO CYCLES, SHAPE 1110 (CONT. ATT. ALT.)")
    levels = envelope_levels(14, 64)
    for panel, (name, mapper) in enumerate((("EXPANDED MODE: 32 LEVELS", lambda v: v),
                                            ("COMPATIBILITY MODE: 16 LEVELS (L0 = 1)", lambda v: v | 1))):
        base = 70 + amp + panel * (amp + 60)
        svg.text(x0, base - amp - 12, name, size=12, weight="bold")
        points = []
        for i, v in enumerate(levels):
            y = base - mapper(v) * amp / 31.0
            points += [(x0 + i * step, y), (x0 + (i + 1) * step, y)]
        svg.polyline(points, width=1.4)
        svg.line(x0, base, x0 + 64 * step, base, color=DARK, width=0.8)
        for v in (0, 15, 31):
            y = base - mapper(v) * amp / 31.0
            svg.text(x0 - 8, y + 4, str(mapper(v)), size=11, anchor="end")
        svg.line(x0 + 32 * step, base - amp - 4, x0 + 32 * step, base + 4, color=DARK, width=0.8, dash="3,3")
    dimension(svg, x0, x0 + 32 * step, 384, "ONE CYCLE: 32 STEPS OF 8 x EP CLOCK CYCLES")
    svg.save("envelope_detail.svg")


def figure_dac():
    bar = 18
    x0, y0, h = 80, 60, 320
    svg = Svg(x0 + 32 * bar + 60, y0 + h + 80)
    title(svg, "D/A CONVERTER OUTPUT, 32 LEVELS")
    base = y0 + h
    for v in (0.0, 0.25, 0.5, 0.75, 1.0):
        y = base - v * h
        svg.line(x0 - 4, y, x0 + 32 * bar, y, color=GREY, width=1)
        svg.text(x0 - 8, y + 4, "%.2f" % v, size=11, anchor="end")
    for i, v in enumerate(DAC_LEVELS):
        svg.rect(x0 + i * bar + 2, base - v * h, bar - 4, v * h, fill=DARK, color=DARK, width=0.5)
        if i % 4 == 0 or i == 31:
            svg.text(x0 + i * bar + bar / 2, base + 16, str(i), size=10, anchor="middle")
    nominal = [(x0 + i * bar + bar / 2, base - (10 ** (-(31 - i) * 1.5 / 20) if i else 0) * h)
               for i in range(32)]
    svg.polyline(nominal, color=ACCENT, width=1.4, dash="5,3")
    svg.line(x0, base, x0 + 32 * bar, base)
    svg.text(x0 + 16 * bar, base + 36, "LEVEL (L4-L0 OR ENVELOPE E4-E0)", size=11, anchor="middle")
    svg.items.append('<text x="0" y="0" transform="translate(30,%g) rotate(-90)" font-family="%s" '
                     'font-size="11" text-anchor="middle" fill="%s">OUTPUT VOLTAGE, NORMALIZED</text>'
                     % (y0 + h / 2, FONT, INK))
    svg.rect(x0 + 10, y0 + 6, 14, 10, fill=DARK, color=DARK)
    svg.text(x0 + 30, y0 + 15, "psg-tools emulation (measured YM2149 levels)", size=11)
    svg.line(x0 + 10, y0 + 30, x0 + 24, y0 + 30, color=ACCENT, width=1.4, dash="5,3")
    svg.text(x0 + 30, y0 + 34, "DS50010C nominal curve, 1.5 dB per level", size=11)
    svg.save("dac_output.svg")


def bus_cycle(svg, oy, name, action, data_label, marks):
    svg.text(30, oy, name, size=13, weight="bold")
    yc, yb = oy + 40, oy + 110
    svg.text(160, yc + 4, "BC1, BDIR", size=11, anchor="end")
    svg.text(160, yb + 4, "DA7-DA0", size=11, anchor="end")
    hi, lo = yc - 14, yc + 14
    svg.polyline([(180, lo), (240, lo), (240, hi), (390, hi), (390, lo), (520, lo), (520, hi),
                  (670, hi), (670, lo), (760, lo)])
    svg.text(315, hi - 6, "LATCH ADDRESS", size=11, anchor="middle")
    svg.text(455, lo + 16, "INACTIVE", size=11, anchor="middle")
    svg.text(595, hi - 6, action, size=11, anchor="middle")
    for x1, x2, label in ((290, 420, "ADDRESS"), (570, 700, data_label)):
        svg.polyline([(x1, yb), (x1 + 8, yb - 12), (x2 - 8, yb - 12), (x2, yb), (x2 - 8, yb + 12),
                      (x1 + 8, yb + 12), (x1, yb)], fill=GREY)
        svg.text((x1 + x2) / 2, yb + 4, label, size=11, anchor="middle")
    svg.line(180, yb, 290, yb, color=DARK, width=1, dash="4,3")
    svg.line(420, yb, 570, yb, color=DARK, width=1, dash="4,3")
    svg.line(700, yb, 760, yb, color=DARK, width=1, dash="4,3")
    for x1, x2, label in marks:
        y = yb + 30
        svg.line(x1, y - 8, x1, y + 6, color=DARK, width=0.8)
        svg.line(x2, y - 8, x2, y + 6, color=DARK, width=0.8)
        svg.line(x1, y, x2, y, color=DARK, width=1, arrow=True)
        svg.line(x2, y, x1, y, color=DARK, width=1, arrow=True)
        svg.text((x1 + x2) / 2, y + 18, label, size=11, anchor="middle", italic=True)


def figure_bus_timing():
    svg = Svg(800, 420)
    title(svg, "BUS TIMING")
    bus_cycle(svg, 60, "WRITE CYCLE", "WRITE TO PSG", "DATA INPUT",
              ((290, 390, "tAS"), (390, 420, "tAH"), (570, 670, "tDS"), (670, 700, "tDH")))
    bus_cycle(svg, 240, "READ CYCLE", "READ FROM PSG", "DATA OUTPUT",
              ((290, 390, "tAS"), (390, 420, "tAH"), (520, 570, "tAB"), (670, 700, "tTD")))
    svg.save("bus_timing.svg")


def figure_bank_switch():
    svg = Svg(940, 300)
    title(svg, "ONE UPDATE THAT CHANGES BOTH BANKS (EXPANDED MODE)")
    writes = [("R0-RC", "BANK A", 120), ("R13", "1011 SSSS", 100), ("R6B-R8B", "BANK B", 120),
              ("R0B-R3B", "BANK B", 120), ("R13", "1010 SSSS", 100)]
    x = 60
    restarts = []
    for i, (name, value, w) in enumerate(writes):
        restart = (name == "R13")
        svg.rect(x, 60, w, 56, fill=GREY if restart else PAPER)
        svg.text(x + w / 2, 84, name, size=13, anchor="middle", weight="bold")
        svg.text(x + w / 2, 104, value, size=11, anchor="middle")
        if restart:
            restarts.append(x + w / 2)
        x += w + 34
        if i < len(writes) - 1:
            svg.line(x - 34, 88, x, 88, arrow=True)
    svg.text(60, 140, "BUS WRITES IN TIME ORDER (S = SHAPE OF ENVELOPE A)", size=11, color=DARK)
    # envelope A level: the writes take microseconds, so the level holds,
    # and each write of R13 sets it back to the start of the shape
    base, amp = 260, 60
    level = base - amp * 0.6
    points = [(40, level), (restarts[0], level), (restarts[0], base), (900, base)]
    svg.polyline(points, width=1.6)
    for cx in restarts:
        svg.items.append('<path d="M%g,%g l-7,-12 l14,0 z" fill="%s"/>' % (cx, base - amp - 4, ACCENT))
        svg.text(cx, base - amp - 22, "ENVELOPE A RESTARTS", size=10, anchor="middle", color=ACCENT)
    svg.line(40, base, 900, base, color=DARK, width=0.6, dash="3,3")
    svg.text(40, base + 22, "ENVELOPE A LEVEL, SHAPE 1101 (ATTACK, HOLD): EACH R13 WRITE SETS IT BACK TO 0",
             size=11, color=DARK)
    svg.save("bank_switch.svg")


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    figure_pinout()
    figure_block_diagram()
    figure_duty_cycles()
    figure_noise_compat()
    figure_noise_expanded()
    figure_mixer()
    figure_envelope_shapes()
    figure_envelope_detail()
    figure_dac()
    figure_bus_timing()
    figure_bank_switch()


if __name__ == "__main__":
    main()
