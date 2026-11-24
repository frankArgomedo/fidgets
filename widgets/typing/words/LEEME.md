# Listas de palabras

Listas para el widget de mecanografía: las palabras más frecuentes de cada idioma,
en orden de frecuencia y con una palabra por línea.

| Archivo | Contenido |
| --- | --- |
| `es_200.txt`, `es_1000.txt` | Español: las 200 y las 1000 más frecuentes |
| `en_200.txt`, `en_1000.txt` | Inglés: las 200 y las 1000 más frecuentes |
| `descartes_*.txt` | Palabras frecuentes descartadas y el motivo, para revisar el filtro |
| `generar.py` | Script que genera todo lo anterior |

## Cómo se hicieron

1. **Frecuencias:** las ~1100 palabras más usadas según [wordfreq](https://github.com/rspeer/wordfreq),
   que combina Wikipedia, subtítulos, noticias, libros y redes.
2. **Filtros:**
   - Solo letras del idioma, así que se descartan números, símbolos y apóstrofos (`don't`).
   - Letras sueltas, solo si son palabras («a», «y», «o», «e», «u»; en inglés, «a»).
   - El corrector ortográfico de LibreOffice (Hunspell), aplicado en minúsculas. Elimina
     nombres propios, extranjerismos, abreviaturas y faltas como «tambien».
   - La lista `EXCLUIR` de `generar.py`, de la revisión a mano: groserías, nombres propios
     que el corrector deja pasar, abreviaturas y «sólo» o «éste».
3. Se guardan las primeras 200 y 1000 que pasan los filtros.

Para cambiar una palabra, agrégala a `EXCLUIR` y vuelve a generar:

```powershell
pip install wordfreq spylls
python generar.py <carpeta con es_ES.dic/.aff y en_US.dic/.aff>
```

Los diccionarios están en https://github.com/LibreOffice/dictionaries (carpetas `es` y `en`).
Solo se usan para verificar y no se incluyen aquí.

## Licencia

Estas listas derivan de los datos de wordfreq (© Robyn Speer), publicados bajo
[CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/). Por eso los archivos `.txt`
de esta carpeta se distribuyen bajo la misma licencia. El código del proyecto no se ve
afectado.
