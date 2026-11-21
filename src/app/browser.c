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
static unsigned format(const char *name){static const char *const extensions[]={"prg","p00","d64","t64","tap","crt","sid","rcs","g64"};for(unsigned i=0;i<9;++i)if(rc_media_extension_in(name,extensions[i]))return i;return RC_BROWSER_UNKNOWN;}
static void reveal(rc_browser *b){if(b->selected<b->top)b->top=b->selected;if(b->selected>=b->top+LIST_ROWS)b->top=b->selected-LIST_ROWS+1;}
static int select_enabled(rc_browser *b,size_t index,int direction){
    while(index<b->count){
        if(!b->items[index].disabled){
            b->selected=index;
            reveal(b);
            return 1;
        }
        if(direction>0)++index;
        else{if(!index)break;--index;}
    }
    return 0;
}
static void clear_items(rc_browser *b){free(b->items);b->items=NULL;b->count=b->selected=b->top=b->capacity=0;b->search[0]=0;}
static int append_item(rc_browser *b,const rc_browser_item *item){
    if(b->count==b->capacity){const size_t capacity=b->capacity?b->capacity*2:32;if(capacity<b->capacity||capacity>SIZE_MAX/sizeof *b->items){b->listing_failed=1;return 0;}
        rc_browser_item *items=realloc(b->items,capacity*sizeof *items);if(!items){b->listing_failed=1;return 0;}b->items=items;b->capacity=capacity;}
    b->items[b->count++]=*item;return 1;
}
static int join(char *out,size_t cap,const char *folder,const char *name){const size_t n=strlen(folder);const char separator[2]={rc_host_separator(),0};const int length=snprintf(out,cap,"%s%s%s",folder,n&&folder[n-1]!='/'&&folder[n-1]!='\\'?separator:"",name);return length>=0&&(size_t)length<cap;}
static int parent(const char *folder,char *out,size_t cap){char path[RC_MEDIA_PATH];if(!rc_host_full_path(folder,path,sizeof path))return 0;size_t n=strlen(path);while(n>1&&(path[n-1]=='/'||path[n-1]=='\\'))path[--n]=0;char *slash=strrchr(path,'/'),*back=strrchr(path,'\\');if(back&&(!slash||back>slash))slash=back;if(!slash)return 0;slash[1]=0;if(rc_host_same_folder(path,folder))return 0;return snprintf(out,cap,"%s",path)>0;}
static int list_entry(void *ctx,const char *name,int folder,uint64_t size){rc_browser *b=ctx;const unsigned kind=folder?RC_BROWSER_UNKNOWN:format(name);const unsigned saves[]={RC_BROWSER_PRG,RC_BROWSER_TAP,RC_BROWSER_D64};if(!folder&&(kind==RC_BROWSER_UNKNOWN||b->folders_only||(b->save&&kind!=saves[b->save_type])))return 1;rc_browser_item item={0};if(strlen(name)>=sizeof item.name)return 1;snprintf(item.name,sizeof item.name,"%s",name);item.folder=(unsigned)folder;item.format=kind;item.bytes=size;item.disabled=!folder&&kind>=RC_BROWSER_CRT&&kind!=RC_BROWSER_G64&&!(kind==RC_BROWSER_STATE&&b->states_enabled)&&!(kind==RC_BROWSER_CRT&&b->cartridges_enabled)&&!(kind==RC_BROWSER_SID&&b->sid_enabled);return append_item(b,&item);}
/* In the folder of the file last loaded, the cursor starts on that file. */
static void select_last(rc_browser *b){
    if(b->save||b->folders_only||b->roots||!b->last[0])return;
    char folder[RC_MEDIA_PATH];snprintf(folder,sizeof folder,"%s",b->last);
    char *slash=strrchr(folder,'/'),*back=strrchr(folder,'\\');if(back&&(!slash||back>slash))slash=back;if(!slash)return;
    const char *name=b->last+(slash-folder)+1;slash[1]=0;
    if(!rc_host_same_folder(folder,b->path))return;
    for(size_t i=0;i<b->count;++i)if(!b->items[i].folder&&!b->items[i].disabled&&!strcmp(b->items[i].name,name)){
        b->selected=i;b->top=i>=LIST_ROWS/2?i-LIST_ROWS/2:0;if(b->count>LIST_ROWS&&b->top>b->count-LIST_ROWS)b->top=b->count-LIST_ROWS;return;}
}
static int refresh(rc_browser *b,char *error,size_t cap){clear_items(b);b->contents=0;b->image[0]=0;b->confirm=0;b->listing_failed=0;b->message[0]=0;
    if(b->roots){if(!rc_host_list_roots(list_entry,b)){snprintf(error,cap,"Cannot list filesystem roots");return 0;}}
    else {char up[RC_MEDIA_PATH];if(parent(b->path,up,sizeof up)){rc_browser_item item={0};snprintf(item.name,sizeof item.name,"..");item.folder=1;if(!append_item(b,&item)){snprintf(error,cap,"Out of memory");return 0;}}
        if(!rc_host_list_folder(b->path,list_entry,b)){snprintf(error,cap,"Cannot read folder");return 0;}}
    if(b->listing_failed){snprintf(error,cap,"Out of memory");return 0;}
    if(b->count>1)qsort(b->items,b->count,sizeof *b->items,compare);
    select_enabled(b,0,1);select_last(b);
    return 1;
}
void rc_browser_init(rc_browser *b){memset(b,0,sizeof *b);rc_tui_init(&b->tui);snprintf(b->filename,sizeof b->filename,"program.prg");}
void rc_browser_close(rc_browser *b){clear_items(b);b->active=b->confirm=b->finding=b->renaming=0;}
void rc_browser_set_tape_reader(rc_browser *b,rc_browser_tape_list_fn reader,void *ctx){b->tape_list=reader;b->tape_ctx=ctx;}
void rc_browser_select(rc_browser *b,const char *name){
    for(size_t i=0;i<b->count;++i)if(!b->items[i].disabled&&!strcmp(b->items[i].name,name)){b->selected=i;b->top=i>=LIST_ROWS?i-LIST_ROWS/2:0;return;}
}
void rc_browser_error(rc_browser *b,const char *message){snprintf(b->message,sizeof b->message,"%s",message?message:"");}
int rc_browser_refresh(rc_browser *b,char *error,size_t cap){b->renaming=0;return refresh(b,error,cap);}
int rc_browser_open(rc_browser *b,const char *folder,int save,char *error,size_t cap){
    if(cap)error[0]=0;
    const char *start=folder&&*folder?folder:b->path[0]?b->path:".";char path[RC_MEDIA_PATH];
    if(!rc_host_full_path(start,path,sizeof path)||!rc_host_folder_exists(path)){snprintf(error,cap,"Folder not found");return 0;}
    snprintf(b->path,sizeof b->path,"%s",path);b->save=save==1;b->folders_only=save==2;b->renaming=b->finding=b->pad_ui=0;b->name_cursor=0;b->focus=0;b->roots=0;b->active=1;b->result=(rc_browser_result){0};b->result.entry=UINT32_MAX;
    return refresh(b,error,cap);
}
static int image_items(rc_browser *b,const rc_browser_content *entries,size_t count,unsigned kind){
    rc_browser_item *items=count?calloc(count,sizeof *items):NULL;if(count&&!items)return 0;
    for(size_t i=0;i<count;++i){items[i].content=entries[i];items[i].format=kind;items[i].bytes=entries[i].length;items[i].disabled=kind==RC_BROWSER_D64&&(entries[i].type&7u)!=2;
        unsigned at=0;for(unsigned j=0;j<16;++j){const unsigned c=entries[i].name[j];if(!c)break;if(c!=0x12&&c!=0x92)items[i].name[at++]=(char)petscii(c);}items[i].name[at]=0;}
    clear_items(b);b->items=items;b->count=b->capacity=count;b->contents=1;b->roots=0;
    select_enabled(b,0,1);
    return 1;
}
static unsigned result(rc_browser *b,unsigned action,const char *path,uint32_t entry,int overwrite){
    b->result=(rc_browser_result){0};snprintf(b->result.path,sizeof b->result.path,"%s",path);b->result.entry=entry;b->result.action=action;b->result.overwrite=overwrite;
    if(action==RC_BROWSER_LOAD||action==RC_BROWSER_INSERT)snprintf(b->last,sizeof b->last,"%s",path);
    return action;
}
unsigned rc_browser_file(rc_browser *b,const char *path,char *error,size_t cap){
    if(cap)error[0]=0;
    char absolute[RC_MEDIA_PATH];if(!rc_host_full_path(path,absolute,sizeof absolute)){snprintf(error,cap,"Path is too long");return RC_BROWSER_NONE;}
    const unsigned kind=format(absolute);if(kind==RC_BROWSER_G64||(kind==RC_BROWSER_STATE&&b->states_enabled)||(kind==RC_BROWSER_CRT&&b->cartridges_enabled)||(kind==RC_BROWSER_SID&&b->sid_enabled))return result(b,RC_BROWSER_LOAD,absolute,UINT32_MAX,0);if(kind>=RC_BROWSER_CRT){snprintf(error,cap,kind==RC_BROWSER_UNKNOWN?"Unsupported file type":"Format is not available yet");return RC_BROWSER_NONE;}
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
    if(ok){ok=image_items(b,entries,count,kind);if(ok){snprintf(b->image,sizeof b->image,"%s",absolute);snprintf(b->path,sizeof b->path,"%s",absolute);char *slash=strrchr(b->path,'/'),*back=strrchr(b->path,'\\');if(back&&(!slash||back>slash))slash=back;if(slash)slash[1]=0;b->active=1;b->save=b->folders_only=b->renaming=b->finding=0;b->focus=0;b->confirm=0;b->message[0]=0;}if(!ok)snprintf(error,cap,"Out of memory");else if(!count)rc_browser_error(b,"No loadable entries");}
    free(entries);return RC_BROWSER_NONE;
}
static void set_save_type(rc_browser *b,unsigned kind){static const char *const extensions[]={".prg",".tap",".d64"};b->save_type=kind;char *dot=strrchr(b->filename,'.');if(dot)*dot=0;const size_t n=strlen(b->filename);if(n+4<sizeof b->filename)memcpy(b->filename+n,extensions[kind],5);}
static unsigned save_action(rc_browser *b,int overwrite){static const unsigned actions[]={RC_BROWSER_SAVE_PRG,RC_BROWSER_SAVE_TAP,RC_BROWSER_SAVE_D64};static const char *const extensions[]={"prg","tap","d64"};
    if(!b->filename[0]||!strcmp(b->filename,".")||!strcmp(b->filename,"..")||strpbrk(b->filename,"<>:\"/\\|?*")){rc_browser_error(b,"Choose a valid file name");return RC_BROWSER_NONE;}
    if(!rc_media_extension_in(b->filename,extensions[b->save_type])){const size_t length=strlen(b->filename);if(length+4>=sizeof b->filename){rc_browser_error(b,"File name is too long");return RC_BROWSER_NONE;}b->filename[length]='.';memcpy(b->filename+length+1,extensions[b->save_type],4);}
    char path[RC_MEDIA_PATH];if(!join(path,sizeof path,b->path,b->filename)){rc_browser_error(b,"Path is too long");return RC_BROWSER_NONE;}
    if(rc_host_folder_exists(path)){rc_browser_error(b,"A folder has this name");return RC_BROWSER_NONE;}
    if(rc_host_file_exists(path)&&!overwrite){b->confirm=1;return RC_BROWSER_NONE;}
    b->confirm=0;return result(b,actions[b->save_type],path,UINT32_MAX,overwrite);
}
static unsigned file_action(rc_browser *b,int rename){
    if(!b->original_name[0])return RC_BROWSER_NONE;char path[RC_MEDIA_PATH];if(!join(path,sizeof path,b->path,b->original_name)){rc_browser_error(b,"Path is too long");return RC_BROWSER_NONE;}
    if(!rename){b->confirm=0;return result(b,RC_BROWSER_DELETE,path,UINT32_MAX,0);}
    char *name=b->edit_name;if(!name[0]||!strcmp(name,".")||!strcmp(name,"..")||strpbrk(name,"<>:\"/\\|?*")||name[strlen(name)-1]=='.'||name[strlen(name)-1]==' '){rc_browser_error(b,"Choose a valid file name");return RC_BROWSER_NONE;}
    const char *extension=strrchr(b->original_name,'.');if(extension&&!rc_media_extension_in(name,extension+1)){const size_t n=strlen(name),e=strlen(extension);if(n+e>=sizeof b->edit_name){rc_browser_error(b,"File name is too long");return RC_BROWSER_NONE;}memcpy(name+n,extension,e+1);}
    char destination[RC_MEDIA_PATH];if(!join(destination,sizeof destination,b->path,name)){rc_browser_error(b,"Path is too long");return RC_BROWSER_NONE;}
    if(rc_host_file_exists(destination)){rc_browser_error(b,"A file with this name exists");return RC_BROWSER_NONE;}
    const unsigned action=result(b,RC_BROWSER_RENAME,path,UINT32_MAX,0);snprintf(b->result.destination,sizeof b->result.destination,"%s",destination);return action;
}
static int go_parent(rc_browser *b){char error[128],up[RC_MEDIA_PATH];if(b->contents){b->contents=0;if(!refresh(b,error,sizeof error)){rc_browser_error(b,error);return 0;}return 1;}
    if(parent(b->path,up,sizeof up))snprintf(b->path,sizeof b->path,"%s",up);else b->roots=1;
    if(!refresh(b,error,sizeof error)){rc_browser_error(b,error);return 0;}return 1;}

