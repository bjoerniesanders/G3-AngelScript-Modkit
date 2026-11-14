# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 G3ASM authors. Zusatzerlaubnisse: LICENSE-EXCEPTIONS.md
"""Test der Datei-Patches (src/file_patches.cpp) ohne Spiel: Overlay-Dateien anlegen und beim naechsten Start wieder entfernen. Geprueft wird vor allem der Umgang
mit einer fremden losen Datei am Zielort: sie wird gesichert, gepatcht und beim naechsten Start zurueckgespielt; eine inzwischen ersetzte Datei bleibt unangetastet.
Jeder Lauf des Testprogramms entspricht einem Spielstart.
Aufruf: python -I tests/file_patches_overlay.py <g3asm_test.exe> <SharedBase.dll>"""
import os
import shutil
import subprocess
import sys
import tempfile

exe, sb = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
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


tmp = tempfile.mkdtemp(prefix='g3asmpatch_')
try:
    game, mods, store = os.path.join(tmp, 'game'), os.path.join(tmp, 'mods'), os.path.join(tmp, 'store')
    script = os.path.join(tmp, 'leer.as')
    write(script, 'void main() {}\n')
    quest = os.path.join(game, 'Data', 'Quests', 'A.quest')
    neu = os.path.join(game, 'Data', 'Quests', 'Neu.quest')
    backup = os.path.join(game, 'g3asm', 'patch_backup', 'Data', 'Quests', 'A.quest')
    manifest = os.path.join(game, 'g3asm', 'patched_files.txt')
    original = 'X=1\r\nY=2\r\nZ=3\r\n'
    write(quest, original)
    os.makedirs(os.path.join(game, 'g3asm'), exist_ok=True)
    os.makedirs(mods, exist_ok=True)
    # Die Mod liegt ausserhalb des Mod-Ordners bereit und wird je Lauf hinein- oder herauskopiert
    mod = os.path.join(store, 'patchtest')
    write(os.path.join(mod, 'main.as'), 'void main() {}\n')
    write(os.path.join(mod, 'ScriptPatches', '01_a.patch'),
          '--- a/Data/Quests/A.quest\n+++ b/Data/Quests/A.quest\n@@ -1,3 +1,3 @@\n-X=1\n+X=5\n Y=2\n Z=3\n'
          '--- /dev/null\n+++ b/Data/Quests/Neu.quest\n@@ -0,0 +1,2 @@\n+N=1\n+M=2\n')
    write(os.path.join(mod, 'ScriptPatches', '02_raus.patch'), '--- /dev/null\n+++ b/../raus.txt\n@@ -0,0 +1,1 @@\n+draussen\n')

    def start(with_mod):
        target = os.path.join(mods, 'patchtest')
        if os.path.isdir(target):
            shutil.rmtree(target)
        if with_mod:
            shutil.copytree(mod, target)
        env = dict(os.environ, G3ASM_TEST_PATCHES=game + '|' + mods)
        r = subprocess.run([exe, sb, script], capture_output=True, text=True, env=env, errors='replace')
        return r.returncode, r.stdout

    rc, out = start(True)
    check(rc < 100 and '2 Datei(en) gepatcht' in out, 'Lauf 1: zwei Dateien gepatcht (Rueckgabe %d)' % rc)
    check(read(quest) == 'X=5\r\nY=2\r\nZ=3\r\n', 'Lauf 1: fremde lose Datei ist gepatcht, Zeilenenden bleiben')
    check(os.path.isfile(backup) and read(backup) == original, 'Lauf 1: fremde Datei liegt unveraendert in g3asm\\patch_backup')
    check(os.path.isfile(neu) and read(neu).split() == ['N=1', 'M=2'], 'Lauf 1: neue Datei angelegt')
    check(not os.path.exists(os.path.join(tmp, 'raus.txt')) and not os.path.exists(os.path.join(game, '..', 'raus.txt')),
          'Lauf 1: Zielpfad mit ".." wird abgelehnt')
    lines = read(manifest).split('\n') if os.path.isfile(manifest) else []
    check(len([x for x in lines if x.count('\t') == 2]) == 2, 'Lauf 1: Liste merkt je Datei Groesse und CRC32')

    rc, out = start(True)
    check(rc < 100 and read(quest) == 'X=5\r\nY=2\r\nZ=3\r\n' and read(backup) == original,
          'Lauf 2: zurueckgespielt und neu gepatcht (nicht doppelt), Sicherung bleibt das Original')

    rc, out = start(False)
    check(rc < 100 and read(quest) == original, 'Lauf 3 ohne Mod: fremde Datei ist wieder im Originalzustand')
    check(not os.path.exists(neu) and not os.path.exists(backup) and not os.path.exists(manifest), 'Lauf 3 ohne Mod: neue Datei, Sicherung und Liste sind weg')

    start(True)
    ersetzt = 'X=1\r\nY=9\r\nZ=3\r\n'
    write(quest, ersetzt)   # jemand ersetzt die gepatchte Datei (z. B. Update der anderen Mod)
    rc, out = start(False)
    check(rc < 100 and read(quest) == ersetzt, 'Lauf 5: inzwischen ersetzte Datei bleibt unangetastet')
    check(not os.path.exists(backup), 'Lauf 5: die veraltete Sicherung ist entfernt')

    alt = os.path.join(game, 'Data', 'Quests', 'Alt.quest')
    write(alt, 'alt\r\n')
    write(manifest, 'Data/Quests/Alt.quest\n..\\draussen.txt\n')   # Liste im frueheren Format (nur Pfade) und ein Pfad, der hinausfuehrt
    write(os.path.join(tmp, 'draussen.txt'), 'bleibt\n')
    rc, out = start(False)
    check(rc < 100 and not os.path.exists(alt), 'Lauf 6: Liste im frueheren Format wird weiter abgearbeitet')
    check(os.path.isfile(os.path.join(tmp, 'draussen.txt')), 'Lauf 6: Pfad mit ".." in der Liste wird nicht geloescht')
finally:
    shutil.rmtree(tmp, ignore_errors=True)

print('alle Pruefungen bestanden' if bad == 0 else '%d Pruefung(en) fehlgeschlagen' % bad)
sys.exit(1 if bad else 0)
