# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 G3ASM authors. Zusatzerlaubnisse: LICENSE-EXCEPTIONS.md
"""Test von 'g3asm_packmod release' von Anfang bis Ende, ohne Spiel: Mod-Ordner -> Paket im Union-Starter-Aufbau -> eingespielt wie der Starter
(ScriptFiles\\* in den Spielordner, ModFiles\\*.m0x als naechste Generation nach Data\\) -> das Modkit entpackt die Mod und ihr main() laeuft.
Aufruf: python -I tests/packmod_release.py <g3asm_test.exe> <SharedBase.dll> <g3asm_packmod.exe>
Die Laufzeit Script_G3ASM_<API>.dll muss neben g3asm_packmod.exe liegen (Build-Ziel g3asm_script)."""
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


def pm(*args):
    r = subprocess.run([packmod] + list(args), capture_output=True, text=True, errors='replace')
    return r.returncode, r.stdout


tmp = tempfile.mkdtemp(prefix='g3asmrelease_')
try:
    src = os.path.join(tmp, 'src', 'meine_mod')
    write(os.path.join(src, 'main.as'), '#include "lib/teil.as"\nvoid main() { print("MEINE_MOD laeuft " + Teil()); }\n')
    write(os.path.join(src, 'lib', 'teil.as'), 'bCString Teil() { return "mit Include"; }\n')
    write(os.path.join(src, 'mod.ini'), '[mod]\r\nversion=1.2\r\ntitle=Meine Mod\r\ncategory=Testkategorie\r\nenabled=0\r\n')
    abh = os.path.join(tmp, 'src', 'abh')   # nur fuer Required= in Mod.ini
    write(os.path.join(abh, 'main.as'), 'void main() {}\n')
    write(os.path.join(abh, 'mod.ini'), '\xef\xbb\xbf[mod]\r\nrequires=basis >= 1.0, extra\r\n')
    write(os.path.join(src, 'ScriptPatches', 'a.patch'), '--- /dev/null\n+++ b/Data/Quests/X.quest\n@@ -0,0 +1 @@\n+X\n')
    write(os.path.join(src, 'Source', 'Quests', 'G3_World_01', 'Meine.quest'), 'XP=1\r\n')
    write(os.path.join(src, '.git', 'HEAD'), 'ref\n')
    os.makedirs(os.path.join(src, 'ModFiles'))
    rc, out = pm('pack', os.path.join(src, 'Source', 'Quests'), os.path.join(src, 'ModFiles', 'Quests.m0x'))
    check(rc == 0, 'Vorbereitung: Spieldaten-Archiv ModFiles\\Quests.m0x gepackt')

    dist = os.path.join(tmp, 'dist')
    os.makedirs(dist)
    rc, out = pm('release', src, dist)
    pkg = os.path.join(dist, 'meine_mod')
    check(rc == 0, 'release: Rueckgabe 0')
    rt = [f for f in os.listdir(os.path.join(pkg, 'ScriptFiles', 'scripts'))] if os.path.isdir(os.path.join(pkg, 'ScriptFiles', 'scripts')) else []
    check(len(rt) == 1 and rt[0].lower().startswith('script_g3asm_'), 'Paket: genau eine Laufzeit in ScriptFiles\\scripts (%s)' % rt)
    check(sorted(os.listdir(os.path.join(pkg, 'ModFiles'))) == ['G3ASM.m0x', 'Quests.m0x'], 'Paket: ModFiles enthaelt Skript-Archiv und Spieldaten-Archiv')
    ini = read(os.path.join(pkg, 'Mod.ini'))
    check(ini == '[Main]\r\nName=Meine Mod\r\nCategory=Testkategorie\r\n', 'Paket: Mod.ini mit Name und Category aus title/category')
    lic = os.path.join(pkg, 'G3ASM-LICENSE.txt')
    lic_text = read(lic) if os.path.isfile(lic) else ''
    check(bool(rt) and rt[0] in lic_text and 'GNU General Public License' in lic_text and 'https://github.com/' in lic_text,
          'Paket: G3ASM-LICENSE.txt nennt Laufzeit, GPL und Quellcode-Adresse')
    rc, o = pm('release', abh, dist)
    check(rc == 0 and read(os.path.join(dist, 'abh', 'Mod.ini')) == '[Main]\r\nName=abh\r\nCategory=G3ASM\r\nRequired=basis;extra\r\n',
          'Paket: ohne title/category Ordnername und G3ASM; requires wird Required (mod.ini mit BOM)')
    rc, lst = pm('list', os.path.join(pkg, 'ModFiles', 'G3ASM.m0x'))
    check('meine_mod/main.as' in lst and 'meine_mod/lib/teil.as' in lst and 'meine_mod/mod.ini' in lst, 'Skript-Archiv: main.as, Include, mod.ini unter meine_mod/')
    check('ScriptPatches' not in lst and 'Source' not in lst and '.git' not in lst and 'Quests.m0x' not in lst,
          'Skript-Archiv: ohne ScriptPatches, Source, .git, eigene Archive')
    check('Datei-Patches wirken nur mit dem Loader' in out, 'release: Hinweis zu Datei-Patches')
    rc, out2 = pm('release', src, dist)
    check(rc != 0 and 'existiert schon' in out2, 'release: vorhandenes Ziel wird nicht ueberschrieben')

    # Einspielen wie der Union Starter, dann ein "Spielstart"
    game = os.path.join(tmp, 'game')
    data = os.path.join(game, 'Data')
    os.makedirs(data)
    shutil.copytree(os.path.join(pkg, 'ScriptFiles'), game, dirs_exist_ok=True)
    for f in ('G3ASM.m0x', 'Quests.m0x'):
        rc, o = pm('copy', os.path.join(pkg, 'ModFiles', f), game, f.split('.')[0])
        check(rc == 0 and ('%s.mod' % f.split('.')[0]) in o, 'Einspielen: %s -> Data\\%s.mod' % (f, f.split('.')[0]))
    mods = os.path.join(game, 'g3asm', 'mods')
    env = dict(os.environ, G3ASM_TEST_PACKED=game + '|' + mods)
    r = subprocess.run([exe, sb, mods], capture_output=True, text=True, env=env, errors='replace')
    check("'meine_mod' entpackt (aus G3ASM.mod)" in r.stdout, 'Start: Mod aus Data\\G3ASM.mod entpackt')
    check('enabled=1' in read(os.path.join(mods, 'meine_mod', 'mod.ini')), 'Start: entpackte mod.ini hat enabled=1')
    check('MEINE_MOD laeuft mit Include' in r.stdout, 'Start: main() der Mod laeuft, #include aufgeloest')
    if bad:
        print(r.stdout)
finally:
    shutil.rmtree(tmp, ignore_errors=True)
print('Ergebnis: %s' % ('alle Pruefungen bestanden' if bad == 0 else '%d Pruefung(en) fehlgeschlagen' % bad))
sys.exit(1 if bad else 0)
