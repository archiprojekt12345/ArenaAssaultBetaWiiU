#!/usr/bin/env python3
from pathlib import Path
import argparse, zipfile

ap = argparse.ArgumentParser(description='Create an extract-to-SD Aroma ZIP')
ap.add_argument('wuhb', type=Path)
ap.add_argument('-o','--output', type=Path, default=Path('ArenaAssault_Aroma_SD.zip'))
args = ap.parse_args()
if not args.wuhb.is_file():
    raise SystemExit(f'WUHB not found: {args.wuhb}')
args.output.parent.mkdir(parents=True, exist_ok=True)
with zipfile.ZipFile(args.output, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as z:
    z.write(args.wuhb, 'wiiu/apps/ArenaAssault/ArenaAssault.wuhb')
    z.writestr('ARENA_ASSAULT_INSTALL.txt',
        'ARENA ASSAULT - AROMA\n\n'
        '1. Wylacz Wii U i wyjmij karte SD.\n'
        '2. Rozpakuj ten ZIP do katalogu glownego karty SD.\n'
        '3. Wloz karte do Wii U i uruchom Aroma.\n'
        '4. Arena Assault pojawi sie na menu Wii U.\n\n'
        'Nie trzeba kopiowac osobnych modeli, tekstur ani glslcompiler.rpl.\n')
print(args.output)
