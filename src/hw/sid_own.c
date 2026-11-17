/* Original SID implementation from MOS documentation and published die analysis.
 * Circuit constants that the documents leave open (levels, cutoff curve, amplifier
 * gains) are calibrated by measurement against the default engine run as a
 * program; no code or table of another SID implementation is used here. */
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

/* One chip revision. Voltages are expressed in output PCM units at volume 15. */
typedef struct chip_profile {
    double ladder_ratio;      /* DAC 2R/R, including driver resistance */
    double wave_zero;         /* normalised waveform DAC level of a silent voice */
    double envelope_floor;    /* envelope DAC output at code zero */
    double voice_gain;        /* full waveform swing at full envelope */
    double voice_dc;          /* offset of each voice output from the amplifier trip point */
    /* Filter summer gains: input, low-pass feedback and the resonance
     * network (a base stepped in octaves, plus a linear ladder). */
    double summer_input, summer_low, summer_band, summer_band_octave, summer_band_step;
    double loss_band, loss_low; /* reciprocal DC gain of the two integrators */
    double mixer_gain;        /* inverter gain of the mixer amplifier, 0 = ideal */
    double volume_knee;       /* finite output amplifier gain, 0 = ideal */
    double output_offset;
    /* Cutoff: floor, slope, threshold, knee and ceiling of the FET curve
     * (6581), or the two terms of the current DAC (8580). */
    double cutoff_base, cutoff_slope, cutoff_threshold, cutoff_knee, cutoff_ceiling;
    double cutoff_feedback;   /* gate drive lost per unit of integrator output (the FET source follows it) */
    double output_knee, output_reach; /* where the output stage starts to bottom out, and how far it goes */
    double host_gain;         /* playback level of the host samples, matching the default engine */
    int ladder_terminated;    /* NMOS ladders lack the LSB termination resistor */
    uint32_t bus_cycles, floating_cycles, fade_start, fade_step;
} chip_profile;
static const chip_profile profiles[2] = {
    /* 6581: NMOS inverters as amplifiers. Their low gain makes every summing
     * stage depend on the number of connected inputs, and the integrators lossy. */
    { 2.20, 0.2075,0.0085, 8870.0,2611.0, 0.438604,0.46667,0.0,0.0,0.0416444, 0.335711,0.146115,
      4.8,50.0,-8469.0, 289.866,21638.7,0.333588,0.0442867,59687.1,6.8989e-6,-21600.0,5900.0,1.5,
      0, 7424u,182000u,35000u,1000u },
    /* 8580: HMOS-II, terminated ladders, near-ideal amplifiers. */
    { 2.00, 2523.5/4095.0,1.0/256.0, 7976.0,0.0, 1.0,1.0,1.415833,0.125,0.0, 0.0,0.0,
      0.0,0.0,0.0, 0.0,6.031136,0.0,0.0,-1.1e-5,0.0,-1e9,1.0,2.5,
      1, 663552u,4500000u,2520000u,315000u }
};
static const chip_profile *profile(const rc_sid_own *s) {return &profiles[s->model];}
/* Selecting several waveforms shorts their drivers on the twelve selector
 * lines. A line reads high when the drive it collects, its own and its
 * neighbours' weakening with distance, stays above the buffer threshold. */
typedef struct selector_profile {
    double conflict;     /* level of a line whose triangle and saw drivers disagree */
    double reach_up, reach_down; /* decay, in lines, of the pull from lower and higher lines */
    double threshold;
    double pulse;        /* drive the high pulse line adds to every line */
} selector_profile;
static const selector_profile selectors[2][4] = {
    { {0.0174,1.7069,1.1449,0.8362,0.0}, {0.0,4.4,7.2583,0.7942,2.9197},
      {0.0,7.8368,0.4248,0.9833,0.2573}, {0.2577,4.071,3.4885,0.887,2.5791} },
    { {0.6078,1.1677,4.8288,0.9075,0.0}, {0.0,1.5712,4.2529,0.8464,2.9772},
      {0.0,1.9775,4.6144,0.8411,2.9953}, {0.406,1.6801,6.4889,0.8691,1.1436} }
};
static uint16_t selector_solve(const selector_profile *q,unsigned saw,unsigned triangle,
                               int use_saw,int use_triangle,int pulse) {
    double drive[12];uint16_t result=0;
    for(unsigned j=0;j<12;++j) {
        unsigned a=(saw>>j)&1u,b=(triangle>>j)&1u;
        drive[j]=use_saw && use_triangle ? (a&b ? 1.0 : a!=b ? q->conflict : 0.0) : use_saw ? a : b;
    }
    for(unsigned i=0;i<12;++i) {
        double sum=pulse?q->pulse:0.0,weight=sum;
        for(unsigned j=0;j<12;++j) {
            double k=j<i?exp(-(double)(i-j)/q->reach_up):j>i?exp(-(double)(j-i)/q->reach_down):1.0;
            sum+=k*drive[j];weight+=k;
        }
        if(sum>q->threshold*weight)result|=(uint16_t)(1u<<i);
    }
    return result;
}

