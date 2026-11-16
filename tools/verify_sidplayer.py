"""Native headless SID player: pinned guest tunes, controls and exact continuation."""
from __future__ import annotations
import argparse
import array
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess
import wave
from verify_states import machine_bytes

ROOT = Path(__file__).resolve().parents[1]

def model(path: Path) -> str:
    data = path.read_bytes(); at = 12
    at += 2+struct.unpack_from('<H', data, at)[0]
    length = struct.unpack_from('<H', data, at)[0]
    return data[at+2:at+2+length].decode('ascii')

def pcm(path: Path) -> bytes:
    with wave.open(str(path), 'rb') as audio:
        assert audio.getnchannels() == 2 and audio.getsampwidth() == 2 and audio.getframerate() == 48000
        return audio.readframes(audio.getnframes())

def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--workspace', type=Path, default=ROOT)
    parser.add_argument('--binary', type=Path)
    args = parser.parse_args(); root = args.workspace.resolve()
    binary = (args.binary or root/'work/build-x64/rec64.exe').resolve()
    work = root/'work/p8/sid-acceptance'; work.mkdir(parents=True, exist_ok=True)
    pins = json.loads((root/'docs/sources.lock.json').read_text())['sid_test_tunes']
    for name in ('Commando.sid', 'Arkanoid.sid'):
        source = root/'work/p8'/name
        assert hashlib.sha256(source.read_bytes()).hexdigest() == pins[name]['sha256']
        shutil.copyfile(source, work/name)
    log = (work/'commands.jsonl').open('w', encoding='utf8')

    class Client:
        def __init__(self, name: str):
            config = work/f'{name}.yaml'
            # Begin with the opposite clock/model to exercise the tune hints.
            config.write_text('machine: {model: c64c-ntsc}\nvideo: {look: plain, filter: nearest}\n')
            self.proc = subprocess.Popen([str(binary), '--hidden', '--config', str(config), '--control', 'stdin'], cwd=root,
                                         stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, bufsize=1)
        def call(self, command: str):
            self.proc.stdin.write(command+'\n'); self.proc.stdin.flush()
            line = self.proc.stdout.readline()
            assert line, (command, self.proc.poll(), self.proc.stderr.read())
            reply = json.loads(line); log.write(json.dumps({'command':command, 'reply':reply})+'\n'); log.flush()
            assert reply['ok'], (command, reply)
            return reply
        def save(self, name: str) -> Path:
            path = work/f'{name}.rcs'; self.call(f'state save {path.as_posix()}'); return path
        def text(self) -> str: return self.call('text')['text']
        def close(self):
            try:
                if self.proc.poll() is None: self.call('quit')
                _, err = self.proc.communicate(timeout=10); assert self.proc.returncode == 0, err
            finally:
                if self.proc.poll() is None: self.proc.kill(); self.proc.communicate()

    results = []
    for name, title, author in [('Commando.sid','COMMANDO','ROB HUBBARD'), ('Arkanoid.sid','ARKANOID','MARTIN GALWAY')]:
        c = Client(Path(name).stem.lower())
        try:
            if name == 'Commando.sid':
                c.call('key Ctrl+F1'); c.call('expect FILES AND FOLDERS'); c.call('type Commando'); c.call('enter')
            else: c.call(f'drop {(work/name).as_posix()}')
            c.call('expect SID PLAYER'); c.call(f'expect {title}'); c.call(f'expect {author}')
            c.call(f'audio {(work/(name+".wav")).as_posix()}'); c.call('frames 600'); c.call('audio off')
            sound = pcm(work/(name+'.wav')); samples = array.array('h', sound)
            peak = max(map(abs, samples)); nonzero = sum(abs(v)>64 for v in samples)
            assert peak>256 and nonzero>48000, (name, peak, nonzero)
            display = c.text(); assert '00:00' not in display, display
            c.call(f'shot {(work/(name+".png")).as_posix()}')
            # Exact deterministic native machine and PCM continuation.
            base = c.save(name+'-base')
            assert model(base) == 'c64-pal', 'PAL/6581 hint did not replace the initial C64C NTSC model'
            for take in ('expected','resumed'):
                if take == 'resumed': c.call(f'state load {base.as_posix()}')
                c.call(f'audio {(work/(name+"-"+take+".wav")).as_posix()}'); c.call('frames 120'); c.call('audio off')
                c.save(name+'-'+take)
            assert pcm(work/(name+'-expected.wav')) == pcm(work/(name+'-resumed.wav')), name+' PCM continuation diverged'
            assert machine_bytes(work/(name+'-expected.rcs')) == machine_bytes(work/(name+'-resumed.rcs')), name+' machine continuation diverged'
            c.call('key Space'); c.call('expect PAUSED'); before = c.save(name+'-paused-before'); c.call('frames 70'); after = c.save(name+'-paused-after')
            assert machine_bytes(before) == machine_bytes(after), name+' pause advanced state'
            c.call('key Space'); c.call('expect PLAYING')
            old = re.search(r'SUB-TUNE (\d+) / (\d+)', c.text()); assert old
            if int(old[2])>1:
                c.call('key Right'); c.call(f'expect SUB-TUNE {int(old[1])+1} /'); c.call('expect 00:00'); c.call('frames 180')
                c.call('key Left'); c.call(f'expect SUB-TUNE {old[1]} /'); c.call('expect 00:00')
            c.call('key Escape'); c.call('frames 200'); c.call('expect READY.')
            assert 'SID PLAYER' not in c.text()
            results.append({'file':name,'peak':peak,'samples_above_64':nonzero,'pcm_bytes':len(sound),'screen':display})
        finally: c.close()
    log.close()
    result = {'status':'passed','version':subprocess.check_output([str(binary),'--version'],text=True).strip(),'checks':['native browser/drop metadata','PAL/6581 hints from opposite initial model','pinned PSID/RSID non-silent audio','subsong/time/pause/close','exact machine and PCM snapshot continuation'],'tunes':results}
    (work/'result.json').write_text(json.dumps(result,indent=2)+'\n')
    print('Native SID player passed: real PSID/RSID sound, controls and exact snapshot/PCM continuation')

if __name__ == '__main__': main()
