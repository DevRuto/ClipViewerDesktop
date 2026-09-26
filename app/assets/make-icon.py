# Builds app-icon.png and app-icon.ico from the shapes in app-icon.svg (keep the two in step).
# Drawn with Pillow, supersampled, so no SVG renderer is needed. Run: python app/assets/make-icon.py
# The .ico's 16-32 px sizes use a heavier cut of the same mark.
import os
from PIL import Image, ImageDraw

def hexc(h): return tuple(int(h[i:i+2], 16) for i in (1, 3, 5)) + (255,)

TOP, BOTTOM, EDGE = hexc("#2A2332"), hexc("#0F0C13"), hexc("#45394F")
ACCENT, INK = hexc("#B48CF2"), hexc("#F4F1F7")
REGULAR = dict(bw=18, brackets=[[(86,66),(62,66),(62,190),(86,190)], [(170,66),(194,66),(194,190),(170,190)]],
               play=[(108,90),(166,128),(108,166)])
# Heavier cut for 16-32 px, where the regular strokes go soft
SMALL = dict(bw=28, brackets=[[(82,62),(56,62),(56,194),(82,194)], [(174,62),(200,62),(200,194),(174,194)]],
             play=[(104,80),(176,128),(104,176)])

def render(size, v, ss=16):
    S = size * ss
    k = S / 256
    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    p = lambda x: x * k
    # tile: edge stroke (3 wide, centred on the rect edge), then the gradient fill inside it
    d.rounded_rectangle([p(6.5), p(6.5), p(249.5), p(249.5)], radius=p(57.5), fill=EDGE)
    grad = Image.new("RGBA", (S, S))
    gd = ImageDraw.Draw(grad)
    for y in range(S):
        t = min(max((y / k - 8) / 240, 0), 1)
        gd.line([(0, y), (S, y)], fill=tuple(round(a + (b - a) * t) for a, b in zip(TOP, BOTTOM)))
    mask = Image.new("L", (S, S), 0)
    ImageDraw.Draw(mask).rounded_rectangle([p(9.5), p(9.5), p(246.5), p(246.5)], radius=p(54.5), fill=255)
    img.paste(grad, (0, 0), mask)

    def stroke(points, width, color, closed=False):
        pts = [(p(x), p(y)) for x, y in points] + ([(p(points[0][0]), p(points[0][1]))] if closed else [])
        d.line(pts, fill=color, width=round(p(width)))
        r = p(width) / 2
        for x, y in pts:  # round caps and joins
            d.ellipse([x - r, y - r, x + r, y + r], fill=color)

    for b in v["brackets"]:
        stroke(b, v["bw"], ACCENT)
    d.polygon([(p(x), p(y)) for x, y in v["play"]], fill=INK)
    stroke(v["play"], 10, INK, closed=True)
    return img.resize((size, size), Image.LANCZOS)

out = os.path.dirname(os.path.abspath(__file__))
render(256, REGULAR).save(os.path.join(out, "app-icon.png"))
sizes = [16, 20, 24, 32, 40, 48, 64, 128, 256]
frames = [render(s, SMALL if s <= 32 else REGULAR) for s in sizes]
frames[-1].save(os.path.join(out, "app-icon.ico"), format="ICO", sizes=[(s, s) for s in sizes], append_images=frames[:-1])
