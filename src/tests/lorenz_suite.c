/* Run the unmodified test programs on the booted board. No ROM traps. */
#include "../core/machines/c64.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct monitor {rc_c64 *c;int status;} monitor;
static void trace(void *ctx,int kind,uint32_t address,uint32_t value){monitor *m=ctx;
    if(kind==RC_TRACE_WRITE&&address==0xd7ff&&(rc_pla_decode(m->c->pla.inputs)&RC_PLA_I_O))m->status=(int)value;}
static int run(rc_c64 *c,const rc_c64 *boot,const char *path){
    FILE *f=fopen(path,"rb");if(!f){fprintf(stderr,"Cannot read %s\n",path);return 0;}
    uint8_t bytes[65538];const size_t n=fread(bytes,1,sizeof bytes,f);fclose(f);if(n<16)return 0;
    const unsigned address=bytes[0]|bytes[1]<<8;if(n-2>65536u-address)return 0;
    void *engine=c->sid.engine;*c=*boot;c->sid.engine=engine;rc_sid_reset(&c->sid);
    memcpy(c->bus.ram+address,bytes+2,n-2);
    /* BASIC SYS header supplies the program's entry, independent of layout. */
    unsigned entry=0;size_t at=6;while(at<n&&bytes[at]!=0x9e)++at;
    for(++at;at<n&&bytes[at]>='0'&&bytes[at]<='9';++at)entry=entry*10u+bytes[at]-'0';
    if(!entry||entry>65535)return 0;
    c->cpu.pc=(uint16_t)entry;c->cpu.s=0xfd;c->cpu.p=RC_P_I|RC_P_U;c->cpu.resetting=0;c->cpu.jammed=0;rc_mos6510_start(&c->cpu);
    monitor m={.c=c,.status=-1};c->trace=trace;c->trace_ctx=&m;
    const uint64_t start=c->sched.now;
    while(m.status<0&&c->sched.now-start<1000000000ull){if(rc_c64_run(c,1000)!=RC_RUN_OK)break;}
    c->trace=NULL;c->trace_ctx=NULL;
    if(m.status!=0){char text[2048];rc_c64_family.impl->text(c,text,sizeof text);fprintf(stderr,"FAIL %s result=%d cycles=%llu PC=%04X\n%s\n",path,m.status,(unsigned long long)(c->sched.now-start),c->cpu.pc,text);return 0;}
    printf("PASS %s %llu\n",path,(unsigned long long)(c->sched.now-start));fflush(stdout);return 1;
}
int main(int argc,char **argv){
    if(argc<3){fputs("lorenz_suite LIST MODEL\n",stderr);return 2;}const unsigned model=(unsigned)strtoul(argv[2],NULL,10);if(model>3)return 2;
    rc_rom_index roms;rc_rom_index_init(&roms);const char *folders[]={RC_ROM_DIR};if(!rc_rom_index_scan(&roms,folders,1))return 1;
    unsigned options[RC_CONFIG_OPTIONS]={0};rc_machine_setup setup={&rc_c64_family,model,options,&roms,NULL,0};char error[256];
    rc_c64 *c=rc_c64_create(&setup,error,sizeof error),*boot=malloc(sizeof *boot);if(!c||!boot)return 1;
    if(rc_c64_run(c,4000000)!=RC_RUN_OK)return 1;*boot=*c;
    FILE *list=fopen(argv[1],"r");if(!list)return 2;char path[1024];unsigned passed=0;int ok=1;
    while(fgets(path,sizeof path,list)){path[strcspn(path,"\r\n")]=0;if(!*path)continue;if(!run(c,boot,path)){ok=0;fflush(stderr);}else ++passed;}
    fclose(list);rc_c64_family.impl->destroy(c);free(boot);rc_rom_index_free(&roms);printf("Lorenz passed %u\n",passed);return ok?0:1;
}
