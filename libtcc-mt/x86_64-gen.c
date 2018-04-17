/*
 *  x86-64 code generator for TCC
 *
 *  Copyright (c) 2008 Shinichiro Hamaji
 *
 *  Based on i386-gen.c by Fabrice Bellard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/******************************************************/
#include "tcc.h"
#include <assert.h>

ST_DATA const int reg_classes[NB_REGS] = {
    /* eax */ RC_INT | RC_RAX,
    /* ecx */ RC_INT | RC_RCX,
    /* edx */ RC_INT | RC_RDX,
    0,
    0,
    0,
    0,
    0,
    RC_R8,
    RC_R9,
    RC_R10,
    RC_R11,
    0,
    0,
    0,
    0,
    /* xmm0 */ RC_FLOAT | RC_XMM0,
    /* xmm1 */ RC_FLOAT | RC_XMM1,
    /* xmm2 */ RC_FLOAT | RC_XMM2,
    /* xmm3 */ RC_FLOAT | RC_XMM3,
    /* xmm4 */ RC_FLOAT | RC_XMM4,
    /* xmm5 */ RC_FLOAT | RC_XMM5,
    /* xmm6 an xmm7 are included so gv() can be used on them,
       but they are not tagged with RC_FLOAT because they are
       callee saved on Windows */
    RC_XMM6,
    RC_XMM7,
    /* st0 */ RC_ST0
};

static unsigned long func_sub_sp_offset;
static int func_ret_sub;

/* XXX: make it faster ? */
ST_FUNC void g(TCCState *s1, int c)
{
    int ind1;
    if (s1->nocode_wanted)
        return;
    ind1 = s1->ind + 1;
    if (ind1 > s1->cur_text_section->data_allocated)
        section_realloc(s1, s1->cur_text_section, ind1);
    s1->cur_text_section->data[s1->ind] = c;
    s1->ind = ind1;
}

ST_FUNC void o(TCCState *s1, unsigned int c)
{
    while (c) {
        g(s1, c);
        c = c >> 8;
    }
}

ST_FUNC void gen_le16(TCCState *s1, int v)
{
    g(s1, v);
    g(s1, v >> 8);
}

ST_FUNC void gen_le32(TCCState *s1, int c)
{
    g(s1, c);
    g(s1, c >> 8);
    g(s1, c >> 16);
    g(s1, c >> 24);
}

ST_FUNC void gen_le64(TCCState *s1, int64_t c)
{
    g(s1, c);
    g(s1, c >> 8);
    g(s1, c >> 16);
    g(s1, c >> 24);
    g(s1, c >> 32);
    g(s1, c >> 40);
    g(s1, c >> 48);
    g(s1, c >> 56);
}

static void orex(TCCState *s1, int ll, int r, int r2, int b)
{
    if ((r & VT_VALMASK) >= VT_CONST)
        r = 0;
    if ((r2 & VT_VALMASK) >= VT_CONST)
        r2 = 0;
    if (ll || REX_BASE(r) || REX_BASE(r2))
        o(s1, 0x40 | REX_BASE(r) | (REX_BASE(r2) << 2) | (ll << 3));
    o(s1, b);
}

/* output a symbol and patch all calls to it */
ST_FUNC void gsym_addr(TCCState *s1, int t, int a)
{
    while (t) {
        unsigned char *ptr = s1->cur_text_section->data + t;
        uint32_t n = read32le(ptr); /* next value */
        write32le(ptr, a - t - 4);
        t = n;
    }
}

void gsym(TCCState *s1, int t)
{
    gsym_addr(s1, t, s1->ind);
}


static int is64_type(int t)
{
    return ((t & VT_BTYPE) == VT_PTR ||
            (t & VT_BTYPE) == VT_FUNC ||
            (t & VT_BTYPE) == VT_LLONG);
}

/* instruction + 4 bytes data. Return the address of the data */
static int oad(TCCState *s1, int c, int s)
{
    int t;
    if (s1->nocode_wanted)
        return s;
    o(s1, c);
    t = s1->ind;
    gen_le32(s1, s);
    return t;
}

/* generate jmp to a label */
#define gjmp2(s1,instr,lbl) oad(s1,instr,lbl)

ST_FUNC void gen_addr32(TCCState *s1, int r, Sym *sym, int c)
{
    if (r & VT_SYM)
        greloca(s1, s1->cur_text_section, sym, s1->ind, R_X86_64_32S, c), c=0;
    gen_le32(s1, c);
}

/* output constant with relocation if 'r & VT_SYM' is true */
ST_FUNC void gen_addr64(TCCState *s1, int r, Sym *sym, int64_t c)
{
    if (r & VT_SYM)
        greloca(s1, s1->cur_text_section, sym, s1->ind, R_X86_64_64, c), c=0;
    gen_le64(s1, c);
}

/* output constant with relocation if 'r & VT_SYM' is true */
ST_FUNC void gen_addrpc32(TCCState *s1, int r, Sym *sym, int c)
{
    if (r & VT_SYM)
        greloca(s1, s1->cur_text_section, sym, s1->ind, R_X86_64_PC32, c-4), c=4;
    gen_le32(s1, c-4);
}

/* output got address with relocation */
static void gen_gotpcrel(TCCState *s1, int r, Sym *sym, int c)
{
#ifdef TCC_TARGET_PE
    tcc_error(s1, "internal error: no GOT on PE: %s %x %x | %02x %02x %02x\n",
        get_tok_str(sym->v, NULL), c, r,
        s1->cur_text_section->data[s1->ind-3],
        s1->cur_text_section->data[s1->ind-2],
        s1->cur_text_section->data[s1->ind-1]
        );
#endif
    greloca(s1, s1->cur_text_section, sym, s1->ind, R_X86_64_GOTPCREL, -4);
    gen_le32(s1, 0);
    if (c) {
        /* we use add c, %xxx for displacement */
        orex(s1, 1, r, 0, 0x81);
        o(s1, 0xc0 + REG_VALUE(r));
        gen_le32(s1, c);
    }
}

static void gen_modrm_impl(TCCState *s1, int op_reg, int r, Sym *sym, int c, int is_got)
{
    op_reg = REG_VALUE(op_reg) << 3;
    if ((r & VT_VALMASK) == VT_CONST) {
        /* constant memory reference */
	if (!(r & VT_SYM)) {
	    /* Absolute memory reference */
	    o(s1, 0x04 | op_reg); /* [sib] | destreg */
	    oad(s1, 0x25, c);     /* disp32 */
	} else {
	    o(s1, 0x05 | op_reg); /* (%rip)+disp32 | destreg */
	    if (is_got) {
		gen_gotpcrel(s1, r, sym, c);
	    } else {
		gen_addrpc32(s1, r, sym, c);
	    }
	}
    } else if ((r & VT_VALMASK) == VT_LOCAL) {
        /* currently, we use only ebp as base */
        if (c == (char)c) {
            /* short reference */
            o(s1, 0x45 | op_reg);
            g(s1, c);
        } else {
            oad(s1, 0x85 | op_reg, c);
        }
    } else if ((r & VT_VALMASK) >= TREG_MEM) {
        if (c) {
            g(s1, 0x80 | op_reg | REG_VALUE(r));
            gen_le32(s1, c);
        } else {
            g(s1, 0x00 | op_reg | REG_VALUE(r));
        }
    } else {
        g(s1, 0x00 | op_reg | REG_VALUE(r));
    }
}

/* generate a modrm reference. 'op_reg' contains the additional 3
   opcode bits */
static void gen_modrm(TCCState *s1, int op_reg, int r, Sym *sym, int c)
{
    gen_modrm_impl(s1, op_reg, r, sym, c, 0);
}

/* generate a modrm reference. 'op_reg' contains the additional 3
   opcode bits */
static void gen_modrm64(TCCState *s1, int opcode, int op_reg, int r, Sym *sym, int c)
{
    int is_got;
    is_got = (op_reg & TREG_MEM) && !(sym->type.t & VT_STATIC);
    orex(s1, 1, r, op_reg, opcode);
    gen_modrm_impl(s1, op_reg, r, sym, c, is_got);
}


