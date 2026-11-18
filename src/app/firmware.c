#include "firmware.h"
#include <stdio.h>
#include <string.h>
static const struct {const char *key,*label,*size;int drive;} ITEMS[]={
    {"basic","BASIC 901226-01","8K",0},{"kernal","KERNAL 901227-03","8K",0},{"chargen","CHARACTERS 901225-01","4K",0},
    {"dos1541","1541 DOS 325302/901229","16K",1},{"dos1541ii","1541-II DOS 251968-03","16K",1}};
#define ITEM_COUNT (sizeof ITEMS/sizeof ITEMS[0])
static int has(const rc_rom_index *roms,const char *key){return rc_rom_index_find(roms,rc_romdb_find(key))!=NULL;}
static int drives(const rc_rom_index *roms){return has(roms,"dos1541")+has(roms,"dos1541ii");}
int rc_firmware_complete(const rc_rom_index *roms){return has(roms,"basic")&&has(roms,"kernal")&&has(roms,"chargen")&&drives(roms)>0;}
int rc_firmware_drive_choice(const rc_rom_index *roms){return drives(roms)==2;}
void rc_firmware_apply(rc_config *c,const rc_rom_index *roms){
    if(drives(roms)==1&&strcmp(c->model,"test"))rc_config_set_option(c,"drive_model",has(roms,"dos1541")?"1541":"1541-ii");
}
void rc_firmware_report(const rc_rom_index *roms,const char *folder,char *out,size_t cap){
    size_t n=0;
#define ADD(...) do{const int added=snprintf(out+n,cap-n,__VA_ARGS__);if(added>0)n+=(size_t)added<cap-n?(size_t)added:cap-n-1;}while(0)
    if(!cap)return;out[0]=0;
    ADD("ReC64 needs the original Commodore ROMs, which it does not include.\n"
        "Copy them into %s under any file name: each ROM is recognised by its content.\n",folder);
    for(unsigned i=0;i<ITEM_COUNT;++i){const rc_known_rom *rom=rc_romdb_find(ITEMS[i].key);
        ADD("  %-7s %s, %u bytes, SHA-256 %s\n",has(roms,ITEMS[i].key)?"found":"missing",rom->name,(unsigned)rom->size,rom->sha256);}
    ADD("BASIC, KERNAL and the character generator are required, and one of the two drive ROMs.\n");
#undef ADD
}
void rc_firmware_draw(rc_tui *t,const rc_rom_index *roms,const char *folder){
    rc_tui_clear(t);rc_tui_bar(t,0,"FIRMWARE");rc_tui_box(t,2,1,19,38);
    rc_tui_put(t,3,3,"REC64 NEEDS THE ORIGINAL COMMODORE",1,6);
    rc_tui_put(t,4,3,"ROMS, WHICH IT DOES NOT INCLUDE.",1,6);
    const int any_drive=drives(roms)>0;
    for(unsigned i=0;i<ITEM_COUNT;++i){char line[35];const int found=has(roms,ITEMS[i].key);
        snprintf(line,sizeof line,"%-22.22s %3s %7s",ITEMS[i].label,ITEMS[i].size,found?"FOUND":"MISSING");
        /* A drive ROM is only wanted while neither was found. */
        rc_tui_put(t,6+i+(ITEMS[i].drive?2u:0u),3,line,(uint8_t)(found?14:ITEMS[i].drive&&any_drive?12:1),6);}
    rc_tui_put(t,10,3,"AND ONE OF THE TWO DRIVE ROMS:",14,6);
    rc_tui_put(t,14,3,"COPY THEM INTO THE ROMS FOLDER:",1,6);
    /* The end of a long path matters most. */
    size_t length=strlen(folder);if(length>68){folder+=length-68;length=68;}
    char part[35];snprintf(part,sizeof part,"%.34s",folder);rc_tui_put(t,15,3,part,14,6);
    if(length>34)rc_tui_put(t,16,3,folder+34,14,6);
    rc_tui_put(t,18,3,"ANY FILE NAME WORKS: EACH ROM IS",14,6);
    rc_tui_put(t,19,3,"RECOGNISED BY ITS CONTENT.",14,6);
    rc_tui_put(t,22,1,"CHECKSUMS ARE LISTED IN THE README",14,6);
    rc_tui_put(t,24,1,"ENTER/A CHECK AGAIN  ESC/B EXIT",14,6);
}
