/* One headless browser flow: files, image contents, save and overwrite. */
#include "../src/app/browser.h"
#include "../src/core/media/d64.h"
#include "../src/core/media/t64.h"
#include "../src/core/palette.h"
#include "../src/host/host.h"
#include "../src/platform/png.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static int write_file(const char *path,const void *data,size_t size){FILE *file=fopen(path,"wb");if(!file)return 0;const int ok=fwrite(data,1,size,file)==size;return fclose(file)==0&&ok;}
static int snapshot(rc_browser *b,const char *path){rc_frame frame;rc_browser_draw(b,rc_palette_builtin("color-corrected"),&frame);uint32_t pixels[384*272];rc_frame_argb(&frame,pixels);return rc_png_write(path,pixels,384,272,384*4,0);}
int main(void){
    CHECK(rc_host_make_folders(RC_WORK "/browser/child"));char error[128]={0};
    const uint8_t prg[]={1,8,0x42};CHECK(write_file(RC_WORK "/browser/existing.prg",prg,sizeof prg));CHECK(write_file(RC_WORK "/browser/hidden.txt",prg,sizeof prg));CHECK(write_file(RC_WORK "/browser/future.crt",prg,sizeof prg));
    uint8_t g64[12+84*8]={0};memcpy(g64,"GCR-1541",8);g64[9]=84;rc_image_put16(g64+10,7928);CHECK(write_file(RC_WORK "/browser/raw.g64",g64,sizeof g64));
    rc_d64 disk={0};CHECK(rc_d64_create(&disk,NULL,NULL,error,sizeof error));uint8_t sector[256]={0};sector[1]=255;sector[2]=0x82;sector[3]=1;memset(sector+5,0xa0,16);sector[5]=0x12;sector[6]=0xc1;sector[7]=0xc2;sector[8]=0x92;sector[30]=1;sector[34]=0x81;sector[35]=1;memset(sector+37,0xa0,16);memcpy(sector+37,"NOT A PROGRAM",13);sector[62]=1;CHECK(rc_d64_write_sector(&disk,18,1,sector));CHECK(write_file(RC_WORK "/browser/directory.d64",disk.image->data,disk.image->size));rc_d64_close(&disk);
    const uint8_t bytes[]={0x42};rc_t64_program programs[2]={{0x801,bytes,1,{0}},{0xc000,bytes,1,{0}}};memcpy(programs[0].name,"FIRST",5);memcpy(programs[1].name,"SECOND",6);rc_t64 tape={0};CHECK(rc_t64_create(&tape,programs,2,NULL,error,sizeof error));CHECK(write_file(RC_WORK "/browser/programs.t64",tape.image->data,tape.image->size));rc_t64_close(&tape);
    rc_browser *b=calloc(1,sizeof *b);CHECK(b);rc_browser_init(b);CHECK(rc_browser_open(b,RC_WORK "/browser",0,error,sizeof error));
    size_t supported=0,disabled=0;for(size_t i=0;i<b->count;++i){CHECK(strcmp(b->items[i].name,"hidden.txt"));if(b->items[i].disabled)++disabled;if(!strcmp(b->items[i].name,"existing.prg"))++supported;}CHECK(supported==1&&disabled==1);CHECK(snapshot(b,RC_WORK "/browser/files.png"));
    rc_browser_select(b,"raw.g64");CHECK(!b->items[b->selected].disabled&&rc_browser_key(b,"SPACE")==RC_BROWSER_INSERT&&b->result.entry==UINT32_MAX);
    CHECK(rc_browser_key(b,"RETURN")==RC_BROWSER_LOAD&&strstr(b->result.path,"raw.g64"));
    rc_browser_input(b,"child");CHECK(!strcmp(b->items[b->selected].name,"child"));CHECK(rc_browser_key(b,"RETURN")==RC_BROWSER_NONE);CHECK(strstr(b->path,"child"));CHECK(rc_browser_key(b,"LEFT")==RC_BROWSER_NONE);CHECK(!strstr(b->path,"child"));
    CHECK(rc_browser_file(b,RC_WORK "/browser/directory.d64",error,sizeof error)==RC_BROWSER_NONE&&b->contents&&b->count==2);char text[2048];rc_browser_text(b,text,sizeof text);CHECK(strstr(text,"AB")&&strstr(text,"PRG"));CHECK(b->tui.paper[5][9]==6&&b->tui.ink[5][9]==14);CHECK(snapshot(b,RC_WORK "/browser/directory.png"));
    CHECK(b->items[1].disabled);rc_browser_pad(b,RC_PAD_DOWN);rc_browser_key(b,"END");rc_browser_input(b,"NOT A PROGRAM");
    CHECK(b->selected==0&&rc_browser_key(b,"RETURN")==RC_BROWSER_LOAD&&b->result.entry==0);
    rc_browser_text(b,text,sizeof text);CHECK(b->tui.ink[6][9]==12);CHECK(rc_browser_key(b,"SPACE")==RC_BROWSER_INSERT&&b->result.entry==UINT32_MAX);
    CHECK(rc_browser_file(b,RC_WORK "/browser/programs.t64",error,sizeof error)==RC_BROWSER_NONE&&b->contents&&b->count==2);CHECK(rc_browser_key(b,"DOWN")==RC_BROWSER_NONE);CHECK(rc_browser_key(b,"RETURN")==RC_BROWSER_LOAD&&b->result.entry==1);CHECK(rc_browser_key(b,"LEFT")==RC_BROWSER_NONE&&!b->contents);
    CHECK(rc_browser_open(b,RC_WORK "/browser",1,error,sizeof error));CHECK(rc_browser_key(b,"TAB")==RC_BROWSER_NONE&&b->focus==1);CHECK(rc_browser_key(b,"TAB")==RC_BROWSER_NONE&&b->focus==2);CHECK(rc_browser_key(b,"Ctrl+A")==RC_BROWSER_NONE);rc_browser_input(b,"existing.prg");CHECK(rc_browser_key(b,"RETURN")==RC_BROWSER_NONE&&b->confirm);CHECK(snapshot(b,RC_WORK "/browser/overwrite.png"));CHECK(rc_browser_key(b,"ESCAPE")==RC_BROWSER_NONE&&!b->confirm);CHECK(rc_browser_key(b,"RETURN")==RC_BROWSER_NONE&&b->confirm);CHECK(rc_browser_key(b,"Y")==RC_BROWSER_SAVE_PRG&&b->result.overwrite&&strstr(b->result.path,"existing.prg"));
    CHECK(rc_browser_key(b,"TAB")==RC_BROWSER_NONE);CHECK(rc_browser_key(b,"TAB")==RC_BROWSER_NONE&&b->focus==1);CHECK(rc_browser_key(b,"RETURN")==RC_BROWSER_NONE&&b->save_type==1);CHECK(rc_browser_key(b,"TAB")==RC_BROWSER_NONE&&b->focus==2);rc_browser_key(b,"Ctrl+A");rc_browser_input(b,"recording.tap");CHECK(rc_browser_key(b,"RETURN")==RC_BROWSER_SAVE_TAP&&!b->result.overwrite);CHECK(snapshot(b,RC_WORK "/browser/save.png"));
    rc_browser_key(b,"TAB");rc_browser_key(b,"TAB");rc_browser_key(b,"RETURN");rc_browser_key(b,"TAB");rc_browser_key(b,"Ctrl+A");rc_browser_input(b,"blank.d64");CHECK(rc_browser_key(b,"RETURN")==RC_BROWSER_SAVE_D64);CHECK(rc_browser_key(b,"ESCAPE")==RC_BROWSER_CLOSE);
    rc_browser_close(b);free(b);puts("Browser folders, PETSCII image contents, save actions and overwrite confirmation OK");return 0;
}