/* load 'r' from value 'sv' */
void load(TCCState *s1, int r, SValue *sv)
{
    int v, t, ft, fc, fr;
    SValue v1;

#ifdef TCC_TARGET_PE
    SValue v2;
    sv = pe_getimport(s1, sv, &v2);
#endif

    fr = sv->r;
    ft = sv->type.t & ~VT_DEFSIGN;
    fc = sv->c.i;
    if (fc != sv->c.i && (fr & VT_SYM))
      tcc_error(s1, "64 bit addend in load");

    ft &= ~(VT_VOLATILE | VT_CONSTANT);

#ifndef TCC_TARGET_PE
    /* we use indirect access via got */
    if ((fr & VT_VALMASK) == VT_CONST && (fr & VT_SYM) &&
        (fr & VT_LVAL) && !(sv->sym->type.t & VT_STATIC)) {
        /* use the result register as a temporal register */
        int tr = r | TREG_MEM;
        if (is_float(ft)) {
            /* we cannot use float registers as a temporal register */
            tr = get_reg(s1, RC_INT) | TREG_MEM;
        }
        gen_modrm64(s1, 0x8b, tr, fr, sv->sym, 0);

        /* load from the temporal register */
        fr = tr | VT_LVAL;
    }
#endif

    v = fr & VT_VALMASK;
    if (fr & VT_LVAL) {
        int b, ll;
        if (v == VT_LLOCAL) {
            v1.type.t = VT_PTR;
            v1.r = VT_LOCAL | VT_LVAL;
            v1.c.i = fc;
            fr = r;
            if (!(reg_classes[fr] & (RC_INT|RC_R11)))
                fr = get_reg(s1, RC_INT);
            load(s1, fr, &v1);
        }
	if (fc != sv->c.i) {
	    /* If the addends doesn't fit into a 32bit signed
	       we must use a 64bit move.  We've checked above
	       that this doesn't have a sym associated.  */
	    v1.type.t = VT_LLONG;
	    v1.r = VT_CONST;
	    v1.c.i = sv->c.i;
	    fr = r;
	    if (!(reg_classes[fr] & (RC_INT|RC_R11)))
	        fr = get_reg(s1, RC_INT);
	    load(s1, fr, &v1);
	    fc = 0;
	}
        ll = 0;
	/* Like GCC we can load from small enough properly sized
	   structs and unions as well.
	   XXX maybe move to generic operand handling, but should
	   occur only with asm, so tccasm.c might also be a better place */
	if ((ft & VT_BTYPE) == VT_STRUCT) {
	    int align;
	    switch (type_size(&sv->type, &align)) {
		case 1: ft = VT_BYTE; break;
		case 2: ft = VT_SHORT; break;
		case 4: ft = VT_INT; break;
		case 8: ft = VT_LLONG; break;
		default:
		    tcc_error(s1, "invalid aggregate type for register load");
		    break;
	    }
	}
        if ((ft & VT_BTYPE) == VT_FLOAT) {
            b = 0x6e0f66;
            r = REG_VALUE(r); /* movd */
        } else if ((ft & VT_BTYPE) == VT_DOUBLE) {
            b = 0x7e0ff3; /* movq */
            r = REG_VALUE(r);
        } else if ((ft & VT_BTYPE) == VT_LDOUBLE) {
            b = 0xdb, r = 5; /* fldt */
        } else if ((ft & VT_TYPE) == VT_BYTE || (ft & VT_TYPE) == VT_BOOL) {
            b = 0xbe0f;   /* movsbl */
        } else if ((ft & VT_TYPE) == (VT_BYTE | VT_UNSIGNED)) {
            b = 0xb60f;   /* movzbl */
        } else if ((ft & VT_TYPE) == VT_SHORT) {
            b = 0xbf0f;   /* movswl */
        } else if ((ft & VT_TYPE) == (VT_SHORT | VT_UNSIGNED)) {
            b = 0xb70f;   /* movzwl */
        } else {
            assert(((ft & VT_BTYPE) == VT_INT)
                   || ((ft & VT_BTYPE) == VT_LLONG)
                   || ((ft & VT_BTYPE) == VT_PTR)
                   || ((ft & VT_BTYPE) == VT_FUNC)
                );
            ll = is64_type(ft);
            b = 0x8b;
        }
        if (ll) {
            gen_modrm64(s1, b, r, fr, sv->sym, fc);
        } else {
            orex(s1, ll, fr, r, b);
            gen_modrm(s1, r, fr, sv->sym, fc);
        }
    } else {
        if (v == VT_CONST) {
            if (fr & VT_SYM) {
#ifdef TCC_TARGET_PE
                orex(s1, 1,0,r,0x8d);
                o(s1, 0x05 + REG_VALUE(r) * 8); /* lea xx(%rip), r */
                gen_addrpc32(s1, fr, sv->sym, fc);
#else
                if (sv->sym->type.t & VT_STATIC) {
                    orex(s1, 1,0,r,0x8d);
                    o(s1, 0x05 + REG_VALUE(r) * 8); /* lea xx(%rip), r */
                    gen_addrpc32(s1, fr, sv->sym, fc);
                } else {
                    orex(s1, 1,0,r,0x8b);
                    o(s1, 0x05 + REG_VALUE(r) * 8); /* mov xx(%rip), r */
                    gen_gotpcrel(s1, r, sv->sym, fc);
                }
#endif
            } else if (is64_type(ft)) {
                orex(s1, 1,r,0, 0xb8 + REG_VALUE(r)); /* mov $xx, r */
                gen_le64(s1, sv->c.i);
            } else {
                orex(s1, 0,r,0, 0xb8 + REG_VALUE(r)); /* mov $xx, r */
                gen_le32(s1, fc);
            }
        } else if (v == VT_LOCAL) {
            orex(s1, 1,0,r,0x8d); /* lea xxx(%ebp), r */
            gen_modrm(s1, r, VT_LOCAL, sv->sym, fc);
        } else if (v == VT_CMP) {
            orex(s1, 0,r,0,0);
	    if ((fc & ~0x100) != TOK_NE)
              oad(s1, 0xb8 + REG_VALUE(r), 0); /* mov $0, r */
	    else
              oad(s1, 0xb8 + REG_VALUE(r), 1); /* mov $1, r */
	    if (fc & 0x100)
	      {
	        /* This was a float compare.  If the parity bit is
		   set the result was unordered, meaning false for everything
		   except TOK_NE, and true for TOK_NE.  */
		fc &= ~0x100;
		o(s1, 0x037a + (REX_BASE(r) << 8));
	      }
            orex(s1, 0,r,0, 0x0f); /* setxx %br */
            o(s1, fc);
            o(s1, 0xc0 + REG_VALUE(r));
        } else if (v == VT_JMP || v == VT_JMPI) {
            t = v & 1;
            orex(s1, 0,r,0,0);
            oad(s1, 0xb8 + REG_VALUE(r), t); /* mov $1, r */
            o(s1, 0x05eb + (REX_BASE(r) << 8)); /* jmp after */
            gsym(s1, fc);
            orex(s1, 0,r,0,0);
            oad(s1, 0xb8 + REG_VALUE(r), t ^ 1); /* mov $0, r */
        } else if (v != r) {
            if ((r >= TREG_XMM0) && (r <= TREG_XMM7)) {
                if (v == TREG_ST0) {
                    /* gen_cvt_ftof(VT_DOUBLE); */
                    o(s1, 0xf0245cdd); /* fstpl -0x10(%rsp) */
                    /* movsd -0x10(%rsp),%xmmN */
                    o(s1, 0x100ff2);
                    o(s1, 0x44 + REG_VALUE(r)*8); /* %xmmN */
                    o(s1, 0xf024);
                } else {
                    assert((v >= TREG_XMM0) && (v <= TREG_XMM7));
                    if ((ft & VT_BTYPE) == VT_FLOAT) {
                        o(s1, 0x100ff3);
                    } else {
                        assert((ft & VT_BTYPE) == VT_DOUBLE);
                        o(s1, 0x100ff2);
                    }
                    o(s1, 0xc0 + REG_VALUE(v) + REG_VALUE(r)*8);
                }
            } else if (r == TREG_ST0) {
                assert((v >= TREG_XMM0) && (v <= TREG_XMM7));
                /* gen_cvt_ftof(VT_LDOUBLE); */
                /* movsd %xmmN,-0x10(%rsp) */
                o(s1, 0x110ff2);
                o(s1, 0x44 + REG_VALUE(r)*8); /* %xmmN */
                o(s1, 0xf024);
                o(s1, 0xf02444dd); /* fldl -0x10(%rsp) */
            } else {
                orex(s1, 1,r,v, 0x89);
                o(s1, 0xc0 + REG_VALUE(r) + REG_VALUE(v) * 8); /* mov v, r */
            }
        }
    }
}

/* store register 'r' in lvalue 'v' */
void store(TCCState *s1, int r, SValue *v)
{
    int fr, bt, ft, fc;
    int op64 = 0;
    /* store the REX prefix in this variable when PIC is enabled */
    int pic = 0;

#ifdef TCC_TARGET_PE
    SValue v2;
    v = pe_getimport(s1, v, &v2);
#endif

    fr = v->r & VT_VALMASK;
    ft = v->type.t;
    fc = v->c.i;
    if (fc != v->c.i && (fr & VT_SYM))
      tcc_error(s1, "64 bit addend in store");
    ft &= ~(VT_VOLATILE | VT_CONSTANT);
    bt = ft & VT_BTYPE;

#ifndef TCC_TARGET_PE
    /* we need to access the variable via got */
    if (fr == VT_CONST && (v->r & VT_SYM)) {
        /* mov xx(%rip), %r11 */
        o(s1, 0x1d8b4c);
        gen_gotpcrel(s1, TREG_R11, v->sym, v->c.i);
        pic = is64_type(bt) ? 0x49 : 0x41;
    }
#endif

    /* XXX: incorrect if float reg to reg */
    if (bt == VT_FLOAT) {
        o(s1, 0x66);
        o(s1, pic);
        o(s1, 0x7e0f); /* movd */
        r = REG_VALUE(r);
    } else if (bt == VT_DOUBLE) {
        o(s1, 0x66);
        o(s1, pic);
        o(s1, 0xd60f); /* movq */
        r = REG_VALUE(r);
    } else if (bt == VT_LDOUBLE) {
        o(s1, 0xc0d9); /* fld %st(0) */
        o(s1, pic);
        o(s1, 0xdb); /* fstpt */
        r = 7;
    } else {
        if (bt == VT_SHORT)
            o(s1, 0x66);
        o(s1, pic);
        if (bt == VT_BYTE || bt == VT_BOOL)
            orex(s1, 0, 0, r, 0x88);
        else if (is64_type(bt))
            op64 = 0x89;
        else
            orex(s1, 0, 0, r, 0x89);
    }
    if (pic) {
        /* xxx r, (%r11) where xxx is mov, movq, fld, or etc */
        if (op64)
            o(s1, op64);
        o(s1, 3 + (r << 3));
    } else if (op64) {
        if (fr == VT_CONST || fr == VT_LOCAL || (v->r & VT_LVAL)) {
            gen_modrm64(s1, op64, r, v->r, v->sym, fc);
        } else if (fr != r) {
            /* XXX: don't we really come here? */
            abort();
            o(s1, 0xc0 + fr + r * 8); /* mov r, fr */
        }
    } else {
        if (fr == VT_CONST || fr == VT_LOCAL || (v->r & VT_LVAL)) {
            gen_modrm(s1, r, v->r, v->sym, fc);
        } else if (fr != r) {
            /* XXX: don't we really come here? */
            abort();
            o(s1, 0xc0 + fr + r * 8); /* mov r, fr */
        }
    }
}

