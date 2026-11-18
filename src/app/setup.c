#include "setup.h"
#include "rc_version.h"
#include "about_logo.h"
#include <stdio.h>
#include <string.h>
static const char *const PAGES[]={"MACHINE","MEDIA","DISPLAY","SOUND","CONTROLLERS","CARTRIDGE","STATES","ABOUT"};
static const char *const COLORS[]={"default","color-corrected"};
static const char *const IDS[]={"machine","media","display","sound","controllers","cartridge","states","about"};
void rc_setup_init(rc_setup *s,rc_config *c){memset(s,0,sizeof *s);s->config=c;s->crt_available=1;rc_tui_init(&s->tui);
    for(unsigned i=0;i<8;++i)if(!strcmp(c->setup_page,IDS[i]))s->page=i;
    if(s->page==RC_SETUP_STATES)s->row=(unsigned)c->state_slot-1u;
}
static unsigned rows(const rc_setup *s){return s->page==RC_SETUP_MACHINE?7u:s->page==RC_SETUP_DISPLAY?7u:s->page==RC_SETUP_SOUND?2u:s->page==RC_SETUP_CONTROLLERS?6u:s->page==RC_SETUP_MEDIA?8u:s->page==RC_SETUP_CARTRIDGE?6u:s->page==RC_SETUP_STATES?13u:0;}
static unsigned cycle(unsigned value,unsigned count,int direction){return (value+count+(direction<0?count-1u:1u))%count;}

static int row_enabled(const rc_setup *s,unsigned index){
    if(index>=rows(s))return 0;
    const rc_config *c=s->config;
    switch(s->page){
    case RC_SETUP_MACHINE:
        if(index==1||index==2)return strcmp(c->model,"test")!=0;
        if(index==3)return s->media_available&&s->drive_choice;
        return 1;
    case RC_SETUP_MEDIA:
        if(!s->media_available)return 0;
        if(index==2||index==5){
            rc_config_media media;
            rc_config_media_settings(c,index==2?"disk":"tape",&media);
            return !media.fast;
        }
        return 1;
    case RC_SETUP_CONTROLLERS:
        if(index==4)return !s->controller_present;
        if(index==1||index==3){
            const unsigned type=c->sticks[index/2].type;
            return type!=RC_CONTROL_MOUSE1351&&type!=RC_CONTROL_NONE;
        }
        return 1;
    case RC_SETUP_CARTRIDGE:
        if(!s->cartridge_available)return 0;
        if(index==1||index==5)return s->has_cartridge;
        if(index==2)return s->can_freeze;
        return 1;
    case RC_SETUP_STATES:
        if(index==11)return s->state_status[c->state_slot-1]==1;
        if(index==12)return s->state_status[c->state_slot-1]!=0;
        return 1;
    default:return 1;
    }
}

static unsigned find_enabled_row(const rc_setup *s,unsigned first,int direction){
    const unsigned count=rows(s);
    for(unsigned i=0;i<count;++i){
        if(row_enabled(s,first))return first;
        first=cycle(first,count,direction);
    }
    return count;
}

static void ensure_selection(rc_setup *s){
    if(!row_enabled(s,s->row))s->row=find_enabled_row(s,s->row<rows(s)?s->row:0,1);
}

