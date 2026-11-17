/* Only the upstream public sid.h API is used. Its State does not include the
 * analogue filters or FIR history. Replaying the compressed access journal
 * reconstructs those as well, so snapshots resume with identical samples. */
#include "sid.h"
#include "../third_party/resid/sid.h"
#include <deque>
#include <memory>
#include <vector>
#include <limits>
#include <new>
#include <cstring>
#include <cstdlib>
extern "C" {
/* The core C declarations have C linkage when included by this bridge. */
}
enum { CLOCK=1,WRITE=2,READ=3 };
struct entry {uint8_t kind,reg,value;uint32_t cycles;};
static uint32_t probe_bus_lifetime(unsigned model){
    void *storage=std::calloc(1,sizeof(reSID::SID));if(!storage)throw std::bad_alloc();
    reSID::SID *probe=nullptr;
    try{probe=new(storage) reSID::SID();probe->set_chip_model(model==RC_SID_8580?reSID::MOS8580:reSID::MOS6581);
        const uint32_t ttl=static_cast<uint32_t>(probe->read_state().bus_value_ttl);probe->~SID();std::free(storage);return ttl;
    }catch(...){if(probe)probe->~SID();std::free(storage);throw;}
}
static uint32_t nominal_bus_lifetime(unsigned model){
    /* read_state changes the oracle's live bus. Query isolated instances once. */
    static const uint32_t ttl[2]={probe_bus_lifetime(RC_SID_6581),probe_bus_lifetime(RC_SID_8580)};return ttl[model];
}
struct engine {
    /* The pinned library expects zeroed construction storage on this host. */
    static void *operator new(size_t bytes){void *p=std::calloc(1,bytes);if(!p)throw std::bad_alloc();return p;}
    static void operator delete(void *p){std::free(p);}
    reSID::SID sid;
    std::vector<entry> journal;
    std::deque<int16_t> samples;
    uint32_t model,hz,rate,bus_lifetime;
    uint8_t pot_bus=0;uint32_t pot_ttl=0;
    bool failed;
    engine(uint32_t m,uint32_t h,uint32_t r):model(m),hz(h),rate(r),bus_lifetime(nominal_bus_lifetime(m)),failed(false){
        sid.set_chip_model(m==RC_SID_8580?reSID::MOS8580:reSID::MOS6581);
        if(!sid.set_sampling_parameters(h,reSID::SAMPLE_RESAMPLE,r))throw std::bad_alloc();
        sid.reset();
    }
    void record(entry e){
        if(e.kind==CLOCK&&!journal.empty()&&journal.back().kind==CLOCK&&e.cycles<=UINT32_MAX-journal.back().cycles)journal.back().cycles+=e.cycles;
        else journal.push_back(e);
    }
    void clock(uint32_t cycles,bool keep){
        while(cycles){
            const uint32_t slice=cycles>100000?100000:cycles;
            reSID::cycle_count left=static_cast<reSID::cycle_count>(slice);
            while(left){
                short out[1024];const int n=sid.clock(left,out,1024);
                if(keep)for(int i=0;i<n;++i){samples.push_back(out[i]);if(samples.size()>rate)samples.pop_front();}
            }
            cycles-=slice;
        }
    }
};
static engine *get(rc_sid *s){return static_cast<engine *>(s->engine);}
static const engine *get(const rc_sid *s){return static_cast<const engine *>(s->engine);}
static uint8_t read_dev(void *p,unsigned space,uint32_t off){(void)space;return rc_sid_read(static_cast<rc_sid *>(p),off);}
static void write_dev(void *p,unsigned space,uint32_t off,uint8_t v){(void)space;rc_sid_write(static_cast<rc_sid *>(p),off,v);}
static void reset_dev(void *p){rc_sid_reset(static_cast<rc_sid *>(p));}
static void save_dev(const void *p,rc_state_writer *w){rc_sid_save(static_cast<const rc_sid *>(p),w);}
static int load_dev(void *p,rc_state_reader *r){return rc_sid_load(static_cast<rc_sid *>(p),r);}
static const rc_dev_ops OPS={read_dev,write_dev,nullptr,reset_dev,nullptr,save_dev,load_dev};
int rc_sid_init(rc_sid *s,const rc_hw_host *host,unsigned model,uint32_t hz,uint32_t rate){
    s->dev=rc_dev{&OPS,host,"SID"};s->engine=nullptr;
    if(model>RC_SID_8580||hz<100000||hz>2000000||rate<8000||rate>192000)return 0;
    try{s->engine=new engine(model,hz,rate);return 1;}catch(...){return 0;}
}
void rc_sid_release(rc_sid *s){delete get(s);s->engine=nullptr;}
void rc_sid_reset(rc_sid *s){engine *e=get(s);if(!e)return;try{std::unique_ptr<engine> next(new engine(e->model,e->hz,e->rate));delete e;s->engine=next.release();}catch(...){e->failed=true;}}
uint8_t rc_sid_read(rc_sid *s,unsigned reg){engine *e=get(s);if(!e||e->failed)return 255;try{e->record(entry{READ,static_cast<uint8_t>(reg&31u),0,0});const uint8_t value=static_cast<uint8_t>(e->sid.read(reg&31u));
        if((reg&31u)>=25&&(reg&31u)<=28){e->pot_ttl=0;return value;}return e->pot_ttl?e->pot_bus:value;}catch(...){e->failed=true;return 255;}}
