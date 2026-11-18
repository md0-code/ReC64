"""Headless disk browser, serial SAVE, sound switch and custom-loader release check."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import urllib.request
import wave
import zipfile
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]

def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, default=ROOT/'work/build-x64/rec64.exe')
    args = parser.parse_args()
    binary = args.binary.resolve()
    work = ROOT/'work/verify-drive'
    work.mkdir(parents=True, exist_ok=True)
    pin = json.loads((ROOT/'docs/sources.lock.json').read_text())['test_programs']['krill-loader-v164']
    archive = ROOT/'work/p5/loader-v164.zip'
    archive.parent.mkdir(parents=True, exist_ok=True)
    if not archive.exists():
        archive.write_bytes(urllib.request.urlopen(pin['url'], timeout=45).read())
    assert hashlib.sha256(archive.read_bytes()).hexdigest() == pin['sha256']
    fixture = work/'loadertest.d64'
    with zipfile.ZipFile(archive) as z:
        data = z.read(pin['file'])
    assert hashlib.sha256(data).hexdigest() == pin['file_sha256']
    fixture.write_bytes(data)

    def run(name: str, config: str, commands: list[str]) -> None:
        cfg = work/f'{name}.yaml'
        cfg.write_text(config, encoding='utf8')
        script = work/f'{name}.script'
        script.write_text('\n'.join(commands)+'\n', encoding='utf8')
        result = subprocess.run([str(binary), '--hidden', '--config', str(cfg), '--script', str(script)],
                                cwd=ROOT, capture_output=True, text=True, timeout=240)
        (work/f'{name}.stdout').write_text(result.stdout, encoding='utf8')
        (work/f'{name}.stderr').write_text(result.stderr, encoding='utf8')
        assert result.returncode == 0, (name, result.stdout, result.stderr)

    serial = work/'serial.d64'
    shutil.copyfile(fixture, serial)
    config = 'machine: {model: c64-pal, options: {drive_device: "11"}}\nui: {setup_page: media}\nmedia: {disk: {fast: false}}\n'
    run('serial', config, [
        'frames 200', 'expect READY.', f'insert disk {serial.as_posix()}',
        'type 10PRINT"SERIAL SAVE"', 'enter', 'type SAVE"SECOND",11', 'enter',
        'frames 200', 'expect SAVING SECOND', f'drop {serial.as_posix()}',
        'expect IMAGE CONTENTS', 'type SECOND', 'expect SECOND',
        f'audio {(work/"drive.wav").as_posix()}', 'enter', 'until 40 SERIAL SAVE',
        'frames 100', 'audio off', f'shot {(work/"loaded.png").as_posix()}',
        'key Alt+F', 'expect Drive fast loading on',
        'frames 5',
        f'audio {(work/"muted.wav").as_posix()}',
        'type LOAD"SECOND",11', 'enter', 'frames 200', 'audio off',
        'key Alt+S', 'expect Drive sounds off', 'key Alt+S', 'expect Drive startup sound on',
        'key Alt+F', 'expect Drive fast loading off',
        'key Alt+S', 'expect Drive sounds off', 'key Alt+S', 'expect Drive sounds on',
        'key F12', 'expect DEVICE',
        'expect FAST LOAD', f'shot {(work/"media.png").as_posix()}', 'key F12'
    ])
    def peak(path: Path) -> int:
        with wave.open(str(path), 'rb') as wav:
            assert (wav.getnchannels(), wav.getsampwidth(), wav.getframerate()) == (2, 2, 48000)
            samples = wav.readframes(wav.getnframes())
        assert samples
        return max(abs(v[0]) for v in struct.iter_unpack('<h', samples))
    startup = work/'startup.wav'
    run('startup', 'media: {disk: {fast: true}}\n',
        [f'audio {startup.as_posix()}', 'frames 150', 'audio off', 'expect READY.'])
    audible, muted, self_test = peak(work/'drive.wav'), peak(work/'muted.wav'), peak(startup)
    assert audible > 0 and muted == 0 and self_test > 0, (audible, muted, self_test)
    for fast in (False, True):
        name = f'loader-{int(fast)}'
        config = ('machine: {model: c64-pal}\nmedia:\n  disk: {fast: '+str(fast).lower()+', write_protect: true}\n')
        run(name, config, ['frames 200', f'drop {fixture.as_posix()}', 'expect IMAGE CONTENTS',
                          'enter', 'until 100 INSTALLING LOADER', 'frames 1000',
                          f'shot {(work/(name+".png")).as_posix()}'])
    off, on = work/'loader-0.png', work/'loader-1.png'
    assert off.read_bytes() == on.read_bytes(), 'Fast setting changed custom-loader output'
    with Image.open(off) as image:
        assert len(image.getcolors(image.width*image.height)) >= 4, 'Custom-loader bitmap was not reached'
    result = {'status':'passed', 'version':subprocess.check_output([str(binary), '--version'], text=True).strip(),
              'checks':['device 11 serial SAVE', 'same-image flush and browser reload', 'disk entry auto-load/RUN',
                        'Alt+F and Alt+S', 'audible self-test with fast load and empty drive', 'fast disk load muted',
                        'Krill custom-loader bitmap identical with fast off/on'],
              'drive_audio_peak':audible, 'muted_audio_peak':muted, 'self_test_audio_peak':self_test,
              'custom_loader_png_sha256':hashlib.sha256(off.read_bytes()).hexdigest()}
    (work/'result.json').write_text(json.dumps(result, indent=2)+'\n')
    print('Drive release passed: serial SAVE/browser reload, sound switch and identical custom-loader bitmap')

if __name__ == '__main__':
    main()
