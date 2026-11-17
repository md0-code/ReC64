"""One native headless flow for engine selection, real tunes and snapshots."""
from pathlib import Path
import argparse
import array
import hashlib
import json
import struct
import subprocess
import wave

ROOT = Path(__file__).resolve().parents[1]


def chunks(path):
    data = path.read_bytes(); at = 12
    for _ in range(2): at += 2 + struct.unpack_from('<H', data, at)[0]
    at += 4 + 32 * struct.unpack_from('<I', data, at)[0]
    result = {}
    while at + 12 <= len(data):
        tag, instance, size = struct.unpack_from('<4sII', data, at)
        result[tag, instance] = data[at+12:at+12+size]; at += 12 + size
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, default=ROOT/'work/build-x64/rec64.exe')
    args = parser.parse_args(); binary = args.binary.resolve()
    work = ROOT/'work/sid-engines'; work.mkdir(parents=True, exist_ok=True)
    pins = json.loads((ROOT/'docs/sources.lock.json').read_text())['sid_test_tunes']
    transcript = (work/'commands.jsonl').open('w', encoding='utf8')
    results = []
    for model, tune in ((6581, 'Commando.sid'), (8580, 'Arkanoid.sid')):
        source = ROOT/'work/p8'/tune
        assert hashlib.sha256(source.read_bytes()).hexdigest() == pins[tune]['sha256']
        config = work/f'{model}.yaml'; config.write_text('machine: {model: c64-pal}\n')
        proc = subprocess.Popen([str(binary), '--hidden', '--config', str(config), '--control', 'stdin'],
                                cwd=ROOT, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                text=True, bufsize=1)

        def call(command):
            proc.stdin.write(command+'\n'); proc.stdin.flush(); line = proc.stdout.readline()
            assert line, (command, proc.poll(), proc.stderr.read())
            result = json.loads(line); transcript.write(json.dumps({'command': command, 'reply': result})+'\n'); transcript.flush()
            assert result['ok'], (command, result)
            return result

        def key(name): call('key '+name)

        def save(name):
            path = work/f'{model}-{name}.rcs'; call('state save '+path.as_posix()); return path

        def sound():
            key('F12')
            # Page is retained after close, so navigate using its displayed title.
            for _ in range(8):
                if 'SOUND 4/8' in call('text')['text']: break
                key('RIGHT')
            else: raise AssertionError('Sound page unavailable')
            key('DOWN'); key('DOWN')

        try:
            call('frames 200'); legacy = save('resid')
            assert struct.unpack_from('<I', chunks(legacy)[b'SID ',0])[0] == 1
            sound(); call('expect RESID'); key('RETURN'); call('expect BUILT-IN (EXPERIMENTAL)')
            call('shot '+(work/f'{model}-sound.png').as_posix()); key('F12')
            assert 'builtin' in config.read_text()
            call('drop '+source.as_posix())
            if model == 8580:
                key('F12')
                for _ in range(8):
                    if 'MACHINE 1/8' in call('text')['text']: break
                    key('LEFT')
                key('DOWN'); key('RETURN'); key('RETURN'); key('F12')
            call('frames 200'); call('expect SID PLAYER'); before = save('builtin')
            payload = chunks(before)[b'SID ',0]
            assert struct.unpack_from('<II', payload) == (2, int(model == 8580))
            call('peek D400'); call('peek D419 4'); after = save('after-peeks')
            assert chunks(after)[b'SID ',0] == payload, 'Peek changed built-in SID state'
            captures = []
            for take in ('first','restored'):
                call('state load '+before.as_posix()); path = work/f'{model}-{take}.wav'
                call('audio '+path.as_posix()); call('frames 30'); call('audio off')
                with wave.open(str(path)) as audio: captures.append(audio.readframes(audio.getnframes()))
            assert captures[0] == captures[1], 'Built-in PCM continuation differs'
            samples = array.array('h', captures[0]); peak = max(map(abs,samples))
            assert peak > 256 and sum(abs(v)>64 for v in samples)>1000, (model,peak)
            sound(); key('RETURN'); call('expect RESID'); key('F12')
            assert struct.unpack_from('<I', chunks(save('switched-resid'))[b'SID ',0])[0] == 1
            call('state load '+before.as_posix())
            assert struct.unpack_from('<I', chunks(save('auto-builtin'))[b'SID ',0])[0] == 2
            if model == 6581:
                call('state load '+legacy.as_posix())
                assert struct.unpack_from('<I', chunks(save('legacy-resid'))[b'SID ',0])[0] == 1
            results.append({'model':model,'file':tune,'peak':peak,'exact_pcm':True,'engine_restored':True})
            print(f'{model} built-in: {tune}, sound, exact PCM, selector and snapshot engine restore passed', flush=True)
        finally:
            if proc.poll() is None:
                call('quit'); _, error = proc.communicate(timeout=10); assert proc.returncode == 0, error
    transcript.close()
    (work/'result.json').write_text(json.dumps({'status':'passed','results':results},indent=2)+'\n')


if __name__ == '__main__': main()
