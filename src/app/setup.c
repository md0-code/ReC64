#include "setup.h"
#include "rc_version.h"
#include <stdio.h>
#include <string.h>
static const char *const PAGES[]={"MACHINE","MEDIA","DISPLAY","SOUND","CONTROLLERS","CARTRIDGE","STATES","ABOUT"};
static const char *const IDS[]={"machine","media","display","sound","controllers","cartridge","states","about"};
void rc_setup_palette(rc_setup *s,const char *name){if(s->palette_count==40||strlen(name)>=128)return;for(unsigned i=0;i<s->palette_count;++i)if(!strcmp(s->palettes[i],name))return;snprintf(s->palettes[s->palette_count++],128,"%s",name);}
void rc_setup_init(rc_setup *s,rc_config *c){memset(s,0,sizeof *s);s->config=c;s->crt_available=1;rc_tui_init(&s->tui);
    for(unsigned i=0;i<8;++i)if(!strcmp(c->setup_page,IDS[i]))s->page=i;
    if(s->page==RC_SETUP_STATES)s->row=(unsigned)c->state_slot-1u;
    rc_setup_palette(s,"cjam");rc_setup_palette(s,"colodore");rc_setup_palette(s,"pepto-pal");rc_setup_palette(s,"pepto-ntsc");rc_setup_palette(s,c->palette);
}
static unsigned rows(const rc_setup *s){return s->page==RC_SETUP_MACHINE?5u:s->page==RC_SETUP_DISPLAY?7u:s->page==RC_SETUP_SOUND?2u:s->page==RC_SETUP_CONTROLLERS?6u:s->page==RC_SETUP_MEDIA?15u:s->page==RC_SETUP_CARTRIDGE?5u:s->page==RC_SETUP_STATES?12u:0;}
static unsigned cycle(unsigned value,unsigned count,int direction){return (value+count+(direction<0?count-1u:1u))%count;}
unsigned rc_setup_key(rc_setup *s,const char *key){
    if(!strcmp(key,"ESCAPE")||!strcmp(key,"F12")){s->active=0;return RC_SETUP_CLOSE;}
    if(!strcmp(key,"LEFT")||!strcmp(key,"RIGHT")||!strcmp(key,"TAB")){
        s->page=cycle(s->page,8,!strcmp(key,"LEFT")?-1:1);s->row=s->page==RC_SETUP_STATES?(unsigned)s->config->state_slot-1u:0;snprintf(s->config->setup_page,sizeof s->config->setup_page,"%s",IDS[s->page]);return s->page==RC_SETUP_STATES?RC_SETUP_STATE_REFRESH:0;}
    const unsigned count=rows(s);if(!count)return 0;
    if(!strcmp(key,"UP")||!strcmp(key,"DOWN")){s->row=cycle(s->row,count,!strcmp(key,"UP")?-1:1);if(s->page==RC_SETUP_STATES&&s->row<10){s->config->state_slot=(int)s->row+1;return RC_SETUP_STATE_REFRESH;}return 0;}
    int direction=1;
    if(!strcmp(key,"-")||!strcmp(key,"PAGEDOWN"))direction=-1;
    else if(strcmp(key,"RETURN")&&strcmp(key,"ENTER")&&strcmp(key,"+")&&strcmp(key,"=")&&strcmp(key,"PAGEUP"))return 0;
    rc_config *c=s->config;
    if(s->page==RC_SETUP_CARTRIDGE){
        if(!s->cartridge_available)return 0;
        if(s->row<3)return !s->row?RC_SETUP_LOAD:s->row==1?(s->has_cartridge?RC_SETUP_EJECT_CART:0):(s->can_freeze?RC_SETUP_FREEZE:0);
        const rc_family *family=rc_family_of_model(c->model,NULL);const unsigned index=s->row==3?2u:3u,other=index==2?3u:2u;
        if(!family||family->option_count<=other)return 0;
        const rc_option_desc *option=&family->options[index];const char *current=rc_config_option_value(c,option->key);unsigned value=option->initial,value_count=0;
        while(option->values[value_count]){if(current&&!strcmp(current,option->values[value_count]))value=value_count;++value_count;}
        value=cycle(value,value_count,direction);rc_config_set_option(c,option->key,option->values[value]);
        if(value)rc_config_set_option(c,family->options[other].key,"off");return RC_SETUP_RESTART;
    }
    if(s->page==RC_SETUP_STATES){if(s->row<10){c->state_slot=(int)s->row+1;return RC_SETUP_STATE_REFRESH;}return s->row==10?RC_SETUP_STATE_SAVE:s->state_status[c->state_slot-1]==1?RC_SETUP_STATE_LOAD:0;}
    if(s->page==RC_SETUP_MACHINE){
        if(!s->row){const rc_family *f=rc_family_at(0);unsigned model=0;const rc_family *old=rc_family_of_model(c->model,&model);if(old!=f)model=direction>0?f->model_count-1u:0;model=cycle(model,f->model_count,direction);snprintf(c->model,sizeof c->model,"%s",f->models[model].id);return RC_SETUP_RESTART;}
        if(s->row==1){if(!strcmp(c->model,"test"))return 0;const char *current=rc_config_option_value(c,"sid");unsigned v=current&&!strcmp(current,"6581")?1u:current&&!strcmp(current,"8580")?2u:0u;const char *values[]={"default","6581","8580"};rc_config_set_option(c,"sid",values[cycle(v,3,direction)]);return RC_SETUP_RESTART;}
        return s->row==2?RC_SETUP_RESET:s->row==3?RC_SETUP_COLD:RC_SETUP_OFF;
    }
    if(s->page==RC_SETUP_DISPLAY){
        switch(s->row){
        case 0:{unsigned v=c->look==RC_LOOK_CRT?0u:c->filter==RC_CONFIG_FILTER_LINEAR?2u:1u;v=cycle(v,3,direction);if(!v&&!s->crt_available)v=direction>0?1u:2u;c->look=v?RC_LOOK_PLAIN:RC_LOOK_CRT;c->filter=v==2?RC_CONFIG_FILTER_LINEAR:RC_CONFIG_FILTER_NEAREST;break;}
        case 1:{unsigned v=0;for(unsigned i=0;i<s->palette_count;++i)if(!strcmp(c->palette,s->palettes[i]))v=i;snprintf(c->palette,sizeof c->palette,"%s",s->palettes[cycle(v,s->palette_count,direction)]);break;}
        case 2:c->border=cycle(c->border,3,direction);break;
        case 3:c->fullscreen=!c->fullscreen;break;
        case 4:c->scale=!c->scale;break;
        case 5:c->aspect=!c->aspect;break;
        case 6:c->vsync=!c->vsync;break;
        }return RC_SETUP_VIDEO;
    }
    if(s->page==RC_SETUP_MEDIA){
        if(!s->media_available)return 0;
        if(s->row<2)return s->row?RC_SETUP_SAVE:RC_SETUP_LOAD;
        if(s->row<7){if(!s->has_tape||(s->row==5&&!s->tape_writable))return 0;const unsigned actions[]={RC_SETUP_PLAY,RC_SETUP_STOP,RC_SETUP_REWIND,RC_SETUP_RECORD,RC_SETUP_EJECT};return actions[s->row-2];}
        if(s->row==10)return s->has_disk?RC_SETUP_EJECT_DISK:0;
        if(s->row==11){const char *current=rc_config_option_value(c,"drive_device");unsigned v=current?(unsigned)(current[0]=='1'?current[1]-'0'+2:current[0]-'8'):0;const char *values[]={"8","9","10","11"};rc_config_set_option(c,"drive_device",values[cycle(v,4,direction)]);return RC_SETUP_RESTART;}
        if(s->row>=12){rc_config_media effective;rc_config_media_settings(c,"disk",&effective);rc_config_media *disk=rc_config_media_slot(c,"disk");if(!disk)return 0;
            if(s->row==12){disk->fast=!effective.fast;disk->explicit_flags|=RC_MEDIA_SETTING_FAST;}
            else if(s->row==13){disk->sound=!effective.sound;disk->explicit_flags|=RC_MEDIA_SETTING_SOUND;}
            else {disk->autoload=!effective.autoload;disk->explicit_flags|=RC_MEDIA_SETTING_AUTOLOAD;}return RC_SETUP_MEDIA_OPTIONS;}
        rc_config_media *tape=rc_config_media_slot(c,"tape");if(!tape)return 0;
        if(s->row==7){tape->fast=!tape->fast;tape->explicit_flags|=RC_MEDIA_SETTING_FAST;}
        else if(s->row==8){tape->sound=!tape->sound;tape->explicit_flags|=RC_MEDIA_SETTING_SOUND;}
        else {tape->autoload=!tape->autoload;tape->explicit_flags|=RC_MEDIA_SETTING_AUTOLOAD;}
        return RC_SETUP_MEDIA_OPTIONS;
    }
    if(s->page==RC_SETUP_SOUND){if(!s->row)c->sound=!c->sound;else c->volume=(int)cycle((unsigned)c->volume/5,21,direction)*5;return RC_SETUP_AUDIO;}
    if(s->page==RC_SETUP_CONTROLLERS){
        if(s->row<4){if((s->row&1u)&&(c->sticks[s->row/2].type==RC_CONTROL_MOUSE1351||c->sticks[s->row/2].type==RC_CONTROL_NONE))return 0;rc_config_stick *st=&c->sticks[s->row/2];
            if(!(s->row&1u)){st->type=cycle(st->type,4,direction);if(st->type==RC_CONTROL_PADDLES)st->device=RC_STICK_MOUSE;else if(st->device==RC_STICK_MOUSE)st->device=RC_STICK_NONE;}
            else {unsigned source=st->device==RC_STICK_NONE?0:st->device==RC_STICK_KEYBOARD?1:st->device==RC_STICK_MOUSE?RC_CONFIG_PADS+2:1+st->pad;
                source=cycle(source,RC_CONFIG_PADS+3,direction);st->device=source==0?RC_STICK_NONE:source==1?RC_STICK_KEYBOARD:source==RC_CONFIG_PADS+2?RC_STICK_MOUSE:RC_STICK_CONTROLLER;st->pad=st->device==RC_STICK_CONTROLLER?source-1:0;}
        }else if(s->row==4)c->keyboard_joystick=!c->keyboard_joystick;
        else {const rc_config_stick temp=c->sticks[0];c->sticks[0]=c->sticks[1];c->sticks[1]=temp;c->keyboard_port^=1u;}
        return RC_SETUP_INPUT;
    }
    return 0;
}
static void row(rc_setup *s,unsigned index,const char *label,const char *value,int enabled){char line[37];snprintf(line,sizeof line," %-16.16s %15.15s ",label,value);const int selected=enabled&&s->row==index;
    rc_tui_put(&s->tui,6+index*2,2,line,(uint8_t)(enabled?(selected?6:14):12),(uint8_t)(selected?14:6));}
