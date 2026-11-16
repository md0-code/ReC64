/* Drive board check, using the unmodified DOS job queue. */
#include "../hw/c1541.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s pc=%04X job=%02X track=%u phase=%u motor=%u pcr=%02X bit=%u latch=%02X\n",__LINE__,#x,d->cpu.pc,d->ram[0],d->halftrack,d->phase,d->motor,d->via[1].pcr,d->bit_count,d->read_latch);return 1;}}while(0)
static void tick(rc_c1541 *d,unsigned cycles){while(cycles--)rc_c1541_tick(d);}
int main(void){
    rc_c1541 *d=calloc(1,sizeof *d);if(!d)return 1;uint8_t rom[16384];FILE *f=fopen(RC_ROM_DIR "/dos1541-325302-01+901229-05.bin","rb");CHECK(f);CHECK(fread(rom,1,sizeof rom,f)==sizeof rom);fclose(f);
    rc_c1541_init(d,rom,8);tick(d,3000000);CHECK(!d->cpu.jammed&&d->via[0].ddr[1]==0x1a&&d->via[1].ddr[1]==0x6f);printf("Boot pc=%04X job=%02X DDR=%02X pcr=%02X\n",d->cpu.pc,d->ram[0],d->via[1].ddr[1],d->via[1].pcr);
    char error[128];uint8_t name[16];memset(name,0xa0,16);memcpy(name,"PHASE FIVE",10);const uint8_t id[2]={'P','5'};rc_d64 disk;CHECK(rc_d64_create(&disk,name,id,error,sizeof error));CHECK(rc_c1541_mount(d,disk.image,error,sizeof error));
    d->ram[0x06]=18;d->ram[0x07]=0;d->ram[0x00]=0xb0;tick(d,1000000);CHECK(d->ram[0]==1);
    d->ram[0x00]=0x80;tick(d,1000000);CHECK(d->ram[0]==1);CHECK(!memcmp(d->ram+0x300,rc_d64_sector(&disk,18,0),256));
    d->ram[0x06]=18;d->ram[0x07]=1;d->ram[0]=0x80;tick(d,13001);
    rc_state_writer a,b,c;rc_sw_init(&a);rc_sw_header(&a,"drive","1541",NULL,0);rc_sw_begin(&a,"DRV ",0);rc_c1541_save(d,&a);rc_sw_end(&a);CHECK(rc_sw_finish(&a));tick(d,17003);
    rc_sw_init(&b);rc_sw_header(&b,"drive","1541",NULL,0);rc_sw_begin(&b,"DRV ",0);rc_c1541_save(d,&b);rc_sw_end(&b);CHECK(rc_sw_finish(&b));
    rc_state_reader r;CHECK(rc_sr_open(&r,a.data,a.size,error,sizeof error)&&rc_sr_chunk(&r,"DRV ",0)&&rc_c1541_load(d,&r));tick(d,17003);
    rc_sw_init(&c);rc_sw_header(&c,"drive","1541",NULL,0);rc_sw_begin(&c,"DRV ",0);rc_c1541_save(d,&c);rc_sw_end(&c);CHECK(rc_sw_finish(&c));CHECK(b.size==c.size&&!memcmp(b.data,c.data,b.size));rc_sw_free(&a);rc_sw_free(&b);rc_sw_free(&c);
    tick(d,1000000);CHECK(d->ram[0]==1);memset(d->ram+0x300,0x37,256);d->ram[0x06]=18;d->ram[0x07]=2;d->ram[0]=0x90;tick(d,1500000);CHECK(d->ram[0]==1);CHECK(rc_c1541_flush(d));CHECK(rc_d64_sector(&disk,18,2)[0]==0x37&&disk.image->dirty);
    rc_iec bus;rc_iec_init(&bus);rc_iec_output(&bus,0,RC_IEC_ALL&~RC_IEC_ATN);rc_c1541_iec_input(d,rc_iec_lines(&bus));rc_iec_output(&bus,1,rc_c1541_iec_output(d));CHECK(!(rc_iec_lines(&bus)&RC_IEC_DATA));
    puts("1541 ROM boot, physical GCR read/write, partial state and IEC acknowledge passed");rc_c1541_close(d);rc_d64_close(&disk);free(d);return 0;
}
