"""One native headless flow for the live status panel and C64 touch keyboard."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import zipfile
from PIL import Image, ImageChops

ROOT = Path(__file__).resolve().parents[1]


def chunk(path: Path, wanted: bytes) -> bytes:
    data = path.read_bytes(); at = 12
    for _ in range(2): at += 2 + struct.unpack_from('<H', data, at)[0]
    at += 4 + 32 * struct.unpack_from('<I', data, at)[0]
    while at + 12 <= len(data):
        tag, _, size = struct.unpack_from('<4sII', data, at)
        if tag == wanted: return data[at+12:at+12+size]
        at += 12 + size
    raise AssertionError(f'Missing chunk {wanted!r}')


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, default=ROOT/'work/build-x64/rec64.exe')
    args = parser.parse_args(); binary = args.binary.resolve()
    work = ROOT/'work/p8/osd-acceptance'; work.mkdir(parents=True, exist_ok=True)
    config = work/'osd.yaml'
    config.write_text(json.dumps({'machine': {'model': 'c64-pal'}, 'video': {'border': 'none'},
                                 'media': {'disk': {'write_protect': True}}}))
    trace = (work/'commands.jsonl').open('w', encoding='utf8')
    proc = subprocess.Popen([str(binary), '--hidden', '--config', str(config), '--control', 'stdin'],
                            cwd=ROOT, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            text=True, bufsize=1)

    def call(command: str):
        proc.stdin.write(command+'\n'); proc.stdin.flush()
        line = proc.stdout.readline()
        assert line, (command, proc.poll(), proc.stderr.read())
        result = json.loads(line); trace.write(json.dumps({'command': command, 'reply': result})+'\n'); trace.flush()
        assert result['ok'], (command, result)
        return result

    def save(name: str) -> Path:
        path = work/f'{name}.rcs'; call(f'state save {path.as_posix()}'); return path

    def matrix(name: str) -> bytes:
        return chunk(save(name), b'KBD ')[4:13]

    def shot(name: str) -> Path:
        path = work/f'{name}.png'; call(f'shot {path.as_posix()}'); return path

    # Read the public original layout rather than maintain a second key map.
    caps = [(int(k), int(x), int(y), int(w)) for k,x,y,w in re.findall(
        r'K\((\d+),(\d+),(\d+),(\d+),"[^"]*"\)', (ROOT/'src/core/machines/c64_layout.c').read_text())]
    panel = False

    def pointer(key: int, action: str, ident: int = 1):
        cap = next(c for c in caps if c[0] == key and (key != 15 or c[2] == 12))
        _, x, y, width = cap
        call(f'pointer {ident} {x*4+width*2} {200-(32 if panel else 0)-96+16+y*4+8} {action}')

    try:
        call('frames 200'); call('expect READY.'); guest = shot('guest')
        call('key F9'); panel = True; call('expect 1541 #08'); call('expect TAPE POS'); call('expect KEYBOARD JOYSTICK OFF')
        panel_image = shot('panel'); call('key Shift+F9'); call('expect ON-SCREEN KEYBOARD OPEN')
        combined = shot('combined')
        with Image.open(guest) as a, Image.open(panel_image) as b, Image.open(combined) as c:
            assert a.size == b.size == c.size == (320, 200)
            assert ImageChops.difference(a.crop((0,0,320,72)), c.crop((0,0,320,72))).getbbox() is None
            assert ImageChops.difference(b.crop((0,168,320,200)), c.crop((0,168,320,200))).getbbox() is None

        # Real BASIC takes the virtual A switch, independent of the physical keymap.
        pointer(10, 'down'); call('frames 3'); pointer(10, 'up'); call('frames 3')
        screen = call('text')['text']; assert re.search(r'\nA\s*\n', screen), screen
        pointer(10, 'down', 1); pointer(10, 'down', 2); pointer(10, 'up', 1)
        assert matrix('two-fingers')[1] & 4
        pointer(28, 'move', 2); state = matrix('slide'); assert not state[1] & 4 and state[3] & 16
        pointer(28, 'cancel', 2); assert not any(matrix('cancel'))
        pointer(15, 'down', 3); pointer(10, 'down', 4); state = matrix('shift-a'); assert state[1] & 132 == 132
        call('frames 3'); pointer(10, 'up', 4); pointer(15, 'up', 3)
        for key in (58, 61): pointer(key, 'down'); pointer(key, 'up')
        sticky = save('sticky'); assert '[LSH]' in call('text')['text']
        call('key Shift+F9'); assert not any(matrix('closed'))
        call(f'state load {sticky.as_posix()}'); call('expect ON-SCREEN KEYBOARD OPEN')
        state = matrix('restored'); assert state[1] & 128 and state[7] & 36 == 36
        # Controller accept uses the restored selection, then navigation reaches another cap.
        call('key RIGHT'); call('key RIGHT'); call('key Pad A'); assert sum(v.bit_count() for v in matrix('pad-down')) >= 4
        call('release Pad A'); assert sum(v.bit_count() for v in matrix('pad-up')) == 3
        shot('sticky-restored'); call('key Pad B'); assert not any(matrix('pad-closed'))
        call('key F11'); call('expect KEYBOARD JOYSTICK PORT 2'); call('key Shift+F11'); call('expect KEYBOARD JOYSTICK PORT 1'); call('key F11')
        print('Native OSK input, multitouch, sticky snapshot and combined crop passed', flush=True)

        # The pinned disk executes its original loader while F9 keeps showing live state.
        pin = json.loads((ROOT/'docs/sources.lock.json').read_text())['test_programs']['krill-loader-v164']
        archive = ROOT/'work/p5/loader-v164.zip'
        assert hashlib.sha256(archive.read_bytes()).hexdigest() == pin['sha256']
        with zipfile.ZipFile(archive) as z: original = z.read(pin['file'])
        assert hashlib.sha256(original).hexdigest() == pin['file_sha256']
        fixture = work/'loader.d64'; fixture.write_bytes(original)
        call('reset power'); call('frames 200'); call(f'drop {fixture.as_posix()}'); call('enter'); call('until 90 LOADING')
        seen = []
        for _ in range(16):
            call('frames 5'); reply = call('text'); line = next(s for s in reply['text'].splitlines() if s.startswith('1541 #'))
            seen.append({'frame': reply['frame'], 'drive': line})
            if 'MOTOR ON' in line and 'LED ON' in line:
                shot('drive-loading'); break
        assert any('MOTOR ON' in s['drive'] and 'LED ON' in s['drive'] for s in seen), seen
        start_frame = seen[-1]['frame']; call('key F9'); call('frames 5'); call('key F9'); call('expect 1541 #08')
        call('until 90 INSTALLING LOADER'); final = call('text'); assert final['frame'] > start_frame
        shot('loader-running'); (work/'drive-observations.json').write_text(json.dumps(seen, indent=2)+'\n')
        (work/'result.json').write_text(json.dumps({'status': 'passed', 'version': subprocess.check_output([str(binary), '--version'], text=True).strip(),
            'checks': ['native BASIC key touch', 'same-key multitouch and slide/cancel', 'SHIFT+A matrix chord',
                       'sticky SHIFT/CTRL/C= snapshot rebind and close release', 'controller navigation and accept release',
                       'live keyboard joystick port', '320x200 combined overlay preserves guest and panel',
                       'F9 shows real drive LED/motor during original custom load; loader continues']}, indent=2)+'\n')
        print('Native status panel during original disk load passed', flush=True)
    finally:
        try:
            if proc.poll() is None: call('quit')
            _, error = proc.communicate(timeout=10); assert proc.returncode == 0, error
        finally:
            if proc.poll() is None: proc.kill(); proc.communicate()
            trace.close()


if __name__ == '__main__': main()
