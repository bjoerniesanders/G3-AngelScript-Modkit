"""Test der Generationsvergabe von og3_packmod: Mod-Archive muessen ohne Luecke nummeriert werden (.mod, .m00, .m01, ...; Alternatives Balancing: .nod, .n00, ...),
weil die Engine an der ersten fehlenden Nummer aufhoert. Ausserdem: ".deleted"-Dateien, Kommentare aus, leere Ordner, ungueltige Volume-Namen.
Aufruf: python -I tests/packmod_generations.py <og3_packmod.exe>"""
import os
import shutil
import subprocess
import sys
import tempfile

exe = sys.argv[1]
bad = 0


def run(*a):
    r = subprocess.run([exe, *a], capture_output=True, text=True)
    return r.returncode, r.stdout


def check(cond, msg):
    global bad
    if not cond:
        bad += 1
    print(('OK       ' if cond else 'FEHLER   ') + msg)


tmp = tempfile.mkdtemp(prefix='og3gen_')
try:
    data = os.path.join(tmp, 'Data')
    src = os.path.join(tmp, 'src', 'G3_World_01')
    os.makedirs(data)
    os.makedirs(src)
    open(os.path.join(src, 'a.quest'), 'wb').write(b'[Quest]\r\nName=A\r\n')
    open(os.path.join(src, 'b.info.deleted'), 'wb').write(b'')
    root = os.path.join(tmp, 'src')

    names = []
    for i in range(4):
        rc, out = run('install', root, data, 'Quests')
        check(rc == 0, 'install #%d' % i)
        names.append(sorted(f for f in os.listdir(data) if f.startswith('Quests.'))[:])
    check(names[3] == ['Quests.m00', 'Quests.m01', 'Quests.m02', 'Quests.mod'], 'Reihenfolge: .mod, .m00, .m01, .m02 (4 Aufrufe -> %s)' % names[3])
    rc, out = run('install', root, data, 'Quests', 'n')
    check(os.path.exists(os.path.join(data, 'Quests.nod')), 'Alternatives Balancing: .nod')
    rc, out = run('install', root, data, 'Quests', 'n')
    check(os.path.exists(os.path.join(data, 'Quests.n00')), 'Alternatives Balancing: .n00')
    # Luecke: .m00 loeschen -> naechste Vergabe fuellt die Luecke (sonst wuerde die Engine .m01 ignorieren)
    os.remove(os.path.join(data, 'Quests.m00'))
    rc, out = run('next', data, 'Quests')
    check(out.strip().lower().endswith('quests.m00'), 'Luecke wird zuerst gefuellt: %s' % out.strip())
    # Inhalt
    rc, out = run('list', os.path.join(data, 'Quests.mod'))
    check('a.quest' in out and '[geloescht] G3_World_01/b.info' in out, 'list: Datei und Loesch-Eintrag')
    d = open(os.path.join(data, 'Quests.mod'), 'rb').read()
    check(src.encode('latin1') not in d and tmp.encode('latin1') not in d, 'keine lokalen Pfade als Kommentar gespeichert')
    check(d[4:8] == b'G3V0' and d[20:24] == b'G3PD', 'Kopf: G3V0 / G3PD')
    # Fehlerfaelle
    check(run('install', os.path.join(tmp, 'gibtsnicht'), data, 'Quests')[0] != 0, 'fehlender Ordner schlaegt fehl')
    empty = os.path.join(tmp, 'leer')
    os.makedirs(empty)
    check(run('install', empty, data, 'Quests')[0] != 0, 'leerer Ordner schlaegt fehl')
    check(run('install', root, data, 'a\\b')[0] != 0 and run('install', root, data, '')[0] != 0, 'ungueltiger Volume-Name wird abgelehnt')
    check(not any(f.endswith('.tmp') for f in os.listdir(data)), 'keine .tmp-Reste')
finally:
    shutil.rmtree(tmp, ignore_errors=True)
print('fehlgeschlagen:', bad)
sys.exit(1 if bad else 0)
