/* Original SID implementation from MOS documentation and published die analysis.
 * This is an experimental candidate, subject to the independent comparison gate.
 * No other SID implementation or waveform table is used here. */
#include "sid_own.h"
#include <math.h>
#include <string.h>

#define PHASE_MASK 0xffffffu
#define NOISE_MASK 0x7fffffu
#define PI 3.14159265358979323846

/* SID_schematics page8: annotated LFSR15 stop counts + reset clock.
 * Store elapsed clocks rather than the electrically equivalent LFSR state. */
static const uint16_t rate_periods[16] = {
    9,32,63,95,149,220,267,313,392,977,1954,3126,3907,11720,19532,31251
};
/* Solve the actual series/shunt ladder, from its LSB to the output MSB.
 * A 1-ohm series resistance sets the units. This is a tridiagonal nodal solve,
 * not a table of measured or emulator waveform values. */
static double ladder(unsigned code,unsigned bits,double ratio,int terminated,
                     double digital_supply,double pull_g,double pull_voltage) {
    double previous_diagonal=0,previous_rhs=0;
    for(unsigned i=0;i<bits;++i) {
        double diagonal=1.0/ratio+(i?1.0:0.0)+(i+1<bits?1.0:0.0);
        double rhs=((code>>i)&1)*digital_supply/ratio;
        if(!i && terminated)diagonal+=1.0/ratio;
        if(i+1==bits){diagonal+=pull_g;rhs+=pull_g*pull_voltage;}
        if(i){diagonal-=1.0/previous_diagonal;rhs+=previous_rhs/previous_diagonal;}
        previous_diagonal=diagonal;previous_rhs=rhs;
    }
    return previous_rhs/previous_diagonal;
}
static double dac(unsigned code,const double *weights,unsigned bits) {
    double value=0;for(unsigned b=0;b<bits;++b)if(code&(1u<<b))value+=weights[b];return value;
}
static void analog_profile(rc_sid_own *s) {
    /* Die-derived R/2R ratios include driver resistance. The NMOS waveform
     * ladder has an open LSB and a pullup of (14.844+22.598)k at its output. */
    double wr=s->model?1.986:2.02,er=s->model?1.979:2.02;
    double pull=s->model?0.0:14.844/(14.844+22.598);
    s->waveform_bias=ladder(0,12,wr,s->model!=0,5.0,pull,12.0);
    for(unsigned b=0;b<12;++b) {
        s->waveform_weight[b]=ladder(1u<<b,12,wr,s->model!=0,5.0,pull,12.0)-s->waveform_bias;
        s->cutoff_weight[b]=ladder(1u<<b,12,1.879,0,12.0,0,0);
    }
    for(unsigned b=0;b<8;++b)s->envelope_weight[b]=ladder(1u<<b,8,er,1,1.0,0,0);
}
static uint32_t bus_lifetime(const rc_sid_own *s) {
    /* Nominal retention only: charge leakage varies with die and temperature. */
    return s->model==RC_SID_OWN_8580 ? 663552u : 7424u;
}
static void envelope_rate(rc_sid_own_voice *v) {
    unsigned rate=v->envelope_state==RC_SID_OWN_ATTACK ? v->attack_decay>>4 :
        v->envelope_state==RC_SID_OWN_DECAY ? v->attack_decay&15 : v->sustain_release&15;
    v->rate_period=rate_periods[rate];
}
static void envelope_threshold(rc_sid_own_voice *v) {
    /* These are equality detectors, not ranges: the latch retains its value
     * while attacking through any of the other 249 envelope values. */
    switch(v->envelope) {
    case 255: v->exponential_period=1; break;
    case 93: v->exponential_period=2; break;
    case 54: v->exponential_period=4; break;
    case 26: v->exponential_period=8; break;
    case 14: v->exponential_period=16; break;
    case 6: v->exponential_period=30; break;
    case 0: v->exponential_period=1; v->hold_zero=1; break;
    default: break;
    }
}
static void envelope_step(rc_sid_own_voice *v) {
    if(v->hold_zero)return;
    if(v->envelope_state==RC_SID_OWN_ATTACK) {
        ++v->envelope;
        if(v->envelope==255) {
            v->pending_state=RC_SID_OWN_DECAY;
            v->state_pipeline=2;
        }
    } else if(v->envelope_state==RC_SID_OWN_RELEASE ||
              v->envelope!=(uint8_t)((v->sustain_release>>4)*17)) --v->envelope;
    envelope_threshold(v);
}
static void envelope_clock(rc_sid_own_voice *v) {
    v->envelope_read=v->envelope;
    /* The two direction latches propagate an edge already in flight even
     * when the gate changes again on the following cycle. */
    if(v->previous_state){v->envelope_state=(uint8_t)(v->previous_state-1u);v->previous_state=0;envelope_rate(v);}
    if(v->state_pipeline && !--v->state_pipeline) {
        v->envelope_state=v->pending_state;
        envelope_rate(v);
        if(v->envelope_state==RC_SID_OWN_ATTACK)v->hold_zero=0;
    }
    if(v->envelope_pipeline && !--v->envelope_pipeline)envelope_step(v);
    if(v->exponential_pipeline && !--v->exponential_pipeline) {
        if(++v->exponential_counter==v->exponential_period) {
            v->exponential_counter=0;
            /* FPF (divide-by-one) bypasses the LFSR5 match/reset latch.
             * Slowed decay uses that extra latch before release_reset. */
            v->envelope_pipeline=v->exponential_period>1?2:1;
        }
    }
    if(v->rate_reset) {
        v->rate_reset=0;
        v->rate_counter=0;
        if(v->envelope_state==RC_SID_OWN_ATTACK) {
            v->exponential_counter=0;
            /* LFSR15 reset is latched before release_reset reaches the DAC
             * counter. The two sid_clk1 latches are visible on page8. */
            v->envelope_pipeline=2;
        } else v->exponential_pipeline=1;
    }
    /* The physical LFSR has 32767 states, not 32768. Changing the rate never
     * resets it: a missed equality must wait for a complete circuit (ADSR bug). */
    if(++v->rate_counter==0x8000)v->rate_counter=1;
    if(v->rate_counter==v->rate_period)v->rate_reset=1;
}
static uint16_t noise_output(uint32_t n) {
    /* Physical output taps in the published die's Wave Selector / Noise
     * Generator. These precede the two shift latches; the older REU analysis
     * numbers its equivalent sequence two bits further along the register. */
    return (uint16_t)(((n>>9)&0x800)|((n>>8)&0x400)|((n>>5)&0x200)|
        ((n>>3)&0x100)|((n>>2)&0x80)|((n<<1)&0x40)|((n<<3)&0x20)|((n<<4)&0x10));
}
static void noise_shift(rc_sid_own_voice *v,int test) {
    uint32_t input=((v->lfsr>>22)|(uint32_t)test)^(v->lfsr>>17);
    v->lfsr=((v->lfsr<<1)|(input&1))&NOISE_MASK;
}
static uint16_t triangle_saw(const rc_sid_own_voice *v,const rc_sid_own_voice *source) {
    uint32_t phase=v->phase;
    if(v->control&0x20)return (uint16_t)(phase>>12);
    /* The ring source is inverted before the triangle XOR on the die. */
    if(v->control&4)phase^=(~source->phase)&0x800000u;
    if(phase&0x800000u)phase^=PHASE_MASK;
    return (uint16_t)((phase>>11)&0xffe);
}
/* Shared selector switches couple waveform bits before the DAC buffers.
 * Conductance and capacitance below are explicit nominal assumptions, not a
 * fit: no chip capture of selector thresholds/parasitics is available. */