/* 'is_jmp' is '1' if it is a jump */
static void gcall_or_jmp(TCCState *s1, int is_jmp)
{
    int r;
    if ((s1->vtop->r & (VT_VALMASK | VT_LVAL)) == VT_CONST &&
	((s1->vtop->r & VT_SYM) || (s1->vtop->c.i-4) == (int)(s1->vtop->c.i-4))) {
        /* constant case */
        if (s1->vtop->r & VT_SYM) {
            /* relocation case */
#ifdef TCC_TARGET_PE
            greloca(s1, s1->cur_text_section, s1->vtop->sym, s1->ind + 1, R_X86_64_PC32, (int)(s1->vtop->c.i-4));
#else
            greloca(s1, s1->cur_text_section, s1->vtop->sym, s1->ind + 1, R_X86_64_PLT32, (int)(s1->vtop->c.i-4));
#endif
        } else {
            /* put an empty PC32 relocation */
            put_elf_reloca(s1, s1->symtab_section, s1->cur_text_section,
                           s1->ind + 1, R_X86_64_PC32, 0, (int)(s1->vtop->c.i-4));
        }
        oad(s1, 0xe8 + is_jmp, 0); /* call/jmp im */
    } else {
        /* otherwise, indirect call */
        r = TREG_R11;
        load(s1, r, s1->vtop);
        o(s1, 0x41); /* REX */
        o(s1, 0xff); /* call/jmp *r */
        o(s1, 0xd0 + REG_VALUE(r) + (is_jmp << 4));
    }
}

#if defined(CONFIG_TCC_BCHECK)
#ifndef TCC_TARGET_PE
static addr_t func_bound_offset;
static unsigned long func_bound_ind;
#endif

static void gen_static_call(TCCState *s1, int v)
{
    Sym *sym = external_global_sym(s1, v, &s1->func_old_type, 0);
    oad(s1, 0xe8, 0);
    greloca(s1, s1->cur_text_section, sym, s1->ind-4, R_X86_64_PC32, -4);
}

/* generate a bounded pointer addition */
ST_FUNC void gen_bounded_ptr_add(TCCState *s1)
{
    /* save all temporary registers */
    save_regs(s1, 0);

    /* prepare fast x86_64 function call */
    gv(s1, RC_RAX);
    o(s1, 0xc68948); // mov  %rax,%rsi ## second arg in %rsi, this must be size
    s1->vtop--;

    gv(s1, RC_RAX);
    o(s1, 0xc78948); // mov  %rax,%rdi ## first arg in %rdi, this must be ptr
    s1->vtop--;

    /* do a fast function call */
    gen_static_call(s1, TOK___bound_ptr_add);

    /* returned pointer is in rax */
    s1->vtop++;
    s1->vtop->r = TREG_RAX | VT_BOUNDED;


    /* relocation offset of the bounding function call point */
    s1->vtop->c.i = (s1->cur_text_section->reloc->data_offset - sizeof(ElfW(Rela)));
}

/* patch pointer addition in s1->vtop so that pointer dereferencing is
   also tested */
ST_FUNC void gen_bounded_ptr_deref(TCCState *s1)
{
    addr_t func;
    int size, align;
    ElfW(Rela) *rel;
    Sym *sym;

    size = 0;
    /* XXX: put that code in generic part of tcc */
    if (!is_float(s1->vtop->type.t)) {
        if (s1->vtop->r & VT_LVAL_BYTE)
            size = 1;
        else if (s1->vtop->r & VT_LVAL_SHORT)
            size = 2;
    }
    if (!size)
    size = type_size(&s1->vtop->type, &align);
    switch(size) {
    case  1: func = TOK___bound_ptr_indir1; break;
    case  2: func = TOK___bound_ptr_indir2; break;
    case  4: func = TOK___bound_ptr_indir4; break;
    case  8: func = TOK___bound_ptr_indir8; break;
    case 12: func = TOK___bound_ptr_indir12; break;
    case 16: func = TOK___bound_ptr_indir16; break;
    default:
        tcc_error(s1, "unhandled size when dereferencing bounded pointer");
        func = 0;
        break;
    }

    sym = external_global_sym(s1, func, &s1->func_old_type, 0);
    if (!sym->c)
        put_extern_sym(s1, sym, NULL, 0, 0);

    /* patch relocation */
    /* XXX: find a better solution ? */

    rel = (ElfW(Rela) *)(s1->cur_text_section->reloc->data + s1->vtop->c.i);
    rel->r_info = ELF64_R_INFO(sym->c, ELF64_R_TYPE(rel->r_info));
}
#endif

#ifdef TCC_TARGET_PE

#define REGN 4
static const uint8_t arg_regs[REGN] = {
    TREG_RCX, TREG_RDX, TREG_R8, TREG_R9
};

/* Prepare arguments in R10 and R11 rather than RCX and RDX
   because gv() will not ever use these */
static int arg_prepare_reg(int idx) {
  if (idx == 0 || idx == 1)
      /* idx=0: r10, idx=1: r11 */
      return idx + 10;
  else
      return arg_regs[idx];
}

static int func_scratch, func_alloca;

/* Generate function call. The function address is pushed first, then
   all the parameters in call order. This functions pops all the
   parameters and the function address. */

static void gen_offs_sp(TCCState *s1, int b, int r, int d)
{
    orex(s1, 1,0,r & 0x100 ? 0 : r, b);
    if (d == (char)d) {
        o(s1, 0x2444 | (REG_VALUE(r) << 3));
        g(s1, d);
    } else {
        o(s1, 0x2484 | (REG_VALUE(r) << 3));
        gen_le32(s1, d);
    }
}

static int using_regs(int size)
{
    return !(size > 8 || (size & (size - 1)));
}

/* Return the number of registers needed to return the struct, or 0 if
   returning via struct pointer. */
ST_FUNC int gfunc_sret(CType *vt, int variadic, CType *ret, int *ret_align, int *regsize)
{
    int size, align;
    *ret_align = 1; // Never have to re-align return values for x86-64
    *regsize = 8;
    size = type_size(vt, &align);
    if (!using_regs(size))
        return 0;
    if (size == 8)
        ret->t = VT_LLONG;
    else if (size == 4)
        ret->t = VT_INT;
    else if (size == 2)
        ret->t = VT_SHORT;
    else
        ret->t = VT_BYTE;
    ret->ref = NULL;
    return 1;
}

static int is_sse_float(int t) {
    int bt;
    bt = t & VT_BTYPE;
    return bt == VT_DOUBLE || bt == VT_FLOAT;
}

static int gfunc_arg_size(CType *type) {
    int align;
    if (type->t & (VT_ARRAY|VT_BITFIELD))
        return 8;
    return type_size(type, &align);
}

void gfunc_call(TCCState *s1, int nb_args)
{
    int size, r, args_size, i, d, bt, struct_size;
    int arg;

    args_size = (nb_args < REGN ? REGN : nb_args) * PTR_SIZE;
    arg = nb_args;

    /* for struct arguments, we need to call memcpy and the function
       call breaks register passing arguments we are preparing.
       So, we process arguments which will be passed by stack first. */
    struct_size = args_size;
    for(i = 0; i < nb_args; i++) {
        SValue *sv;

        --arg;
        sv = &s1->vtop[-i];
        bt = (sv->type.t & VT_BTYPE);
        size = gfunc_arg_size(&sv->type);

        if (using_regs(size))
            continue; /* arguments smaller than 8 bytes passed in registers or on stack */

        if (bt == VT_STRUCT) {
            /* align to stack align size */
            size = (size + 15) & ~15;
            /* generate structure store */
            r = get_reg(s1, RC_INT);
            gen_offs_sp(s1, 0x8d, r, struct_size);
            struct_size += size;

            /* generate memcpy call */
            vset(s1, &sv->type, r | VT_LVAL, 0);
            vset(s1, sv);
            vstore(s1);
            --s1->vtop;
        } else if (bt == VT_LDOUBLE) {
            gv(s1, RC_ST0);
            gen_offs_sp(s1, 0xdb, 0x107, struct_size);
            struct_size += 16;
        }
    }

    if (func_scratch < struct_size)
        func_scratch = struct_size;

    arg = nb_args;
    struct_size = args_size;

    for(i = 0; i < nb_args; i++) {
        --arg;
        bt = (s1->vtop->type.t & VT_BTYPE);

        size = gfunc_arg_size(&s1->vtop->type);
        if (!using_regs(size)) {
            /* align to stack align size */
            size = (size + 15) & ~15;
            if (arg >= REGN) {
                d = get_reg(s1, RC_INT);
                gen_offs_sp(s1, 0x8d, d, struct_size);
                gen_offs_sp(s1, 0x89, d, arg*8);
            } else {
                d = arg_prepare_reg(arg);
                gen_offs_sp(s1, 0x8d, d, struct_size);
            }
            struct_size += size;
        } else {
            if (is_sse_float(s1->vtop->type.t)) {
		if (tcc_state->nosse)
		  tcc_error(s1, "SSE disabled");
                gv(s1, RC_XMM0); /* only use one float register */
                if (arg >= REGN) {
                    /* movq %xmm0, j*8(%rsp) */
                    gen_offs_sp(s1, 0xd60f66, 0x100, arg*8);
                } else {
                    /* movaps %xmm0, %xmmN */
                    o(s1, 0x280f);
                    o(s1, 0xc0 + (arg << 3));
                    d = arg_prepare_reg(arg);
                    /* mov %xmm0, %rxx */
                    o(s1, 0x66);
                    orex(s1, 1,d,0, 0x7e0f);
                    o(s1, 0xc0 + REG_VALUE(d));
                }
            } else {
                if (bt == VT_STRUCT) {
                    s1->vtop->type.ref = NULL;
                    s1->vtop->type.t = size > 4 ? VT_LLONG : size > 2 ? VT_INT
                        : size > 1 ? VT_SHORT : VT_BYTE;
                }

                r = gv(s1, RC_INT);
                if (arg >= REGN) {
                    gen_offs_sp(s1, 0x89, r, arg*8);
                } else {
                    d = arg_prepare_reg(arg);
                    orex(s1, 1,d,r,0x89); /* mov */
                    o(s1, 0xc0 + REG_VALUE(r) * 8 + REG_VALUE(d));
                }
            }
        }
        s1->vtop--;
    }
    save_regs(s1, 0);

    /* Copy R10 and R11 into RCX and RDX, respectively */
    if (nb_args > 0) {
        o(s1, 0xd1894c); /* mov %r10, %rcx */
        if (nb_args > 1) {
            o(s1, 0xda894c); /* mov %r11, %rdx */
        }
    }

    gcall_or_jmp(s1, 0);

    if ((s1->vtop->r & VT_SYM) && s1->vtop->sym->v == TOK_alloca) {
        /* need to add the "func_scratch" area after alloca */
        o(s1, 0x0548), gen_le32(s1, func_alloca), func_alloca = ind - 4;
    }

    /* other compilers don't clear the upper bits when returning char/short */
    bt = s1->vtop->type.ref->type.t & (VT_BTYPE | VT_UNSIGNED);
    if (bt == (VT_BYTE | VT_UNSIGNED))
        o(s1, 0xc0b60f);  /* movzbl %al, %eax */
    else if (bt == VT_BYTE)
        o(s1, 0xc0be0f); /* movsbl %al, %eax */
    else if (bt == VT_SHORT)
        o(s1, 0x98); /* cwtl */
    else if (bt == (VT_SHORT | VT_UNSIGNED))
        o(s1, 0xc0b70f);  /* movzbl %al, %eax */
#if 0 /* handled in gen_cast() */
    else if (bt == VT_INT)
        o(s1, 0x9848); /* cltq */
    else if (bt == (VT_INT | VT_UNSIGNED))
        o(s1, 0xc089); /* mov %eax,%eax */
#endif
    s1->vtop--;
}


