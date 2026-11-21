/* Original G64 fixture through the unmodified 1541-II DOS job queue. */
#include "../src/hw/c1541.h"
#include "../src/core/romdb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static rc_c1541 *drive;
static char error[128];
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s (%s) pc=%04X job=%02X\n",__LINE__,#x,error,drive?drive->cpu.pc:0,drive?drive->ram[0]:0);return 1;}}while(0)
static void tick(unsigned cycles){while(cycles--)rc_c1541_tick(drive);}
static int save(rc_state_writer *w){rc_sw_init(w);rc_sw_header(w,"drive","1541-ii",NULL,0);rc_sw_begin(w,"DRV ",0);rc_c1541_save(drive,w);rc_sw_end(w);return rc_sw_finish(w);}
int main(void){
    drive=calloc(1,sizeof *drive);CHECK(drive);uint8_t rom[16384];rc_rom_index rom_index;rc_rom_index_init(&rom_index);const char *rom_folders[]={RC_ROM_DIR};CHECK(rc_rom_index_scan(&rom_index,rom_folders,1));const rc_rom_file *rom_file=rc_rom_index_find(&rom_index,rc_romdb_find("dos1541ii"));CHECK(rom_file);memcpy(rom,rom_file->data,sizeof rom);rc_rom_index_free(&rom_index);
    rc_c1541_init(drive,rom,8);rc_c1541_reset(drive); /* board startup also asserts RESET */
    rc_d64 disk={0};CHECK(rc_d64_create(&disk,NULL,NULL,error,sizeof error)&&rc_c1541_mount(drive,disk.image,error,sizeof error));
    uint8_t header[12+70*8]={0};memcpy(header,"GCR-1541",8);header[9]=70;rc_image_put16(header+10,7928);
    rc_media *image=rc_media_from(RC_WORK "/g64-roundtrip.g64",header,sizeof header);CHECK(image);image->writable=1;rc_g64 g;
    CHECK(rc_g64_open(&g,image,error,sizeof error));
    for(unsigned t=1;t<=35;++t){const rc_gcr_track *raw=&drive->track[t*2];CHECK(rc_g64_write(&g,t*2-2,raw->image->data,(unsigned)raw->image->size,NULL,raw->zone));}
    const uint8_t half[16]={0x52,0x55,0xaa,0xa5},speeds[4]={0x1b,0xe4,0x1b,0xe4};
    CHECK(rc_g64_write(&g,1,half,sizeof half,speeds,0)&&rc_g64_write(&g,83,half,sizeof half,NULL,2));
    CHECK(g.tracks==84&&g.length[83]==16&&rc_g64_speed(&g,1,0)==0&&rc_g64_speed(&g,1,1)==1&&rc_g64_speed(&g,1,2)==2&&rc_g64_speed(&g,1,3)==3);
    rc_media bad=*image;bad.size=12;rc_g64 rejected;CHECK(!rc_g64_open(&rejected,&bad,error,sizeof error));error[0]=0;
    CHECK(rc_c1541_mount(drive,image,error,sizeof error));CHECK(drive->track[2].bits!=drive->track[3].bits&&drive->track[85].bits==128);
    tick(3000000);CHECK(!drive->cpu.jammed&&drive->via[0].ddr[1]==0x1a&&drive->via[1].ddr[1]==0x6f);
    drive->ram[6]=18;drive->ram[7]=0;drive->ram[0]=0xb0;tick(1000000);CHECK(drive->ram[0]==1);
    drive->ram[0]=0x80;tick(1000000);CHECK(drive->ram[0]==1&&!memcmp(drive->ram+0x300,rc_d64_sector(&disk,18,0),256));
    drive->ram[6]=18;drive->ram[7]=2;memset(drive->ram+0x300,0x37,256);drive->ram[0]=0x90;tick(13001);
    rc_state_writer before,expected,actual;CHECK(save(&before));tick(17003);CHECK(save(&expected));rc_state_reader r;
    CHECK(rc_sr_open(&r,before.data,before.size,error,sizeof error)&&rc_sr_chunk(&r,"DRV ",0)&&rc_c1541_load(drive,&r));tick(17003);CHECK(save(&actual));
    CHECK(expected.size==actual.size&&!memcmp(expected.data,actual.data,actual.size));rc_sw_free(&before);rc_sw_free(&expected);rc_sw_free(&actual);
    tick(1500000);CHECK(drive->ram[0]==1&&rc_c1541_flush(drive)&&image->dirty&&rc_media_save(image));
    rc_media *reopened=rc_media_open(image->path,0,error,sizeof error);CHECK(reopened&&rc_c1541_mount(drive,reopened,error,sizeof error));
    memset(drive->ram+0x300,0,256);drive->ram[6]=18;drive->ram[7]=2;drive->ram[0]=0x80;tick(1000000);CHECK(drive->ram[0]==1);
    for(unsigned i=0;i<256;++i)CHECK(drive->ram[0x300+i]==0x37);
    CHECK(rc_g64_open(&g,reopened,error,sizeof error)&&rc_g64_speed(&g,1,1)==1&&!memcmp(reopened->data+g.offset[83]+2,half,sizeof half));
    /* One physical step reaches the distinct half-track; its byte speeds
     * drive the recorded bit rate rather than rounding to a whole track. */
    drive->halftrack=2;drive->phase=0;drive->rotation_track=2;drive->track[2].position=0;drive->track[3].position=0;drive->bit_ticks=0;
    rc_c1541_write(drive,0x1c00,0x65);CHECK(drive->halftrack==3);tick(32);CHECK(drive->track[3].position==8);
    reopened->writable=0;rc_c1541_write(drive,0x1c0c,0xcc);tick(40);CHECK(!drive->dirty_track[3]);
    drive->halftrack=2;drive->phase=0;rc_c1541_write(drive,0x1c00,0x67);CHECK(drive->halftrack==2);
    rc_c1541_write(drive,0x1c00,0x64);CHECK(drive->halftrack==2);rc_c1541_write(drive,0x1c00,0x65);rc_c1541_write(drive,0x1c00,0x66);CHECK(drive->halftrack==4);
    rc_c1541_close(drive);rc_media_close(reopened);rc_media_close(image);rc_d64_close(&disk);free(drive);
    puts("G64 half-tracks, speed maps, 1541-II ROM read/write, file persistence and exact active state OK");return 0;
}
