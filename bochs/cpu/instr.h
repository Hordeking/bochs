/////////////////////////////////////////////////////////////////////////
// $Id$
/////////////////////////////////////////////////////////////////////////
//
//   Copyright (c) 2008-2012 Stanislav Shwartsman
//          Written by Stanislav Shwartsman [sshwarts at sourceforge net]
//
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2 of the License, or (at your option) any later version.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public
//  License along with this library; if not, write to the Free Software
//  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA B 02110-1301 USA
//
/////////////////////////////////////////////////////////////////////////

#ifndef BX_INSTR_H
#define BX_INSTR_H

class bxInstruction_c;

typedef void BX_INSF_TYPE;

#if BX_SUPPORT_HANDLERS_CHAINING_SPEEDUPS

#define BX_SYNC_TIME_IF_SINGLE_PROCESSOR(allowed_delta) {                     \
  if (BX_SMP_PROCESSORS == 1) {                                               \
    Bit32u delta = (Bit32u)(BX_CPU_THIS_PTR icount - BX_CPU_THIS_PTR icount_last_sync); \
    if (delta >= allowed_delta) {                                             \
      BX_CPU_THIS_PTR sync_icount();                                          \
      BX_TICKN(delta);                                                        \
    }                                                                         \
  }                                                                           \
}

#define BX_COMMIT_INSTRUCTION(i) {                     \
  BX_CPU_THIS_PTR prev_rip = RIP; /* commit new RIP */ \
  BX_INSTR_AFTER_EXECUTION(BX_CPU_ID, (i));            \
  BX_CPU_THIS_PTR icount++;                            \
}

#define BX_EXECUTE_INSTRUCTION(i) {                    \
  BX_INSTR_BEFORE_EXECUTION(BX_CPU_ID, (i));           \
  RIP += (i)->ilen();                                  \
  return BX_CPU_CALL_METHOD(i->execute1, (i));         \
}

#define BX_NEXT_TRACE(i) {                             \
  BX_COMMIT_INSTRUCTION(i);                            \
  return;                                              \
}

#define BX_LINK_TRACE(i) {                             \
  BX_COMMIT_INSTRUCTION(i);                            \
  linkTrace(i);                                        \
  return;                                              \
}

#define BX_NEXT_INSTR(i) {                             \
  BX_COMMIT_INSTRUCTION(i);                            \
  if (BX_CPU_THIS_PTR async_event) return;             \
  ++i;                                                 \
  BX_EXECUTE_INSTRUCTION(i);                           \
}

#else // BX_SUPPORT_HANDLERS_CHAINING_SPEEDUPS

#define BX_NEXT_TRACE(i) { return; }
#define BX_NEXT_INSTR(i) { return; }
#define BX_LINK_TRACE(i) { return; }

#define BX_SYNC_TIME_IF_SINGLE_PROCESSOR(allowed_delta) \
  if (BX_SMP_PROCESSORS == 1) BX_TICK1()

#endif

// <TAG-TYPE-EXECUTEPTR-START>
#if BX_USE_CPU_SMF
typedef BX_INSF_TYPE (BX_CPP_AttrRegparmN(1) *BxExecutePtr_tR)(bxInstruction_c *);
typedef bx_address (BX_CPP_AttrRegparmN(1) *BxResolvePtr_tR)(bxInstruction_c *);
typedef void (BX_CPP_AttrRegparmN(1) *BxRepIterationPtr_tR)(bxInstruction_c *);
#else
typedef BX_INSF_TYPE (BX_CPU_C::*BxExecutePtr_tR)(bxInstruction_c *) BX_CPP_AttrRegparmN(1);
typedef bx_address (BX_CPU_C::*BxResolvePtr_tR)(bxInstruction_c *) BX_CPP_AttrRegparmN(1);
typedef void (BX_CPU_C::*BxRepIterationPtr_tR)(bxInstruction_c *) BX_CPP_AttrRegparmN(1);
#endif
// <TAG-TYPE-EXECUTEPTR-END>

extern bx_address bx_asize_mask[];

const char *get_bx_opcode_name(Bit16u ia_opcode);

// <TAG-CLASS-INSTRUCTION-START>
class bxInstruction_c {
public:
  // Function pointers; a function to resolve the modRM address
  // given the current state of the CPU and the instruction data,
  // and a function to execute the instruction after resolving
  // the memory address (if any).
  BxExecutePtr_tR execute1;