uint8_t rc_sid_pot_read(rc_sid *s,unsigned reg,uint8_t value){
    rc_sid_read(s,reg);engine *e=get(s);if(!e||e->failed)return 255;
    e->pot_bus=value;e->pot_ttl=e->bus_lifetime;return value;
}
uint8_t rc_sid_peek(rc_sid *s,unsigned reg){
    engine *e=get(s);if(!e||e->failed)return 255;reg&=31u;if(e->pot_ttl&&(reg<25||reg>28))return e->pot_bus;
    /* A debugger read must not call read_state on the live chip: that public
     * API performs register reads and changes its bus. Replay into a copy. */
    try{std::unique_ptr<engine> copy(new engine(e->model,e->hz,e->rate));
        for(const entry &v:e->journal){if(v.kind==CLOCK)copy->clock(v.cycles,false);else if(v.kind==WRITE)copy->sid.write(v.reg,v.value);else copy->sid.read(v.reg);}
        return static_cast<uint8_t>(copy->sid.read(reg));
    }catch(...){return 255;}
}
void rc_sid_write(rc_sid *s,unsigned reg,uint8_t v){engine *e=get(s);if(!e||e->failed)return;try{e->record(entry{WRITE,static_cast<uint8_t>(reg&31u),v,0});e->sid.write(reg&31u,v);e->pot_ttl=0;}catch(...){e->failed=true;}}
int rc_sid_clock(rc_sid *s,uint32_t cycles){engine *e=get(s);if(!e||e->failed)return 0;try{if(cycles){e->record(entry{CLOCK,0,0,cycles});e->clock(cycles,true);e->pot_ttl=cycles>=e->pot_ttl?0:e->pot_ttl-cycles;}return 1;}catch(...){e->failed=true;return 0;}}
size_t rc_sid_audio_pull(rc_sid *s,int16_t *mono,size_t frames){engine *e=get(s);if(!e||e->failed)return 0;size_t n=0;while(n<frames&&!e->samples.empty()){mono[n++]=e->samples.front();e->samples.pop_front();}return n;}
int rc_sid_ok(const rc_sid *s){return s->engine&&!get(s)->failed;}
void rc_sid_save(const rc_sid *s,rc_state_writer *w){
    const engine *e=get(s);if(!e||e->failed||e->journal.size()>UINT32_MAX){w->failed=1;return;}
    rc_sw_u32(w,1);rc_sw_u32(w,e->model);rc_sw_u32(w,e->hz);rc_sw_u32(w,e->rate);rc_sw_u32(w,static_cast<uint32_t>(e->journal.size()));
    for(const entry &v:e->journal){rc_sw_u8(w,v.kind);rc_sw_u8(w,v.reg);rc_sw_u8(w,v.value);rc_sw_u32(w,v.cycles);}
    rc_sw_u32(w,static_cast<uint32_t>(e->samples.size()));for(int16_t sample:e->samples)rc_sw_u16(w,static_cast<uint16_t>(sample));
    rc_sw_u8(w,e->pot_bus);rc_sw_u32(w,e->pot_ttl);
}
int rc_sid_load(rc_sid *s,rc_state_reader *r){
    if(r->end-r->at<24||rc_sr_u32(r)!=1)return 0;
    const uint32_t model=rc_sr_u32(r),hz=rc_sr_u32(r),rate=rc_sr_u32(r),count=rc_sr_u32(r);
    if(model>1||hz<100000||hz>2000000||rate<8000||rate>192000||count>(r->end-r->at-4)/7)return 0;
    try{
        std::unique_ptr<engine> next(new engine(model,hz,rate));next->journal.reserve(count);
        for(uint32_t i=0;i<count;++i){entry v;v.kind=rc_sr_u8(r);v.reg=rc_sr_u8(r);v.value=rc_sr_u8(r);v.cycles=rc_sr_u32(r);
            if(v.reg>31)return 0;
            if(v.kind==CLOCK)next->clock(v.cycles,false);else if(v.kind==WRITE)next->sid.write(v.reg,v.value);else if(v.kind==READ)next->sid.read(v.reg);else return 0;
            next->journal.push_back(v);
        }
        const uint32_t samples=rc_sr_u32(r);if(samples>rate||samples>(r->end-r->at)/2)return 0;
        for(uint32_t i=0;i<samples;++i)next->samples.push_back(static_cast<int16_t>(rc_sr_u16(r)));
        if(r->end-r->at>=5){next->pot_bus=rc_sr_u8(r);next->pot_ttl=rc_sr_u32(r);}
        if(!rc_sr_ok(r))return 0;delete get(s);s->engine=next.release();return 1;
    }catch(...){return 0;}
}