static int find_prefix(rc_browser *b,const char *prefix){
    const size_t length=strlen(prefix);
    if(!length)return 0;
    for(size_t i=0;i<b->count;++i){
        if(b->items[i].disabled)continue;
        size_t j=0;
        while(j<length&&b->items[i].name[j]&&
            toupper((unsigned char)prefix[j])==toupper((unsigned char)b->items[i].name[j]))++j;
        if(j==length){b->selected=i;reveal(b);return 1;}
    }
    return 0;
}

static char *edit_target(rc_browser *b){
    return b->renaming||b->finding?b->edit_name:b->filename;
}

static size_t edit_end(const rc_browser *b,const char *name){
    static const char *const extensions[]={"prg","tap","d64"};
    if(!b->finding){
        const char *extension=b->renaming?strrchr(b->original_name,'.'):NULL;
        if(!b->renaming)extension=extensions[b->save_type];
        else if(extension)++extension;
        if(extension&&rc_media_extension_in(name,extension))return (size_t)(strrchr(name,'.')-name);
    }
    return strlen(name);
}

static void pad_edit(rc_browser *b,rc_pad_key key){
    static const char characters[]="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 -_.!#$%&'()+,;=@[]^{}`~";
    char *name=edit_target(b);
    const size_t end=edit_end(b,name);
    if(b->name_cursor>end)b->name_cursor=end;
    size_t cursor=b->name_cursor;
    if(key==RC_PAD_LEFT){if(cursor)--cursor;}
    else if(key==RC_PAD_RIGHT){if(cursor<end)++cursor;}
    else if(key==RC_PAD_X&&end){
        if(cursor==end)--cursor;
        memmove(name+cursor,name+cursor+1,strlen(name+cursor+1)+1);
    }else if(key==RC_PAD_Y){
        memmove(name,name+end,strlen(name+end)+1);
        cursor=0;
    }else if(key==RC_PAD_UP||key==RC_PAD_DOWN){
        const size_t count=sizeof characters-1;
        if(cursor==end){
            if(strlen(name)+1>=RC_BROWSER_NAME)return;
            memmove(name+cursor+1,name+cursor,strlen(name+cursor)+1);
            name[cursor]=key==RC_PAD_UP?characters[0]:characters[count-1];
        }else{
            const char *found=strchr(characters,toupper((unsigned char)name[cursor]));
            size_t index=found?(size_t)(found-characters):key==RC_PAD_UP?count-1:0;
            index=(index+(key==RC_PAD_UP?1:count-1))%count;
            name[cursor]=characters[index];
        }
    }
    b->name_cursor=cursor;
    b->replace_name=0;
}