static void media_row(rc_setup *s,unsigned index,const char *label,const char *value,int enabled){char line[37];snprintf(line,sizeof line," %-16.16s %15.15s ",label,value);const int selected=enabled&&s->row==index;rc_tui_put(&s->tui,5+index,2,line,(uint8_t)(enabled?(selected?6:14):12),(uint8_t)(selected?14:6));}
void rc_setup_draw(rc_setup *s,const uint32_t palette[16],rc_frame *f){
    rc_tui_clear(&s->tui);char title[40];snprintf(title,sizeof title,"REC64  %s",PAGES[s->page]);rc_tui_bar(&s->tui,0,title);
    char page[40];snprintf(page,sizeof page,"< %s %u/8 >",PAGES[s->page],s->page+1);rc_tui_put(&s->tui,2,2,page,14,6);rc_tui_box(&s->tui,4,1,17,38);
    rc_config *c=s->config;
    switch(s->page){
    case RC_SETUP_MACHINE:{unsigned model=0;const rc_family *family=rc_family_of_model(c->model,&model);const char *sid=rc_config_option_value(c,"sid");
        row(s,0,"MODEL",family?family->models[model].name:c->model,1);row(s,1,"SID",sid?sid:"DEFAULT",strcmp(c->model,"test")!=0);row(s,2,"RESET","ENTER",1);row(s,3,"COLD BOOT","ENTER",1);row(s,4,"POWER OFF","ENTER",1);break;}
    case RC_SETUP_DISPLAY:row(s,0,"FILTER",c->look==RC_LOOK_CRT?"CRT":c->filter==RC_CONFIG_FILTER_LINEAR?"LINEAR":"NEAREST",1);
        row(s,1,"COLOURS",c->palette,1);row(s,2,"BORDER",c->border==RC_BORDER_NONE?"NONE":c->border==RC_BORDER_MINIMAL?"MINIMAL":"FULL",1);
        row(s,3,"FULL SCREEN",c->fullscreen?"ON":"OFF",1);row(s,4,"SCALE",c->scale?"INTEGER":"FIT",1);row(s,5,"PIXEL SHAPE",c->aspect?"SQUARE":"ORIGINAL",1);row(s,6,"VSYNC",c->vsync?"ON":"OFF",1);break;
    case RC_SETUP_SOUND:{char volume[16];snprintf(volume,sizeof volume,"%d%%",c->volume);row(s,0,"SOUND",c->sound?"ON":"OFF",1);row(s,1,"VOLUME",volume,1);break;}
    case RC_SETUP_MEDIA:{rc_config_media tape;rc_config_media_settings(c,"tape",&tape);
        media_row(s,0,"LOAD IMAGE","CTRL+F1",s->media_available);media_row(s,1,"SAVE","CTRL+F2",s->media_available);
        media_row(s,2,"PLAY",s->tape_playing?"PLAYING":"ENTER",s->has_tape);media_row(s,3,"STOP","ENTER",s->has_tape);media_row(s,4,"REWIND","ENTER",s->has_tape);
        media_row(s,5,"RECORD",s->tape_recording?"RECORDING":"ENTER",s->has_tape&&s->tape_writable);media_row(s,6,"EJECT TAPE","ENTER",s->has_tape);
        media_row(s,7,"TAPE FAST LOAD",tape.fast?"ON":"OFF",s->media_available);media_row(s,8,"TAPE SOUND",tape.sound?"ON":"OFF",s->media_available);media_row(s,9,"TAPE AUTO-LOAD",tape.autoload?"ON":"OFF",s->media_available);
        rc_config_media disk;rc_config_media_settings(c,"disk",&disk);const char *device=rc_config_option_value(c,"drive_device");
        media_row(s,10,"EJECT DISK","ENTER",s->has_disk);media_row(s,11,"DRIVE DEVICE",device?device:"8",s->media_available);
        media_row(s,12,"DISK FAST LOAD",disk.fast?"ON":"OFF",s->media_available);media_row(s,13,"DRIVE SOUNDS",disk.sound?"ON":"OFF",s->media_available);media_row(s,14,"DISK AUTO-LOAD",disk.autoload?"ON":"OFF",s->media_available);
        const char *name=s->row>=10?(disk.image[0]?disk.image:"NO DISK INSERTED"):(tape.image[0]?tape.image:"NO TAPE INSERTED");size_t length=strlen(name);rc_tui_put(&s->tui,3,2,length>36?name+length-36:name,12,6);break;}

    case RC_SETUP_CONTROLLERS:{const char *types[]={"JOYSTICK","1351 MOUSE","PADDLES","NONE"};char source[32];
        row(s,0,"PORT 1",types[c->sticks[0].type],1);rc_config_stick_device_name(&c->sticks[0],source,sizeof source);row(s,1,"INPUT 1",c->sticks[0].type==RC_CONTROL_MOUSE1351?"MOUSE":source,c->sticks[0].type!=RC_CONTROL_MOUSE1351&&c->sticks[0].type!=RC_CONTROL_NONE);
        row(s,2,"PORT 2",types[c->sticks[1].type],1);rc_config_stick_device_name(&c->sticks[1],source,sizeof source);row(s,3,"INPUT 2",c->sticks[1].type==RC_CONTROL_MOUSE1351?"MOUSE":source,c->sticks[1].type!=RC_CONTROL_MOUSE1351&&c->sticks[1].type!=RC_CONTROL_NONE);
        row(s,4,"KEYBOARD JOY",c->keyboard_joystick?"ON":"OFF",1);row(s,5,"SWAP PORTS","SHIFT+F11",1);
        rc_tui_put(&s->tui,19,3,"F11 KEYBOARD JOYSTICK ON/OFF",14,6);break;}

    case RC_SETUP_CARTRIDGE:{
        rc_config_media cart;rc_config_media_settings(c,"cartridge",&cart);const char *name=cart.image[0]?cart.image:"NO CARTRIDGE ATTACHED";size_t length=strlen(name);
        rc_tui_put(&s->tui,3,2,length>36?name+length-36:name,12,6);
        row(s,0,"LOAD CARTRIDGE","ENTER",s->cartridge_available);row(s,1,"EJECT CARTRIDGE","ENTER",s->has_cartridge);row(s,2,"FREEZE","RIGHT ALT",s->can_freeze);
        const char *reu=rc_config_option_value(c,"reu"),*geo=rc_config_option_value(c,"georam");
        row(s,3,"REU",reu?reu:"off",s->cartridge_available);row(s,4,"GEORAM",geo?geo:"off",s->cartridge_available);break;}
    case RC_SETUP_STATES:{
        for(unsigned i=0;i<RC_STATES_SLOTS;++i){char line[22];const int selected=s->row==i;
            snprintf(line,sizeof line,"%c%02u %-14.14s",(unsigned)c->state_slot==i+1?'>':' ',i+1,s->state_status[i]==1?(s->state_time[i][0]?s->state_time[i]+5:"SAVED"):s->state_status[i]==2?"INVALID":"EMPTY");
            rc_tui_put(&s->tui,6+i,2,line,(uint8_t)(selected?6:14),(uint8_t)(selected?14:6));}
        char slot_title[20];snprintf(slot_title,sizeof slot_title,"SLOT %02u",(unsigned)c->state_slot);rc_tui_put(&s->tui,5,25,slot_title,14,6);
        if(!s->state_thumb.width)rc_tui_put(&s->tui,10,24,s->state_status[c->state_slot-1]==1?"NO PREVIEW":"EMPTY SLOT",12,6);
        if(s->state_version[0]){char version[32];snprintf(version,sizeof version,"REC64 %s",s->state_version);rc_tui_put(&s->tui,16,23,version,12,6);}
        rc_tui_put(&s->tui,17,3,s->state_time[c->state_slot-1],14,6);
        rc_tui_put(&s->tui,18,3,"SAVE CURRENT SLOT  CTRL+F5",(uint8_t)(s->row==10?6:14),(uint8_t)(s->row==10?14:6));
        const int can_load=s->state_status[c->state_slot-1]==1;
        rc_tui_put(&s->tui,19,3,"LOAD CURRENT SLOT  CTRL+F6",(uint8_t)(can_load?(s->row==11?6:14):12),(uint8_t)(can_load&&s->row==11?14:6));break;}
    case RC_SETUP_ABOUT:rc_tui_put(&s->tui,7,3,"REC64 " RC_VERSION,14,6);rc_tui_put(&s->tui,10,3,"COMMODORE 64 / C64C",14,6);rc_tui_put(&s->tui,12,3,"GPL-3.0-OR-LATER",14,6);rc_tui_put(&s->tui,15,3,"SID: RESID 1.0-PRE2",14,6);break;
    }
    rc_tui_put(&s->tui,22,1,"LEFT/RIGHT PAGE  UP/DOWN SELECT",14,6);rc_tui_put(&s->tui,24,1,"ENTER CHANGE  +/- VALUE  F12 CLOSE",14,6);
    rc_tui_frame(&s->tui,palette,f);
    if(s->page==RC_SETUP_STATES&&s->state_thumb.width){const rc_states_thumb *t=&s->state_thumb;uint8_t map[256];
        for(unsigned i=0;i<t->colors;++i){uint32_t best=UINT32_MAX;map[i]=0;for(unsigned j=0;j<16;++j){const int r=(int)(t->palette[i]>>16&255u)-(int)(palette[j]>>16&255u),g=(int)(t->palette[i]>>8&255u)-(int)(palette[j]>>8&255u),b=(int)(t->palette[i]&255u)-(int)(palette[j]&255u);const uint32_t delta=(uint32_t)(r*r+g*g+b*b);if(delta<best){best=delta;map[i]=(uint8_t)j;}}}
        for(unsigned y=0;y<t->height;++y)for(unsigned x=0;x<t->width;++x)s->tui.pixels[(92u+y)*RC_TUI_W+216u+x]=map[t->pixels[y*t->width+x]];
    }
}
