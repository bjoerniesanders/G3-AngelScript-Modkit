# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 G3ASM authors. Zusatzerlaubnisse: LICENSE-EXCEPTIONS.md
"""Test der Dev-Pipe ohne Spiel: g3asm_test laedt eine Test-Mod und bedient die Pipe (G3ASM_TEST_DEVPIPE), g3asm_dev schickt Anfragen.
Geprueft: Ausgabe von print, Code im Modul einer Mod (deren Globale), Syntaxfehler mit Zeilennummer, Skript-Ausnahme, Abbruch einer
Endlosschleife nach 5 s, unbekannte Mod, Exit-Codes des Clients.
Aufruf: python -I tests/dev_pipe.py <g3asm_test.exe> <SharedBase.dll> <g3asm_dev.exe>"""
import json
import os
import shutil
import subprocess
import sys
import tempfile
import time

exe, sb, client = (os.path.abspath(a) for a in sys.argv[1:4])
bad = 0


def check(cond, msg):
    global bad
    if not cond:
        bad += 1
    print(('OK       ' if cond else 'FEHLER   ') + msg)


def dev(*args):
    r = subprocess.run([client, '-w', '20'] + list(args), capture_output=True, timeout=60)
    text = r.stdout.decode('utf-8', 'replace').strip()
    try:
        data = json.loads(text)
    except ValueError:
        data = {}
    return r.returncode, data, text


tmp = tempfile.mkdtemp(prefix='g3asmdevpipe_')
mods = os.path.join(tmp, 'mods')
os.makedirs(os.path.join(mods, 'pipe_ziel'))
with open(os.path.join(mods, 'pipe_ziel', 'main.as'), 'wb') as f:
    f.write(b'int zaehler = 41;\nvoid main() { print("pipe_ziel geladen"); }\n')

env = dict(os.environ, G3ASM_TEST_DEVPIPE='60000')
srv = subprocess.Popen([exe, sb, mods], env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
try:
    rc, d, raw = dev('print("hallo von der Pipe");')
    check(rc == 0 and d.get('ok') is True and 'hallo von der Pipe' in d.get('out', ''), 'print kommt zurueck: ' + raw[:120])
    check(isinstance(d.get('ms'), (int, float)), 'Antwort enthaelt die Laufzeit (ms)')

    rc, d, raw = dev('-m', 'pipe_ziel', 'zaehler++; print("zaehler " + zaehler);')
    check(rc == 0 and 'zaehler 42' in d.get('out', ''), 'Code im Modul der Mod sieht und aendert ihre Globale: ' + raw[:120])
    rc, d, raw = dev('-m', 'pipe_ziel', 'print("zaehler " + zaehler);')
    check(rc == 0 and 'zaehler 42' in d.get('out', ''), 'Aenderung bleibt in der Mod erhalten')

    rc, d, raw = dev('int a = 1;\nint b = ;')
    check(rc == 1 and d.get('ok') is False and '(2,' in d.get('out', ''), 'Syntaxfehler mit Zeile 2 gemeldet: ' + raw[:160])

    rc, d, raw = dev('array<int> x; x[3] = 1;')
    check(rc == 1 and 'Ausnahme' in d.get('out', ''), 'Skript-Ausnahme gemeldet: ' + raw[:160])

    t0 = time.time()
    rc, d, raw = dev('while (true) {}')
    dt = time.time() - t0
    check(rc == 1 and 'abgebrochen' in d.get('out', '') and 4 < dt < 15, 'Endlosschleife nach 5 s abgebrochen (%.1f s)' % dt)

    rc, d, raw = dev('-m', 'gibt_es_nicht', 'print("x");')
    check(rc == 1 and 'nicht geladen' in d.get('out', ''), 'unbekannte Mod gemeldet')

    rc, d, raw = dev('print("nach den Fehlern");')
    check(rc == 0 and 'nach den Fehlern' in d.get('out', ''), 'Pipe arbeitet nach Fehlern weiter')
finally:
    srv.kill()
    srv.wait()

# ohne Server: Exit-Code 2
r = subprocess.run([client, '-w', '1', 'print("x");'], capture_output=True, timeout=30)
check(r.returncode == 2, 'ohne laufende Pipe: Exit-Code 2')

shutil.rmtree(tmp, ignore_errors=True)
print('Dev-Pipe: %s' % ('alle Pruefungen bestanden' if bad == 0 else '%d Pruefung(en) fehlgeschlagen' % bad))
sys.exit(1 if bad else 0)