#define FUNC_PROLOG_SIZE 11

/* generate function prolog of type 't' */
void gfunc_prolog(TCCState *s1, CType *func_type)
{
    int addr, reg_param_index, bt, size;
    Sym *sym;
    CType *type;

    func_ret_sub = 0;
    func_scratch = 0;
    func_alloca = 0;
    loc = 0;

    addr = PTR_SIZE * 2;
    ind += FUNC_PROLOG_SIZE;
    func_sub_sp_offset = ind;
    reg_param_index = 0;

    sym = func_type->ref;

    /* if the function returns a structure, then add an
       implicit pointer parameter */
    func_vt = sym->type;
    func_var = (sym->f.func_type == FUNC_ELLIPSIS);
    size = gfunc_arg_size(&func_vt);
    if (!using_regs(size)) {
        gen_modrm64(s1, 0x89, arg_regs[reg_param_index], VT_LOCAL, NULL, addr);
        func_vc = addr;
        reg_param_index++;
        addr += 8;
    }

    /* define parameters */
    while ((sym = sym->next) != NULL) {
        type = &sym->type;
        bt = type->t & VT_BTYPE;
        size = gfunc_arg_size(type);
        if (!using_regs(size)) {
            if (reg_param_index < REGN) {
                gen_modrm64(s1, 0x89, arg_regs[reg_param_index], VT_LOCAL, NULL, addr);
            }
            sym_push(s1, sym->v & ~SYM_FIELD, type, VT_LLOCAL | VT_LVAL, addr);
        } else {
            if (reg_param_index < REGN) {
                /* save arguments passed by register */
                if ((bt == VT_FLOAT) || (bt == VT_DOUBLE)) {
		    if (tcc_state->nosse)
		      tcc_error(s1, "SSE disabled");
                    o(s1, 0xd60f66); /* movq */
                    gen_modrm(s1, reg_param_index, VT_LOCAL, NULL, addr);
                } else {
                    gen_modrm64(s1, 0x89, arg_regs[reg_param_index], VT_LOCAL, NULL, addr);
                }
            }
            sym_push(s1, sym->v & ~SYM_FIELD, type, VT_LOCAL | VT_LVAL, addr);
        }
        addr += 8;
        reg_param_index++;
    }

    while (reg_param_index < REGN) {
        if (func_type->ref->f.func_type == FUNC_ELLIPSIS) {
            gen_modrm64(s1, 0x89, arg_regs[reg_param_index], VT_LOCAL, NULL, addr);
            addr += 8;
        }
        reg_param_index++;
    }
}

/* generate function epilog */
void gfunc_epilog(TCCState *s1)
{
    int v, saved_ind;

    o(s1, 0xc9); /* leave */
    if (func_ret_sub == 0) {
        o(s1, 0xc3); /* ret */
    } else {
        o(s1, 0xc2); /* ret n */
        g(s1, func_ret_sub);
        g(s1, func_ret_sub >> 8);
    }

    saved_ind = ind;
    ind = func_sub_sp_offset - FUNC_PROLOG_SIZE;
    /* align local size to word & save local variables */
    func_scratch = (func_scratch + 15) & -16;
    v = (func_scratch + -loc + 15) & -16;

    if (v >= 4096) {
        Sym *sym = external_global_sym(s1, TOK___chkstk, &func_old_type, 0);
        oad(s1, 0xb8, v); /* mov stacksize, %eax */
        oad(s1, 0xe8, 0); /* call __chkstk, (does the stackframe too) */
        greloca(s1, cur_text_section, sym, ind-4, R_X86_64_PC32, -4);
        o(s1, 0x90); /* fill for FUNC_PROLOG_SIZE = 11 bytes */
    } else {
        o(s1, 0xe5894855);  /* push %rbp, mov %rsp, %rbp */
        o(s1, 0xec8148);  /* sub rsp, stacksize */
        gen_le32(s1, v);
    }

    /* add the "func_scratch" area after each alloca seen */
    while (func_alloca) {
        unsigned char *ptr = cur_text_section->data + func_alloca;
        func_alloca = read32le(ptr);
        write32le(ptr, func_scratch);
    }

    cur_text_section->data_offset = saved_ind;
    pe_add_unwind_data(s1, ind, saved_ind, v);
    ind = cur_text_section->data_offset;
}

#else

static void gadd_sp(TCCState *s1, int val)
{
    if (val == (char)val) {
        o(s1, 0xc48348);
        g(s1, val);
    } else {
        oad(s1, 0xc48148, val); /* add $xxx, %rsp */
    }
}

typedef enum X86_64_Mode {
  x86_64_mode_none,
  x86_64_mode_memory,
  x86_64_mode_integer,
  x86_64_mode_sse,
  x86_64_mode_x87
} X86_64_Mode;

static X86_64_Mode classify_x86_64_merge(X86_64_Mode a, X86_64_Mode b)
{
    if (a == b)
        return a;
    else if (a == x86_64_mode_none)
        return b;
    else if (b == x86_64_mode_none)
        return a;
    else if ((a == x86_64_mode_memory) || (b == x86_64_mode_memory))
        return x86_64_mode_memory;
    else if ((a == x86_64_mode_integer) || (b == x86_64_mode_integer))
        return x86_64_mode_integer;
    else if ((a == x86_64_mode_x87) || (b == x86_64_mode_x87))
        return x86_64_mode_memory;
    else
        return x86_64_mode_sse;
}

static X86_64_Mode classify_x86_64_inner(CType *ty)
{
    X86_64_Mode mode;
    Sym *f;

    switch (ty->t & VT_BTYPE) {
    case VT_VOID: return x86_64_mode_none;

    case VT_INT:
    case VT_BYTE:
    case VT_SHORT:
    case VT_LLONG:
    case VT_BOOL:
    case VT_PTR:
    case VT_FUNC:
        return x86_64_mode_integer;

    case VT_FLOAT:
    case VT_DOUBLE: return x86_64_mode_sse;

    case VT_LDOUBLE: return x86_64_mode_x87;

    case VT_STRUCT:
        f = ty->ref;

        mode = x86_64_mode_none;
        for (f = f->next; f; f = f->next)
            mode = classify_x86_64_merge(mode, classify_x86_64_inner(&f->type));

        return mode;
    }
    assert(0);
    return 0;
}

static X86_64_Mode classify_x86_64_arg(CType *ty, CType *ret, int *psize, int *palign, int *reg_count)
{
    X86_64_Mode mode;
    int size, align, ret_t = 0;

    if (ty->t & (VT_BITFIELD|VT_ARRAY)) {
        *psize = 8;
        *palign = 8;
        *reg_count = 1;
        ret_t = ty->t;
        mode = x86_64_mode_integer;
    } else {
        size = type_size(ty, &align);
        *psize = (size + 7) & ~7;
        *palign = (align + 7) & ~7;

        if (size > 16) {
            mode = x86_64_mode_memory;
        } else {
            mode = classify_x86_64_inner(ty);
            switch (mode) {
            case x86_64_mode_integer:
                if (size > 8) {
                    *reg_count = 2;
                    ret_t = VT_QLONG;
                } else {
                    *reg_count = 1;
                    ret_t = (size > 4) ? VT_LLONG : VT_INT;
                }
                break;

            case x86_64_mode_x87:
                *reg_count = 1;
                ret_t = VT_LDOUBLE;
                break;

            case x86_64_mode_sse:
                if (size > 8) {
                    *reg_count = 2;
                    ret_t = VT_QFLOAT;
                } else {
                    *reg_count = 1;
                    ret_t = (size > 4) ? VT_DOUBLE : VT_FLOAT;
                }
                break;
            default: break; /* nothing to be done for x86_64_mode_memory and x86_64_mode_none*/
            }
        }
    }

    if (ret) {
        ret->ref = NULL;
        ret->t = ret_t;
    }

    return mode;
}

