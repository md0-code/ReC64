/* Independent executable comparison: public reSID sid.h, original register logs. */
#include "../third_party/resid/sid.h"
#include "../hw/sid_own.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
struct Event { unsigned cycle, reg, value; char kind; };
struct Oracle {
    void *storage;
    reSID::SID *sid;
    explicit Oracle(unsigned model) : storage(std::calloc(1,sizeof(reSID::SID))), sid(nullptr) {
        if(!storage) throw std::bad_alloc();
        sid=new(storage) reSID::SID();
        sid->set_chip_model(model ? reSID::MOS8580 : reSID::MOS6581);
        sid->reset(); sid->enable_external_filter(false);
    }
    ~Oracle(){if(sid) sid->~SID(); std::free(storage);}
    Oracle(const Oracle&)=delete; Oracle& operator=(const Oracle&)=delete;
};
struct Difference {
    unsigned long long count=0;
    unsigned cycle=0, voice=0, expected=0, actual=0;
    void compare(unsigned c,unsigned v,unsigned e,unsigned a) {
        if(e==a) return;
        if(!count){cycle=c;voice=v;expected=e;actual=a;} ++count;
    }
    void emit(FILE *f,const char *name) const {
        std::fprintf(f,"\"%s\":{\"mismatches\":%llu,\"first\":{\"cycle\":%u,\"voice\":%u,\"expected\":%u,\"actual\":%u}}",
                     name,count,cycle,voice,expected,actual);
    }
};
struct Audio {
    unsigned begin=0, count=0;
    double error=0, ref_energy=0, candidate_energy=0, product=0;
    void add(int ref,int candidate){
        const double r=ref,c=candidate,d=c-r;
        ++count; error+=d*d;ref_energy+=r*r;candidate_energy+=c*c;product+=r*c;
    }
    void emit(FILE *f) const {
        const double rms=ref_energy ? std::sqrt(error/ref_energy) : (error ? 1e30 : 0);
        const double corr=ref_energy && candidate_energy ? product/std::sqrt(ref_energy*candidate_energy) : (!error ? 1 : 0);
        std::fprintf(f,"{\"begin\":%u,\"cycles\":%u,\"relative_rms_error\":%.12g,\"uncentered_correlation\":%.12g,\"reference_energy\":%.12g}",
                     begin,count,rms,corr,ref_energy);
    }
};
unsigned mapped(unsigned reg,unsigned rotation){
    return reg<21 ? (((reg/7+rotation)%3)*7+reg%7) : reg;
}
void discard_audio(rc_sid_own *s){int16_t data[64];while(rc_sid_own_audio_pull(s,data,64)){} }
std::vector<uint8_t> save(const rc_sid_own *s){
    rc_state_writer w;rc_sw_init(&w);rc_sw_header(&w,"sid-comparison","candidate",nullptr,0);
    rc_sw_begin(&w,"SIDO",0);rc_sid_own_save(s,&w);rc_sw_end(&w);
    if(!rc_sw_finish(&w)){rc_sw_free(&w);throw std::runtime_error("candidate save failed");}
    std::vector<uint8_t> bytes(w.data,w.data+w.size);rc_sw_free(&w);return bytes;
}
bool load(rc_sid_own *s,const std::vector<uint8_t>& bytes){
    rc_state_reader r;char why[128];
    return rc_sr_open(&r,bytes.data(),bytes.size(),why,sizeof(why)) && rc_sr_chunk(&r,"SIDO",0) && rc_sid_own_load(s,&r);
}
bool probes_equal(const rc_sid_own *a,const rc_sid_own *b){
    for(unsigned v=0;v<3;++v){rc_sid_own_probe x{},y{};rc_sid_own_get_probe(a,v,&x);rc_sid_own_get_probe(b,v,&y);
        if(x.phase!=y.phase||x.lfsr!=y.lfsr||x.waveform!=y.waveform||x.envelope!=y.envelope)return false;}
    return rc_sid_own_output_cycle(a)==rc_sid_own_output_cycle(b);
}
}
int main(int argc,char **argv){
    try {
        const char *log_path=nullptr,*out_path=nullptr;unsigned model=2;
        for(int i=1;i<argc;++i){
            if(i+1>=argc)throw std::runtime_error("option needs value");
            const std::string flag=argv[i];const char *value=argv[++i];
            if(flag=="--log")log_path=value;
            else if(flag=="--out")out_path=value;
            else if(flag=="--model")model=std::strcmp(value,"6581")==0 ? 0 : (std::strcmp(value,"8580")==0 ? 1 : 2);
            else throw std::runtime_error("unknown option");
        }
        if(!log_path||!out_path||model>1)throw std::runtime_error("usage: --log events.txt --model 6581|8580 --out result.json");
        std::ifstream input(log_path);unsigned cycles=0,segment_cycles=0,hz=0;
        if(!(input>>cycles>>segment_cycles>>hz)||cycles<8192||cycles>2000000||hz!=985248||segment_cycles>cycles)
            throw std::runtime_error("invalid log header");
        std::vector<Event> events;Event event{};unsigned last=0;
        while(input>>event.cycle>>event.kind>>event.reg>>event.value){
            if(event.cycle>=cycles||(!events.empty()&&event.cycle<=last)||event.reg>31||event.value>255||(event.kind!='w'&&event.kind!='r'))
                throw std::runtime_error("invalid event");
            events.push_back(event);last=event.cycle;
        }
        if(!input.eof())throw std::runtime_error("malformed event");
        std::array<std::unique_ptr<Oracle>,3> oracles;
        auto candidates=std::unique_ptr<rc_sid_own[]>(new rc_sid_own[3]);
        auto resumed=std::unique_ptr<rc_sid_own>(new rc_sid_own);
        for(unsigned rotation=0;rotation<3;++rotation){
            oracles[rotation].reset(new Oracle(model));
            if(!rc_sid_own_init(&candidates[rotation],model,hz,48000))throw std::runtime_error("candidate init failed");
            rc_sid_own_set_external_filter(&candidates[rotation],0);
        }
        Difference phase,lfsr,envelope,osc8,env3,reads,continuation;
        std::vector<Audio> segments; Audio audio; size_t at=0;
        bool resumed_active=false;unsigned continuation_cycles=0;
        const unsigned midpoint=cycles/2;
        for(unsigned cycle=0;cycle<cycles;++cycle){
            if(cycle==midpoint){
                if(!rc_sid_own_init(resumed.get(),model,hz,48000)||!load(resumed.get(),save(&candidates[0])))
                    throw std::runtime_error("candidate midpoint load failed");
                resumed_active=true;
            }
            while(at<events.size()&&events[at].cycle==cycle){
                const Event &e=events[at++];
                for(unsigned rotation=0;rotation<3;++rotation){
                    const unsigned reg=mapped(e.reg,rotation);
                    if(e.kind=='w'){
                        oracles[rotation]->sid->write(reg,e.value);rc_sid_own_write(&candidates[rotation],reg,static_cast<uint8_t>(e.value));
                    }else if(!rotation){
                        const unsigned reference=oracles[0]->sid->read(reg),candidate=rc_sid_own_read(&candidates[0],reg);
                        reads.compare(cycle,reg,reference,candidate);
                        if(resumed_active)continuation.compare(cycle,reg,candidate,rc_sid_own_read(resumed.get(),reg));
                    }
                }
                if(resumed_active&&e.kind=='w')rc_sid_own_write(resumed.get(),e.reg,static_cast<uint8_t>(e.value));
            }
            for(unsigned rotation=0;rotation<3;++rotation){
                oracles[rotation]->sid->clock();rc_sid_own_tick(&candidates[rotation]);discard_audio(&candidates[rotation]);
                const auto state=oracles[rotation]->sid->read_state();
                if(!rotation)for(unsigned v=0;v<3;++v){
                    rc_sid_own_probe probe{};rc_sid_own_get_probe(&candidates[0],v,&probe);
                    phase.compare(cycle,v,state.accumulator[v],probe.phase);
                    lfsr.compare(cycle,v,state.shift_register[v],probe.lfsr);
                    envelope.compare(cycle,v,state.envelope_counter[v],probe.envelope);
                }
                const unsigned original_voice=(2+3-rotation)%3;
                osc8.compare(cycle,original_voice,static_cast<uint8_t>(state.sid_register[27]),rc_sid_own_peek(&candidates[rotation],27));
                env3.compare(cycle,original_voice,static_cast<uint8_t>(state.sid_register[28]),rc_sid_own_peek(&candidates[rotation],28));
            }
            if(resumed_active){
                rc_sid_own_tick(resumed.get());discard_audio(resumed.get());++continuation_cycles;
                continuation.compare(cycle,0,1,probes_equal(&candidates[0],resumed.get())?1:0);
            }
            if(segment_cycles){
                audio.add(oracles[0]->sid->output(),rc_sid_own_output_cycle(&candidates[0]));
                if(audio.count==segment_cycles||cycle+1==cycles){segments.push_back(audio);audio=Audio{};audio.begin=cycle+1;}
            }
        }
        continuation.compare(cycles,0,1,save(&candidates[0])==save(resumed.get())?1:0);
        FILE *file=std::fopen(out_path,"wb");if(!file)throw std::runtime_error("cannot open report");
        std::fprintf(file,"{\"schema\":1,\"completed\":true,\"model\":%u,\"cycles\":%u,\"waveform12\":{\"status\":\"unobservable\",\"checked\":0},\"digital\":{",model?8580:6581,cycles);
        const Difference *diffs[]={&phase,&lfsr,&envelope,&osc8,&env3,&reads,&continuation};
        const char *names[]={"phase24","noise23","envelope8","osc3_per_voice8","env3_per_voice8","guest_reads","save_load"};
        for(unsigned i=0;i<7;++i){if(i)std::fputc(',',file);diffs[i]->emit(file,names[i]);}
        std::fprintf(file,"},\"continuation_cycles\":%u,\"audio\":[",continuation_cycles);
        for(size_t i=0;i<segments.size();++i){if(i)std::fputc(',',file);segments[i].emit(file);}
        std::fprintf(file,"]}\n");const int closed=std::fclose(file);
        if(closed)throw std::runtime_error("cannot finish report");
        return 0;
    }catch(const std::exception &e){std::fprintf(stderr,"sid_compare: %s\n",e.what());return 2;}
}
