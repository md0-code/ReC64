/* MOS programming/hardware manuals, 64doc instruction timing and NMOS
 * arithmetic descriptions. Opcode matrix transcribed from those documents. */
#include "mos6510.h"
#include <string.h>
enum { IMP,ACC,IMM,ZP,ZX,ZY,ABS,AX,AY,IX,IY,REL,IND };
enum { BRK,ORA,JAM,SLO,NOP,ASL,PHP,ANC,BPL,CLC,JSR,AND,RLA,BIT,ROL,PLP,BMI,SEC,
    RTI,EOR,SRE,LSR,PHA,ALR,JMP,BVC,CLI,RTS,ADC,RRA,ROR,PLA,ARR,BVS,SEI,
    STA,SAX,STY,STX,DEY,TXA,XAA,BCC,AHX,TYA,TXS,TAS,SHY,SHX,
    LDY,LDA,LDX,LAX,TAY,TAX,BCS,CLV,TSX,LAS,CPY,CMP,DCP,DEC,INY,DEX,AXS,BNE,CLD,
    CPX,SBC,ISC,INC,INX,BEQ,SED };
typedef struct instruction { uint8_t op,mode; } instruction;
#define O(op,mode) {op,mode}
static const instruction TABLE[256]={
    O(BRK,IMP),O(ORA,IX),O(JAM,IMP),O(SLO,IX),O(NOP,ZP),O(ORA,ZP),O(ASL,ZP),O(SLO,ZP),O(PHP,IMP),O(ORA,IMM),O(ASL,ACC),O(ANC,IMM),O(NOP,ABS),O(ORA,ABS),O(ASL,ABS),O(SLO,ABS),
    O(BPL,REL),O(ORA,IY),O(JAM,IMP),O(SLO,IY),O(NOP,ZX),O(ORA,ZX),O(ASL,ZX),O(SLO,ZX),O(CLC,IMP),O(ORA,AY),O(NOP,IMP),O(SLO,AY),O(NOP,AX),O(ORA,AX),O(ASL,AX),O(SLO,AX),
    O(JSR,ABS),O(AND,IX),O(JAM,IMP),O(RLA,IX),O(BIT,ZP),O(AND,ZP),O(ROL,ZP),O(RLA,ZP),O(PLP,IMP),O(AND,IMM),O(ROL,ACC),O(ANC,IMM),O(BIT,ABS),O(AND,ABS),O(ROL,ABS),O(RLA,ABS),
    O(BMI,REL),O(AND,IY),O(JAM,IMP),O(RLA,IY),O(NOP,ZX),O(AND,ZX),O(ROL,ZX),O(RLA,ZX),O(SEC,IMP),O(AND,AY),O(NOP,IMP),O(RLA,AY),O(NOP,AX),O(AND,AX),O(ROL,AX),O(RLA,AX),
    O(RTI,IMP),O(EOR,IX),O(JAM,IMP),O(SRE,IX),O(NOP,ZP),O(EOR,ZP),O(LSR,ZP),O(SRE,ZP),O(PHA,IMP),O(EOR,IMM),O(LSR,ACC),O(ALR,IMM),O(JMP,ABS),O(EOR,ABS),O(LSR,ABS),O(SRE,ABS),
    O(BVC,REL),O(EOR,IY),O(JAM,IMP),O(SRE,IY),O(NOP,ZX),O(EOR,ZX),O(LSR,ZX),O(SRE,ZX),O(CLI,IMP),O(EOR,AY),O(NOP,IMP),O(SRE,AY),O(NOP,AX),O(EOR,AX),O(LSR,AX),O(SRE,AX),
    O(RTS,IMP),O(ADC,IX),O(JAM,IMP),O(RRA,IX),O(NOP,ZP),O(ADC,ZP),O(ROR,ZP),O(RRA,ZP),O(PLA,IMP),O(ADC,IMM),O(ROR,ACC),O(ARR,IMM),O(JMP,IND),O(ADC,ABS),O(ROR,ABS),O(RRA,ABS),
    O(BVS,REL),O(ADC,IY),O(JAM,IMP),O(RRA,IY),O(NOP,ZX),O(ADC,ZX),O(ROR,ZX),O(RRA,ZX),O(SEI,IMP),O(ADC,AY),O(NOP,IMP),O(RRA,AY),O(NOP,AX),O(ADC,AX),O(ROR,AX),O(RRA,AX),
    O(NOP,IMM),O(STA,IX),O(NOP,IMM),O(SAX,IX),O(STY,ZP),O(STA,ZP),O(STX,ZP),O(SAX,ZP),O(DEY,IMP),O(NOP,IMM),O(TXA,IMP),O(XAA,IMM),O(STY,ABS),O(STA,ABS),O(STX,ABS),O(SAX,ABS),
    O(BCC,REL),O(STA,IY),O(JAM,IMP),O(AHX,IY),O(STY,ZX),O(STA,ZX),O(STX,ZY),O(SAX,ZY),O(TYA,IMP),O(STA,AY),O(TXS,IMP),O(TAS,AY),O(SHY,AX),O(STA,AX),O(SHX,AY),O(AHX,AY),
    O(LDY,IMM),O(LDA,IX),O(LDX,IMM),O(LAX,IX),O(LDY,ZP),O(LDA,ZP),O(LDX,ZP),O(LAX,ZP),O(TAY,IMP),O(LDA,IMM),O(TAX,IMP),O(LAX,IMM),O(LDY,ABS),O(LDA,ABS),O(LDX,ABS),O(LAX,ABS),
    O(BCS,REL),O(LDA,IY),O(JAM,IMP),O(LAX,IY),O(LDY,ZX),O(LDA,ZX),O(LDX,ZY),O(LAX,ZY),O(CLV,IMP),O(LDA,AY),O(TSX,IMP),O(LAS,AY),O(LDY,AX),O(LDA,AX),O(LDX,AY),O(LAX,AY),
    O(CPY,IMM),O(CMP,IX),O(NOP,IMM),O(DCP,IX),O(CPY,ZP),O(CMP,ZP),O(DEC,ZP),O(DCP,ZP),O(INY,IMP),O(CMP,IMM),O(DEX,IMP),O(AXS,IMM),O(CPY,ABS),O(CMP,ABS),O(DEC,ABS),O(DCP,ABS),
    O(BNE,REL),O(CMP,IY),O(JAM,IMP),O(DCP,IY),O(NOP,ZX),O(CMP,ZX),O(DEC,ZX),O(DCP,ZX),O(CLD,IMP),O(CMP,AY),O(NOP,IMP),O(DCP,AY),O(NOP,AX),O(CMP,AX),O(DEC,AX),O(DCP,AX),
    O(CPX,IMM),O(SBC,IX),O(NOP,IMM),O(ISC,IX),O(CPX,ZP),O(SBC,ZP),O(INC,ZP),O(ISC,ZP),O(INX,IMP),O(SBC,IMM),O(NOP,IMP),O(SBC,IMM),O(CPX,ABS),O(SBC,ABS),O(INC,ABS),O(ISC,ABS),
    O(BEQ,REL),O(SBC,IY),O(JAM,IMP),O(ISC,IY),O(NOP,ZX),O(SBC,ZX),O(INC,ZX),O(ISC,ZX),O(SED,IMP),O(SBC,AY),O(NOP,IMP),O(ISC,AY),O(NOP,AX),O(SBC,AX),O(INC,AX),O(ISC,AX)
};
#undef O
static void read_at(rc_mos6510 *c,uint16_t a) { c->pins=(rc_cpu_cycle){a,0,0,0}; }
static void write_at(rc_mos6510 *c,uint16_t a,uint8_t v) { c->pins=(rc_cpu_cycle){a,v,1,0}; }
static void flag(rc_mos6510 *c,unsigned mask,int set) { c->p=(uint8_t)((c->p&~mask)|(set?mask:0)); }
static uint8_t nz(rc_mos6510 *c,uint8_t v) { flag(c,RC_P_N,v&128);flag(c,RC_P_Z,v==0);return v; }
static void compare(rc_mos6510 *c,uint8_t a,uint8_t b) { flag(c,RC_P_C,a>=b);nz(c,(uint8_t)(a-b)); }
static void adc(rc_mos6510 *c,uint8_t v) {
    const unsigned a=c->a,carry=c->p&RC_P_C,sum=a+v+carry;
    unsigned out=sum;
    if(c->p&RC_P_D) {
        unsigned low=(a&15u)+(v&15u)+carry;
        if(low>9) low=((low+6)&15u)+16u;
        out=(a&240u)+(v&240u)+low;
        flag(c,RC_P_N,out&128);flag(c,RC_P_Z,(uint8_t)sum==0);
        flag(c,RC_P_V,(~(a^v)&(a^out)&128)!=0);
        if(out>0x9f) out+=0x60;
    } else { nz(c,(uint8_t)sum);flag(c,RC_P_V,(~(a^v)&(a^sum)&128)!=0); }
    flag(c,RC_P_C,out>255);c->a=(uint8_t)out;
}
static void sbc(rc_mos6510 *c,uint8_t v) {
    const int a=c->a,borrow=!(c->p&RC_P_C),sum=a-v-borrow;
    flag(c,RC_P_C,sum>=0);flag(c,RC_P_V,((a^v)&(a^sum)&128)!=0);nz(c,(uint8_t)sum);
    if(c->p&RC_P_D) {
        int low=(a&15)-(v&15)-borrow;
        int high=(a>>4)-(v>>4);
        if(low<0) {low-=6;--high;}
        if(high<0) high-=6;
        c->a=(uint8_t)(((unsigned)high<<4)|((unsigned)low&15u));
    } else c->a=(uint8_t)sum;
}
static uint8_t modify(rc_mos6510 *c,unsigned op,uint8_t v) {
    const unsigned carry=c->p&RC_P_C;uint8_t r=v;
    switch(op) {
    case ASL:case SLO:flag(c,RC_P_C,v&128);r=(uint8_t)(v<<1);break;
    case LSR:case SRE:flag(c,RC_P_C,v&1);r=v>>1;break;
    case ROL:case RLA:flag(c,RC_P_C,v&128);r=(uint8_t)((v<<1)|carry);break;
    case ROR:case RRA:flag(c,RC_P_C,v&1);r=(uint8_t)((v>>1)|(carry<<7));break;
    case INC:case ISC:r=(uint8_t)(v+1);break;
    case DEC:case DCP:r=(uint8_t)(v-1);break;
    default:break;
    }
    nz(c,r);
    switch(op) {
    case SLO:c->a=nz(c,c->a|r);break;case RLA:c->a=nz(c,c->a&r);break;
    case SRE:c->a=nz(c,c->a^r);break;case RRA:adc(c,r);break;
    case ISC:sbc(c,r);break;case DCP:compare(c,c->a,r);break;default:break;
    }
    return r;
}
static void execute(rc_mos6510 *c,unsigned op,uint8_t v) {
    switch(op) {
    case ORA:c->a=nz(c,c->a|v);break;case AND:c->a=nz(c,c->a&v);break;case EOR:c->a=nz(c,c->a^v);break;
    case ADC:adc(c,v);break;case SBC:sbc(c,v);break;
    case LDA:c->a=nz(c,v);break;case LDX:c->x=nz(c,v);break;case LDY:c->y=nz(c,v);break;
    case LAX:c->a=c->x=nz(c,c->opcode==0xab?(uint8_t)((c->a|0xee)&v):v);break;
    case LAS:c->a=c->x=c->s=nz(c,v&c->s);break;
    case CMP:compare(c,c->a,v);break;case CPX:compare(c,c->x,v);break;case CPY:compare(c,c->y,v);break;
    case BIT:flag(c,RC_P_N,v&128);flag(c,RC_P_V,v&64);flag(c,RC_P_Z,!(c->a&v));break;
    case ANC:c->a=nz(c,c->a&v);flag(c,RC_P_C,c->a&128);break;
    case ALR:c->a=(uint8_t)(c->a&v);c->a=modify(c,LSR,c->a);break;
    case ARR:{
        const uint8_t t=c->a&v;c->a=nz(c,(uint8_t)((t>>1)|((c->p&1u)<<7)));
        flag(c,RC_P_V,(t^c->a)&64);
        if(c->p&RC_P_D) {
            if((t&15u)+(t&1u)>5) c->a=(uint8_t)((c->a&240u)|((c->a+6u)&15u));
            flag(c,RC_P_C,(t>>4)+(t>>4&1u)>5);
            if(c->p&RC_P_C)c->a=(uint8_t)(c->a+0x60);
        } else flag(c,RC_P_C,c->a&64);
        break;}
    case XAA:c->a=nz(c,(c->a|0xee)&c->x&v);break;
    case AXS:{const unsigned a=c->a&c->x;flag(c,RC_P_C,a>=v);c->x=nz(c,(uint8_t)(a-v));break;}
    case CLC:c->p&=(uint8_t)~RC_P_C;break;case SEC:c->p|=RC_P_C;break;
    case CLI:c->p&=(uint8_t)~RC_P_I;break;case SEI:c->p|=RC_P_I;break;
    case CLD:c->p&=(uint8_t)~RC_P_D;break;case SED:c->p|=RC_P_D;break;case CLV:c->p&=(uint8_t)~RC_P_V;break;
    case DEY:c->y=nz(c,(uint8_t)(c->y-1));break;case INY:c->y=nz(c,(uint8_t)(c->y+1));break;
    case DEX:c->x=nz(c,(uint8_t)(c->x-1));break;case INX:c->x=nz(c,(uint8_t)(c->x+1));break;
    case TXA:c->a=nz(c,c->x);break;case TYA:c->a=nz(c,c->y);break;case TAY:c->y=nz(c,c->a);break;
    case TAX:c->x=nz(c,c->a);break;case TSX:c->x=nz(c,c->s);break;case TXS:c->s=c->x;break;
    default:break;
    }
}
static int store(unsigned op) {return op==STA||op==STX||op==STY||op==SAX||op==AHX||op==TAS||op==SHX||op==SHY;}
static int rmw(unsigned op) {return op==ASL||op==LSR||op==ROL||op==ROR||op==INC||op==DEC||op==SLO||op==SRE||op==RLA||op==RRA||op==ISC||op==DCP;}
static void access(rc_mos6510 *c,unsigned op) {
    if(!store(op)){read_at(c,c->address);return;}
    uint8_t v=op==STA?c->a:op==STX?c->x:op==STY?c->y:(uint8_t)(c->a&c->x);
    if(op==AHX||op==TAS||op==SHX||op==SHY) {
        if(op==TAS)c->s=c->a&c->x;
        if(op==SHX)v=c->x;if(op==SHY)v=c->y;
        v&=(uint8_t)((c->base>>8)+1u);
        if((c->base^c->address)&0xff00u)c->address=(uint16_t)((uint16_t)v<<8|(c->address&255u));
    }
    write_at(c,c->address,v);
}
void rc_mos6510_start(rc_mos6510 *c) {c->step=0;read_at(c,c->pc);c->pins.sync=1;}
static void finish(rc_mos6510 *c) {
    c->irq_pending=c->irq_poll;
    rc_mos6510_start(c);
}
static void operand(rc_mos6510 *c,unsigned op,uint8_t v) {
    if(store(op)){finish(c);return;}
    if(rmw(op)){c->value=v;c->step=20;write_at(c,c->address,v);return;}
    execute(c,op,v);finish(c);
}
static int branch(const rc_mos6510 *c,unsigned op) {
    switch(op){case BPL:return !(c->p&RC_P_N);case BMI:return c->p&RC_P_N;case BVC:return !(c->p&RC_P_V);case BVS:return c->p&RC_P_V;
    case BCC:return !(c->p&RC_P_C);case BCS:return c->p&RC_P_C;case BNE:return !(c->p&RC_P_Z);default:return c->p&RC_P_Z;}
}
static uint16_t stack(const rc_mos6510 *c) {return (uint16_t)(0x100u|c->s);}
void rc_mos6510_clock(rc_mos6510 *c,uint8_t v,int ready) {
    ++c->cycles;if(!ready&&!c->pins.write)return;
    c->irq_poll=(uint8_t)(c->irq&&!(c->p&RC_P_I));
    if(c->resetting) {
        switch(c->resetting++) {
        case 1:read_at(c,c->pc);break;case 2:read_at(c,stack(c));break;
        case 3:case 4:--c->s;read_at(c,stack(c));break;
        case 5:--c->s;c->p|=RC_P_I;read_at(c,0xfffc);break;
        case 6:c->lo=v;read_at(c,0xfffd);break;
        default:c->pc=(uint16_t)(v<<8|c->lo);c->resetting=0;finish(c);break;
        }return;
    }
    if(c->jammed) {
        if(c->step==2){read_at(c,0xfffe);c->step=3;}
        else if(c->step==3)c->step=4;
        else {read_at(c,0xffff);c->step=5;}
        return;
    }
    if(c->step==0) {
        c->interrupt=0;
        if(c->nmi_pending||c->irq_pending) {
            c->interrupt=1;c->vector=c->nmi_pending?0xfffa:0xfffe;c->nmi_pending=0;c->opcode=0;
        } else {
            if(rc_cpu_watched(c->hooks,c->pc)&&c->hooks->before_fetch&&c->hooks->before_fetch(c->hooks->ctx,c->pc)) {rc_mos6510_start(c);return;}
            c->opcode=v;
            if(rc_cpu_watched(c->hooks,c->pc)&&c->hooks->after_fetch)c->hooks->after_fetch(c->hooks->ctx,c->pc,v);
            ++c->pc;
        }
        c->step=1;read_at(c,c->pc);return;
    }
    const unsigned op=TABLE[c->opcode].op,mode=TABLE[c->opcode].mode;
    if(op==BRK) {
        switch(c->step++) {
        case 1:if(!c->interrupt)++c->pc;write_at(c,stack(c),(uint8_t)(c->pc>>8));break;
        case 2:--c->s;write_at(c,stack(c),(uint8_t)c->pc);break;
        case 3:--c->s;write_at(c,stack(c),(uint8_t)((c->p|RC_P_U)|(c->interrupt?0:RC_P_B)));if(c->interrupt)c->pins.data&=(uint8_t)~RC_P_B;break;
        case 4:--c->s;c->p|=RC_P_I;if(c->nmi_pending){c->vector=0xfffa;c->nmi_pending=0;}else if(!c->interrupt)c->vector=0xfffe;read_at(c,c->vector);break;
        case 5:c->lo=v;read_at(c,(uint16_t)(c->vector+1));break;
        default:c->pc=(uint16_t)(v<<8|c->lo);c->irq_pending=0;rc_mos6510_start(c);break;
        }return;
    }
    if(op==JAM){c->jammed=1;c->step=2;read_at(c,0xffff);return;}
    if(op==JSR) {
        switch(c->step++) {
        case 1:c->lo=v;++c->pc;read_at(c,stack(c));break;
        case 2:write_at(c,stack(c),(uint8_t)(c->pc>>8));break;
        case 3:--c->s;write_at(c,stack(c),(uint8_t)c->pc);break;
        case 4:--c->s;read_at(c,c->pc);break;
        default:c->pc=(uint16_t)(v<<8|c->lo);finish(c);break;
        }return;
    }
    if(op==PHA||op==PHP) {
        if(c->step++==1)write_at(c,stack(c),op==PHA?c->a:(uint8_t)(c->p|RC_P_B|RC_P_U));
        else {--c->s;finish(c);}return;
    }
    if(op==PLA||op==PLP||op==RTI||op==RTS) {
        switch(c->step++) {
        case 1:read_at(c,stack(c));break;
        case 2:++c->s;read_at(c,stack(c));break;
        case 3:
            if(op==PLA){c->a=nz(c,v);finish(c);}
            else if(op==PLP){c->p=(uint8_t)((v|RC_P_U)&~RC_P_B);finish(c);}
            else {if(op==RTI)c->p=(uint8_t)((v|RC_P_U)&~RC_P_B);else c->lo=v;++c->s;read_at(c,stack(c));}break;
        case 4:if(op==RTI){c->lo=v;++c->s;read_at(c,stack(c));}else {c->pc=(uint16_t)(v<<8|c->lo);read_at(c,c->pc);}break;
        default:if(op==RTI)c->pc=(uint16_t)(v<<8|c->lo);else ++c->pc;finish(c);break;
        }return;
    }
    if(c->step==20){c->result=modify(c,op,c->value);c->step=21;write_at(c,c->address,c->result);return;}
    if(c->step==21){finish(c);return;}
    switch(mode) {
    case IMP:execute(c,op,v);finish(c);break;
    case ACC:c->a=modify(c,op,c->a);finish(c);break;
    case IMM:++c->pc;execute(c,op,v);finish(c);break;
    case REL:
        if(c->step==1){++c->pc;c->address=(uint16_t)(c->pc+(v<128?(int)v:(int)v-256));if(!branch(c,op))finish(c);else {c->step=2;read_at(c,c->pc);}}
        else if(c->step==2&&((c->pc^c->address)&0xff00u)){read_at(c,(uint16_t)((c->pc&0xff00u)|(c->address&255u)));c->step=3;}
        else {c->pc=c->address;finish(c);}break;
    case ZP:case ZX:case ZY:
        if(c->step==1){c->address=v;++c->pc;c->step=2;if(mode==ZP)access(c,op);else read_at(c,v);}
        else if(c->step==2&&mode!=ZP){c->address=(uint8_t)(c->address+(mode==ZX?c->x:c->y));c->step=3;access(c,op);}
        else operand(c,op,v);break;
    case ABS:case AX:case AY:case IND:
        if(c->step==1){c->lo=v;++c->pc;c->step=2;read_at(c,c->pc);}
        else if(c->step==2){++c->pc;c->base=c->address=(uint16_t)(v<<8|c->lo);c->step=3;
            if(op==JMP&&mode==ABS){c->pc=c->address;finish(c);}
            else if(mode==AX||mode==AY){c->address=(uint16_t)(c->base+(mode==AX?c->x:c->y));read_at(c,(uint16_t)((c->base&0xff00u)|(c->address&255u)));}
            else access(c,op);}
        else if(mode==IND){if(c->step==3){c->lo=v;c->step=4;read_at(c,(uint16_t)((c->address&0xff00u)|((c->address+1)&255u)));}else {c->pc=(uint16_t)(v<<8|c->lo);finish(c);}}
        else if(c->step==3&&(mode==AX||mode==AY)&&(store(op)||rmw(op)||((c->base^c->address)&0xff00u))){c->step=4;access(c,op);}
        else operand(c,op,v);break;
    case IX:
        switch(c->step++) {
        case 1:c->lo=v;++c->pc;read_at(c,v);break;
        case 2:c->lo=(uint8_t)(c->lo+c->x);read_at(c,c->lo);break;
        case 3:c->address=v;read_at(c,(uint8_t)(c->lo+1));break;
        case 4:c->address|=(uint16_t)v<<8;access(c,op);break;
        default:operand(c,op,v);break;
        }break;
    case IY:
        switch(c->step++) {
        case 1:c->lo=v;++c->pc;read_at(c,v);break;
        case 2:c->address=v;read_at(c,(uint8_t)(c->lo+1));break;
        case 3:c->base=(uint16_t)(c->address|v<<8);c->address=(uint16_t)(c->base+c->y);read_at(c,(uint16_t)((c->base&0xff00u)|(c->address&255u)));break;
        case 4:if(store(op)||rmw(op)||((c->base^c->address)&0xff00u)){access(c,op);break;} /* fall through */
        default:operand(c,op,v);break;
        }break;
    default:break;
    }
}
void rc_mos6510_reset(rc_mos6510 *c) {
    c->resetting=1;c->jammed=0;c->nmi_pending=c->irq_pending=0;read_at(c,c->pc);
}
void rc_mos6510_input(rc_mos6510 *c,int pin,int level) {
    level=level!=0;
    if(pin==RC_CPU_IRQ)c->irq=(uint8_t)level;
    else if(pin==RC_CPU_NMI){if(level&&!c->nmi)c->nmi_pending=1;c->nmi=(uint8_t)level;}
    else if(pin==RC_CPU_RESET&&level)rc_mos6510_reset(c);
    else if(pin==RC_CPU_SO){if(level&&!c->so)c->p|=RC_P_V;c->so=(uint8_t)level;}
}
uint8_t rc_mos6510_port_read(const rc_mos6510 *c,unsigned reg) {return reg&1u?(uint8_t)((c->port&c->ddr)|(c->port_input&~c->ddr)):c->ddr;}
void rc_mos6510_port_write(rc_mos6510 *c,unsigned reg,uint8_t v) {
    if(reg&1u)c->port=v;else c->ddr=v;
    if(c->dev.host&&c->dev.host->line)for(unsigned i=0;i<6;++i)rc_dev_line(&c->dev,RC_CPU_PORT0+(int)i,((c->port|~c->ddr)>>i)&1u);
}
void rc_mos6510_port_input(rc_mos6510 *c,uint8_t pins) {c->port_input=pins;}
static void dev_reset(void *self){rc_mos6510_reset(self);}
static void dev_input(void *self,int pin,int level){rc_mos6510_input(self,pin,level);}
static uint8_t dev_read(void *self,unsigned space,uint32_t off){(void)space;return rc_mos6510_port_read(self,off);}
static void dev_write(void *self,unsigned space,uint32_t off,uint8_t v){(void)space;rc_mos6510_port_write(self,off,v);}
static void dev_save(const void *self,rc_state_writer *w){rc_mos6510_save(self,w);}
static int dev_load(void *self,rc_state_reader *r){return rc_mos6510_load(self,r);}
static const rc_dev_ops OPS={.read=dev_read,.write=dev_write,.reset=dev_reset,.input=dev_input,.save=dev_save,.load=dev_load};
void rc_mos6510_init(rc_mos6510 *c,const rc_hw_host *host,unsigned model) {
    memset(c,0,sizeof *c);c->dev=(rc_dev){&OPS,host,"6510/6502"};c->model=(uint8_t)model;c->p=RC_P_I|RC_P_U;c->port_input=0xff;rc_mos6510_reset(c);
}
/* Fixed-width fields, including the current bus access and every in-flight
 * addressing/interrupt latch, so a save can occur between any two cycles. */