  union {
    BxExecutePtr_tR execute2;
    bxInstruction_c *next;
  } handlers;

  BxResolvePtr_tR ResolveModrm;

  struct {
    // 15...0 opcode
    Bit16u ia_opcode;

    //  7...4 (unused)
    //  3...0 ilen (0..15)
    Bit8u ilen;

    //  7...6 lockUsed, repUsed (0=none, 1=0xF0, 2=0xF2, 3=0xF3)
    //  5...5 extend8bit
    //  4...4 mod==c0 (modrm)
    //  3...3 os64
    //  2...2 os32
    //  1...1 as64
    //  0...0 as32
    Bit8u metaInfo1;
  } metaInfo;

#define BX_INSTR_METADATA_DST   0
#define BX_INSTR_METADATA_SRC1  1
#define BX_INSTR_METADATA_SRC2  2
#define BX_INSTR_METADATA_SRC3  3
#define BX_INSTR_METADATA_SEG   4
#define BX_INSTR_METADATA_BASE  5
#define BX_INSTR_METADATA_INDEX 6
#define BX_INSTR_METADATA_SCALE 7

  // using 5-bit field for registers (16 regs in 64-bit, RIP, NIL)
  Bit8u metaData[8];

  union {
    // Form (longest case): [opcode+modrm+sib/displacement32/immediate32]
    struct {
      union {
        Bit32u Id;
        Bit16u Iw[2];
        // use Ib[3] as AVX mask register
        // use Ib[2] as AVX attributes
        //     7..4 (unused)
        //     3..3 Broadcast/RC/SAE control (EVEX.b)
        //     2..2 Zeroing/Merging mask (EVEX.z)
        //     1..0 Round control
        // use Ib[1] as AVX VL
        Bit8u  Ib[4];
      };
      union {
        Bit16u displ16u; // for 16-bit modrm forms
        Bit32u displ32u; // for 32-bit modrm forms

        Bit32u Id2;
        Bit16u Iw2[2];
        Bit8u  Ib2[4];
      };
    } modRMForm;

#if BX_SUPPORT_X86_64
    struct {
      Bit64u   Iq;  // for MOV Rx,imm64
    } IqForm;
#endif
  };

#ifdef BX_INSTR_STORE_OPCODE_BYTES
  Bit8u opcode_bytes[16];

  BX_CPP_INLINE const Bit8u* get_opcode_bytes(void) const {
    return opcode_bytes;
  }

  BX_CPP_INLINE void set_opcode_bytes(const Bit8u *opcode) {
    memcpy(opcode_bytes, opcode, ilen());
  }
#endif

  BX_CPP_INLINE BxExecutePtr_tR execute2(void) const {
    return handlers.execute2;
  }

  BX_CPP_INLINE unsigned seg(void) const {
    return metaData[BX_INSTR_METADATA_SEG];
  }
  BX_CPP_INLINE void setSeg(unsigned val) {
    metaData[BX_INSTR_METADATA_SEG] = val;
  }

  BX_CPP_INLINE void setFoo(unsigned foo) {
    // none of x87 instructions has immediate
    modRMForm.Iw[0] = foo;
  }
  BX_CPP_INLINE unsigned foo() const {
    return modRMForm.Iw[0];
  }
  BX_CPP_INLINE unsigned b1() const {
    return modRMForm.Iw[0] >> 8;
  }

