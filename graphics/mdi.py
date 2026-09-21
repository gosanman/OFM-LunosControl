# -*- coding: utf-8 -*-
"""Rastert ein MDI-SVG (Material Design Icons, ein <path> im viewBox 0 0 24 24)
auf ein 32x32-PNG im Stil des OpenKNX-Icon-Satzes: reines Schwarz auf
Transparenz, antialiasiert.

Warum selbst rastern: auf diesem Rechner ist kein SVG-Rasterizer installiert
(kein cairosvg, kein Inkscape, kein ImageMagick). MDI-Pfade sind aber einfach -
M/L/H/V/C/S/Q/T/A/Z ohne Transformationen, Fuellregel nonzero. Die Kurven werden
in Polygone zerlegt, jeder Teilpfad einzeln gefuellt und die Teilpfade per XOR
verrechnet (even-odd). Bei MDI liefert das dasselbe Ergebnis wie nonzero, weil
Loecher gegenlaeufig gewickelt sind - Ergebnis trotzdem ansehen.

Aufruf:  python graphics/mdi.py <svg> [<svg> ...]   -> src/Baggages/Icons/<name>.png
"""
import io
import math
import os
import re
import sys

from PIL import Image, ImageChops, ImageDraw

SS = 16                      # Ueberabtastung: 24 * 16 = 384 px Kantenlaenge
PAD = 0.0                    # MDI zeichnet den Rand selbst mit ein
SEGS = 24                    # Stuetzpunkte je Kurvensegment

_TOKEN = re.compile(r'[MmLlHhVvCcSsQqTtAaZz]|[-+]?(?:\d*\.\d+|\d+)(?:[eE][-+]?\d+)?')


def _tokens(d):
    return _TOKEN.findall(d)


def _arc(x0, y0, rx, ry, phi, large, sweep, x1, y1):
    """Endpunkt- zu Mittelpunktform nach SVG-Spezifikation, dann als Polylinie."""
    if rx == 0 or ry == 0 or (x0 == x1 and y0 == y1):
        return [(x1, y1)]
    phi = math.radians(phi)
    cp, sp = math.cos(phi), math.sin(phi)
    dx2, dy2 = (x0 - x1) / 2.0, (y0 - y1) / 2.0
    x1p = cp * dx2 + sp * dy2
    y1p = -sp * dx2 + cp * dy2
    rx, ry = abs(rx), abs(ry)
    lam = (x1p * x1p) / (rx * rx) + (y1p * y1p) / (ry * ry)
    if lam > 1:
        f = math.sqrt(lam)
        rx, ry = rx * f, ry * f
    num = rx * rx * ry * ry - rx * rx * y1p * y1p - ry * ry * x1p * x1p
    den = rx * rx * y1p * y1p + ry * ry * x1p * x1p
    co = math.sqrt(max(0.0, num / den)) if den else 0.0
    if large == sweep:
        co = -co
    cxp = co * rx * y1p / ry
    cyp = -co * ry * x1p / rx
    cx = cp * cxp - sp * cyp + (x0 + x1) / 2.0
    cy = sp * cxp + cp * cyp + (y0 + y1) / 2.0

    def ang(ux, uy, vx, vy):
        dot = ux * vx + uy * vy
        n = math.hypot(ux, uy) * math.hypot(vx, vy)
        a = math.acos(max(-1.0, min(1.0, dot / n))) if n else 0.0
        return -a if ux * vy - uy * vx < 0 else a

    th1 = ang(1, 0, (x1p - cxp) / rx, (y1p - cyp) / ry)
    dth = ang((x1p - cxp) / rx, (y1p - cyp) / ry, (-x1p - cxp) / rx, (-y1p - cyp) / ry)
    if not sweep and dth > 0:
        dth -= 2 * math.pi
    elif sweep and dth < 0:
        dth += 2 * math.pi
    n = max(2, int(abs(dth) / (math.pi / 2) * SEGS))
    out = []
    for i in range(1, n + 1):
        t = th1 + dth * i / n
        ex = cx + rx * math.cos(t) * cp - ry * math.sin(t) * sp
        ey = cy + rx * math.cos(t) * sp + ry * math.sin(t) * cp
        out.append((ex, ey))
    return out


def _bez(p0, pts, n=SEGS):
    """Bezier beliebigen Grades (quadratisch oder kubisch) als Polylinie."""
    ctrl = [p0] + list(pts)
    out = []
    for i in range(1, n + 1):
        t = i / float(n)
        q = ctrl
        while len(q) > 1:
            q = [(a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t)
                 for a, b in zip(q, q[1:])]
        out.append(q[0])
    return out