void rc_mos6510_save(const rc_mos6510 *c,rc_state_writer *w) {
    rc_sw_u32(w,1);rc_sw_u64(w,c->cycles);rc_sw_u16(w,c->pc);rc_sw_u16(w,c->address);rc_sw_u16(w,c->base);rc_sw_u16(w,c->vector);
#define B(n) rc_sw_u8(w,c->n)
    B(a);B(x);B(y);B(s);B(p);B(opcode);B(step);B(lo);B(value);B(result);B(model);B(ddr);B(port);B(port_input);
    B(irq);B(nmi);B(nmi_pending);B(irq_pending);B(irq_poll);B(resetting);B(interrupt);B(jammed);B(so);
#undef B
    rc_sw_u16(w,c->pins.address);rc_sw_u8(w,c->pins.data);rc_sw_u8(w,c->pins.write);rc_sw_u8(w,c->pins.sync);
}
int rc_mos6510_load(rc_mos6510 *c,rc_state_reader *r) {
    if(r->end-r->at<48||rc_sr_u32(r)!=1)return 0;
    c->cycles=rc_sr_u64(r);c->pc=rc_sr_u16(r);c->address=rc_sr_u16(r);c->base=rc_sr_u16(r);c->vector=rc_sr_u16(r);
#define B(n) c->n=rc_sr_u8(r)
    B(a);B(x);B(y);B(s);B(p);B(opcode);B(step);B(lo);B(value);B(result);B(model);B(ddr);B(port);B(port_input);
    B(irq);B(nmi);B(nmi_pending);B(irq_pending);B(irq_poll);B(resetting);B(interrupt);B(jammed);B(so);
#undef B
    c->pins.address=rc_sr_u16(r);c->pins.data=rc_sr_u8(r);c->pins.write=rc_sr_u8(r);c->pins.sync=rc_sr_u8(r);
    return rc_sr_ok(r)&&c->model<=RC_MOS6510&&c->step<=21&&c->resetting<=7&&c->pins.write<=1&&c->pins.sync<=1;
}
