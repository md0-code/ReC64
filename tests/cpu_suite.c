/* Reads the losslessly converted SingleStepTests vectors, no model of its own. */
#include "mos6510.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static FILE *input;
static unsigned get(unsigned bytes) {unsigned n=0;for(unsigned i=0;i<bytes;++i){int v=fgetc(input);if(v==EOF){fputs("Short vector file\n",stderr);exit(2);}n|=(unsigned)v<<(i*8);}return n;}
int main(int argc,char **argv) {
    if(argc!=2)return 2;input=fopen(argv[1],"rb");if(!input)return 2;
    static uint8_t ram[65536];unsigned long count=0;
    while(fgetc(input)!=EOF) {
        rc_mos6510 c;rc_mos6510_init(&c,NULL,RC_MOS6502);c.resetting=0;
        c.pc=(uint16_t)get(2);c.s=(uint8_t)get(1);c.a=(uint8_t)get(1);c.x=(uint8_t)get(1);c.y=(uint8_t)get(1);c.p=(uint8_t)get(1);
        memset(ram,0,sizeof ram);unsigned n=get(2);
        for(unsigned i=0;i<n;++i){unsigned a=get(2);ram[a]=(uint8_t)get(1);}
        const uint16_t pc=(uint16_t)get(2);const uint8_t s=(uint8_t)get(1),a=(uint8_t)get(1),x=(uint8_t)get(1),y=(uint8_t)get(1),p=(uint8_t)get(1);
        n=get(2);uint16_t addr[64];uint8_t data[64];if(n>64)return 2;
        for(unsigned i=0;i<n;++i){addr[i]=(uint16_t)get(2);data[i]=(uint8_t)get(1);}
        rc_mos6510_start(&c);const unsigned cycles=get(1);
        for(unsigned i=0;i<cycles;++i) {
            const unsigned ca=get(2),cv=get(1),wr=get(1);const uint8_t got=c.pins.write?c.pins.data:ram[c.pins.address];
            if(c.pins.address!=ca||got!=cv||c.pins.write!=wr) {
                fprintf(stderr,"%s vector %lu op %02X cycle %u: bus %04X %02X %c expected %04X %02X %c\n",argv[1],count,c.opcode,i,c.pins.address,got,c.pins.write?'W':'R',ca,cv,wr?'W':'R');return 1;
            }
            if(wr)ram[ca]=c.pins.data;
            rc_mos6510_clock(&c,got,1);
        }
        if(c.pc!=pc||c.s!=s||c.a!=a||c.x!=x||c.y!=y||c.p!=p) {
            fprintf(stderr,"%s vector %lu: PC %04X/%04X S %02X/%02X A %02X/%02X X %02X/%02X Y %02X/%02X P %02X/%02X\n",argv[1],count,c.pc,pc,c.s,s,c.a,a,c.x,x,c.y,y,c.p,p);return 1;
        }
        for(unsigned i=0;i<n;++i)if(ram[addr[i]]!=data[i]){fprintf(stderr,"RAM mismatch\n");return 1;}
        ++count;
    }
    fclose(input);printf("%lu vectors passed\n",count);return 0;
}
