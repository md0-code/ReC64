#include "browser.h"
#include "../host/host.h"
#include "../core/media/d64.h"
#include "../core/media/t64.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define LIST_ROWS 15u
static unsigned petscii(unsigned c);
static int same(const char *a,const char *b){for(;*a&&*b;++a,++b)if(toupper((unsigned char)*a)!=toupper((unsigned char)*b))return 0;return *a==*b;}
static int compare_name(const char *a,const char *b){for(;*a&&*b;++a,++b){const int d=toupper((unsigned char)*a)-toupper((unsigned char)*b);if(d)return d;}return (unsigned char)*a-(unsigned char)*b;}
static int compare(const void *a,const void *b){const rc_browser_item *x=a,*y=b;if(x->folder!=y->folder)return x->folder?-1:1;return compare_name(x->name,y->name);}
static unsigned format(const char *name){static const char *const extensions[]={"prg","p00","d64","t64","tap","crt","sid","rcs"};for(unsigned i=0;i<8;++i)if(rc_media_extension_in(name,extensions[i]))return i;return RC_BROWSER_UNKNOWN;}
static void clear_items(rc_browser *b){free(b->items);b->items=NULL;b->count=b->selected=b->top=b->capacity=0;b->search[0]=0;}
static int append_item(rc_browser *b,const rc_browser_item *item){
    if(b->count==b->capacity){const size_t capacity=b->capacity?b->capacity*2:32;if(capacity<b->capacity||capacity>SIZE_MAX/sizeof *b->items){b->listing_failed=1;return 0;}
        rc_browser_item *items=realloc(b->items,capacity*sizeof *items);if(!items){b->listing_failed=1;return 0;}b->items=items;b->capacity=capacity;}
    b->items[b->count++]=*item;return 1;
}
static int join(char *out,size_t cap,const char *folder,const char *name){const size_t n=strlen(folder);const int length=snprintf(out,cap,"%s%s%s",folder,n&&folder[n-1]!='/'&&folder[n-1]!='\\'?"/":"",name);return length>=0&&(size_t)length<cap;}
static int parent(const char *folder,char *out,size_t cap){char path[RC_MEDIA_PATH];if(!rc_host_full_path(folder,path,sizeof path))return 0;size_t n=strlen(path);while(n>1&&(path[n-1]=='/'||path[n-1]=='\\'))path[--n]=0;char *slash=strrchr(path,'/'),*back=strrchr(path,'\\');if(back&&(!slash||back>slash))slash=back;if(!slash)return 0;slash[1]=0;if(rc_host_same_folder(path,folder))return 0;return snprintf(out,cap,"%s",path)>0;}
static int list_entry(void *ctx,const char *name,int folder,uint64_t size){rc_browser *b=ctx;const unsigned kind=folder?RC_BROWSER_UNKNOWN:format(name);if(!folder&&kind==RC_BROWSER_UNKNOWN)return 1;rc_browser_item item={0};if(strlen(name)>=sizeof item.name)return 1;snprintf(item.name,sizeof item.name,"%s",name);item.folder=(unsigned)folder;item.format=kind;item.bytes=size;item.disabled=!folder&&kind>=RC_BROWSER_CRT&&!(kind==RC_BROWSER_STATE&&b->states_enabled)&&!(kind==RC_BROWSER_CRT&&b->cartridges_enabled);return append_item(b,&item);}
static int refresh(rc_browser *b,char *error,size_t cap){clear_items(b);b->contents=0;b->image[0]=0;b->confirm=0;b->listing_failed=0;b->message[0]=0;
    if(b->roots){if(!rc_host_list_roots(list_entry,b)){snprintf(error,cap,"Cannot list filesystem roots");return 0;}}
    else {char up[RC_MEDIA_PATH];if(parent(b->path,up,sizeof up)){rc_browser_item item={0};snprintf(item.name,sizeof item.name,"..");item.folder=1;if(!append_item(b,&item)){snprintf(error,cap,"Out of memory");return 0;}}
        if(!rc_host_list_folder(b->path,list_entry,b)){snprintf(error,cap,"Cannot read folder");return 0;}}
    if(b->listing_failed){snprintf(error,cap,"Out of memory");return 0;}
    if(b->count>1)qsort(b->items,b->count,sizeof *b->items,compare);return 1;
}
void rc_browser_init(rc_browser *b){memset(b,0,sizeof *b);rc_tui_init(&b->tui);snprintf(b->filename,sizeof b->filename,"program.prg");}
void rc_browser_close(rc_browser *b){clear_items(b);b->active=b->confirm=0;}
void rc_browser_set_tape_reader(rc_browser *b,rc_browser_tape_list_fn reader,void *ctx){b->tape_list=reader;b->tape_ctx=ctx;}
void rc_browser_error(rc_browser *b,const char *message){snprintf(b->message,sizeof b->message,"%s",message?message:"");}
int rc_browser_open(rc_browser *b,const char *folder,int save,char *error,size_t cap){
    if(cap)error[0]=0;
    const char *start=folder&&*folder?folder:b->path[0]?b->path:".";char path[RC_MEDIA_PATH];
    if(!rc_host_full_path(start,path,sizeof path)||!rc_host_folder_exists(path)){snprintf(error,cap,"Folder not found");return 0;}
    snprintf(b->path,sizeof b->path,"%s",path);b->save=save!=0;b->focus=0;b->roots=0;b->active=1;b->result=(rc_browser_result){0};b->result.entry=UINT32_MAX;
    return refresh(b,error,cap);
}
static int image_items(rc_browser *b,const rc_browser_content *entries,size_t count,unsigned kind){
    rc_browser_item *items=count?calloc(count,sizeof *items):NULL;if(count&&!items)return 0;
    for(size_t i=0;i<count;++i){items[i].content=entries[i];items[i].format=kind;items[i].bytes=entries[i].length;items[i].disabled=kind==RC_BROWSER_D64&&(entries[i].type&7u)!=2;
        unsigned at=0;for(unsigned j=0;j<16;++j){const unsigned c=entries[i].name[j];if(!c)break;if(c!=0x12&&c!=0x92)items[i].name[at++]=(char)petscii(c);}items[i].name[at]=0;}
    clear_items(b);b->items=items;b->count=b->capacity=count;b->contents=1;b->roots=0;return 1;
}
static unsigned result(rc_browser *b,unsigned action,const char *path,uint32_t entry,int overwrite){
    b->result=(rc_browser_result){0};snprintf(b->result.path,sizeof b->result.path,"%s",path);b->result.entry=entry;b->result.action=action;b->result.overwrite=overwrite;return action;
}
unsigned rc_browser_file(rc_browser *b,const char *path,char *error,size_t cap){
    if(cap)error[0]=0;
    char absolute[RC_MEDIA_PATH];if(!rc_host_full_path(path,absolute,sizeof absolute)){snprintf(error,cap,"Path is too long");return RC_BROWSER_NONE;}
    const unsigned kind=format(absolute);if((kind==RC_BROWSER_STATE&&b->states_enabled)||(kind==RC_BROWSER_CRT&&b->cartridges_enabled))return result(b,RC_BROWSER_LOAD,absolute,UINT32_MAX,0);if(kind>=RC_BROWSER_CRT){snprintf(error,cap,kind==RC_BROWSER_UNKNOWN?"Unsupported file type":"Format is not available yet");return RC_BROWSER_NONE;}
    if(kind<=RC_BROWSER_P00)return result(b,RC_BROWSER_LOAD,absolute,UINT32_MAX,0);
    rc_media *image=rc_media_open(absolute,1,error,cap);if(!image)return RC_BROWSER_NONE;
    rc_browser_content *entries=NULL;size_t count=0;int ok=0;
    if(kind==RC_BROWSER_D64){rc_d64 d={0};if(rc_d64_open(&d,image->data,image->size,error,cap)){
        if(rc_d64_directory(&d,NULL,0,&count,error,cap)){rc_d64_entry *directory=count?calloc(count,sizeof *directory):NULL;entries=count?calloc(count,sizeof *entries):NULL;
            if(!count||(directory&&entries)){if(rc_d64_directory(&d,directory,count,&count,error,cap)){ok=1;for(size_t i=0;i<count;++i){memcpy(entries[i].name,directory[i].name,16);entries[i].length=(uint32_t)directory[i].blocks*254u;entries[i].index=(uint32_t)i;entries[i].type=directory[i].type;}}}else snprintf(error,cap,"Out of memory");free(directory);}
        rc_d64_close(&d);}}
    else if(kind==RC_BROWSER_T64){rc_t64 t={0};if(rc_t64_open(&t,image->data,image->size,error,cap)){count=t.count;entries=count?calloc(count,sizeof *entries):NULL;if(!count||entries){ok=1;for(size_t i=0;i<count;++i){rc_t64_entry e;if(!rc_t64_entry_at(&t,(unsigned)i,&e)){ok=0;snprintf(error,cap,"Invalid T64 directory");break;}memcpy(entries[i].name,e.name,16);entries[i].address=e.address;entries[i].length=e.length;entries[i].index=(uint32_t)i;entries[i].type=e.type;}}else snprintf(error,cap,"Out of memory");rc_t64_close(&t);}}
    else if(kind==RC_BROWSER_TAP){if(!b->tape_list)snprintf(error,cap,"TAP directory reader unavailable");else if(b->tape_list(b->tape_ctx,image,NULL,0,&count,error,cap)){entries=count?calloc(count,sizeof *entries):NULL;if(!count||entries)ok=b->tape_list(b->tape_ctx,image,entries,count,&count,error,cap);else snprintf(error,cap,"Out of memory");}}
    rc_media_close(image);
    if(ok){ok=image_items(b,entries,count,kind);if(ok){snprintf(b->image,sizeof b->image,"%s",absolute);snprintf(b->path,sizeof b->path,"%s",absolute);char *slash=strrchr(b->path,'/'),*back=strrchr(b->path,'\\');if(back&&(!slash||back>slash))slash=back;if(slash)slash[1]=0;b->active=1;b->save=0;b->focus=0;b->confirm=0;b->message[0]=0;}if(!ok)snprintf(error,cap,"Out of memory");else if(!count)rc_browser_error(b,"No loadable entries. Space inserts image");}
    free(entries);return RC_BROWSER_NONE;
}
static void reveal(rc_browser *b){if(b->selected<b->top)b->top=b->selected;if(b->selected>=b->top+LIST_ROWS)b->top=b->selected-LIST_ROWS+1;}
static void set_save_type(rc_browser *b,unsigned kind){static const char *const extensions[]={".prg",".tap",".d64"};b->save_type=kind;char *dot=strrchr(b->filename,'.');if(dot)*dot=0;const size_t n=strlen(b->filename);if(n+4<sizeof b->filename)memcpy(b->filename+n,extensions[kind],5);}
static unsigned save_action(rc_browser *b,int overwrite){static const unsigned actions[]={RC_BROWSER_SAVE_PRG,RC_BROWSER_SAVE_TAP,RC_BROWSER_SAVE_D64};static const char *const extensions[]={"prg","tap","d64"};
    if(!b->filename[0]||!strcmp(b->filename,".")||!strcmp(b->filename,"..")||strpbrk(b->filename,"<>:\"/\\|?*")){rc_browser_error(b,"Choose a valid file name");return RC_BROWSER_NONE;}
    if(!rc_media_extension_in(b->filename,extensions[b->save_type])){const size_t length=strlen(b->filename);if(length+4>=sizeof b->filename){rc_browser_error(b,"File name is too long");return RC_BROWSER_NONE;}b->filename[length]='.';memcpy(b->filename+length+1,extensions[b->save_type],4);}
    char path[RC_MEDIA_PATH];if(!join(path,sizeof path,b->path,b->filename)){rc_browser_error(b,"Path is too long");return RC_BROWSER_NONE;}
    if(rc_host_folder_exists(path)){rc_browser_error(b,"A folder has this name");return RC_BROWSER_NONE;}
    if(rc_host_file_exists(path)&&!overwrite){b->confirm=1;return RC_BROWSER_NONE;}
    b->confirm=0;return result(b,actions[b->save_type],path,UINT32_MAX,overwrite);
}
static int go_parent(rc_browser *b){char error[128],up[RC_MEDIA_PATH];if(b->contents){b->contents=0;if(!refresh(b,error,sizeof error)){rc_browser_error(b,error);return 0;}return 1;}
    if(parent(b->path,up,sizeof up))snprintf(b->path,sizeof b->path,"%s",up);else b->roots=1;
    if(!refresh(b,error,sizeof error)){rc_browser_error(b,error);return 0;}return 1;}
