"""One headless flow for setup, session media, input and transport controls."""
from pathlib import Path
import argparse
import json
import struct
import subprocess
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, default=ROOT/'work/build-x64/rec64.exe')
    args = parser.parse_args()
    work = ROOT/'work/verify-shell'; work.mkdir(parents=True, exist_ok=True)
    config = work/'rec64.yaml'
    config.write_text('# Keep this comment\n'+json.dumps({
        'machine': {'model': 'c64-pal'}, 'video': {'palette': 'default', 'border': 'full'},
        'media': {'disk': {'image': 'missing.d64'}},
        'controllers': {'joystick1': {'device': 'controller 2'}, 'joystick2': {'device': 'controller 1'}}}), encoding='utf8')
    version = (ROOT/'src/VERSION').read_text().strip()
    trace = (work/'commands.jsonl').open('w', encoding='utf8')

    def start():
        return subprocess.Popen([str(args.binary.resolve()), '--hidden', '--config', str(config), '--control', 'stdin'],
                                cwd=ROOT, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, bufsize=1)

    proc = start()

    def call(command):
        proc.stdin.write(command+'\n'); proc.stdin.flush()
        line = proc.stdout.readline()
        assert line, (command, proc.poll(), proc.stderr.read())
        result = json.loads(line); trace.write(json.dumps({'command': command, 'reply': result})+'\n'); trace.flush()
        assert result['ok'], (command, result)
        return result

    def shot(name):
        path = work/f'{name}.png'; call(f'shot {path.as_posix()}'); return path

    def text(): return call('text')['text']

    def key(name, count=1):
        for _ in range(count): call('key '+name)

    try:
        call('frames 200'); call('expect READY.')
        assert 'image:' not in config.read_text() and 'missing.d64' not in config.read_text()
        key('F12'); screen = text()
        assert f'REC64 {version}' in screen and screen.count('MACHINE') == 1 and '1/8' in screen, screen
        pal = shot('machine')
        key('RIGHT'); screen = text(); assert screen.index('DRIVE') < screen.index('TAPE') and 'FAST LOAD' in screen and 'EJECT' not in screen
        shot('media')
        key('RIGHT'); key('DOWN'); key('RETURN'); call('expect COLOR-CORRECTED')
        key('RETURN'); call('expect TUNED')
        key('RETURN'); call('expect TUNED-STRONG')
        key('RETURN'); call('expect DEFAULT')
        key('LEFT', 2); key('DOWN'); key('RETURN'); key('F12'); call('frames 200')
        key('F12'); ntsc = shot('ntsc')
        with Image.open(pal) as a, Image.open(ntsc) as b:
            assert a.getpixel((40,55)) != b.getpixel((40,55)), 'Automatic PAL/NTSC palette did not change'
        key('DOWN', 4); key('RETURN'); assert 'MACHINE' not in text(); call('frames 200'); call('expect READY.')
        key('F12'); key('DOWN', 5); key('RETURN'); assert 'MACHINE' not in text(); call('frames 200'); call('expect READY.')
        key('F12'); key('RIGHT', 4); key('DOWN'); sources=[]
        for _ in range(3):
            key('RETURN'); sources.append(next(line.strip() for line in text().splitlines() if 'INPUT 1' in line))
        assert any('NONE' in s for s in sources) and any('CONTROLLER 1' in s for s in sources) and any('CONTROLLER 2' in s for s in sources), sources
        assert not any('KEYBOARD' in s or 'CONTROLLER 3' in s for s in sources)
        shot('controllers'); key('F12'); key('F11')
        call('poke DC02 00'); key('Up'); assert int(call('peek DC00')['data'],16)&1 == 0
        call('release Up'); assert int(call('peek DC00')['data'],16)&1 == 1
        key('Shift+F11'); call('expect Joystick port 1'); call('poke DC03 00'); key('Up'); assert int(call('peek DC01')['data'],16)&1 == 0
        call('release Up'); key('F11')
        key('Ctrl+F3'); call('expect DRIVE 1/2'); shot('drive-widget'); assert 'FAST LOAD' not in text()
        key('RIGHT'); call('expect TAPE 2/2'); shot('tape-empty'); key('Ctrl+F4'); assert 'TAPE 2/2' not in text()
        # The widget uses the actual writable Datasette transport.
        tape = work/'controls.tap'; tape.write_bytes(b'C64-TAPE-RAW'+bytes([1,0,0,0])+struct.pack('<I',64)+bytes([48])*64)
        call(f'insert tape {tape.as_posix()}')
        def tape_action(label):
            if 'TAPE 2/2' in text(): key('ESCAPE')
            key('Ctrl+F4')
            actions=[line.strip().split('  ')[0] for line in text().splitlines()[5:12] if 'ENTER' in line]
            key('DOWN',actions.index(label)); key('RETURN')
        tape_action('PLAY'); call('expect PLAY'); tape_action('STOP'); call('expect STOP')
        tape_action('RECORD'); call('expect RECORD'); shot('tape-widget')
        tape_action('EJECT'); call('expect NO IMAGE INSERTED'); key('ESCAPE')
        # A real KERNAL LOAD and RUN complete using the instant command paste.
        program = work/'instant.prg'; body = b'\x99"SHELL OK"'; line = struct.pack('<H',10)+body+b'\0'
        program.write_bytes(b'\x01\x08'+struct.pack('<H',0x801+2+len(line))+line+b'\0\0')
        call(f'drop {program.as_posix()}'); call('frames 200'); call('expect SHELL OK'); shot('instant-load')
        assert 'instant.prg' not in config.read_text(), config.read_text()
        # Menu reset hotkeys close setup too.
        key('F12'); key('Alt+F12'); assert f'REC64 {version}' not in text()
        key('F12'); key('Ctrl+F12'); assert f'REC64 {version}' not in text()
        call('quit'); _,error = proc.communicate(timeout=10); assert proc.returncode == 0, error
        # Launch again: the program and tape must be absent.
        proc = start(); key('Ctrl+F4'); call('expect NO IMAGE INSERTED'); key('ESCAPE')
        call('frames 200'); call('expect READY.'); assert 'SHELL OK' not in text()
        assert config.read_text().startswith('# Keep this comment')
        call('quit'); _,error = proc.communicate(timeout=10); assert proc.returncode == 0, error
        (work/'result.json').write_text(json.dumps({'status':'passed','version':version,'checks':['built-in palettes and automatic PAL/NTSC', 'version/page header', 'drive-first media settings', 'two controller sources and independent keyboard joystick', 'reset closes menus', 'drive/tape live widgets and recording/eject', 'instant LOAD/RUN', 'session-only media and configuration reload']},indent=2)+'\n')
        print('Shell controls, palettes, session media, instant loading and transport widgets passed')
    finally:
        if proc.poll() is None:
            proc.kill(); proc.communicate()
        trace.close()
    return 0


if __name__ == '__main__': raise SystemExit(main())
