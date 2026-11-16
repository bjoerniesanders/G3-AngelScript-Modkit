# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 G3ASM authors. Zusatzerlaubnisse: LICENSE-EXCEPTIONS.md
"""Test der gepackten Mods (src/packed_mods.cpp) ohne Spiel: Data\\G3ASM.mod/.m00 entpacken, aktuell halten, entfernen. Jeder Lauf des Testprogramms
entspricht einem Spielstart; danach laedt das Testprogramm den Mod-Ordner wie das Spiel (main() jeder Mod laeuft).
Aufruf: python -I tests/packed_mods.py <g3asm_test.exe> <SharedBase.dll> <g3asm_packmod.exe>"""
import os
import shutil
import subprocess
import sys
import tempfile

exe, sb, packmod = (os.path.abspath(a) for a in sys.argv[1:4])
bad = 0


def check(cond, msg):
    global bad
    if not cond:
        bad += 1
    print(('OK       ' if cond else 'FEHLER   ') + msg)


def write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'wb') as f:
        f.write(text.encode('latin-1'))


def read(path):
    with open(path, 'rb') as f:
        return f.read().decode('latin-1')


def mod(root, name, text, extra=None):
    write(os.path.join(root, name, 'main.as'), 'void main() { print("%s"); }\n' % text)
    write(os.path.join(root, name, 'mod.ini'), '[mod]\r\nversion=1.0\r\n')
    for rel, body in (extra or {}).items():
        write(os.path.join(root, name, rel), body)


def pack(src, archive):
    r = subprocess.run([packmod, 'pack', src, archive], capture_output=True, text=True, errors='replace')
    check(r.returncode == 0, 'Archiv gepackt: ' + os.path.basename(archive))


tmp = tempfile.mkdtemp(prefix='g3asmpacked_')
try:
    game, mods = os.path.join(tmp, 'game'), os.path.join(tmp, 'game', 'g3asm', 'mods')
    data = os.path.join(game, 'Data')
    os.makedirs(data)
    # G3ASM.mod: alpha. G3ASM.m00: beta (mit Unterordner), alpha noch einmal (Doppel), lose (Ordner gleichen Namens liegt schon da), Datei ohne Mod-Ordner
    s1, s2 = os.path.join(tmp, 's1'), os.path.join(tmp, 's2')
    mod(s1, 'alpha', 'ALPHA laeuft')
    mod(s2, 'beta', 'BETA v1 laeuft', {'lib\\teil.as': '// alt\n', 'extra.txt': 'nur v1\n'})
    mod(s2, 'alpha', 'ALPHA DOPPEL')
    mod(s2, 'lose', 'LOSE GEPACKT')
    write(os.path.join(s2, 'liesmich.txt'), 'oben\n')
    pack(s1, os.path.join(data, 'G3ASM.mod'))
    pack(s2, os.path.join(data, 'G3ASM.m00'))
    mod(mods, 'lose', 'LOSE ORDNER laeuft')
    # Markierung mit Pfad nach draussen: darf beim Entfernen nichts ausserhalb treffen
    draussen = os.path.join(game, 'g3asm', 'draussen.txt')
    write(draussen, 'bleibt\n')
    write(os.path.join(mods, 'boese', '.g3asm_packed'), 'G3ASM packed 1\r\nsource\tG3ASM.m07\t1\t1\r\nfile\t..\\..\\draussen.txt\r\nfile\tmain.as\r\n')
    write(os.path.join(mods, 'boese', 'main.as'), 'void main() {}\n')

    def start():
        env = dict(os.environ, G3ASM_TEST_PACKED=game + '|' + mods)
        r = subprocess.run([exe, sb, mods], capture_output=True, text=True, env=env, errors='replace')
        return r.returncode, r.stdout

    rc, out = start()
    check('Gepackte Mods bereit: 2' in out, 'Lauf 1: zwei gepackte Mods bereit (alpha, beta)')
    check(os.path.isfile(os.path.join(mods, 'alpha', '.g3asm_packed')) and os.path.isfile(os.path.join(mods, 'beta', 'lib', 'teil.as')),
          'Lauf 1: entpackt mit Markierung und Unterordner')
    check('ALPHA laeuft' in out and 'BETA v1 laeuft' in out, 'Lauf 1: main() beider gepackter Mods laeuft')
    check('ALPHA DOPPEL' not in out and "steht schon in G3ASM.mod" in out, 'Lauf 1: Doppel in G3ASM.m00 ignoriert und gemeldet')
    check(read(os.path.join(mods, 'lose', 'main.as')).find('LOSE ORDNER') >= 0 and 'LOSE ORDNER laeuft' in out and 'LOSE GEPACKT' not in out,
          'Lauf 1: loser Ordner gewinnt und bleibt unangetastet')
    check(not os.path.exists(os.path.join(mods, 'lose', '.g3asm_packed')), 'Lauf 1: loser Ordner bekommt keine Markierung')
    check('ausserhalb eines Mod-Ordners ignoriert' in out and not os.path.exists(os.path.join(mods, 'liesmich.txt')),
          'Lauf 1: Datei auf oberster Ebene des Archivs ignoriert')
    check(os.path.isfile(draussen) and read(draussen) == 'bleibt\n', 'Lauf 1: Markierung mit ".." loescht nichts ausserhalb')
    check(not os.path.exists(os.path.join(mods, 'boese')), "Lauf 1: veraltete markierte Mod 'boese' entfernt")

    rc, out = start()
    check('Gepackte Mods bereit: 2' in out and "entpackt (aus" not in out, 'Lauf 2: unveraendert, nichts neu entpackt')

    # Neue Fassung von beta (andere Groesse): extra.txt faellt weg
    shutil.rmtree(s2)
    mod(s2, 'beta', 'BETA v2 laeuft mit mehr Text', {'lib\\teil.as': '// neu\n'})
    pack(s2, os.path.join(data, 'G3ASM.m00'))
    write(os.path.join(mods, 'beta', 'notiz.txt'), 'vom Spieler\n')
    rc, out = start()
    check("'beta' entpackt (aus G3ASM.m00)" in out and 'BETA v2 laeuft' in out, 'Lauf 3: geaendertes Archiv neu entpackt')
    check(not os.path.exists(os.path.join(mods, 'beta', 'extra.txt')) and read(os.path.join(mods, 'beta', 'lib', 'teil.as')) == '// neu\n',
          'Lauf 3: alte Datei entfernt, neue Fassung geschrieben')
    check(os.path.isfile(os.path.join(mods, 'beta', 'notiz.txt')), 'Lauf 3: fremde Datei im Mod-Ordner bleibt')

    # Mod abgewaehlt: Archiv weg
    os.remove(os.path.join(data, 'G3ASM.m00'))
    rc, out = start()
    check("'beta' entfernt" in out and 'BETA' not in out.replace("'beta'", ''), 'Lauf 4: Mod ohne Archiv entfernt, laeuft nicht mehr')
    check(os.listdir(os.path.join(mods, 'beta')) == ['notiz.txt'], 'Lauf 4: nur die fremde Datei bleibt, Markierung weg')
    check('ALPHA laeuft' in out and 'LOSE ORDNER laeuft' in out, 'Lauf 4: andere Mods laufen weiter')
finally:
    shutil.rmtree(tmp, ignore_errors=True)
print('Ergebnis: %s' % ('alle Pruefungen bestanden' if bad == 0 else '%d Pruefung(en) fehlgeschlagen' % bad))
sys.exit(1 if bad else 0)
