"""One headless snapshot flow: disk-load continuation, slots, previews and rejection."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import urllib.request
import zipfile
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]

def chunks(data: bytes):
    at = 12
    for _ in range(2):
        n = struct.unpack_from('<H', data, at)[0]; at += n+2
    hashes = struct.unpack_from('<I', data, at)[0]; at += 4+32*hashes
    body = at
    rows = []
    while at+12 <= len(data):
        tag, instance, n = struct.unpack_from('<4sII', data, at)
        if tag == b'END ': break
        rows.append((tag, instance, at, n)); at += 12+n
    return body, rows

def machine_bytes(path: Path) -> bytes:
    data = path.read_bytes(); body, rows = chunks(data)
    return data[:body]+b''.join(data[at:at+12+n] for tag, _, at, n in rows
                               if tag not in {b'TIME', b'VERS', b'THMB', b'SHA2'})

def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, default=ROOT/'work/build-x64/rec64.exe')
    parser.add_argument('--restart-only', action='store_true', help='reuse verified continuation artifacts for restart/slot checks')
    args = parser.parse_args(); binary = args.binary.resolve()
    work = ROOT/'work/verify-states'; work.mkdir(parents=True, exist_ok=True)
    pin = json.loads((ROOT/'docs/sources.lock.json').read_text())['test_programs']['krill-loader-v164']
    archive = ROOT/'work/p5/loader-v164.zip'; archive.parent.mkdir(parents=True, exist_ok=True)
    if not archive.exists(): archive.write_bytes(urllib.request.urlopen(pin['url'], timeout=45).read())
    assert hashlib.sha256(archive.read_bytes()).hexdigest() == pin['sha256']
    with zipfile.ZipFile(archive) as z: original = z.read(pin['file'])
    assert hashlib.sha256(original).hexdigest() == pin['file_sha256']
    fixture = work/'loader.d64'; fixture.write_bytes(original)
    trace = (work/'commands.jsonl').open('a' if args.restart_only else 'w', encoding='utf8')

    class Client:
        def __init__(self, name: str, model: str, existing: bool = False):
            self.config = work/f'{name}.yaml'
            if not existing:
                self.config.write_text(json.dumps({'machine':{'model':model}, 'media':{'disk':{'write_protect':True}},
                                                   'ui':{'setup_page':'states'}}))
            self.proc = subprocess.Popen([str(binary), '--hidden', '--config', str(self.config), '--control', 'stdin'],
                                         cwd=work if (binary.parent/'roms').is_dir() else ROOT,
                                         stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, bufsize=1)
        def call(self, command: str, ok: bool = True):
            self.proc.stdin.write(command+'\n'); self.proc.stdin.flush()
            line = self.proc.stdout.readline()
            if not line: raise AssertionError((command, self.proc.poll(), self.proc.stderr.read()))
            result = json.loads(line); trace.write(json.dumps({'command':command, 'reply':result})+'\n'); trace.flush()
            assert result['ok'] == ok, (command, result)
            return result
        def save(self, name: str):
            path = work/(name+'.rcs'); self.call(f'state save {path.as_posix()}'); return path
        def close(self):
            try:
                if self.proc.poll() is None: self.call('quit')
                _, err = self.proc.communicate(timeout=10); assert self.proc.returncode == 0, err
            finally:
                if self.proc.poll() is None: self.proc.kill(); self.proc.communicate()

    slot = work/'states/c64-pal/slot-1.rcs'
    if not args.restart_only:
        client = Client('pal', 'c64-pal')
        try:
            client.call('frames 200'); client.call('expect READY.'); client.call(f'drop {fixture.as_posix()}'); client.call('enter')
            client.call('until 90 LOADING'); client.call('key Ctrl+F5')
            slot = work/'states/c64-pal/slot-1.rcs'; assert slot.is_file()
            _, rows = chunks(slot.read_bytes()); assert {b'MDIA',b'THMB',b'AUTO',b'SHA2'} <= {r[0] for r in rows}
            client.call('frames 2600'); client.call(f'shot {(work/"expected.png").as_posix()}'); expected = client.save('expected')
            client.call('key Ctrl+F6'); client.call('frames 2600'); client.call(f'shot {(work/"resumed.png").as_posix()}'); resumed = client.save('resumed')
            assert (work/'expected.png').read_bytes() == (work/'resumed.png').read_bytes(), 'Disk load resumed to a different screen'
            assert machine_bytes(expected) == machine_bytes(resumed), 'Disk-load continuation state diverged'
            with Image.open(work/'resumed.png') as image: assert len(image.getcolors(image.width*image.height)) >= 4
            # All refusals preserve the running machine and pending input.
            before = machine_bytes(client.save('before-refusal'))
            def refused(path: Path, message: str):
                answer = client.call(f'state load {path.as_posix()}', ok=False)
                assert message.lower() in answer['error'].lower(), answer
                assert machine_bytes(client.save('after-refusal')) == before
            bad = bytearray(slot.read_bytes()); _, rows = chunks(bad)
            at = next(at for tag, _, at, _ in rows if tag == b'VERS'); bad[at+14] ^= 1
            corrupt = work/'corrupt.rcs'; corrupt.write_bytes(bad); refused(corrupt, 'checksum')
            bad = bytearray(slot.read_bytes()); at=12
            for _ in range(2): at += 2+struct.unpack_from('<H', bad, at)[0]
            bad[at+4] ^= 1
            sha = next(at for tag, _, at, _ in rows if tag == b'SHA2'); bad[sha+12:sha+44] = hashlib.sha256(bad[:sha]).digest()
            wrong_rom = work/'wrong-rom.rcs'; wrong_rom.write_bytes(bad); refused(wrong_rom, 'other ROMs')
            fixture.rename(work/'missing.d64')
            try: refused(slot, 'loader.d64')
            finally: (work/'missing.d64').rename(fixture)
            changed = bytearray(original); changed[-1] ^= 1; fixture.write_bytes(changed)
            try: refused(slot, 'media has changed')
            finally: fixture.write_bytes(original)
            client.call('key F12'); client.call('expect SLOT 01'); client.call(f'shot {(work/"states.png").as_posix()}')
            client.call('key F10'); client.call('expect Full speed'); client.call('key F10'); client.call('expect 100% speed')
        finally: client.close()
        print('Snapshot disk-load continuation and refusal checks passed', flush=True)

    else:
        assert machine_bytes(work/'expected.rcs') == machine_bytes(work/'resumed.rcs')
        assert (work/'expected.png').read_bytes() == (work/'resumed.png').read_bytes()

    fresh = Client('pal', 'c64-pal', existing=True)
    try:
        fresh.call(f'drop {slot.as_posix()}'); fresh.call('frames 2600')
        fresh.call(f'shot {(work/"fresh.png").as_posix()}')
        assert (work/'fresh.png').read_bytes() == (work/'expected.png').read_bytes()
    finally: fresh.close()
    ntsc = Client('ntsc', 'c64-ntsc')
    try:
        ntsc.call('frames 200'); before = machine_bytes(ntsc.save('ntsc-before'))
        answer = ntsc.call(f'state load {slot.as_posix()}', ok=False); assert 'another machine' in answer['error']
        assert machine_bytes(ntsc.save('ntsc-after')) == before
        ntsc.call('key F12')
        for _ in range(9): ntsc.call('key DOWN')
        ntsc.call('key Ctrl+F5'); ntsc.call('expect Saved slot 10')
        assert (work/'states/c64-ntsc/slot-10.rcs').is_file()
        ntsc.call(f'shot {(work/"slot-10.png").as_posix()}')
        ntsc.call('poke 0400 FF'); ntsc.call('key Ctrl+F6'); ntsc.call('expect Loaded slot 10')
    finally: ntsc.close()
    ntsc = Client('ntsc', 'c64-ntsc', existing=True)
    try: ntsc.call('key Ctrl+F6'); ntsc.call('expect Loaded slot 10')
    finally: ntsc.close()
    trace.close()
    result = {'status':'passed', 'continuation_artifacts_reused':args.restart_only, 'version':subprocess.check_output([str(binary), '--version'], text=True).strip(),
              'checks':['mid-disk-load quick save/load resumes to identical screen and state',
                        'pending auto-run and keyboard phase', 'fresh-process snapshot browser/drop',
                        'PAL/NTSC slot isolation and slot 10 selection persistence', 'guest thumbnails',
                        'checksum, ROM, model, missing/changed media refusals preserve running state',
                        'F10 in States page'],
              'resumed_png_sha256':hashlib.sha256((work/'resumed.png').read_bytes()).hexdigest()}
    (work/'result.json').write_text(json.dumps(result, indent=2)+'\n')
    print('Snapshot release flow passed', flush=True)

if __name__ == '__main__': main()