unsigned rc_browser_key(rc_browser *b,const char *name){
    if(!b->active)return RC_BROWSER_NONE;char key[64];size_t n=strlen(name);if(n>=sizeof key)return RC_BROWSER_NONE;for(size_t i=0;i<=n;++i)key[i]=(char)toupper((unsigned char)name[i]);
    if(b->confirm){if(same(key,"Y")||same(key,"RETURN")||same(key,"ENTER"))return b->confirm==2?file_action(b,0):save_action(b,1);if(same(key,"N")||same(key,"ESCAPE")||same(key,"BACKSPACE"))b->confirm=0;return RC_BROWSER_NONE;}
    if(b->finding){
        if(same(key,"ESCAPE")||same(key,"F12"))b->finding=0;
        else if(same(key,"RETURN")||same(key,"ENTER")){
            b->finding=0;
            if(b->edit_name[0]&&!find_prefix(b,b->edit_name))rc_browser_error(b,"No matching name");
        }else if(same(key,"BACKSPACE")){size_t length=strlen(b->edit_name);if(length)b->edit_name[length-1]=0;}
        return RC_BROWSER_NONE;
    }
    if(b->renaming){if(same(key,"ESCAPE")){b->renaming=0;b->message[0]=0;}else if(same(key,"RETURN")||same(key,"ENTER"))return file_action(b,1);else if(same(key,"CTRL+A"))b->replace_name=1;else if(same(key,"BACKSPACE")||same(key,"DELETE")){const size_t length=strlen(b->edit_name);if(b->replace_name)b->edit_name[0]=0;else if(length)b->edit_name[length-1]=0;b->replace_name=0;}return RC_BROWSER_NONE;}
    if(same(key,"ESCAPE")||same(key,"F12"))return RC_BROWSER_CLOSE;
    if(b->folders_only&&same(key,"SPACE")&&!b->roots)return result(b,RC_BROWSER_FOLDER,b->path,UINT32_MAX,0);
    if(b->save&&!b->focus&&b->count&&!b->items[b->selected].folder&&(same(key,"F2")||same(key,"DELETE"))){snprintf(b->original_name,sizeof b->original_name,"%s",b->items[b->selected].name);snprintf(b->edit_name,sizeof b->edit_name,"%s",b->original_name);b->message[0]=0;if(same(key,"F2")){b->renaming=1;b->replace_name=1;b->name_cursor=0;}else b->confirm=2;return RC_BROWSER_NONE;}
    if(same(key,"TAB")&&b->save){b->focus=(b->focus+1)%3;b->name_cursor=0;return RC_BROWSER_NONE;}
    if(b->save&&b->focus==1){if(same(key,"LEFT")||same(key,"RIGHT")||same(key,"+")||same(key,"-")||same(key,"RETURN")||same(key,"ENTER")){set_save_type(b,(b->save_type+(same(key,"LEFT")||same(key,"-")?2u:1u))%3);char error[128];if(!refresh(b,error,sizeof error))rc_browser_error(b,error);return RC_BROWSER_NONE;}}
    if(b->save&&b->focus==2){if(same(key,"CTRL+A")){b->replace_name=1;return RC_BROWSER_NONE;}if(same(key,"BACKSPACE")||same(key,"DELETE")){size_t length=strlen(b->filename);if(b->replace_name){b->filename[0]=0;b->replace_name=0;}else if(length)b->filename[length-1]=0;return RC_BROWSER_NONE;}if(same(key,"RETURN")||same(key,"ENTER"))return save_action(b,0);return RC_BROWSER_NONE;}
    if(same(key,"LEFT")||same(key,"BACKSPACE")){go_parent(b);return RC_BROWSER_NONE;}
    if(same(key,"HOME")||same(key,"END")||same(key,"UP")||same(key,"DOWN")||same(key,"PAGEUP")||same(key,"PAGEDOWN"))b->search[0]=0;
    if(same(key,"HOME"))select_enabled(b,0,1);
    else if(same(key,"END")){if(b->count)select_enabled(b,b->count-1,-1);}
    else if(same(key,"UP")){if(b->selected)select_enabled(b,b->selected-1,-1);}
    else if(same(key,"DOWN"))select_enabled(b,b->selected+1,1);
    else if(same(key,"PAGEUP")){
        const size_t index=b->selected>LIST_ROWS?b->selected-LIST_ROWS:0;
        if(!select_enabled(b,index,-1))select_enabled(b,index,1);
    }
    else if(same(key,"PAGEDOWN")){
        if(b->count){
            const size_t index=b->selected+LIST_ROWS<b->count?b->selected+LIST_ROWS:b->count-1;
            if(!select_enabled(b,index,1))select_enabled(b,index,-1);
        }
    }
    else if(same(key,"SPACE")||same(key,"RETURN")||same(key,"ENTER")||same(key,"RIGHT")){
        if(b->save&&same(key,"SPACE"))return save_action(b,0);
        if(b->contents){if(same(key,"SPACE"))return result(b,RC_BROWSER_INSERT,b->image,UINT32_MAX,0);if(b->count){if(b->items[b->selected].disabled){rc_browser_error(b,"Only PRG files can be loaded");return RC_BROWSER_NONE;}return result(b,RC_BROWSER_LOAD,b->image,b->items[b->selected].content.index,0);}return RC_BROWSER_NONE;}
        if(!b->count){if(b->save){b->focus=2;return save_action(b,0);}return RC_BROWSER_NONE;}
        rc_browser_item *item=&b->items[b->selected];char path[RC_MEDIA_PATH],error[128]="";
        if(item->disabled){rc_browser_error(b,"Format is not available yet");return RC_BROWSER_NONE;}
        if(item->folder){if(!strcmp(item->name,"..")){go_parent(b);return RC_BROWSER_NONE;}
            if(b->roots){snprintf(path,sizeof path,"%s",item->name);b->roots=0;}else if(!join(path,sizeof path,b->path,item->name)){rc_browser_error(b,"Path is too long");return RC_BROWSER_NONE;}
            snprintf(b->path,sizeof b->path,"%s",path);if(!refresh(b,error,sizeof error))rc_browser_error(b,error);return RC_BROWSER_NONE;}
        if(b->save){snprintf(b->filename,sizeof b->filename,"%s",item->name);set_save_type(b,b->save_type);b->focus=2;b->name_cursor=0;return RC_BROWSER_NONE;}
        if(!join(path,sizeof path,b->path,item->name)){rc_browser_error(b,"Path is too long");return RC_BROWSER_NONE;}
        if(same(key,"SPACE")&&((item->format>=RC_BROWSER_D64&&item->format<=RC_BROWSER_TAP)||item->format==RC_BROWSER_G64))return result(b,RC_BROWSER_INSERT,path,UINT32_MAX,0);
        const unsigned action=rc_browser_file(b,path,error,sizeof error);if(error[0])rc_browser_error(b,error);return action;
    }
    reveal(b);return RC_BROWSER_NONE;
}
static int can_insert(const rc_browser *b){
    if(b->contents)return 1;
    if(!b->count||b->items[b->selected].folder||b->items[b->selected].disabled)return 0;
    const unsigned format=b->items[b->selected].format;
    return (format>=RC_BROWSER_D64&&format<=RC_BROWSER_TAP)||format==RC_BROWSER_G64;
}
unsigned rc_browser_pad(rc_browser *b,rc_pad_key key){
    if(!b->active)return RC_BROWSER_NONE;
    b->pad_ui=1;
    if(key==RC_PAD_B)return rc_browser_key(b,"ESCAPE");
    if(key==RC_PAD_A){
        if(!b->confirm&&!b->finding&&(b->renaming||(b->save&&b->focus==2))&&!edit_end(b,edit_target(b))){
            rc_browser_error(b,"Choose a valid file name");return RC_BROWSER_NONE;
        }
        return rc_browser_key(b,"RETURN");
    }
    if(b->confirm)return RC_BROWSER_NONE;
    if(b->save&&!b->renaming&&!b->finding&&(key==RC_PAD_SHOULDER_LEFT||key==RC_PAD_SHOULDER_RIGHT)){
        b->focus=(b->focus+(key==RC_PAD_SHOULDER_LEFT?2u:1u))%3u;
        b->name_cursor=0;
        return RC_BROWSER_NONE;
    }
    if(b->renaming||b->finding||(b->save&&b->focus==2)){pad_edit(b,key);return RC_BROWSER_NONE;}
    if(key>=RC_PAD_UP&&key<=RC_PAD_RIGHT){
        static const char *const directions[]={"UP","DOWN","LEFT","RIGHT"};
        return rc_browser_key(b,directions[key-RC_PAD_UP]);
    }
    if(b->save&&b->focus)return RC_BROWSER_NONE;
    switch(key){
    case RC_PAD_X:
        if(b->save)return rc_browser_key(b,"F2");
        if((b->folders_only&&!b->roots)||can_insert(b))return rc_browser_key(b,"SPACE");
        break;
    case RC_PAD_Y:if(b->save)return rc_browser_key(b,"DELETE");/* fall through - Y finds in the load browser */
    case RC_PAD_BACK:
        if(b->count){b->finding=1;b->name_cursor=0;b->replace_name=0;b->edit_name[0]=0;b->message[0]=0;}
        break;
    case RC_PAD_SHOULDER_LEFT:return rc_browser_key(b,"PAGEUP");
    case RC_PAD_SHOULDER_RIGHT:return rc_browser_key(b,"PAGEDOWN");
    default:break;
    }
    return RC_BROWSER_NONE;
}
void rc_browser_input(rc_browser *b,const char *text){if(!b->active||b->confirm)return;
    char *target=b->renaming||b->finding||(b->save&&b->focus==2)?edit_target(b):b->search;if(target!=b->search&&b->replace_name){target[0]=0;b->replace_name=0;}size_t length=strlen(target);
    for(;*text;++text){const unsigned char c=(unsigned char)*text;if(c<32||c>126)continue;if(length+1<RC_BROWSER_NAME)target[length++]=(char)c;}target[length]=0;
    if(target==b->search)find_prefix(b,target);
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
static void draw_edit_line(rc_browser *b,unsigned row,unsigned col,unsigned width,const char *name){
    const size_t end=edit_end(b,name);
    const size_t cursor=b->name_cursor<end?b->name_cursor:end;
    const size_t first=cursor>=width?cursor-width+1:0;
    char shown[RC_TUI_COLS+1];
    snprintf(shown,sizeof shown,"%-*.*s",(int)width,(int)width,name+first);
    rc_tui_put(&b->tui,row,col,shown,14,6);
    const char caret[]={name[cursor]?name[cursor]:'_',0};
    rc_tui_put(&b->tui,row,col+(unsigned)(cursor-first),caret,6,14);
}
void rc_browser_draw(rc_browser *b,const uint32_t palette[16],rc_frame *frame){
    char title[40];snprintf(title,sizeof title,"%s %zu/%zu",b->folders_only?"FOLDER":b->save?"SAVE":"LOAD",b->selected/15u+1u,(b->count+14u)/15u? (b->count+14u)/15u:1u);rc_tui_clear(&b->tui);rc_tui_bar(&b->tui,0,title);rc_tui_put(&b->tui,2,2,b->roots?"FILESYSTEM ROOTS":tail(b->contents?b->image:b->path,36),14,6);rc_tui_box(&b->tui,4,1,17,38);
    if(b->save){const char *types[]={"PROGRAM .PRG","PROGRAM .TAP","PROGRAM .D64"};char line[37];snprintf(line,sizeof line,"FORMAT: %-27s",types[b->save_type]);rc_tui_put(&b->tui,3,2,line,b->focus==1?6:14,b->focus==1?14:6);}
    else rc_tui_put(&b->tui,3,2,b->folders_only?"SELECT DEFAULT MEDIA FOLDER":b->contents?"IMAGE CONTENTS":"FILES AND FOLDERS",14,6);
    for(unsigned row=0;row<LIST_ROWS&&b->top+row<b->count;++row){const size_t index=b->top+row;const rc_browser_item *item=&b->items[index];const int selected=index==b->selected&&(!b->save||!b->focus)&&!item->disabled;char line[37];memset(line,' ',36);line[36]=0;rc_tui_put(&b->tui,5+row,2,line,selected?6:item->disabled?12:14,selected?14:6);
        if(b->contents)content_row(b,5+row,item,selected);
        else {snprintf(line,sizeof line,"%c %-31.31s%s",item->folder?'[':' ',item->name,item->folder?"]":" ");rc_tui_put(&b->tui,5+row,2,line,selected?6:item->disabled?12:14,selected?14:6);}}
    if(!b->count)rc_tui_put(&b->tui,7,3,b->contents?"NO PROGRAM ENTRIES":"NO MATCHING FILES",12,6);
    if(b->save){
        if(b->pad_ui&&b->focus==2){
            rc_tui_put(&b->tui,21,2,"NAME:",14,6);
            draw_edit_line(b,21,8,30,b->filename);
        }else{char line[37];snprintf(line,sizeof line,"NAME: %-30.30s",tail(b->filename,30));rc_tui_put(&b->tui,21,2,line,b->focus==2?6:14,b->focus==2?14:6);}
    }else if(b->pad_ui){
        const char *hint=b->folders_only?(b->roots?"A OPEN DRIVE":"A OPEN  X USE CURRENT FOLDER"):
            can_insert(b)?"A OPEN/LOAD  X INSERT ONLY":"A OPEN/LOAD";
        rc_tui_put(&b->tui,21,2,hint,14,6);
    }else rc_tui_put(&b->tui,21,2,b->folders_only?"SPACE USES CURRENT FOLDER":b->contents?"SPACE INSERTS WITHOUT LOADING":"SPACE INSERT  ENTER OPEN/LOAD",14,6);
    if(b->message[0])rc_tui_put(&b->tui,22,1,b->message,14,6);
    else if(b->save&&!b->search[0]&&!b->pad_ui)rc_tui_put(&b->tui,22,1,"F2 RENAME  DEL DELETE SELECTED FILE",14,6);
    else if(b->search[0]){char line[39];snprintf(line,sizeof line,"FIND: %.31s",b->search);rc_tui_put(&b->tui,22,1,line,14,6);}
    if(b->pad_ui){
        if(!b->save)rc_tui_put(&b->tui,24,1,"LB/RB PAGE   Y FIND   B CLOSE",14,6);
        else if(b->focus==2){
            rc_tui_put(&b->tui,23,1,"UP/DOWN CHAR  LEFT/RIGHT  B CLOSE",14,6);
            rc_tui_put(&b->tui,24,1,"LB/RB FIELD  A SAVE  X ERASE Y CLEAR",14,6);
        }else if(b->focus==1)rc_tui_put(&b->tui,24,1,"LB/RB FIELD  LEFT/RIGHT TYPE  B CLOSE",14,6);
        else {
            if(b->count&&!b->items[b->selected].folder)rc_tui_put(&b->tui,23,1,"X RENAME   Y DELETE",14,6);
            rc_tui_put(&b->tui,24,1,"LB/RB FIELD   BACK FIND   B CLOSE",14,6);
        }
    }else rc_tui_put(&b->tui,24,1,b->folders_only?"SPACE SELECT FOLDER  ENTER OPEN  ESC":b->save?"TAB FIELD  ENTER SAVE  ESC CANCEL":"UP/DOWN SELECT  LEFT BACK  ESC CLOSE",14,6);
    if(b->confirm||b->renaming||b->finding){
        rc_tui_box(&b->tui,9,3,7,34);
        for(unsigned row=10;row<15;++row)rc_tui_put(&b->tui,row,4,"                                ",14,6);
        rc_tui_put(&b->tui,10,5,b->finding?"FIND BY NAME":b->renaming?"RENAME SELECTED FILE":
            b->confirm==2?"DELETE SELECTED FILE?":"REPLACE EXISTING FILE?",14,6);
        if(b->pad_ui&&(b->renaming||b->finding)){
            draw_edit_line(b,11,5,28,b->edit_name);
            rc_tui_put(&b->tui,13,5,"UP/DOWN CHAR  LEFT/RIGHT MOVE",14,6);
            rc_tui_put(&b->tui,14,5,b->finding?"A FIND B BACK X ERASE Y CLEAR":"A RENAME B BACK X ERASE Y CLEAR",14,6);
        }else{
            const char *name=b->renaming||b->finding?b->edit_name:b->confirm==2?b->original_name:b->filename;
            rc_tui_put(&b->tui,12,5,tail(name,28),14,6);
            const char *help=b->pad_ui?(b->confirm==2?"A DELETE    B CANCEL":"A REPLACE    B CANCEL"):
                b->finding?"ENTER FIND     ESC CANCEL":b->renaming?"ENTER RENAME   ESC CANCEL":
                b->confirm==2?"ENTER/Y DELETE   ESC/N CANCEL":"ENTER/Y REPLACE   ESC/N CANCEL";
            rc_tui_put(&b->tui,14,5,help,14,6);
        }
    }
    rc_tui_frame(&b->tui,palette,frame);
}
size_t rc_browser_text(rc_browser *b,char *out,size_t capacity){rc_frame frame;rc_browser_draw(b,NULL,&frame);return rc_tui_text(&b->tui,out,capacity);}
