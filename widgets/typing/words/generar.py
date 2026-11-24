"""
Genera las listas de palabras del widget de mecanografia.

Frecuencias: wordfreq (https://github.com/rspeer/wordfreq), datos CC BY-SA 4.0.
Ortografia: diccionarios Hunspell de LibreOffice (solo para verificar, no se incluyen).

Uso (en un entorno con `pip install wordfreq spylls`):
    python generar.py <carpeta con es_ES.dic/.aff y en_US.dic/.aff>

Proceso para cada idioma:
  1. Las N palabras mas frecuentes segun wordfreq (mezcla de Wikipedia,
     subtitulos, noticias, libros y redes).
  2. Se descartan: lo que no sea solo letras del idioma, letras sueltas que no
     son palabras, lo que no pasa el corrector ortografico en minusculas
     (elimina nombres propios, extranjerismos, abreviaturas y faltas como
     "tambien") y la lista EXCLUIR (revision a mano: groserias, etc.).
  3. Se guardan las primeras 200 y 1000 en orden de frecuencia.
"""
import os
import sys

from spylls.hunspell import Dictionary
from wordfreq import top_n_list

IDIOMAS = {
    "es": {
        "dic": "es_ES",
        "letras": set("abcdefghijklmnopqrstuvwxyzáéíóúüñ"),
        "sueltas": {"a", "e", "o", "u", "y"},
    },
    "en": {
        "dic": "en_US",
        "letras": set("abcdefghijklmnopqrstuvwxyz"),
        "sueltas": {"a"},  # "I" se escribe en mayuscula: se omite como en monkeytype
    },
}

# Revision a mano: palabras validas pero que no queremos en un test (groserias,
# insultos, temas delicados) o que se cuelan aunque pasen el corrector.
EXCLUIR = {
    "es": {
        # groserias y temas que no queremos en un test
        "mierda", "puta", "sexo",
        # nombres propios que el corrector acepta en minuscula
        "argentina", "brasil", "china", "cuba", "francisco", "santiago", "juan", "luis", "maría", "pedro", "york",
        # abreviaturas y prefijos sueltos
        "etc", "ex", "in", "pp", "re", "sr",
        # tildes que la RAE ya no recomienda (se usan "solo" y "este")
        "sólo", "éste",
    },
    "en": {
        "ass", "damn", "fuck", "fucking", "hell", "sex", "shit",
        "john",
        "anti", "co", "etc", "ii", "la", "non", "re", "st",
        "gonna", "wanna",
    },
}

TAMANOS = (200, 1000)
CANDIDATAS = 6000


def generar(idioma, carpeta_dic, salida):
    cfg = IDIOMAS[idioma]
    dic = Dictionary.from_files(os.path.join(carpeta_dic, cfg["dic"]))
    elegidas, descartes = [], []
    for w in top_n_list(idioma, CANDIDATAS):
        motivo = None
        if not set(w) <= cfg["letras"]:
            motivo = "letras"
        elif len(w) == 1 and w not in cfg["sueltas"]:
            motivo = "suelta"
        elif w in EXCLUIR[idioma]:
            motivo = "excluida"
        elif not dic.lookup(w):
            motivo = "ortografia"
        if motivo:
            descartes.append((w, motivo))
        else:
            elegidas.append(w)
        if len(elegidas) >= max(TAMANOS):
            break
    for n in TAMANOS:
        with open(os.path.join(salida, f"{idioma}_{n}.txt"), "w", encoding="utf-8", newline="\n") as f:
            f.write("\n".join(elegidas[:n]) + "\n")
    with open(os.path.join(salida, f"descartes_{idioma}.txt"), "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(f"{w}\t{m}" for w, m in descartes) + "\n")
    print(f"{idioma}: {len(elegidas)} elegidas, {len(descartes)} descartadas")


if __name__ == "__main__":
    carpeta = sys.argv[1]
    salida = sys.argv[2] if len(sys.argv) > 2 else os.path.dirname(os.path.abspath(__file__))
    for idioma in IDIOMAS:
        generar(idioma, carpeta, salida)