unsigned rc_browser_key(rc_browser *b,const char *name){
    if(!b->active)return RC_BROWSER_NONE;char key[64];size_t n=strlen(name);if(n>=sizeof key)return RC_BROWSER_NONE;for(size_t i=0;i<=n;++i)key[i]=(char)toupper((unsigned char)name[i]);
    if(b->confirm){if(same(key,"Y")||same(key,"RETURN")||same(key,"ENTER"))return save_action(b,1);if(same(key,"N")||same(key,"ESCAPE")||same(key,"BACKSPACE"))b->confirm=0;return RC_BROWSER_NONE;}
    if(same(key,"ESCAPE")||same(key,"F12"))return RC_BROWSER_CLOSE;
    if(same(key,"TAB")&&b->save){b->focus=(b->focus+1)%3;return RC_BROWSER_NONE;}
    if(b->save&&b->focus==1){if(same(key,"LEFT")||same(key,"RIGHT")||same(key,"+")||same(key,"-")||same(key,"RETURN")||same(key,"ENTER")){set_save_type(b,(b->save_type+(same(key,"LEFT")||same(key,"-")?2u:1u))%3);return RC_BROWSER_NONE;}}
    if(b->save&&b->focus==2){if(same(key,"CTRL+A")){b->replace_name=1;return RC_BROWSER_NONE;}if(same(key,"BACKSPACE")||same(key,"DELETE")){size_t length=strlen(b->filename);if(b->replace_name){b->filename[0]=0;b->replace_name=0;}else if(length)b->filename[length-1]=0;return RC_BROWSER_NONE;}if(same(key,"RETURN")||same(key,"ENTER"))return save_action(b,0);return RC_BROWSER_NONE;}
    if(same(key,"LEFT")||same(key,"BACKSPACE")){go_parent(b);return RC_BROWSER_NONE;}
    if(same(key,"HOME")||same(key,"END")||same(key,"UP")||same(key,"DOWN")||same(key,"PAGEUP")||same(key,"PAGEDOWN"))b->search[0]=0;
    if(same(key,"HOME"))b->selected=0;
    else if(same(key,"END")){if(b->count)b->selected=b->count-1;}
    else if(same(key,"UP")){if(b->selected)--b->selected;}
    else if(same(key,"DOWN")){if(b->selected+1<b->count)++b->selected;}
    else if(same(key,"PAGEUP"))b->selected=b->selected>LIST_ROWS?b->selected-LIST_ROWS:0;
    else if(same(key,"PAGEDOWN")){if(b->count)b->selected=b->selected+LIST_ROWS<b->count?b->selected+LIST_ROWS:b->count-1;}
    else if(same(key,"SPACE")||same(key,"RETURN")||same(key,"ENTER")||same(key,"RIGHT")){
        if(b->save&&same(key,"SPACE"))return save_action(b,0);
        if(b->contents){if(same(key,"SPACE"))return result(b,RC_BROWSER_INSERT,b->image,UINT32_MAX,0);if(b->count){if(b->items[b->selected].disabled){rc_browser_error(b,"Only PRG files can be loaded");return RC_BROWSER_NONE;}return result(b,RC_BROWSER_LOAD,b->image,b->items[b->selected].content.index,0);}return RC_BROWSER_NONE;}
        if(!b->count){if(b->save){b->focus=2;return save_action(b,0);}return RC_BROWSER_NONE;}
        rc_browser_item *item=&b->items[b->selected];char path[RC_MEDIA_PATH],error[128]="";
        if(item->disabled){rc_browser_error(b,"Format is not available yet");return RC_BROWSER_NONE;}
        if(item->folder){if(!strcmp(item->name,"..")){go_parent(b);return RC_BROWSER_NONE;}
            if(b->roots){snprintf(path,sizeof path,"%s",item->name);b->roots=0;}else if(!join(path,sizeof path,b->path,item->name)){rc_browser_error(b,"Path is too long");return RC_BROWSER_NONE;}
            snprintf(b->path,sizeof b->path,"%s",path);if(!refresh(b,error,sizeof error))rc_browser_error(b,error);return RC_BROWSER_NONE;}
        if(b->save){snprintf(b->filename,sizeof b->filename,"%s",item->name);set_save_type(b,b->save_type);b->focus=2;return RC_BROWSER_NONE;}
        if(!join(path,sizeof path,b->path,item->name)){rc_browser_error(b,"Path is too long");return RC_BROWSER_NONE;}
        if(same(key,"SPACE")&&item->format>=RC_BROWSER_D64&&item->format<=RC_BROWSER_TAP)return result(b,RC_BROWSER_INSERT,path,UINT32_MAX,0);
        const unsigned action=rc_browser_file(b,path,error,sizeof error);if(error[0])rc_browser_error(b,error);return action;
    }
    reveal(b);return RC_BROWSER_NONE;
}
void rc_browser_input(rc_browser *b,const char *text){if(!b->active||b->confirm)return;
    char *target=b->save&&b->focus==2?b->filename:b->search;if(target==b->filename&&b->replace_name){target[0]=0;b->replace_name=0;}size_t length=strlen(target);
    for(;*text;++text){const unsigned char c=(unsigned char)*text;if(c<32||c>126)continue;if(length+1<RC_BROWSER_NAME)target[length++]=(char)c;}target[length]=0;
    if(target==b->search&&length){for(size_t i=0;i<b->count;++i){size_t j=0;for(;j<length&&b->items[i].name[j];++j)if(toupper((unsigned char)target[j])!=toupper((unsigned char)b->items[i].name[j]))break;if(j==length){b->selected=i;reveal(b);break;}}}
}
/* PETSCII case variants and reverse controls are resolved before the ASCII
 * UI font is used. Unsupported graphics remain visible as a question mark. */
