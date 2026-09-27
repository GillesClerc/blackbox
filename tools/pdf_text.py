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


# --------------------------------------------------------------------------
# Polices a encodage propriétaire (PUI Audio, certains PDF Word/InDesign) :
# les codes des chaines ne sont pas du latin-1 et il faut passer par la table
# /ToUnicode de chaque police. Sans ca le texte sort en charabia.
# --------------------------------------------------------------------------

def _obj_table(data: bytes):
    """numero d'objet -> corps brut."""
    return {int(m.group(1)): m.group(2)
            for m in re.finditer(rb'(?:^|[\s>])(\d+)\s+0\s+obj\b([\s\S]*?)endobj', data)}


def _obj_stream(body: bytes):
    m = re.search(rb'stream\r?\n', body)
    if not m:
        return None
    end = body.find(b'endstream', m.end())
    raw = body[m.end():end if end > 0 else None]
    if b'FlateDecode' in body[:m.start()]:
        try:
            return zlib.decompress(raw)
        except zlib.error:
            return None
    return raw


def _parse_cmap(cmap: bytes):
    """CMap ToUnicode -> {code: texte}."""
    table = {}
    for blk in re.findall(rb'beginbfchar([\s\S]*?)endbfchar', cmap):
        for src, dst in re.findall(rb'<([0-9A-Fa-f]+)>\s*<([0-9A-Fa-f]+)>', blk):
            table[int(src, 16)] = _utf16(dst)
    for blk in re.findall(rb'beginbfrange([\s\S]*?)endbfrange', cmap):
        # <lo> <hi> <dst>
        for lo, hi, dst in re.findall(
                rb'<([0-9A-Fa-f]+)>\s*<([0-9A-Fa-f]+)>\s*<([0-9A-Fa-f]+)>', blk):
            base = int(dst, 16)
            for i in range(int(lo, 16), int(hi, 16) + 1):
                table[i] = chr(base + i - int(lo, 16))
        # <lo> <hi> [<d1> <d2> ...]
        for lo, _hi, arr in re.findall(
                rb'<([0-9A-Fa-f]+)>\s*<([0-9A-Fa-f]+)>\s*\[([\s\S]*?)\]', blk):
            for k, dst in enumerate(re.findall(rb'<([0-9A-Fa-f]+)>', arr)):
                table[int(lo, 16) + k] = _utf16(dst)
    return table


def _utf16(hexstr: bytes) -> str:
    raw = bytes.fromhex(hexstr.decode('ascii'))
    if len(raw) >= 2:
        try:
            return raw.decode('utf-16-be')
        except UnicodeDecodeError:
            pass
    return raw.decode('latin-1')


# Noms de glyphes PostScript les plus courants dans les datasheets. Un nom d'une
# seule lettre vaut cette lettre ; /uniXXXX donne le point de code directement ;
# les noms anonymes (/g3, /g367) ne sont pas decodables et sont ignores.
GLYPH_NAMES = {
    'space': ' ', 'period': '.', 'comma': ',', 'hyphen': '-', 'slash': '/',
    'percent': '%', 'degree': '°', 'plusminus': '±', 'copyright': '©',
    'bullet': '•', 'at': '@', 'colon': ':', 'semicolon': ';', 'plus': '+',
    'equal': '=', 'asterisk': '*', 'quotesingle': "'", 'quotedbl': '"',
    'parenleft': '(', 'parenright': ')', 'bracketleft': '[', 'bracketright': ']',
    'braceleft': '{', 'braceright': '}', 'underscore': '_', 'ampersand': '&',
    'exclam': '!', 'question': '?', 'numbersign': '#', 'dollar': '$',
    'less': '<', 'greater': '>', 'endash': '–', 'emdash': '—',
    'quoteright': '’', 'quoteleft': '‘',
    'quotedblleft': '“', 'quotedblright': '”',
    'registered': '®', 'trademark': '™', 'micro': 'µ', 'mu': 'µ',
    'Omega': 'Ω', 'ohm': 'Ω', 'multiply': '×', 'divide': '÷', 'minus': '−',
    'zero': '0', 'one': '1', 'two': '2', 'three': '3', 'four': '4',
    'five': '5', 'six': '6', 'seven': '7', 'eight': '8', 'nine': '9',
    'fi': 'fi', 'fl': 'fl', 'tilde': '~', 'asciitilde': '~', 'bar': '|',
}


def _glyph(name: str) -> str:
    if name in GLYPH_NAMES:
        return GLYPH_NAMES[name]
    if len(name) == 1:
        return name
    m = re.fullmatch(r'uni([0-9A-Fa-f]{4,6})', name)
    if m:
        return chr(int(m.group(1), 16))
    return ''            # /g3, /g367 : glyphe anonyme, non decodable