  BX_CPP_INLINE void setSibScale(unsigned scale) {
    metaData[BX_INSTR_METADATA_SCALE] = scale;
  }
  BX_CPP_INLINE unsigned sibScale() const {
    return metaData[BX_INSTR_METADATA_SCALE];
  }
  BX_CPP_INLINE void setSibIndex(unsigned index) {
    metaData[BX_INSTR_METADATA_INDEX] = index;
  }
  BX_CPP_INLINE unsigned sibIndex() const {
    return metaData[BX_INSTR_METADATA_INDEX];
  }
  BX_CPP_INLINE void setSibBase(unsigned base) {
    metaData[BX_INSTR_METADATA_BASE] = base;
  }
  BX_CPP_INLINE unsigned sibBase() const {
    return metaData[BX_INSTR_METADATA_BASE];
  }
  BX_CPP_INLINE Bit32s displ32s() const { return (Bit32s) modRMForm.displ32u; }
  BX_CPP_INLINE Bit16s displ16s() const { return (Bit16s) modRMForm.displ16u; }
  BX_CPP_INLINE Bit32u Id() const  { return modRMForm.Id; }
  BX_CPP_INLINE Bit16u Iw() const  { return modRMForm.Iw[0]; }
  BX_CPP_INLINE Bit8u  Ib() const  { return modRMForm.Ib[0]; }
  BX_CPP_INLINE Bit16u Id2() const { return modRMForm.Id2; }
  BX_CPP_INLINE Bit16u Iw2() const { return modRMForm.Iw2[0]; }
  BX_CPP_INLINE Bit8u  Ib2() const { return modRMForm.Ib2[0]; }
#if BX_SUPPORT_X86_64
  BX_CPP_INLINE Bit64u Iq() const  { return IqForm.Iq; }
#endif

  // Info in the metaInfo field.
  // Note: the 'L' at the end of certain flags, means the value returned
  // is for Logical comparisons, eg if (i->os32L() && i->as32L()).  If you
  // want a bx_bool value, use os32B() etc.  This makes for smaller
  // code, when a strict 0 or 1 is not necessary.
  BX_CPP_INLINE void init(unsigned os32, unsigned as32, unsigned os64, unsigned as64)
  {
    metaInfo.metaInfo1 = (os32<<2) | (os64<<3) | (as32<<0) | (as64<<1);
  }

  BX_CPP_INLINE unsigned os32L(void) const {
    return metaInfo.metaInfo1 & (1<<2);
  }
  BX_CPP_INLINE void setOs32B(unsigned bit) {
    metaInfo.metaInfo1 = (metaInfo.metaInfo1 & ~(1<<2)) | (bit<<2);
  }
  BX_CPP_INLINE void assertOs32(void) {
    metaInfo.metaInfo1 |= (1<<2);
  }

#if BX_SUPPORT_X86_64
  BX_CPP_INLINE unsigned os64L(void) const {
    return metaInfo.metaInfo1 & (1<<3);
  }
  BX_CPP_INLINE void assertOs64(void) {
    metaInfo.metaInfo1 |= (1<<3);
  }
#else
  BX_CPP_INLINE unsigned os64L(void) const { return 0; }
#endif
  BX_CPP_INLINE unsigned osize(void) const {
    return (metaInfo.metaInfo1 >> 2) & 0x3;
  }

  BX_CPP_INLINE unsigned as32L(void) const {
    return metaInfo.metaInfo1 & 0x1;
  }
  BX_CPP_INLINE void setAs32B(unsigned bit) {
    metaInfo.metaInfo1 = (metaInfo.metaInfo1 & ~0x1) | (bit);
  }

#if BX_SUPPORT_X86_64
  BX_CPP_INLINE unsigned as64L(void) const {
    return metaInfo.metaInfo1 & (1<<1);
  }
  BX_CPP_INLINE void clearAs64(void) {
    metaInfo.metaInfo1 &= ~(1<<1);
  }
#else
  BX_CPP_INLINE unsigned as64L(void) const { return 0; }
#endif

  BX_CPP_INLINE unsigned asize(void) const {
    return metaInfo.metaInfo1 & 0x3;
  }
  BX_CPP_INLINE bx_address asize_mask(void) const {
    return bx_asize_mask[asize()];
  }

#if BX_SUPPORT_X86_64
  BX_CPP_INLINE unsigned extend8bitL(void) const {
    return metaInfo.metaInfo1 & (1<<5);
  }
  BX_CPP_INLINE void assertExtend8bit(void) {
    metaInfo.metaInfo1 |= (1<<5);
  }
#endif

  BX_CPP_INLINE unsigned ilen(void) const {
    return metaInfo.ilen;
  }
  BX_CPP_INLINE void setILen(unsigned ilen) {
    metaInfo.ilen = ilen;
  }

  BX_CPP_INLINE unsigned getIaOpcode(void) const {
    return metaInfo.ia_opcode;
  }
  BX_CPP_INLINE void setIaOpcode(Bit16u op) {
    metaInfo.ia_opcode = op;
  }
  BX_CPP_INLINE const char* getIaOpcodeName(void) const {
    return get_bx_opcode_name(getIaOpcode());
  }
  BX_CPP_INLINE const char* getIaOpcodeNameShort(void) const {
    return get_bx_opcode_name(getIaOpcode()) + /*"BX_IA_"*/ 6;
  }

