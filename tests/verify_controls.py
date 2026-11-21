"""One headless settings, control OSD, border and controller-action flow."""
from pathlib import Path
import argparse
import json
import subprocess
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, default=ROOT/'build/x64/rec64.exe')
    args = parser.parse_args()
    work = ROOT/'work/verify-controls'; work.mkdir(parents=True, exist_ok=True)
    media = work/'pad-media'; media.mkdir(exist_ok=True)
    for name in ('AB.PRG', 'BA.PRG', 'AB.TAP'):
        (media/name).unlink(missing_ok=True)
    config = work/'rec64.yaml'
    config.write_text(json.dumps({'machine': {'model': 'c64-pal', 'options': {'sid': 'default'}}, 'video': {'border': 'none'},
                                 'controllers': {'keyboard_joystick': True}, 'ui': {'state_slot': 3}}))
    log = (work/'commands.jsonl').open('w')
    proc = subprocess.Popen([str(args.binary.resolve()), '--hidden', '--config', str(config),
                             '--control', 'stdin'], cwd=ROOT, stdin=subprocess.PIPE,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

    def call(command):
        proc.stdin.write(command+'\n'); proc.stdin.flush()
        line = proc.stdout.readline(); assert line, (command, proc.poll(), proc.stderr.read())
        reply = json.loads(line); log.write(json.dumps({'command': command, 'reply': reply})+'\n'); log.flush()
        assert reply['ok'], (command, reply)
        return reply

    def key(name, count=1):
        for _ in range(count): call('key '+name)

    def text(): return call('text')['text']

    def pad(*buttons):
        for button in buttons: key('Pad '+button)

    def shot(name, size=(320, 200)):
        path = work/f'{name}.png'; call('shot '+path.as_posix())
        with Image.open(path) as im: assert im.size == size, (name, im.size, size)
        return path

    def selected_row(name, selected, disabled=None):
        with Image.open(shot(name)) as im:
            highlight = im.getpixel((0, 0))
            assert im.getpixel((16, selected*8)) == highlight, name
            if disabled is not None:
                assert im.getpixel((16, disabled*8)) != highlight, name

    try:
        call('frames 150'); call('expect READY.')
        key('Pad B'); t = text(); assert '6581' in t and 'DEFAULT' not in t, t
        key('DOWN', 2); key('Pad A'); call('expect 8580'); key('Pad A'); call('expect 6581'); shot('machine-settings')
        key('RIGHT'); t = text()
        assert 'DEVICE' in t and 'FAST LOAD' in t and 'CONTROLS' not in t and 'EJECT' not in t and 'LOAD IMAGE' not in t, t
        assert 'STARTUP ONLY' not in t and all(row.strip().endswith('OFF') for row in t.splitlines() if row.strip().startswith('SOUND'))
        key('DOWN', 2); selected_row('media-skip-drive-sound', 8, 7)
        pad('Down', 'Down'); selected_row('media-skip-tape-sound', 13, 12)
        pad('Up', 'A', 'Down'); selected_row('media-tape-sound-enabled', 12)
        pad('Up', 'A')
        key('RIGHT', 3); t = text(); assert 'KEYBOARD JOYSTICK (P2)' in t and 'F11' in t, t
        assert 'KEYBOARD JOY ' not in t and 'ON/OFF' not in t
        key('F11'); call('expect KEYBOARD JOYSTICK (OFF)'); key('DOWN', 4); key('Pad A'); call('expect KEYBOARD JOYSTICK (P2)')
        t = text(); rows=t.splitlines(); keyboard=next(row for row in rows if 'KEYBOARD JOYSTICK (' in row); swap=next(row for row in rows if 'SWAP PORTS' in row)
        assert keyboard.index('F11')+3 == swap.index('SHIFT+F11')+9, 'Shortcut columns differ'
        shot('controllers-settings'); key('Pad LB'); call('expect KEYBOARD JOYSTICK (P1)')
        key('RIGHT'); t = text(); assert 'REU' in t and 'LOAD CARTRIDGE' in t and 'FREEZE' in t, t
        shot('cartridge-settings'); key('Pad A'); call('expect FILES AND FOLDERS'); key('Pad B'); call('expect CARTRIDGE 6/8'); key('Pad B')
        key('Pad Y'); call('expect ON-SCREEN KEYBOARD OPEN'); key('Pad Y')
        key('Pad X'); call('expect FILES AND FOLDERS'); shot('load-browser'); key('Pad B')
        key('Pad RB'); call('expect Full speed'); key('Pad RB'); call('expect 100% speed')
        key('Pad RT'); call('expect SELECT SNAPSHOT SLOT'); key('Pad A'); call('expect No snapshot in this slot'); shot('first-load-picker'); key('Pad B')
        call('poke 0400 42'); key('Pad LT'); call('expect SELECT SNAPSHOT SLOT'); key('DOWN'); shot('save-picker'); key('Pad A')
        call('expect Saved slot 4'); assert (work/'states/c64-pal/slot-4.rcs').exists()
        call('poke 0400 99'); key('Pad RT'); call('expect Loaded slot 4'); assert call('peek 0400')['data'] == '42'
        assert 'SELECT SNAPSHOT SLOT' not in text()
        # A live border change keeps settings and the guest at the same dimensions.
        key('Pad B'); key('LEFT', 3); key('DOWN', 2); key('Pad A'); shot('full-border-settings', (384, 272))
        key('Pad A'); shot('medium-border-settings', (352, 232)); key('Pad B'); shot('medium-border-guest', (352, 232))

        # Folder, save format, filename, search and file actions use the controller.
        pad('B', 'Left', 'Up', 'A', 'Down', 'A')
        call('expect X USE CURRENT FOLDER')
        pad('X', 'B')
        call('type 10 PRINT 42'); call('enter'); call('frames 4')
        key('Ctrl+F2'); pad('RB','Right','Left','RB','Y','Up','Right','Up','Up')
        call('expect NAME: AB.PRG')
        shot('pad-filename', (352, 232))
        pad('A'); assert (media/'AB.PRG').exists()

        key('Ctrl+F2'); pad('Back','Up','A','X','Y','Up','Up','Right','Up')
        call('expect RENAME SELECTED FILE')
        shot('pad-rename', (352, 232))
        pad('A'); assert (media/'BA.PRG').exists() and not (media/'AB.PRG').exists()
        pad('Y','B'); assert (media/'BA.PRG').exists()
        pad('Y','A'); assert not (media/'BA.PRG').exists()
        pad('RB','Right','RB','Y','Up','Right','Up','Up','A')
        assert (media/'AB.TAP').exists()
        pad('X','Down','X'); key('Ctrl+F4'); call('expect AB.TAP'); pad('B')

        # Delete the active snapshot directly and cancel without leaving States.
        pad('B','Left','Left','Left')
        call('expect STATES')
        pad('Y'); call('expect DELETE CURRENT SNAPSHOT?')
        pad('B'); call('expect STATES'); assert (work/'states/c64-pal/slot-4.rcs').exists()
        pad('Y','A'); assert not (work/'states/c64-pal/slot-4.rcs').exists()

        call('poke 3000 42')
        pad('Start+Y'); call('expect Cold boot')
        assert call('peek 3000')['data'] != '42'
        call('frames 150'); call('expect READY.')
        pad('L3'); call('expect Paused'); pad('L3'); call('expect Running')
        call('frames 130')
        pad('R3'); call('expect D8:'); pad('R3')
        pad('Start+R3'); call('expect screenshots')
        pad('Start+X')
        assert proc.wait(timeout=10)==0
        print('Controller menus, file and snapshot management, cold boot and exit passed')
    finally:
        if proc.poll() is None: call('quit')
        proc.wait(timeout=10); log.close()


if __name__ == '__main__': main()
