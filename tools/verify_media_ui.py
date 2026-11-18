"""One headless flow for setup, BASIC saves, file management and snapshots."""
from pathlib import Path
import argparse
import json
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, default=ROOT/'work/build-x64/rec64.exe')
    args = parser.parse_args()
    work = ROOT/'work/verify-media-ui'
    work.mkdir(parents=True, exist_ok=True)
    media = work/'media'
    media.mkdir(exist_ok=True)
    for name in ('program.prg', 'program.tap', 'program.d64', 'renamed.prg'):
        (media/name).unlink(missing_ok=True)
    config = work/'rec64.yaml'
    config.write_text('# Preserve this comment\nmachine: {model: c64-pal}\nui: {setup_page: machine}\n')
    log = (work/'commands.jsonl').open('w')

    def start():
        return subprocess.Popen([str(args.binary.resolve()), '--hidden', '--config', str(config), '--control', 'stdin'],
                                cwd=ROOT, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

    proc = start()

    def call(command):
        proc.stdin.write(command+'\n'); proc.stdin.flush()
        line = proc.stdout.readline()
        assert line, (command, proc.poll(), proc.stderr.read())
        reply = json.loads(line)
        log.write(json.dumps({'command': command, 'reply': reply})+'\n'); log.flush()
        assert reply['ok'], (command, reply)
        return reply

    def key(name, count=1):
        for _ in range(count): call('key '+name)

    def text(): return call('text')['text']

    def shot(name): call('shot '+(work/f'{name}.png').as_posix())

    def save(kind):
        key('Ctrl+F2'); key('TAB')
        for _ in range((kind-save.kind)%3): key('RETURN')
        save.kind = kind
        key('TAB'); key('Ctrl+A'); call('type program.'+('prg', 'tap', 'd64')[kind])
        shot('save-'+str(kind)); key('RETURN'); call('expect Saved')

    save.kind = 0
    try:
        call('frames 180'); call('expect READY.')
        key('F12'); t = text(); assert 'VIDEO STANDARD' in t and 'SID TYPE' in t and 'C64 PAL' not in t, t
        key('RETURN'); key('DOWN'); key('RETURN'); key('F12'); call('frames 180')
        assert 'c64c-ntsc' in config.read_text()
        key('F12'); shot('machine'); key('RETURN'); key('F12'); call('frames 180')
        assert 'c64-ntsc' in config.read_text()
        # Return to PAL and show the new palette label.
        key('F12'); key('DOWN'); key('RETURN'); key('RIGHT', 2); call('expect PALETTE'); assert 'COLOURS' not in text(); shot('display')
        key('LEFT'); key('DOWN', 7); call('expect DEFAULT MEDIA FOLDER'); shot('media')
        key('RETURN'); call('expect FOLDER'); call('type media'); key('RETURN'); shot('folder-selector'); key('SPACE')
        call('expect DEFAULT MEDIA FOLDER'); key('F12'); call('frames 180')
        assert 'media_folder' in config.read_text() and config.read_text().startswith('# Preserve this comment')
        key('Ctrl+F3'); t=text(); assert 'LOAD IMAGE' in t and 'SAVE FILE' not in t and 'EJECT' not in t, t; shot('empty-drive')
        key('RIGHT'); t=text(); assert not any(label in '\n'.join(t.splitlines()[5:12]) for label in ('SAVE FILE','PLAY ', 'STOP ', 'REWIND', 'RECORD', 'EJECT')), t; shot('empty-tape'); key('ESCAPE')
        key('Ctrl+F2'); call('expect No BASIC program to save')
        call('type 10 PRINT 42'); call('enter'); call('frames 3')
        key('Ctrl+F4'); t=text(); assert t.index('LOAD IMAGE')<t.index('SAVE FILE') and 'EJECT' not in t, t; key('ESCAPE')
        save(0); original=(media/'program.prg').read_bytes(); assert original[:2]==b'\x01\x08' and b'42' in original
        save(1); assert (media/'program.tap').stat().st_size>1000
        key('Ctrl+F4'); t=text(); assert t.index('LOAD IMAGE')<t.index('SAVE FILE')<t.index('PLAY ') and ' STOP ' not in '\n'.join(t.splitlines()[5:12]), t; shot('tape-controls'); key('ESCAPE')
        call('frames 100'); key('F9'); call('frames 160'); assert 'S-' in text(), text(); shot('after-save-idle'); key('F9')
        save(2)
        # Extract the one actual PRG from the saved disk and compare it byte for byte.
        disk=(media/'program.d64').read_bytes(); directory=disk[(17*21+1)*256:(17*21+2)*256]
        assert directory[2]==0x82
        track,sector=directory[3:5]; data=bytearray()
        while track:
            sectors=lambda t: 21 if t<=17 else 19 if t<=24 else 18 if t<=30 else 17
            offset=(sum(sectors(t) for t in range(1,track))+sector)*256
            block=disk[offset:offset+256]
            data.extend(block[2:256 if block[0] else block[1]+1]); track,sector=block[:2]
        assert data==original
        # Only files of the chosen type remain; folders still allow navigation.
        key('Ctrl+F2'); t=text(); assert 'PROGRAM.D64' in t and 'PROGRAM.PRG' not in t and 'PROGRAM.TAP' not in t, t
        key('TAB'); key('RETURN'); save.kind=0; t=text(); assert 'PROGRAM.PRG' in t and 'PROGRAM.D64' not in t, t; key('TAB',2)
        call('type program.prg'); key('F2'); call('type renamed'); key('RETURN'); assert (media/'renamed.prg').read_bytes()==original and not (media/'program.prg').exists()
        shot('renamed'); key('DELETE'); shot('delete-file'); key('ESCAPE'); assert (media/'renamed.prg').exists()
        key('DELETE'); key('RETURN'); assert not (media/'renamed.prg').exists(); key('ESCAPE')
        # The saved TAP contains a real program directory entry.
        call('drop '+(media/'program.tap').as_posix()); call('expect PROGRAM'); shot('saved-tape-content'); key('ESCAPE')
        key('F12'); key('RIGHT',5); call('expect STATES'); key('DOWN',10); key('RETURN')
        slot=work/'states/c64-pal/slot-10.rcs'; assert slot.exists(); shot('states')
        key('DOWN',2); key('RETURN'); key('ESCAPE'); assert slot.exists()
        key('RETURN'); shot('delete-state'); key('RETURN'); assert not slot.exists(); assert 'EMPTY SLOT' in text(); shot('empty-state')
        key('RIGHT'); call('expect NEW IN'); shot('about'); key('F12')
        call('quit'); proc.wait(timeout=10)
        # The chosen folder survives a fresh process.
        proc=start(); key('Ctrl+F1'); assert 'PROGRAM.D64' in text() and 'PROGRAM.TAP' in text(), text()
        shot('persisted-folder')
        (work/'result.json').write_text(json.dumps({'status':'passed','version':(ROOT/'src/VERSION').read_text().strip(),
            'checks':['labels and independent model/video','snapshot delete/cancel','context controls and BASIC gating',
                      'PRG/TAP/D64 saves and idle tape','file filtering, rename, delete/cancel','persistent folder selector','About logo and changes']},indent=2)+'\n')
        print('Setup, BASIC media saves, file management, state deletion, folder persistence and About passed')
    finally:
        if proc.poll() is None: call('quit')
        proc.wait(timeout=10); log.close()


if __name__=='__main__': main()
