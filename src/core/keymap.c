#include "keymap.h"
#include <ctype.h>
#include <string.h>
static int same(const char *a,const char *b){while(*a&&*b){if(toupper((unsigned char)*a++)!=toupper((unsigned char)*b++))return 0;}return *a==*b;}
int rc_keymap_action(const rc_family *f,const char *name,uint8_t out[2]){
    for(unsigned i=0;i<f->key_count;++i)if(same(name,f->keys[i].name)){out[0]=(uint8_t)i;out[1]=RC_KEY_NONE;return 1;}
    for(unsigned i=0;i<f->combo_count;++i)if(same(name,f->combos[i].host)){out[0]=f->combos[i].key;out[1]=f->combos[i].shift;return 1;}
    return 0;
}
static int is_shift(const rc_family *f,unsigned key){return same(f->keys[key].name,"LEFT SHIFT")||same(f->keys[key].name,"RIGHT SHIFT");}
static void apply(rc_keymap *k){
    const rc_family *f=rc_machine_family(k->machine);const rc_typing_desc *ch=k->count?&k->queue[k->read]:NULL;
    for(unsigned i=0;i<f->key_count&&i<256;++i){int down=k->held[i]!=0||k->virtual_keys[i]!=0;
        if(ch){if(is_shift(f,i))down=0;if(k->phase<3&&(ch->key==i||ch->shift==i))down=1;}
        if(k->sent[i]!=down){rc_machine_key(k->machine,i,down);k->sent[i]=(uint8_t)down;}}
}
void rc_keymap_init(rc_keymap *k,rc_machine *m,const rc_config *c){memset(k,0,sizeof *k);memset(k->keys,RC_KEY_NONE,sizeof k->keys);k->machine=m;k->config=c;}
void rc_keymap_clear(rc_keymap *k){memset(k->held,0,sizeof k->held);memset(k->virtual_keys,0,sizeof k->virtual_keys);memset(k->keys,RC_KEY_NONE,sizeof k->keys);k->count=k->read=k->phase=0;apply(k);}
void rc_keymap_virtual(rc_keymap *k,unsigned key,int down){if(key<rc_machine_family(k->machine)->key_count&&key<256){k->virtual_keys[key]=(uint8_t)(down!=0);apply(k);}}
int rc_keymap_key(rc_keymap *k,unsigned host,const char *name,int down){
    if(host>=RC_KEYMAP_HOSTS)return 0;uint8_t *keys=k->keys[host];
    if(!down){const int used=keys[0]!=RC_KEY_NONE;for(unsigned i=0;i<2;++i)if(keys[i]!=RC_KEY_NONE&&k->held[keys[i]])--k->held[keys[i]];keys[0]=keys[1]=RC_KEY_NONE;apply(k);return used;}
    if(keys[0]!=RC_KEY_NONE)return 1;
    const rc_family *f=rc_machine_family(k->machine);const rc_config *c=k->config;uint8_t found[2]={RC_KEY_NONE,RC_KEY_NONE};int bound=0;
    for(unsigned i=0;i<c->binding_count;++i)if(same(name,c->bindings[i].host)){bound=rc_keymap_action(f,c->bindings[i].key,found);break;}
    if(!bound){
        for(unsigned i=0;i<f->combo_count;++i)if(same(name,f->combos[i].host)){
            int remapped=0;for(unsigned b=0;b<c->binding_count;++b)if(same(c->bindings[b].key,f->combos[i].host))remapped=1;
            if(!remapped){found[0]=f->combos[i].key;found[1]=f->combos[i].shift;bound=1;}break;}
    }
    int control=0;for(unsigned i=0;i<f->key_count;++i)if(k->held[i]&&(same(f->keys[i].name,"CTRL")||same(f->keys[i].name,"COMMODORE")))control=1;
    if(!bound)for(unsigned i=0;i<f->key_count;++i){
        if(!f->keys[i].host||!same(name,f->keys[i].host))continue;
        int remapped=0;for(unsigned b=0;b<c->binding_count;++b)if(same(c->bindings[b].key,f->keys[i].name))remapped=1;
        /* Printable keys arrive through text input, except control chords. */
        const int printable=strlen(name)==1||same(name,"Space");
        if(!remapped&&(!printable||control)){found[0]=(uint8_t)i;bound=1;}break;}
    if(!bound)return 0;
    /* Queue editing keys after pending text, respecting remapped bindings. */
    if(k->count&&found[1]==RC_KEY_NONE){const char *action=f->keys[found[0]].name;
        if(same(action,"RETURN")||same(action,"DELETE"))return rc_keymap_text(k,same(action,"RETURN")?"\n":"\b");}
    memcpy(keys,found,2);for(unsigned i=0;i<2;++i)if(keys[i]!=RC_KEY_NONE)++k->held[keys[i]];apply(k);return 1;
}
int rc_keymap_text(rc_keymap *k,const char *text){
    const rc_family *f=rc_machine_family(k->machine);rc_typing_desc chars[RC_KEYMAP_QUEUE];unsigned count=0;
    const unsigned char *p=(const unsigned char *)text;
    while(*p){uint32_t ch=*p++;
        if(ch>=0xc2&&ch<=0xdf){if((*p&0xc0)!=0x80)return 0;ch=(ch&31u)<<6|(*p++&63u);}
        else if(ch>=0xe0&&ch<=0xef){if(!p[0]||!p[1]||(p[0]&0xc0)!=0x80||(p[1]&0xc0)!=0x80)return 0;ch=(ch&15u)<<12|(p[0]&63u)<<6|(p[1]&63u);p+=2;}
        else if(ch>=128)return 0;
        uint8_t key,shift;if(!rc_family_typing(f,(int)ch,&key,&shift)||k->count+count==RC_KEYMAP_QUEUE)return 0;
        chars[count++]=(rc_typing_desc){ch,key,shift};
    }
    for(unsigned i=0;i<count;++i)k->queue[(k->read+k->count++)%RC_KEYMAP_QUEUE]=chars[i];apply(k);return 1;
}
void rc_keymap_frame(rc_keymap *k){
    if(k->count){unsigned duration=6;
        if(k->count>1&&k->queue[k->read].key==k->queue[(k->read+1)%RC_KEYMAP_QUEUE].key)duration+=6;
        if(++k->phase>=duration){k->phase=0;k->read=(k->read+1)%RC_KEYMAP_QUEUE;--k->count;}apply(k);}
}
void rc_keymap_save(const rc_keymap *k,rc_state_writer *w){
    uint8_t sent[256]={0};const unsigned n=rc_machine_family(k->machine)->key_count;memcpy(sent,k->sent,n<256?n:256);
    rc_sw_u32(w,1);rc_sw_u32(w,k->count);rc_sw_u32(w,k->phase);rc_sw_bytes(w,sent,sizeof sent);
    for(unsigned i=0;i<k->count;++i){const rc_typing_desc *ch=&k->queue[(k->read+i)%RC_KEYMAP_QUEUE];rc_sw_u32(w,ch->ch);rc_sw_u8(w,ch->key);rc_sw_u8(w,ch->shift);}
}
int rc_keymap_load(rc_keymap *k,rc_state_reader *r){
    if(rc_sr_u32(r)!=1)return 0;const unsigned count=rc_sr_u32(r),phase=rc_sr_u32(r);const rc_family *f=rc_machine_family(k->machine);
    if(count>RC_KEYMAP_QUEUE||phase>=12||r->end-r->at<sizeof k->sent+count*6u)return 0;
    rc_sr_bytes(r,k->sent,sizeof k->sent);memset(k->held,0,sizeof k->held);memset(k->keys,RC_KEY_NONE,sizeof k->keys);k->read=0;k->count=count;k->phase=phase;
    for(unsigned i=0;i<count;++i){rc_typing_desc *ch=&k->queue[i];ch->ch=rc_sr_u32(r);ch->key=rc_sr_u8(r);ch->shift=rc_sr_u8(r);if(ch->key>=f->key_count||(ch->shift!=RC_KEY_NONE&&ch->shift>=f->key_count))return 0;}
    if(!rc_sr_ok(r))return 0;memset(k->sent,255,sizeof k->sent);apply(k);return 1;
}