ST_FUNC int classify_x86_64_va_arg(CType *ty)
{
    /* This definition must be synced with stdarg.h */
    enum __va_arg_type {
        __va_gen_reg, __va_float_reg, __va_stack
    };
    int size, align, reg_count;
    X86_64_Mode mode = classify_x86_64_arg(ty, NULL, &size, &align, &reg_count);
    switch (mode) {
    default: return __va_stack;
    case x86_64_mode_integer: return __va_gen_reg;
    case x86_64_mode_sse: return __va_float_reg;
    }
}

/* Return the number of registers needed to return the struct, or 0 if
   returning via struct pointer. */
ST_FUNC int gfunc_sret(CType *vt, int variadic, CType *ret, int *ret_align, int *regsize)
{
    int size, align, reg_count;
    *ret_align = 1; // Never have to re-align return values for x86-64
    *regsize = 8;
    return (classify_x86_64_arg(vt, ret, &size, &align, &reg_count) != x86_64_mode_memory);
}

#define REGN 6
static const uint8_t arg_regs[REGN] = {
    TREG_RDI, TREG_RSI, TREG_RDX, TREG_RCX, TREG_R8, TREG_R9
};

static int arg_prepare_reg(int idx) {
  if (idx == 2 || idx == 3)
      /* idx=2: r10, idx=3: r11 */
      return idx + 8;
  else
      return arg_regs[idx];
}

/* Generate function call. The function address is pushed first, then
   all the parameters in call order. This functions pops all the
   parameters and the function address. */
void gfunc_call(TCCState *s1, int nb_args)
{
    X86_64_Mode mode;
    CType type;
    int size, align, r, args_size, stack_adjust, i, reg_count;
    int nb_reg_args = 0;
    int nb_sse_args = 0;
    int sse_reg, gen_reg;
    char _onstack[nb_args], *onstack = _onstack;

    /* calculate the number of integer/float register arguments, remember
       arguments to be passed via stack (in onstack[]), and also remember
       if we have to align the stack pointer to 16 (onstack[i] == 2).  Needs
       to be done in a left-to-right pass over arguments.  */
    stack_adjust = 0;
    for(i = nb_args - 1; i >= 0; i--) {
        mode = classify_x86_64_arg(&s1->vtop[-i].type, NULL, &size, &align, &reg_count);
        if (mode == x86_64_mode_sse && nb_sse_args + reg_count <= 8) {
            nb_sse_args += reg_count;
	    onstack[i] = 0;
	} else if (mode == x86_64_mode_integer && nb_reg_args + reg_count <= REGN) {
            nb_reg_args += reg_count;
	    onstack[i] = 0;
	} else if (mode == x86_64_mode_none) {
	    onstack[i] = 0;
	} else {
	    if (align == 16 && (stack_adjust &= 15)) {
		onstack[i] = 2;
		stack_adjust = 0;
	    } else
	      onstack[i] = 1;
	    stack_adjust += size;
	}
    }

    if (nb_sse_args && s1->nosse)
      tcc_error(s1, "SSE disabled but floating point arguments passed");

    /* fetch cpu flag before generating any code */
    if (s1->vtop >= (s1->__vstack + 1) && (s1->vtop->r & VT_VALMASK) == VT_CMP)
      gv(s1, RC_INT);

    /* for struct arguments, we need to call memcpy and the function
       call breaks register passing arguments we are preparing.
       So, we process arguments which will be passed by stack first. */
    gen_reg = nb_reg_args;
    sse_reg = nb_sse_args;
    args_size = 0;
    stack_adjust &= 15;
    for (i = 0; i < nb_args;) {
	mode = classify_x86_64_arg(&s1->vtop[-i].type, NULL, &size, &align, &reg_count);
	if (!onstack[i]) {
	    ++i;
	    continue;
	}
        /* Possibly adjust stack to align SSE boundary.  We're processing
	   args from right to left while allocating happens left to right
	   (stack grows down), so the adjustment needs to happen _after_
	   an argument that requires it.  */
        if (stack_adjust) {
	    o(s1, 0x50); /* push %rax; aka sub $8,%rsp */
            args_size += 8;
	    stack_adjust = 0;
        }
	if (onstack[i] == 2)
	  stack_adjust = 1;

	vrotb(s1, i+1);

	switch (s1->vtop->type.t & VT_BTYPE) {
	    case VT_STRUCT:
		/* allocate the necessary size on stack */
		o(s1, 0x48);
		oad(s1, 0xec81, size); /* sub $xxx, %rsp */
		/* generate structure store */
		r = get_reg(s1, RC_INT);
		orex(s1, 1, r, 0, 0x89); /* mov %rsp, r */
		o(s1, 0xe0 + REG_VALUE(r));
		vset(s1, &s1->vtop->type, r | VT_LVAL, 0);
		vswap(s1);
		vstore(s1);
		break;

	    case VT_LDOUBLE:
                gv(s1, RC_ST0);
                oad(s1, 0xec8148, size); /* sub $xxx, %rsp */
                o(s1, 0x7cdb); /* fstpt 0(%rsp) */
                g(s1, 0x24);
                g(s1, 0x00);
		break;

	    case VT_FLOAT:
	    case VT_DOUBLE:
		assert(mode == x86_64_mode_sse);
		r = gv(s1, RC_FLOAT);
		o(s1, 0x50); /* push $rax */
		/* movq %xmmN, (%rsp) */
		o(s1, 0xd60f66);
		o(s1, 0x04 + REG_VALUE(r)*8);
		o(s1, 0x24);
		break;

	    default:
		assert(mode == x86_64_mode_integer);
		/* simple type */
		/* XXX: implicit cast ? */
		r = gv(s1, RC_INT);
		orex(s1, 0,r,0,0x50 + REG_VALUE(r)); /* push r */
		break;
	}
	args_size += size;

	vpop(s1);
	--nb_args;
	onstack++;
    }

    /* XXX This should be superfluous.  */
    save_regs(s1, 0); /* save used temporary registers */

    /* then, we prepare register passing arguments.
       Note that we cannot set RDX and RCX in this loop because gv()
       may break these temporary registers. Let's use R10 and R11
       instead of them */
    assert(gen_reg <= REGN);
    assert(sse_reg <= 8);
    for(i = 0; i < nb_args; i++) {
        mode = classify_x86_64_arg(&s1->vtop->type, &type, &size, &align, &reg_count);
        /* Alter stack entry type so that gv() knows how to treat it */
        s1->vtop->type = type;
        if (mode == x86_64_mode_sse) {
            if (reg_count == 2) {
                sse_reg -= 2;
                gv(s1, RC_FRET); /* Use pair load into xmm0 & xmm1 */
                if (sse_reg) { /* avoid redundant movaps %xmm0, %xmm0 */
                    /* movaps %xmm0, %xmmN */
                    o(s1, 0x280f);
                    o(s1, 0xc0 + (sse_reg << 3));
                    /* movaps %xmm1, %xmmN */
                    o(s1, 0x280f);
                    o(s1, 0xc1 + ((sse_reg+1) << 3));
                }
            } else {
                assert(reg_count == 1);
                --sse_reg;
                /* Load directly to register */
                gv(s1, RC_XMM0 << sse_reg);
            }
        } else if (mode == x86_64_mode_integer) {
            /* simple type */
            /* XXX: implicit cast ? */
            int d;
            gen_reg -= reg_count;
            r = gv(s1, RC_INT);
            d = arg_prepare_reg(gen_reg);
            orex(s1, 1,d,r,0x89); /* mov */
            o(s1, 0xc0 + REG_VALUE(r) * 8 + REG_VALUE(d));
            if (reg_count == 2) {
                d = arg_prepare_reg(gen_reg+1);
                orex(s1, 1,d,s1->vtop->r2,0x89); /* mov */
                o(s1, 0xc0 + REG_VALUE(s1->vtop->r2) * 8 + REG_VALUE(d));
            }
        }
        s1->vtop--;
    }
    assert(gen_reg == 0);
    assert(sse_reg == 0);

    /* We shouldn't have many operands on the stack anymore, but the
       call address itself is still there, and it might be in %eax
       (or edx/ecx) currently, which the below writes would clobber.
       So evict all remaining operands here.  */
    save_regs(s1, 0);

    /* Copy R10 and R11 into RDX and RCX, respectively */
    if (nb_reg_args > 2) {
        o(s1, 0xd2894c); /* mov %r10, %rdx */
        if (nb_reg_args > 3) {
            o(s1, 0xd9894c); /* mov %r11, %rcx */
        }
    }

    if (s1->vtop->type.ref->f.func_type != FUNC_NEW) /* implies FUNC_OLD or FUNC_ELLIPSIS */
        oad(s1, 0xb8, nb_sse_args < 8 ? nb_sse_args : 8); /* mov nb_sse_args, %eax */
    gcall_or_jmp(s1, 0);
    if (args_size)
        gadd_sp(s1, args_size);
    s1->vtop--;
}


#define FUNC_PROLOG_SIZE 11

static void push_arg_reg(TCCState *s1, int i) {
    s1->loc -= 8;
    gen_modrm64(s1, 0x89, arg_regs[i], VT_LOCAL, NULL, s1->loc);
}

