#!/usr/bin/env python3
"""Extrai pares TR("pt", "en") e I18n::tr("pt", "en") do codigo-fonte para .ts.

Gera translations/aletheia_pt_BR.ts e translations/aletheia_en.ts a partir dos
chamados TR(ptBr, en) / I18n::tr(ptBr, en) nos ficheiros .cpp/.h. Estes .ts
podem depois ser editados no Qt Linguist.

Uso: python3 scripts/extract_tr.py [src_dir]
"""
import re
import sys
import os
from xml.sax.saxutils import escape

SRC_DIR = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "src")
OUT_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "translations")

# Captura TR("pt", "en") e I18n::tr("pt", "en"), tolerando escapes e quebras.
TR_RE = re.compile(
    r'(?:TR|I18n::tr)\s*\(\s*"((?:[^"\\]|\\.)*)"\s*,\s*"((?:[^"\\]|\\.)*)"\s*\)',
    re.DOTALL)

def unescape(s: str) -> str:
    """Converte escapes C++ numa string real (\\\\ primeiro para nao corromper)."""
    return (s.replace("\\\\", "\x00")
             .replace('\\"', '"').replace("\\n", "\n")
             .replace("\\t", "\t").replace("\x00", "\\"))

def scan(root: str):
    pairs = []  # (ficheiro, linha, pt, en)
    seen = set()
    for dirpath, _, files in os.walk(root):
        for f in sorted(files):
            if not f.endswith((".cpp", ".h")):
                continue
            path = os.path.join(dirpath, f)
            try:
                text = open(path, encoding="utf-8").read()
            except OSError:
                continue
            for m in TR_RE.finditer(text):
                line = text[:m.start()].count("\n") + 1
                pt = unescape(m.group(1))
                en = unescape(m.group(2))
                key = (pt, en)
                if key in seen:
                    continue
                seen.add(key)
                pairs.append((path, line, pt, en))
    return pairs

def write_ts(path: str, lang: str, pairs: list):
    """Escreve um .ts agrupado por ficheiro de origem."""
    with open(path, "w", encoding="utf-8") as f:
        f.write('<?xml version="1.0" encoding="utf-8"?>\n')
        f.write('<!DOCTYPE TS>\n')
        f.write(f'<TS version="2.1" language="{lang}">\n')
        by_file = {}
        for fp, line, pt, en in pairs:
            by_file.setdefault(fp, []).append((line, pt, en))
        for fp in sorted(by_file):
            safe_fp = escape(fp, {'"': '&quot;'})
            f.write(f'<context>\n  <name>{escape(fp)}</name>\n')
            for line, pt, en in by_file[fp]:
                f.write(f'  <message>\n    <location filename="{safe_fp}" line="{line}"/>\n')
                f.write(f'    <source>{escape(pt)}</source>\n')
                if lang.startswith("pt"):
                    f.write(f'    <translation>{escape(pt)}</translation>\n')
                else:
                    f.write(f'    <translation>{escape(en)}</translation>\n')
                f.write('  </message>\n')
            f.write('</context>\n')
        f.write('</TS>\n')

def main():
    pairs = scan(SRC_DIR)
    os.makedirs(OUT_DIR, exist_ok=True)
    write_ts(os.path.join(OUT_DIR, "aletheia_pt_BR.ts"), "pt_BR", pairs)
    write_ts(os.path.join(OUT_DIR, "aletheia_en.ts"), "en", pairs)
    print(f"Extraidos {len(pairs)} pares TR()/I18n::tr() de {SRC_DIR}")
    print(f"  -> {OUT_DIR}/aletheia_pt_BR.ts")
    print(f"  -> {OUT_DIR}/aletheia_en.ts")

if __name__ == "__main__":
    main()
