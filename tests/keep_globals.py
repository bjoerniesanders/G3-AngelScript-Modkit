# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 G3ASM authors. Zusatzerlaubnisse: LICENSE-EXCEPTIONS.md
"""Test des Hot-Reloads ohne Spiel: g3asm_test bedient die Dev-Pipe (G3ASM_TEST_DEVPIPE) und prueft wie im Spiel einmal je Sekunde auf
geaenderte Mods. Der Test aendert main.as und fragt die Globalen ueber g3asm_dev ab.
Geprueft: Globale behalten ihren Wert, main() laeuft danach erneut, ein geaenderter const-Wert gilt, die Werte ueberstehen einen Lauf mit
Syntaxfehler, mod.ini keepglobals=0 schaltet es ab.
Aufruf: python -I tests/keep_globals.py <g3asm_test.exe> <SharedBase.dll> <g3asm_dev.exe>"""
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
    return r.returncode, data.get('out', ''), text


def write(path, text):
    with open(path, 'wb') as f:
        f.write(text.encode('ascii'))
    # the reload compares the file time: make sure that it changes, also on file systems with a coarse time
    t = time.time() + 2
    os.utime(path, (t, t))


def wait_reload():
    time.sleep(2.5)   # CheckReload looks 1 time each second


SCRIPT = '''const int VERSION = %d;
int zaehler = 0;
int mainRuns = 0;
bCString text;
dictionary werte;
void main() { mainRuns++; }
'''

tmp = tempfile.mkdtemp(prefix='g3asmkeep_')
mods = os.path.join(tmp, 'mods')
ziel = os.path.join(mods, 'keep_ziel')
aus = os.path.join(mods, 'keep_aus')
os.makedirs(ziel)
os.makedirs(aus)
write(os.path.join(ziel, 'main.as'), SCRIPT % 1)
write(os.path.join(aus, 'main.as'), 'int zaehler = 0;\n')
with open(os.path.join(aus, 'mod.ini'), 'w') as f:
    f.write('[mod]\nkeepglobals=0\n')

env = dict(os.environ, G3ASM_TEST_DEVPIPE='60000')
srv = subprocess.Popen([exe, sb, mods], env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
try:
    rc, out, raw = dev('-m', 'keep_ziel', 'zaehler = 41; text = "Myrtana"; werte.set("gold", 100); print("gesetzt " + mainRuns);')
    check(rc == 0 and 'gesetzt 1' in out, 'Werte gesetzt, main() lief einmal: ' + raw[:120])
    dev('-m', 'keep_aus', 'zaehler = 5;')

    write(os.path.join(ziel, 'main.as'), SCRIPT % 2)
    write(os.path.join(aus, 'main.as'), 'int zaehler = 0;\nvoid main() {}\n')
    wait_reload()
    rc, out, raw = dev('-m', 'keep_ziel', 'int g; if (!werte.get("gold", g)) g = -1; '
                       'print("z " + zaehler + " t " + text + " g " + g + " v " + VERSION + " m " + mainRuns);')
    check(rc == 0 and 'z 41 t Myrtana g 100' in out, 'Globale nach dem Hot-Reload behalten: ' + raw[:160])
    check('v 2' in out, 'geaenderter const-Wert gilt: ' + out.strip())
    check('m 2' in out, 'main() lief nach dem Neuladen erneut (Zaehler behalten): ' + out.strip())
    rc, out, raw = dev('-m', 'keep_aus', 'print("z " + zaehler);')
    check(rc == 0 and 'z 0' in out, 'keepglobals=0: Globale starten neu: ' + raw[:120])

    write(os.path.join(ziel, 'main.as'), 'int zaehler = ;\n')   # syntax error: the mod stays unloaded
    wait_reload()
    rc, out, raw = dev('-m', 'keep_ziel', 'print("x");')
    check(rc == 1 and 'nicht geladen' in out, 'Mod mit Syntaxfehler nicht geladen')
    write(os.path.join(ziel, 'main.as'), SCRIPT % 3)
    wait_reload()
    rc, out, raw = dev('-m', 'keep_ziel', 'print("z " + zaehler + " v " + VERSION);')
    check(rc == 0 and 'z 41 v 3' in out, 'Werte ueberstehen einen Lauf mit Syntaxfehler: ' + raw[:120])
finally:
    srv.kill()
    log = srv.communicate()[0].decode('utf-8', 'replace')
check('Hot-Reload: ' in log and 'globale Variablen uebernommen' in log, 'Protokoll meldet die uebernommenen Variablen')

shutil.rmtree(tmp, ignore_errors=True)
print('Hot-Reload: %s' % ('alle Pruefungen bestanden' if bad == 0 else '%d Pruefung(en) fehlgeschlagen' % bad))
sys.exit(1 if bad else 0)
