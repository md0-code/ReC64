"""Headless FC3 cold boot and opt-in startup-cartridge lifecycle."""
from pathlib import Path
import argparse
import json
import subprocess
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, default=ROOT/'work/build-x64/rec64.exe')
    parser.add_argument('--cartridge', type=Path, default=ROOT/'work/p7/freezers/fixtures/fc3.crt')
    args = parser.parse_args()
    work = ROOT/'work/cartridge-startup'; work.mkdir(parents=True, exist_ok=True)
    config = work/'rec64.yaml'
    config.write_text('# Startup test\n'+json.dumps({'machine': {'model': 'c64-pal'},
                      'media': {'disk': {'image': 'absent.d64'}}}))
    log = (work/'commands.jsonl').open('w')

    def start():
        return subprocess.Popen([str(args.binary.resolve()), '--hidden', '--config', str(config),
                                 '--control', 'stdin'], cwd=ROOT, stdin=subprocess.PIPE,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

    def call(command):
        proc.stdin.write(command+'\n'); proc.stdin.flush()
        line = proc.stdout.readline()
        assert line, (command, proc.poll(), proc.stderr.read())
        reply = json.loads(line)
        log.write(json.dumps({'command': command, 'reply': reply})+'\n'); log.flush()
        assert reply['ok'], (command, reply)
        return reply

    def startup_toggle():
        call('key F12')
        if 'CARTRIDGE 6/8' not in call('text')['text']:
            for _ in range(5): call('key RIGHT')
        for _ in range(2): call('key DOWN')
        call('key RETURN')

    def desktop(label):
        call('frames 150')
        path = work/f'{label}.png'; call('shot '+path.as_posix())
        with Image.open(path) as im:
            assert len(im.convert('RGB').getcolors(im.width*im.height)) >= 2, 'Blank FC3 desktop'
        assert int(call('peek D011')['data'], 16)&0x30 == 0x30, 'FC3 desktop did not start'

    proc = start()
    try:
        call('insert cartridge '+args.cartridge.resolve().as_posix())
        startup_toggle(); call('expect START INSERTED')
        call('shot '+(work/'startup-option.png').as_posix())
        assert 'startup: true' in config.read_text() and 'image:' in config.read_text()
        assert 'absent.d64' not in config.read_text()
        call('quit'); proc.wait(timeout=10); proc = start()
        desktop('launch')
        call('key Ctrl+F12'); desktop('cold-boot')
        # RUN/STOP during original FC3 startup selects BASIC instead of its desktop.
        call('reset power'); call('key Escape'); call('frames 150'); call('release Escape')
        call('expect READY.')
        call('eject cartridge')
        assert 'image:' not in config.read_text()
        call('insert cartridge '+args.cartridge.resolve().as_posix())
        startup_toggle(); call('key F12')
        assert 'image:' not in config.read_text() and 'startup: false' in config.read_text()
        call('quit'); proc.wait(timeout=10); proc = start()
        call('frames 150'); call('expect 38911 BASIC BYTES FREE')
        assert call('peek 8004 5')['data'].replace(' ', '').lower() != 'c3c2cd3830'
        print('FC3 cold boot, startup setting, relaunch, eject and opt-out passed')
    finally:
        if proc.poll() is None: call('quit')
        proc.wait(timeout=10); log.close()


if __name__ == '__main__': main()
