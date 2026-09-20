# -*- coding: utf-8 -*-
"""Erzeugt die Bilder fuer die ETS-Applikation.

  Baggages/Icons/kwlhw-N.png   32x32, je Eintrag der Hardwareauswahl
  Baggages/Icons/kwlboard-N.png groesseres Klemmenbild fuer die Seite "Anschluss"
  graphics/kwlboard-N.svg      bearbeitbare Quelle desselben Bildes

Die Klemmen sind von RECHTS nach LINKS angeordnet: Kanal 1 liegt rechts aussen.
Aufruf aus dem OFM-Wurzelverzeichnis:  python graphics/make_images.py
"""
import os
from PIL import Image, ImageDraw, ImageFont

ICON_DIR = os.path.join('src', 'Baggages', 'Icons')
SVG_DIR = 'graphics'

# Klemmenbeschriftung je Platine. Kanal 1 steht vorn und liegt rechts aussen.
BOARDS = {
    2: ['VOUT0', 'VOUT1'],
    4: ['J3', 'J4', 'J5', 'J6'],
}
NETS = {
    2: ['S1', 'S2'],
    4: ['S1', 'S2', 'S3', 'S4'],
}
BOARD_NAME = {2: 'Entwicklungsaufbau Pico + DFR1073', 4: 'KNXFANDRV Rev 0.1'}

INK = (32, 38, 46)
MUTED = (120, 130, 142)
BOARD = (222, 232, 226)
BOARD_EDGE = (120, 150, 130)
TERM = (60, 68, 78)
PIN12 = (206, 76, 60)     # 12 V
PINS = (236, 170, 54)     # Stellsignal
PINGND = (70, 78, 88)     # GND
PAPER = (255, 255, 255)


def font(size, bold=False):
    for name in (('seguisb.ttf', 'segoeuib.ttf', 'arialbd.ttf') if bold
                 else ('segoeui.ttf', 'arial.ttf')):
        try:
            return ImageFont.truetype(os.path.join('C:\\Windows\\Fonts', name), size)
        except Exception:
            pass
    return ImageFont.load_default()


def centred(d, x, y, text, f, fill):
    try:
        l, t, r, b = d.textbbox((0, 0), text, font=f)
        w, h = r - l, b - t
    except Exception:
        w, h = d.textsize(text, font=f)
        l = t = 0
    d.text((x - w / 2 - l, y - h / 2 - t), text, font=f, fill=fill)


