"""Genera widgets/<nombre>/icon.ico (16, 32, 48 y 256 px, PNG) con el estilo común.

Estilo: cuadrado redondeado oscuro (como `ink` de la paleta) con un dibujo simple en
los colores de la paleta (o del juego, p.ej. las caras del cubo). Se dibuja a 1024 px
y se reduce, así los bordes salen suavizados en todos los tamaños.

Herramienta de desarrollo (necesita Pillow); los .ico se guardan en el repositorio y
compilar no la necesita. Uso:  python herramientas/iconos.py
"""
import io
import struct
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent.parent
SIZES = (16, 32, 48, 256)
BIG = 1024

# paleta (common/widget.c)
INK = (22, 24, 31, 255)
TEXT = (240, 242, 248, 255)
SUB = (150, 156, 170, 255)
ACCENT = (250, 190, 70, 255)
ERROR = (235, 80, 80, 255)
OK = (50, 195, 170, 255)
# colores del juego del cubo (widgets/cube/cube.c, FACE_COL)
FACES = [(255, 255, 255), (255, 0, 0), (0, 255, 0), (255, 255, 0), (255, 144, 0), (0, 0, 255)]


def base():
    im = Image.new("RGBA", (BIG, BIG), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.rounded_rectangle((32, 32, BIG - 32, BIG - 32), radius=200, fill=INK)
    return im, d


def cube():
    # cara frontal 3x3: pegatinas con un patrón mezclado, fijo
    im, d = base()
    pattern = [0, 1, 2, 4, 0, 5, 3, 2, 1]
    cell, gap, x0 = 230, 34, 512 - (3 * 230 + 2 * 34) // 2
    for i, f in enumerate(pattern):
        x = x0 + (i % 3) * (cell + gap)
        y = x0 + (i // 3) * (cell + gap)
        d.rounded_rectangle((x, y, x + cell, y + cell), radius=40, fill=FACES[f] + (255,))
    return im


def dot():
    # el punto que hay que clicar, con un anillo tenue alrededor
    im, d = base()
    d.ellipse((232, 232, 792, 792), outline=OK[:3] + (110,), width=40)
    d.ellipse((352, 352, 672, 672), fill=OK)
    return im


def typing():
    # líneas de texto (la escrita en blanco, el resto en gris) y el cursor en acento
    im, d = base()
    d.rounded_rectangle((200, 300, 560, 380), radius=40, fill=TEXT)
    d.rounded_rectangle((600, 300, 824, 380), radius=40, fill=SUB)
    d.rounded_rectangle((200, 472, 460, 552), radius=40, fill=SUB)
    d.rounded_rectangle((500, 472, 824, 552), radius=40, fill=SUB)
    d.rounded_rectangle((200, 644, 640, 724), radius=40, fill=SUB)
    d.rounded_rectangle((572, 270, 600, 410), radius=14, fill=ACCENT)
    return im


def png(im, size):
    small = im.resize((size, size), Image.LANCZOS)
    # paleta de 32 colores con alfa: ~4 KB por .ico (cuenta para el tope del .exe) y
    # sin diferencia visible en dibujos planos
    q = small.quantize(colors=32, method=Image.FASTOCTREE, dither=Image.NONE)
    buf = io.BytesIO()
    q.save(buf, "PNG", optimize=True)
    return buf.getvalue()


def write_ico(path, im):
    blobs = [png(im, s) for s in SIZES]
    out = struct.pack("<HHH", 0, 1, len(blobs))
    offset = 6 + 16 * len(blobs)
    for s, b in zip(SIZES, blobs):
        out += struct.pack("<BBBBHHII", s % 256, s % 256, 0, 0, 1, 32, len(b), offset)
        offset += len(b)
    path.write_bytes(out + b"".join(blobs))
    print(f"{path.relative_to(ROOT)}  {(len(out) + sum(map(len, blobs))) / 1024:.1f} KB")


for name, draw in (("cube", cube), ("dot", dot), ("typing", typing)):
    write_ico(ROOT / "widgets" / name / "icon.ico", draw())
