#!/usr/bin/env python3
"""Which creature displays have every animation a player needs? Reads CreatureDisplayInfo / CreatureModelData and
the M2 models from the client's MPQs (highest-priority archive wins) and checks each model's animation list.
Writes ~/tc/costumes/anims.json: {display: {"model": path, "missing": [names], "anims": n}}."""
import ctypes, os, struct, json, sys, re
sys.path.insert(0, os.path.expanduser('~/tc'))
import dbctool as T
DATA = os.path.expanduser('~/mnt/ChromieCraft_3.3.5a/Data')
L = ctypes.CDLL(os.path.expanduser('~/StormLib/b/libstorm.so')); v = ctypes.c_void_p
L.SFileOpenArchive.argtypes = [ctypes.c_char_p, ctypes.c_uint, ctypes.c_uint, ctypes.POINTER(v)]
L.SFileHasFile.argtypes = [v, ctypes.c_char_p]
L.SFileOpenFileEx.argtypes = [v, ctypes.c_char_p, ctypes.c_uint, ctypes.POINTER(v)]
L.SFileGetFileSize.argtypes = [v, ctypes.POINTER(ctypes.c_uint)]; L.SFileGetFileSize.restype = ctypes.c_uint
L.SFileReadFile.argtypes = [v, ctypes.c_void_p, ctypes.c_uint, ctypes.POINTER(ctypes.c_uint), ctypes.c_void_p]
L.SFileCloseFile.argtypes = [v]

# the animations a player body uses (AnimationData ids), as in the Model Animations check
NEEDED = {
    'Walk': 4, 'Run': 5, 'Walk backwards': 13, 'Strafe left': 11, 'Strafe right': 12, 'Sprint': 143,
    'Jump start': 37, 'Jump (in the air)': 38, 'Jump landing': 39, 'Landing while running': 187, 'Fall': 40,
    'Swim idle': 41, 'Swim forward': 42, 'Swim left': 43, 'Swim right': 44, 'Swim backwards': 45,
    'Combat ready': 25, 'Attack unarmed': 16, 'Attack one-handed': 17, 'Attack two-handed': 18, 'Getting hit': 9,
    'Stun': 14, 'Death': 1,
    'Ready spell (directed)': 51, 'Ready spell (omni)': 52, 'Cast (directed)': 53, 'Cast (omni)': 54, 'Channel': 76,
    'Sit': 97, 'Riding a mount': 91, 'Loot': 50, 'Sleep': 100, 'Kneel': 114,
}

def order():
    letters = [chr(c) for c in range(ord('2'), ord('9') + 1)] + [chr(c) for c in range(ord('A'), ord('Z') + 1)]
    names = ['common.MPQ', 'common-2.MPQ', 'expansion.MPQ', 'lichking.MPQ', 'enUS/locale-enUS.MPQ', 'enUS/expansion-locale-enUS.MPQ',
             'enUS/lichking-locale-enUS.MPQ', 'patch.MPQ', 'enUS/patch-enUS.MPQ']
    for x in letters:
        names += [f'patch-{x}.MPQ', f'enUS/patch-enUS-{x}.MPQ', f'enUS/patch-enUS-{x.lower()}.MPQ']
    out = []
    for n in names:
        p = os.path.join(DATA, n)
        if os.path.exists(p):
            h = v()
            if L.SFileOpenArchive(p.encode(), 0, 0x100, ctypes.byref(h)):
                out.append((n, h))
    return out[::-1]          # highest priority first

ARCH = order()

def read(name):
    for n, h in ARCH:
        if L.SFileHasFile(h, name.encode()):
            f = v()
            if not L.SFileOpenFileEx(h, name.encode(), 0, ctypes.byref(f)):
                continue
            size = L.SFileGetFileSize(f, None)
            buf = ctypes.create_string_buffer(size); got = ctypes.c_uint()
            L.SFileReadFile(f, buf, size, ctypes.byref(got), None); L.SFileCloseFile(f)
            return buf.raw[:got.value], n
    return None, None

def dbc(name):
    data, src = read('DBFilesClient\\' + name)
    path = '/tmp/' + name; open(path, 'wb').write(data)
    d = T.DBC(path); os.remove(path); return d

def anims(m2):
    if not m2 or m2[:4] != b'MD20':
        return None
    n, ofs = struct.unpack_from('<II', m2, 0x1C)
    return {struct.unpack_from('<H', m2, ofs + i * 64)[0] for i in range(n) if ofs + i * 64 + 2 <= len(m2)}

if __name__ == '__main__':
    import time
    start = time.time()
    CACHE = os.path.expanduser('~/tc/costumes/models.json')     # model -> [anim ids] or None, kept between runs
    cache = json.load(open(CACHE)) if os.path.exists(CACHE) else {}
    cdi, cmd = dbc('CreatureDisplayInfo.dbc'), dbc('CreatureModelData.dbc')
    todo = []
    for did, r in cdi.rows.items():
        mrow = cmd.rows.get(r[1])
        path = cmd.s(mrow[2]) if mrow else ''
        if path:
            todo.append((did, re.sub(r'\.md[xl]$', '.m2', path, flags=re.I)))
    left = 0
    for did, m2path in todo:
        if m2path in cache:
            continue
        if time.time() - start > 140:
            left += 1
            continue
        data, src = read(m2path)
        a = anims(data)
        cache[m2path] = sorted(a) if a is not None else None
    json.dump(cache, open(CACHE, 'w'))
    if left:
        print('models still to read:', len({m for d, m in todo if m not in cache}), '- run again'); sys.exit(1)
    out = {}
    for did, m2path in todo:
        a = cache.get(m2path)
        if a is None:
            continue
        a = set(a)
        out[did] = {'model': m2path, 'missing': [k for k, i in NEEDED.items() if i not in a], 'anims': len(a)}
    json.dump(out, open(os.path.expanduser('~/tc/costumes/anims.json'), 'w'))
    full = [d for d, x in out.items() if not x['missing']]
    print(len(out), 'displays checked,', len(cache), 'models,', len(full), 'with every animation')