# ---------------------------------------------------------------- 32x32-Icon
def make_icon(n):
    S = 4  # vierfach zeichnen, dann herunterrechnen: weiche Kanten ohne AA-Code
    img = Image.new('RGBA', (32 * S, 32 * S), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([3 * S, 7 * S, 29 * S, 25 * S], radius=2 * S,
                        fill=BOARD, outline=BOARD_EDGE, width=1 * S)
    # Klemmen an der Unterkante, von rechts nach links gefuellt
    slot_w = 2.0 * S
    gap = max(0.6 * S, (22.0 * S - n * slot_w) / max(n - 1, 1))
    x = 27.0 * S
    for _ in range(n):
        d.rectangle([x - slot_w, 19 * S, x, 24 * S], fill=TERM)
        x -= slot_w + gap
        if x < 4 * S:
            break
    # Kanalzahl als Ziffer in die freie Flaeche
    centred(d, 16 * S, 13 * S, str(n), font(9 * S, bold=True), INK)
    return img.resize((32, 32), Image.LANCZOS)


# ---------------------------------------------------------------- Klemmenbild
PIN_TXT = [('1', '12 V', PIN12), ('2', 'S', PINS), ('3', 'GND', PINGND)]


def board_layout(n):
    """Gemeinsame Geometrie fuer PNG und SVG."""
    term_w, term_h = 104, 54
    gap = 18
    margin = 26
    width = margin * 2 + n * term_w + (n - 1) * gap
    height = 218
    y_board_top, y_board_bot = 56, 154
    y_term = y_board_bot - term_h
    # Kanal 1 ganz rechts
    xs = [width - margin - term_w - i * (term_w + gap) for i in range(n)]
    return dict(w=width, h=height, tw=term_w, th=term_h, yt=y_term,
                ybt=y_board_top, ybb=y_board_bot, xs=xs, m=margin)


def make_board_png(n):
    L = board_layout(n)
    S = 2
    img = Image.new('RGB', (L['w'] * S, L['h'] * S), PAPER)
    d = ImageDraw.Draw(img)
    f_small, f_mid, f_big = font(11 * S), font(13 * S, True), font(15 * S, True)

    centred(d, L['w'] * S / 2, 14 * S, BOARD_NAME[n], f_mid, INK)

    d.rounded_rectangle([L['m'] * S - 12 * S, L['ybt'] * S,
                         (L['w'] - L['m']) * S + 12 * S, L['ybb'] * S],
                        radius=6 * S, fill=BOARD, outline=BOARD_EDGE, width=2 * S)

    for i, x in enumerate(L['xs']):
        x0, x1 = x * S, (x + L['tw']) * S
        y0, y1 = L['yt'] * S, (L['yt'] + L['th']) * S
        d.rounded_rectangle([x0, y0, x1, y1], radius=3 * S, fill=TERM)
        pw = (x1 - x0) / 3.0
        for p, (num, lbl, col) in enumerate(PIN_TXT):
            cx = x0 + pw * (p + 0.5)
            d.ellipse([cx - 9 * S, y1 - 22 * S, cx + 9 * S, y1 - 4 * S], fill=col)
            centred(d, cx, y1 - 13 * S, num, f_small, PAPER if col != PINS else INK)
            centred(d, cx, (L['ybb'] + 14) * S, lbl, f_small, MUTED)
        centred(d, (x0 + x1) / 2, y0 + 14 * S, BOARDS[n][i], f_mid, PAPER)
        centred(d, (x0 + x1) / 2, (L['ybt'] - 11) * S,
                'Kanal %d  ·  %s' % (i + 1, NETS[n][i]), f_big, INK)

    centred(d, L['w'] * S / 2, (L['h'] - 14) * S,
            'Kanal 1 liegt rechts aussen.  1 = 12 V   2 = Stellsignal S   3 = GND',
            f_small, MUTED)
    return img.resize((L['w'], L['h']), Image.LANCZOS)


def make_board_svg(n):
    L = board_layout(n)
    o = ['<?xml version="1.0" encoding="utf-8"?>',
         '<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" '
         'viewBox="0 0 %d %d" font-family="Segoe UI, Arial, sans-serif">'
         % (L['w'], L['h'], L['w'], L['h']),
         '<rect width="100%%" height="100%%" fill="#fff"/>',
         '<text x="%d" y="19" text-anchor="middle" font-size="13" font-weight="600" fill="#20262e">%s</text>'
         % (L['w'] // 2, BOARD_NAME[n]),
         '<rect x="%d" y="%d" width="%d" height="%d" rx="6" fill="#dee8e2" stroke="#789682" stroke-width="2"/>'
         % (L['m'] - 12, L['ybt'], L['w'] - 2 * L['m'] + 24, L['ybb'] - L['ybt'])]
    cols = ['#ce4c3c', '#ecaa36', '#464e58']
    for i, x in enumerate(L['xs']):
        o.append('<rect x="%d" y="%d" width="%d" height="%d" rx="3" fill="#3c444e"/>'
                 % (x, L['yt'], L['tw'], L['th']))
        o.append('<text x="%d" y="%d" text-anchor="middle" font-size="13" font-weight="600" fill="#fff">%s</text>'
                 % (x + L['tw'] // 2, L['yt'] + 18, BOARDS[n][i]))
        o.append('<text x="%d" y="%d" text-anchor="middle" font-size="15" font-weight="600" fill="#20262e">Kanal %d &#183; %s</text>'
                 % (x + L['tw'] // 2, L['ybt'] - 6, i + 1, NETS[n][i]))
        for p, (num, lbl, _c) in enumerate(PIN_TXT):
            cx = x + L['tw'] * (p + 0.5) / 3.0
            o.append('<circle cx="%.1f" cy="%d" r="9" fill="%s"/>'
                     % (cx, L['yt'] + L['th'] - 13, cols[p]))
            o.append('<text x="%.1f" y="%d" text-anchor="middle" font-size="11" fill="%s">%s</text>'
                     % (cx, L['yt'] + L['th'] - 9, '#20262e' if p == 1 else '#fff', num))
            o.append('<text x="%.1f" y="%d" text-anchor="middle" font-size="11" fill="#78828e">%s</text>'
                     % (cx, L['ybb'] + 18, lbl))
    o.append('<text x="%d" y="%d" text-anchor="middle" font-size="11" fill="#78828e">'
             'Kanal 1 liegt rechts au&#223;en. 1 = 12 V &#183; 2 = Stellsignal S &#183; 3 = GND</text>'
             % (L['w'] // 2, L['h'] - 10))
    o.append('</svg>')
    return '\n'.join(o)


if __name__ == '__main__':
    os.makedirs(ICON_DIR, exist_ok=True)
    os.makedirs(SVG_DIR, exist_ok=True)
    for n in (2, 4, 6, 8, 10, 12):
        make_icon(n).save(os.path.join(ICON_DIR, 'kwlhw-%d.png' % n))
    for n in BOARDS:
        make_board_png(n).save(os.path.join(ICON_DIR, 'kwlboard-%d.png' % n))
        with open(os.path.join(SVG_DIR, 'kwlboard-%d.svg' % n), 'w', encoding='utf-8') as fh:
            fh.write(make_board_svg(n))
    print('Bilder erzeugt in %s und %s' % (ICON_DIR, SVG_DIR))