static unsigned petscii(unsigned c){if(c==0xa0)return ' ';if(c>=0xc1&&c<=0xda)return c-0x80;if(c>=0x61&&c<=0x7a)return c-0x20;if(c>=0x20&&c<=0x5f)return c;return '?';}
static void content_row(rc_browser *b,unsigned row,const rc_browser_item *item,int selected){const uint8_t ink=(uint8_t)(item->disabled?12:14);char prefix[10],suffix[8];const unsigned blocks=(unsigned)((item->bytes+253u)/254u);snprintf(prefix,sizeof prefix,"%5u \"",blocks);rc_tui_put(&b->tui,row,2,prefix,selected?6:ink,selected?ink:6);
    int reverse=0;unsigned column=9;for(unsigned i=0;i<16&&column<26;++i){const unsigned c=item->content.name[i];if(!c)break;if(c==0x12){reverse=1;continue;}if(c==0x92){reverse=0;continue;}b->tui.ch[row][column]=(uint8_t)petscii(c);const int invert=selected^reverse;b->tui.ink[row][column]=(uint8_t)(invert?6:ink);b->tui.paper[row][column]=(uint8_t)(invert?ink:6);++column;}
    while(column<25){b->tui.ch[row][column]=' ';b->tui.ink[row][column]=(uint8_t)(selected?6:ink);b->tui.paper[row][column]=(uint8_t)(selected?ink:6);++column;}
    const unsigned type=item->content.type&7u;const char *types[]={"DEL","SEQ","PRG","USR","REL","???","???","???"};snprintf(suffix,sizeof suffix,"\" %s",item->format==RC_BROWSER_D64?types[type]:"PRG");rc_tui_put(&b->tui,row,25,suffix,selected?6:ink,selected?ink:6);
}
static const char *tail(const char *path,size_t width){const size_t length=strlen(path);return length>width?path+length-width:path;}
void rc_browser_draw(rc_browser *b,const uint32_t palette[16],rc_frame *frame){
    rc_tui_clear(&b->tui);rc_tui_bar(&b->tui,0,b->save?"REC64  SAVE":"REC64  LOAD");rc_tui_put(&b->tui,2,2,b->roots?"FILESYSTEM ROOTS":tail(b->contents?b->image:b->path,36),14,6);rc_tui_box(&b->tui,4,1,17,38);
    if(b->save){const char *types[]={"PROGRAM .PRG","RECORD .TAP","BLANK .D64"};char line[37];snprintf(line,sizeof line,"FORMAT: %-27s",types[b->save_type]);rc_tui_put(&b->tui,3,2,line,b->focus==1?6:14,b->focus==1?14:6);}
    else rc_tui_put(&b->tui,3,2,b->contents?"IMAGE CONTENTS":"FILES AND FOLDERS",14,6);
    for(unsigned row=0;row<LIST_ROWS&&b->top+row<b->count;++row){const size_t index=b->top+row;const rc_browser_item *item=&b->items[index];const int selected=index==b->selected&&(!b->save||!b->focus)&&!item->disabled;char line[37];memset(line,' ',36);line[36]=0;rc_tui_put(&b->tui,5+row,2,line,selected?6:item->disabled?12:14,selected?14:6);
        if(b->contents)content_row(b,5+row,item,selected);
        else {snprintf(line,sizeof line,"%c %-31.31s%s",item->folder?'[':' ',item->name,item->folder?"]":" ");rc_tui_put(&b->tui,5+row,2,line,selected?6:item->disabled?12:14,selected?14:6);}}
    if(!b->count)rc_tui_put(&b->tui,7,3,b->contents?"NO PROGRAM ENTRIES":"NO MATCHING FILES",12,6);
    if(b->save){char line[37];snprintf(line,sizeof line,"NAME: %-30.30s",tail(b->filename,30));rc_tui_put(&b->tui,21,2,line,b->focus==2?6:14,b->focus==2?14:6);}
    else rc_tui_put(&b->tui,21,2,b->contents?"SPACE INSERTS WITHOUT LOADING":"SPACE INSERT  ENTER OPEN/LOAD",14,6);
    if(b->message[0])rc_tui_put(&b->tui,22,1,b->message,14,6);
    else if(b->search[0]){char line[39];snprintf(line,sizeof line,"FIND: %.31s",b->search);rc_tui_put(&b->tui,22,1,line,14,6);}
    rc_tui_put(&b->tui,24,1,b->save?"TAB FIELD  ENTER SAVE  ESC CANCEL":"UP/DOWN SELECT  LEFT BACK  ESC CLOSE",14,6);
    if(b->confirm){rc_tui_box(&b->tui,9,3,7,34);for(unsigned row=10;row<15;++row)rc_tui_put(&b->tui,row,4,"                                ",14,6);rc_tui_put(&b->tui,10,5,"REPLACE EXISTING FILE?",14,6);rc_tui_put(&b->tui,12,5,tail(b->filename,28),14,6);rc_tui_put(&b->tui,14,5,"ENTER/Y REPLACE   ESC/N CANCEL",14,6);}
    rc_tui_frame(&b->tui,palette,frame);
}
size_t rc_browser_text(rc_browser *b,char *out,size_t capacity){rc_frame frame;rc_browser_draw(b,NULL,&frame);return rc_tui_text(&b->tui,out,capacity);}