static uint16_t combined_waveform(rc_sid_own *s,rc_sid_own_voice *v,uint16_t ts) {
    double diag[12],rhs[12],upper[12];unsigned wave=v->control>>4;
    uint16_t noise=noise_output(v->lfsr),result=0;
    /* Triangle and saw connect adjacent XOR driver lines. Other combinations
     * retain a weaker adjacent capacitance; charge persists between cycles. */
    double coupling=(wave&3)==3?1.0:0.08;
    double history=0.25;
    for(unsigned b=0;b<12;++b) {
        double conductance=history,value=history*v->bitline[b];
        uint16_t words[4]={(uint16_t)((wave&2)?(ts<<1):ts),ts,v->pulse_latch,noise};
        for(unsigned source=0;source<4;++source)if(wave&(1u<<source)) {
            int high=(words[source]&(1u<<b))!=0;
            /* NMOS low drivers discharge more strongly than their pullups.
             * The narrower upper triangle selectors on 8580 resist more. */
            double selector=(s->model && source==0 && b>=4)?0.8:1.0;
            double g=selector*(high?1.0:2.0);
            conductance+=g;value+=high?g:0.0;
        }
        diag[b]=conductance+(b?coupling:0.0)+(b<11?coupling:0.0);
        rhs[b]=value;upper[b]=b<11?-coupling:0;
        if(b){double m=-coupling/diag[b-1];diag[b]-=m*upper[b-1];rhs[b]-=m*rhs[b-1];}
    }
    for(int b=11;b>=0;--b) {
        double value=(rhs[b]-(b<11?upper[b]*v->bitline[b+1]:0))/diag[b];
        v->bitline[b]=value;
        if(value>0.5)result|=(uint16_t)(1u<<b);
    }
    /* The NMOS saw MSB wire reaches the accumulator directly. HMOS has a
     * separate latch/buffer, preventing selector loading from resetting it. */
    if(!s->model && (wave&3)==3 && !(result&0x800))v->phase&=0x7fffff;
    return result;
}
static void waveform_clock(rc_sid_own *s,unsigned i) {
    rc_sid_own_voice *v=&s->voice[i];
    const rc_sid_own_voice *source=&s->voice[(i+2)%3];
    uint16_t ts=triangle_saw(v,source),value=0xfff;
    unsigned wave=v->control>>4;
    if(!wave) {
        /* Floating selector/DAC nodes retain the last code before leakage. */
        if(v->floating_cycles && !--v->floating_cycles)v->waveform=0;
    } else {
        uint16_t selected_ts=s->model==RC_SID_OWN_8580?v->tri_saw_latch:ts;
        if(wave&(wave-1))value=combined_waveform(s,v,selected_ts);
        else {
            if(wave&3)value=selected_ts;
            if(wave&4)value=v->pulse_latch;
            if(wave&8)value=noise_output(v->lfsr);
            for(unsigned b=0;b<12;++b)v->bitline[b]=(value>>b)&1;
        }
        v->waveform=value;
        if((wave&8) && (wave&7)) {
            /* The selected noise driver is bidirectional: discharged selector
             * nodes feed back to their physical LFSR storage bits. */
            static const unsigned taps[8]={0,2,5,9,11,14,18,20};
            for(unsigned b=0;b<8;++b)
                if(!(value&(1u<<(b+4))))v->lfsr&=~(1u<<taps[b]);
        }
    }
    v->tri_saw_latch=ts;
    v->pulse_latch=(v->control&8)||(v->phase>>12)>=v->pulse_width?0xfff:0;
}
static void filter_setup(rc_sid_own *s) {
    unsigned fc=((unsigned)s->registers[22]<<3)|(s->registers[21]&7);
    unsigned resonance=s->registers[23]>>4;
    /* Nini/ttlworks 8580 schematic: two switched resistor networks around the
     * bandpass feedback amplifier. Values are die-derived kilohms. */
    static const double input_resistor[4]={58.975,45.278,47.67,53.97};
    static const double feedback_resistor[4]={37.055,51.113,72.38,103.18};
    /* 11-bit conductance DAC, with a half-LSB path selected at code zero.
     * 12 kHz nominal full scale is the MOS specification, not an oracle fit. */
    double cutoff=12000.0*(fc?fc:0.5)/2047.0;
    s->filter_coefficient=2.0*sin(PI*cutoff/s->clock_hz);
    s->filter_damping=input_resistor[resonance&3]/feedback_resistor[resonance>>2];
    if(s->model==RC_SID_OWN_6581) {
        /* The real 12-bit voltage DAC has its MSB tied high. */
        s->cutoff_voltage=dac(fc|0x800,s->cutoff_weight,12);
        static const double resistance[4]={273.79,138.08,65.886,28.85};
        double conductance=0;
        for(unsigned b=0;b<4;++b)if(!(resonance&(1u<<b)))conductance+=1.0/resistance[b];
        s->filter_damping=38.667*conductance;
    }
}
static void write_register(rc_sid_own *s,unsigned reg,uint8_t value) {
    if(reg>24)return;
    s->registers[reg]=value;
    if(reg<21) {
        rc_sid_own_voice *v=&s->voice[reg/7];
        switch(reg%7) {
        case 0:v->frequency=(uint16_t)((v->frequency&0xff00)|value);break;
        case 1:v->frequency=(uint16_t)((v->frequency&0xff)|((unsigned)value<<8));break;
        case 2:v->pulse_width=(uint16_t)((v->pulse_width&0xf00)|value);break;
        case 3:v->pulse_width=(uint16_t)((v->pulse_width&0xff)|((value&15)<<8));break;
        case 4: {
            uint8_t previous=v->control;
            v->control=value;
            if((previous^value)&1) {
                if(v->state_pipeline==1)v->previous_state=(uint8_t)(v->pending_state+1u);
                v->pending_state=(value&1)?RC_SID_OWN_ATTACK:RC_SID_OWN_RELEASE;
                v->state_pipeline=2;
            }
            if((value&8) && !(previous&8)) {
                v->phase=0;v->noise_pipeline=0;v->test_cycles=0;
                v->pulse_latch=0xfff;
            } else if(!(value&8) && (previous&8))noise_shift(v,1);
            if(!(value&0xf0) && (previous&0xf0))
                v->floating_cycles=s->clock_hz/5;
            break;
        }
        case 5:v->attack_decay=value;envelope_rate(v);break;
        case 6:v->sustain_release=value;envelope_rate(v);break;
        default:break;
        }
    } else if(reg<24)filter_setup(s);
}
/* First-order long-channel NMOS channel current, integrated between its two
 * endpoints. Clamping each overdrive at zero includes pinch-off/saturation.
 * MIT 6.012 lecture 9; process beta and threshold are explicit assumptions. */