def path_to_subpaths(d):
    """Pfaddaten -> Liste von Punktlisten (je Teilpfad), Koordinaten im viewBox."""
    t = _tokens(d)
    i = 0
    cur = []
    subs = []
    x = y = 0.0
    sx = sy = 0.0
    cmd = None
    prev_c2 = None          # letzter Kontrollpunkt fuer S/T
    prev_was_cubic = False

    def num():
        nonlocal i
        v = float(t[i]); i += 1
        return v

    while i < len(t):
        if t[i] in 'MmLlHhVvCcSsQqTtAaZz':
            cmd = t[i]; i += 1
        rel = cmd.islower()
        c = cmd.upper()
        if c == 'M':
            if cur:
                subs.append(cur)
            px, py = num(), num()
            x, y = (x + px, y + py) if rel else (px, py)
            sx, sy = x, y
            cur = [(x, y)]
            cmd = 'l' if rel else 'L'
            prev_was_cubic = False
        elif c == 'L':
            px, py = num(), num()
            x, y = (x + px, y + py) if rel else (px, py)
            cur.append((x, y)); prev_was_cubic = False
        elif c == 'H':
            px = num(); x = x + px if rel else px
            cur.append((x, y)); prev_was_cubic = False
        elif c == 'V':
            py = num(); y = y + py if rel else py
            cur.append((x, y)); prev_was_cubic = False
        elif c in 'CS':
            if c == 'C':
                a = (num(), num()); b = (num(), num())
            else:
                b0 = prev_c2 if prev_was_cubic and prev_c2 else (x, y)
                a = (2 * x - b0[0], 2 * y - b0[1])
                if rel:
                    a = (a[0] - x, a[1] - y)
                b = (num(), num())
            e = (num(), num())
            if rel:
                a = (x + a[0], y + a[1]); b = (x + b[0], y + b[1]); e = (x + e[0], y + e[1])
            cur += _bez((x, y), [a, b, e])
            prev_c2 = b; prev_was_cubic = True
            x, y = e
        elif c in 'QT':
            if c == 'Q':
                a = (num(), num())
            else:
                b0 = prev_c2 if (not prev_was_cubic) and prev_c2 else (x, y)
                a = (2 * x - b0[0], 2 * y - b0[1])
                if rel:
                    a = (a[0] - x, a[1] - y)
            e = (num(), num())
            if rel:
                a = (x + a[0], y + a[1]); e = (x + e[0], y + e[1])
            cur += _bez((x, y), [a, e])
            prev_c2 = a; prev_was_cubic = False
            x, y = e
        elif c == 'A':
            rx, ry, phi = num(), num(), num()
            large, sweep = int(num()), int(num())
            ex, ey = num(), num()
            if rel:
                ex, ey = x + ex, y + ey
            cur += _arc(x, y, rx, ry, phi, large, sweep, ex, ey)
            x, y = ex, ey
            prev_was_cubic = False
        elif c == 'Z':
            if cur:
                cur.append((sx, sy)); subs.append(cur); cur = []
            x, y = sx, sy
            prev_was_cubic = False
        else:
            raise ValueError('unbekanntes Kommando %r' % cmd)
    if cur:
        subs.append(cur)
    return subs


def render(svg_text, size=32):
    m = re.search(r'\bd="([^"]+)"', svg_text)
    if not m:
        raise ValueError('kein <path d="..."> gefunden')
    vb = re.search(r'viewBox="([\d.\s-]+)"', svg_text)
    vx, vy, vw, vh = [float(v) for v in vb.group(1).split()] if vb else (0, 0, 24, 24)
    subs = path_to_subpaths(m.group(1))
    side = int(vw * SS)
    mask = Image.new('1', (side, side), 0)
    for sp in subs:
        if len(sp) < 3:
            continue
        one = Image.new('1', (side, side), 0)
        ImageDraw.Draw(one).polygon([((px - vx) * SS, (py - vy) * SS) for px, py in sp], fill=1)
        mask = ImageChops.logical_xor(mask, one)          # even-odd
    # Graustufen, herunterrechnen, als Alpha eines schwarzen Bildes verwenden
    alpha = mask.convert('L').resize((size, size), Image.LANCZOS)
    out = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    out.putalpha(alpha)
    return out


if __name__ == '__main__':
    dst = os.path.join('src', 'Baggages', 'Icons')
    os.makedirs(dst, exist_ok=True)
    for p in sys.argv[1:]:
        name = os.path.splitext(os.path.basename(p))[0]
        img = render(io.open(p, encoding='utf-8').read())
        img.save(os.path.join(dst, name + '.png'))
        print('%-30s -> %s' % (name, os.path.join(dst, name + '.png')))
