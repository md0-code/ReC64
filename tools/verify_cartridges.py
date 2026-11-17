"""Headless C64 startup for original mapper fixtures and file-backed EasyFlash."""
from pathlib import Path
import argparse
import json
import struct
import subprocess
from verify_states import machine_bytes

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, default=ROOT/'work/build-x64/rec64.exe')
    args = parser.parse_args()
    work = ROOT/'work/p7/cartridges'; work.mkdir(parents=True, exist_ok=True)
    binary = args.binary.resolve()
    names = {'normal8k':'NORMAL 8K', 'normal16k':'NORMAL 16K', 'ultimax':'ULTIMAX',
             'simonsbasic':'SIMONS BASIC', 'ocean':'OCEAN', 'funplay':'FUN PLAY',
             'supergames':'SUPER GAMES', 'system3':'SYSTEM 3', 'dinamic':'DINAMIC',
             'zaxxon':'ZAXXON', 'magicdesk':'MAGIC DESK'}
    for name, text in list(names.items())+[('easyflash','EASYFLASH WRITE PASSED')]:
        fixture = ROOT/'work/p7/cart-fixtures'/f'{name}.crt' if name in names else ROOT/'work/p7/easyflash.crt'
        image = work/f'{name}.crt'; image.write_bytes(fixture.read_bytes())
        config = work/f'{name}.yaml'
        config.write_text(json.dumps({'machine':{'model':'c64-pal'},'media':{'cartridge':{'image':image.as_posix(),'write_protect':False}}, 'ui':{'setup_page':'cartridge'}}))
        proc = subprocess.Popen([str(binary),'--hidden','--config',str(config),'--control','stdin'],cwd=ROOT,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        def call(command):
            proc.stdin.write(command+'\n');proc.stdin.flush()
            line=proc.stdout.readline();assert line,(command,proc.poll(),proc.stderr.read())
            reply=json.loads(line);assert reply['ok'],(command,reply)
            return reply
        try:
            call('insert cartridge '+image.as_posix())
            call('frames 100');call('expect '+text);call('shot '+(work/f'{name}.png').as_posix())
            if name=='ultimax':
                saved=work/'cart.rcs'; expected=work/'expected.rcs'; actual=work/'actual.rcs'
                call('state save '+saved.as_posix());call('frames 7');call('state save '+expected.as_posix())
                call('state load '+saved.as_posix());call('frames 7');call('state save '+actual.as_posix())
                assert machine_bytes(expected)==machine_bytes(actual),'Cartridge state diverged'
            if name=='easyflash':
                assert call('peek 02FF')['data']=='5A'
            call('eject cartridge');call('frames 200');call('expect READY.')
            call('quit');_,err=proc.communicate(timeout=10);assert proc.returncode==0,err
        finally:
            if proc.poll() is None:proc.kill();proc.communicate()
        if name=='easyflash':
            data=image.read_bytes();at=64;found=False
            while at<len(data):
                size=struct.unpack_from('>I',data,at+4)[0]
                typ,bank,address,length=struct.unpack_from('>HHHH',data,at+8)
                if bank==17 and address==0x8000:
                    assert typ==2 and length==8192 and data[at+16+0x123]==0x5a;found=True
                at+=size
            assert found,'EasyFlash omitted bank was not persisted'
        print(name+' native startup'+(' and persisted flash write' if name=='easyflash' else '')+' passed',flush=True)
    (work/'result.json').write_text(json.dumps({'status':'passed','mappers':list(names),'easyflash':'native write persisted in appended CRT CHIP','exact_state':'Ultimax continuation'},indent=2)+'\n')

if __name__=='__main__':main()
