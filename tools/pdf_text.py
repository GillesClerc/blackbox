#!/usr/bin/env python3
"""Extrait le texte d'un PDF sans dependance externe (stdlib seule).

Le container n'a ni poppler (pdftotext) ni pypdf, et la regle « datasheet
d'abord » du projet impose de lire les PDF. Ce script decode les flux de
contenu (FlateDecode) et reconstruit le texte a partir des operateurs Tj/TJ,
en se servant du kerning des tableaux TJ pour savoir ou sont les espaces
(sans ca, les datasheets Murata/PUI sortent avec une lettre par mot).

Usage :
    tools/pdf_text.py fichier.pdf                  # tout le texte
    tools/pdf_text.py fichier.pdf -g "rated current"   # lignes correspondantes
    tools/pdf_text.py fichier.pdf -g motif -C 3    # avec 3 lignes de contexte

Limites : ne gere pas les polices CID/hex (<0041>) ni les PDF scannes (images).
Si la sortie est vide ou illisible, le PDF est probablement scanne.
"""
import re
import sys
import zlib

# kerning (en millièmes d'em) au-dela duquel on considere un espace
SPACE_KERN = -140


def content_streams(data: bytes):
    """Rend les flux de contenu decompresses du PDF."""
    for m in re.finditer(rb'stream\r?\n', data):
        start = m.end()
        end = data.find(b'endstream', start)
        if end < 0:
            continue
        raw = data[start:end]
        try:
            out = zlib.decompress(raw)
        except zlib.error:
            try:  # flux tronque : garder ce qui se decompresse
                out = zlib.decompressobj().decompress(raw)
            except zlib.error:
                continue
        if b'Tj' in out or b'TJ' in out:
            yield out


def unescape(s: bytes) -> str:
    out = bytearray()
    i = 0
    while i < len(s):
        c = s[i]
        if c == 0x5C and i + 1 < len(s):  # backslash
            n = s[i + 1]
            if n in b'nrtbf':
                out.append({0x6E: 10, 0x72: 13, 0x74: 9, 0x62: 8, 0x66: 12}[n])
                i += 2
            elif 0x30 <= n <= 0x37:  # \ooo octal
                j = i + 1
                digits = b''
                while j < len(s) and len(digits) < 3 and 0x30 <= s[j] <= 0x37:
                    digits += s[j:j + 1]
                    j += 1
                out.append(int(digits, 8) & 0xFF)
                i = j
            else:
                out.append(n)
                i += 2
        else:
            out.append(c)
            i += 1
    return out.decode('latin-1')


TOKEN = re.compile(rb'''
      \((?:\\.|[^()\\])*\)        # chaine litterale
    | \[                          # debut de tableau TJ
    | \]
    | -?\d+\.?\d*                 # nombre
    | (?:Tj|TJ|Td|TD|T\*|Tm|BT|ET|')  # operateurs utiles
''', re.VERBOSE | re.DOTALL)


def stream_text(stream: bytes) -> str:
    out = []
    array = None          # elements du tableau TJ en cours
    pending = []          # operandes numeriques avant un operateur
    for m in TOKEN.finditer(stream):
        t = m.group(0)
        if t == b'[':
            array = []
        elif t == b']':
            pass
        elif t.startswith(b'('):
            s = unescape(t[1:-1])
            if array is not None:
                array.append(s)
            else:
                pending.append(s)
        elif re.fullmatch(rb'-?\d+\.?\d*', t):
            if array is not None:
                array.append(float(t))
            else:
                pending.append(float(t))
        elif t in (b'Tj', b"'"):
            out.extend(x for x in pending if isinstance(x, str))
            pending = []
        elif t == b'TJ':
            if array:
                buf = []
                for el in array:
                    if isinstance(el, str):
                        buf.append(el)
                    elif el <= SPACE_KERN:
                        buf.append(' ')
                out.append(''.join(buf))
            array = None
            pending = []
        elif t in (b'Td', b'TD', b'T*', b'Tm'):
            out.append('\n')
            pending = []
            array = None
        elif t in (b'BT', b'ET'):
            pending = []
            array = None
    text = ''.join(out)
    text = re.sub(r'[ \t]+', ' ', text)
    text = re.sub(r'\n\s*\n+', '\n', text)
    return text


def extract(path: str) -> str:
    data = open(path, 'rb').read()
    pages = [stream_text(s) for s in content_streams(data)]
    return '\n'.join(p.strip() for p in pages if p.strip())


def main():
    args = sys.argv[1:]
    if not args:
        print(__doc__)
        return 1
    path = args[0]
    text = extract(path)
    pattern = None
    context = 0
    if '-g' in args:
        pattern = args[args.index('-g') + 1]
    if '-C' in args:
        context = int(args[args.index('-C') + 1])
    if not pattern:
        print(text)
        return 0
    lines = text.splitlines()
    rx = re.compile(pattern, re.IGNORECASE)
    shown = set()
    for i, line in enumerate(lines):
        if rx.search(line):
            for j in range(max(0, i - context), min(len(lines), i + context + 1)):
                if j not in shown:
                    print(f'{j:5d}: {lines[j]}')
                    shown.add(j)
    return 0


if __name__ == '__main__':
    sys.exit(main())