  BX_CPP_INLINE unsigned repUsedL(void) const {
    return metaInfo.metaInfo1 >> 7;
  }
  BX_CPP_INLINE unsigned lockRepUsedValue(void) const {
    return metaInfo.metaInfo1 >> 6;
  }
  BX_CPP_INLINE void setLockRepUsed(unsigned value) {
    metaInfo.metaInfo1 = (metaInfo.metaInfo1 & 0x3f) | (value << 6);
  }

  BX_CPP_INLINE unsigned getVL(void) const {
#if BX_SUPPORT_AVX
    return modRMForm.Ib[1];
#else
    return 0;
#endif
  }
  BX_CPP_INLINE void setVL(unsigned value) {
    modRMForm.Ib[1] = value;
  }

#if BX_SUPPORT_EVEX
  BX_CPP_INLINE void setOpmask(unsigned reg) {
    modRMForm.Ib[3] = reg;
  }
  BX_CPP_INLINE unsigned opmask(void) const {
    return modRMForm.Ib[3];
  }

  BX_CPP_INLINE void setEvexb(unsigned bit) {
    modRMForm.Ib[2] = (modRMForm.Ib[2] & ~(1<<3)) | (bit<<3);
  } 
  BX_CPP_INLINE unsigned getEvexb(void) const {
    return modRMForm.Ib[2] & (1 << 3);
  } 

  BX_CPP_INLINE void setZeroMasking(unsigned bit) {
    modRMForm.Ib[2] = (modRMForm.Ib[2] & ~(1<<2)) | (bit<<2);
  } 
  BX_CPP_INLINE unsigned isZeroMasking(void) const {
    return modRMForm.Ib[2] & (1 << 2);
  } 

  BX_CPP_INLINE void setRC(unsigned rc) {
    modRMForm.Ib[2] = (modRMForm.Ib[2] & ~0x3) | rc;
  } 
  BX_CPP_INLINE unsigned getRC(void) const {
    return modRMForm.Ib[2] & 0x3;
  } 
#endif

  BX_CPP_INLINE void setSrcReg(unsigned src, unsigned reg) {
    metaData[src] = reg;
  }
  BX_CPP_INLINE unsigned getSrcReg(unsigned src) const {
    return metaData[src];
  }

  BX_CPP_INLINE unsigned dst() const {
    return metaData[BX_INSTR_METADATA_DST];
  }
 
  BX_CPP_INLINE unsigned src1() const {
    return metaData[BX_INSTR_METADATA_SRC1];
  }
  BX_CPP_INLINE unsigned src2() const {
    return metaData[BX_INSTR_METADATA_SRC2];
  }
  BX_CPP_INLINE unsigned src3() const {
    return metaData[BX_INSTR_METADATA_SRC3];
  }

  BX_CPP_INLINE unsigned src() const { return src1(); }

  BX_CPP_INLINE unsigned modC0() const
  {
    // This is a cheaper way to test for modRM instructions where
    // the mod field is 0xc0.  FetchDecode flags this condition since
    // it is quite common to be tested for.
    return metaInfo.metaInfo1 & (1<<4);
  }
  BX_CPP_INLINE void assertModC0()
  {
    metaInfo.metaInfo1 |= (1<<4);
  }

#if BX_SUPPORT_HANDLERS_CHAINING_SPEEDUPS
  BX_CPP_INLINE bxInstruction_c* getNextTrace(Bit32u currTraceLinkTimeStamp) {
    if (currTraceLinkTimeStamp > modRMForm.Id2) handlers.next = NULL;
    return handlers.next;
  }
  BX_CPP_INLINE void setNextTrace(bxInstruction_c* iptr, Bit32u traceLinkTimeStamp) {
    handlers.next = iptr;
    modRMForm.Id2 = traceLinkTimeStamp;
  }
#endif

};
// <TAG-CLASS-INSTRUCTION-END>

enum {
#define bx_define_opcode(a, b, c, d, s1, s2, s3, s4, e) a,
#include "ia_opcodes.h"
   BX_IA_LAST
};
#undef  bx_define_opcode

#endif
