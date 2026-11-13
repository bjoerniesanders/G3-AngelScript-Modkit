# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 G3ASM authors. Zusatzerlaubnisse: LICENSE-EXCEPTIONS.md
"""Erzeugt Abschnitt 8 von docs/modkit/reference.md aus dem Protokoll der Mod reference_dump.

Aufruf:  python -I tools/reference_from_log.py [Protokoll] [reference.md]
  Protokoll   Standard G:\\Gothic3\\g3asm.log (der letzte Dump darin zaehlt)
  reference.md  Standard docs/modkit/reference.md; der Text zwischen <!-- DUMP-ANFANG --> und <!-- DUMP-ENDE --> wird ersetzt.
Ohne zweites Argument wird nur auf die Standardausgabe geschrieben, wenn die Marker fehlen.
"""
import io
import os
import re
import sys

log_path = sys.argv[1] if len(sys.argv) > 1 else r'G:\Gothic3\g3asm.log'
md_path = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'docs', 'modkit', 'reference.md')

PREFIX = re.compile(r'^\d\d:\d\d:\d\d\.\d+ \[reference_dump\] ')
lines = io.open(log_path, encoding='cp1252', errors='replace').read().splitlines()

starts_dump = [i for i, l in enumerate(lines) if '=== Referenz-Dump' in l]
if not starts_dump:
    sys.exit('Kein Referenz-Dump im Protokoll %s (das Protokoll gilt nur fuer den letzten Spielstart). Zuerst im Spiel den Ziffernblock / druecken.' % log_path)
start = max(starts_dump)
end = next(i for i in range(start, len(lines)) if '=== Ende des Dumps ===' in lines[i])
block = [PREFIX.sub('', l) for l in lines[start + 1:end]]

# --- Attribute: Kennungen aus Quest-Belohnungen und Werte des Helden
quest_tags = []
hero = []
for l in block:
    if l.startswith('Quests mit Attribut-Belohnung'):
        for m in re.finditer(r'([A-Z]{2,4}) \((\d+)x\), z\. B\. (\S+) \(\+?(-?\d+)\)', l.replace('\\n', '\n')):
            quest_tags.append(m.groups())
    m = re.match(r'\s+Held (\w+): Wert (-?\d+), Grundwert (-?\d+), Maximum (-?\d+)', l)
    if m:
        hero.append(m.groups())

# --- Vorlagen-Gruppen
groups = []   # (teil, anzahl_eintraege, gekuerzt, namen)
cur = None
for l in block:
    m = re.match(r"Vorlagen mit '(.+)': (\d+) Eintraege( \(nur der Anfang\))?", l)
    if m:
        cur = [m.group(1), int(m.group(2)), bool(m.group(3)), []]
        groups.append(cur)
        continue
    m = re.match(r"Vorlagen mit '(.+)': \(keine\)", l)
    if m:
        groups.append([m.group(1), 0, False, []])
        cur = None
        continue
    if cur is not None and l.startswith('  '):
        cur[3].extend(n for n in l.strip().split(';') if n)
    elif not l.startswith('  '):
        cur = None

out = []
out.append('Quelle: Dump der Mod `reference_dump` aus dem laufenden Spiel, erzeugt mit `tools/reference_from_log.py`. `FindTemplateNames` liefert die meisten Namen mehrfach; unten stehen eindeutige Namen.')
out.append('')
out.append('### Attribute')
out.append('')
if quest_tags:
    out.append('Kennungen, die Quests des Spiels als Belohnung vergeben (`DescribeQuestRewards`):')
    out.append('')
    out.append('| Kennung | Quests | Beispiel |')
    out.append('|---|---|---|')
    for tag, n, quest, val in quest_tags:
        out.append('| `%s` | %s | `%s` (%+d) |' % (tag, n, quest, int(val)))
    out.append('')
if hero:
    out.append('Werte des Helden zum Zeitpunkt des Dumps (Wert / Grundwert / Maximum). Das ist **ein Spielstand**, kein Standardwert:')
    out.append('')
    out.append('| Kennung | Wert | Grundwert | Maximum |')
    out.append('|---|---|---|---|')
    for tag, v, b, mx in hero:
        out.append('| `%s` | %s | %s | %s |' % (tag, v, b, mx))
    out.append('')
out.append('### Vorlagen nach Namensgruppen')
out.append('')
out.append('| Namensteil | eindeutige Namen | Hinweis |')
out.append('|---|---|---|')
for teil, n, gek, namen in groups:
    uniq = sorted(set(namen))
    out.append('| `%s` | %d | %s |' % (teil, len(uniq), 'nur der Anfang der Liste' if gek else ('keine Vorlage' if n == 0 else '')))
out.append('')
for teil, n, gek, namen in groups:
    uniq = sorted(set(namen))
    if not uniq:
        continue
    out.append('**`%s`** (%d%s)' % (teil, len(uniq), ', gekürzt' if gek else ''))
    out.append('')
    out.append(', '.join('`%s`' % u for u in uniq))
    out.append('')

# --- Gesamtliste aller Vorlagen (zweiter Dump der Mod, Rollen-Taste): docs/modkit/data/templates.txt, eindeutig und sortiert
starts = [i for i, l in enumerate(lines) if '=== Alle Vorlagen ===' in l]
if starts:
    st = starts[-1]
    en = next((i for i in range(st, len(lines)) if '=== Ende der Vorlagen ===' in lines[i]), None)
    if en is not None:
        names = set()
        for l in lines[st + 1:en]:
            l = PREFIX.sub('', l)
            if l.startswith('  '):
                names.update(n for n in l.strip().split(';') if n)
        if len(names) < 1000:
            print('Hinweis: nur %d Vorlagennamen im zweiten Dump (alte Mod-Fassung oder zu kleine Obergrenze?) - data/templates.txt wird nicht geschrieben' % len(names))
            names = None
    if en is not None and names is not None:
        data_dir = os.path.join(os.path.dirname(os.path.abspath(md_path)), 'data')
        os.makedirs(data_dir, exist_ok=True)
        data_path = os.path.join(data_dir, 'templates.txt')
        io.open(data_path, 'w', encoding='utf-8', newline='\n').write('\n'.join(sorted(names)) + '\n')
        print('geschrieben:', os.path.normpath(data_path), '(%d eindeutige Vorlagennamen)' % len(names))
        out.append('Die vollständige Liste aller Vorlagen (%d eindeutige Namen) steht in [`data/templates.txt`](data/templates.txt).' % len(names))
        out.append('')

text = '\n'.join(out).rstrip() + '\n'
A, E = '<!-- DUMP-ANFANG -->', '<!-- DUMP-ENDE -->'
if os.path.exists(md_path):
    s = io.open(md_path, encoding='utf-8', newline='').read()
    nl = '\r\n' if '\r\n' in s else '\n'
    s = s.replace('\r\n', '\n')
    if A in s and E in s:
        a = s.index(A) + len(A)
        b = s.index(E)
        s = s[:a] + '\n' + text + s[b:]
        io.open(md_path, 'w', encoding='utf-8', newline='').write(s.replace('\n', nl))
        print('geschrieben:', os.path.normpath(md_path), '(%d Gruppen, %d Quest-Kennungen, %d Heldenwerte)' % (len(groups), len(quest_tags), len(hero)))
        sys.exit(0)
sys.stdout.write(text)