/* generate function prolog of type 't' */
void gfunc_prolog(TCCState *s1, CType *func_type)
{
    X86_64_Mode mode;
    int i, addr, align, size, reg_count;
    int param_addr = 0, reg_param_index, sse_param_index;
    Sym *sym;
    CType *type;

    sym = func_type->ref;
    addr = PTR_SIZE * 2;
    s1->loc = 0;
    s1->ind += FUNC_PROLOG_SIZE;
    func_sub_sp_offset = s1->ind;
    func_ret_sub = 0;

    if (sym->f.func_type == FUNC_ELLIPSIS) {
        int seen_reg_num, seen_sse_num, seen_stack_size;
        seen_reg_num = seen_sse_num = 0;
        /* frame pointer and return address */
        seen_stack_size = PTR_SIZE * 2;
        /* count the number of seen parameters */
        sym = func_type->ref;
        while ((sym = sym->next) != NULL) {
            type = &sym->type;
            mode = classify_x86_64_arg(type, NULL, &size, &align, &reg_count);
            switch (mode) {
            default:
            stack_arg:
                seen_stack_size = ((seen_stack_size + align - 1) & -align) + size;
                break;

            case x86_64_mode_integer:
                if (seen_reg_num + reg_count > REGN)
		    goto stack_arg;
		seen_reg_num += reg_count;
                break;

            case x86_64_mode_sse:
                if (seen_sse_num + reg_count > 8)
		    goto stack_arg;
		seen_sse_num += reg_count;
                break;
            }
        }

        s1->loc -= 16;
        /* movl $0x????????, -0x10(%rbp) */
        o(s1, 0xf045c7);
        gen_le32(s1, seen_reg_num * 8);
        /* movl $0x????????, -0xc(%rbp) */
        o(s1, 0xf445c7);
        gen_le32(s1, seen_sse_num * 16 + 48);
        /* movl $0x????????, -0x8(%rbp) */
        o(s1, 0xf845c7);
        gen_le32(s1, seen_stack_size);

        /* save all register passing arguments */
        for (i = 0; i < 8; i++) {
            s1->loc -= 16;
	    if (!s1->nosse) {
		o(s1, 0xd60f66); /* movq */
		gen_modrm(s1, 7 - i, VT_LOCAL, NULL, s1->loc);
	    }
            /* movq $0, loc+8(%rbp) */
            o(s1, 0x85c748);
            gen_le32(s1, s1->loc + 8);
            gen_le32(s1, 0);
        }
        for (i = 0; i < REGN; i++) {
            push_arg_reg(s1, REGN-1-i);
        }
    }

    sym = func_type->ref;
    reg_param_index = 0;
    sse_param_index = 0;

    /* if the function returns a structure, then add an
       implicit pointer parameter */
    s1->func_vt = sym->type;
    mode = classify_x86_64_arg(&s1->func_vt, NULL, &size, &align, &reg_count);
    if (mode == x86_64_mode_memory) {
        push_arg_reg(s1, reg_param_index);
        s1->func_vc = s1->loc;
        reg_param_index++;
    }
    /* define parameters */
    while ((sym = sym->next) != NULL) {
        type = &sym->type;
        mode = classify_x86_64_arg(type, NULL, &size, &align, &reg_count);
        switch (mode) {
        case x86_64_mode_sse:
	    if (s1->nosse)
	        tcc_error(s1, "SSE disabled but floating point arguments used");
            if (sse_param_index + reg_count <= 8) {
                /* save arguments passed by register */
                s1->loc -= reg_count * 8;
                param_addr = s1->loc;
                for (i = 0; i < reg_count; ++i) {
                    o(s1, 0xd60f66); /* movq */
                    gen_modrm(s1, sse_param_index, VT_LOCAL, NULL, param_addr + i*8);
                    ++sse_param_index;
                }
            } else {
                addr = (addr + align - 1) & -align;
                param_addr = addr;
                addr += size;
            }
            break;

        case x86_64_mode_memory:
        case x86_64_mode_x87:
            addr = (addr + align - 1) & -align;
            param_addr = addr;
            addr += size;
            break;

        case x86_64_mode_integer: {
            if (reg_param_index + reg_count <= REGN) {
                /* save arguments passed by register */
                s1->loc -= reg_count * 8;
                param_addr = s1->loc;
                for (i = 0; i < reg_count; ++i) {
                    gen_modrm64(s1, 0x89, arg_regs[reg_param_index], VT_LOCAL, NULL, param_addr + i*8);
                    ++reg_param_index;
                }
            } else {
                addr = (addr + align - 1) & -align;
                param_addr = addr;
                addr += size;
            }
            break;
        }
	default: break; /* nothing to be done for x86_64_mode_none */
        }
        sym_push(s1, sym->v & ~SYM_FIELD, type,
                 VT_LOCAL | VT_LVAL, param_addr);
    }

#ifdef CONFIG_TCC_BCHECK
    /* leave some room for bound checking code */
    if (s1->do_bounds_check) {
        func_bound_offset = s1->lbounds_section->data_offset;
        func_bound_ind = s1->ind;
        oad(s1, 0xb8, 0); /* lbound section pointer */
	o(s1, 0xc78948);  /* mov  %rax,%rdi ## first arg in %rdi, this must be ptr */
	oad(s1, 0xb8, 0); /* call to function */
    }
#endif
}

/* generate function epilog */
void gfunc_epilog(TCCState *s1)
{
    int v, saved_ind;

#ifdef CONFIG_TCC_BCHECK
    if (s1->do_bounds_check
	&& func_bound_offset != s1->lbounds_section->data_offset)
    {
        addr_t saved_ind;
        addr_t *bounds_ptr;
        Sym *sym_data;

        /* add end of table info */
        bounds_ptr = section_ptr_add(s1, s1->lbounds_section, sizeof(addr_t));
        *bounds_ptr = 0;

        /* generate bound local allocation */
        sym_data = get_sym_ref(s1, &s1->char_pointer_type, s1->lbounds_section,
                               func_bound_offset, s1->lbounds_section->data_offset);
        saved_ind = s1->ind;
        s1->ind = func_bound_ind;
        greloca(s1, s1->cur_text_section, sym_data, s1->ind + 1, R_X86_64_64, 0);
        s1->ind = s1->ind + 5 + 3;
        gen_static_call(s1, TOK___bound_local_new);
        s1->ind = saved_ind;

        /* generate bound check local freeing */
        o(s1, 0x5250); /* save returned value, if any */
        greloca(s1, s1->cur_text_section, sym_data, s1->ind + 1, R_X86_64_64, 0);
        oad(s1, 0xb8, 0); /* mov xxx, %rax */
        o(s1, 0xc78948);  /* mov %rax,%rdi # first arg in %rdi, this must be ptr */
        gen_static_call(s1, TOK___bound_local_delete);
        o(s1, 0x585a); /* restore returned value, if any */
    }
#endif
    o(s1, 0xc9); /* leave */
    if (func_ret_sub == 0) {
        o(s1, 0xc3); /* ret */
    } else {
        o(s1, 0xc2); /* ret n */
        g(s1, func_ret_sub);
        g(s1, func_ret_sub >> 8);
    }
    /* align local size to word & save local variables */
    v = (-s1->loc + 15) & -16;
    saved_ind = s1->ind;
    s1->ind = func_sub_sp_offset - FUNC_PROLOG_SIZE;
    o(s1, 0xe5894855);  /* push %rbp, mov %rsp, %rbp */
    o(s1, 0xec8148);  /* sub rsp, stacksize */
    gen_le32(s1, v);
    s1->ind = saved_ind;
}

#endif /* not PE */

/* generate a jump to a label */
int gjmp(TCCState *s1, int t)
{
    return gjmp2(s1, 0xe9, t);
}

/* generate a jump to a fixed address */
void gjmp_addr(TCCState *s1, int a)
{
    int r;
    r = a - s1->ind - 2;
    if (r == (char)r) {
        g(s1, 0xeb);
        g(s1, r);
    } else {
        oad(s1, 0xe9, a - s1->ind - 5);
    }
}

ST_FUNC void gtst_addr(TCCState *s1, int inv, int a)
{
    int v = s1->vtop->r & VT_VALMASK;
    if (v == VT_CMP) {
	inv ^= (s1->vtop--)->c.i;
	a -= s1->ind + 2;
	if (a == (char)a) {
	    g(s1, inv - 32);
	    g(s1, a);
	} else {
	    g(s1, 0x0f);
	    oad(s1, inv - 16, a - 4);
	}
    } else if ((v & ~1) == VT_JMP) {
	if ((v & 1) != inv) {
	    gjmp_addr(s1, a);
	    gsym(s1, s1->vtop->c.i);
	} else {
	    gsym(s1, s1->vtop->c.i);
	    o(s1, 0x05eb);
	    gjmp_addr(s1, a);
	}
	s1->vtop--;
    }
}

/* generate a test. set 'inv' to invert test. Stack entry is popped */
ST_FUNC int gtst(TCCState *s1, int inv, int t)
{
    int v = s1->vtop->r & VT_VALMASK;

    if (s1->nocode_wanted) {
        ;
    } else if (v == VT_CMP) {
        /* fast case : can jump directly since flags are set */
	if (s1->vtop->c.i & 0x100)
	  {
	    /* This was a float compare.  If the parity flag is set
	       the result was unordered.  For anything except != this
	       means false and we don't jump (anding both conditions).
	       For != this means true (oring both).
	       Take care about inverting the test.  We need to jump
	       to our target if the result was unordered and test wasn't NE,
	       otherwise if unordered we don't want to jump.  */
	    s1->vtop->c.i &= ~0x100;
            if (inv == (s1->vtop->c.i == TOK_NE))
	      o(s1, 0x067a);  /* jp +6 */
	    else
	      {
	        g(s1, 0x0f);
		t = gjmp2(s1, 0x8a, t); /* jp t */
	      }
	  }
        g(s1, 0x0f);
        t = gjmp2(s1, (s1->vtop->c.i - 16) ^ inv, t);
    } else if (v == VT_JMP || v == VT_JMPI) {
        /* && or || optimization */
        if ((v & 1) == inv) {
            /* insert s1->vtop->c jump list in t */
            uint32_t n1, n = s1->vtop->c.i;
            if (n) {
                while ((n1 = read32le(s1->cur_text_section->data + n)))
                    n = n1;
                write32le(s1->cur_text_section->data + n, t);
                t = s1->vtop->c.i;
            }
        } else {
            t = gjmp(s1, t);
            gsym(s1, s1->vtop->c.i);
        }
    }
    s1->vtop--;
    return t;
}

