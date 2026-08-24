#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Genera los assets de NEUN a partir del logo JPG de la araña.

Entrada: templates/Gemini_Generated_Image_*.jpg (araña negra sobre fondo blanco)
Salidas:
  static/spider_bg.png   -> silueta con degradado rojo-violeta-azul (marca de agua de fondo)
  static/spider_mark.png -> silueta casi blanca (logos pequeños y favicon)
Uso puntual: python make_assets.py
"""
from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageOps

BASE = Path(__file__).resolve().parent
SRC = next(iter((BASE / "templates").glob("Gemini_Generated_Image_*.jpg")))
OUT_DIR = BASE / "static"
OUT_DIR.mkdir(exist_ok=True)

STOPS = [(0.0, (255, 43, 67)), (0.5, (176, 58, 158)), (1.0, (37, 103, 255))]


def gradient_rgb(w: int, h: int) -> Image.Image:
    """Degradado diagonal rojo->violeta->azul del mismo tamaño que la imagen."""
    grad = Image.new("RGB", (w, h))
    px = grad.load()
    for y in range(h):
        for x in range(0, w, 2):  # paso 2: suficiente suavidad, doble de rápido
            t = (x / max(w - 1, 1) + y / max(h - 1, 1)) / 2
            for i in range(len(STOPS) - 1):
                t0, c0 = STOPS[i]
                t1, c1 = STOPS[i + 1]
                if t0 <= t <= t1:
                    k = (t - t0) / (t1 - t0)
                    r = round(c0[0] + (c1[0] - c0[0]) * k)
                    g = round(c0[1] + (c1[1] - c0[1]) * k)
                    b = round(c0[2] + (c1[2] - c0[2]) * k)
                    break
            px[x, y] = (r, g, b)
            if x + 1 < w:
                px[x + 1, y] = (r, g, b)
    return grad


def solid_rgb(w: int, h: int, color: tuple[int, int, int]) -> Image.Image:
    return Image.new("RGB", (w, h), color)


def build(tint_mode: str, out_name: str) -> None:
    img = Image.open(SRC).convert("L")
    # Alpha = oscuridad del trazo (negro opaco, blanco transparente)
    alpha = ImageOps.invert(img)
    # Limpiar ruido casi-blanco del JPEG antes de recortar
    alpha = alpha.point(lambda p: 0 if p < 18 else min(p, 255))
    # Recortar al bounding box del trazo con un pequeño margen
    bbox = alpha.getbbox()
    pad = 8
    l = max(bbox[0] - pad, 0)
    t = max(bbox[1] - pad, 0)
    r = min(bbox[2] + pad, img.width)
    b = min(bbox[3] + pad, img.height)
    alpha = alpha.crop((l, t, r, b))

    w, h = alpha.size
    if tint_mode == "gradient":
        color = gradient_rgb(w, h)
    else:
        color = solid_rgb(w, h, (238, 242, 255))

    out = Image.merge("RGBA", (*color.split(), alpha))
    out.save(OUT_DIR / out_name, optimize=True)
    print(f"OK {out_name}: {w}x{h}")


if __name__ == "__main__":
    build("gradient", "spider_bg.png")
    build("light", "spider_mark.png")