def _parse_differences(enc_body: bytes):
    """/Encoding ... /Differences [1 /P /U ...] -> {code: caractere}."""
    m = re.search(rb'/Differences\s*\[([\s\S]*?)\]', enc_body)
    if not m:
        return {}
    table, code = {}, 0
    for tok in re.findall(rb'\d+|/[^\s/\]]+', m.group(1)):
        if tok.isdigit():
            code = int(tok)
        else:
            ch = _glyph(tok[1:].decode('latin-1'))
            if ch:
                table[code] = ch
            code += 1
    return table


def font_maps(data: bytes):
    """nom de ressource (/F1...) -> table de decodage, et signale les conflits."""
    objs = _obj_table(data)

    def deref(body, key):
        """Corps de l'objet vise par /key N 0 R, ou le dictionnaire inline."""
        m = re.search(rb'/' + key + rb'\s+(\d+)\s+0\s+R', body)
        if m:
            return objs.get(int(m.group(1)), b'')
        m = re.search(rb'/' + key + rb'\s*<<([\s\S]*?)>>', body)
        return m.group(1) if m else b''

    table_of_obj = {}
    for n, body in objs.items():
        if b'/ToUnicode' in body:
            s = _obj_stream(deref(body, b'ToUnicode'))
            if s:
                t = _parse_cmap(s)
                if t:
                    table_of_obj[n] = t
        if n not in table_of_obj:
            enc = deref(body, b'Encoding')
            t = _parse_differences(enc) if enc else {}
            if t:
                table_of_obj[n] = t

    out, conflicts = {}, set()
    for body in objs.values():
        fonts_dict = deref(body, b'Font')
        if not fonts_dict:
            continue
        for name, ref in re.findall(rb'/(\w+)\s+(\d+)\s+0\s+R', fonts_dict):
            t = table_of_obj.get(int(ref))
            if not t:
                continue
            key = name.decode('latin-1')
            if key in out and out[key] != t:
                conflicts.add(key)
            out[key] = t
    return out, conflicts


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


def unescape_bytes(s: bytes) -> bytes:
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
    return bytes(out)


TOKEN = re.compile(rb'''
      \((?:\\.|[^()\\])*\)        # chaine litterale
    | <[0-9A-Fa-f\s]*>            # chaine hexadecimale
    | /\w+                        # nom de ressource (police)
    | \[                          # debut de tableau TJ
    | \]
    | -?\d+\.?\d*                 # nombre
    | (?:Tj|TJ|Td|TD|T\*|Tm|Tf|BT|ET|')  # operateurs utiles
''', re.VERBOSE | re.DOTALL)


def decode_bytes(raw: bytes, table) -> str:
    """Applique la table /ToUnicode ; sans table, latin-1."""
    if not table:
        return raw.decode('latin-1')
    wide = any(k > 255 for k in table)
    out = []
    if wide:
        for i in range(0, len(raw) - 1, 2):
            code = (raw[i] << 8) | raw[i + 1]
            out.append(table.get(code, ''))
    else:
        for b in raw:
            out.append(table.get(b, ''))
    return ''.join(out)


def stream_text(stream: bytes, fonts=None) -> str:
    fonts = fonts or {}
    out = []
    array = None          # elements du tableau TJ en cours
    pending = []          # operandes numeriques avant un operateur
    table = None          # table de la police active
    last_name = None
    for m in TOKEN.finditer(stream):
        t = m.group(0)
        if t == b'[':
            array = []
        elif t == b']':
            pass
        elif t.startswith(b'/'):
            last_name = t[1:].decode('latin-1')
        elif t == b'Tf':
            if last_name is not None:
                table = fonts.get(last_name)
            pending = []
        elif t.startswith(b'<'):
            hexstr = re.sub(rb'\s', b'', t[1:-1])
            if not re.fullmatch(rb'[0-9A-Fa-f]*', hexstr):
                continue    # « << » de dictionnaire inline (images), pas une chaine hex
            if len(hexstr) % 2:
                hexstr += b'0'  # PDF : un dernier chiffre impair vaut « x0 »
            raw = bytes.fromhex(hexstr.decode('ascii'))
            s = decode_bytes(raw, table)
            if array is not None:
                array.append(s)
            else:
                pending.append(s)
        elif t.startswith(b'('):
            s = decode_bytes(unescape_bytes(t[1:-1]), table)
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


def extract(path: str, warn=True) -> str:
    data = open(path, 'rb').read()
    fonts, conflicts = font_maps(data)
    if conflicts and warn:
        print(f'# attention : polices au mapping ambigu : {sorted(conflicts)}',
              file=sys.stderr)
    pages = [stream_text(s, fonts) for s in content_streams(data)]
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
