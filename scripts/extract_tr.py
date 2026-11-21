#!/usr/bin/env python3
"""Extrai pares TR("pt", "en") do codigo-fonte para ficheiros .ts (Qt Linguist).

Gera translations/aletheia_pt.ts e translations/aletheia_en.ts a partir dos
chamados TR(ptBr, en) nos ficheiros .cpp/.h. Estes .ts podem depois ser
editados no Qt Linguist e compilados com lrelease para .qm.

Uso: python3 scripts/extract_tr.py [src_dir]
"""
import re
import sys
import os
from xml.sax.saxutils import escape

SRC_DIR = sys.argv[1] if len(sys.argv) > 1 else "src"
OUT_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "translations")

# Captura TR("texto pt", "texto en") tolerando escapes e quebras de linha.
TR_RE = re.compile(
    r'TR\(\s*"((?:[^"\\]|\\.)*)"\s*,\s*"((?:[^"\\]|\\.)*)"\s*\)',
    re.DOTALL)

def unescape(s: str) -> str:
    """Converte escapes C++ numa string real."""
    return (s.replace('\\"', '"').replace("\\n", "\n")
             .replace("\\t", "\t").replace("\\\\", "\\"))

def scan(root: str):
    pairs = []  # (ficheiro, linha, pt, en)
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
                pairs.append((path, line, pt, en))
    return pairs

def write_ts(path: str, lang: str, pairs: list, source_lang: bool):
    """Escreve um .ts. Se source_lang, o <source> fica vazio e o <translation>
    recebe o texto da lingua de origem (para o .ts da propria lingua)."""
    with open(path, "w", encoding="utf-8") as f:
        f.write('<?xml version="1.0" encoding="utf-8"?>\n')
        f.write('<!DOCTYPE TS>\n')
        f.write(f'<TS version="2.1" language="{lang}">\n')
        # Agrupa por ficheiro de origem
        by_file = {}
        for fp, line, pt, en in pairs:
            by_file.setdefault(fp, []).append((line, pt, en))
        for fp in sorted(by_file):
            f.write(f'<context>\n  <name>{escape(fp)}</name>\n')
            for line, pt, en in by_file[fp]:
                src = pt if lang.startswith("pt") else en
                f.write(f'  <message>\n    <location filename="{escape(fp)}" line="{line}"/>\n')
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
    write_ts(os.path.join(OUT_DIR, "aletheia_pt_BR.ts"), "pt_BR", pairs, True)
    write_ts(os.path.join(OUT_DIR, "aletheia_en.ts"), "en", pairs, False)
    print(f"Extraidos {len(pairs)} pares TR() de {SRC_DIR}")
    print(f"  -> {OUT_DIR}/aletheia_pt_BR.ts")
    print(f"  -> {OUT_DIR}/aletheia_en.ts")

if __name__ == "__main__":
    main()
