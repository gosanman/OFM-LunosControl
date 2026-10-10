# -*- coding: utf-8 -*-
"""Erzeugt das Diagramm "Richtungswechsel" fuer die Verbundseite der ETS.

  Baggages/kwlramp.png   TypePicture auf der Verbundseite, unter Totzeit und Rampe

Zeigt den Ablauf seit 2026-10-10: Stufenspannung -> Rampe auf 5,00 V -> Totzeit
-> Rampe in die neue Richtung, und ab wann die Zykluszeit zaehlt. Beispiel e²60,
Stufe 2, Polaritaet Standard (ueber 5 V Zuluft).

Farben und Schrift wie make_images.py. Gezeichnet wird doppelt so gross und dann
verkleinert, damit die schraegen Linien glatt werden.
Aufruf aus dem OFM-Wurzelverzeichnis:  python graphics/make_ramp.py
"""
import os
from PIL import Image, ImageDraw, ImageFont

PIC = os.path.join('src', 'Baggages', 'kwlramp.png')

INK = (32, 38, 46)
MUTED = (120, 130, 142)
GRID = (205, 212, 220)
SUPPLY = (52, 120, 196)     # Zuluft
EXHAUST = (206, 76, 60)     # Abluft
RAMP = (236, 170, 54)       # Rampe
DEAD = (70, 150, 100)       # Totzeit, 5,00 V
PAPER = (255, 255, 255)

W, H = 560, 260
S = 2  # Ueberabtastung


def font(size, bold=False):
    for name in (('seguisb.ttf', 'segoeuib.ttf', 'arialbd.ttf') if bold
                 else ('segoeui.ttf', 'arial.ttf')):
        try:
            return ImageFont.truetype(os.path.join('C:\\Windows\\Fonts', name), size * S)
        except Exception:
            pass
    return ImageFont.load_default()


def text(d, x, y, s, f, fill, anchor='mm'):
    d.text((x * S, y * S), s, font=f, fill=fill, anchor=anchor)


def line(d, pts, fill, width=2):
    d.line([(x * S, y * S) for x, y in pts], fill=fill, width=width * S, joint='curve')


def dashed(d, x0, x1, y, fill, dash=5, gap=4):
    x = x0
    while x < x1:
        line(d, [(x, y), (min(x + dash, x1), y)], fill, 1)
        x += dash + gap


def bracket(d, x0, x1, y, label, f, fill, arrow=False):
    """Masslinie mit Endstrichen und Beschriftung darunter."""
    line(d, [(x0, y), (x1, y)], fill, 1)
    line(d, [(x0, y - 5), (x0, y + 5)], fill, 1)
    if arrow:
        line(d, [(x1 - 7, y - 4), (x1, y), (x1 - 7, y + 4)], fill, 1)
    else:
        line(d, [(x1, y - 5), (x1, y + 5)], fill, 1)
    text(d, (x0 + x1) / 2, y + 13, label, f, fill)


def main():
    im = Image.new('RGB', (W * S, H * S), PAPER)
    d = ImageDraw.Draw(im)
    f_title = font(15, bold=True)
    f = font(12)
    f_b = font(12, bold=True)
    f_small = font(11)

    # Spannungsachse: drei Pegel genuegen.
    y_sup, y_mid, y_exh = 78, 130, 182
    x_axis = 74
    text(d, W / 2, 16, 'Richtungswechsel im Verbund', f_title, INK)
    for y, s in ((y_sup, '6,91 V'), (y_mid, '5,00 V'), (y_exh, '3,21 V')):
        text(d, x_axis - 8, y, s, f_small, MUTED, anchor='rm')
    line(d, [(x_axis, 40), (x_axis, 200)], GRID, 1)
    dashed(d, x_axis, W - 16, y_mid, GRID)
    text(d, W - 16, y_mid - 9, 'Stillstand', f_small, MUTED, anchor='rm')

    # Zeitabschnitte
    x0, x_rd, x_dt, x_ru, x_run, x1 = x_axis + 6, 214, 266, 318, 370, W - 20

    line(d, [(x0, y_sup), (x_rd, y_sup)], SUPPLY, 3)
    line(d, [(x_rd, y_sup), (x_dt, y_mid)], RAMP, 3)
    line(d, [(x_dt, y_mid), (x_ru, y_mid)], DEAD, 3)
    line(d, [(x_ru, y_mid), (x_run, y_exh)], RAMP, 3)
    line(d, [(x_run, y_exh), (x1, y_exh)], EXHAUST, 3)

    text(d, (x0 + x_rd) / 2, y_sup - 14, 'Zuluft, Stufe 2', f_b, SUPPLY)
    text(d, (x_run + x1) / 2, y_exh + 16, 'Abluft, Stufe 2', f_b, EXHAUST)

    # Masslinien unter dem Wechsel
    bracket(d, x_rd, x_dt, 214, 'Rampe', f, RAMP)
    bracket(d, x_dt, x_ru, 214, 'Totzeit', f, DEAD)
    bracket(d, x_ru, x_run, 214, 'Rampe', f, RAMP)

    # Die Zykluszeit zaehlt ab der Uebernahme der neuen Richtung.
    bracket(d, x_ru, x1, 46, '', f, INK, arrow=True)
    text(d, (x_ru + x1) / 2, 34, 'Zykluszeit ab Übernahme der neuen Richtung', f_small, INK)

    text(d, W / 2, H - 14,
         'Beispiel e²60, Stufe 2, Polarität Standard. Rampe 0 s = Sprung wie bisher.',
         f_small, MUTED)

    im = im.resize((W, H), Image.LANCZOS)
    im.save(PIC, optimize=True)
    print('geschrieben:', PIC, im.size)


if __name__ == '__main__':
    main()
