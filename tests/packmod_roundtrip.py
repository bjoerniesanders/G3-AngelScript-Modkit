"""Rundlauf-Test fuer g3asm_packmod: echte Gothic-3-Archive (z. B. Community-.mod) mit einem unabhaengigen Python-Leser entpacken, mit g3asm_packmod neu packen und
Namen, Inhalte, geloeschte Eintraege und Kopf vergleichen.
Aufruf: python -I tests/packmod_roundtrip.py <g3asm_packmod.exe> <Archiv> [<Archiv> ...]
Rueckgabe 0 = alle Archive stimmen ueberein."""
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib


def parse(path):
    """Liest ein G3V0-Archiv: Kopf, Eintraege (Name, Attribute, Inhalt)."""
    data = open(path, 'rb').read()
    h = struct.unpack_from('<12I', data, 0)
    assert h[1] == 0x30563347, 'kein G3V0'
    moff = h[6] | (h[7] << 32)
    pos = [moff]

    def u32():
        v = struct.unpack_from('<I', data, pos[0])[0]
        pos[0] += 4
        return v

    def u64():
        v = struct.unpack_from('<Q', data, pos[0])[0]
        pos[0] += 8
        return v

    def s():
        n = u32()
        r = data[pos[0]:pos[0] + n].decode('latin1')
        pos[0] += n + (1 if n else 0)
        return r

    dirs, files = [], {}

    def rec():
        hdr = struct.unpack_from('<9I', data, pos[0])
        pos[0] += 0x24
        attr = hdr[8]
        if attr & 0x10:
            name = s()
            dirs.append(name)
            for _ in range(2):
                for _ in range(u32()):
                    rec()
            return
        off, a, b = u64(), u64(), u64()
        u32()
        comp = u32()
        name = s()
        comment = s()
        if attr & 0x8000:
            files[name] = {'deleted': True, 'attr': attr, 'data': b'', 'comment': comment}
            return
        raw = data[off:off + a]
        content = zlib.decompress(raw) if comp == 2 else raw
        files[name] = {'deleted': False, 'attr': attr, 'data': content, 'comment': comment, 'time': hdr[0:6]}

    rec()
    return h, len(data), dirs, files


def main():
    exe = sys.argv[1]
    bad = 0
    for arc in sys.argv[2:]:
        h0, n0, dirs0, files0 = parse(arc)
        tmp = tempfile.mkdtemp(prefix='g3asmpack_')
        try:
            src = os.path.join(tmp, 'src')
            for name, f in files0.items():
                p = os.path.join(src, name.replace('/', os.sep) + ('.deleted' if f['deleted'] else ''))
                os.makedirs(os.path.dirname(p), exist_ok=True)
                open(p, 'wb').write(f['data'])
            out = os.path.join(tmp, 'neu.mod')
            r = subprocess.run([exe, 'pack', src, out], capture_output=True, text=True)
            if r.returncode != 0:
                print('FEHLER packen:', arc, r.stdout, r.stderr)
                bad += 1
                continue
            h1, n1, dirs1, files1 = parse(out)
            problems = []
            if h1[1] != 0x30563347 or h1[5] != 0x44503347:
                problems.append('Kopf: Produkt/Reserved %x %x' % (h1[1], h1[5]))
            if (h1[6] | h1[7] << 32) != (h1[8] | h1[9] << 32) or (h1[10] | h1[11] << 32) != n1 - 4:
                problems.append('Kopf: Offsets %s, Groesse %d' % ([hex(x) for x in h1[6:12]], n1))
            if set(files0) != set(files1):
                problems.append('Dateinamen: nur alt %s, nur neu %s' % (sorted(set(files0) - set(files1))[:3], sorted(set(files1) - set(files0))[:3]))
            if sorted(dirs0, key=str.lower) != sorted(dirs1, key=str.lower):
                problems.append('Verzeichnisse: alt %d neu %d (%s | %s)' % (len(dirs0), len(dirs1), sorted(set(dirs0) - set(dirs1))[:3], sorted(set(dirs1) - set(dirs0))[:3]))
            for name in set(files0) & set(files1):
                a, b = files0[name], files1[name]
                if a['deleted'] != b['deleted'] or a['data'] != b['data']:
                    problems.append('Inhalt/geloescht: ' + name)
                    break
                # Attribute: "Compressed" (0x800) darf abweichen, alles andere nicht
                if (a['attr'] & ~0x800) != (b['attr'] & ~0x800):
                    problems.append('Attribute %s: %x -> %x' % (name, a['attr'], b['attr']))
                    break
            comp = sum(1 for f in files0.values() if f['attr'] & 0x800)
            deleted = sum(1 for f in files0.values() if f['deleted'])
            print('%-60s %6d Dateien (%d gepackt, %d geloescht) %9d -> %9d Byte  %s' % (
                os.path.basename(arc), len(files0), comp, deleted, n0, n1, 'OK' if not problems else 'ABWEICHUNG'))
            for p in problems:
                print('    ', p)
            bad += bool(problems)
        finally:
            shutil.rmtree(tmp, ignore_errors=True)
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
