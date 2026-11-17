"""Run unpatched freezer firmware from work/p7/freezers/fixtures (pins in sources.lock.json)."""
from pathlib import Path
import argparse
import hashlib
import json
import struct
import subprocess
from verify_states import chunks, machine_bytes

ROOT = Path(__file__).resolve().parents[1]
NAMES = ('ar4', 'ar41', 'ar5', 'ar6', 'rr', 'fc3')

def cart_bytes(path):
    data = path.read_bytes()
    _, rows = chunks(data)
    return next(data[at+12:at+12+n] for tag, _, at, n in rows if tag == b'CART')

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, default=ROOT/'work/build-x64/rec64.exe')
    parser.add_argument('--case', choices=NAMES, action='append')
    args = parser.parse_args()
    work = ROOT/'work/p7/freezers'; work.mkdir(parents=True, exist_ok=True)
    pins = json.loads((ROOT/'docs/sources.lock.json').read_text())['test_suites']['freezer_firmware']
    for name in args.case or NAMES:
        fixture = work/'fixtures'/f'{name}.crt'
        data = fixture.read_bytes()
        if name.startswith('ar'):
            filenames = dict(zip(NAMES[:4], pins['action_replay']['files']))
            digest = pins['action_replay']['files'][filenames[name]]['sha256']
        else:
            payload = bytearray(); at = 64
            while at < len(data):
                size = struct.unpack_from('>I', data, at+4)[0]
                payload.extend(data[at+16:at+size]); at += size
            data = bytes(payload)
            pin = pins['retro_replay' if name == 'rr' else 'final_cartridge_iii']
            digest = pin['rom_sha256' if name == 'rr' else 'sha256']
        assert hashlib.sha256(data).hexdigest() == digest, name+' firmware differs from original'
        config = work/f'{name}.yaml'
        config.write_text(json.dumps({'machine':{'model':'c64-pal'}, 'media':{'cartridge':{
            'image':fixture.as_posix(), 'write_protect':True}}, 'video':{'look':'plain','filter':'nearest'}}))
        log = (work/f'{name}-commands.jsonl').open('w')
        err = (work/f'{name}-stderr.log').open('w')
        proc = subprocess.Popen([str(args.binary.resolve()), '--hidden', '--config', str(config), '--control', 'stdin'],
                                cwd=ROOT, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=err, text=True)
        def call(command):
            proc.stdin.write(command+'\n'); proc.stdin.flush()
            line = proc.stdout.readline(); assert line, (command, proc.poll())
            reply = json.loads(line); log.write(json.dumps({'command':command,'reply':reply})+'\n'); log.flush()
            assert reply['ok'], (command,reply)
            return reply
        def save(label):
            path = work/f'{name}-{label}.rcs'; call('state save '+path.as_posix()); return path
        def key(keyname):
            call('key '+keyname); call('frames 5'); call('release '+keyname)
        try:
            call('insert cartridge '+fixture.as_posix())
            # Original FC3 manual: RUN/STOP during power-on enters BASIC.
            if name == 'fc3': call('key Escape')
            call('frames 150')
            if name == 'fc3': call('release Escape')
            startup = call('text')['text']
            expected = 'READY.' if name in ('ar4','fc3') else 'RETRO REPLAY' if name == 'rr' else 'ACTION REPLAY'
            assert expected in startup, (name,startup)
            call('shot '+(work/f'{name}-startup.png').as_posix())
            if name in ('ar5','ar6','rr'):
                key('F1'); call('frames 100'); call('expect READY.')
            before = save('before')
            # Right Alt combined with another key or text must not pulse the cartridge.
            for chord in (('key Q','release Q'), ('type @',)):
                call('key RightAlt')
                for command in chord: call(command)
                call('release RightAlt')
                assert cart_bytes(save('chord')) == cart_bytes(before), name+' AltGr initiated freeze'
                call('state load '+before.as_posix())
            call('key RightAlt'); call('release RightAlt')
            active = save('active')
            assert struct.unpack_from('<I',cart_bytes(active),4)[0] > 0, 'No physical freeze pulse'
            call('frames 100'); first = save('expected')
            call('shot '+(work/f'{name}-freeze.png').as_posix())
            call('state load '+active.as_posix()); call('frames 100'); second = save('resumed')
            call('shot '+(work/f'{name}-resumed.png').as_posix())
            assert machine_bytes(first) == machine_bytes(second), name+' freeze continuation diverged'
            assert (work/f'{name}-freeze.png').read_bytes() == (work/f'{name}-resumed.png').read_bytes()
            menu = call('text')['text']
            if name in ('ar4','ar41'):
                assert 'F5/MENU' in menu, menu
                key('F5'); call('frames 100'); menu = call('text')['text']
                assert 'BACKUP' in menu, menu
                call('shot '+(work/f'{name}-full-menu.png').as_posix())
            elif name != 'fc3': assert 'BACKUP' in menu and 'MONITOR' in menu, menu
            # FC3 renders its original menu with ROM sprites over invalid bitmap mode;
            # the text-plane extractor cannot read it. Inspect fc3-freeze.png visually.
            result = {'status':'passed','firmware_sha256':digest,'startup':startup,'menu_text':menu,
                      'checks':['original firmware startup','RightAlt alone','AltGr key/text chords',
                                'exact machine and screen continuation from active physical freeze pulse'],
                      'freeze_png_sha256':hashlib.sha256((work/f'{name}-freeze.png').read_bytes()).hexdigest()}
            (work/f'{name}-result.json').write_text(json.dumps(result,indent=2)+'\n')
            print(name+' original firmware and active-freeze snapshot passed',flush=True)
        finally:
            if proc.poll() is None: call('quit')
            proc.wait(timeout=10); log.close(); err.close()

if __name__ == '__main__': main()