static double mos_current(double gate,double source,double drain) {
    const double beta=20e-6,threshold=1.0;
    double a=gate-source-threshold,b=gate-drain-threshold;
    if(a<0)a=0;if(b<0)b=0;
    return 0.5*beta*(a*a-b*b);
}
static double bound_voltage(double v) {return v>6.0?6.0:v< -4.0?-4.0:v;}
static void analog_6581(rc_sid_own *s) {
    unsigned route=s->registers[23],mode=s->registers[24];
    double input=0,bypass=0,filtered=0;
    for(unsigned i=0;i<3;++i) {
        const rc_sid_own_voice *v=&s->voice[i];
        double wave=s->waveform_bias+dac(v->waveform,s->waveform_weight,12);
        double env=dac(v->envelope,s->envelope_weight,8);
        /* Envelope off connects its ladder to +5V. The +1V displacement
         * through the two-FET buffer is an explicit unmeasured assumption. */
        double voice=(wave-5.0)*env+1.0;
        if(route&(1u<<i))input+=voice;
        else if(i!=2 || !(mode&128))bypass+=voice;
    }
    /* Four substeps limit integration error at the largest conductance. The
     * 6581 uses self-centering inverters, represented by finite supply bounds.
     * Physical capacitor/leakage values come from the die and C64 schematic. */
    const double dt=1.0/(4.0*s->clock_hz),capacitance=470e-12,center=5.0;
    for(unsigned step=0;step<4;++step) {
        double hp=s->filter_damping*s->filter_band-s->filter_low-input;
        s->filter_high=bound_voltage(hp);
        double source_hp=center+s->filter_high,source_bp=center+s->filter_band;
        double gate_bp=(source_hp/35.625+s->cutoff_voltage/43.571)/(1.0/35.625+1.0/43.571);
        double gate_lp=(source_bp/40.174+s->cutoff_voltage/42.143)/(1.0/40.174+1.0/42.143);
        double bp_current=mos_current(gate_bp,source_hp,center)-s->filter_high/1035010.0;
        double lp_current=mos_current(gate_lp,source_bp,center)-s->filter_band/1011520.0;
        s->filter_band=bound_voltage(s->filter_band+dt*bp_current/capacitance);
        s->filter_low=bound_voltage(s->filter_low+dt*lp_current/capacitance);
    }
    if(mode&16)filtered+=s->filter_low;
    if(mode&32)filtered+=s->filter_band;
    if(mode&64)filtered+=s->filter_high;
    /* Mixer and four switched volume conductances, in kOhm. Finite transistor
     * output resistance means the 16 gains are not an ideal integer ramp. */
    static const double volume_resistor[4]={790.109,404.67,189.23,100.83};
    double volume_gain=0;
    for(unsigned b=0;b<4;++b)if(mode&(1u<<b))volume_gain+=74.592/volume_resistor[b];
    double mixed=bound_voltage(bypass*(330.05/233.67)+filtered*(330.05/251.54));
    /* A fixed 10 V reference corresponds to PCM magnitude 32768.
     * DC changes with the volume ladder and survives when AC coupling is off. */
    s->cycle_output=bound_voltage(mixed*volume_gain)*3276.8;
}
static void analog_clock(rc_sid_own *s) {
    double input=0,bypass=0,filtered=0;
    unsigned route=s->registers[23],mode=s->registers[24];
    for(unsigned i=0;i<3;++i) {
        const rc_sid_own_voice *v=&s->voice[i];
        double voice=((double)v->waveform-2048.0)*v->envelope;
        if(route&(1u<<i))input+=voice;
        else if(i!=2 || !(mode&128))bypass+=voice;
    }
    /* Inverting HP summer and two inverting integrators. The BP feedback
     * amplifier adds one more inversion, making its summer term positive. */
    if(s->model==RC_SID_OWN_8580) {
        s->filter_low-=s->filter_coefficient*s->filter_band;
        s->filter_band-=s->filter_coefficient*s->filter_high;
        s->filter_high=s->filter_damping*s->filter_band-s->filter_low-input;
    }
    if(mode&16)filtered+=s->filter_low;
    if(mode&32)filtered+=s->filter_band;
    if(mode&64)filtered+=s->filter_high;
    double mixed=(bypass+filtered)*(mode&15)/(15.0*64.0);
    if(s->model==RC_SID_OWN_6581){analog_6581(s);mixed=s->cycle_output;}
    /* Board 250469: 10 kohm / 1000 pF LP and 10 uF / 1 kohm AC coupling.
     * Backward Euler is stable at every supported machine/sample frequency. */
    double lp=1.0/(1.0+0.00001*s->clock_hz);
    double hp=1.0/(1.0+0.01*s->clock_hz);
    s->external_low+=lp*(mixed-s->external_low);
    s->external_high+=hp*(s->external_low-s->external_high);
    s->cycle_output=s->external_filter?s->external_low-s->external_high:mixed;
}
int rc_sid_own_init(rc_sid_own *s,unsigned model,uint32_t clock_hz,uint32_t sample_rate) {
    if(!s || model>1 || clock_hz<100000 || clock_hz>2000000 ||
        sample_rate<8000 || sample_rate>192000 || sample_rate>=clock_hz)return 0;
    memset(s,0,sizeof *s);
    s->model=(uint8_t)model;s->clock_hz=clock_hz;s->sample_rate=sample_rate;
    /* Reproducible cold-start convention; the silicon power-on state of the
     * oscillator/envelope storage nodes is not specified by the data sheet. */
    s->pot[0]=s->pot[1]=255;s->external_filter=1;
    for(unsigned i=0;i<3;++i) {
        rc_sid_own_voice *v=&s->voice[i];
        v->lfsr=0x7ffff8;v->pulse_latch=0xfff;
        v->envelope_state=v->pending_state=RC_SID_OWN_RELEASE;
        v->exponential_period=1;v->hold_zero=1;envelope_rate(v);
    }
    analog_profile(s);filter_setup(s);
    return 1;
}
void rc_sid_own_reset(rc_sid_own *s) {
    /* RESET is not power-on: the die has no RESET net to the oscillator
     * accumulator or envelope DAC counter. Analog capacitors also retain charge.
     * The API applies the reset-pin effect without advancing guest time. */
    memset(s->registers,0,sizeof s->registers);
    s->bus_value=0;s->bus_cycles=0;s->write_pending=0;s->write_reg=0;s->write_value=0;
    for(unsigned i=0;i<3;++i) {
        rc_sid_own_voice *v=&s->voice[i];
        v->frequency=0;v->pulse_width=0;v->control=0;
        v->attack_decay=0;v->sustain_release=0;
        v->envelope_state=v->pending_state=RC_SID_OWN_RELEASE;
        v->rate_counter=0;v->exponential_counter=0;v->exponential_period=1;
        v->envelope_pipeline=0;v->exponential_pipeline=0;v->state_pipeline=0;v->previous_state=0;
        v->rate_reset=0;v->hold_zero=(uint8_t)(v->envelope==0);
        v->noise_pipeline=0;v->test_cycles=0;v->msb_rising=0;
        v->pulse_latch=0xfff;v->floating_cycles=s->clock_hz/5;
        /* Reset, like TEST, opens noise shift phase 1. Releasing the reset
         * pin closes phase 2 with the forced-high XOR feedback input. */
        noise_shift(v,1);
        envelope_rate(v);
    }
    filter_setup(s);
}
void rc_sid_own_write(rc_sid_own *s,unsigned reg,uint8_t value) {
    reg&=31;s->bus_value=value;s->bus_cycles=bus_lifetime(s);
    if(s->model==RC_SID_OWN_8580) {
        /* CPU writes occur once per cycle. Flush a previous unclocked write to
         * permit deterministic bulk register initialization by API clients. */
        if(s->write_pending)write_register(s,s->write_reg,s->write_value);
        s->write_pending=1;s->write_reg=(uint8_t)reg;s->write_value=value;
    } else write_register(s,reg,value);
}
uint8_t rc_sid_own_peek(const rc_sid_own *s,unsigned reg) {
    switch(reg&31) {
    case 25:return s->pot[0];
    case 26:return s->pot[1];
    case 27:return (uint8_t)(s->voice[2].waveform>>4);
    case 28:return s->voice[2].envelope_read;
    default:return s->bus_value;
    }
}
uint8_t rc_sid_own_read(rc_sid_own *s,unsigned reg) {
    uint8_t value=rc_sid_own_peek(s,reg);
    if((reg&31)>=25 && (reg&31)<=28) {
        s->bus_value=value;s->bus_cycles=bus_lifetime(s);
    }
    return value;
}
uint8_t rc_sid_own_pot_read(rc_sid_own *s,unsigned reg,uint8_t value) {
    s->pot[(reg-25)&1]=value;s->bus_value=value;s->bus_cycles=bus_lifetime(s);
    return value;
}
void rc_sid_own_tick(rc_sid_own *s) {
    ++s->cycles;
    if(s->bus_cycles && !--s->bus_cycles)s->bus_value=0;
    for(unsigned i=0;i<3;++i) {
        rc_sid_own_voice *v=&s->voice[i];uint32_t previous=v->phase;
        envelope_clock(v);v->msb_rising=0;
        if(v->control&8) {
            v->phase=0;
            /* Die analysis: open storage gates drift high; measurements give
             * 8-32 ms on NMOS. Use the conservative end until chip sampling. */
            if(v->test_cycles<s->clock_hz/30 && ++v->test_cycles==s->clock_hz/30)
                v->lfsr=NOISE_MASK;
        } else {
            if(v->noise_pipeline && !--v->noise_pipeline)noise_shift(v,0);
            v->phase=(v->phase+v->frequency)&PHASE_MASK;
            v->msb_rising=(uint8_t)(!(previous&0x800000) && (v->phase&0x800000));
            if(!(previous&0x80000) && (v->phase&0x80000))v->noise_pipeline=2;
        }
    }
    /* Simultaneous sync: a source reset on this cycle cannot reset its target. */
    for(unsigned i=0;i<3;++i) {
        rc_sid_own_voice *v=&s->voice[i];
        const rc_sid_own_voice *source=&s->voice[(i+2)%3];
        const rc_sid_own_voice *upstream=&s->voice[(i+1)%3];
        if((v->control&2) && source->msb_rising &&
            !((source->control&2) && upstream->msb_rising))v->phase=0;
    }
    for(unsigned i=0;i<3;++i)waveform_clock(s,i);
    analog_clock(s);
    /* Exact area of each chip-clock interval over the host sample interval.
     * Rational phase avoids PAL/NTSC drift and splits the boundary interval. */
    uint32_t available=s->sample_rate;
    while(available) {
        uint32_t part=s->clock_hz-s->sample_phase;
        if(part>available)part=available;
        s->sample_sum+=s->cycle_output*part;s->sample_phase+=part;available-=part;
        if(s->sample_phase==s->clock_hz) {
            double sample=s->sample_sum/s->clock_hz;
            if(sample>32767)sample=32767;
            if(sample< -32768)sample= -32768;
            /* Unconsumed host audio must not stop the guest clock. Keep the
             * newest samples, as the reSID bridge does for its output queue. */
            if(s->audio_count==RC_SID_OWN_AUDIO_CAPACITY){s->audio_head=(s->audio_head+1u)%RC_SID_OWN_AUDIO_CAPACITY;--s->audio_count;}
            unsigned tail=(s->audio_head+s->audio_count)%RC_SID_OWN_AUDIO_CAPACITY;
            s->audio[tail]=(int16_t)sample;++s->audio_count;
            s->sample_phase=0;s->sample_sum=0;
        }
    }
    if(s->write_pending) {
        s->write_pending=0;write_register(s,s->write_reg,s->write_value);
    }
}
int rc_sid_own_clock(rc_sid_own *s,uint32_t cycles) {
    while(cycles--)rc_sid_own_tick(s);
    return !s->failed;
}
size_t rc_sid_own_audio_pull(rc_sid_own *s,int16_t *mono,size_t frames) {
    if(!mono)return 0;
    if(frames>s->audio_count)frames=s->audio_count;
    for(size_t i=0;i<frames;++i)mono[i]=s->audio[(s->audio_head+(unsigned)i)%RC_SID_OWN_AUDIO_CAPACITY];
    s->audio_head=(s->audio_head+(unsigned)frames)%RC_SID_OWN_AUDIO_CAPACITY;
    s->audio_count-=(unsigned)frames;return frames;
}
void rc_sid_own_get_probe(const rc_sid_own *s,unsigned voice,rc_sid_own_probe *out) {
    if(voice>=3 || !out)return;
    const rc_sid_own_voice *v=&s->voice[voice];
    out->phase=v->phase;out->lfsr=v->lfsr;out->waveform=v->waveform;out->envelope=v->envelope;
}
void rc_sid_own_set_external_filter(rc_sid_own *s,int enabled) {s->external_filter=(uint8_t)(enabled!=0);}
int32_t rc_sid_own_output_cycle(const rc_sid_own *s) {
    double value=s->cycle_output;
    if(value>2147483647.0)return INT32_MAX;
    if(value< -2147483648.0)return INT32_MIN;
    return (int32_t)value;
}
/* Explicit LE fields; doubles preserve their IEEE-754 bits, never struct padding. */
static void write_double(rc_state_writer *w,double value) {
    uint64_t bits;memcpy(&bits,&value,sizeof bits);rc_sw_u64(w,bits);
}
static double read_double(rc_state_reader *r) {
    uint64_t bits=rc_sr_u64(r);double value;memcpy(&value,&bits,sizeof value);return value;
}
#define VOICE_U32(X) X(phase) X(lfsr) X(test_cycles) X(floating_cycles)
#define VOICE_U16(X) X(frequency) X(pulse_width) X(waveform) X(pulse_latch) X(tri_saw_latch) X(rate_counter) X(rate_period)
#define VOICE_U8(X) X(control) X(attack_decay) X(sustain_release) X(envelope) X(envelope_read) X(envelope_state) X(exponential_counter) X(exponential_period) X(hold_zero) X(noise_pipeline) X(envelope_pipeline) X(exponential_pipeline) X(state_pipeline) X(pending_state) X(rate_reset) X(msb_rising) X(previous_state)
#define CHIP_U32(X) X(clock_hz) X(sample_rate) X(bus_cycles) X(sample_phase) X(sample_cycles) X(audio_head) X(audio_count)
#define CHIP_U8(X) X(model) X(bus_value) X(write_pending) X(write_reg) X(write_value) X(failed) X(external_filter)
#define CHIP_DOUBLE(X) X(filter_low) X(filter_band) X(filter_high) X(external_low) X(external_high) X(filter_coefficient) X(filter_damping) X(sample_sum) X(cycle_output) X(waveform_bias) X(cutoff_voltage)
void rc_sid_own_save(const rc_sid_own *s,rc_state_writer *w) {
    rc_sw_u32(w,3);
    for(unsigned i=0;i<3;++i) {
        const rc_sid_own_voice *v=&s->voice[i];
#define W32(f) rc_sw_u32(w,v->f);
#define W16(f) rc_sw_u16(w,v->f);
#define W8(f) rc_sw_u8(w,v->f);
        VOICE_U32(W32) VOICE_U16(W16) VOICE_U8(W8)
        for(unsigned b=0;b<12;++b)write_double(w,v->bitline[b]);
#undef W32
#undef W16
#undef W8
    }
#define W32(f) rc_sw_u32(w,s->f);
#define W8(f) rc_sw_u8(w,s->f);
#define WD(f) write_double(w,s->f);
    CHIP_U32(W32) CHIP_U8(W8) CHIP_DOUBLE(WD)
#undef W32
#undef W8
#undef WD
    rc_sw_u64(w,s->cycles);rc_sw_bytes(w,s->registers,32);rc_sw_bytes(w,s->pot,2);
    for(unsigned b=0;b<12;++b){write_double(w,s->waveform_weight[b]);write_double(w,s->cutoff_weight[b]);}
    for(unsigned b=0;b<8;++b)write_double(w,s->envelope_weight[b]);
    for(unsigned i=0;i<RC_SID_OWN_AUDIO_CAPACITY;++i)rc_sw_u16(w,(uint16_t)s->audio[i]);
}
int rc_sid_own_load(rc_sid_own *s,rc_state_reader *r) {
    rc_sid_own next;memset(&next,0,sizeof next);
    if(rc_sr_u32(r)!=3)return 0;
    for(unsigned i=0;i<3;++i) {
        rc_sid_own_voice *v=&next.voice[i];
#define R32(f) v->f=rc_sr_u32(r);
#define R16(f) v->f=rc_sr_u16(r);
#define R8(f) v->f=rc_sr_u8(r);
        VOICE_U32(R32) VOICE_U16(R16) VOICE_U8(R8)
        for(unsigned b=0;b<12;++b) {
            v->bitline[b]=read_double(r);
            if(!isfinite(v->bitline[b]) || v->bitline[b]<0 || v->bitline[b]>1)return 0;
        }
#undef R32
#undef R16
#undef R8
        if(v->phase>PHASE_MASK || v->lfsr>NOISE_MASK || v->pulse_width>0xfff ||
           v->waveform>0xfff || v->pulse_latch>0xfff || v->tri_saw_latch>0xfff ||
           v->envelope_state>RC_SID_OWN_RELEASE || v->pending_state>RC_SID_OWN_RELEASE ||
           v->previous_state>3 || v->rate_counter>32767 || !v->rate_period || v->rate_period>31251 ||
           !v->exponential_period || v->exponential_period>30 ||
           v->noise_pipeline>2 || v->state_pipeline>2 || v->envelope_pipeline>2 ||
           v->exponential_pipeline>2 || v->hold_zero>1 || v->rate_reset>1)return 0;
    }
#define R32(f) next.f=rc_sr_u32(r);
#define R8(f) next.f=rc_sr_u8(r);
#define RD(f) next.f=read_double(r); if(!isfinite(next.f))return 0;
    CHIP_U32(R32) CHIP_U8(R8) CHIP_DOUBLE(RD)
#undef R32
#undef R8
#undef RD
    next.cycles=rc_sr_u64(r);rc_sr_bytes(r,next.registers,32);rc_sr_bytes(r,next.pot,2);
    for(unsigned b=0;b<12;++b) {
        next.waveform_weight[b]=read_double(r);next.cutoff_weight[b]=read_double(r);
        if(!isfinite(next.waveform_weight[b]) || !isfinite(next.cutoff_weight[b]) ||
           next.waveform_weight[b]<0 || next.waveform_weight[b]>5 ||
           next.cutoff_weight[b]<0 || next.cutoff_weight[b]>12)return 0;
    }
    for(unsigned b=0;b<8;++b) {
        next.envelope_weight[b]=read_double(r);
        if(!isfinite(next.envelope_weight[b]) || next.envelope_weight[b]<0 || next.envelope_weight[b]>1)return 0;
    }
    for(unsigned i=0;i<RC_SID_OWN_AUDIO_CAPACITY;++i)next.audio[i]=(int16_t)rc_sr_u16(r);
    if(!rc_sr_ok(r) || next.model!=s->model || next.clock_hz!=s->clock_hz ||
       next.sample_rate!=s->sample_rate || next.sample_phase>=next.clock_hz ||
       next.audio_head>=RC_SID_OWN_AUDIO_CAPACITY || next.audio_count>RC_SID_OWN_AUDIO_CAPACITY ||
       next.write_pending>1 || next.write_reg>31 || next.failed>1 || next.external_filter>1 ||
       next.filter_coefficient<0 || next.filter_coefficient>1 ||
       next.filter_damping<0 || next.filter_damping>4)return 0;
    *s=next;return 1;
}
