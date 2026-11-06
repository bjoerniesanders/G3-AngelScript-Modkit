"""Liest G3V0-Archive (Gothic-3-.pak/.cpt/.p00/.m00/.mod): Verzeichnis anzeigen, Dateien ansehen und entpacken, auch komprimierte (zlib).

Aufruf (nur Lesen; mit  python -I  starten):
  python -I tools/g3pak_read.py list    <Archiv> [Teilname]            Dateien (mit Teilname im Pfad) auflisten
  python -I tools/g3pak_read.py show    <Archiv> <Teilname> [Zeichen]   lesbare Zeichen der Datei(en) ausgeben (nicht lesbare Bytes als '.')
  python -I tools/g3pak_read.py extract <Archiv> <Teilname> <Ordner>    Datei(en) in den Ordner schreiben (Pfade aus dem Archiv, unterhalb des Ordners)

Das Format ist das, das og3_packmod schreibt (siehe src/pack_mod.cpp): Kopf mit Verzeichnisversatz, Verzeichnis am Ende, je Datei Versatz, gepackte und
echte Groesse, Komprimierung (0 gespeichert, sonst zlib). Dieses Werkzeug schliesst die Luecke, dass og3_packmod komprimierte Eintraege nur auflistet.
Vorlagen (.tple) sind ein Eigenschaftsstrom (GENOMFLE/GENOMETP), kein Text; mit 'show' sieht man die Namen der Eigenschaften, die Werte stehen binaer.
"""
import os
import re
import struct
import sys
import zlib


class _Reader:
    def __init__(self, data):
        self.m = data
        self.pos = 0

    def u32(self):
        v = struct.unpack_from('<I', self.m, self.pos)[0]
        self.pos += 4
        return v

    def u64(self):
        v = struct.unpack_from('<Q', self.m, self.pos)[0]
        self.pos += 8
        return v

    def text(self):
        n = self.u32()
        t = self.m[self.pos:self.pos + n].decode('latin1')
        self.pos += n + (1 if n else 0)
        return t


def read_directory(path):
    """Liefert (Archivbytes, [(Name, Versatz, gepackte Groesse, Groesse, Komprimierung, geloescht)])."""
    with open(path, 'rb') as f:
        d = f.read()
    h = struct.unpack_from('<12I', d, 0)
    if h[1] != 0x30563347:
        raise SystemExit('kein G3V0-Archiv: ' + path)
    meta_off = h[6] | (h[7] << 32)
    r = _Reader(d[meta_off:])
    out = []

    def rec(depth):
        if depth > 64:
            raise SystemExit('Verzeichnis beschaedigt (zu tief)')
        head = struct.unpack_from('<9I', r.m, r.pos)
        r.pos += 0x24
        attr = head[8]
        if not (attr & 0x10):
            off, stored, size = r.u64(), r.u64(), r.u64()
            r.u32()
            comp = r.u32()
            name = r.text()
            r.text()
            out.append((name, off, stored, size, comp, bool(attr & 0x8000)))
            return
        r.text()
        for _ in range(2):
            for _ in range(r.u32()):
                rec(depth + 1)

    rec(0)
    return d, out


def file_data(d, entry):
    name, off, stored, size, comp, deleted = entry
    raw = d[off:off + stored]
    if not comp:
        return raw
    try:
        return zlib.decompress(raw)
    except zlib.error:
        return zlib.decompress(raw, -15)


def main(argv):
    if len(argv) < 3 or argv[1] not in ('list', 'show', 'extract'):
        sys.stdout.write(__doc__)
        return 2
    cmd, path = argv[1], argv[2]
    part = argv[3].lower() if len(argv) > 3 else ''
    d, entries = read_directory(path)
    hits = [e for e in entries if part in e[0].lower() and not e[5]]
    if cmd == 'list':
        for e in hits:
            print('%10d %10d %s  %s' % (e[3] or e[2], e[2], 'zlib' if e[4] else '    ', e[0]))
        print('%d von %d Dateien' % (len(hits), len(entries)))
        return 0
    if not part:
        print('Teilname fehlt')
        return 2
    for e in hits:
        data = file_data(d, e)
        if cmd == 'show':
            limit = int(argv[4]) if len(argv) > 4 else 3000
            print('###', e[0], '(%d Bytes)' % len(data))
            print(re.sub(rb'[^\x20-\x7e]', b'.', data[:limit]).decode())
        else:
            out_dir = os.path.abspath(argv[4])
            target = os.path.abspath(os.path.join(out_dir, *e[0].split('/')))
            if os.path.commonpath([out_dir, target]) != out_dir:   # kein Ausbrechen aus dem Zielordner
                print('uebersprungen (Pfad ausserhalb):', e[0])
                continue
            os.makedirs(os.path.dirname(target), exist_ok=True)
            with open(target, 'wb') as f:
                f.write(data)
            print('geschrieben:', target, '(%d Bytes)' % len(data))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
