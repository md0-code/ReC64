#include "typist.h"
#include <stdio.h>
#include <string.h>
void rc_typist_init(rc_typist *t){memset(t,0,sizeof *t);}
void rc_typist_cancel(rc_typist *t){t->active=t->queued=0;t->position=t->length=0;t->commands[0]=0;}
int rc_typist_busy(const rc_typist *t){return t->active;}
void rc_typist_save(const rc_typist *t,rc_state_writer *w){
    rc_sw_u32(w,1);rc_sw_str(w,t->commands);rc_sw_u32(w,t->position);rc_sw_u32(w,t->length);rc_sw_u8(w,(uint8_t)t->active);rc_sw_u8(w,(uint8_t)t->queued);
}
int rc_typist_load(rc_typist *t,rc_state_reader *r){
    rc_typist_init(t);if(rc_sr_u32(r)!=1)return 0;rc_sr_str(r,t->commands,sizeof t->commands);if(r->end-r->at<10)return 0;
    t->position=rc_sr_u32(r);t->length=rc_sr_u32(r);t->active=rc_sr_u8(r);t->queued=rc_sr_u8(r);
    return rc_sr_ok(r)&&t->length==strlen(t->commands)&&t->position<=t->length&&t->active<=1&&t->queued<=1;
}
static int fail(rc_typist *t,const char *message,char *error,size_t cap){snprintf(t->error,sizeof t->error,"%s",message);if(cap)snprintf(error,cap,"%s",message);rc_typist_cancel(t);return 0;}
int rc_typist_start(rc_typist *t,const char *commands,char *error,size_t cap){
    rc_typist_init(t);unsigned line=0;
    for(const char *p=commands;*p;++p){char ch=*p;if(ch=='\\'&&p[1]=='n'){ch='\n';++p;}if(ch=='\r')ch='\n';
        if(t->length+2>=sizeof t->commands||line+2>=RC_KEYMAP_QUEUE)return fail(t,"Auto-load command is too long",error,cap);
        t->commands[t->length++]=ch;if(ch=='\n')line=0;else ++line;
    }
    if(t->length&&t->commands[t->length-1]!='\n')t->commands[t->length++]='\n';
    t->active=t->length!=0;if(cap)error[0]=0;return 1;
}
static int ready(const char *text){
    const char *end=text+strlen(text);while(end>text&&(end[-1]=='\n'||end[-1]=='\r'||end[-1]==' '))--end;
    const char *start=end;while(start>text&&start[-1]!='\n'&&start[-1]!='\r')--start;
    while(start<end&&*start==' ')++start;return end-start==6&&!memcmp(start,"READY.",6);
}
static int basic_error(const char *text){
    while(*text){const char *end=strchr(text,'\n');if(!end)end=text+strlen(text);
        while(text<end&&*text==' ')++text;
        if(text<end&&*text=='?'){const char *error=strstr(text,"ERROR");if(error&&error<end)return 1;}
        text=*end?end+1:end;
    }return 0;
}
int rc_typist_frame(rc_typist *t,rc_machine *machine,rc_keymap *keys){
    if(!t->active||keys->count)return 0;
    if(t->queued){uint8_t count;if(!rc_machine_peek(machine,0xc6,&count)||count)return 0;t->queued=0;if(t->position==t->length){t->active=0;return 0;}}
    if(rc_machine_activity(machine))return 0;
    char text[2048];rc_machine_text(machine,text,sizeof text);if(!ready(text))return 0;
    if(t->position&&basic_error(text)){fail(t,"Auto-load stopped after a BASIC error",NULL,0);return -1;}
    /* Feed the documented KERNAL keyboard FIFO ($0277, length $C6).
     * Advance the original editor between batches without presenting partial
     * lines. Firmware and the physical keyboard matrix remain untouched. */
    unsigned end=t->position;while(end<t->length&&t->commands[end++]!='\n'){}
    unsigned budget=1000000;
    while(t->position<end){uint8_t count;
        if(!rc_machine_peek(machine,0xc6,&count))return fail(t,"Keyboard buffer unavailable",NULL,0),-1;
        if(!count){
            /* Let GETIN finish shifting its previous FIFO before refilling. */
            if(rc_machine_run(machine,256)!=RC_RUN_OK)return fail(t,"Auto-load input failed",NULL,0),-1;
            const unsigned n=end-t->position<10?end-t->position:10;
            for(unsigned i=0;i<n;++i){unsigned char ch=(unsigned char)t->commands[t->position++];if(ch=='\n')ch=13;else if(ch>='a'&&ch<='z')ch-=32;rc_machine_poke(machine,0x277+i,ch);}
            rc_machine_poke(machine,0xc6,(uint8_t)n);
        }
        if(budget<256||rc_machine_run(machine,256)!=RC_RUN_OK)return fail(t,"Auto-load keyboard timed out",NULL,0),-1;
        budget-=256;
    }
    /* Finish displaying the last batch (including RETURN) in this update. */
    for(uint8_t count=1;count&&budget>=256;budget-=256){if(rc_machine_run(machine,256)!=RC_RUN_OK)return -1;rc_machine_peek(machine,0xc6,&count);}
    t->queued=1;return 1;
}
