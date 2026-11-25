# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 G3ASM authors. Zusatzerlaubnisse: LICENSE-EXCEPTIONS.md
"""Test der Kommandozeile g3asm.exe ohne Spiel: new, check (mit der Ersatz-SharedBase), pack, release, log, jeweils mit --json.
check findet sbstub\\SharedBase.dll neben dem Build-Ordner g3asm\\ selbst; der Test gibt sie zusaetzlich einmal mit --sharedbase an.
Aufruf: python -I tests/cli.py <g3asm.exe> <SharedBase.dll>"""
import json
import os
import shutil
import subprocess
import sys
import tempfile

exe, sb = (os.path.abspath(a) for a in sys.argv[1:3])
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
bad = 0


def check(cond, msg):
    global bad
    if not cond:
        bad += 1
    print(('OK       ' if cond else 'FEHLER   ') + msg)


def run(*args):
    r = subprocess.run([exe] + list(args), capture_output=True, timeout=120)
    return r.returncode, r.stdout.decode('utf-8', 'replace'), r.stderr.decode('utf-8', 'replace')


def run_json(*args):
    rc, out, err = run('--json', *args)
    try:
        return rc, json.loads(out)
    except ValueError:
        return rc, {'raw': out + err}


tmp = tempfile.mkdtemp(prefix='g3asmcli_')
try:
    mod = os.path.join(tmp, 'mods', 'hallo_cli')
    os.makedirs(os.path.dirname(mod))

    # new
    rc, j = run_json('new', mod)
    ini = open(os.path.join(mod, 'mod.ini'), encoding='utf-8').read() if os.path.isfile(os.path.join(mod, 'mod.ini')) else ''
    check(rc == 0 and j.get('ok') and os.path.isfile(os.path.join(mod, 'main.as')) and 'api=47' in ini,
          'new legt main.as und mod.ini (api=47) an: %s' % j)
    rc, j = run_json('new', mod)
    check(rc == 2 and not j.get('ok') and 'gibt es schon' in j.get('error', ''), 'new: vorhandener Ordner wird nicht ueberschrieben')
    rc, j = run_json('new', os.path.join(tmp, 'mods', 'mit leerzeichen'))
    check(rc == 2 and 'Ungueltiger Mod-Name' in j.get('error', ''), 'new: ungueltiger Mod-Name abgelehnt')

    # check
    rc, j = run_json('check', mod)
    check(rc == 0 and j.get('ok') and j.get('errors') == 0 and j.get('api') == 47 and j.get('mod') == 'hallo_cli',
          'check: neue Mod ohne Fehler: %s' % j)
    rc, out, _ = run('check', mod, '--sharedbase', sb)
    check(rc == 0 and out.startswith('OK: hallo_cli ohne Fehler'), 'check mit --sharedbase, Textausgabe: ' + out.strip())

    with open(os.path.join(mod, 'main.as'), 'wb') as f:
        f.write(b'void main() {\r\n    int x = ;\r\n}\r\n')
    rc, j = run_json('check', mod)
    errs = [m for m in j.get('messages', []) if m.get('type') == 'error']
    check(rc == 1 and not j.get('ok') and errs and errs[0].get('line') == 2 and errs[0].get('col') == 13 and
          errs[0].get('file', '').endswith('main.as'), 'check: Syntaxfehler mit Zeile 2, Spalte 13: %s' % errs[:1])
    rc, out, _ = run('check', os.path.join(mod, 'main.as'))
    check(rc == 1 and '(2, 13): FEHLER:' in out and '2 Fehler' in out, 'check einer Datei, Textausgabe: ' + out.strip()[-80:])

    with open(os.path.join(mod, 'main.as'), 'wb') as f:
        f.write(b'void main() {}\r\n')
    with open(os.path.join(mod, 'mod.ini'), 'wb') as f:
        f.write(b'[mod]\r\napi=999   ; zu neu\r\n')
    rc, j = run_json('check', mod)
    check(rc == 1 and any('api=999' in m.get('text', '') for m in j.get('messages', [])), 'check: mod.ini mit zu neuer api ist ein Fehler')
    with open(os.path.join(mod, 'mod.ini'), 'wb') as f:
        f.write(b'[mod]\r\napi=47\r\ntitle=Hallo CLI\r\n')

    # all example mods compile
    ex = os.path.join(root, 'examples', 'mods')
    fails = []
    for name in sorted(os.listdir(ex)):
        if os.path.isfile(os.path.join(ex, name, 'main.as')):
            rc, j = run_json('check', os.path.join(ex, name))
            if rc != 0:
                fails.append(name)
    check(not fails, 'check: alle Beispiel-Mods ohne Fehler' + (': ' + ', '.join(fails) if fails else ''))

    # pack, release (g3asm_packmod does the work)
    arc = os.path.join(tmp, 'mit leer', 'Test.mod')
    os.makedirs(os.path.dirname(arc))
    rc, j = run_json('pack', mod, arc)
    check(rc == 0 and j.get('ok') and os.path.isfile(arc), 'pack: Archiv geschrieben (Pfad mit Leerzeichen): %s' % j.get('out', j)[:80])
    rc, j = run_json('release', mod, os.path.join(tmp, 'paket'))
    check(rc == 0 and j.get('ok') and os.path.isfile(os.path.join(tmp, 'paket', 'hallo_cli', 'Mod.ini')), 'release: Paket fuer Spieler angelegt')
    rc, j = run_json('release', mod, os.path.join(tmp, 'paket'))
    check(rc != 0 and not j.get('ok'), 'release: vorhandenes Paket wird nicht ueberschrieben (exit %d)' % rc)

    # log
    game = os.path.join(tmp, 'spiel')
    os.makedirs(game)
    with open(os.path.join(game, 'g3asm.log'), 'wb') as f:
        f.write(b'10:00:00.001 AngelScript 2.38.0 bereit\r\n'
                b'10:00:00.002 [hallo] Hallo\r\n'
                b'10:00:00.003 G:/x/main.as (3, 5): FEHLER: No matching symbol\r\n'
                b'10:00:00.004 [hallo] Skript-Ausnahme in void main() (Zeile 4): Null pointer access\r\n'
                b'10:00:00.005 [demo] Koroutine abgebrochen (laenger als 5 s ohne Wait in void Loop() - Endlosschleife?)\r\n')
    rc, j = run_json('log', game)
    lines = j.get('lines', [])
    check(rc == 0 and j.get('count') == 3 and lines[1].get('mod') == 'hallo' and lines[1].get('time') == '10:00:00.004' and
          lines[1].get('text', '').startswith('Skript-Ausnahme'), 'log: nur Fehler, mit Zeit und Mod: %s' % lines[1:2])
    rc, j = run_json('log', game, '-n', '1')
    check(j.get('count') == 1 and 'Koroutine abgebrochen' in j['lines'][0]['text'], 'log -n 1: nur die letzte Zeile')
    rc, out, _ = run('log', game, '--all', '-n', '2')
    check(rc == 0 and out.count('\n') == 2 and 'Skript-Ausnahme' in out, 'log --all: alle Zeilen, Textausgabe')
    rc, j = run_json('log', tmp)
    check(rc == 2 and 'Kein g3asm.log' in j.get('error', ''), 'log: ohne g3asm.log klare Meldung')

    rc, out, err = run()
    check(rc == 2 and 'g3asm check' in err, 'ohne Befehl: Aufruf-Hilfe')
finally:
    shutil.rmtree(tmp, ignore_errors=True)

print('Kommandozeile: %s' % ('alle Pruefungen bestanden' if bad == 0 else '%d Pruefung(en) fehlgeschlagen' % bad))
sys.exit(1 if bad else 0)