/* Solve the actual series/shunt ladder, from its LSB to the output MSB.
 * A 1-ohm series resistance sets the units. This is a tridiagonal nodal solve,
 * not a table of measured or emulator waveform values. */
static double ladder(unsigned code,unsigned bits,double ratio,int terminated) {
    double previous_diagonal=0,previous_rhs=0;
    for(unsigned i=0;i<bits;++i) {
        double diagonal=1.0/ratio+(i?1.0:0.0)+(i+1<bits?1.0:0.0);
        double rhs=((code>>i)&1)/ratio;
        if(!i && terminated)diagonal+=1.0/ratio;
        if(i){diagonal-=1.0/previous_diagonal;rhs+=previous_rhs/previous_diagonal;}
        previous_diagonal=diagonal;previous_rhs=rhs;
    }
    return previous_rhs/previous_diagonal;
}
static void ladder_weights(double *weight,unsigned bits,double ratio,int terminated) {
    double full=ladder((1u<<bits)-1u,bits,ratio,terminated);
    for(unsigned b=0;b<bits;++b)weight[b]=ladder(1u<<b,bits,ratio,terminated)/full;
}
static double bessel_i0(double x) {
    double sum=1,term=1;
    for(unsigned k=1;k<40;++k){term*=(x/(2.0*k))*(x/(2.0*k));sum+=term;}
    return sum;
}
static double kernel_at(const rc_sid_own *s,double position) {
    if(position>=RC_SID_OWN_KERNEL)return 0;
    unsigned i=(unsigned)position;double f=position-i;
    return s->kernel[i]+f*(s->kernel[i+1]-s->kernel[i]);
}
/* Everything that follows from the model and the two rates alone. */
static void derived_setup(rc_sid_own *s) {
    const chip_profile *p=profile(s);double weight[12];
    ladder_weights(weight,12,p->ladder_ratio,p->ladder_terminated);
    for(unsigned code=0;code<4096;++code) {
        double level=0;for(unsigned b=0;b<12;++b)if(code&(1u<<b))level+=weight[b];
        s->waveform_level[code]=(float)(level-p->wave_zero);
    }
    ladder_weights(weight,8,p->ladder_ratio,p->ladder_terminated);
    for(unsigned code=0;code<256;++code) {
        double level=0;for(unsigned b=0;b<8;++b)if(code&(1u<<b))level+=weight[b];
        s->envelope_level[code]=(float)(s->model?(code+1u)/256.0:
            p->envelope_floor+(1.0-p->envelope_floor)*level);
    }
    ladder_weights(s->cutoff_weight,11,profiles[0].ladder_ratio,0);
    /* The cutoff DAC drives the gate of a triode-region FET in parallel with
     * a fixed resistor: a floor, a soft threshold, then a slope that follows
     * the ladder's own steps. Tabulated over gate drive from -1 to 2. */
    for(unsigned i=0;i<=RC_SID_OWN_CURVE+1;++i) {
        double gate=-1.0+3.0*i/RC_SID_OWN_CURVE;
        double drive=(gate-p->cutoff_threshold)/(p->cutoff_knee>0?p->cutoff_knee:1.0);
        drive=drive>30.0?drive:log1p(exp(drive));
        double frequency=p->cutoff_base+p->cutoff_slope*p->cutoff_knee*drive;
        if(p->cutoff_ceiling>0)frequency/=sqrt(1.0+(frequency/p->cutoff_ceiling)*(frequency/p->cutoff_ceiling));
        s->cutoff_curve[i]=(float)(2.0*sin(PI*frequency/s->clock_hz));
    }
    const selector_profile *q=selectors[s->model];
    for(unsigned code=0;code<4096;++code) {
        /* With the saw selected the HMOS triangle loses its MSB inversion. */
        unsigned rising=(code<<1)&0xffeu,falling=rising^0xffeu;
        unsigned natural=s->model?rising:(code&0x800u?falling:rising);
        unsigned inverted=s->model?rising:(code&0x800u?rising:falling);
        s->combined[0][code]=selector_solve(&q[0],code,natural,1,1,0);
        s->combined[1][code]=selector_solve(&q[0],code,inverted,1,1,0);
        s->combined[2][code]=selector_solve(&q[1],0,code,0,1,1);
        s->combined[3][code]=selector_solve(&q[2],code,0,1,0,1);
        s->combined[4][code]=selector_solve(&q[3],code,natural,1,1,1);
        s->combined[5][code]=selector_solve(&q[3],code,inverted,1,1,1);
    }
    /* Kaiser-windowed sinc over the stream at a quarter of the chip clock:
     * 16 zero crossings either side where the history holds them. */
    double ratio=s->clock_hz/(4.0*s->sample_rate),span=16.0*ratio;
    if(span>RC_SID_OWN_HISTORY/2-2)span=RC_SID_OWN_HISTORY/2-2;
    s->kernel_span=(uint32_t)span;
    double cutoff=0.438/ratio,beta=6.5,window=bessel_i0(beta),norm=0;
    for(unsigned i=0;i<RC_SID_OWN_KERNEL;++i) {
        double t=(double)i/RC_SID_OWN_KERNEL,x=2.0*cutoff*t*s->kernel_span;
        double sinc=x>0?sin(PI*x)/(PI*x):1.0;
        s->kernel[i]=(float)(2.0*cutoff*sinc*bessel_i0(beta*sqrt(1.0-t*t))/window);
    }
    s->kernel[RC_SID_OWN_KERNEL]=s->kernel[RC_SID_OWN_KERNEL+1]=0;
    double step=(double)RC_SID_OWN_KERNEL/s->kernel_span;
    for(int j=-(int)s->kernel_span;j<=(int)s->kernel_span;++j)norm+=kernel_at(s,fabs((double)j)*step);
    for(unsigned i=0;i<RC_SID_OWN_KERNEL;++i)s->kernel[i]=(float)(s->kernel[i]/norm);
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
/* GATE reaches the counter through two latches. A rising edge selects the decay
 * rate until the attack latch opens; an edge that meets a rate or exponential
 * match already in flight lets that step through the longer path. */
static void envelope_gate(rc_sid_own_voice *v,int on) {
    if(on) {
        v->pending_state=RC_SID_OWN_ATTACK;
        v->envelope_state=RC_SID_OWN_DECAY;envelope_rate(v);
        v->state_pipeline=2;
        if(v->rate_reset || v->exponential_pipeline==2)
            v->envelope_pipeline=(v->exponential_period==1 || v->exponential_pipeline==2)?2:4;
        else if(v->exponential_pipeline==1)v->state_pipeline=3;
    } else {
        v->pending_state=RC_SID_OWN_RELEASE;
        v->state_pipeline=v->envelope_pipeline?3:2;
    }
}
static void envelope_clock(rc_sid_own_voice *v) {
    v->envelope_read=v->envelope;
    if(v->state_pipeline) {
        --v->state_pipeline;
        if(v->pending_state==RC_SID_OWN_ATTACK) {
            if(!v->state_pipeline){v->envelope_state=RC_SID_OWN_ATTACK;envelope_rate(v);v->hold_zero=0;}
        } else if((v->envelope_state==RC_SID_OWN_ATTACK && !v->state_pipeline) ||
                  (v->envelope_state==RC_SID_OWN_DECAY && v->state_pipeline==1)) {
            v->envelope_state=RC_SID_OWN_RELEASE;envelope_rate(v);
        }
    }
    if(v->envelope_pipeline && !--v->envelope_pipeline && !v->hold_zero) {
        if(v->envelope_state==RC_SID_OWN_ATTACK) {
            if(++v->envelope==255){v->envelope_state=RC_SID_OWN_DECAY;envelope_rate(v);}
        } else --v->envelope;
        envelope_threshold(v);
    }
    if(v->exponential_pipeline && !--v->exponential_pipeline) {
        v->exponential_counter=0;
        if(v->envelope_state==RC_SID_OWN_RELEASE ||
           (v->envelope_state==RC_SID_OWN_DECAY && v->envelope!=(uint8_t)((v->sustain_release>>4)*17)))
            v->envelope_pipeline=1;
    } else if(v->rate_reset) {
        v->rate_reset=0;
        v->rate_counter=0;
        if(v->envelope_state==RC_SID_OWN_ATTACK) {
            /* LFSR15 reset is latched before release_reset reaches the DAC
             * counter. The two sid_clk1 latches are visible on page8. */
            v->exponential_counter=0;
            v->envelope_pipeline=2;
        } else if(!v->hold_zero && ++v->exponential_counter==v->exponential_period) {
            /* FPF (divide-by-one) bypasses the LFSR5 match/reset latch. */
            v->exponential_pipeline=v->exponential_period>1?2:1;
        }
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
/* The selector word before the pulse and noise lines gate it: triangle,
 * sawtooth, or the combined drive of the selected sources. The ring source
 * is inverted before the triangle XOR on the die. */
static uint16_t accumulator_word(const rc_sid_own *s,const rc_sid_own_voice *v,
                                 const rc_sid_own_voice *source,unsigned wave) {
    unsigned saw=v->phase>>12;
    unsigned falling=(v->phase>>23)&1u;
    if(v->control&4)falling^=(~source->phase>>23)&1u;
    unsigned triangle=((saw<<1)&0xffeu)^(falling?0xffeu:0u);
    unsigned inverted=falling!=(saw>>11);
    switch(wave&7) {
    case 1: return (uint16_t)triangle;
    case 2: return (uint16_t)saw;
    case 3: return s->combined[inverted][saw];
    case 5: return s->combined[2][triangle];
    case 6: return s->combined[3][saw];
    case 7: return s->combined[4+inverted][saw];
    default: return 0xfff;
    }
}
/* A low pulse line or noise bit holds its selector line low. */
static uint16_t selector_output(const rc_sid_own_voice *v,unsigned wave,uint16_t word) {
    if(wave&4)word&=v->pulse_latch;
    if(wave&8)word&=noise_output(v->lfsr);
    return word;
}
/* The comparator works on the accumulator bits latched for the selector, so
 * its result reaches the output a cycle late; TEST overrides it at the clock. */
static void pulse_compare(rc_sid_own_voice *v) {
    v->pulse_latch=(v->phase>>12)>=v->pulse_width?0xfff:0;
}
static void waveform_clock(rc_sid_own *s,unsigned i,int clocked) {
    rc_sid_own_voice *v=&s->voice[i];
    const rc_sid_own_voice *source=&s->voice[(i+2)%3];
    unsigned wave=v->control>>4;
    if(!wave) {
        /* Floating selector/DAC nodes retain the last code before leakage. */
        if(clocked && v->floating_cycles && !--v->floating_cycles)v->waveform=v->osc3=0;
    } else {
        uint16_t word=accumulator_word(s,v,source,wave);
        v->waveform=selector_output(v,wave,word);
        if(s->model==RC_SID_OWN_8580 && (wave&3)) {
            /* HMOS latches the accumulator bits half a cycle before the
             * selector: OSC3, sampled on the first phase, sees the last word. */
            v->osc3=selector_output(v,wave,v->tri_saw_latch);
            v->tri_saw_latch=word;
        } else v->osc3=v->waveform;
        if((wave&8) && (wave&7)) {
            /* The selected noise driver is bidirectional: discharged selector
             * nodes feed back to their physical LFSR storage bits. */
            static const unsigned taps[8]={0,2,5,9,11,14,18,20};
            for(unsigned b=0;b<8;++b)
                if(!(v->waveform&(1u<<(b+4))))v->lfsr&=~(1u<<taps[b]);
        }
        /* The NMOS saw MSB wire reaches the accumulator directly. HMOS has a
         * separate latch/buffer, preventing selector loading from resetting it. */
        if(!s->model && (wave&2) && wave!=2 && !(v->waveform&0x800))v->phase&=0x7fffff;
    }
    pulse_compare(v);
}
static void filter_setup(rc_sid_own *s) {
    const chip_profile *p=profile(s);
    unsigned fc=((unsigned)s->registers[22]<<3)|(s->registers[21]&7);
    unsigned resonance=s->registers[23]>>4,volume=s->registers[24]&15;
    /* The resonance ladder: linear conductance steps on NMOS, an eighth of
     * an octave of feedback gain per step on HMOS. */
    s->filter_damping=p->summer_band*pow(2.0,-p->summer_band_octave*resonance)+
        p->summer_band_step*(15u-resonance);
    if(s->model==RC_SID_OWN_6581) {
        s->cutoff_gate=0;
        for(unsigned b=0;b<11;++b)if(fc&(1u<<b))s->cutoff_gate+=s->cutoff_weight[b];
        /* The curve is the resonant frequency; the loop gain below one
         * sets it under the integrator frequency. */
        s->cutoff_scale=1.0/sqrt(p->summer_low+(s->filter_damping+p->loss_band)*p->loss_low);
        s->volume_gain=volume*(15.0+p->volume_knee)/(15.0*(volume+p->volume_knee));
    } else {
        /* Linear current DAC, drooping slightly towards full scale. */
        double frequency=fc*(p->cutoff_slope+p->cutoff_ceiling*fc);
        s->filter_coefficient=2.0*sin(PI*frequency/s->clock_hz);
        s->volume_gain=volume/15.0;
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
        case 2:v->pulse_width=(uint16_t)((v->pulse_width&0xf00)|value);pulse_compare(v);break;
        case 3:v->pulse_width=(uint16_t)((v->pulse_width&0xff)|((value&15)<<8));pulse_compare(v);break;
        case 4: {
            uint8_t previous=v->control;
            v->control=value;
            if((previous^value)&1)envelope_gate(v,value&1);
            if((value&8) && !(previous&8)) {
                v->phase=0;v->noise_pipeline=0;v->test_cycles=0;
                v->pulse_latch=0xfff;
            } else if(!(value&8) && (previous&8))noise_shift(v,1);
            if(!(value&0xf0) && (previous&0xf0))
                v->floating_cycles=profile(s)->floating_cycles;
            break;
        }
        case 5:v->attack_decay=value;envelope_rate(v);break;
        case 6:v->sustain_release=value;envelope_rate(v);break;
        default:break;
        }
    } else filter_setup(s);
}
static double cutoff_rate(const rc_sid_own *s,double gate) {
    double position=(gate+1.0)*(RC_SID_OWN_CURVE/3.0);
    if(position<0)position=0;
    if(position>RC_SID_OWN_CURVE)position=RC_SID_OWN_CURVE;
    unsigned i=(unsigned)position;double f=position-i;
    return s->cutoff_curve[i]+f*(s->cutoff_curve[i+1]-s->cutoff_curve[i]);
}
static void analog_clock(rc_sid_own *s) {
    const chip_profile *p=profile(s);
    unsigned route=s->registers[23],mode=s->registers[24],inputs=0;
    double input=0,mix=0;
    for(unsigned i=0;i<3;++i) {
        const rc_sid_own_voice *v=&s->voice[i];
        double voice=p->voice_dc+p->voice_gain*s->waveform_level[v->waveform]*s->envelope_level[v->envelope];
        if(route&(1u<<i))input+=voice;
        else if(i!=2 || !(mode&128)){mix+=voice;++inputs;}
    }
    /* Inverting HP summer and two inverting integrators. The BP feedback
     * amplifier adds one more inversion, making its summer term positive. */
    double low_rate=s->filter_coefficient,band_rate=s->filter_coefficient;
    if(s->model==RC_SID_OWN_6581) {
        /* The channel potential lowers each FET's conductance: the cutoff
         * moves with the signal and with the DC the voices carry. */
        low_rate=s->cutoff_scale*cutoff_rate(s,s->cutoff_gate+p->cutoff_feedback*s->filter_low);
        band_rate=s->cutoff_scale*cutoff_rate(s,s->cutoff_gate+p->cutoff_feedback*s->filter_band);
    }
    s->filter_low-=low_rate*(s->filter_band+p->loss_low*s->filter_low);
    s->filter_band-=band_rate*(s->filter_high+p->loss_band*s->filter_band);
    s->filter_high=s->filter_damping*s->filter_band-p->summer_low*s->filter_low-p->summer_input*input;
    if(mode&16){mix+=s->filter_low;++inputs;}
    if(mode&32){mix+=s->filter_band;++inputs;}
    if(mode&64){mix+=s->filter_high;++inputs;}
    /* A summing inverter of gain A divides by 1+(n+1)/A for n equal inputs;
     * levels are referred to the three-voice case. */
    if(p->mixer_gain>0)mix*=(1.0+4.0/p->mixer_gain)/(1.0+(inputs+1.0)/p->mixer_gain);
    double mixed=p->output_offset+s->volume_gain*mix;
    /* The NMOS output stage bottoms out well below its idle level. */
    if(mixed<p->output_knee)mixed=p->output_knee-p->output_reach*tanh((p->output_knee-mixed)/p->output_reach);
    /* Board 250469: 10 kohm / 1000 pF LP and 10 uF / 1 kohm AC coupling.
     * Backward Euler is stable at every supported machine/sample frequency. */
    double lp=1.0/(1.0+0.00001*s->clock_hz);
    double hp=1.0/(1.0+0.01*s->clock_hz);
    s->external_low+=lp*(mixed-s->external_low);
    s->external_high+=hp*(s->external_low-s->external_high);
    s->cycle_output=s->external_filter?s->external_low-s->external_high:mixed;
}
static void audio_push(rc_sid_own *s,double sample) {
    sample=floor(sample+0.5);
    if(sample>32767)sample=32767;
    if(sample< -32768)sample= -32768;
    /* Unconsumed host audio must not stop the guest clock. Keep the
     * newest samples, as the reSID bridge does for its output queue. */
    if(s->audio_count==RC_SID_OWN_AUDIO_CAPACITY){s->audio_head=(s->audio_head+1u)%RC_SID_OWN_AUDIO_CAPACITY;--s->audio_count;}
    unsigned tail=(s->audio_head+s->audio_count)%RC_SID_OWN_AUDIO_CAPACITY;
    s->audio[tail]=(int16_t)sample;++s->audio_count;
}
static void resample_clock(rc_sid_own *s) {
    s->recent[s->recent_head]=s->cycle_output;s->recent_head=(s->recent_head+1u)&7u;
    if(s->cycles&3u)return;
    /* Two cascaded four-cycle averages: nulls on the folding frequencies. */
    static const double triangle[7]={1,2,3,4,3,2,1};
    double sum=0;
    for(unsigned j=0;j<7;++j)sum+=triangle[j]*s->recent[(s->recent_head+7u-j)&7u];
    s->history[s->history_head]=(float)(sum/16.0);
    s->history_head=(s->history_head+1u)&(RC_SID_OWN_HISTORY-1u);
    /* Rational phase avoids PAL/NTSC drift. */
    s->sample_phase+=4u*s->sample_rate;
    if(s->sample_phase<s->clock_hz)return;
    s->sample_phase-=s->clock_hz;
    /* The sample instant lies this far before the newest history entry. */
    double step=(double)RC_SID_OWN_KERNEL/s->kernel_span;
    double position=(s->kernel_span+(double)s->sample_phase/(4.0*s->sample_rate))*step;
    double sample=0;
    for(unsigned j=0;j<=2u*s->kernel_span;++j,position-=step)
        sample+=s->history[(s->history_head-1u-j)&(RC_SID_OWN_HISTORY-1u)]*kernel_at(s,fabs(position));
    audio_push(s,sample*profile(s)->host_gain);
}
int rc_sid_own_init(rc_sid_own *s,unsigned model,uint32_t clock_hz,uint32_t sample_rate) {
    if(!s || model>1 || clock_hz<100000 || clock_hz>2000000 ||
        sample_rate<8000 || sample_rate>192000 || sample_rate>=clock_hz/4)return 0;
    memset(s,0,sizeof *s);
    s->model=(uint8_t)model;s->clock_hz=clock_hz;s->sample_rate=sample_rate;
    /* Reproducible cold-start convention; the silicon power-on state of the
     * oscillator/envelope storage nodes is not specified by the data sheet.
     * Phase and noise register start where the default engine starts, so
     * the two engines can be compared sample for sample on the same tune. */
    s->pot[0]=s->pot[1]=255;s->external_filter=1;
    for(unsigned i=0;i<3;++i) {
        rc_sid_own_voice *v=&s->voice[i];
        v->lfsr=0x7fffff;v->pulse_latch=0xfff;v->phase=0x555555;
        v->envelope_state=v->pending_state=RC_SID_OWN_RELEASE;
        v->exponential_period=1;v->hold_zero=1;envelope_rate(v);
    }
    derived_setup(s);filter_setup(s);
    /* The coupling capacitor starts charged to the idle output level. */
    s->external_low=s->external_high=profile(s)->output_offset;
    for(unsigned i=0;i<8;++i)s->recent[i]=0;
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
        v->envelope_pipeline=0;v->exponential_pipeline=0;v->state_pipeline=0;
        v->rate_reset=0;v->hold_zero=(uint8_t)(v->envelope==0);
        v->noise_pipeline=0;v->test_cycles=0;v->msb_rising=0;
        v->pulse_latch=0xfff;v->tri_saw_latch=0;v->osc3=v->waveform;
        v->floating_cycles=profile(s)->floating_cycles;
        /* Reset, like TEST, opens noise shift phase 1. Releasing the reset
         * pin closes phase 2 with the forced-high XOR feedback input. */
        noise_shift(v,1);
        envelope_rate(v);
    }
    filter_setup(s);
}
void rc_sid_own_write(rc_sid_own *s,unsigned reg,uint8_t value) {
    reg&=31;s->bus_value=value;s->bus_cycles=profile(s)->bus_cycles;
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
    case 27:return (uint8_t)(s->voice[2].osc3>>4);
    case 28:return s->voice[2].envelope_read;
    default:return s->bus_value;
    }
}
uint8_t rc_sid_own_read(rc_sid_own *s,unsigned reg) {
    uint8_t value=rc_sid_own_peek(s,reg);
    if((reg&31)>=25 && (reg&31)<=28) {
        s->bus_value=value;s->bus_cycles=profile(s)->bus_cycles;
    }
    return value;
}
uint8_t rc_sid_own_pot_read(rc_sid_own *s,unsigned reg,uint8_t value) {
    s->pot[(reg-25)&1]=value;s->bus_value=value;s->bus_cycles=profile(s)->bus_cycles;
    return value;
}
void rc_sid_own_tick(rc_sid_own *s) {
    const chip_profile *p=profile(s);
    ++s->cycles;
    if(s->bus_cycles && !--s->bus_cycles)s->bus_value=0;
    for(unsigned i=0;i<3;++i) {
        rc_sid_own_voice *v=&s->voice[i];uint32_t previous=v->phase;
        envelope_clock(v);v->msb_rising=0;
        if(v->control&8) {
            v->phase=0;v->pulse_latch=0xfff;
            /* Die analysis: with both shift phases open the storage gates
             * leak towards one, a neighbour at a time. */
            if(v->lfsr!=NOISE_MASK && ++v->test_cycles>=p->fade_start &&
               !((v->test_cycles-p->fade_start)%p->fade_step))
                {v->lfsr|=1u;v->lfsr=(v->lfsr|(v->lfsr<<1))&NOISE_MASK;}
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
    for(unsigned i=0;i<3;++i)waveform_clock(s,i,1);
    analog_clock(s);
    resample_clock(s);
    if(s->write_pending) {
        s->write_pending=0;write_register(s,s->write_reg,s->write_value);
        /* The selector is combinational: OSC3 follows a new control value
         * on the latched accumulator bits before the next clock. */
        if(s->write_reg<21 && s->write_reg%7==4)waveform_clock(s,s->write_reg/7u,0);
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
#define VOICE_U16(X) X(frequency) X(pulse_width) X(waveform) X(pulse_latch) X(tri_saw_latch) X(osc3) X(rate_counter) X(rate_period)
#define VOICE_U8(X) X(control) X(attack_decay) X(sustain_release) X(envelope) X(envelope_read) X(envelope_state) X(exponential_counter) X(exponential_period) X(hold_zero) X(noise_pipeline) X(envelope_pipeline) X(exponential_pipeline) X(state_pipeline) X(pending_state) X(rate_reset) X(msb_rising)
#define CHIP_U32(X) X(clock_hz) X(sample_rate) X(bus_cycles) X(sample_phase) X(recent_head) X(history_head) X(audio_head) X(audio_count)
#define CHIP_U8(X) X(model) X(bus_value) X(write_pending) X(write_reg) X(write_value) X(failed) X(external_filter)
#define CHIP_DOUBLE(X) X(filter_low) X(filter_band) X(filter_high) X(external_low) X(external_high) X(cycle_output)
void rc_sid_own_save(const rc_sid_own *s,rc_state_writer *w) {
    rc_sw_u32(w,4);
    for(unsigned i=0;i<3;++i) {
        const rc_sid_own_voice *v=&s->voice[i];
#define W32(f) rc_sw_u32(w,v->f);
#define W16(f) rc_sw_u16(w,v->f);
#define W8(f) rc_sw_u8(w,v->f);
        VOICE_U32(W32) VOICE_U16(W16) VOICE_U8(W8)
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
    for(unsigned i=0;i<8;++i)write_double(w,s->recent[i]);
    for(unsigned i=0;i<RC_SID_OWN_HISTORY;++i) {
        uint32_t bits;memcpy(&bits,&s->history[i],sizeof bits);rc_sw_u32(w,bits);
    }
    for(unsigned i=0;i<RC_SID_OWN_AUDIO_CAPACITY;++i)rc_sw_u16(w,(uint16_t)s->audio[i]);
}
int rc_sid_own_load(rc_sid_own *s,rc_state_reader *r) {
    rc_sid_own next;memset(&next,0,sizeof next);
    if(rc_sr_u32(r)!=4)return 0;
    for(unsigned i=0;i<3;++i) {
        rc_sid_own_voice *v=&next.voice[i];
#define R32(f) v->f=rc_sr_u32(r);
#define R16(f) v->f=rc_sr_u16(r);
#define R8(f) v->f=rc_sr_u8(r);
        VOICE_U32(R32) VOICE_U16(R16) VOICE_U8(R8)
#undef R32
#undef R16
#undef R8
        if(v->phase>PHASE_MASK || v->lfsr>NOISE_MASK || v->pulse_width>0xfff ||
           v->waveform>0xfff || v->pulse_latch>0xfff || v->tri_saw_latch>0xfff || v->osc3>0xfff ||
           v->envelope_state>RC_SID_OWN_RELEASE || v->pending_state>RC_SID_OWN_RELEASE ||
           v->rate_counter>32767 || !v->rate_period || v->rate_period>31251 ||
           !v->exponential_period || v->exponential_period>30 ||
           v->noise_pipeline>2 || v->state_pipeline>3 || v->envelope_pipeline>4 ||
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
    for(unsigned i=0;i<8;++i){next.recent[i]=read_double(r);if(!isfinite(next.recent[i]))return 0;}
    for(unsigned i=0;i<RC_SID_OWN_HISTORY;++i) {
        uint32_t bits=rc_sr_u32(r);memcpy(&next.history[i],&bits,sizeof bits);
        if(!isfinite(next.history[i]))return 0;
    }
    for(unsigned i=0;i<RC_SID_OWN_AUDIO_CAPACITY;++i)next.audio[i]=(int16_t)rc_sr_u16(r);
    if(!rc_sr_ok(r) || next.model!=s->model || next.clock_hz!=s->clock_hz ||
       next.sample_rate!=s->sample_rate || next.sample_phase>=next.clock_hz ||
       next.recent_head>7 || next.history_head>=RC_SID_OWN_HISTORY ||
       next.audio_head>=RC_SID_OWN_AUDIO_CAPACITY || next.audio_count>RC_SID_OWN_AUDIO_CAPACITY ||
       next.write_pending>1 || next.write_reg>31 || next.failed>1 || next.external_filter>1)return 0;
    derived_setup(&next);filter_setup(&next);
    *s=next;return 1;
}