unsigned rc_setup_key(rc_setup *s,const char *key){
    if(s->delete_confirm){if(!strcmp(key,"RETURN")||!strcmp(key,"ENTER")||!strcmp(key,"Y")){s->delete_confirm=0;return RC_SETUP_STATE_DELETE;}if(!strcmp(key,"ESCAPE")||!strcmp(key,"N")||!strcmp(key,"F12"))s->delete_confirm=0;return 0;}
    if(!strcmp(key,"ESCAPE")||!strcmp(key,"F12")){s->active=0;return RC_SETUP_CLOSE;}
    if(!strcmp(key,"LEFT")||!strcmp(key,"RIGHT")||!strcmp(key,"TAB")){
        s->page=cycle(s->page,8,!strcmp(key,"LEFT")?-1:1);s->row=s->page==RC_SETUP_STATES?(unsigned)s->config->state_slot-1u:0;
        ensure_selection(s);
        snprintf(s->config->setup_page,sizeof s->config->setup_page,"%s",IDS[s->page]);return s->page==RC_SETUP_STATES?RC_SETUP_STATE_REFRESH:0;}
    const unsigned count=rows(s);if(!count)return 0;
    if(!strcmp(key,"UP")||!strcmp(key,"DOWN")){
        const int direction=!strcmp(key,"UP")?-1:1;
        const unsigned first=s->row<count?cycle(s->row,count,direction):(direction<0?count-1:0);
        s->row=find_enabled_row(s,first,direction);
        if(s->page==RC_SETUP_STATES&&s->row<RC_STATES_SLOTS){
            s->config->state_slot=(int)s->row+1;
            return RC_SETUP_STATE_REFRESH|RC_SETUP_STATE_SELECT;
        }
        return 0;
    }
    if(s->page==RC_SETUP_STATES&&!strcmp(key,"DELETE")){s->delete_confirm=row_enabled(s,12);return 0;}
    int direction=1;
    if(!strcmp(key,"-")||!strcmp(key,"PAGEDOWN"))direction=-1;
    else if(strcmp(key,"RETURN")&&strcmp(key,"ENTER")&&strcmp(key,"+")&&strcmp(key,"=")&&strcmp(key,"PAGEUP"))return 0;
    if(!row_enabled(s,s->row)){ensure_selection(s);return 0;}
    rc_config *c=s->config;
    if(s->page==RC_SETUP_CARTRIDGE){
        if(s->row<3)return !s->row?RC_SETUP_LOAD:s->row==1?RC_SETUP_EJECT_CART:RC_SETUP_FREEZE;
        if(s->row==5){rc_config_media *cart=rc_config_media_slot(c,"cartridge");if(cart)cart->startup=!cart->startup;return RC_SETUP_MEDIA_OPTIONS;}
        const rc_family *family=rc_family_of_model(c->model,NULL);const unsigned index=s->row==3?2u:3u,other=index==2?3u:2u;
        if(!family||family->option_count<=other)return 0;
        const rc_option_desc *option=&family->options[index];const char *current=rc_config_option_value(c,option->key);unsigned value=option->initial,value_count=0;
        while(option->values[value_count]){if(current&&!strcmp(current,option->values[value_count]))value=value_count;++value_count;}
        value=cycle(value,value_count,direction);rc_config_set_option(c,option->key,option->values[value]);
        if(value)rc_config_set_option(c,family->options[other].key,"off");return RC_SETUP_RESTART;
    }
    if(s->page==RC_SETUP_STATES){if(s->row<10){c->state_slot=(int)s->row+1;return RC_SETUP_STATE_REFRESH|RC_SETUP_STATE_SELECT;}if(s->row==12){s->delete_confirm=1;return 0;}return s->row==10?RC_SETUP_STATE_SAVE:RC_SETUP_STATE_LOAD;}
    if(s->page==RC_SETUP_MACHINE){
        if(!s->row&&!strcmp(c->model,"test")){snprintf(c->model,sizeof c->model,"c64-pal");return RC_SETUP_RESTART;}
        if(s->row<2){int c64c=!strncmp(c->model,"c64c-",5),ntsc=strstr(c->model,"ntsc")!=NULL;if(!s->row)c64c=!c64c;else ntsc=!ntsc;snprintf(c->model,sizeof c->model,"%s-%s",c64c?"c64c":"c64",ntsc?"ntsc":"pal");return RC_SETUP_RESTART;}
        if(s->row==2){const char *current=rc_config_option_value(c,"sid");unsigned v=current&&!strcmp(current,"8580");const char *values[]={"6581","8580"};rc_config_set_option(c,"sid",values[cycle(v,2,direction)]);return RC_SETUP_RESTART;}
        if(s->row==3){const char *current=rc_config_option_value(c,"drive_model");const unsigned v=current&&!strcmp(current,"1541-ii");rc_config_set_option(c,"drive_model",v?"1541":"1541-ii");return RC_SETUP_RESTART;}
        return s->row==4?RC_SETUP_RESET:s->row==5?RC_SETUP_COLD:RC_SETUP_OFF;
    }
    if(s->page==RC_SETUP_DISPLAY){
        switch(s->row){
        case 0:{unsigned v=c->look==RC_LOOK_CRT?0u:c->filter==RC_CONFIG_FILTER_LINEAR?2u:1u;v=cycle(v,3,direction);if(!v&&!s->crt_available)v=direction>0?1u:2u;c->look=v?RC_LOOK_PLAIN:RC_LOOK_CRT;c->filter=v==2?RC_CONFIG_FILTER_LINEAR:RC_CONFIG_FILTER_NEAREST;break;}
        case 1:{const unsigned v=!strcmp(c->palette,COLORS[1]);snprintf(c->palette,sizeof c->palette,"%s",COLORS[cycle(v,2,direction)]);break;}
        case 2:c->border=cycle(c->border,3,direction);break;
        case 3:c->fullscreen=!c->fullscreen;break;
        case 4:c->scale=!c->scale;break;
        case 5:c->aspect=!c->aspect;break;
        case 6:c->vsync=!c->vsync;break;
        }return RC_SETUP_VIDEO;
    }
    if(s->page==RC_SETUP_MEDIA){
        if(s->row==7)return RC_SETUP_MEDIA_FOLDER;
        if(s->row==0){const char *current=rc_config_option_value(c,"drive_device");unsigned v=current?(unsigned)(current[0]=='1'?current[1]-'0'+2:current[0]-'8'):0;const char *values[]={"8","9","10","11"};rc_config_set_option(c,"drive_device",values[cycle(v,4,direction)]);return RC_SETUP_RESTART;}
        const char *kind=s->row<4?"disk":"tape";rc_config_media effective;rc_config_media_settings(c,kind,&effective);rc_config_media *media=rc_config_media_slot(c,kind);if(!media)return 0;
        const unsigned field=s->row<4?s->row-1:s->row-4;
        if(field==0){media->fast=!effective.fast;media->explicit_flags|=RC_MEDIA_SETTING_FAST;}
        else if(field==1){
            media->sound=!effective.sound;media->explicit_flags|=RC_MEDIA_SETTING_SOUND;
        }
        else {media->autoload=!effective.autoload;media->explicit_flags|=RC_MEDIA_SETTING_AUTOLOAD;}
        return RC_SETUP_MEDIA_OPTIONS;
    }

    if(s->page==RC_SETUP_SOUND){if(!s->row)c->sound=!c->sound;else c->volume=(int)cycle((unsigned)c->volume/5,21,direction)*5;return RC_SETUP_AUDIO;}
    if(s->page==RC_SETUP_CONTROLLERS){
        if(s->row<4){rc_config_stick *st=&c->sticks[s->row/2];
            if(!(s->row&1u)){st->type=cycle(st->type,4,direction);if(st->type==RC_CONTROL_PADDLES)st->device=RC_STICK_MOUSE;else if(st->device==RC_STICK_MOUSE)st->device=RC_STICK_NONE;}
            else {const int mouse=st->type==RC_CONTROL_PADDLES;unsigned source=st->device==RC_STICK_CONTROLLER?st->pad:st->device==RC_STICK_MOUSE&&mouse?3u:0u;
                source=cycle(source,mouse?4u:3u,direction);st->device=source==0?RC_STICK_NONE:source==3?RC_STICK_MOUSE:RC_STICK_CONTROLLER;st->pad=st->device==RC_STICK_CONTROLLER?source:0;}

        }else if(s->row==4)return RC_SETUP_KEYBOARD;
        else return RC_SETUP_SWAP;
        return RC_SETUP_INPUT;
    }
    return 0;
}
static void option_text(rc_setup *s,unsigned index,unsigned y,unsigned x,const char *text){
    const int enabled=row_enabled(s,index);
    const int selected=enabled&&s->row==index;
    rc_tui_put(&s->tui,y,x,text,(uint8_t)(enabled?(selected?6:14):12),(uint8_t)(selected?14:6));
}
static void row(rc_setup *s,unsigned index,const char *label,const char *value){
    char line[37];
    snprintf(line,sizeof line," %-16.16s %15.15s ",label,value);
    /* Without a choice of drive the rows below its place move up. */
    if(s->page==RC_SETUP_MACHINE&&index==3&&!s->drive_choice)return;
    const unsigned y=s->page==RC_SETUP_MEDIA?5+index+(index>=4?2u:0u):6+index*2-(s->page==RC_SETUP_MACHINE&&index>3&&!s->drive_choice?2u:0u);
    option_text(s,index,y,2,line);
}
void rc_setup_draw(rc_setup *s,const uint32_t palette[16],rc_frame *f){
    ensure_selection(s);
    rc_tui_clear(&s->tui);char title[40];snprintf(title,sizeof title,"%s %u/8",PAGES[s->page],s->page+1);rc_tui_bar(&s->tui,0,title);
    rc_tui_box(&s->tui,3,1,s->page==RC_SETUP_STATES||s->page==RC_SETUP_ABOUT?19u:18u,38);
    rc_config *c=s->config;
    switch(s->page){
    case RC_SETUP_MACHINE:{const char *sid=rc_config_option_value(c,"sid");
        const char *drive=rc_config_option_value(c,"drive_model");row(s,0,"MODEL",!strcmp(c->model,"test")?"TEST":!strncmp(c->model,"c64c-",5)?"C64C":"C64");row(s,1,"VIDEO STANDARD",strstr(c->model,"ntsc")?"NTSC":"PAL");row(s,2,"SID TYPE",sid?sid:"6581");
        row(s,3,"DRIVE TYPE",drive&&!strcmp(drive,"1541-ii")?"1541-II":"1541");row(s,4,"RESET","ENTER");row(s,5,"COLD BOOT","ENTER");row(s,6,"POWER OFF","ENTER");break;}
    case RC_SETUP_DISPLAY:row(s,0,"FILTER",c->look==RC_LOOK_CRT?"CRT":c->filter==RC_CONFIG_FILTER_LINEAR?"LINEAR":"NEAREST");
        row(s,1,"PALETTE",c->palette);row(s,2,"BORDER",c->border==RC_BORDER_NONE?"NONE":c->border==RC_BORDER_MINIMAL?"MINIMAL":"FULL");
        row(s,3,"FULL SCREEN",c->fullscreen?"ON":"OFF");row(s,4,"SCALE",c->scale?"INTEGER":"FIT");row(s,5,"PIXEL SHAPE",c->aspect?"SQUARE":"ORIGINAL");row(s,6,"VSYNC",c->vsync?"ON":"OFF");break;
    case RC_SETUP_SOUND:{char volume[16];snprintf(volume,sizeof volume,"%d%%",c->volume);row(s,0,"SOUND",c->sound?"ON":"OFF");row(s,1,"VOLUME",volume);break;}
    case RC_SETUP_MEDIA:{rc_config_media disk,tape;rc_config_media_settings(c,"disk",&disk);rc_config_media_settings(c,"tape",&tape);const char *device=rc_config_option_value(c,"drive_device");
        rc_tui_put(&s->tui,4,3,"DRIVE",1,6);rc_tui_put(&s->tui,10,3,"TAPE",1,6);
        row(s,0,"DEVICE",device?device:"8");row(s,1,"FAST LOAD",disk.fast?"ON":"OFF");
        row(s,2,"SOUNDS",disk.sound&&!disk.fast?"ON":"OFF");row(s,3,"AUTO-LOAD",disk.autoload?"ON":"OFF");
        row(s,4,"FAST LOAD",tape.fast?"ON":"OFF");row(s,5,"SOUND",tape.sound&&!tape.fast?"ON":"OFF");row(s,6,"AUTO-LOAD",tape.autoload?"ON":"OFF");
        option_text(s,7,16,3,"DEFAULT MEDIA FOLDER");option_text(s,7,16,32,"ENTER");
        const char *folder=c->media_folder[0]?c->media_folder:"PROGRAM FOLDER";const size_t length=strlen(folder);rc_tui_put(&s->tui,18,3,length>34?folder+length-34:folder,14,6);break;}

    case RC_SETUP_CONTROLLERS:{const char *types[]={"JOYSTICK","1351 MOUSE","PADDLES","NONE"};char source[32];
        row(s,0,"PORT 1",types[c->sticks[0].type]);rc_config_stick_device_name(&c->sticks[0],source,sizeof source);row(s,1,"INPUT 1",c->sticks[0].type==RC_CONTROL_MOUSE1351?"MOUSE":source);
        row(s,2,"PORT 2",types[c->sticks[1].type]);rc_config_stick_device_name(&c->sticks[1],source,sizeof source);row(s,3,"INPUT 2",c->sticks[1].type==RC_CONTROL_MOUSE1351?"MOUSE":source);
        char label[37],line[37];snprintf(label,sizeof label,"KEYBOARD JOYSTICK (%s)",s->keyboard_active?(c->keyboard_port?"P2":"P1"):s->controller_present?"AUTO OFF":"OFF");snprintf(line,sizeof line," %-28.28s %3s ",label,"F11");
        option_text(s,4,14,2,line);
        row(s,5,"SWAP PORTS","SHIFT+F11");break;}

    case RC_SETUP_CARTRIDGE:{
        rc_config_media cart;rc_config_media_settings(c,"cartridge",&cart);const char *name=cart.image[0]?cart.image:"NO CARTRIDGE ATTACHED";size_t length=strlen(name);
        rc_tui_put(&s->tui,2,2,length>36?name+length-36:name,12,6);
        row(s,0,"LOAD CARTRIDGE","ENTER");row(s,1,"EJECT CARTRIDGE","ENTER");row(s,2,"FREEZE","RIGHT ALT");
        const char *reu=rc_config_option_value(c,"reu"),*geo=rc_config_option_value(c,"georam");
        row(s,3,"REU",reu?reu:"off");row(s,4,"GEORAM",geo?geo:"off");
        row(s,5,"START INSERTED",cart.startup?"ON":"OFF");break;}
    case RC_SETUP_STATES:{
        for(unsigned i=0;i<RC_STATES_SLOTS;++i){char line[22];const int selected=s->row==i;
            snprintf(line,sizeof line,"%c%02u %-14.14s",(unsigned)c->state_slot==i+1?'>':' ',i+1,s->state_status[i]==1?(s->state_time[i][0]?s->state_time[i]+5:"SAVED"):s->state_status[i]==2?"INVALID":"EMPTY");
            rc_tui_put(&s->tui,6+i,2,line,(uint8_t)(selected?6:14),(uint8_t)(selected?14:6));}
        char slot_title[20];snprintf(slot_title,sizeof slot_title,"SLOT %02u",(unsigned)c->state_slot);rc_tui_put(&s->tui,5,25,slot_title,14,6);
        if(!s->state_thumb.width)rc_tui_put(&s->tui,10,24,s->state_status[c->state_slot-1]==1?"NO PREVIEW":"EMPTY SLOT",12,6);
        if(s->state_version[0]){char version[32];snprintf(version,sizeof version,"REC64 %s",s->state_version);rc_tui_put(&s->tui,16,23,version,12,6);}
        rc_tui_put(&s->tui,17,3,s->state_time[c->state_slot-1],14,6);
        option_text(s,10,18,3,"SAVE CURRENT SLOT  CTRL+F5");
        option_text(s,11,19,3,"LOAD CURRENT SLOT  CTRL+F6");
        option_text(s,12,20,3,"DELETE CURRENT SLOT  DEL/Y");break;}
    case RC_SETUP_ABOUT:
        rc_tui_put(&s->tui,10,3,"NEW IN " RC_VERSION,1,6);
        rc_tui_put(&s->tui,11,3,"FULL SPEED RUNS TWICE AS FAST",14,6);
        rc_tui_put(&s->tui,12,3,"SPRITES ON MULTICOLOUR TEXT FIXED",14,6);
        rc_tui_put(&s->tui,13,3,"QUICK SAVE KEEPS A PENDING RUN",14,6);
        rc_tui_put(&s->tui,16,3,"ALSO IN 0.21",1,6);
        rc_tui_put(&s->tui,17,3,"FIRMWARE SCREEN LISTS MISSING ROMS",14,6);
        rc_tui_put(&s->tui,18,3,"ONE DRIVE ROM IS ENOUGH",14,6);
        rc_tui_put(&s->tui,19,3,"COLOR-CORRECTED HAS A TRUE BLUE",14,6);
        rc_tui_put(&s->tui,20,3,"DRIVE AND TAPE SOUNDS MATCH",14,6);
        break;
    }
    if(s->page==RC_SETUP_ABOUT)rc_tui_put(&s->tui,24,1,"LEFT/RIGHT PAGE  F12 CLOSE",14,6);
    else {
        rc_tui_put(&s->tui,22,1,"LEFT/RIGHT PAGE  UP/DOWN SELECT",14,6);
        rc_tui_put(&s->tui,24,1,"ENTER CHANGE  +/- VALUE  F12 CLOSE",14,6);
    }
    if(s->delete_confirm){rc_tui_box(&s->tui,9,3,6,34);for(unsigned y=10;y<14;++y)rc_tui_put(&s->tui,y,4,"                                ",14,6);rc_tui_put(&s->tui,10,5,"DELETE CURRENT SNAPSHOT?",14,6);rc_tui_put(&s->tui,13,5,"ENTER/A DELETE  ESC/B CANCEL",14,6);}
    rc_tui_frame(&s->tui,palette,f);
    if(s->page==RC_SETUP_ABOUT&&palette){memcpy(s->about_palette,palette,16*sizeof *palette);for(unsigned i=0;i<240;++i){const uint32_t color=RC_ABOUT_COLORS[i],a=color>>24;uint32_t blended=0;for(unsigned shift=0;shift<=16;shift+=8)blended|=((((color>>shift)&255u)*a+((palette[6]>>shift)&255u)*(255u-a)+127u)/255u)<<shift;s->about_palette[16+i]=blended;}
        const unsigned left=(RC_TUI_W-RC_ABOUT_W)/2;
        for(unsigned y=0;y<RC_ABOUT_H;++y)
            for(unsigned x=0;x<RC_ABOUT_W;++x)
                s->tui.pixels[(68u+y)*RC_TUI_W+left+x]=(uint8_t)(16u+RC_ABOUT_PIXELS[y*RC_ABOUT_W+x]);
        f->palette=s->about_palette;f->colors=256;
    }
    if(s->page==RC_SETUP_STATES&&s->state_thumb.width&&!s->delete_confirm){const rc_states_thumb *t=&s->state_thumb;uint8_t map[256];
        for(unsigned i=0;i<t->colors;++i){uint32_t best=UINT32_MAX;map[i]=0;for(unsigned j=0;j<16;++j){const int r=(int)(t->palette[i]>>16&255u)-(int)(palette[j]>>16&255u),g=(int)(t->palette[i]>>8&255u)-(int)(palette[j]>>8&255u),b=(int)(t->palette[i]&255u)-(int)(palette[j]&255u);const uint32_t delta=(uint32_t)(r*r+g*g+b*b);if(delta<best){best=delta;map[i]=(uint8_t)j;}}}
        for(unsigned y=0;y<t->height;++y)for(unsigned x=0;x<t->width;++x)s->tui.pixels[(92u+y)*RC_TUI_W+216u+x]=map[t->pixels[y*t->width+x]];
    }
}