/* generate an integer binary operation */
void gen_opi(TCCState *s1, int op)
{
    int r, fr, opc, c;
    int ll, uu, cc;

    ll = is64_type(s1->vtop[-1].type.t);
    uu = (s1->vtop[-1].type.t & VT_UNSIGNED) != 0;
    cc = (s1->vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST;

    switch(op) {
    case '+':
    case TOK_ADDC1: /* add with carry generation */
        opc = 0;
    gen_op8:
        if (cc && (!ll || (int)s1->vtop->c.i == s1->vtop->c.i)) {
            /* constant case */
            vswap(s1);
            r = gv(s1, RC_INT);
            vswap(s1);
            c = s1->vtop->c.i;
            if (c == (char)c) {
                /* XXX: generate inc and dec for smaller code ? */
                orex(s1, ll, r, 0, 0x83);
                o(s1, 0xc0 | (opc << 3) | REG_VALUE(r));
                g(s1, c);
            } else {
                orex(s1, ll, r, 0, 0x81);
                oad(s1, 0xc0 | (opc << 3) | REG_VALUE(r), c);
            }
        } else {
            gv2(s1, RC_INT, RC_INT);
            r = s1->vtop[-1].r;
            fr = s1->vtop[0].r;
            orex(s1, ll, r, fr, (opc << 3) | 0x01);
            o(s1, 0xc0 + REG_VALUE(r) + REG_VALUE(fr) * 8);
        }
        s1->vtop--;
        if (op >= TOK_ULT && op <= TOK_GT) {
            s1->vtop->r = VT_CMP;
            s1->vtop->c.i = op;
        }
        break;
    case '-':
    case TOK_SUBC1: /* sub with carry generation */
        opc = 5;
        goto gen_op8;
    case TOK_ADDC2: /* add with carry use */
        opc = 2;
        goto gen_op8;
    case TOK_SUBC2: /* sub with carry use */
        opc = 3;
        goto gen_op8;
    case '&':
        opc = 4;
        goto gen_op8;
    case '^':
        opc = 6;
        goto gen_op8;
    case '|':
        opc = 1;
        goto gen_op8;
    case '*':
        gv2(s1, RC_INT, RC_INT);
        r = s1->vtop[-1].r;
        fr = s1->vtop[0].r;
        orex(s1, ll, fr, r, 0xaf0f); /* imul fr, r */
        o(s1, 0xc0 + REG_VALUE(fr) + REG_VALUE(r) * 8);
        s1->vtop--;
        break;
    case TOK_SHL:
        opc = 4;
        goto gen_shift;
    case TOK_SHR:
        opc = 5;
        goto gen_shift;
    case TOK_SAR:
        opc = 7;
    gen_shift:
        opc = 0xc0 | (opc << 3);
        if (cc) {
            /* constant case */
            vswap(s1);
            r = gv(s1, RC_INT);
            vswap(s1);
            orex(s1, ll, r, 0, 0xc1); /* shl/shr/sar $xxx, r */
            o(s1, opc | REG_VALUE(r));
            g(s1, s1->vtop->c.i & (ll ? 63 : 31));
        } else {
            /* we generate the shift in ecx */
            gv2(s1, RC_INT, RC_RCX);
            r = s1->vtop[-1].r;
            orex(s1, ll, r, 0, 0xd3); /* shl/shr/sar %cl, r */
            o(s1, opc | REG_VALUE(r));
        }
        s1->vtop--;
        break;
    case TOK_UDIV:
    case TOK_UMOD:
        uu = 1;
        goto divmod;
    case '/':
    case '%':
    case TOK_PDIV:
        uu = 0;
    divmod:
        /* first operand must be in eax */
        /* XXX: need better constraint for second operand */
        gv2(s1, RC_RAX, RC_RCX);
        r = s1->vtop[-1].r;
        fr = s1->vtop[0].r;
        s1->vtop--;
        save_reg(s1, TREG_RDX);
        orex(s1, ll, 0, 0, uu ? 0xd231 : 0x99); /* xor %edx,%edx : cqto */
        orex(s1, ll, fr, 0, 0xf7); /* div fr, %eax */
        o(s1, (uu ? 0xf0 : 0xf8) + REG_VALUE(fr));
        if (op == '%' || op == TOK_UMOD)
            r = TREG_RDX;
        else
            r = TREG_RAX;
        s1->vtop->r = r;
        break;
    default:
        opc = 7;
        goto gen_op8;
    }
}

void gen_opl(TCCState *s1, int op)
{
    gen_opi(s1, op);
}

/* generate a floating point operation 'v = t1 op t2' instruction. The
   two operands are guaranteed to have the same floating point type */
/* XXX: need to use ST1 too */
void gen_opf(TCCState *s1, int op)
{
    int a, ft, fc, swapped, r;
    int float_type =
        (s1->vtop->type.t & VT_BTYPE) == VT_LDOUBLE ? RC_ST0 : RC_FLOAT;

    /* convert constants to memory references */
    if ((s1->vtop[-1].r & (VT_VALMASK | VT_LVAL)) == VT_CONST) {
        vswap(s1);
        gv(s1, float_type);
        vswap(s1);
    }
    if ((s1->vtop[0].r & (VT_VALMASK | VT_LVAL)) == VT_CONST)
        gv(s1, float_type);

    /* must put at least one value in the floating point register */
    if ((s1->vtop[-1].r & VT_LVAL) &&
        (s1->vtop[0].r & VT_LVAL)) {
        vswap(s1);
        gv(s1, float_type);
        vswap(s1);
    }
    swapped = 0;
    /* swap the stack if needed so that t1 is the register and t2 is
       the memory reference */
    if (s1->vtop[-1].r & VT_LVAL) {
        vswap(s1);
        swapped = 1;
    }
    if ((s1->vtop->type.t & VT_BTYPE) == VT_LDOUBLE) {
        if (op >= TOK_ULT && op <= TOK_GT) {
            /* load on stack second operand */
            load(s1, TREG_ST0, s1->vtop);
            save_reg(s1, TREG_RAX); /* eax is used by FP comparison code */
            if (op == TOK_GE || op == TOK_GT)
                swapped = !swapped;
            else if (op == TOK_EQ || op == TOK_NE)
                swapped = 0;
            if (swapped)
                o(s1, 0xc9d9); /* fxch %st(1) */
            if (op == TOK_EQ || op == TOK_NE)
                o(s1, 0xe9da); /* fucompp */
            else
                o(s1, 0xd9de); /* fcompp */
            o(s1, 0xe0df); /* fnstsw %ax */
            if (op == TOK_EQ) {
                o(s1, 0x45e480); /* and $0x45, %ah */
                o(s1, 0x40fC80); /* cmp $0x40, %ah */
            } else if (op == TOK_NE) {
                o(s1, 0x45e480); /* and $0x45, %ah */
                o(s1, 0x40f480); /* xor $0x40, %ah */
                op = TOK_NE;
            } else if (op == TOK_GE || op == TOK_LE) {
                o(s1, 0x05c4f6); /* test $0x05, %ah */
                op = TOK_EQ;
            } else {
                o(s1, 0x45c4f6); /* test $0x45, %ah */
                op = TOK_EQ;
            }
            s1->vtop--;
            s1->vtop->r = VT_CMP;
            s1->vtop->c.i = op;
        } else {
            /* no memory reference possible for long double operations */
            load(s1, TREG_ST0, s1->vtop);
            swapped = !swapped;

            switch(op) {
            default:
            case '+':
                a = 0;
                break;
            case '-':
                a = 4;
                if (swapped)
                    a++;
                break;
            case '*':
                a = 1;
                break;
            case '/':
                a = 6;
                if (swapped)
                    a++;
                break;
            }
            ft = s1->vtop->type.t;
            fc = s1->vtop->c.i;
            o(s1, 0xde); /* fxxxp %st, %st(1) */
            o(s1, 0xc1 + (a << 3));
            s1->vtop--;
        }
    } else {
        if (op >= TOK_ULT && op <= TOK_GT) {
            /* if saved lvalue, then we must reload it */
            r = s1->vtop->r;
            fc = s1->vtop->c.i;
            if ((r & VT_VALMASK) == VT_LLOCAL) {
                SValue v1;
                r = get_reg(s1, RC_INT);
                v1.type.t = VT_PTR;
                v1.r = VT_LOCAL | VT_LVAL;
                v1.c.i = fc;
                load(s1, r, &v1);
                fc = 0;
            }

            if (op == TOK_EQ || op == TOK_NE) {
                swapped = 0;
            } else {
                if (op == TOK_LE || op == TOK_LT)
                    swapped = !swapped;
                if (op == TOK_LE || op == TOK_GE) {
                    op = 0x93; /* setae */
                } else {
                    op = 0x97; /* seta */
                }
            }

            if (swapped) {
                gv(s1, RC_FLOAT);
                vswap(s1);
            }
            assert(!(s1->vtop[-1].r & VT_LVAL));

            if ((s1->vtop->type.t & VT_BTYPE) == VT_DOUBLE)
                o(s1, 0x66);
            if (op == TOK_EQ || op == TOK_NE)
                o(s1, 0x2e0f); /* ucomisd */
            else
                o(s1, 0x2f0f); /* comisd */

            if (s1->vtop->r & VT_LVAL) {
                gen_modrm(s1, s1->vtop[-1].r, r, s1->vtop->sym, fc);
            } else {
                o(s1, 0xc0 + REG_VALUE(s1->vtop[0].r) + REG_VALUE(s1->vtop[-1].r)*8);
            }

            s1->vtop--;
            s1->vtop->r = VT_CMP;
            s1->vtop->c.i = op | 0x100;
        } else {
            assert((s1->vtop->type.t & VT_BTYPE) != VT_LDOUBLE);
            switch(op) {
            default:
            case '+':
                a = 0;
                break;
            case '-':
                a = 4;
                break;
            case '*':
                a = 1;
                break;
            case '/':
                a = 6;
                break;
            }
            ft = s1->vtop->type.t;
            fc = s1->vtop->c.i;
            assert((ft & VT_BTYPE) != VT_LDOUBLE);

            r = s1->vtop->r;
            /* if saved lvalue, then we must reload it */
            if ((s1->vtop->r & VT_VALMASK) == VT_LLOCAL) {
                SValue v1;
                r = get_reg(s1, RC_INT);
                v1.type.t = VT_PTR;
                v1.r = VT_LOCAL | VT_LVAL;
                v1.c.i = fc;
                load(s1, r, &v1);
                fc = 0;
            }

            assert(!(s1->vtop[-1].r & VT_LVAL));
            if (swapped) {
                assert(s1->vtop->r & VT_LVAL);
                gv(s1, RC_FLOAT);
                vswap(s1);
            }

            if ((ft & VT_BTYPE) == VT_DOUBLE) {
                o(s1, 0xf2);
            } else {
                o(s1, 0xf3);
            }
            o(s1, 0x0f);
            o(s1, 0x58 + a);

            if (s1->vtop->r & VT_LVAL) {
                gen_modrm(s1, s1->vtop[-1].r, r, s1->vtop->sym, fc);
            } else {
                o(s1, 0xc0 + REG_VALUE(s1->vtop[0].r) + REG_VALUE(s1->vtop[-1].r)*8);
            }

            s1->vtop--;
        }
    }
}

/* convert integers to fp 't' type. Must handle 'int', 'unsigned int'
   and 'long long' cases. */
void gen_cvt_itof(TCCState *s1, int t)
{
    if ((t & VT_BTYPE) == VT_LDOUBLE) {
        save_reg(s1, TREG_ST0);
        gv(s1, RC_INT);
        if ((s1->vtop->type.t & VT_BTYPE) == VT_LLONG) {
            /* signed long long to float/double/long double (unsigned case
               is handled generically) */
            o(s1, 0x50 + (s1->vtop->r & VT_VALMASK)); /* push r */
            o(s1, 0x242cdf); /* fildll (%rsp) */
            o(s1, 0x08c48348); /* add $8, %rsp */
        } else if ((s1->vtop->type.t & (VT_BTYPE | VT_UNSIGNED)) ==
                   (VT_INT | VT_UNSIGNED)) {
            /* unsigned int to float/double/long double */
            o(s1, 0x6a); /* push $0 */
            g(s1, 0x00);
            o(s1, 0x50 + (s1->vtop->r & VT_VALMASK)); /* push r */
            o(s1, 0x242cdf); /* fildll (%rsp) */
            o(s1, 0x10c48348); /* add $16, %rsp */
        } else {
            /* int to float/double/long double */
            o(s1, 0x50 + (s1->vtop->r & VT_VALMASK)); /* push r */
            o(s1, 0x2404db); /* fildl (%rsp) */
            o(s1, 0x08c48348); /* add $8, %rsp */
        }
        s1->vtop->r = TREG_ST0;
    } else {
        int r = get_reg(s1, RC_FLOAT);
        gv(s1, RC_INT);
        o(s1, 0xf2 + ((t & VT_BTYPE) == VT_FLOAT?1:0));
        if ((s1->vtop->type.t & (VT_BTYPE | VT_UNSIGNED)) ==
            (VT_INT | VT_UNSIGNED) ||
            (s1->vtop->type.t & VT_BTYPE) == VT_LLONG) {
            o(s1, 0x48); /* REX */
        }
        o(s1, 0x2a0f);
        o(s1, 0xc0 + (s1->vtop->r & VT_VALMASK) + REG_VALUE(r)*8); /* cvtsi2sd */
        s1->vtop->r = r;
    }
}

/* convert from one floating point type to another */
void gen_cvt_ftof(TCCState *s1, int t)
{
    int ft, bt, tbt;

    ft = s1->vtop->type.t;
    bt = ft & VT_BTYPE;
    tbt = t & VT_BTYPE;

    if (bt == VT_FLOAT) {
        gv(s1, RC_FLOAT);
        if (tbt == VT_DOUBLE) {
            o(s1, 0x140f); /* unpcklps */
            o(s1, 0xc0 + REG_VALUE(s1->vtop->r)*9);
            o(s1, 0x5a0f); /* cvtps2pd */
            o(s1, 0xc0 + REG_VALUE(s1->vtop->r)*9);
        } else if (tbt == VT_LDOUBLE) {
            save_reg(s1, RC_ST0);
            /* movss %xmm0,-0x10(%rsp) */
            o(s1, 0x110ff3);
            o(s1, 0x44 + REG_VALUE(s1->vtop->r)*8);
            o(s1, 0xf024);
            o(s1, 0xf02444d9); /* flds -0x10(%rsp) */
            s1->vtop->r = TREG_ST0;
        }
    } else if (bt == VT_DOUBLE) {
        gv(s1, RC_FLOAT);
        if (tbt == VT_FLOAT) {
            o(s1, 0x140f66); /* unpcklpd */
            o(s1, 0xc0 + REG_VALUE(s1->vtop->r)*9);
            o(s1, 0x5a0f66); /* cvtpd2ps */
            o(s1, 0xc0 + REG_VALUE(s1->vtop->r)*9);
        } else if (tbt == VT_LDOUBLE) {
            save_reg(s1, RC_ST0);
            /* movsd %xmm0,-0x10(%rsp) */
            o(s1, 0x110ff2);
            o(s1, 0x44 + REG_VALUE(s1->vtop->r)*8);
            o(s1, 0xf024);
            o(s1, 0xf02444dd); /* fldl -0x10(%rsp) */
            s1->vtop->r = TREG_ST0;
        }
    } else {
        int r;
        gv(s1, RC_ST0);
        r = get_reg(s1, RC_FLOAT);
        if (tbt == VT_DOUBLE) {
            o(s1, 0xf0245cdd); /* fstpl -0x10(%rsp) */
            /* movsd -0x10(%rsp),%xmm0 */
            o(s1, 0x100ff2);
            o(s1, 0x44 + REG_VALUE(r)*8);
            o(s1, 0xf024);
            s1->vtop->r = r;
        } else if (tbt == VT_FLOAT) {
            o(s1, 0xf0245cd9); /* fstps -0x10(%rsp) */
            /* movss -0x10(%rsp),%xmm0 */
            o(s1, 0x100ff3);
            o(s1, 0x44 + REG_VALUE(r)*8);
            o(s1, 0xf024);
            s1->vtop->r = r;
        }
    }
}

/* convert fp to int 't' type */
void gen_cvt_ftoi(TCCState *s1, int t)
{
    int ft, bt, size, r;
    ft = s1->vtop->type.t;
    bt = ft & VT_BTYPE;
    if (bt == VT_LDOUBLE) {
        gen_cvt_ftof(s1, VT_DOUBLE);
        bt = VT_DOUBLE;
    }

    gv(s1, RC_FLOAT);
    if (t != VT_INT)
        size = 8;
    else
        size = 4;

    r = get_reg(s1, RC_INT);
    if (bt == VT_FLOAT) {
        o(s1, 0xf3);
    } else if (bt == VT_DOUBLE) {
        o(s1, 0xf2);
    } else {
        assert(0);
    }
    orex(s1, size == 8, r, 0, 0x2c0f); /* cvttss2si or cvttsd2si */
    o(s1, 0xc0 + REG_VALUE(s1->vtop->r) + REG_VALUE(r)*8);
    s1->vtop->r = r;
}

/* computed goto support */
void ggoto(TCCState *s1)
{
    gcall_or_jmp(s1, 1);
    s1->vtop--;
}

/* Save the stack pointer onto the stack and return the location of its address */
ST_FUNC void gen_vla_sp_save(TCCState *s1, int addr) {
    /* mov %rsp,addr(%rbp)*/
    gen_modrm64(s1, 0x89, TREG_RSP, VT_LOCAL, NULL, addr);
}

/* Restore the SP from a location on the stack */
ST_FUNC void gen_vla_sp_restore(TCCState *s1, int addr) {
    gen_modrm64(s1, 0x8b, TREG_RSP, VT_LOCAL, NULL, addr);
}

#ifdef TCC_TARGET_PE
/* Save result of gen_vla_alloc onto the stack */
ST_FUNC void gen_vla_result(TCCState *s1, int addr) {
    /* mov %rax,addr(%rbp)*/
    gen_modrm64(s1, 0x89, TREG_RAX, VT_LOCAL, NULL, addr);
}
#endif

/* Subtract from the stack pointer, and push the resulting value onto the stack */
ST_FUNC void gen_vla_alloc(TCCState *s1, CType *type, int align) {
#ifdef TCC_TARGET_PE
    /* alloca does more than just adjust %rsp on Windows */
    vpush_global_sym(s1, &func_old_type, TOK_alloca);
    vswap(s1); /* Move alloca ref past allocation size */
    gfunc_call(s1, 1);
#else
    int r;
    r = gv(s1, RC_INT); /* allocation size */
    /* sub r,%rsp */
    o(s1, 0x2b48);
    o(s1, 0xe0 | REG_VALUE(r));
    /* We align to 16 bytes rather than align */
    /* and ~15, %rsp */
    o(s1, 0xf0e48348);
    vpop(s1);
#endif
}


/* end of x86-64 code generator */
/*************************************************************/
