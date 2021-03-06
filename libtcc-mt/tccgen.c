/*
 *  TCC - Tiny C Compiler
 *
 *  Copyright (c) 2001-2004 Fabrice Bellard
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

#include "tcc.h"

#define NODATA_WANTED (s1->nocode_wanted > 0) /* no static data output wanted either */
#define STATIC_DATA_WANTED (s1->nocode_wanted & 0xC0000000) /* only static data output */

/* ------------------------------------------------------------------------- */

static void gen_cast(TCCState *s1, CType *type);
static void gen_cast_s(TCCState *s1, int t);
static inline CType *pointed_type(CType *type);
static int is_compatible_types(CType *type1, CType *type2);
static int parse_btype(TCCState *s1, CType *type, AttributeDef *ad);
static CType *type_decl(TCCState *s1, CType *type, AttributeDef *ad, int *v, int td);
static void parse_expr_type(TCCState *s1, CType *type);
static void init_putv(TCCState *s1, CType *type, Section *sec, unsigned long c);
static void decl_initializer(TCCState *s1, CType *type, Section *sec, unsigned long c, int first, int size_only);
static void block(TCCState *s1, int *bsym, int *csym, int is_expr);
static void decl_initializer_alloc(TCCState *s1, CType *type, AttributeDef *ad, int r, int has_init, int v, int scope);
static void decl(TCCState *s1, int l);
static int decl0(TCCState *s1, int l, int is_for_loop_init, Sym *);
static void expr_eq(TCCState *s1);
static void vla_runtime_type_size(TCCState *s1, CType *type, int *a);
static void vla_sp_restore(TCCState *s1);
static void vla_sp_restore_root(TCCState *s1);
static int is_compatible_unqualified_types(CType *type1, CType *type2);
static inline int64_t expr_const64(TCCState *s1);
static void vpush64(TCCState *s1, int ty, unsigned long long v);
static void vpush(TCCState *s1, CType *type);
static int gvtst(TCCState *s1, int inv, int t);
static void gen_inline_functions(TCCState *s);
static void skip_or_save_block(TCCState *s1, TokenString **str);
static void gv_dup(TCCState *s1);

ST_INLN int is_float(int t)
{
    int bt;
    bt = t & VT_BTYPE;
    return bt == VT_LDOUBLE || bt == VT_DOUBLE || bt == VT_FLOAT || bt == VT_QFLOAT;
}

/* we use our own 'finite' function to avoid potential problems with
   non standard math libs */
/* XXX: endianness dependent */
ST_FUNC int ieee_finite(double d)
{
    int p[4];
    memcpy(p, &d, sizeof(double));
    return ((unsigned)((p[1] | 0x800fffff) + 1)) >> 31;
}

/* compiling intel long double natively */
#if (defined __i386__ || defined __x86_64__) \
    && (defined TCC_TARGET_I386 || defined TCC_TARGET_X86_64)
# define TCC_IS_NATIVE_387
#endif

ST_FUNC void test_lvalue(TCCState *s1)
{
    if (!(s1->vtop->r & VT_LVAL))
        expect(s1, "lvalue");
}

ST_FUNC void check_vstack(TCCState *s1)
{
    if (s1->pvtop != s1->vtop)
        tcc_error(s1, "internal compiler error: vstack leak (%d)", s1->vtop - s1->pvtop);
}

/* ------------------------------------------------------------------------- */
/* start of translation unit info */
ST_FUNC void tcc_debug_start(TCCState *s1)
{
    if (s1->do_debug) {
        char buf[512];

        /* s1->file info: full path + filename */
        s1->section_sym = put_elf_sym(s1, s1->symtab_section, 0, 0,
                                      ELFW(ST_INFO)(STB_LOCAL, STT_SECTION), 0,
                                      s1->text_section->sh_num, NULL);
        getcwd(buf, sizeof(buf));
#ifdef _WIN32
        normalize_slashes(buf);
#endif
        pstrcat(buf, sizeof(buf), "/");
        put_stabs_r(s1, buf, N_SO, 0, 0,
                    s1->text_section->data_offset, s1->text_section, s1->section_sym);
        put_stabs_r(s1, s1->file->filename, N_SO, 0, 0,
                    s1->text_section->data_offset, s1->text_section, s1->section_sym);
        s1->last_ind = 0;
        s1->last_line_num = 0;
    }

    /* an elf symbol of type STT_FILE must be put so that STB_LOCAL
       symbols can be safely used */
    put_elf_sym(s1, s1->symtab_section, 0, 0,
                ELFW(ST_INFO)(STB_LOCAL, STT_FILE), 0,
                SHN_ABS, s1->file->filename);
}

/* put end of translation unit info */
ST_FUNC void tcc_debug_end(TCCState *s1)
{
    if (!s1->do_debug)
        return;
    put_stabs_r(s1, NULL, N_SO, 0, 0,
        s1->text_section->data_offset, s1->text_section, s1->section_sym);

}

/* generate line number info */
ST_FUNC void tcc_debug_line(TCCState *s1)
{
    if (!s1->do_debug)
        return;
    if ((s1->last_line_num != s1->file->line_num || s1->last_ind != s1->ind)) {
        put_stabn(s1, N_SLINE, 0, s1->file->line_num, s1->ind - s1->func_ind);
        s1->last_ind = s1->ind;
        s1->last_line_num = s1->file->line_num;
    }
}

/* put function symbol */
ST_FUNC void tcc_debug_funcstart(TCCState *s1, Sym *sym)
{
    char buf[512];

    if (!s1->do_debug)
        return;

    /* stabs info */
    /* XXX: we put here a dummy type */
    snprintf(buf, sizeof(buf), "%s:%c1",
             s1->funcname, sym->type.t & VT_STATIC ? 'f' : 'F');
    put_stabs_r(s1, buf, N_FUN, 0, s1->file->line_num, 0,
                s1->cur_text_section, sym->c);
    /* //gr gdb wants a line at the function */
    put_stabn(s1, N_SLINE, 0, s1->file->line_num, 0);

    s1->last_ind = 0;
    s1->last_line_num = 0;
}

/* put function size */
ST_FUNC void tcc_debug_funcend(TCCState *s1, int size)
{
    if (!s1->do_debug)
        return;
    put_stabn(s1, N_FUN, 0, 0, size);
}

/* ------------------------------------------------------------------------- */
ST_FUNC int tccgen_compile(TCCState *s1)
{
    s1->cur_text_section = NULL;
    s1->funcname = "";
    s1->anon_sym = SYM_FIRST_ANOM;
    s1->section_sym = 0;
    s1->const_wanted = 0;
    s1->nocode_wanted = 0x80000000;

    /* define some often used types */
    s1->int_type.t = VT_INT;
    s1->char_pointer_type.t = VT_BYTE;
    mk_pointer(s1, &s1->char_pointer_type);
#if PTR_SIZE == 4
    s1->size_type.t = VT_INT | VT_UNSIGNED;
    s1->ptrdiff_type.t = VT_INT;
#elif LONG_SIZE == 4
    s1->size_type.t = VT_LLONG | VT_UNSIGNED;
    s1->ptrdiff_type.t = VT_LLONG;
#else
    s1->size_type.t = VT_LONG | VT_LLONG | VT_UNSIGNED;
    s1->ptrdiff_type.t = VT_LONG | VT_LLONG;
#endif
    s1->func_old_type.t = VT_FUNC;
    s1->func_old_type.ref = sym_push(s1, SYM_FIELD, &s1->int_type, 0, 0);
    s1->func_old_type.ref->f.func_call = FUNC_CDECL;
    s1->func_old_type.ref->f.func_type = FUNC_OLD;

    tcc_debug_start(s1);

#ifdef TCC_TARGET_ARM
    arm_init(s1);
#endif

#ifdef INC_DEBUG
    printf("%s: **** new file\n", s1->file->filename);
#endif

    s1->parse_flags = PARSE_FLAG_PREPROCESS | PARSE_FLAG_TOK_NUM | PARSE_FLAG_TOK_STR;
    next(s1);
    decl(s1, VT_CONST);
    gen_inline_functions(s1);
    check_vstack(s1);
    /* end of translation unit info */
    tcc_debug_end(s1);
    return 0;
}

/* ------------------------------------------------------------------------- */
ST_FUNC ElfSym *elfsym(TCCState *s1, Sym *s)
{
  if (!s || !s->c)
    return NULL;
  return &((ElfSym *)s1->symtab_section->data)[s->c];
}

/* apply storage attributes to Elf symbol */
ST_FUNC void update_storage(TCCState *s1, Sym *sym)
{
    ElfSym *esym;
    int sym_bind, old_sym_bind;

    esym = elfsym(s1, sym);
    if (!esym)
        return;

    if (sym->a.visibility)
        esym->st_other = (esym->st_other & ~ELFW(ST_VISIBILITY)(-1))
            | sym->a.visibility;

    if (sym->type.t & VT_STATIC)
        sym_bind = STB_LOCAL;
    else if (sym->a.weak)
        sym_bind = STB_WEAK;
    else
        sym_bind = STB_GLOBAL;
    old_sym_bind = ELFW(ST_BIND)(esym->st_info);
    if (sym_bind != old_sym_bind) {
        esym->st_info = ELFW(ST_INFO)(sym_bind, ELFW(ST_TYPE)(esym->st_info));
    }

#ifdef TCC_TARGET_PE
    if (sym->a.dllimport)
        esym->st_other |= ST_PE_IMPORT;
    if (sym->a.dllexport)
        esym->st_other |= ST_PE_EXPORT;
#endif

#if 0
    printf("storage %s: bind=%c vis=%d exp=%d imp=%d\n",
        get_tok_str(s1, sym->v, NULL),
        sym_bind == STB_WEAK ? 'w' : sym_bind == STB_LOCAL ? 'l' : 'g',
        sym->a.visibility,
        sym->a.dllexport,
        sym->a.dllimport
        );
#endif
}

/* ------------------------------------------------------------------------- */
/* update sym->c so that it points to an external symbol in section
   'section' with value 'value' */

ST_FUNC void put_extern_sym2(TCCState *s1, Sym *sym, int sh_num,
                            addr_t value, unsigned long size,
                            int can_add_underscore)
{
    int sym_type, sym_bind, info, other, t;
    ElfSym *esym;
    const char *name;
    char buf1[256];
#ifdef CONFIG_TCC_BCHECK
    char buf[32];
#endif

    if (!sym->c) {
        name = get_tok_str(s1, sym->v, NULL);
#ifdef CONFIG_TCC_BCHECK
        if (s1->do_bounds_check) {
            /* XXX: avoid doing that for statics ? */
            /* if bound checking is activated, we change some function
               names by adding the "__bound" prefix */
            switch(sym->v) {
#ifdef TCC_TARGET_PE
            /* XXX: we rely only on malloc hooks */
            case TOK_malloc:
            case TOK_free:
            case TOK_realloc:
            case TOK_memalign:
            case TOK_calloc:
#endif
            case TOK_memcpy:
            case TOK_memmove:
            case TOK_memset:
            case TOK_strlen:
            case TOK_strcpy:
            case TOK_alloca:
                strcpy(buf, "__bound_");
                strcat(buf, name);
                name = buf;
                break;
            }
        }
#endif
        t = sym->type.t;
        if ((t & VT_BTYPE) == VT_FUNC) {
            sym_type = STT_FUNC;
        } else if ((t & VT_BTYPE) == VT_VOID) {
            sym_type = STT_NOTYPE;
        } else {
            sym_type = STT_OBJECT;
        }
        if (t & VT_STATIC)
            sym_bind = STB_LOCAL;
        else
            sym_bind = STB_GLOBAL;
        other = 0;
#ifdef TCC_TARGET_PE
        if (sym_type == STT_FUNC && sym->type.ref) {
            Sym *ref = sym->type.ref;
            if (ref->f.func_call == FUNC_STDCALL && can_add_underscore) {
                sprintf(buf1, "_%s@%d", name, ref->f.func_args * PTR_SIZE);
                name = buf1;
                other |= ST_PE_STDCALL;
                can_add_underscore = 0;
            }
        }
#endif
        if (s1->leading_underscore && can_add_underscore) {
            buf1[0] = '_';
            pstrcpy(buf1 + 1, sizeof(buf1) - 1, name);
            name = buf1;
        }
        if (sym->asm_label)
            name = get_tok_str(s1, sym->asm_label, NULL);
        info = ELFW(ST_INFO)(sym_bind, sym_type);
        sym->c = put_elf_sym(s1, s1->symtab_section, value, size, info, other, sh_num, name);
    } else {
        esym = elfsym(s1, sym);
        esym->st_value = value;
        esym->st_size = size;
        esym->st_shndx = sh_num;
    }
    update_storage(s1, sym);
}

ST_FUNC void put_extern_sym(TCCState *s1, Sym *sym, Section *section,
                           addr_t value, unsigned long size)
{
    int sh_num = section ? section->sh_num : SHN_UNDEF;
    put_extern_sym2(s1, sym, sh_num, value, size, 1);
}

/* add a new relocation entry to symbol 'sym' in section 's' */
ST_FUNC void greloca(TCCState *s1, Section *s, Sym *sym, unsigned long offset, int type,
                     addr_t addend)
{
    int c = 0;

    if (s1->nocode_wanted && s == s1->cur_text_section)
        return;

    if (sym) {
        if (0 == sym->c)
            put_extern_sym(s1, sym, NULL, 0, 0);
        c = sym->c;
    }

    /* now we can add ELF relocation info */
    put_elf_reloca(s1, s1->symtab_section, s, offset, type, c, addend);
}

#if PTR_SIZE == 4
ST_FUNC void greloc(TCCState *s1, Section *s, Sym *sym, unsigned long offset, int type)
{
    greloca(s1, s, sym, offset, type, 0);
}
#endif

/* ------------------------------------------------------------------------- */
/* symbol allocator */
static Sym *__sym_malloc(TCCState *s1)
{
    Sym *sym_pool, *sym, *last_sym;
    int i;

    sym_pool = tcc_malloc(s1, SYM_POOL_NB * sizeof(Sym));
    dynarray_add(s1, &s1->sym_pools, &s1->nb_sym_pools, sym_pool);

    last_sym = s1->sym_free_first;
    sym = sym_pool;
    for(i = 0; i < SYM_POOL_NB; i++) {
        sym->next = last_sym;
        last_sym = sym;
        sym++;
    }
    s1->sym_free_first = last_sym;
    return last_sym;
}

static inline Sym *sym_malloc(TCCState *s1)
{
    Sym *sym;
#ifndef SYM_DEBUG
    sym = s1->sym_free_first;
    if (!sym)
        sym = __sym_malloc(s1);
    s1->sym_free_first = sym->next;
    return sym;
#else
    sym = tcc_malloc(s1, sizeof(Sym));
    return sym;
#endif
}

ST_INLN void sym_free(TCCState *s1, Sym *sym)
{
#ifndef SYM_DEBUG
    sym->next = s1->sym_free_first;
    s1->sym_free_first = sym;
#else
    tcc_free(s1, sym);
#endif
}

/* push, without hashing */
ST_FUNC Sym *sym_push2(TCCState *s1, Sym **ps, int v, int t, int c)
{
    Sym *s;

    s = sym_malloc(s1);
    memset(s, 0, sizeof *s);
    s->v = v;
    s->type.t = t;
    s->c = c;
    /* add in stack */
    s->prev = *ps;
    *ps = s;
    return s;
}

/* find a symbol and return its associated structure. 's' is the top
   of the symbol stack */
ST_FUNC Sym *sym_find2(TCCState *s1, Sym *s, int v)
{
    while (s) {
        if (s->v == v)
            return s;
        else if (s->v == -1)
            return NULL;
        s = s->prev;
    }
    return NULL;
}

/* structure lookup */
ST_INLN Sym *struct_find(TCCState *s1, int v)
{
    v -= TOK_IDENT;
    if ((unsigned)v >= (unsigned)(s1->tok_ident - TOK_IDENT))
        return NULL;
    return s1->table_ident[v]->sym_struct;
}

/* find an identifier */
ST_INLN Sym *sym_find(TCCState *s1, int v)
{
    v -= TOK_IDENT;
    if ((unsigned)v >= (unsigned)(s1->tok_ident - TOK_IDENT))
        return NULL;
    return s1->table_ident[v]->sym_identifier;
}

/* push a given symbol on the symbol stack */
ST_FUNC Sym *sym_push(TCCState *s1, int v, CType *type, int r, int c)
{
    Sym *s, **ps;
    TokenSym *ts;

    if (s1->local_stack)
        ps = &s1->local_stack;
    else
        ps = &s1->global_stack;
    s = sym_push2(s1, ps, v, type->t, c);
    s->type.ref = type->ref;
    s->r = r;
    /* don't record fields or anonymous symbols */
    /* XXX: simplify */
    if (!(v & SYM_FIELD) && (v & ~SYM_STRUCT) < SYM_FIRST_ANOM) {
        /* record symbol in token array */
        ts = s1->table_ident[(v & ~SYM_STRUCT) - TOK_IDENT];
        if (v & SYM_STRUCT)
            ps = &ts->sym_struct;
        else
            ps = &ts->sym_identifier;
        s->prev_tok = *ps;
        *ps = s;
        s->sym_scope = s1->local_scope;
        if (s->prev_tok && s->prev_tok->sym_scope == s->sym_scope)
            tcc_error(s1, "redeclaration of '%s'",
                get_tok_str(s1, v & ~SYM_STRUCT, NULL));
    }
    return s;
}

/* push a global identifier */
ST_FUNC Sym *global_identifier_push(TCCState *s1, int v, int t, int c)
{
    Sym *s, **ps;
    s = sym_push2(s1, &s1->global_stack, v, t, c);
    /* don't record anonymous symbol */
    if (v < SYM_FIRST_ANOM) {
        ps = &s1->table_ident[v - TOK_IDENT]->sym_identifier;
        /* modify the top most local identifier, so that
           sym_identifier will point to 's' when popped */
        while (*ps != NULL && (*ps)->sym_scope)
            ps = &(*ps)->prev_tok;
        s->prev_tok = *ps;
        *ps = s;
    }
    return s;
}

/* pop symbols until top reaches 'b'.  If KEEP is non-zero don't really
   pop them yet from the list, but do remove them from the token array.  */
ST_FUNC void sym_pop(TCCState *s1, Sym **ptop, Sym *b, int keep)
{
    Sym *s, *ss, **ps;
    TokenSym *ts;
    int v;

    s = *ptop;
    while(s != b) {
        ss = s->prev;
        v = s->v;
        /* remove symbol in token array */
        /* XXX: simplify */
        if (!(v & SYM_FIELD) && (v & ~SYM_STRUCT) < SYM_FIRST_ANOM) {
            ts = s1->table_ident[(v & ~SYM_STRUCT) - TOK_IDENT];
            if (v & SYM_STRUCT)
                ps = &ts->sym_struct;
            else
                ps = &ts->sym_identifier;
            *ps = s->prev_tok;
        }
    if (!keep)
        sym_free(s1, s);
        s = ss;
    }
    if (!keep)
    *ptop = b;
}

/* ------------------------------------------------------------------------- */

static void vsetc(TCCState *s1, CType *type, int r, CValue *vc)
{
    int v;

    if (s1->vtop >= (s1->__vstack + 1) + (VSTACK_SIZE - 1))
        tcc_error(s1, "memory full (vstack)");
    /* cannot let cpu flags if other instruction are generated. Also
       avoid leaving VT_JMP anywhere except on the top of the stack
       because it would complicate the code generator.

       Don't do this when s1->nocode_wanted.  s1->vtop might come from
       !s1->nocode_wanted regions (see 88_codeopt.c) and transforming
       it to a register without actually generating code is wrong
       as their value might still be used for real.  All values
       we push under s1->nocode_wanted will eventually be popped
       again, so that the VT_CMP/VT_JMP value will be in s1->vtop
       when code is unsuppressed again.

       Same logic below in vswap(s1); */
    if (s1->vtop >= (s1->__vstack + 1) && !s1->nocode_wanted) {
        v = s1->vtop->r & VT_VALMASK;
        if (v == VT_CMP || (v & ~1) == VT_JMP)
            gv(s1, RC_INT);
    }

    s1->vtop++;
    s1->vtop->type = *type;
    s1->vtop->r = r;
    s1->vtop->r2 = VT_CONST;
    s1->vtop->c = *vc;
    s1->vtop->sym = NULL;
}

ST_FUNC void vswap(TCCState *s1)
{
    SValue tmp;
    /* cannot vswap cpu flags. See comment at vsetc() above */
    if (s1->vtop >= (s1->__vstack + 1) && !s1->nocode_wanted) {
        int v = s1->vtop->r & VT_VALMASK;
        if (v == VT_CMP || (v & ~1) == VT_JMP)
            gv(s1, RC_INT);
    }
    tmp = s1->vtop[0];
    s1->vtop[0] = s1->vtop[-1];
    s1->vtop[-1] = tmp;
}

/* pop stack value */
ST_FUNC void vpop(TCCState *s1)
{
    int v;
    v = s1->vtop->r & VT_VALMASK;
#if defined(TCC_TARGET_I386) || defined(TCC_TARGET_X86_64)
    /* for x86, we need to pop the FP stack */
    if (v == TREG_ST0) {
        o(s1, 0xd8dd); /* fstp %st(0) */
    } else
#endif
    if (v == VT_JMP || v == VT_JMPI) {
        /* need to put correct jump if && or || without test */
        gsym(s1, s1->vtop->c.i);
    }
    s1->vtop--;
}

/* push constant of type "type" with useless value */
ST_FUNC void vpush(TCCState *s1, CType *type)
{
    vset(s1, type, VT_CONST, 0);
}

/* push integer constant */
ST_FUNC void vpushi(TCCState *s1, int v)
{
    CValue cval;
    cval.i = v;
    vsetc(s1, &s1->int_type, VT_CONST, &cval);
}

/* push a pointer sized constant */
static void vpushs(TCCState *s1, addr_t v)
{
  CValue cval;
  cval.i = v;
  vsetc(s1, &s1->size_type, VT_CONST, &cval);
}

/* push arbitrary 64bit constant */
ST_FUNC void vpush64(TCCState *s1, int ty, unsigned long long v)
{
    CValue cval;
    CType ctype;
    ctype.t = ty;
    ctype.ref = NULL;
    cval.i = v;
    vsetc(s1, &ctype, VT_CONST, &cval);
}

/* push long long constant */
static inline void vpushll(TCCState *s1, long long v)
{
    vpush64(s1, VT_LLONG, v);
}

ST_FUNC void vset(TCCState *s1, CType *type, int r, int v)
{
    CValue cval;

    cval.i = v;
    vsetc(s1, type, r, &cval);
}

static void vseti(TCCState *s1, int r, int v)
{
    CType type;
    type.t = VT_INT;
    type.ref = NULL;
    vset(s1, &type, r, v);
}

ST_FUNC void vpushv(TCCState *s1, SValue *v)
{
    if (s1->vtop >= (s1->__vstack + 1) + (VSTACK_SIZE - 1))
        tcc_error(s1, "memory full (vstack)");
    s1->vtop++;
    *s1->vtop = *v;
}

static void vdup(TCCState *s1)
{
    vpushv(s1, s1->vtop);
}

/* rotate n first stack elements to the bottom
   I1 ... In -> I2 ... In I1 [top is right]
*/
ST_FUNC void vrotb(TCCState *s1, int n)
{
    int i;
    SValue tmp;

    tmp = s1->vtop[-n + 1];
    for(i=-n+1;i!=0;i++)
        s1->vtop[i] = s1->vtop[i+1];
    s1->vtop[0] = tmp;
}

/* rotate the n elements before entry e towards the top
   I1 ... In ... -> In I1 ... I(n-1) ... [top is right]
 */
ST_FUNC void vrote(TCCState *s1, SValue *e, int n)
{
    int i;
    SValue tmp;

    tmp = *e;
    for(i = 0;i < n - 1; i++)
        e[-i] = e[-i - 1];
    e[-n + 1] = tmp;
}

/* rotate n first stack elements to the top
   I1 ... In -> In I1 ... I(n-1)  [top is right]
 */
ST_FUNC void vrott(TCCState *s1, int n)
{
    vrote(s1, s1->vtop, n);
}

/* push a symbol value of TYPE */
static inline void vpushsym(TCCState *s1, CType *type, Sym *sym)
{
    CValue cval;
    cval.i = 0;
    vsetc(s1, type, VT_CONST | VT_SYM, &cval);
    s1->vtop->sym = sym;
}

/* Return a static symbol pointing to a section */
ST_FUNC Sym *get_sym_ref(TCCState *s1, CType *type, Section *sec, unsigned long offset, unsigned long size)
{
    int v;
    Sym *sym;

    v = s1->anon_sym++;
    sym = global_identifier_push(s1, v, type->t | VT_STATIC, 0);
    sym->type.ref = type->ref;
    sym->r = VT_CONST | VT_SYM;
    put_extern_sym(s1, sym, sec, offset, size);
    return sym;
}

/* push a reference to a section offset by adding a dummy symbol */
static void vpush_ref(TCCState *s1, CType *type, Section *sec, unsigned long offset, unsigned long size)
{
    vpushsym(s1, type, get_sym_ref(s1, type, sec, offset, size));
}

/* define a new external reference to a symbol 'v' of type 'u' */
ST_FUNC Sym *external_global_sym(TCCState *s1, int v, CType *type, int r)
{
    Sym *s;

    s = sym_find(s1, v);
    if (!s) {
        /* push forward reference */
        s = global_identifier_push(s1, v, type->t | VT_EXTERN, 0);
        s->type.ref = type->ref;
        s->r = r | VT_CONST | VT_SYM;
    } else if (IS_ASM_SYM(s)) {
        s->type.t = type->t | (s->type.t & VT_EXTERN);
        s->type.ref = type->ref;
        update_storage(s1, s);
    }
    return s;
}

/* Merge some type attributes.  */
static void patch_type(TCCState *s1, Sym *sym, CType *type)
{
    if (!(type->t & VT_EXTERN)) {
        if (!(sym->type.t & VT_EXTERN))
            tcc_error(s1, "redefinition of '%s'", get_tok_str(s1, sym->v, NULL));
        sym->type.t &= ~VT_EXTERN;
    }

    if (IS_ASM_SYM(sym)) {
        /* stay static if both are static */
        sym->type.t = type->t & (sym->type.t | ~VT_STATIC);
        sym->type.ref = type->ref;
    }

    if (!is_compatible_types(&sym->type, type)) {
        tcc_error(s1, "incompatible types for redefinition of '%s'",
                  get_tok_str(s1, sym->v, NULL));

    } else if ((sym->type.t & VT_BTYPE) == VT_FUNC) {
        int static_proto = sym->type.t & VT_STATIC;
        /* warn if static follows non-static function declaration */
        if ((type->t & VT_STATIC) && !static_proto && !(type->t & VT_INLINE))
            tcc_warning(s1, "static storage ignored for redefinition of '%s'",
                get_tok_str(s1, sym->v, NULL));

        if (0 == (type->t & VT_EXTERN)) {
            /* put complete type, use static from prototype */
            sym->type.t = (type->t & ~VT_STATIC) | static_proto;
            if (type->t & VT_INLINE)
                sym->type.t = type->t;
            sym->type.ref = type->ref;
        }

    } else {
        if ((sym->type.t & VT_ARRAY) && type->ref->c >= 0) {
            /* set array size if it was omitted in extern declaration */
            if (sym->type.ref->c < 0)
                sym->type.ref->c = type->ref->c;
            else if (sym->type.ref->c != type->ref->c)
                tcc_error(s1, "conflicting type for '%s'", get_tok_str(s1, sym->v, NULL));
        }
        if ((type->t ^ sym->type.t) & VT_STATIC)
            tcc_warning(s1, "storage mismatch for redefinition of '%s'",
                get_tok_str(s1, sym->v, NULL));
    }
}


/* Merge some storage attributes.  */
static void patch_storage(TCCState *s1, Sym *sym, AttributeDef *ad, CType *type)
{
    if (type)
        patch_type(s1, sym, type);

#ifdef TCC_TARGET_PE
    if (sym->a.dllimport != ad->a.dllimport)
        tcc_error(s1, "incompatible dll linkage for redefinition of '%s'",
            get_tok_str(s1, sym->v, NULL));
    sym->a.dllexport |= ad->a.dllexport;
#endif
    sym->a.weak |= ad->a.weak;
    if (ad->a.visibility) {
        int vis = sym->a.visibility;
        int vis2 = ad->a.visibility;
        if (vis == STV_DEFAULT)
            vis = vis2;
        else if (vis2 != STV_DEFAULT)
            vis = (vis < vis2) ? vis : vis2;
        sym->a.visibility = vis;
    }
    if (ad->a.aligned)
        sym->a.aligned = ad->a.aligned;
    if (ad->asm_label)
        sym->asm_label = ad->asm_label;
    update_storage(s1, sym);
}

/* define a new external reference to a symbol 'v' */
static Sym *external_sym(TCCState *s1, int v, CType *type, int r, AttributeDef *ad)
{
    Sym *s;
    s = sym_find(s1, v);
    if (!s) {
        /* push forward reference */
        s = sym_push(s1, v, type, r | VT_CONST | VT_SYM, 0);
        s->type.t |= VT_EXTERN;
        s->a = ad->a;
        s->sym_scope = 0;
    } else {
        if (s->type.ref == s1->func_old_type.ref) {
            s->type.ref = type->ref;
            s->r = r | VT_CONST | VT_SYM;
            s->type.t |= VT_EXTERN;
        }
        patch_storage(s1, s, ad, type);
    }
    return s;
}

/* push a reference to global symbol v */
ST_FUNC void vpush_global_sym(TCCState *s1, CType *type, int v)
{
    vpushsym(s1, type, external_global_sym(s1, v, type, 0));
}

/* save registers up to (s1->vtop - n) stack entry */
ST_FUNC void save_regs(TCCState *s1, int n)
{
    SValue *p, *p1;
    for(p = (s1->__vstack + 1), p1 = s1->vtop - n; p <= p1; p++)
        save_reg(s1, p->r);
}

/* save r to the memory stack, and mark it as being free */
ST_FUNC void save_reg(TCCState *s1, int r)
{
    save_reg_upstack(s1, r, 0);
}

/* save r to the memory stack, and mark it as being free,
   if seen up to (s1->vtop - n) stack entry */
ST_FUNC void save_reg_upstack(TCCState *s1, int r, int n)
{
    int l, saved, size, align;
    SValue *p, *p1, sv;
    CType *type;

    if ((r &= VT_VALMASK) >= VT_CONST)
        return;
    if (s1->nocode_wanted)
        return;

    /* modify all stack values */
    saved = 0;
    l = 0;
    for(p = (s1->__vstack + 1), p1 = s1->vtop - n; p <= p1; p++) {
        if ((p->r & VT_VALMASK) == r ||
            ((p->type.t & VT_BTYPE) == VT_LLONG && (p->r2 & VT_VALMASK) == r)) {
            /* must save value on stack if not already done */
            if (!saved) {
                /* NOTE: must reload 'r' because r might be equal to r2 */
                r = p->r & VT_VALMASK;
                /* store register in the stack */
                type = &p->type;
                if ((p->r & VT_LVAL) ||
                    (!is_float(type->t) && (type->t & VT_BTYPE) != VT_LLONG))
#if PTR_SIZE == 8
                    type = &s1->char_pointer_type;
#else
                    type = &s1->int_type;
#endif
                size = type_size(type, &align);
                s1->loc = (s1->loc - size) & -align;
                sv.type.t = type->t;
                sv.r = VT_LOCAL | VT_LVAL;
                sv.c.i = s1->loc;
                store(s1, r, &sv);
#if defined(TCC_TARGET_I386) || defined(TCC_TARGET_X86_64)
                /* x86 specific: need to pop fp register ST0 if saved */
                if (r == TREG_ST0) {
                    o(s1, 0xd8dd); /* fstp %st(0) */
                }
#endif
#if PTR_SIZE == 4
                /* special long long case */
                if ((type->t & VT_BTYPE) == VT_LLONG) {
                    sv.c.i += 4;
                    store(s1, p->r2, &sv);
                }
#endif
                l = s1->loc;
                saved = 1;
            }
            /* mark that stack entry as being saved on the stack */
            if (p->r & VT_LVAL) {
                /* also clear the bounded flag because the
                   relocation address of the function was stored in
                   p->c.i */
                p->r = (p->r & ~(VT_VALMASK | VT_BOUNDED)) | VT_LLOCAL;
            } else {
                p->r = lvalue_type(p->type.t) | VT_LOCAL;
            }
            p->r2 = VT_CONST;
            p->c.i = l;
        }
    }
}

#ifdef TCC_TARGET_ARM
/* find a register of class 'rc2' with at most one reference on stack.
 * If none, call get_reg(s1, rc) */
ST_FUNC int get_reg_ex(TCCState *s1, int rc, int rc2)
{
    int r;
    SValue *p;

    for(r=0;r<NB_REGS;r++) {
        if (reg_classes[r] & rc2) {
            int n;
            n=0;
            for(p = (s1->__vstack + 1); p <= s1->vtop; p++) {
                if ((p->r & VT_VALMASK) == r ||
                    (p->r2 & VT_VALMASK) == r)
                    n++;
            }
            if (n <= 1)
                return r;
        }
    }
    return get_reg(s1, rc);
}
#endif

/* find a free register of class 'rc'. If none, save one register */
ST_FUNC int get_reg(TCCState *s1, int rc)
{
    int r;
    SValue *p;

    /* find a free register */
    for(r=0;r<NB_REGS;r++) {
        if (reg_classes[r] & rc) {
            if (s1->nocode_wanted)
                return r;
            for(p=(s1->__vstack + 1);p<=s1->vtop;p++) {
                if ((p->r & VT_VALMASK) == r ||
                    (p->r2 & VT_VALMASK) == r)
                    goto notfound;
            }
            return r;
        }
    notfound: ;
    }

    /* no register left : free the first one on the stack (VERY
       IMPORTANT to start from the bottom to ensure that we don't
       spill registers used in gen_opi()) */
    for(p=(s1->__vstack + 1);p<=s1->vtop;p++) {
        /* look at second register (if long long) */
        r = p->r2 & VT_VALMASK;
        if (r < VT_CONST && (reg_classes[r] & rc))
            goto save_found;
        r = p->r & VT_VALMASK;
        if (r < VT_CONST && (reg_classes[r] & rc)) {
        save_found:
            save_reg(s1, r);
            return r;
        }
    }
    /* Should never comes here */
    return -1;
}

/* move register 's' (of type 't') to 'r', and flush previous value of r to memory
   if needed */
static void move_reg(TCCState *s1, int r, int s, int t)
{
    SValue sv;

    if (r != s) {
        save_reg(s1, r);
        sv.type.t = t;
        sv.type.ref = NULL;
        sv.r = s;
        sv.c.i = 0;
        load(s1, r, &sv);
    }
}

/* get address of s1->vtop (s1->vtop MUST BE an lvalue) */
ST_FUNC void gaddrof(TCCState *s1)
{
    s1->vtop->r &= ~VT_LVAL;
    /* tricky: if saved lvalue, then we can go back to lvalue */
    if ((s1->vtop->r & VT_VALMASK) == VT_LLOCAL)
        s1->vtop->r = (s1->vtop->r & ~(VT_VALMASK | VT_LVAL_TYPE)) | VT_LOCAL | VT_LVAL;
}

#ifdef CONFIG_TCC_BCHECK
/* generate lvalue bound code */
static void gbound(TCCState *s1)
{
    int lval_type;
    CType type1;

    s1->vtop->r &= ~VT_MUSTBOUND;
    /* if lvalue, then use checking code before dereferencing */
    if (s1->vtop->r & VT_LVAL) {
        /* if not VT_BOUNDED value, then make one */
        if (!(s1->vtop->r & VT_BOUNDED)) {
            lval_type = s1->vtop->r & (VT_LVAL_TYPE | VT_LVAL);
            /* must save type because we must set it to int to get pointer */
            type1 = s1->vtop->type;
            s1->vtop->type.t = VT_PTR;
            gaddrof(s1);
            vpushi(s1, 0);
            gen_bounded_ptr_add(s1);
            s1->vtop->r |= lval_type;
            s1->vtop->type = type1;
        }
        /* then check for dereferencing */
        gen_bounded_ptr_deref(s1);
    }
}
#endif

static void incr_bf_adr(TCCState *s1, int o)
{
    s1->vtop->type = s1->char_pointer_type;
    gaddrof(s1);
    vpushi(s1, o);
    gen_op(s1, '+');
    s1->vtop->type.t = (s1->vtop->type.t & ~(VT_BTYPE|VT_DEFSIGN))
        | (VT_BYTE|VT_UNSIGNED);
    s1->vtop->r = (s1->vtop->r & ~VT_LVAL_TYPE)
        | (VT_LVAL_BYTE|VT_LVAL_UNSIGNED|VT_LVAL);
}

/* single-byte load mode for packed or otherwise unaligned bitfields */
static void load_packed_bf(TCCState *s1, CType *type, int bit_pos, int bit_size)
{
    int n, o, bits;
    save_reg_upstack(s1, s1->vtop->r, 1);
    vpush64(s1, type->t & VT_BTYPE, 0); // B X
    bits = 0, o = bit_pos >> 3, bit_pos &= 7;
    do {
        vswap(s1); // X B
        incr_bf_adr(s1, o);
        vdup(s1); // X B B
        n = 8 - bit_pos;
        if (n > bit_size)
            n = bit_size;
        if (bit_pos)
            vpushi(s1, bit_pos), gen_op(s1, TOK_SHR), bit_pos = 0; // X B Y
        if (n < 8)
            vpushi(s1, (1 << n) - 1), gen_op(s1, '&');
        gen_cast(s1, type);
        if (bits)
            vpushi(s1, bits), gen_op(s1, TOK_SHL);
        vrotb(s1, 3); // B Y X
        gen_op(s1, '|'); // B X
        bits += n, bit_size -= n, o = 1;
    } while (bit_size);
    vswap(s1), vpop(s1);
    if (!(type->t & VT_UNSIGNED)) {
        n = ((type->t & VT_BTYPE) == VT_LLONG ? 64 : 32) - bits;
        vpushi(s1, n), gen_op(s1, TOK_SHL);
        vpushi(s1, n), gen_op(s1, TOK_SAR);
    }
}

/* single-byte store mode for packed or otherwise unaligned bitfields */
static void store_packed_bf(TCCState *s1, int bit_pos, int bit_size)
{
    int bits, n, o, m, c;

    c = (s1->vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST;
    vswap(s1); // X B
    save_reg_upstack(s1, s1->vtop->r, 1);
    bits = 0, o = bit_pos >> 3, bit_pos &= 7;
    do {
        incr_bf_adr(s1, o); // X B
        vswap(s1); //B X
        c ? vdup(s1) : gv_dup(s1); // B V X
        vrott(s1, 3); // X B V
        if (bits)
            vpushi(s1, bits), gen_op(s1, TOK_SHR);
        if (bit_pos)
            vpushi(s1, bit_pos), gen_op(s1, TOK_SHL);
        n = 8 - bit_pos;
        if (n > bit_size)
            n = bit_size;
        if (n < 8) {
            m = ((1 << n) - 1) << bit_pos;
            vpushi(s1, m), gen_op(s1, '&'); // X B V1
            vpushv(s1, s1->vtop-1); // X B V1 B
            vpushi(s1, m & 0x80 ? ~m & 0x7f : ~m);
            gen_op(s1, '&'); // X B V1 B1
            gen_op(s1, '|'); // X B V2
        }
        vdup(s1), s1->vtop[-1] = s1->vtop[-2]; // X B B V2
        vstore(s1), vpop(s1); // X B
        bits += n, bit_size -= n, bit_pos = 0, o = 1;
    } while (bit_size);
    vpop(s1), vpop(s1);
}

static int adjust_bf(SValue *sv, int bit_pos, int bit_size)
{
    int t;
    if (0 == sv->type.ref)
        return 0;
    t = sv->type.ref->auxtype;
    if (t != -1 && t != VT_STRUCT) {
        sv->type.t = (sv->type.t & ~VT_BTYPE) | t;
        sv->r = (sv->r & ~VT_LVAL_TYPE) | lvalue_type(sv->type.t);
    }
    return t;
}

/* store s1->vtop a register belonging to class 'rc'. lvalues are
   converted to values. Cannot be used if cannot be converted to
   register value (such as structures). */
ST_FUNC int gv(TCCState *s1, int rc)
{
    int r, bit_pos, bit_size, size, align, rc2;

    /* NOTE: get_reg can modify vstack[] */
    if (s1->vtop->type.t & VT_BITFIELD) {
        CType type;

        bit_pos = BIT_POS(s1->vtop->type.t);
        bit_size = BIT_SIZE(s1->vtop->type.t);
        /* remove bit field info to avoid loops */
        s1->vtop->type.t &= ~VT_STRUCT_MASK;

        type.ref = NULL;
        type.t = s1->vtop->type.t & VT_UNSIGNED;
        if ((s1->vtop->type.t & VT_BTYPE) == VT_BOOL)
            type.t |= VT_UNSIGNED;

        r = adjust_bf(s1->vtop, bit_pos, bit_size);

        if ((s1->vtop->type.t & VT_BTYPE) == VT_LLONG)
            type.t |= VT_LLONG;
        else
            type.t |= VT_INT;

        if (r == VT_STRUCT) {
            load_packed_bf(s1, &type, bit_pos, bit_size);
        } else {
            int bits = (type.t & VT_BTYPE) == VT_LLONG ? 64 : 32;
            /* cast to int to propagate signedness in following ops */
            gen_cast(s1, &type);
            /* generate shifts */
            vpushi(s1, bits - (bit_pos + bit_size));
            gen_op(s1, TOK_SHL);
            vpushi(s1, bits - bit_size);
            /* NOTE: transformed to SHR if unsigned */
            gen_op(s1, TOK_SAR);
        }
        r = gv(s1, rc);
    } else {
        if (is_float(s1->vtop->type.t) &&
            (s1->vtop->r & (VT_VALMASK | VT_LVAL)) == VT_CONST) {
            unsigned long offset;
            /* CPUs usually cannot use float constants, so we store them
               generically in data segment */
            size = type_size(&s1->vtop->type, &align);
            if (NODATA_WANTED)
                size = 0, align = 1;
            offset = section_add(s1, s1->data_section, size, align);
            vpush_ref(s1, &s1->vtop->type, s1->data_section, offset, size);
        vswap(s1);
        init_putv(s1, &s1->vtop->type, s1->data_section, offset);
        s1->vtop->r |= VT_LVAL;
        }
#ifdef CONFIG_TCC_BCHECK
        if (s1->vtop->r & VT_MUSTBOUND)
            gbound(s1);
#endif

        r = s1->vtop->r & VT_VALMASK;
        rc2 = (rc & RC_FLOAT) ? RC_FLOAT : RC_INT;
#ifndef TCC_TARGET_ARM64
        if (rc == RC_IRET)
            rc2 = RC_LRET;
#ifdef TCC_TARGET_X86_64
        else if (rc == RC_FRET)
            rc2 = RC_QRET;
#endif
#endif
        /* need to reload if:
           - constant
           - lvalue (need to dereference pointer)
           - already a register, but not in the right class */
        if (r >= VT_CONST
         || (s1->vtop->r & VT_LVAL)
         || !(reg_classes[r] & rc)
#if PTR_SIZE == 8
         || ((s1->vtop->type.t & VT_BTYPE) == VT_QLONG && !(reg_classes[s1->vtop->r2] & rc2))
         || ((s1->vtop->type.t & VT_BTYPE) == VT_QFLOAT && !(reg_classes[s1->vtop->r2] & rc2))
#else
         || ((s1->vtop->type.t & VT_BTYPE) == VT_LLONG && !(reg_classes[s1->vtop->r2] & rc2))
#endif
            )
        {
            r = get_reg(s1, rc);
#if PTR_SIZE == 8
            if (((s1->vtop->type.t & VT_BTYPE) == VT_QLONG) || ((s1->vtop->type.t & VT_BTYPE) == VT_QFLOAT)) {
                int addr_type = VT_LLONG, load_size = 8, load_type = ((s1->vtop->type.t & VT_BTYPE) == VT_QLONG) ? VT_LLONG : VT_DOUBLE;
#else
            if ((s1->vtop->type.t & VT_BTYPE) == VT_LLONG) {
                int addr_type = VT_INT, load_size = 4, load_type = VT_INT;
                unsigned long long ll;
#endif
                int r2, original_type;
                original_type = s1->vtop->type.t;
                /* two register type load : expand to two words
                   temporarily */
#if PTR_SIZE == 4
                if ((s1->vtop->r & (VT_VALMASK | VT_LVAL)) == VT_CONST) {
                    /* load constant */
                    ll = s1->vtop->c.i;
                    s1->vtop->c.i = ll; /* first word */
                    load(s1, r, s1->vtop);
                    s1->vtop->r = r; /* save register value */
                    vpushi(s1, ll >> 32); /* second word */
                } else
#endif
                if (s1->vtop->r & VT_LVAL) {
                    /* We do not want to modifier the long long
                       pointer here, so the safest (and less
                       efficient) is to save all the other registers
                       in the stack. XXX: totally inefficient. */
               #if 0
                    save_regs(s1, 1);
               #else
                    /* lvalue_save: save only if used further down the stack */
                    save_reg_upstack(s1, s1->vtop->r, 1);
               #endif
                    /* load from memory */
                    s1->vtop->type.t = load_type;
                    load(s1, r, s1->vtop);
                    vdup(s1);
                    s1->vtop[-1].r = r; /* save register value */
                    /* increment pointer to get second word */
                    s1->vtop->type.t = addr_type;
                    gaddrof(s1);
                    vpushi(s1, load_size);
                    gen_op(s1, '+');
                    s1->vtop->r |= VT_LVAL;
                    s1->vtop->type.t = load_type;
                } else {
                    /* move registers */
                    load(s1, r, s1->vtop);
                    vdup(s1);
                    s1->vtop[-1].r = r; /* save register value */
                    s1->vtop->r = s1->vtop[-1].r2;
                }
                /* Allocate second register. Here we rely on the fact that
                   get_reg(s1, ) tries first to free r2 of an SValue. */
                r2 = get_reg(s1, rc2);
                load(s1, r2, s1->vtop);
                vpop(s1);
                /* write second register */
                s1->vtop->r2 = r2;
                s1->vtop->type.t = original_type;
            } else if ((s1->vtop->r & VT_LVAL) && !is_float(s1->vtop->type.t)) {
                int t1, t;
                /* lvalue of scalar type : need to use lvalue type
                   because of possible cast */
                t = s1->vtop->type.t;
                t1 = t;
                /* compute memory access type */
                if (s1->vtop->r & VT_LVAL_BYTE)
                    t = VT_BYTE;
                else if (s1->vtop->r & VT_LVAL_SHORT)
                    t = VT_SHORT;
                if (s1->vtop->r & VT_LVAL_UNSIGNED)
                    t |= VT_UNSIGNED;
                s1->vtop->type.t = t;
                load(s1, r, s1->vtop);
                /* restore wanted type */
                s1->vtop->type.t = t1;
            } else {
                /* one register type load */
                load(s1, r, s1->vtop);
            }
        }
        s1->vtop->r = r;
#ifdef TCC_TARGET_C67
        /* uses register pairs for doubles */
        if ((s1->vtop->type.t & VT_BTYPE) == VT_DOUBLE)
            s1->vtop->r2 = r+1;
#endif
    }
    return r;
}

/* generate s1->vtop[-1] and s1->vtop[0] in resp. classes rc1 and rc2 */
ST_FUNC void gv2(TCCState *s1, int rc1, int rc2)
{
    int v;

    /* generate more generic register first. But VT_JMP or VT_CMP
       values must be generated first in all cases to avoid possible
       reload errors */
    v = s1->vtop[0].r & VT_VALMASK;
    if (v != VT_CMP && (v & ~1) != VT_JMP && rc1 <= rc2) {
        vswap(s1);
        gv(s1, rc1);
        vswap(s1);
        gv(s1, rc2);
        /* test if reload is needed for first register */
        if ((s1->vtop[-1].r & VT_VALMASK) >= VT_CONST) {
            vswap(s1);
            gv(s1, rc1);
            vswap(s1);
        }
    } else {
        gv(s1, rc2);
        vswap(s1);
        gv(s1, rc1);
        vswap(s1);
        /* test if reload is needed for first register */
        if ((s1->vtop[0].r & VT_VALMASK) >= VT_CONST) {
            gv(s1, rc2);
        }
    }
}

#ifndef TCC_TARGET_ARM64
/* wrapper around RC_FRET to return a register by type */
static int rc_fret(int t)
{
#ifdef TCC_TARGET_X86_64
    if (t == VT_LDOUBLE) {
        return RC_ST0;
    }
#endif
    return RC_FRET;
}
#endif

/* wrapper around REG_FRET to return a register by type */
static int reg_fret(int t)
{
#ifdef TCC_TARGET_X86_64
    if (t == VT_LDOUBLE) {
        return TREG_ST0;
    }
#endif
    return REG_FRET;
}

#if PTR_SIZE == 4
/* expand 64bit on stack in two ints */
static void lexpand(TCCState *s1)
{
    int u, v;
    u = s1->vtop->type.t & (VT_DEFSIGN | VT_UNSIGNED);
    v = s1->vtop->r & (VT_VALMASK | VT_LVAL);
    if (v == VT_CONST) {
        vdup(s1);
        s1->vtop[0].c.i >>= 32;
    } else if (v == (VT_LVAL|VT_CONST) || v == (VT_LVAL|VT_LOCAL)) {
        vdup(s1);
        s1->vtop[0].c.i += 4;
    } else {
        gv(s1, RC_INT);
        vdup(s1);
        s1->vtop[0].r = s1->vtop[-1].r2;
        s1->vtop[0].r2 = s1->vtop[-1].r2 = VT_CONST;
    }
    s1->vtop[0].type.t = s1->vtop[-1].type.t = VT_INT | u;
}
#endif

#ifdef TCC_TARGET_ARM
/* expand long long on stack */
ST_FUNC void lexpand_nr(TCCState *s1)
{
    int u,v;

    u = s1->vtop->type.t & (VT_DEFSIGN | VT_UNSIGNED);
    vdup(s1);
    s1->vtop->r2 = VT_CONST;
    s1->vtop->type.t = VT_INT | u;
    v=s1->vtop[-1].r & (VT_VALMASK | VT_LVAL);
    if (v == VT_CONST) {
      s1->vtop[-1].c.i = s1->vtop->c.i;
      s1->vtop->c.i = s1->vtop->c.i >> 32;
      s1->vtop->r = VT_CONST;
    } else if (v == (VT_LVAL|VT_CONST) || v == (VT_LVAL|VT_LOCAL)) {
      s1->vtop->c.i += 4;
      s1->vtop->r = s1->vtop[-1].r;
    } else if (v > VT_CONST) {
      s1->vtop--;
      lexpand(s1);
    } else
      s1->vtop->r = s1->vtop[-1].r2;
    s1->vtop[-1].r2 = VT_CONST;
    s1->vtop[-1].type.t = VT_INT | u;
}
#endif

#if PTR_SIZE == 4
/* build a long long from two ints */
static void lbuild(TCCState *s1, int t)
{
    gv2(s1, RC_INT, RC_INT);
    s1->vtop[-1].r2 = s1->vtop[0].r;
    s1->vtop[-1].type.t = t;
    vpop(s1);
}
#endif

/* convert stack entry to register and duplicate its value in another
   register */
static void gv_dup(TCCState *s1)
{
    int rc, t, r, r1;
    SValue sv;

    t = s1->vtop->type.t;
#if PTR_SIZE == 4
    if ((t & VT_BTYPE) == VT_LLONG) {
        if (t & VT_BITFIELD) {
            gv(s1, RC_INT);
            t = s1->vtop->type.t;
        }
        lexpand(s1);
        gv_dup(s1);
        vswap(s1);
        vrotb(s1, 3);
        gv_dup(s1);
        vrotb(s1, 4);
        /* stack: H L L1 H1 */
        lbuild(s1, t);
        vrotb(s1, 3);
        vrotb(s1, 3);
        vswap(s1);
        lbuild(s1, t);
        vswap(s1);
    } else
#endif
    {
        /* duplicate value */
        rc = RC_INT;
        sv.type.t = VT_INT;
        if (is_float(t)) {
            rc = RC_FLOAT;
#ifdef TCC_TARGET_X86_64
            if ((t & VT_BTYPE) == VT_LDOUBLE) {
                rc = RC_ST0;
            }
#endif
            sv.type.t = t;
        }
        r = gv(s1, rc);
        r1 = get_reg(s1, rc);
        sv.r = r;
        sv.c.i = 0;
        load(s1, r1, &sv); /* move r to r1 */
        vdup(s1);
        /* duplicates value */
        if (r != r1)
            s1->vtop->r = r1;
    }
}

/* Generate value test
 *
 * Generate a test for any value (jump, comparison and integers) */
ST_FUNC int gvtst(TCCState *s1, int inv, int t)
{
    int v = s1->vtop->r & VT_VALMASK;
    if (v != VT_CMP && v != VT_JMP && v != VT_JMPI) {
        vpushi(s1, 0);
        gen_op(s1, TOK_NE);
    }
    if ((s1->vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST) {
        /* constant jmp optimization */
        if ((s1->vtop->c.i != 0) != inv)
            t = gjmp(s1, t);
        s1->vtop--;
        return t;
    }
    return gtst(s1, inv, t);
}

#if PTR_SIZE == 4
/* generate CPU independent (unsigned) long long operations */
static void gen_opl(TCCState *s1, int op)
{
    int t, a, b, op1, c, i;
    int func;
    unsigned short reg_iret = REG_IRET;
    unsigned short reg_lret = REG_LRET;
    SValue tmp;

    switch(op) {
    case '/':
    case TOK_PDIV:
        func = TOK___divdi3;
        goto gen_func;
    case TOK_UDIV:
        func = TOK___udivdi3;
        goto gen_func;
    case '%':
        func = TOK___moddi3;
        goto gen_mod_func;
    case TOK_UMOD:
        func = TOK___umoddi3;
    gen_mod_func:
#ifdef TCC_ARM_EABI
        reg_iret = TREG_R2;
        reg_lret = TREG_R3;
#endif
    gen_func:
        /* call generic long long function */
        vpush_global_sym(s1, &s1->func_old_type, func);
        vrott(s1, 3);
        gfunc_call(s1, 2);
        vpushi(s1, 0);
        s1->vtop->r = reg_iret;
        s1->vtop->r2 = reg_lret;
        break;
    case '^':
    case '&':
    case '|':
    case '*':
    case '+':
    case '-':
        //pv("gen_opl A",0,2);
        t = s1->vtop->type.t;
        vswap(s1);
        lexpand(s1);
        vrotb(s1, 3);
        lexpand(s1);
        /* stack: L1 H1 L2 H2 */
        tmp = s1->vtop[0];
        s1->vtop[0] = s1->vtop[-3];
        s1->vtop[-3] = tmp;
        tmp = s1->vtop[-2];
        s1->vtop[-2] = s1->vtop[-3];
        s1->vtop[-3] = tmp;
        vswap(s1);
        /* stack: H1 H2 L1 L2 */
        //pv("gen_opl B",0,4);
        if (op == '*') {
            vpushv(s1, s1->vtop - 1);
            vpushv(s1, s1->vtop - 1);
            gen_op(s1, TOK_UMULL);
            lexpand(s1);
            /* stack: H1 H2 L1 L2 ML MH */
            for(i=0;i<4;i++)
                vrotb(s1, 6);
            /* stack: ML MH H1 H2 L1 L2 */
            tmp = s1->vtop[0];
            s1->vtop[0] = s1->vtop[-2];
            s1->vtop[-2] = tmp;
            /* stack: ML MH H1 L2 H2 L1 */
            gen_op(s1, '*');
            vrotb(s1, 3);
            vrotb(s1, 3);
            gen_op(s1, '*');
            /* stack: ML MH M1 M2 */
            gen_op(s1, '+');
            gen_op(s1, '+');
        } else if (op == '+' || op == '-') {
            /* XXX: add non carry method too (for MIPS or alpha) */
            if (op == '+')
                op1 = TOK_ADDC1;
            else
                op1 = TOK_SUBC1;
            gen_op(s1, op1);
            /* stack: H1 H2 (L1 op L2) */
            vrotb(s1, 3);
            vrotb(s1, 3);
            gen_op(s1, op1 + 1); /* TOK_xxxC2 */
        } else {
            gen_op(s1, op);
            /* stack: H1 H2 (L1 op L2) */
            vrotb(s1, 3);
            vrotb(s1, 3);
            /* stack: (L1 op L2) H1 H2 */
            gen_op(s1, op);
            /* stack: (L1 op L2) (H1 op H2) */
        }
        /* stack: L H */
        lbuild(s1, t);
        break;
    case TOK_SAR:
    case TOK_SHR:
    case TOK_SHL:
        if ((s1->vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST) {
            t = s1->vtop[-1].type.t;
            vswap(s1);
            lexpand(s1);
            vrotb(s1, 3);
            /* stack: L H shift */
            c = (int)s1->vtop->c.i;
            /* constant: simpler */
            /* NOTE: all comments are for SHL. the other cases are
               done by swapping words */
            vpop(s1);
            if (op != TOK_SHL)
                vswap(s1);
            if (c >= 32) {
                /* stack: L H */
                vpop(s1);
                if (c > 32) {
                    vpushi(s1, c - 32);
                    gen_op(s1, op);
                }
                if (op != TOK_SAR) {
                    vpushi(s1, 0);
                } else {
                    gv_dup(s1);
                    vpushi(s1, 31);
                    gen_op(s1, TOK_SAR);
                }
                vswap(s1);
            } else {
                vswap(s1);
                gv_dup(s1);
                /* stack: H L L */
                vpushi(s1, c);
                gen_op(s1, op);
                vswap(s1);
                vpushi(s1, 32 - c);
                if (op == TOK_SHL)
                    gen_op(s1, TOK_SHR);
                else
                    gen_op(s1, TOK_SHL);
                vrotb(s1, 3);
                /* stack: L L H */
                vpushi(s1, c);
                if (op == TOK_SHL)
                    gen_op(s1, TOK_SHL);
                else
                    gen_op(s1, TOK_SHR);
                gen_op(s1, '|');
            }
            if (op != TOK_SHL)
                vswap(s1);
            lbuild(s1, t);
        } else {
            /* XXX: should provide a faster fallback on x86 ? */
            switch(op) {
            case TOK_SAR:
                func = TOK___ashrdi3;
                goto gen_func;
            case TOK_SHR:
                func = TOK___lshrdi3;
                goto gen_func;
            case TOK_SHL:
                func = TOK___ashldi3;
                goto gen_func;
            }
        }
        break;
    default:
        /* compare operations */
        t = s1->vtop->type.t;
        vswap(s1);
        lexpand(s1);
        vrotb(s1, 3);
        lexpand(s1);
        /* stack: L1 H1 L2 H2 */
        tmp = s1->vtop[-1];
        s1->vtop[-1] = s1->vtop[-2];
        s1->vtop[-2] = tmp;
        /* stack: L1 L2 H1 H2 */
        /* compare high */
        op1 = op;
        /* when values are equal, we need to compare low words. since
           the jump is inverted, we invert the test too. */
        if (op1 == TOK_LT)
            op1 = TOK_LE;
        else if (op1 == TOK_GT)
            op1 = TOK_GE;
        else if (op1 == TOK_ULT)
            op1 = TOK_ULE;
        else if (op1 == TOK_UGT)
            op1 = TOK_UGE;
        a = 0;
        b = 0;
        gen_op(s1, op1);
        if (op == TOK_NE) {
            b = gvtst(s1, 0, 0);
        } else {
            a = gvtst(s1, 1, 0);
            if (op != TOK_EQ) {
                /* generate non equal test */
                vpushi(s1, TOK_NE);
                s1->vtop->r = VT_CMP;
                b = gvtst(s1, 0, 0);
            }
        }
        /* compare low. Always unsigned */
        op1 = op;
        if (op1 == TOK_LT)
            op1 = TOK_ULT;
        else if (op1 == TOK_LE)
            op1 = TOK_ULE;
        else if (op1 == TOK_GT)
            op1 = TOK_UGT;
        else if (op1 == TOK_GE)
            op1 = TOK_UGE;
        gen_op(s1, op1);
        a = gvtst(s1, 1, a);
        gsym(s1, b);
        vseti(s1, VT_JMPI, a);
        break;
    }
}
#endif

static uint64_t gen_opic_sdiv(uint64_t a, uint64_t b)
{
    uint64_t x = (a >> 63 ? -a : a) / (b >> 63 ? -b : b);
    return (a ^ b) >> 63 ? -x : x;
}

static int gen_opic_lt(uint64_t a, uint64_t b)
{
    return (a ^ (uint64_t)1 << 63) < (b ^ (uint64_t)1 << 63);
}

/* handle integer constant optimizations and various machine
   independent opt */
static void gen_opic(TCCState *s1, int op)
{
    SValue *v1 = s1->vtop - 1;
    SValue *v2 = s1->vtop;
    int t1 = v1->type.t & VT_BTYPE;
    int t2 = v2->type.t & VT_BTYPE;
    int c1 = (v1->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST;
    int c2 = (v2->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST;
    uint64_t l1 = c1 ? v1->c.i : 0;
    uint64_t l2 = c2 ? v2->c.i : 0;
    int shm = (t1 == VT_LLONG) ? 63 : 31;

    if (t1 != VT_LLONG && (PTR_SIZE != 8 || t1 != VT_PTR))
        l1 = ((uint32_t)l1 |
              (v1->type.t & VT_UNSIGNED ? 0 : -(l1 & 0x80000000)));
    if (t2 != VT_LLONG && (PTR_SIZE != 8 || t2 != VT_PTR))
        l2 = ((uint32_t)l2 |
              (v2->type.t & VT_UNSIGNED ? 0 : -(l2 & 0x80000000)));

    if (c1 && c2) {
        switch(op) {
        case '+': l1 += l2; break;
        case '-': l1 -= l2; break;
        case '&': l1 &= l2; break;
        case '^': l1 ^= l2; break;
        case '|': l1 |= l2; break;
        case '*': l1 *= l2; break;

        case TOK_PDIV:
        case '/':
        case '%':
        case TOK_UDIV:
        case TOK_UMOD:
            /* if division by zero, generate explicit division */
            if (l2 == 0) {
                if (s1->const_wanted)
                    tcc_error(s1, "division by zero in constant");
                goto general_case;
            }
            switch(op) {
            default: l1 = gen_opic_sdiv(l1, l2); break;
            case '%': l1 = l1 - l2 * gen_opic_sdiv(l1, l2); break;
            case TOK_UDIV: l1 = l1 / l2; break;
            case TOK_UMOD: l1 = l1 % l2; break;
            }
            break;
        case TOK_SHL: l1 <<= (l2 & shm); break;
        case TOK_SHR: l1 >>= (l2 & shm); break;
        case TOK_SAR:
            l1 = (l1 >> 63) ? ~(~l1 >> (l2 & shm)) : l1 >> (l2 & shm);
            break;
            /* tests */
        case TOK_ULT: l1 = l1 < l2; break;
        case TOK_UGE: l1 = l1 >= l2; break;
        case TOK_EQ: l1 = l1 == l2; break;
        case TOK_NE: l1 = l1 != l2; break;
        case TOK_ULE: l1 = l1 <= l2; break;
        case TOK_UGT: l1 = l1 > l2; break;
        case TOK_LT: l1 = gen_opic_lt(l1, l2); break;
        case TOK_GE: l1 = !gen_opic_lt(l1, l2); break;
        case TOK_LE: l1 = !gen_opic_lt(l2, l1); break;
        case TOK_GT: l1 = gen_opic_lt(l2, l1); break;
            /* logical */
        case TOK_LAND: l1 = l1 && l2; break;
        case TOK_LOR: l1 = l1 || l2; break;
        default:
            goto general_case;
        }
    if (t1 != VT_LLONG && (PTR_SIZE != 8 || t1 != VT_PTR))
        l1 = ((uint32_t)l1 |
        (v1->type.t & VT_UNSIGNED ? 0 : -(l1 & 0x80000000)));
        v1->c.i = l1;
        s1->vtop--;
    } else {
        /* if commutative ops, put c2 as constant */
        if (c1 && (op == '+' || op == '&' || op == '^' ||
                   op == '|' || op == '*')) {
            vswap(s1);
            c2 = c1; //c = c1, c1 = c2, c2 = c;
            l2 = l1; //l = l1, l1 = l2, l2 = l;
        }
        if (!s1->const_wanted &&
            c1 && ((l1 == 0 &&
                    (op == TOK_SHL || op == TOK_SHR || op == TOK_SAR)) ||
                   (l1 == -1 && op == TOK_SAR))) {
            /* treat (0 << x), (0 >> x) and (-1 >> x) as constant */
            s1->vtop--;
        } else if (!s1->const_wanted &&
                   c2 && ((l2 == 0 && (op == '&' || op == '*')) ||
                          (op == '|' &&
                            (l2 == -1 || (l2 == 0xFFFFFFFF && t2 != VT_LLONG))) ||
                          (l2 == 1 && (op == '%' || op == TOK_UMOD)))) {
            /* treat (x & 0), (x * 0), (x | -1) and (x % 1) as constant */
            if (l2 == 1)
                s1->vtop->c.i = 0;
            vswap(s1);
            s1->vtop--;
        } else if (c2 && (((op == '*' || op == '/' || op == TOK_UDIV ||
                          op == TOK_PDIV) &&
                           l2 == 1) ||
                          ((op == '+' || op == '-' || op == '|' || op == '^' ||
                            op == TOK_SHL || op == TOK_SHR || op == TOK_SAR) &&
                           l2 == 0) ||
                          (op == '&' &&
                            (l2 == -1 || (l2 == 0xFFFFFFFF && t2 != VT_LLONG))))) {
            /* filter out NOP operations like x*1, x-0, x&-1... */
            s1->vtop--;
        } else if (c2 && (op == '*' || op == TOK_PDIV || op == TOK_UDIV)) {
            /* try to use shifts instead of muls or divs */
            if (l2 > 0 && (l2 & (l2 - 1)) == 0) {
                int n = -1;
                while (l2) {
                    l2 >>= 1;
                    n++;
                }
                s1->vtop->c.i = n;
                if (op == '*')
                    op = TOK_SHL;
                else if (op == TOK_PDIV)
                    op = TOK_SAR;
                else
                    op = TOK_SHR;
            }
            goto general_case;
        } else if (c2 && (op == '+' || op == '-') &&
                   (((s1->vtop[-1].r & (VT_VALMASK | VT_LVAL | VT_SYM)) == (VT_CONST | VT_SYM))
                    || (s1->vtop[-1].r & (VT_VALMASK | VT_LVAL)) == VT_LOCAL)) {
            /* symbol + constant case */
            if (op == '-')
                l2 = -l2;
        l2 += s1->vtop[-1].c.i;
        /* The backends can't always deal with addends to symbols
           larger than +-1<<31.  Don't construct such.  */
        if ((int)l2 != l2)
            goto general_case;
            s1->vtop--;
            s1->vtop->c.i = l2;
        } else {
        general_case:
                /* call low level op generator */
                if (t1 == VT_LLONG || t2 == VT_LLONG ||
                    (PTR_SIZE == 8 && (t1 == VT_PTR || t2 == VT_PTR)))
                    gen_opl(s1, op);
                else
                    gen_opi(s1, op);
        }
    }
}

/* generate a floating point operation with constant propagation */
static void gen_opif(TCCState *s1, int op)
{
    int c1, c2;
    SValue *v1, *v2;
#if defined _MSC_VER && defined _AMD64_
    /* avoid bad optimization with f1 -= f2 for f1:-0.0, f2:0.0 */
    volatile
#endif
    long double f1, f2;

    v1 = s1->vtop - 1;
    v2 = s1->vtop;
    /* currently, we cannot do computations with forward symbols */
    c1 = (v1->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST;
    c2 = (v2->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST;
    if (c1 && c2) {
        if (v1->type.t == VT_FLOAT) {
            f1 = v1->c.f;
            f2 = v2->c.f;
        } else if (v1->type.t == VT_DOUBLE) {
            f1 = v1->c.d;
            f2 = v2->c.d;
        } else {
            f1 = v1->c.ld;
            f2 = v2->c.ld;
        }

        /* NOTE: we only do constant propagation if finite number (not
           NaN or infinity) (ANSI spec) */
        if (!ieee_finite(f1) || !ieee_finite(f2))
            goto general_case;

        switch(op) {
        case '+': f1 += f2; break;
        case '-': f1 -= f2; break;
        case '*': f1 *= f2; break;
        case '/':
            if (f2 == 0.0) {
                if (s1->const_wanted)
                    tcc_error(s1, "division by zero in constant");
                goto general_case;
            }
            f1 /= f2;
            break;
            /* XXX: also handles tests ? */
        default:
            goto general_case;
        }
        /* XXX: overflow test ? */
        if (v1->type.t == VT_FLOAT) {
            v1->c.f = f1;
        } else if (v1->type.t == VT_DOUBLE) {
            v1->c.d = f1;
        } else {
            v1->c.ld = f1;
        }
        s1->vtop--;
    } else {
    general_case:
        gen_opf(s1, op);
    }
}

static int pointed_size(CType *type)
{
    int align;
    return type_size(pointed_type(type), &align);
}

static void vla_runtime_pointed_size(TCCState *s1, CType *type)
{
    int align;
    vla_runtime_type_size(s1, pointed_type(type), &align);
}

static inline int is_null_pointer(SValue *p)
{
    if ((p->r & (VT_VALMASK | VT_LVAL | VT_SYM)) != VT_CONST)
        return 0;
    return ((p->type.t & VT_BTYPE) == VT_INT && (uint32_t)p->c.i == 0) ||
        ((p->type.t & VT_BTYPE) == VT_LLONG && p->c.i == 0) ||
        ((p->type.t & VT_BTYPE) == VT_PTR &&
         (PTR_SIZE == 4 ? (uint32_t)p->c.i == 0 : p->c.i == 0));
}

static inline int is_integer_btype(int bt)
{
    return (bt == VT_BYTE || bt == VT_SHORT ||
            bt == VT_INT || bt == VT_LLONG);
}

/* check types for comparison or subtraction of pointers */
static void check_comparison_pointer_types(TCCState *s1, SValue *p1, SValue *p2, int op)
{
    CType *type1, *type2, tmp_type1, tmp_type2;
    int bt1, bt2;

    /* null pointers are accepted for all comparisons as gcc */
    if (is_null_pointer(p1) || is_null_pointer(p2))
        return;
    type1 = &p1->type;
    type2 = &p2->type;
    bt1 = type1->t & VT_BTYPE;
    bt2 = type2->t & VT_BTYPE;
    /* accept comparison between pointer and integer with a warning */
    if ((is_integer_btype(bt1) || is_integer_btype(bt2)) && op != '-') {
        if (op != TOK_LOR && op != TOK_LAND )
            tcc_warning(s1, "comparison between pointer and integer");
        return;
    }

    /* both must be pointers or implicit function pointers */
    if (bt1 == VT_PTR) {
        type1 = pointed_type(type1);
    } else if (bt1 != VT_FUNC)
        goto invalid_operands;

    if (bt2 == VT_PTR) {
        type2 = pointed_type(type2);
    } else if (bt2 != VT_FUNC) {
    invalid_operands:
        tcc_error(s1, "invalid operands to binary %s", get_tok_str(s1, op, NULL));
    }
    if ((type1->t & VT_BTYPE) == VT_VOID ||
        (type2->t & VT_BTYPE) == VT_VOID)
        return;
    tmp_type1 = *type1;
    tmp_type2 = *type2;
    tmp_type1.t &= ~(VT_DEFSIGN | VT_UNSIGNED | VT_CONSTANT | VT_VOLATILE);
    tmp_type2.t &= ~(VT_DEFSIGN | VT_UNSIGNED | VT_CONSTANT | VT_VOLATILE);
    if (!is_compatible_types(&tmp_type1, &tmp_type2)) {
        /* gcc-like error if '-' is used */
        if (op == '-')
            goto invalid_operands;
        else
            tcc_warning(s1, "comparison of distinct pointer types lacks a cast");
    }
}

/* generic gen_op: handles types problems */
ST_FUNC void gen_op(TCCState *s1, int op)
{
    int u, t1, t2, bt1, bt2, t;
    CType type1;

redo:
    t1 = s1->vtop[-1].type.t;
    t2 = s1->vtop[0].type.t;
    bt1 = t1 & VT_BTYPE;
    bt2 = t2 & VT_BTYPE;

    if (bt1 == VT_STRUCT || bt2 == VT_STRUCT) {
        tcc_error(s1, "operation on a struct");
    } else if (bt1 == VT_FUNC || bt2 == VT_FUNC) {
    if (bt2 == VT_FUNC) {
        mk_pointer(s1, &s1->vtop->type);
        gaddrof(s1);
    }
    if (bt1 == VT_FUNC) {
        vswap(s1);
        mk_pointer(s1, &s1->vtop->type);
        gaddrof(s1);
        vswap(s1);
    }
    goto redo;
    } else if (bt1 == VT_PTR || bt2 == VT_PTR) {
        /* at least one operand is a pointer */
        /* relational op: must be both pointers */
        if (op >= TOK_ULT && op <= TOK_LOR) {
            check_comparison_pointer_types(s1, s1->vtop - 1, s1->vtop, op);
            /* pointers are handled are unsigned */
#if PTR_SIZE == 8
            t = VT_LLONG | VT_UNSIGNED;
#else
            t = VT_INT | VT_UNSIGNED;
#endif
            goto std_op;
        }
        /* if both pointers, then it must be the '-' op */
        if (bt1 == VT_PTR && bt2 == VT_PTR) {
            if (op != '-')
                tcc_error(s1, "cannot use pointers here");
            check_comparison_pointer_types(s1, s1->vtop - 1, s1->vtop, op);
            /* XXX: check that types are compatible */
            if (s1->vtop[-1].type.t & VT_VLA) {
                vla_runtime_pointed_size(s1, &s1->vtop[-1].type);
            } else {
                vpushi(s1, pointed_size(&s1->vtop[-1].type));
            }
            vrott(s1, 3);
            gen_opic(s1, op);
            s1->vtop->type.t = s1->ptrdiff_type.t;
            vswap(s1);
            gen_op(s1, TOK_PDIV);
        } else {
            /* exactly one pointer : must be '+' or '-'. */
            if (op != '-' && op != '+')
                tcc_error(s1, "cannot use pointers here");
            /* Put pointer as first operand */
            if (bt2 == VT_PTR) {
                vswap(s1);
                t = t1, t1 = t2, t2 = t;
            }
#if PTR_SIZE == 4
            if ((s1->vtop[0].type.t & VT_BTYPE) == VT_LLONG)
                /* XXX: truncate here because gen_opl can't handle ptr + long long */
                gen_cast_s(s1, VT_INT);
#endif
            type1 = s1->vtop[-1].type;
            type1.t &= ~VT_ARRAY;
            if (s1->vtop[-1].type.t & VT_VLA)
                vla_runtime_pointed_size(s1, &s1->vtop[-1].type);
            else {
                u = pointed_size(&s1->vtop[-1].type);
                if (u < 0)
                    tcc_error(s1, "unknown array element size");
#if PTR_SIZE == 8
                vpushll(s1, u);
#else
                /* XXX: cast to int ? (long long case) */
                vpushi(s1, u);
#endif
            }
            gen_op(s1, '*');
            gen_opic(s1, op);
            /* put again type if gen_opic() swaped operands */
            s1->vtop->type = type1;
        }
    } else if (is_float(bt1) || is_float(bt2)) {
        /* compute bigger type and do implicit casts */
        if (bt1 == VT_LDOUBLE || bt2 == VT_LDOUBLE) {
            t = VT_LDOUBLE;
        } else if (bt1 == VT_DOUBLE || bt2 == VT_DOUBLE) {
            t = VT_DOUBLE;
        } else {
            t = VT_FLOAT;
        }
        /* floats can only be used for a few operations */
        if (op != '+' && op != '-' && op != '*' && op != '/' &&
            (op < TOK_ULT || op > TOK_GT))
            tcc_error(s1, "invalid operands for binary operation");
        goto std_op;
    } else if (op == TOK_SHR || op == TOK_SAR || op == TOK_SHL) {
        t = bt1 == VT_LLONG ? VT_LLONG : VT_INT;
        if ((t1 & (VT_BTYPE | VT_UNSIGNED | VT_BITFIELD)) == (t | VT_UNSIGNED))
          t |= VT_UNSIGNED;
        t |= (VT_LONG & t1);
        goto std_op;
    } else if (bt1 == VT_LLONG || bt2 == VT_LLONG) {
        /* cast to biggest op */
        t = VT_LLONG | VT_LONG;
        if (bt1 == VT_LLONG)
            t &= t1;
        if (bt2 == VT_LLONG)
            t &= t2;
        /* convert to unsigned if it does not fit in a long long */
        if ((t1 & (VT_BTYPE | VT_UNSIGNED | VT_BITFIELD)) == (VT_LLONG | VT_UNSIGNED) ||
            (t2 & (VT_BTYPE | VT_UNSIGNED | VT_BITFIELD)) == (VT_LLONG | VT_UNSIGNED))
            t |= VT_UNSIGNED;
        goto std_op;
    } else {
        /* integer operations */
        t = VT_INT | (VT_LONG & (t1 | t2));
        /* convert to unsigned if it does not fit in an integer */
        if ((t1 & (VT_BTYPE | VT_UNSIGNED | VT_BITFIELD)) == (VT_INT | VT_UNSIGNED) ||
            (t2 & (VT_BTYPE | VT_UNSIGNED | VT_BITFIELD)) == (VT_INT | VT_UNSIGNED))
            t |= VT_UNSIGNED;
    std_op:
        /* XXX: currently, some unsigned operations are explicit, so
           we modify them here */
        if (t & VT_UNSIGNED) {
            if (op == TOK_SAR)
                op = TOK_SHR;
            else if (op == '/')
                op = TOK_UDIV;
            else if (op == '%')
                op = TOK_UMOD;
            else if (op == TOK_LT)
                op = TOK_ULT;
            else if (op == TOK_GT)
                op = TOK_UGT;
            else if (op == TOK_LE)
                op = TOK_ULE;
            else if (op == TOK_GE)
                op = TOK_UGE;
        }
        vswap(s1);
        type1.t = t;
        type1.ref = NULL;
        gen_cast(s1, &type1);
        vswap(s1);
        /* special case for shifts and long long: we keep the shift as
           an integer */
        if (op == TOK_SHR || op == TOK_SAR || op == TOK_SHL)
            type1.t = VT_INT;
        gen_cast(s1, &type1);
        if (is_float(t))
            gen_opif(s1, op);
        else
            gen_opic(s1, op);
        if (op >= TOK_ULT && op <= TOK_GT) {
            /* relational op: the result is an int */
            s1->vtop->type.t = VT_INT;
        } else {
            s1->vtop->type.t = t;
        }
    }
    // Make sure that we have converted to an rvalue:
    if (s1->vtop->r & VT_LVAL)
        gv(s1, is_float(s1->vtop->type.t & VT_BTYPE) ? RC_FLOAT : RC_INT);
}

#ifndef TCC_TARGET_ARM
/* generic itof for unsigned long long case */
static void gen_cvt_itof1(TCCState *s1, int t)
{
#ifdef TCC_TARGET_ARM64
    gen_cvt_itof(s1, t);
#else
    if ((s1->vtop->type.t & (VT_BTYPE | VT_UNSIGNED)) ==
        (VT_LLONG | VT_UNSIGNED)) {

        if (t == VT_FLOAT)
            vpush_global_sym(s1, &s1->func_old_type, TOK___floatundisf);
#if LDOUBLE_SIZE != 8
        else if (t == VT_LDOUBLE)
            vpush_global_sym(s1, &s1->func_old_type, TOK___floatundixf);
#endif
        else
            vpush_global_sym(s1, &s1->func_old_type, TOK___floatundidf);
        vrott(s1, 2);
        gfunc_call(s1, 1);
        vpushi(s1, 0);
        s1->vtop->r = reg_fret(t);
    } else {
        gen_cvt_itof(s1, t);
    }
#endif
}
#endif

/* generic ftoi for unsigned long long case */
static void gen_cvt_ftoi1(TCCState *s1, int t)
{
#ifdef TCC_TARGET_ARM64
    gen_cvt_ftoi(s1, t);
#else
    int st;

    if (t == (VT_LLONG | VT_UNSIGNED)) {
        /* not handled natively */
        st = s1->vtop->type.t & VT_BTYPE;
        if (st == VT_FLOAT)
            vpush_global_sym(s1, &s1->func_old_type, TOK___fixunssfdi);
#if LDOUBLE_SIZE != 8
        else if (st == VT_LDOUBLE)
            vpush_global_sym(s1, &s1->func_old_type, TOK___fixunsxfdi);
#endif
        else
            vpush_global_sym(s1, &s1->func_old_type, TOK___fixunsdfdi);
        vrott(s1, 2);
        gfunc_call(s1, 1);
        vpushi(s1, 0);
        s1->vtop->r = REG_IRET;
        s1->vtop->r2 = REG_LRET;
    } else {
        gen_cvt_ftoi(s1, t);
    }
#endif
}

/* force char or short cast */
static void force_charshort_cast(TCCState *s1, int t)
{
    int bits, dbt;

    /* cannot cast static initializers */
    if (STATIC_DATA_WANTED)
    return;

    dbt = t & VT_BTYPE;
    /* XXX: add optimization if lvalue : just change type and offset */
    if (dbt == VT_BYTE)
        bits = 8;
    else
        bits = 16;
    if (t & VT_UNSIGNED) {
        vpushi(s1, (1 << bits) - 1);
        gen_op(s1, '&');
    } else {
        if ((s1->vtop->type.t & VT_BTYPE) == VT_LLONG)
            bits = 64 - bits;
        else
            bits = 32 - bits;
        vpushi(s1, bits);
        gen_op(s1, TOK_SHL);
        /* result must be signed or the SAR is converted to an SHL
           This was not the case when "t" was a signed short
           and the last value on the stack was an unsigned int */
        s1->vtop->type.t &= ~VT_UNSIGNED;
        vpushi(s1, bits);
        gen_op(s1, TOK_SAR);
    }
}

/* cast 's1->vtop' to 'type'. Casting to bitfields is forbidden. */
static void gen_cast_s(TCCState *s1, int t)
{
    CType type;
    type.t = t;
    type.ref = NULL;
    gen_cast(s1, &type);
}

static void gen_cast(TCCState *s1, CType *type)
{
    int sbt, dbt, sf, df, c, p;

    /* special delayed cast for char/short */
    /* XXX: in some cases (multiple cascaded casts), it may still
       be incorrect */
    if (s1->vtop->r & VT_MUSTCAST) {
        s1->vtop->r &= ~VT_MUSTCAST;
        force_charshort_cast(s1, s1->vtop->type.t);
    }

    /* bitfields first get cast to ints */
    if (s1->vtop->type.t & VT_BITFIELD) {
        gv(s1, RC_INT);
    }

    dbt = type->t & (VT_BTYPE | VT_UNSIGNED);
    sbt = s1->vtop->type.t & (VT_BTYPE | VT_UNSIGNED);

    if (sbt != dbt) {
        sf = is_float(sbt);
        df = is_float(dbt);
        c = (s1->vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST;
        p = (s1->vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == (VT_CONST | VT_SYM);
#if !defined TCC_IS_NATIVE && !defined TCC_IS_NATIVE_387
        c &= dbt != VT_LDOUBLE;
#endif
        if (c) {
            /* constant case: we can do it now */
            /* XXX: in ISOC, cannot do it if error in convert */
            if (sbt == VT_FLOAT)
                s1->vtop->c.ld = s1->vtop->c.f;
            else if (sbt == VT_DOUBLE)
                s1->vtop->c.ld = s1->vtop->c.d;

            if (df) {
                if ((sbt & VT_BTYPE) == VT_LLONG) {
                    if ((sbt & VT_UNSIGNED) || !(s1->vtop->c.i >> 63))
                        s1->vtop->c.ld = s1->vtop->c.i;
                    else
                        s1->vtop->c.ld = -(long double)-s1->vtop->c.i;
                } else if(!sf) {
                    if ((sbt & VT_UNSIGNED) || !(s1->vtop->c.i >> 31))
                        s1->vtop->c.ld = (uint32_t)s1->vtop->c.i;
                    else
                        s1->vtop->c.ld = -(long double)-(uint32_t)s1->vtop->c.i;
                }

                if (dbt == VT_FLOAT)
                    s1->vtop->c.f = (float)s1->vtop->c.ld;
                else if (dbt == VT_DOUBLE)
                    s1->vtop->c.d = (double)s1->vtop->c.ld;
            } else if (sf && dbt == (VT_LLONG|VT_UNSIGNED)) {
                s1->vtop->c.i = s1->vtop->c.ld;
            } else if (sf && dbt == VT_BOOL) {
                s1->vtop->c.i = (s1->vtop->c.ld != 0);
            } else {
                if(sf)
                    s1->vtop->c.i = s1->vtop->c.ld;
                else if (sbt == (VT_LLONG|VT_UNSIGNED))
                    ;
                else if (sbt & VT_UNSIGNED)
                    s1->vtop->c.i = (uint32_t)s1->vtop->c.i;
#if PTR_SIZE == 8
                else if (sbt == VT_PTR)
                    ;
#endif
                else if (sbt != VT_LLONG)
                    s1->vtop->c.i = ((uint32_t)s1->vtop->c.i |
                                  -(s1->vtop->c.i & 0x80000000));

                if (dbt == (VT_LLONG|VT_UNSIGNED))
                    ;
                else if (dbt == VT_BOOL)
                    s1->vtop->c.i = (s1->vtop->c.i != 0);
#if PTR_SIZE == 8
                else if (dbt == VT_PTR)
                    ;
#endif
                else if (dbt != VT_LLONG) {
                    uint32_t m = ((dbt & VT_BTYPE) == VT_BYTE ? 0xff :
                                  (dbt & VT_BTYPE) == VT_SHORT ? 0xffff :
                                  0xffffffff);
                    s1->vtop->c.i &= m;
                    if (!(dbt & VT_UNSIGNED))
                        s1->vtop->c.i |= -(s1->vtop->c.i & ((m >> 1) + 1));
                }
            }
        } else if (p && dbt == VT_BOOL) {
            s1->vtop->r = VT_CONST;
            s1->vtop->c.i = 1;
        } else {
            /* non constant case: generate code */
            if (sf && df) {
                /* convert from fp to fp */
                gen_cvt_ftof(s1, dbt);
            } else if (df) {
                /* convert int to fp */
                gen_cvt_itof1(s1, dbt);
            } else if (sf) {
                /* convert fp to int */
                if (dbt == VT_BOOL) {
                     vpushi(s1, 0);
                     gen_op(s1, TOK_NE);
                } else {
                    /* we handle char/short/etc... with generic code */
                    if (dbt != (VT_INT | VT_UNSIGNED) &&
                        dbt != (VT_LLONG | VT_UNSIGNED) &&
                        dbt != VT_LLONG)
                        dbt = VT_INT;
                    gen_cvt_ftoi1(s1, dbt);
                    if (dbt == VT_INT && (type->t & (VT_BTYPE | VT_UNSIGNED)) != dbt) {
                        /* additional cast for char/short... */
                        s1->vtop->type.t = dbt;
                        gen_cast(s1, type);
                    }
                }
#if PTR_SIZE == 4
            } else if ((dbt & VT_BTYPE) == VT_LLONG) {
                if ((sbt & VT_BTYPE) != VT_LLONG) {
                    /* scalar to long long */
                    /* machine independent conversion */
                    gv(s1, RC_INT);
                    /* generate high word */
                    if (sbt == (VT_INT | VT_UNSIGNED)) {
                        vpushi(s1, 0);
                        gv(s1, RC_INT);
                    } else {
                        if (sbt == VT_PTR) {
                            /* cast from pointer to int before we apply
                               shift operation, which pointers don't support*/
                            gen_cast_s(s1, VT_INT);
                        }
                        gv_dup(s1);
                        vpushi(s1, 31);
                        gen_op(s1, TOK_SAR);
                    }
                    /* patch second register */
                    s1->vtop[-1].r2 = s1->vtop->r;
                    vpop(s1);
                }
#else
            } else if ((dbt & VT_BTYPE) == VT_LLONG ||
                       (dbt & VT_BTYPE) == VT_PTR ||
                       (dbt & VT_BTYPE) == VT_FUNC) {
                if ((sbt & VT_BTYPE) != VT_LLONG &&
                    (sbt & VT_BTYPE) != VT_PTR &&
                    (sbt & VT_BTYPE) != VT_FUNC) {
                    /* need to convert from 32bit to 64bit */
                    gv(s1, RC_INT);
                    if (sbt != (VT_INT | VT_UNSIGNED)) {
#if defined(TCC_TARGET_ARM64)
                        gen_cvt_sxtw();
#elif defined(TCC_TARGET_X86_64)
                        int r = gv(s1, RC_INT);
                        /* x86_64 specific: movslq */
                        o(s1, 0x6348);
                        o(s1, 0xc0 + (REG_VALUE(r) << 3) + REG_VALUE(r));
#else
#error
#endif
                    }
                }
#endif
            } else if (dbt == VT_BOOL) {
                /* scalar to bool */
                vpushi(s1, 0);
                gen_op(s1, TOK_NE);
            } else if ((dbt & VT_BTYPE) == VT_BYTE ||
                       (dbt & VT_BTYPE) == VT_SHORT) {
                if (sbt == VT_PTR) {
                    s1->vtop->type.t = VT_INT;
                    tcc_warning(s1, "nonportable conversion from pointer to char/short");
                }
                force_charshort_cast(s1, dbt);
#if PTR_SIZE == 4
            } else if ((dbt & VT_BTYPE) == VT_INT) {
                /* scalar to int */
                if ((sbt & VT_BTYPE) == VT_LLONG) {
                    /* from long long: just take low order word */
                    lexpand(s1);
                    vpop(s1);
                }
                /* if lvalue and single word type, nothing to do because
                   the lvalue already contains the real type size (see
                   VT_LVAL_xxx constants) */
#endif
            }
        }
    } else if ((dbt & VT_BTYPE) == VT_PTR && !(s1->vtop->r & VT_LVAL)) {
        /* if we are casting between pointer types,
           we must update the VT_LVAL_xxx size */
        s1->vtop->r = (s1->vtop->r & ~VT_LVAL_TYPE)
                  | (lvalue_type(type->ref->type.t) & VT_LVAL_TYPE);
    }
    s1->vtop->type = *type;
}

/* return type size as known at compile time. Put alignment at 'a' */
ST_FUNC int type_size(CType *type, int *a)
{
    Sym *s;
    int bt;

    bt = type->t & VT_BTYPE;
    if (bt == VT_STRUCT) {
        /* struct/union */
        s = type->ref;
        *a = s->r;
        return s->c;
    } else if (bt == VT_PTR) {
        if (type->t & VT_ARRAY) {
            int ts;

            s = type->ref;
            ts = type_size(&s->type, a);

            if (ts < 0 && s->c < 0)
                ts = -ts;

            return ts * s->c;
        } else {
            *a = PTR_SIZE;
            return PTR_SIZE;
        }
    } else if (IS_ENUM(type->t) && type->ref->c == -1) {
        return -1; /* incomplete enum */
    } else if (bt == VT_LDOUBLE) {
        *a = LDOUBLE_ALIGN;
        return LDOUBLE_SIZE;
    } else if (bt == VT_DOUBLE || bt == VT_LLONG) {
#ifdef TCC_TARGET_I386
#ifdef TCC_TARGET_PE
        *a = 8;
#else
        *a = 4;
#endif
#elif defined(TCC_TARGET_ARM)
#ifdef TCC_ARM_EABI
        *a = 8;
#else
        *a = 4;
#endif
#else
        *a = 8;
#endif
        return 8;
    } else if (bt == VT_INT || bt == VT_FLOAT) {
        *a = 4;
        return 4;
    } else if (bt == VT_SHORT) {
        *a = 2;
        return 2;
    } else if (bt == VT_QLONG || bt == VT_QFLOAT) {
        *a = 8;
        return 16;
    } else {
        /* char, void, function, _Bool */
        *a = 1;
        return 1;
    }
}

/* push type size as known at runtime time on top of value stack. Put
   alignment at 'a' */
ST_FUNC void vla_runtime_type_size(TCCState *s1, CType *type, int *a)
{
    if (type->t & VT_VLA) {
        type_size(&type->ref->type, a);
        vset(s1, &s1->int_type, VT_LOCAL|VT_LVAL, type->ref->c);
    } else {
        vpushi(s1, type_size(type, a));
    }
}

static void vla_sp_restore(TCCState *s1) {
    if (s1->vlas_in_scope) {
        gen_vla_sp_restore(s1, s1->vla_sp_loc);
    }
}

static void vla_sp_restore_root(TCCState *s1) {
    if (s1->vlas_in_scope) {
        gen_vla_sp_restore(s1, s1->vla_sp_root_loc);
    }
}

/* return the pointed type of t */
static inline CType *pointed_type(CType *type)
{
    return &type->ref->type;
}

/* modify type so that its it is a pointer to type. */
ST_FUNC void mk_pointer(TCCState *s1, CType *type)
{
    Sym *s;
    s = sym_push(s1, SYM_FIELD, type, 0, -1);
    type->t = VT_PTR | (type->t & VT_STORAGE);
    type->ref = s;
}

/* compare function types. OLD functions match any new functions */
static int is_compatible_func(CType *type1, CType *type2)
{
    Sym *s1, *s2;

    s1 = type1->ref;
    s2 = type2->ref;
    if (!is_compatible_types(&s1->type, &s2->type))
        return 0;
    /* check func_call */
    if (s1->f.func_call != s2->f.func_call)
        return 0;
    /* XXX: not complete */
    if (s1->f.func_type == FUNC_OLD || s2->f.func_type == FUNC_OLD)
        return 1;
    if (s1->f.func_type != s2->f.func_type)
        return 0;
    while (s1 != NULL) {
        if (s2 == NULL)
            return 0;
        if (!is_compatible_unqualified_types(&s1->type, &s2->type))
            return 0;
        s1 = s1->next;
        s2 = s2->next;
    }
    if (s2)
        return 0;
    return 1;
}

/* return true if type1 and type2 are the same.  If unqualified is
   true, qualifiers on the types are ignored.

   - enums are not checked as gcc __builtin_types_compatible_p ()
 */
static int compare_types(CType *type1, CType *type2, int unqualified)
{
    int bt1, t1, t2;

    t1 = type1->t & VT_TYPE;
    t2 = type2->t & VT_TYPE;
    if (unqualified) {
        /* strip qualifiers before comparing */
        t1 &= ~(VT_CONSTANT | VT_VOLATILE);
        t2 &= ~(VT_CONSTANT | VT_VOLATILE);
    }

    /* Default Vs explicit signedness only matters for char */
    if ((t1 & VT_BTYPE) != VT_BYTE) {
        t1 &= ~VT_DEFSIGN;
        t2 &= ~VT_DEFSIGN;
    }
    /* XXX: bitfields ? */
    if (t1 != t2)
        return 0;
    /* test more complicated cases */
    bt1 = t1 & VT_BTYPE;
    if (bt1 == VT_PTR) {
        type1 = pointed_type(type1);
        type2 = pointed_type(type2);
        return is_compatible_types(type1, type2);
    } else if (bt1 == VT_STRUCT) {
        return (type1->ref == type2->ref);
    } else if (bt1 == VT_FUNC) {
        return is_compatible_func(type1, type2);
    } else {
        return 1;
    }
}

/* return true if type1 and type2 are exactly the same (including
   qualifiers).
*/
static int is_compatible_types(CType *type1, CType *type2)
{
    return compare_types(type1,type2,0);
}

/* return true if type1 and type2 are the same (ignoring qualifiers).
*/
static int is_compatible_unqualified_types(CType *type1, CType *type2)
{
    return compare_types(type1,type2,1);
}

/* print a type. If 'varstr' is not NULL, then the variable is also
   printed in the type */
/* XXX: union */
/* XXX: add array and function pointers */
ST_FUNC void type_to_str(TCCState *s1, char *buf, int buf_size,
                         CType *type, const char *varstr)
{
    int bt, v, t;
    Sym *s, *sa;
    char buf1[256];
    const char *tstr;

    t = type->t;
    bt = t & VT_BTYPE;
    buf[0] = '\0';

    if (t & VT_EXTERN)
        pstrcat(buf, buf_size, "extern ");
    if (t & VT_STATIC)
        pstrcat(buf, buf_size, "static ");
    if (t & VT_TYPEDEF)
        pstrcat(buf, buf_size, "typedef ");
    if (t & VT_INLINE)
        pstrcat(buf, buf_size, "inline ");
    if (t & VT_VOLATILE)
        pstrcat(buf, buf_size, "volatile ");
    if (t & VT_CONSTANT)
        pstrcat(buf, buf_size, "const ");

    if (((t & VT_DEFSIGN) && bt == VT_BYTE)
        || ((t & VT_UNSIGNED)
            && (bt == VT_SHORT || bt == VT_INT || bt == VT_LLONG)
            && !IS_ENUM(t)
            ))
        pstrcat(buf, buf_size, (t & VT_UNSIGNED) ? "unsigned " : "signed ");

    buf_size -= strlen(buf);
    buf += strlen(buf);

    switch(bt) {
    case VT_VOID:
        tstr = "void";
        goto add_tstr;
    case VT_BOOL:
        tstr = "_Bool";
        goto add_tstr;
    case VT_BYTE:
        tstr = "char";
        goto add_tstr;
    case VT_SHORT:
        tstr = "short";
        goto add_tstr;
    case VT_INT:
        tstr = "int";
        goto maybe_long;
    case VT_LLONG:
        tstr = "long long";
    maybe_long:
        if (t & VT_LONG)
            tstr = "long";
        if (!IS_ENUM(t))
            goto add_tstr;
        tstr = "enum ";
        goto tstruct;
    case VT_FLOAT:
        tstr = "float";
        goto add_tstr;
    case VT_DOUBLE:
        tstr = "double";
        goto add_tstr;
    case VT_LDOUBLE:
        tstr = "long double";
    add_tstr:
        pstrcat(buf, buf_size, tstr);
        break;
    case VT_STRUCT:
        tstr = "struct ";
        if (IS_UNION(t))
            tstr = "union ";
    tstruct:
        pstrcat(buf, buf_size, tstr);
        v = type->ref->v & ~SYM_STRUCT;
        if (v >= SYM_FIRST_ANOM)
            pstrcat(buf, buf_size, "<anonymous>");
        else
            pstrcat(buf, buf_size, get_tok_str(s1, v, NULL));
        break;
    case VT_FUNC:
        s = type->ref;
        type_to_str(s1, buf, buf_size, &s->type, varstr);
        pstrcat(buf, buf_size, "(");
        sa = s->next;
        while (sa != NULL) {
            type_to_str(s1, buf1, sizeof(buf1), &sa->type, NULL);
            pstrcat(buf, buf_size, buf1);
            sa = sa->next;
            if (sa)
                pstrcat(buf, buf_size, ", ");
        }
        pstrcat(buf, buf_size, ")");
        goto no_var;
    case VT_PTR:
        s = type->ref;
        if (t & VT_ARRAY) {
            snprintf(buf1, sizeof(buf1), "%s[%d]", varstr ? varstr : "", s->c);
            type_to_str(s1, buf, buf_size, &s->type, buf1);
            goto no_var;
        }
        pstrcpy(buf1, sizeof(buf1), "*");
        if (t & VT_CONSTANT)
            pstrcat(buf1, buf_size, "const ");
        if (t & VT_VOLATILE)
            pstrcat(buf1, buf_size, "volatile ");
        if (varstr)
            pstrcat(buf1, sizeof(buf1), varstr);
        type_to_str(s1, buf, buf_size, &s->type, buf1);
        goto no_var;
    }
    if (varstr) {
        pstrcat(buf, buf_size, " ");
        pstrcat(buf, buf_size, varstr);
    }
 no_var: ;
}

/* verify type compatibility to store s1->vtop in 'dt' type, and generate
   casts if needed. */
static void gen_assign_cast(TCCState *s1, CType *dt)
{
    CType *st, *type1, *type2;
    char buf1[256], buf2[256];
    int dbt, sbt;

    st = &s1->vtop->type; /* source type */
    dbt = dt->t & VT_BTYPE;
    sbt = st->t & VT_BTYPE;
    if (sbt == VT_VOID || dbt == VT_VOID) {
    if (sbt == VT_VOID && dbt == VT_VOID)
        ; /*
          It is Ok if both are void
          A test program:
            void func1() {}
        void func2() {
          return func1();
        }
          gcc accepts this program
          */
    else
            tcc_error(s1, "cannot cast from/to void");
    }
    if (dt->t & VT_CONSTANT)
        tcc_warning(s1, "assignment of read-only location");
    switch(dbt) {
    case VT_PTR:
        /* special cases for pointers */
        /* '0' can also be a pointer */
        if (is_null_pointer(s1->vtop))
            goto type_ok;
        /* accept implicit pointer to integer cast with warning */
        if (is_integer_btype(sbt)) {
            tcc_warning(s1, "assignment makes pointer from integer without a cast");
            goto type_ok;
        }
        type1 = pointed_type(dt);
        /* a function is implicitly a function pointer */
        if (sbt == VT_FUNC) {
            if ((type1->t & VT_BTYPE) != VT_VOID &&
                !is_compatible_types(pointed_type(dt), st))
                tcc_warning(s1, "assignment from incompatible pointer type");
            goto type_ok;
        }
        if (sbt != VT_PTR)
            goto error;
        type2 = pointed_type(st);
        if ((type1->t & VT_BTYPE) == VT_VOID ||
            (type2->t & VT_BTYPE) == VT_VOID) {
            /* void * can match anything */
        } else {
            //printf("types %08x %08x\n", type1->t, type2->t);
            /* exact type match, except for qualifiers */
            if (!is_compatible_unqualified_types(type1, type2)) {
        /* Like GCC don't warn by default for merely changes
           in pointer target signedness.  Do warn for different
           base types, though, in particular for unsigned enums
           and signed int targets.  */
        if ((type1->t & (VT_BTYPE|VT_LONG)) != (type2->t & (VT_BTYPE|VT_LONG))
                    || IS_ENUM(type1->t) || IS_ENUM(type2->t)
                    )
            tcc_warning(s1, "assignment from incompatible pointer type");
        }
        }
        /* check const and volatile */
        if ((!(type1->t & VT_CONSTANT) && (type2->t & VT_CONSTANT)) ||
            (!(type1->t & VT_VOLATILE) && (type2->t & VT_VOLATILE)))
            tcc_warning(s1, "assignment discards qualifiers from pointer target type");
        break;
    case VT_BYTE:
    case VT_SHORT:
    case VT_INT:
    case VT_LLONG:
        if (sbt == VT_PTR || sbt == VT_FUNC) {
            tcc_warning(s1, "assignment makes integer from pointer without a cast");
        } else if (sbt == VT_STRUCT) {
            goto case_VT_STRUCT;
        }
        /* XXX: more tests */
        break;
    case VT_STRUCT:
    case_VT_STRUCT:
        if (!is_compatible_unqualified_types(dt, st)) {
        error:
            type_to_str(s1, buf1, sizeof(buf1), st, NULL);
            type_to_str(s1, buf2, sizeof(buf2), dt, NULL);
            tcc_error(s1, "cannot cast '%s' to '%s'", buf1, buf2);
        }
        break;
    }
 type_ok:
    gen_cast(s1, dt);
}

/* store s1->vtop in lvalue pushed on stack */
ST_FUNC void vstore(TCCState *s1)
{
    int sbt, dbt, ft, r, t, size, align, bit_size, bit_pos, rc, delayed_cast;

    ft = s1->vtop[-1].type.t;
    sbt = s1->vtop->type.t & VT_BTYPE;
    dbt = ft & VT_BTYPE;
    if ((((sbt == VT_INT || sbt == VT_SHORT) && dbt == VT_BYTE) ||
         (sbt == VT_INT && dbt == VT_SHORT))
    && !(s1->vtop->type.t & VT_BITFIELD)) {
        /* optimize char/short casts */
        delayed_cast = VT_MUSTCAST;
        s1->vtop->type.t = ft & VT_TYPE;
        /* XXX: factorize */
        if (ft & VT_CONSTANT)
            tcc_warning(s1, "assignment of read-only location");
    } else {
        delayed_cast = 0;
        if (!(ft & VT_BITFIELD))
            gen_assign_cast(s1, &s1->vtop[-1].type);
    }

    if (sbt == VT_STRUCT) {
        /* if structure, only generate pointer */
        /* structure assignment : generate memcpy */
        /* XXX: optimize if small size */
            size = type_size(&s1->vtop->type, &align);

            /* destination */
            vswap(s1);
            s1->vtop->type.t = VT_PTR;
            gaddrof(s1);

            /* address of memcpy() */
#ifdef TCC_ARM_EABI
            if(!(align & 7))
                vpush_global_sym(s1, &s1->func_old_type, TOK_memcpy8);
            else if(!(align & 3))
                vpush_global_sym(s1, &s1->func_old_type, TOK_memcpy4);
            else
#endif
            /* Use memmove, rather than memcpy, as dest and src may be same: */
            vpush_global_sym(s1, &s1->func_old_type, TOK_memmove);

            vswap(s1);
            /* source */
            vpushv(s1, s1->vtop - 2);
            s1->vtop->type.t = VT_PTR;
            gaddrof(s1);
            /* type size */
            vpushi(s1, size);
            gfunc_call(s1, 3);

        /* leave source on stack */
    } else if (ft & VT_BITFIELD) {
        /* bitfield store handling */

        /* save lvalue as expression result (example: s.b = s.a = n;) */
        vdup(s1), s1->vtop[-1] = s1->vtop[-2];

        bit_pos = BIT_POS(ft);
        bit_size = BIT_SIZE(ft);
        /* remove bit field info to avoid loops */
        s1->vtop[-1].type.t = ft & ~VT_STRUCT_MASK;

        if ((ft & VT_BTYPE) == VT_BOOL) {
            gen_cast(s1, &s1->vtop[-1].type);
            s1->vtop[-1].type.t = (s1->vtop[-1].type.t & ~VT_BTYPE) | (VT_BYTE | VT_UNSIGNED);
        }

        r = adjust_bf(s1->vtop - 1, bit_pos, bit_size);
        if (r == VT_STRUCT) {
            gen_cast_s(s1, (ft & VT_BTYPE) == VT_LLONG ? VT_LLONG : VT_INT);
            store_packed_bf(s1, bit_pos, bit_size);
        } else {
            unsigned long long mask = (1ULL << bit_size) - 1;
            if ((ft & VT_BTYPE) != VT_BOOL) {
                /* mask source */
                if ((s1->vtop[-1].type.t & VT_BTYPE) == VT_LLONG)
                    vpushll(s1, mask);
                else
                    vpushi(s1, (unsigned)mask);
                gen_op(s1, '&');
            }
            /* shift source */
            vpushi(s1, bit_pos);
            gen_op(s1, TOK_SHL);
            vswap(s1);
            /* duplicate destination */
            vdup(s1);
            vrott(s1, 3);
            /* load destination, mask and or with source */
            if ((s1->vtop->type.t & VT_BTYPE) == VT_LLONG)
                vpushll(s1, ~(mask << bit_pos));
            else
                vpushi(s1, ~((unsigned)mask << bit_pos));
            gen_op(s1, '&');
            gen_op(s1, '|');
            /* store result */
            vstore(s1);
            /* ... and discard */
            vpop(s1);
        }
    } else if (dbt == VT_VOID) {
        --s1->vtop;
    } else {
#ifdef CONFIG_TCC_BCHECK
            /* bound check case */
            if (s1->vtop[-1].r & VT_MUSTBOUND) {
                vswap(s1);
                gbound(s1);
                vswap(s1);
            }
#endif
            rc = RC_INT;
            if (is_float(ft)) {
                rc = RC_FLOAT;
#ifdef TCC_TARGET_X86_64
                if ((ft & VT_BTYPE) == VT_LDOUBLE) {
                    rc = RC_ST0;
                } else if ((ft & VT_BTYPE) == VT_QFLOAT) {
                    rc = RC_FRET;
                }
#endif
            }
            r = gv(s1, rc);  /* generate value */
            /* if lvalue was saved on stack, must read it */
            if ((s1->vtop[-1].r & VT_VALMASK) == VT_LLOCAL) {
                SValue sv;
                t = get_reg(s1, RC_INT);
#if PTR_SIZE == 8
                sv.type.t = VT_PTR;
#else
                sv.type.t = VT_INT;
#endif
                sv.r = VT_LOCAL | VT_LVAL;
                sv.c.i = s1->vtop[-1].c.i;
                load(s1, t, &sv);
                s1->vtop[-1].r = t | VT_LVAL;
            }
            /* two word case handling : store second register at word + 4 (or +8 for x86-64)  */
#if PTR_SIZE == 8
            if (((ft & VT_BTYPE) == VT_QLONG) || ((ft & VT_BTYPE) == VT_QFLOAT)) {
                int addr_type = VT_LLONG, load_size = 8, load_type = ((s1->vtop->type.t & VT_BTYPE) == VT_QLONG) ? VT_LLONG : VT_DOUBLE;
#else
            if ((ft & VT_BTYPE) == VT_LLONG) {
                int addr_type = VT_INT, load_size = 4, load_type = VT_INT;
#endif
                s1->vtop[-1].type.t = load_type;
                store(s1, r, s1->vtop - 1);
                vswap(s1);
                /* convert to int to increment easily */
                s1->vtop->type.t = addr_type;
                gaddrof(s1);
                vpushi(s1, load_size);
                gen_op(s1, '+');
                s1->vtop->r |= VT_LVAL;
                vswap(s1);
                s1->vtop[-1].type.t = load_type;
                /* XXX: it works because r2 is spilled last ! */
                store(s1, s1->vtop->r2, s1->vtop - 1);
            } else {
                store(s1, r, s1->vtop - 1);
            }

        vswap(s1);
        s1->vtop--; /* NOT vpop(s1) because on x86 it would flush the fp stack */
        s1->vtop->r |= delayed_cast;
    }
}

/* post defines POST/PRE add. c is the token ++ or -- */
ST_FUNC void inc(TCCState *s1, int post, int c)
{
    test_lvalue(s1);
    vdup(s1); /* save lvalue */
    if (post) {
        gv_dup(s1); /* duplicate value */
        vrotb(s1, 3);
        vrotb(s1, 3);
    }
    /* add constant */
    vpushi(s1, c - TOK_MID);
    gen_op(s1, '+');
    vstore(s1); /* store value */
    if (post)
        vpop(s1); /* if post op, return saved value */
}

ST_FUNC void parse_mult_str (TCCState *s1, CString *astr, const char *msg)
{
    /* read the string */
    if (s1->tok != TOK_STR)
        expect(s1, msg);
    cstr_new(s1, astr);
    while (s1->tok == TOK_STR) {
        /* XXX: add \0 handling too ? */
        cstr_cat(s1, astr, s1->tokc.str.data, -1);
        next(s1);
    }
    cstr_ccat(s1, astr, '\0');
}

/* If I is >= 1 and a power of two, returns log2(i)+1.
   If I is 0 returns 0.  */
static int exact_log2p1(int i)
{
  int ret;
  if (!i)
    return 0;
  for (ret = 1; i >= 1 << 8; ret += 8)
    i >>= 8;
  if (i >= 1 << 4)
    ret += 4, i >>= 4;
  if (i >= 1 << 2)
    ret += 2, i >>= 2;
  if (i >= 1 << 1)
    ret++;
  return ret;
}

/* Parse __attribute__((...)) GNUC extension. */
static void parse_attribute(TCCState *s1, AttributeDef *ad)
{
    int t, n;
    CString astr;

redo:
    if (s1->tok != TOK_ATTRIBUTE1 && s1->tok != TOK_ATTRIBUTE2)
        return;
    next(s1);
    skip(s1, '(');
    skip(s1, '(');
    while (s1->tok != ')') {
        if (s1->tok < TOK_IDENT)
            expect(s1, "attribute name");
        t = s1->tok;
        next(s1);
        switch(t) {
        case TOK_SECTION1:
        case TOK_SECTION2:
            skip(s1, '(');
        parse_mult_str(s1, &astr, "section name");
            ad->section = find_section(s1, (char *)astr.data);
            skip(s1, ')');
        cstr_free(s1, &astr);
            break;
        case TOK_ALIAS1:
        case TOK_ALIAS2:
            skip(s1, '(');
        parse_mult_str(s1, &astr, "alias(\"target\")");
            ad->alias_target = /* save string as token, for later */
              tok_alloc(s1, (char*)astr.data, astr.size-1)->tok;
            skip(s1, ')');
        cstr_free(s1, &astr);
            break;
    case TOK_VISIBILITY1:
    case TOK_VISIBILITY2:
            skip(s1, '(');
        parse_mult_str(s1, &astr,
               "visibility(\"default|hidden|internal|protected\")");
        if (!strcmp (astr.data, "default"))
            ad->a.visibility = STV_DEFAULT;
        else if (!strcmp (astr.data, "hidden"))
            ad->a.visibility = STV_HIDDEN;
        else if (!strcmp (astr.data, "internal"))
            ad->a.visibility = STV_INTERNAL;
        else if (!strcmp (astr.data, "protected"))
            ad->a.visibility = STV_PROTECTED;
        else
                expect(s1, "visibility(\"default|hidden|internal|protected\")");
            skip(s1, ')');
        cstr_free(s1, &astr);
            break;
        case TOK_ALIGNED1:
        case TOK_ALIGNED2:
            if (s1->tok == '(') {
                next(s1);
                n = expr_const(s1);
                if (n <= 0 || (n & (n - 1)) != 0)
                    tcc_error(s1, "alignment must be a positive power of two");
                skip(s1, ')');
            } else {
                n = MAX_ALIGN;
            }
            ad->a.aligned = exact_log2p1(n);
        if (n != 1 << (ad->a.aligned - 1))
          tcc_error(s1, "alignment of %d is larger than implemented", n);
            break;
        case TOK_PACKED1:
        case TOK_PACKED2:
            ad->a.packed = 1;
            break;
        case TOK_WEAK1:
        case TOK_WEAK2:
            ad->a.weak = 1;
            break;
        case TOK_UNUSED1:
        case TOK_UNUSED2:
            /* currently, no need to handle it because tcc does not
               track unused objects */
            break;
        case TOK_NORETURN1:
        case TOK_NORETURN2:
            /* currently, no need to handle it because tcc does not
               track unused objects */
            break;
        case TOK_CDECL1:
        case TOK_CDECL2:
        case TOK_CDECL3:
            ad->f.func_call = FUNC_CDECL;
            break;
        case TOK_STDCALL1:
        case TOK_STDCALL2:
        case TOK_STDCALL3:
            ad->f.func_call = FUNC_STDCALL;
            break;
#ifdef TCC_TARGET_I386
        case TOK_REGPARM1:
        case TOK_REGPARM2:
            skip(s1, '(');
            n = expr_const(s1);
            if (n > 3)
                n = 3;
            else if (n < 0)
                n = 0;
            if (n > 0)
                ad->f.func_call = FUNC_FASTCALL1 + n - 1;
            skip(s1, ')');
            break;
        case TOK_FASTCALL1:
        case TOK_FASTCALL2:
        case TOK_FASTCALL3:
            ad->f.func_call = FUNC_FASTCALLW;
            break;
#endif
        case TOK_MODE:
            skip(s1, '(');
            switch(s1->tok) {
                case TOK_MODE_DI:
                    ad->attr_mode = VT_LLONG + 1;
                    break;
                case TOK_MODE_QI:
                    ad->attr_mode = VT_BYTE + 1;
                    break;
                case TOK_MODE_HI:
                    ad->attr_mode = VT_SHORT + 1;
                    break;
                case TOK_MODE_SI:
                case TOK_MODE_word:
                    ad->attr_mode = VT_INT + 1;
                    break;
                default:
                    tcc_warning(s1, "__mode__(%s) not supported\n", get_tok_str(s1, s1->tok, NULL));
                    break;
            }
            next(s1);
            skip(s1, ')');
            break;
        case TOK_DLLEXPORT:
            ad->a.dllexport = 1;
            break;
        case TOK_DLLIMPORT:
            ad->a.dllimport = 1;
            break;
        default:
            if (s1->warn_unsupported)
                tcc_warning(s1, "'%s' attribute ignored", get_tok_str(s1, t, NULL));
            /* skip parameters */
            if (s1->tok == '(') {
                int parenthesis = 0;
                do {
                    if (s1->tok == '(')
                        parenthesis++;
                    else if (s1->tok == ')')
                        parenthesis--;
                    next(s1);
                } while (parenthesis && s1->tok != -1);
            }
            break;
        }
        if (s1->tok != ',')
            break;
        next(s1);
    }
    skip(s1, ')');
    skip(s1, ')');
    goto redo;
}

static Sym * find_field (CType *type, int v)
{
    Sym *s = type->ref;
    v |= SYM_FIELD;
    while ((s = s->next) != NULL) {
    if ((s->v & SYM_FIELD) &&
        (s->type.t & VT_BTYPE) == VT_STRUCT &&
        (s->v & ~SYM_FIELD) >= SYM_FIRST_ANOM) {
        Sym *ret = find_field (&s->type, v);
        if (ret)
            return ret;
    }
    if (s->v == v)
      break;
    }
    return s;
}

static void struct_add_offset (Sym *s, int offset)
{
    while ((s = s->next) != NULL) {
    if ((s->v & SYM_FIELD) &&
        (s->type.t & VT_BTYPE) == VT_STRUCT &&
        (s->v & ~SYM_FIELD) >= SYM_FIRST_ANOM) {
        struct_add_offset(s->type.ref, offset);
    } else
      s->c += offset;
    }
}

static void struct_layout(TCCState *s1, CType *type, AttributeDef *ad)
{
    int size, align, maxalign, offset, c, bit_pos, bit_size;
    int packed, a, bt, prevbt, prev_bit_size;
    int pcc = !s1->ms_bitfields;
    int pragma_pack = *s1->pack_stack_ptr;
    Sym *f;

    maxalign = 1;
    offset = 0;
    c = 0;
    bit_pos = 0;
    prevbt = VT_STRUCT; /* make it never match */
    prev_bit_size = 0;

//#define BF_DEBUG

    for (f = type->ref->next; f; f = f->next) {
        if (f->type.t & VT_BITFIELD)
            bit_size = BIT_SIZE(f->type.t);
        else
            bit_size = -1;
        size = type_size(&f->type, &align);
        a = f->a.aligned ? 1 << (f->a.aligned - 1) : 0;
        packed = 0;

        if (pcc && bit_size == 0) {
            /* in pcc mode, packing does not affect zero-width bitfields */

        } else {
            /* in pcc mode, attribute packed overrides if set. */
            if (pcc && (f->a.packed || ad->a.packed))
                align = packed = 1;

            /* pragma pack overrides align if lesser and packs bitfields always */
            if (pragma_pack) {
                packed = 1;
                if (pragma_pack < align)
                    align = pragma_pack;
                /* in pcc mode pragma pack also overrides individual align */
                if (pcc && pragma_pack < a)
                    a = 0;
            }
        }
        /* some individual align was specified */
        if (a)
            align = a;

        if (type->ref->type.t == VT_UNION) {
        if (pcc && bit_size >= 0)
            size = (bit_size + 7) >> 3;
        offset = 0;
        if (size > c)
            c = size;

    } else if (bit_size < 0) {
            if (pcc)
                c += (bit_pos + 7) >> 3;
        c = (c + align - 1) & -align;
        offset = c;
        if (size > 0)
            c += size;
        bit_pos = 0;
        prevbt = VT_STRUCT;
        prev_bit_size = 0;

    } else {
        /* A bit-field.  Layout is more complicated.  There are two
           options: PCC (GCC) compatible and MS compatible */
            if (pcc) {
        /* In PCC layout a bit-field is placed adjacent to the
                   preceding bit-fields, except if:
                   - it has zero-width
                   - an individual alignment was given
                   - it would overflow its base type container and
                     there is no packing */
                if (bit_size == 0) {
            new_field:
            c = (c + ((bit_pos + 7) >> 3) + align - 1) & -align;
            bit_pos = 0;
                } else if (f->a.aligned) {
                    goto new_field;
                } else if (!packed) {
                    int a8 = align * 8;
                int ofs = ((c * 8 + bit_pos) % a8 + bit_size + a8 - 1) / a8;
                    if (ofs > size / align)
                        goto new_field;
                }

                /* in pcc mode, long long bitfields have type int if they fit */
                if (size == 8 && bit_size <= 32)
                    f->type.t = (f->type.t & ~VT_BTYPE) | VT_INT, size = 4;

                while (bit_pos >= align * 8)
                    c += align, bit_pos -= align * 8;
                offset = c;

        /* In PCC layout named bit-fields influence the alignment
           of the containing struct using the base types alignment,
           except for packed fields (which here have correct align).  */
        if (f->v & SYM_FIRST_ANOM
                    // && bit_size // ??? gcc on ARM/rpi does that
                    )
            align = 1;

        } else {
        bt = f->type.t & VT_BTYPE;
        if ((bit_pos + bit_size > size * 8)
                    || (bit_size > 0) == (bt != prevbt)
                    ) {
            c = (c + align - 1) & -align;
            offset = c;
            bit_pos = 0;
            /* In MS bitfield mode a bit-field run always uses
               at least as many bits as the underlying type.
               To start a new run it's also required that this
               or the last bit-field had non-zero width.  */
            if (bit_size || prev_bit_size)
                c += size;
        }
        /* In MS layout the records alignment is normally
           influenced by the field, except for a zero-width
           field at the start of a run (but by further zero-width
           fields it is again).  */
        if (bit_size == 0 && prevbt != bt)
            align = 1;
        prevbt = bt;
                prev_bit_size = bit_size;
        }

        f->type.t = (f->type.t & ~(0x3f << VT_STRUCT_SHIFT))
                | (bit_pos << VT_STRUCT_SHIFT);
        bit_pos += bit_size;
    }
    if (align > maxalign)
        maxalign = align;

#ifdef BF_DEBUG
    printf("set field %s offset %-2d size %-2d align %-2d",
           get_tok_str(s1, f->v & ~SYM_FIELD, NULL), offset, size, align);
    if (f->type.t & VT_BITFIELD) {
        printf(" pos %-2d bits %-2d",
                    BIT_POS(f->type.t),
                    BIT_SIZE(f->type.t)
                    );
    }
    printf("\n");
#endif

    if (f->v & SYM_FIRST_ANOM && (f->type.t & VT_BTYPE) == VT_STRUCT) {
        Sym *ass;
        /* An anonymous struct/union.  Adjust member offsets
           to reflect the real offset of our containing struct.
           Also set the offset of this anon member inside
           the outer struct to be zero.  Via this it
           works when accessing the field offset directly
           (from base object), as well as when recursing
           members in initializer handling.  */
        int v2 = f->type.ref->v;
        if (!(v2 & SYM_FIELD) &&
        (v2 & ~SYM_STRUCT) < SYM_FIRST_ANOM) {
        Sym **pps;
        /* This happens only with MS extensions.  The
           anon member has a named struct type, so it
           potentially is shared with other references.
           We need to unshare members so we can modify
           them.  */
        ass = f->type.ref;
        f->type.ref = sym_push(s1, s1->anon_sym++ | SYM_FIELD,
                       &f->type.ref->type, 0,
                       f->type.ref->c);
        pps = &f->type.ref->next;
        while ((ass = ass->next) != NULL) {
            *pps = sym_push(s1, ass->v, &ass->type, 0, ass->c);
            pps = &((*pps)->next);
        }
        *pps = NULL;
        }
        struct_add_offset(f->type.ref, offset);
        f->c = 0;
    } else {
        f->c = offset;
    }

    f->r = 0;
    }

    if (pcc)
        c += (bit_pos + 7) >> 3;

    /* store size and alignment */
    a = bt = ad->a.aligned ? 1 << (ad->a.aligned - 1) : 1;
    if (a < maxalign)
        a = maxalign;
    type->ref->r = a;
    if (pragma_pack && pragma_pack < maxalign && 0 == pcc) {
        /* can happen if individual align for some member was given.  In
           this case MSVC ignores maxalign when aligning the size */
        a = pragma_pack;
        if (a < bt)
            a = bt;
    }
    c = (c + a - 1) & -a;
    type->ref->c = c;

#ifdef BF_DEBUG
    printf("struct size %-2d align %-2d\n\n", c, a), fflush(stdout);
#endif

    /* check whether we can access bitfields by their type */
    for (f = type->ref->next; f; f = f->next) {
        int s, px, cx, c0;
        CType t;

        if (0 == (f->type.t & VT_BITFIELD))
            continue;
        f->type.ref = f;
        f->auxtype = -1;
        bit_size = BIT_SIZE(f->type.t);
        if (bit_size == 0)
            continue;
        bit_pos = BIT_POS(f->type.t);
        size = type_size(&f->type, &align);
        if (bit_pos + bit_size <= size * 8 && f->c + size <= c)
            continue;

        /* try to access the field using a different type */
        c0 = -1, s = align = 1;
        for (;;) {
            px = f->c * 8 + bit_pos;
            cx = (px >> 3) & -align;
            px = px - (cx << 3);
            if (c0 == cx)
                break;
            s = (px + bit_size + 7) >> 3;
            if (s > 4) {
                t.t = VT_LLONG;
            } else if (s > 2) {
                t.t = VT_INT;
            } else if (s > 1) {
                t.t = VT_SHORT;
            } else {
                t.t = VT_BYTE;
            }
            s = type_size(&t, &align);
            c0 = cx;
        }

        if (px + bit_size <= s * 8 && cx + s <= c) {
            /* update offset and bit position */
            f->c = cx;
            bit_pos = px;
        f->type.t = (f->type.t & ~(0x3f << VT_STRUCT_SHIFT))
                | (bit_pos << VT_STRUCT_SHIFT);
            if (s != size)
                f->auxtype = t.t;
#ifdef BF_DEBUG
            printf("FIX field %s offset %-2d size %-2d align %-2d "
                "pos %-2d bits %-2d\n",
                get_tok_str(s1, f->v & ~SYM_FIELD, NULL),
                cx, s, align, px, bit_size);
#endif
        } else {
            /* fall back to load/store single-byte wise */
            f->auxtype = VT_STRUCT;
#ifdef BF_DEBUG
            printf("FIX field %s : load byte-wise\n",
                 get_tok_str(s1, f->v & ~SYM_FIELD, NULL));
#endif
        }
    }
}

/* enum/struct/union declaration. u is VT_ENUM/VT_STRUCT/VT_UNION */
static void struct_decl(TCCState *s1, CType *type, int u)
{
    int v, c, size, align, flexible;
    int bit_size, bsize, bt;
    Sym *s, *ss, **ps;
    AttributeDef ad, ad1;
    CType type1, btype;

    memset(&ad, 0, sizeof ad);
    next(s1);
    parse_attribute(s1, &ad);
    if (s1->tok != '{') {
        v = s1->tok;
        next(s1);
        /* struct already defined ? return it */
        if (v < TOK_IDENT)
            expect(s1, "struct/union/enum name");
        s = struct_find(s1, v);
        if (s && (s->sym_scope == s1->local_scope || s1->tok != '{')) {
            if (u == s->type.t)
                goto do_decl;
            if (u == VT_ENUM && IS_ENUM(s->type.t))
                goto do_decl;
            tcc_error(s1, "redefinition of '%s'", get_tok_str(s1, v, NULL));
        }
    } else {
        v = s1->anon_sym++;
    }
    /* Record the original enum/struct/union token.  */
    type1.t = u == VT_ENUM ? u | VT_INT | VT_UNSIGNED : u;
    type1.ref = NULL;
    /* we put an undefined size for struct/union */
    s = sym_push(s1, v | SYM_STRUCT, &type1, 0, -1);
    s->r = 0; /* default alignment is zero as gcc */
do_decl:
    type->t = s->type.t;
    type->ref = s;

    if (s1->tok == '{') {
        next(s1);
        if (s->c != -1)
            tcc_error(s1, "struct/union/enum already defined");
        /* cannot be empty */
        /* non empty enums are not allowed */
        ps = &s->next;
        if (u == VT_ENUM) {
            long long ll = 0, pl = 0, nl = 0;
        CType t;
            t.ref = s;
            /* enum symbols have static storage */
            t.t = VT_INT|VT_STATIC|VT_ENUM_VAL;
            for(;;) {
                v = s1->tok;
                if (v < TOK_UIDENT)
                    expect(s1, "identifier");
                ss = sym_find(s1, v);
                if (ss && !s1->local_stack)
                    tcc_error(s1, "redefinition of enumerator '%s'",
                              get_tok_str(s1, v, NULL));
                next(s1);
                if (s1->tok == '=') {
                    next(s1);
            ll = expr_const64(s1);
                }
                ss = sym_push(s1, v, &t, VT_CONST, 0);
                ss->enum_val = ll;
                *ps = ss, ps = &ss->next;
                if (ll < nl)
                    nl = ll;
                if (ll > pl)
                    pl = ll;
                if (s1->tok != ',')
                    break;
                next(s1);
                ll++;
                /* NOTE: we accept a trailing comma */
                if (s1->tok == '}')
                    break;
            }
            skip(s1, '}');
            /* set integral type of the enum */
            t.t = VT_INT;
            if (nl >= 0) {
                if (pl != (unsigned)pl)
                    t.t = (LONG_SIZE==8 ? VT_LLONG|VT_LONG : VT_LLONG);
                t.t |= VT_UNSIGNED;
            } else if (pl != (int)pl || nl != (int)nl)
                t.t = (LONG_SIZE==8 ? VT_LLONG|VT_LONG : VT_LLONG);
            s->type.t = type->t = t.t | VT_ENUM;
            s->c = 0;
            /* set type for enum members */
            for (ss = s->next; ss; ss = ss->next) {
                ll = ss->enum_val;
                if (ll == (int)ll) /* default is int if it fits */
                    continue;
                if (t.t & VT_UNSIGNED) {
                    ss->type.t |= VT_UNSIGNED;
                    if (ll == (unsigned)ll)
                        continue;
                }
                ss->type.t = (ss->type.t & ~VT_BTYPE)
                    | (LONG_SIZE==8 ? VT_LLONG|VT_LONG : VT_LLONG);
            }
        } else {
            c = 0;
            flexible = 0;
            while (s1->tok != '}') {
                if (!parse_btype(s1, &btype, &ad1)) {
            skip(s1, ';');
            continue;
        }
                while (1) {
            if (flexible)
                tcc_error(s1, "flexible array member '%s' not at the end of struct",
                              get_tok_str(s1, v, NULL));
                    bit_size = -1;
                    v = 0;
                    type1 = btype;
                    if (s1->tok != ':') {
            if (s1->tok != ';')
                            type_decl(s1, &type1, &ad1, &v, TYPE_DIRECT);
                        if (v == 0) {
                            if ((type1.t & VT_BTYPE) != VT_STRUCT)
                            expect(s1, "identifier");
                            else {
                int v = btype.ref->v;
                if (!(v & SYM_FIELD) && (v & ~SYM_STRUCT) < SYM_FIRST_ANOM) {
                    if (s1->ms_extensions == 0)
                                expect(s1, "identifier");
                }
                            }
                        }
                        if (type_size(&type1, &align) < 0) {
                if ((u == VT_STRUCT) && (type1.t & VT_ARRAY) && c)
                    flexible = 1;
                else
                    tcc_error(s1, "field '%s' has incomplete type",
                                      get_tok_str(s1, v, NULL));
                        }
                        if ((type1.t & VT_BTYPE) == VT_FUNC ||
                            (type1.t & VT_STORAGE))
                            tcc_error(s1, "invalid type for '%s'",
                                  get_tok_str(s1, v, NULL));
                    }
                    if (s1->tok == ':') {
                        next(s1);
                        bit_size = expr_const(s1);
                        /* XXX: handle v = 0 case for messages */
                        if (bit_size < 0)
                            tcc_error(s1, "negative width in bit-field '%s'",
                                  get_tok_str(s1, v, NULL));
                        if (v && bit_size == 0)
                            tcc_error(s1, "zero width for bit-field '%s'",
                                  get_tok_str(s1, v, NULL));
            parse_attribute(s1, &ad1);
                    }
                    size = type_size(&type1, &align);
                    if (bit_size >= 0) {
                        bt = type1.t & VT_BTYPE;
                        if (bt != VT_INT &&
                            bt != VT_BYTE &&
                            bt != VT_SHORT &&
                            bt != VT_BOOL &&
                            bt != VT_LLONG)
                            tcc_error(s1, "bitfields must have scalar type");
                        bsize = size * 8;
                        if (bit_size > bsize) {
                            tcc_error(s1, "width of '%s' exceeds its type",
                                  get_tok_str(s1, v, NULL));
                        } else if (bit_size == bsize
                                    && !ad.a.packed && !ad1.a.packed) {
                            /* no need for bit fields */
                            ;
                        } else if (bit_size == 64) {
                            tcc_error(s1, "field width 64 not implemented");
                        } else {
                            type1.t = (type1.t & ~VT_STRUCT_MASK)
                                | VT_BITFIELD
                                | (bit_size << (VT_STRUCT_SHIFT + 6));
                        }
                    }
                    if (v != 0 || (type1.t & VT_BTYPE) == VT_STRUCT) {
                        /* Remember we've seen a real field to check
               for placement of flexible array member. */
            c = 1;
                    }
            /* If member is a struct or bit-field, enforce
               placing into the struct (as anonymous).  */
                    if (v == 0 &&
            ((type1.t & VT_BTYPE) == VT_STRUCT ||
             bit_size >= 0)) {
                v = s1->anon_sym++;
            }
                    if (v) {
                        ss = sym_push(s1, v | SYM_FIELD, &type1, 0, 0);
                        ss->a = ad1.a;
                        *ps = ss;
                        ps = &ss->next;
                    }
                    if (s1->tok == ';' || s1->tok == TOK_EOF)
                        break;
                    skip(s1, ',');
                }
                skip(s1, ';');
            }
            skip(s1, '}');
        parse_attribute(s1, &ad);
        struct_layout(s1, type, &ad);
        }
    }
}

static void sym_to_attr(AttributeDef *ad, Sym *s)
{
    if (s->a.aligned && 0 == ad->a.aligned)
        ad->a.aligned = s->a.aligned;
    if (s->f.func_call && 0 == ad->f.func_call)
        ad->f.func_call = s->f.func_call;
    if (s->f.func_type && 0 == ad->f.func_type)
        ad->f.func_type = s->f.func_type;
    if (s->a.packed)
        ad->a.packed = 1;
}

/* Add type qualifiers to a type. If the type is an array then the qualifiers
   are added to the element type, copied because it could be a typedef. */
static void parse_btype_qualify(TCCState *s1, CType *type, int qualifiers)
{
    while (type->t & VT_ARRAY) {
        type->ref = sym_push(s1, SYM_FIELD, &type->ref->type, 0, type->ref->c);
        type = &type->ref->type;
    }
    type->t |= qualifiers;
}

/* return 0 if no type declaration. otherwise, return the basic type
   and skip it.
 */
static int parse_btype(TCCState *s1, CType *type, AttributeDef *ad)
{
    int t, u, bt, st, type_found, typespec_found, g;
    Sym *s;
    CType type1;

    memset(ad, 0, sizeof(AttributeDef));
    type_found = 0;
    typespec_found = 0;
    t = VT_INT;
    bt = st = -1;
    type->ref = NULL;

    while(1) {
        switch(s1->tok) {
        case TOK_EXTENSION:
            /* currently, we really ignore extension */
            next(s1);
            continue;

            /* basic types */
        case TOK_CHAR:
            u = VT_BYTE;
        basic_type:
            next(s1);
        basic_type1:
            if (u == VT_SHORT || u == VT_LONG) {
                if (st != -1 || (bt != -1 && bt != VT_INT))
                    tmbt: tcc_error(s1, "too many basic types");
                st = u;
            } else {
                if (bt != -1 || (st != -1 && u != VT_INT))
                    goto tmbt;
                bt = u;
            }
            if (u != VT_INT)
                t = (t & ~(VT_BTYPE|VT_LONG)) | u;
            typespec_found = 1;
            break;
        case TOK_VOID:
            u = VT_VOID;
            goto basic_type;
        case TOK_SHORT:
            u = VT_SHORT;
            goto basic_type;
        case TOK_INT:
            u = VT_INT;
            goto basic_type;
        case TOK_LONG:
            if ((t & VT_BTYPE) == VT_DOUBLE) {
                t = (t & ~(VT_BTYPE|VT_LONG)) | VT_LDOUBLE;
            } else if ((t & (VT_BTYPE|VT_LONG)) == VT_LONG) {
                t = (t & ~(VT_BTYPE|VT_LONG)) | VT_LLONG;
            } else {
                u = VT_LONG;
                goto basic_type;
            }
            next(s1);
            break;
#ifdef TCC_TARGET_ARM64
        case TOK_UINT128:
            /* GCC's __uint128_t appears in some Linux header files. Make it a
               synonym for long double to get the size and alignment right. */
            u = VT_LDOUBLE;
            goto basic_type;
#endif
        case TOK_BOOL:
            u = VT_BOOL;
            goto basic_type;
        case TOK_FLOAT:
            u = VT_FLOAT;
            goto basic_type;
        case TOK_DOUBLE:
            if ((t & (VT_BTYPE|VT_LONG)) == VT_LONG) {
                t = (t & ~(VT_BTYPE|VT_LONG)) | VT_LDOUBLE;
            } else {
                u = VT_DOUBLE;
                goto basic_type;
            }
            next(s1);
            break;
        case TOK_ENUM:
            struct_decl(s1, &type1, VT_ENUM);
        basic_type2:
            u = type1.t;
            type->ref = type1.ref;
            goto basic_type1;
        case TOK_STRUCT:
            struct_decl(s1, &type1, VT_STRUCT);
            goto basic_type2;
        case TOK_UNION:
            struct_decl(s1, &type1, VT_UNION);
            goto basic_type2;

            /* type modifiers */
        case TOK_CONST1:
        case TOK_CONST2:
        case TOK_CONST3:
            type->t = t;
            parse_btype_qualify(s1, type, VT_CONSTANT);
            t = type->t;
            next(s1);
            break;
        case TOK_VOLATILE1:
        case TOK_VOLATILE2:
        case TOK_VOLATILE3:
            type->t = t;
            parse_btype_qualify(s1, type, VT_VOLATILE);
            t = type->t;
            next(s1);
            break;
        case TOK_SIGNED1:
        case TOK_SIGNED2:
        case TOK_SIGNED3:
            if ((t & (VT_DEFSIGN|VT_UNSIGNED)) == (VT_DEFSIGN|VT_UNSIGNED))
                tcc_error(s1, "signed and unsigned modifier");
            t |= VT_DEFSIGN;
            next(s1);
            typespec_found = 1;
            break;
        case TOK_REGISTER:
        case TOK_AUTO:
        case TOK_RESTRICT1:
        case TOK_RESTRICT2:
        case TOK_RESTRICT3:
            next(s1);
            break;
        case TOK_UNSIGNED:
            if ((t & (VT_DEFSIGN|VT_UNSIGNED)) == VT_DEFSIGN)
                tcc_error(s1, "signed and unsigned modifier");
            t |= VT_DEFSIGN | VT_UNSIGNED;
            next(s1);
            typespec_found = 1;
            break;

            /* storage */
        case TOK_EXTERN:
            g = VT_EXTERN;
            goto storage;
        case TOK_STATIC:
            g = VT_STATIC;
            goto storage;
        case TOK_TYPEDEF:
            g = VT_TYPEDEF;
            goto storage;
       storage:
            if (t & (VT_EXTERN|VT_STATIC|VT_TYPEDEF) & ~g)
                tcc_error(s1, "multiple storage classes");
            t |= g;
            next(s1);
            break;
        case TOK_INLINE1:
        case TOK_INLINE2:
        case TOK_INLINE3:
            t |= VT_INLINE;
            next(s1);
            break;

            /* GNUC attribute */
        case TOK_ATTRIBUTE1:
        case TOK_ATTRIBUTE2:
            parse_attribute(s1, ad);
            if (ad->attr_mode) {
                u = ad->attr_mode -1;
                t = (t & ~(VT_BTYPE|VT_LONG)) | u;
            }
            break;
            /* GNUC typeof */
        case TOK_TYPEOF1:
        case TOK_TYPEOF2:
        case TOK_TYPEOF3:
            next(s1);
            parse_expr_type(s1, &type1);
            /* remove all storage modifiers except typedef */
            type1.t &= ~(VT_STORAGE&~VT_TYPEDEF);
        if (type1.ref)
                sym_to_attr(ad, type1.ref);
            goto basic_type2;
        default:
            if (typespec_found)
                goto the_end;
            s = sym_find(s1, s1->tok);
            if (!s || !(s->type.t & VT_TYPEDEF))
                goto the_end;
            t &= ~(VT_BTYPE|VT_LONG);
            u = t & ~(VT_CONSTANT | VT_VOLATILE), t ^= u;
            type->t = (s->type.t & ~VT_TYPEDEF) | u;
            type->ref = s->type.ref;
            if (t)
                parse_btype_qualify(s1, type, t);
            t = type->t;
            /* get attributes from typedef */
            sym_to_attr(ad, s);
            next(s1);
            typespec_found = 1;
            st = bt = -2;
            break;
        }
        type_found = 1;
    }
the_end:
    if (s1->char_is_unsigned) {
        if ((t & (VT_DEFSIGN|VT_BTYPE)) == VT_BYTE)
            t |= VT_UNSIGNED;
    }
    /* VT_LONG is used just as a modifier for VT_INT / VT_LLONG */
    bt = t & (VT_BTYPE|VT_LONG);
    if (bt == VT_LONG)
        t |= LONG_SIZE == 8 ? VT_LLONG : VT_INT;
#ifdef TCC_TARGET_PE
    if (bt == VT_LDOUBLE)
        t = (t & ~(VT_BTYPE|VT_LONG)) | VT_DOUBLE;
#endif
    type->t = t;
    return type_found;
}

/* convert a function parameter type (array to pointer and function to
   function pointer) */
static inline void convert_parameter_type(TCCState *s1, CType *pt)
{
    /* remove const and volatile qualifiers (XXX: const could be used
       to indicate a const function parameter */
    pt->t &= ~(VT_CONSTANT | VT_VOLATILE);
    /* array must be transformed to pointer according to ANSI C */
    pt->t &= ~VT_ARRAY;
    if ((pt->t & VT_BTYPE) == VT_FUNC) {
        mk_pointer(s1, pt);
    }
}

ST_FUNC void parse_asm_str(TCCState *s1, CString *astr)
{
    skip(s1, '(');
    parse_mult_str(s1, astr, "string constant");
}

/* Parse an asm label and return the token */
static int asm_label_instr(TCCState *s1)
{
    int v;
    CString astr;

    next(s1);
    parse_asm_str(s1, &astr);
    skip(s1, ')');
#ifdef ASM_DEBUG
    printf("asm_alias: \"%s\"\n", (char *)astr.data);
#endif
    v = tok_alloc(s1, astr.data, astr.size - 1)->tok;
    cstr_free(s1, &astr);
    return v;
}

static int post_type(TCCState *s1, CType *type, AttributeDef *ad, int storage, int td)
{
    int n, l, t1, arg_size, align;
    Sym **plast, *s, *first;
    AttributeDef ad1;
    CType pt;

    if (s1->tok == '(') {
        /* function type, or recursive declarator (return if so) */
        next(s1);
    if (td && !(td & TYPE_ABSTRACT))
      return 0;
    if (s1->tok == ')')
      l = 0;
    else if (parse_btype(s1, &pt, &ad1))
      l = FUNC_NEW;
    else if (td)
      return 0;
    else
      l = FUNC_OLD;
        first = NULL;
        plast = &first;
        arg_size = 0;
        if (l) {
            for(;;) {
                /* read param name and compute offset */
                if (l != FUNC_OLD) {
                    if ((pt.t & VT_BTYPE) == VT_VOID && s1->tok == ')')
                        break;
                    type_decl(s1, &pt, &ad1, &n, TYPE_DIRECT | TYPE_ABSTRACT);
                    if ((pt.t & VT_BTYPE) == VT_VOID)
                        tcc_error(s1, "parameter declared as void");
                    arg_size += (type_size(&pt, &align) + PTR_SIZE - 1) / PTR_SIZE;
                } else {
                    n = s1->tok;
                    if (n < TOK_UIDENT)
                        expect(s1, "identifier");
                    pt.t = VT_VOID; /* invalid type */
                    next(s1);
                }
                convert_parameter_type(s1, &pt);
                s = sym_push(s1, n | SYM_FIELD, &pt, 0, 0);
                *plast = s;
                plast = &s->next;
                if (s1->tok == ')')
                    break;
                skip(s1, ',');
                if (l == FUNC_NEW && s1->tok == TOK_DOTS) {
                    l = FUNC_ELLIPSIS;
                    next(s1);
                    break;
                }
        if (l == FUNC_NEW && !parse_btype(s1, &pt, &ad1))
            tcc_error(s1, "invalid type");
            }
        } else
            /* if no parameters, then old type prototype */
            l = FUNC_OLD;
        skip(s1, ')');
        /* NOTE: const is ignored in returned type as it has a special
           meaning in gcc / C++ */
        type->t &= ~VT_CONSTANT;
        /* some ancient pre-K&R C allows a function to return an array
           and the array brackets to be put after the arguments, such
           that "int c()[]" means something like "int[] c()" */
        if (s1->tok == '[') {
            next(s1);
            skip(s1, ']'); /* only handle simple "[]" */
            mk_pointer(s1, type);
        }
        /* we push a anonymous symbol which will contain the function prototype */
        ad->f.func_args = arg_size;
        ad->f.func_type = l;
        s = sym_push(s1, SYM_FIELD, type, 0, 0);
        s->a = ad->a;
        s->f = ad->f;
        s->next = first;
        type->t = VT_FUNC;
        type->ref = s;
    } else if (s1->tok == '[') {
    int saved_nocode_wanted = s1->nocode_wanted;
        /* array definition */
        next(s1);
        if (s1->tok == TOK_RESTRICT1)
            next(s1);
        n = -1;
        t1 = 0;
        if (s1->tok != ']') {
            if (!s1->local_stack || (storage & VT_STATIC))
                vpushi(s1, expr_const(s1));
            else {
        /* VLAs (which can only happen with s1->local_stack && !VT_STATIC)
           length must always be evaluated, even under s1->nocode_wanted,
           so that its size slot is initialized (e.g. under sizeof
           or typeof).  */
        s1->nocode_wanted = 0;
        gexpr(s1);
        }
            if ((s1->vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST) {
                n = s1->vtop->c.i;
                if (n < 0)
                    tcc_error(s1, "invalid array size");
            } else {
                if (!is_integer_btype(s1->vtop->type.t & VT_BTYPE))
                    tcc_error(s1, "size of variable length array should be an integer");
                t1 = VT_VLA;
            }
        }
        skip(s1, ']');
        /* parse next post type */
        post_type(s1, type, ad, storage, 0);
        if (type->t == VT_FUNC)
            tcc_error(s1, "declaration of an array of functions");
        t1 |= type->t & VT_VLA;

        if (t1 & VT_VLA) {
            s1->loc -= type_size(&s1->int_type, &align);
            s1->loc &= -align;
            n = s1->loc;

            vla_runtime_type_size(s1, type, &align);
            gen_op(s1, '*');
            vset(s1, &s1->int_type, VT_LOCAL|VT_LVAL, n);
            vswap(s1);
            vstore(s1);
        }
        if (n != -1)
            vpop(s1);
    s1->nocode_wanted = saved_nocode_wanted;

        /* we push an anonymous symbol which will contain the array
           element type */
        s = sym_push(s1, SYM_FIELD, type, 0, n);
        type->t = (t1 ? VT_VLA : VT_ARRAY) | VT_PTR;
        type->ref = s;
    }
    return 1;
}

/* Parse a type declarator (except basic type), and return the type
   in 'type'. 'td' is a bitmask indicating which kind of type decl is
   expected. 'type' should contain the basic type. 'ad' is the
   attribute definition of the basic type. It can be modified by
   type_decl().  If this (possibly abstract) declarator is a pointer chain
   it returns the innermost pointed to type (equals *type, but is a different
   pointer), otherwise returns type itself, that's used for recursive calls.  */
static CType *type_decl(TCCState *s1, CType *type, AttributeDef *ad, int *v, int td)
{
    CType *post, *ret;
    int qualifiers, storage;

    /* recursive type, remove storage bits first, apply them later again */
    storage = type->t & VT_STORAGE;
    type->t &= ~VT_STORAGE;
    post = ret = type;

    while (s1->tok == '*') {
        qualifiers = 0;
    redo:
        next(s1);
        switch(s1->tok) {
        case TOK_CONST1:
        case TOK_CONST2:
        case TOK_CONST3:
            qualifiers |= VT_CONSTANT;
            goto redo;
        case TOK_VOLATILE1:
        case TOK_VOLATILE2:
        case TOK_VOLATILE3:
            qualifiers |= VT_VOLATILE;
            goto redo;
        case TOK_RESTRICT1:
        case TOK_RESTRICT2:
        case TOK_RESTRICT3:
            goto redo;
        /* XXX: clarify attribute handling */
        case TOK_ATTRIBUTE1:
        case TOK_ATTRIBUTE2:
            parse_attribute(s1, ad);
            break;
        }
        mk_pointer(s1, type);
        type->t |= qualifiers;
    if (ret == type)
        /* innermost pointed to type is the one for the first derivation */
        ret = pointed_type(type);
    }

    if (s1->tok == '(') {
        /* This is possibly a parameter type list for abstract declarators
           ('int ()'), use post_type for testing this.  */
        if (!post_type(s1, type, ad, 0, td)) {
            /* It's not, so it's a nested declarator, and the post operations
               apply to the innermost pointed to type (if any).  */
            /* XXX: this is not correct to modify 'ad' at this point, but
               the syntax is not clear */
            parse_attribute(s1, ad);
            post = type_decl(s1, type, ad, v, td);
            skip(s1, ')');
        }
    } else if (s1->tok >= TOK_IDENT && (td & TYPE_DIRECT)) {
        /* type identifier */
        *v = s1->tok;
        next(s1);
    } else {
        if (!(td & TYPE_ABSTRACT))
            expect(s1, "identifier");
        *v = 0;
    }
    post_type(s1, post, ad, storage, 0);
    parse_attribute(s1, ad);
    type->t |= storage;
    return ret;
}

/* compute the lvalue VT_LVAL_xxx needed to match type t. */
ST_FUNC int lvalue_type(int t)
{
    int bt, r;
    r = VT_LVAL;
    bt = t & VT_BTYPE;
    if (bt == VT_BYTE || bt == VT_BOOL)
        r |= VT_LVAL_BYTE;
    else if (bt == VT_SHORT)
        r |= VT_LVAL_SHORT;
    else
        return r;
    if (t & VT_UNSIGNED)
        r |= VT_LVAL_UNSIGNED;
    return r;
}

/* indirection with full error checking and bound check */
ST_FUNC void indir(TCCState *s1)
{
    if ((s1->vtop->type.t & VT_BTYPE) != VT_PTR) {
        if ((s1->vtop->type.t & VT_BTYPE) == VT_FUNC)
            return;
        expect(s1, "pointer");
    }
    if (s1->vtop->r & VT_LVAL)
        gv(s1, RC_INT);
    s1->vtop->type = *pointed_type(&s1->vtop->type);
    /* Arrays and functions are never lvalues */
    if (!(s1->vtop->type.t & VT_ARRAY) && !(s1->vtop->type.t & VT_VLA)
        && (s1->vtop->type.t & VT_BTYPE) != VT_FUNC) {
        s1->vtop->r |= lvalue_type(s1->vtop->type.t);
        /* if bound checking, the referenced pointer must be checked */
#ifdef CONFIG_TCC_BCHECK
        if (s1->do_bounds_check)
            s1->vtop->r |= VT_MUSTBOUND;
#endif
    }
}

/* pass a parameter to a function and do type checking and casting */
static void gfunc_param_typed(TCCState *s1, Sym *func, Sym *arg)
{
    int func_type;
    CType type;

    func_type = func->f.func_type;
    if (func_type == FUNC_OLD ||
        (func_type == FUNC_ELLIPSIS && arg == NULL)) {
        /* default casting : only need to convert float to double */
        if ((s1->vtop->type.t & VT_BTYPE) == VT_FLOAT) {
            gen_cast_s(s1, VT_DOUBLE);
        } else if (s1->vtop->type.t & VT_BITFIELD) {
            type.t = s1->vtop->type.t & (VT_BTYPE | VT_UNSIGNED);
        type.ref = s1->vtop->type.ref;
            gen_cast(s1, &type);
        }
    } else if (arg == NULL) {
        tcc_error(s1, "too many arguments to function");
    } else {
        type = arg->type;
        type.t &= ~VT_CONSTANT; /* need to do that to avoid false warning */
        gen_assign_cast(s1, &type);
    }
}

/* parse an expression and return its type without any side effect. */
static void expr_type(TCCState *s1, CType *type, void (*expr_fn)(TCCState *s1))
{
    s1->nocode_wanted++;
    expr_fn(s1);
    *type = s1->vtop->type;
    vpop(s1);
    s1->nocode_wanted--;
}

/* parse an expression of the form '(type)' or '(expr)' and return its
   type */
static void parse_expr_type(TCCState *s1, CType *type)
{
    int n;
    AttributeDef ad;

    skip(s1, '(');
    if (parse_btype(s1, type, &ad)) {
        type_decl(s1, type, &ad, &n, TYPE_ABSTRACT);
    } else {
        expr_type(s1, type, gexpr);
    }
    skip(s1, ')');
}

static void parse_type(TCCState *s1, CType *type)
{
    AttributeDef ad;
    int n;

    if (!parse_btype(s1, type, &ad)) {
        expect(s1, "type");
    }
    type_decl(s1, type, &ad, &n, TYPE_ABSTRACT);
}

static void parse_builtin_params(TCCState *s1, int nc, const char *args)
{
    char c, sep = '(';
    CType t;
    if (nc)
        s1->nocode_wanted++;
    next(s1);
    while ((c = *args++)) {
    skip(s1, sep);
    sep = ',';
    switch (c) {
        case 'e': expr_eq(s1); continue;
        case 't': parse_type(s1, &t); vpush(s1, &t); continue;
        default: tcc_error(s1, "internal error"); break;
    }
    }
    skip(s1, ')');
    if (nc)
        s1->nocode_wanted--;
}

ST_FUNC void unary(TCCState *s1)
{
    int n, t, align, size, r, sizeof_caller;
    CType type;
    Sym *s;
    AttributeDef ad;

    sizeof_caller = s1->in_sizeof;
    s1->in_sizeof = 0;
    type.ref = NULL;
    /* XXX: GCC 2.95.3 does not generate a table although it should be
       better here */
 tok_next:
    switch(s1->tok) {
    case TOK_EXTENSION:
        next(s1);
        goto tok_next;
    case TOK_LCHAR:
#ifdef TCC_TARGET_PE
        t = VT_SHORT|VT_UNSIGNED;
        goto push_tokc;
#endif
    case TOK_CINT:
    case TOK_CCHAR:
    t = VT_INT;
 push_tokc:
    type.t = t;
    vsetc(s1, &type, VT_CONST, &s1->tokc);
        next(s1);
        break;
    case TOK_CUINT:
        t = VT_INT | VT_UNSIGNED;
        goto push_tokc;
    case TOK_CLLONG:
        t = VT_LLONG;
    goto push_tokc;
    case TOK_CULLONG:
        t = VT_LLONG | VT_UNSIGNED;
    goto push_tokc;
    case TOK_CFLOAT:
        t = VT_FLOAT;
    goto push_tokc;
    case TOK_CDOUBLE:
        t = VT_DOUBLE;
    goto push_tokc;
    case TOK_CLDOUBLE:
        t = VT_LDOUBLE;
    goto push_tokc;
    case TOK_CLONG:
        t = (LONG_SIZE == 8 ? VT_LLONG : VT_INT) | VT_LONG;
    goto push_tokc;
    case TOK_CULONG:
        t = (LONG_SIZE == 8 ? VT_LLONG : VT_INT) | VT_LONG | VT_UNSIGNED;
    goto push_tokc;
    case TOK___FUNCTION__:
        if (!s1->gnu_ext)
            goto tok_identifier;
        /* fall thru */
    case TOK___FUNC__:
        {
            void *ptr;
            int len;
            /* special function name identifier */
            len = strlen(s1->funcname) + 1;
            /* generate char[len] type */
            type.t = VT_BYTE;
            mk_pointer(s1, &type);
            type.t |= VT_ARRAY;
            type.ref->c = len;
            vpush_ref(s1, &type, s1->data_section, s1->data_section->data_offset, len);
            if (!NODATA_WANTED) {
                ptr = section_ptr_add(s1, s1->data_section, len);
                memcpy(ptr, s1->funcname, len);
            }
            next(s1);
        }
        break;
    case TOK_LSTR:
#ifdef TCC_TARGET_PE
        t = VT_SHORT | VT_UNSIGNED;
#else
        t = VT_INT;
#endif
        goto str_init;
    case TOK_STR:
        /* string parsing */
        t = VT_BYTE;
        if (s1->char_is_unsigned)
            t = VT_BYTE | VT_UNSIGNED;
    str_init:
        if (s1->warn_write_strings)
            t |= VT_CONSTANT;
        type.t = t;
        mk_pointer(s1, &type);
        type.t |= VT_ARRAY;
        memset(&ad, 0, sizeof(AttributeDef));
        decl_initializer_alloc(s1, &type, &ad, VT_CONST, 2, 0, 0);
        break;
    case '(':
        next(s1);
        /* cast ? */
        if (parse_btype(s1, &type, &ad)) {
            type_decl(s1, &type, &ad, &n, TYPE_ABSTRACT);
            skip(s1, ')');
            /* check ISOC99 compound literal */
            if (s1->tok == '{') {
                    /* data is allocated locally by default */
                if (s1->global_expr)
                    r = VT_CONST;
                else
                    r = VT_LOCAL;
                /* all except arrays are lvalues */
                if (!(type.t & VT_ARRAY))
                    r |= lvalue_type(type.t);
                memset(&ad, 0, sizeof(AttributeDef));
                decl_initializer_alloc(s1, &type, &ad, r, 1, 0, 0);
            } else {
                if (sizeof_caller) {
                    vpush(s1, &type);
                    return;
                }
                unary(s1);
                gen_cast(s1, &type);
            }
        } else if (s1->tok == '{') {
        int saved_nocode_wanted = s1->nocode_wanted;
            if (s1->const_wanted)
                tcc_error(s1, "expected constant");
            /* save all registers */
            save_regs(s1, 0);
            /* statement expression : we do not accept break/continue
               inside as GCC does.  We do retain the s1->nocode_wanted state,
           as statement expressions can't ever be entered from the
           outside, so any reactivation of code emission (from labels
           or loop heads) can be disabled again after the end of it. */
            block(s1, NULL, NULL, 1);
        s1->nocode_wanted = saved_nocode_wanted;
            skip(s1, ')');
        } else {
            gexpr(s1);
            skip(s1, ')');
        }
        break;
    case '*':
        next(s1);
        unary(s1);
        indir(s1);
        break;
    case '&':
        next(s1);
        unary(s1);
        /* functions names must be treated as function pointers,
           except for unary '&' and sizeof. Since we consider that
           functions are not lvalues, we only have to handle it
           there and in function calls. */
        /* arrays can also be used although they are not lvalues */
        if ((s1->vtop->type.t & VT_BTYPE) != VT_FUNC &&
            !(s1->vtop->type.t & VT_ARRAY))
            test_lvalue(s1);
        mk_pointer(s1, &s1->vtop->type);
        gaddrof(s1);
        break;
    case '!':
        next(s1);
        unary(s1);
        if ((s1->vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST) {
            gen_cast_s(s1, VT_BOOL);
            s1->vtop->c.i = !s1->vtop->c.i;
        } else if ((s1->vtop->r & VT_VALMASK) == VT_CMP)
            s1->vtop->c.i ^= 1;
        else {
            save_regs(s1, 1);
            vseti(s1, VT_JMP, gvtst(s1, 1, 0));
        }
        break;
    case '~':
        next(s1);
        unary(s1);
        vpushi(s1, -1);
        gen_op(s1, '^');
        break;
    case '+':
        next(s1);
        unary(s1);
        if ((s1->vtop->type.t & VT_BTYPE) == VT_PTR)
            tcc_error(s1, "pointer not accepted for unary plus");
        /* In order to force cast, we add zero, except for floating point
       where we really need an noop (otherwise -0.0 will be transformed
       into +0.0).  */
    if (!is_float(s1->vtop->type.t)) {
        vpushi(s1, 0);
        gen_op(s1, '+');
    }
        break;
    case TOK_SIZEOF:
    case TOK_ALIGNOF1:
    case TOK_ALIGNOF2:
        t = s1->tok;
        next(s1);
        s1->in_sizeof++;
        expr_type(s1, &type, unary); /* Perform a s1->in_sizeof = 0; */
        s = s1->vtop[1].sym; /* hack: accessing previous s1->vtop */
        size = type_size(&type, &align);
        if (s && s->a.aligned)
            align = 1 << (s->a.aligned - 1);
        if (t == TOK_SIZEOF) {
            if (!(type.t & VT_VLA)) {
                if (size < 0)
                    tcc_error(s1, "sizeof applied to an incomplete type");
                vpushs(s1, size);
            } else {
                vla_runtime_type_size(s1, &type, &align);
            }
        } else {
            vpushs(s1, align);
        }
        s1->vtop->type.t |= VT_UNSIGNED;
        break;

    case TOK_builtin_expect:
    /* __builtin_expect is a no-op for now */
    parse_builtin_params(s1, 0, "ee");
    vpop(s1);
        break;
    case TOK_builtin_types_compatible_p:
    parse_builtin_params(s1, 0, "tt");
    s1->vtop[-1].type.t &= ~(VT_CONSTANT | VT_VOLATILE);
    s1->vtop[0].type.t &= ~(VT_CONSTANT | VT_VOLATILE);
    n = is_compatible_types(&s1->vtop[-1].type, &s1->vtop[0].type);
    s1->vtop -= 2;
    vpushi(s1, n);
        break;
    case TOK_builtin_choose_expr:
    {
        int64_t c;
        next(s1);
        skip(s1, '(');
        c = expr_const64(s1);
        skip(s1, ',');
        if (!c) {
        s1->nocode_wanted++;
        }
        expr_eq(s1);
        if (!c) {
        vpop(s1);
        s1->nocode_wanted--;
        }
        skip(s1, ',');
        if (c) {
        s1->nocode_wanted++;
        }
        expr_eq(s1);
        if (c) {
        vpop(s1);
        s1->nocode_wanted--;
        }
        skip(s1, ')');
    }
        break;
    case TOK_builtin_constant_p:
    parse_builtin_params(s1, 1, "e");
    n = (s1->vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST;
    s1->vtop--;
    vpushi(s1, n);
        break;
    case TOK_builtin_frame_address:
    case TOK_builtin_return_address:
        {
            int tok1 = s1->tok;
            int level;
            next(s1);
            skip(s1, '(');
            if (s1->tok != TOK_CINT) {
                tcc_error(s1, "%s only takes positive integers",
                          tok1 == TOK_builtin_return_address ?
                          "__builtin_return_address" :
                          "__builtin_frame_address");
            }
            level = (uint32_t)s1->tokc.i;
            next(s1);
            skip(s1, ')');
            type.t = VT_VOID;
            mk_pointer(s1, &type);
            vset(s1, &type, VT_LOCAL, 0);       /* local frame */
            while (level--) {
                mk_pointer(s1, &s1->vtop->type);
                indir(s1);                    /* -> parent frame */
            }
            if (tok1 == TOK_builtin_return_address) {
                // assume return address is just above frame pointer on stack
                vpushi(s1, PTR_SIZE);
                gen_op(s1, '+');
                mk_pointer(s1, &s1->vtop->type);
                indir(s1);
            }
        }
        break;
#ifdef TCC_TARGET_X86_64
#ifdef TCC_TARGET_PE
    case TOK_builtin_va_start:
    parse_builtin_params(s1, 0, "ee");
        r = s1->vtop->r & VT_VALMASK;
        if (r == VT_LLOCAL)
            r = VT_LOCAL;
        if (r != VT_LOCAL)
            tcc_error(s1, "__builtin_va_start expects a local variable");
        s1->vtop->r = r;
    s1->vtop->type = s1->char_pointer_type;
    s1->vtop->c.i += 8;
    vstore(s1);
        break;
#else
    case TOK_builtin_va_arg_types:
    parse_builtin_params(s1, 0, "t");
    vpushi(s1, classify_x86_64_va_arg(&s1->vtop->type));
    vswap(s1);
    vpop(s1);
        break;
#endif
#endif

#ifdef TCC_TARGET_ARM64
    case TOK___va_start: {
    parse_builtin_params(s1, 0, "ee");
        //xx check types
        gen_va_start(s1);
        vpushi(s1, 0);
        s1->vtop->type.t = VT_VOID;
        break;
    }
    case TOK___va_arg: {
    parse_builtin_params(s1, 0, "et");
    type = s1->vtop->type;
    vpop(s1);
        //xx check types
        gen_va_arg(s1, &type);
        s1->vtop->type = type;
        break;
    }
    case TOK___arm64_clear_cache: {
    parse_builtin_params(s1, 0, "ee");
        gen_clear_cache(s1);
        vpushi(s1, 0);
        s1->vtop->type.t = VT_VOID;
        break;
    }
#endif
    /* pre operations */
    case TOK_INC:
    case TOK_DEC:
        t = s1->tok;
        next(s1);
        unary(s1);
        inc(s1, 0, t);
        break;
    case '-':
        next(s1);
        unary(s1);
        t = s1->vtop->type.t & VT_BTYPE;
    if (is_float(t)) {
            /* In IEEE negate(x) isn't subtract(0,x), but rather
           subtract(-0, x).  */
        vpush(s1, &s1->vtop->type);
        if (t == VT_FLOAT)
            s1->vtop->c.f = -1.0 * 0.0;
        else if (t == VT_DOUBLE)
            s1->vtop->c.d = -1.0 * 0.0;
        else
            s1->vtop->c.ld = -1.0 * 0.0;
    } else
        vpushi(s1, 0);
    vswap(s1);
    gen_op(s1, '-');
        break;
    case TOK_LAND:
        if (!s1->gnu_ext)
            goto tok_identifier;
        next(s1);
        /* allow to take the address of a label */
        if (s1->tok < TOK_UIDENT)
            expect(s1, "label identifier");
        s = label_find(s1, s1->tok);
        if (!s) {
            s = label_push(s1, &s1->global_label_stack, s1->tok, LABEL_FORWARD);
        } else {
            if (s->r == LABEL_DECLARED)
                s->r = LABEL_FORWARD;
        }
        if (!s->type.t) {
            s->type.t = VT_VOID;
            mk_pointer(s1, &s->type);
            s->type.t |= VT_STATIC;
        }
        vpushsym(s1, &s->type, s);
        next(s1);
        break;

    case TOK_GENERIC:
    {
    CType controlling_type;
    int has_default = 0;
    int has_match = 0;
    int learn = 0;
    TokenString *str = NULL;

    next(s1);
    skip(s1, '(');
    expr_type(s1, &controlling_type, expr_eq);
    controlling_type.t &= ~(VT_CONSTANT | VT_VOLATILE | VT_ARRAY);
    for (;;) {
        learn = 0;
        skip(s1, ',');
        if (s1->tok == TOK_DEFAULT) {
        if (has_default)
            tcc_error(s1, "too many 'default'");
        has_default = 1;
        if (!has_match)
            learn = 1;
        next(s1);
        } else {
            AttributeDef ad_tmp;
        int itmp;
            CType cur_type;
        parse_btype(s1, &cur_type, &ad_tmp);
        type_decl(s1, &cur_type, &ad_tmp, &itmp, TYPE_ABSTRACT);
        if (compare_types(&controlling_type, &cur_type, 0)) {
            if (has_match) {
              tcc_error(s1, "type match twice");
            }
            has_match = 1;
            learn = 1;
        }
        }
        skip(s1, ':');
        if (learn) {
        if (str)
            tok_str_free(s1, str);
        skip_or_save_block(s1, &str);
        } else {
        skip_or_save_block(s1, NULL);
        }
        if (s1->tok == ')')
        break;
    }
    if (!str) {
        char buf[60];
        type_to_str(s1, buf, sizeof buf, &controlling_type, NULL);
        tcc_error(s1, "type '%s' does not match any association", buf);
    }
    begin_macro(s1, str, 1);
    next(s1);
    expr_eq(s1);
    if (s1->tok != TOK_EOF)
        expect(s1, ",");
    end_macro(s1);
        next(s1);
    break;
    }
    // special qnan , snan and infinity values
    case TOK___NAN__:
        vpush64(s1, VT_DOUBLE, 0x7ff8000000000000ULL);
        next(s1);
        break;
    case TOK___SNAN__:
        vpush64(s1, VT_DOUBLE, 0x7ff0000000000001ULL);
        next(s1);
        break;
    case TOK___INF__:
        vpush64(s1, VT_DOUBLE, 0x7ff0000000000000ULL);
        next(s1);
        break;

    default:
    tok_identifier:
        t = s1->tok;
        next(s1);
        if (t < TOK_UIDENT)
            expect(s1, "identifier");
        s = sym_find(s1, t);
        if (!s || IS_ASM_SYM(s)) {
            const char *name = get_tok_str(s1, t, NULL);
            if (s1->tok != '(')
                tcc_error(s1, "'%s' undeclared", name);
            /* for simple function calls, we tolerate undeclared
               external reference to int() function */
            if (s1->warn_implicit_function_declaration
#ifdef TCC_TARGET_PE
                /* people must be warned about using undeclared WINAPI functions
                   (which usually start with uppercase letter) */
                || (name[0] >= 'A' && name[0] <= 'Z')
#endif
            )
                tcc_warning(s1, "implicit declaration of function '%s'", name);
            s = external_global_sym(s1, t, &s1->func_old_type, 0);
        }

        r = s->r;
        /* A symbol that has a register is a local register variable,
           which starts out as VT_LOCAL value.  */
        if ((r & VT_VALMASK) < VT_CONST)
            r = (r & ~VT_VALMASK) | VT_LOCAL;

        vset(s1, &s->type, r, s->c);
        /* Point to s as backpointer (even without r&VT_SYM).
       Will be used by at least the x86 inline asm parser for
       regvars.  */
    s1->vtop->sym = s;

        if (r & VT_SYM) {
            s1->vtop->c.i = 0;
        } else if (r == VT_CONST && IS_ENUM_VAL(s->type.t)) {
            s1->vtop->c.i = s->enum_val;
        }
        break;
    }

    /* post operations */
    while (1) {
        if (s1->tok == TOK_INC || s1->tok == TOK_DEC) {
            inc(s1, 1, s1->tok);
            next(s1);
        } else if (s1->tok == '.' || s1->tok == TOK_ARROW || s1->tok == TOK_CDOUBLE) {
            int qualifiers;
            /* field */
            if (s1->tok == TOK_ARROW)
                indir(s1);
            qualifiers = s1->vtop->type.t & (VT_CONSTANT | VT_VOLATILE);
            test_lvalue(s1);
            gaddrof(s1);
            /* expect pointer on structure */
            if ((s1->vtop->type.t & VT_BTYPE) != VT_STRUCT)
                expect(s1, "struct or union");
            if (s1->tok == TOK_CDOUBLE)
                expect(s1, "field name");
            next(s1);
            if (s1->tok == TOK_CINT || s1->tok == TOK_CUINT)
                expect(s1, "field name");
        s = find_field(&s1->vtop->type, s1->tok);
            if (!s)
                tcc_error(s1, "field not found: %s",  get_tok_str(s1, s1->tok & ~SYM_FIELD, &s1->tokc));
            /* add field offset to pointer */
            s1->vtop->type = s1->char_pointer_type; /* change type to 'char *' */
            vpushi(s1, s->c);
            gen_op(s1, '+');
            /* change type to field type, and set to lvalue */
            s1->vtop->type = s->type;
            s1->vtop->type.t |= qualifiers;
            /* an array is never an lvalue */
            if (!(s1->vtop->type.t & VT_ARRAY)) {
                s1->vtop->r |= lvalue_type(s1->vtop->type.t);
#ifdef CONFIG_TCC_BCHECK
                /* if bound checking, the referenced pointer must be checked */
                if (s1->do_bounds_check && (s1->vtop->r & VT_VALMASK) != VT_LOCAL)
                    s1->vtop->r |= VT_MUSTBOUND;
#endif
            }
            next(s1);
        } else if (s1->tok == '[') {
            next(s1);
            gexpr(s1);
            gen_op(s1, '+');
            indir(s1);
            skip(s1, ']');
        } else if (s1->tok == '(') {
            SValue ret;
            Sym *sa;
            int nb_args, ret_nregs, ret_align, regsize, variadic;

            /* function call  */
            if ((s1->vtop->type.t & VT_BTYPE) != VT_FUNC) {
                /* pointer test (no array accepted) */
                if ((s1->vtop->type.t & (VT_BTYPE | VT_ARRAY)) == VT_PTR) {
                    s1->vtop->type = *pointed_type(&s1->vtop->type);
                    if ((s1->vtop->type.t & VT_BTYPE) != VT_FUNC)
                        goto error_func;
                } else {
                error_func:
                    expect(s1, "function pointer");
                }
            } else {
                s1->vtop->r &= ~VT_LVAL; /* no lvalue */
            }
            /* get return type */
            s = s1->vtop->type.ref;
            next(s1);
            sa = s->next; /* first parameter */
            nb_args = regsize = 0;
            ret.r2 = VT_CONST;
            /* compute first implicit argument if a structure is returned */
            if ((s->type.t & VT_BTYPE) == VT_STRUCT) {
                variadic = (s->f.func_type == FUNC_ELLIPSIS);
                ret_nregs = gfunc_sret(&s->type, variadic, &ret.type,
                                       &ret_align, &regsize);
                if (!ret_nregs) {
                    /* get some space for the returned structure */
                    size = type_size(&s->type, &align);
#ifdef TCC_TARGET_ARM64
                /* On arm64, a small struct is return in registers.
                   It is much easier to write it to memory if we know
                   that we are allowed to write some extra bytes, so
                   round the allocated space up to a power of 2: */
                if (size < 16)
                    while (size & (size - 1))
                        size = (size | (size - 1)) + 1;
#endif
                    s1->loc = (s1->loc - size) & -align;
                    ret.type = s->type;
                    ret.r = VT_LOCAL | VT_LVAL;
                    /* pass it as 'int' to avoid structure arg passing
                       problems */
                    vseti(s1, VT_LOCAL, s1->loc);
                    ret.c = s1->vtop->c;
                    nb_args++;
                }
            } else {
                ret_nregs = 1;
                ret.type = s->type;
            }

            if (ret_nregs) {
                /* return in register */
                if (is_float(ret.type.t)) {
                    ret.r = reg_fret(ret.type.t);
#ifdef TCC_TARGET_X86_64
                    if ((ret.type.t & VT_BTYPE) == VT_QFLOAT)
                      ret.r2 = REG_QRET;
#endif
                } else {
#ifndef TCC_TARGET_ARM64
#ifdef TCC_TARGET_X86_64
                    if ((ret.type.t & VT_BTYPE) == VT_QLONG)
#else
                    if ((ret.type.t & VT_BTYPE) == VT_LLONG)
#endif
                        ret.r2 = REG_LRET;
#endif
                    ret.r = REG_IRET;
                }
                ret.c.i = 0;
            }
            if (s1->tok != ')') {
                for(;;) {
                    expr_eq(s1);
                    gfunc_param_typed(s1, s, sa);
                    nb_args++;
                    if (sa)
                        sa = sa->next;
                    if (s1->tok == ')')
                        break;
                    skip(s1, ',');
                }
            }
            if (sa)
                tcc_error(s1, "too few arguments to function");
            skip(s1, ')');
            gfunc_call(s1, nb_args);

            /* return value */
            for (r = ret.r + ret_nregs + !ret_nregs; r-- > ret.r;) {
                vsetc(s1, &ret.type, r, &ret.c);
                s1->vtop->r2 = ret.r2; /* Loop only happens when r2 is VT_CONST */
            }

            /* handle packed struct return */
            if (((s->type.t & VT_BTYPE) == VT_STRUCT) && ret_nregs) {
                int addr, offset;

                size = type_size(&s->type, &align);
        /* We're writing whole regs often, make sure there's enough
           space.  Assume register size is power of 2.  */
        if (regsize > align)
          align = regsize;
                s1->loc = (s1->loc - size) & -align;
                addr = s1->loc;
                offset = 0;
                for (;;) {
                    vset(s1, &ret.type, VT_LOCAL | VT_LVAL, addr + offset);
                    vswap(s1);
                    vstore(s1);
                    s1->vtop--;
                    if (--ret_nregs == 0)
                        break;
                    offset += regsize;
                }
                vset(s1, &s->type, VT_LOCAL | VT_LVAL, addr);
            }
        } else {
            break;
        }
    }
}

ST_FUNC void expr_prod(TCCState *s1)
{
    int t;

    unary(s1);
    while (s1->tok == '*' || s1->tok == '/' || s1->tok == '%') {
        t = s1->tok;
        next(s1);
        unary(s1);
        gen_op(s1, t);
    }
}

ST_FUNC void expr_sum(TCCState *s1)
{
    int t;

    expr_prod(s1);
    while (s1->tok == '+' || s1->tok == '-') {
        t = s1->tok;
        next(s1);
        expr_prod(s1);
        gen_op(s1, t);
    }
}

static void expr_shift(TCCState *s1)
{
    int t;

    expr_sum(s1);
    while (s1->tok == TOK_SHL || s1->tok == TOK_SAR) {
        t = s1->tok;
        next(s1);
        expr_sum(s1);
        gen_op(s1, t);
    }
}

static void expr_cmp(TCCState *s1)
{
    int t;

    expr_shift(s1);
    while ((s1->tok >= TOK_ULE && s1->tok <= TOK_GT) ||
           s1->tok == TOK_ULT || s1->tok == TOK_UGE) {
        t = s1->tok;
        next(s1);
        expr_shift(s1);
        gen_op(s1, t);
    }
}

static void expr_cmpeq(TCCState *s1)
{
    int t;

    expr_cmp(s1);
    while (s1->tok == TOK_EQ || s1->tok == TOK_NE) {
        t = s1->tok;
        next(s1);
        expr_cmp(s1);
        gen_op(s1, t);
    }
}

static void expr_and(TCCState *s1)
{
    expr_cmpeq(s1);
    while (s1->tok == '&') {
        next(s1);
        expr_cmpeq(s1);
        gen_op(s1, '&');
    }
}

static void expr_xor(TCCState *s1)
{
    expr_and(s1);
    while (s1->tok == '^') {
        next(s1);
        expr_and(s1);
        gen_op(s1, '^');
    }
}

static void expr_or(TCCState *s1)
{
    expr_xor(s1);
    while (s1->tok == '|') {
        next(s1);
        expr_xor(s1);
        gen_op(s1, '|');
    }
}

static void expr_land(TCCState *s1)
{
    expr_or(s1);
    if (s1->tok == TOK_LAND) {
    int t = 0;
    for(;;) {
        if ((s1->vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST) {
                gen_cast_s(s1, VT_BOOL);
        if (s1->vtop->c.i) {
            vpop(s1);
        } else {
            s1->nocode_wanted++;
            while (s1->tok == TOK_LAND) {
            next(s1);
            expr_or(s1);
            vpop(s1);
            }
            s1->nocode_wanted--;
            if (t)
              gsym(s1, t);
            gen_cast_s(s1, VT_INT);
            break;
        }
        } else {
        if (!t)
          save_regs(s1, 1);
        t = gvtst(s1, 1, t);
        }
        if (s1->tok != TOK_LAND) {
        if (t)
          vseti(s1, VT_JMPI, t);
        else
          vpushi(s1, 1);
        break;
        }
        next(s1);
        expr_or(s1);
    }
    }
}

static void expr_lor(TCCState *s1)
{
    expr_land(s1);
    if (s1->tok == TOK_LOR) {
    int t = 0;
    for(;;) {
        if ((s1->vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST) {
                gen_cast_s(s1, VT_BOOL);
        if (!s1->vtop->c.i) {
            vpop(s1);
        } else {
            s1->nocode_wanted++;
            while (s1->tok == TOK_LOR) {
            next(s1);
            expr_land(s1);
            vpop(s1);
            }
            s1->nocode_wanted--;
            if (t)
              gsym(s1, t);
            gen_cast_s(s1, VT_INT);
            break;
        }
        } else {
        if (!t)
          save_regs(s1, 1);
        t = gvtst(s1, 0, t);
        }
        if (s1->tok != TOK_LOR) {
        if (t)
          vseti(s1, VT_JMP, t);
        else
          vpushi(s1, 0);
        break;
        }
        next(s1);
        expr_land(s1);
    }
    }
}

/* Assuming s1->vtop is a value used in a conditional context
   (i.e. compared with zero) return 0 if it's false, 1 if
   true and -1 if it can't be statically determined.  */
static int condition_3way(TCCState *s1)
{
    int c = -1;
    if ((s1->vtop->r & (VT_VALMASK | VT_LVAL)) == VT_CONST &&
    (!(s1->vtop->r & VT_SYM) || !s1->vtop->sym->a.weak)) {
    vdup(s1);
        gen_cast_s(s1, VT_BOOL);
    c = s1->vtop->c.i;
    vpop(s1);
    }
    return c;
}

static void expr_cond(TCCState *s1)
{
    int tt, u, r1, r2, rc, t1, t2, bt1, bt2, islv, c, g;
    SValue sv;
    CType type, type1, type2;

    expr_lor(s1);
    if (s1->tok == '?') {
        next(s1);
    c = condition_3way(s1);
        g = (s1->tok == ':' && s1->gnu_ext);
        if (c < 0) {
            /* needed to avoid having different registers saved in
               each branch */
            if (is_float(s1->vtop->type.t)) {
                rc = RC_FLOAT;
#ifdef TCC_TARGET_X86_64
                if ((s1->vtop->type.t & VT_BTYPE) == VT_LDOUBLE) {
                    rc = RC_ST0;
                }
#endif
            } else
                rc = RC_INT;
            gv(s1, rc);
            save_regs(s1, 1);
            if (g)
                gv_dup(s1);
            tt = gvtst(s1, 1, 0);

        } else {
            if (!g)
                vpop(s1);
            tt = 0;
        }

        if (1) {
            if (c == 0)
                s1->nocode_wanted++;
            if (!g)
                gexpr(s1);

            type1 = s1->vtop->type;
            sv = *s1->vtop; /* save value to handle it later */
            s1->vtop--; /* no vpop so that FP stack is not flushed */
            skip(s1, ':');

            u = 0;
            if (c < 0)
                u = gjmp(s1, 0);
            gsym(s1, tt);

            if (c == 0)
                s1->nocode_wanted--;
            if (c == 1)
                s1->nocode_wanted++;
            expr_cond(s1);
            if (c == 1)
                s1->nocode_wanted--;

            type2 = s1->vtop->type;
            t1 = type1.t;
            bt1 = t1 & VT_BTYPE;
            t2 = type2.t;
            bt2 = t2 & VT_BTYPE;
            type.ref = NULL;

            /* cast operands to correct type according to ISOC rules */
            if (is_float(bt1) || is_float(bt2)) {
                if (bt1 == VT_LDOUBLE || bt2 == VT_LDOUBLE) {
                    type.t = VT_LDOUBLE;

                } else if (bt1 == VT_DOUBLE || bt2 == VT_DOUBLE) {
                    type.t = VT_DOUBLE;
                } else {
                    type.t = VT_FLOAT;
                }
            } else if (bt1 == VT_LLONG || bt2 == VT_LLONG) {
                /* cast to biggest op */
                type.t = VT_LLONG | VT_LONG;
                if (bt1 == VT_LLONG)
                    type.t &= t1;
                if (bt2 == VT_LLONG)
                    type.t &= t2;
                /* convert to unsigned if it does not fit in a long long */
                if ((t1 & (VT_BTYPE | VT_UNSIGNED | VT_BITFIELD)) == (VT_LLONG | VT_UNSIGNED) ||
                    (t2 & (VT_BTYPE | VT_UNSIGNED | VT_BITFIELD)) == (VT_LLONG | VT_UNSIGNED))
                    type.t |= VT_UNSIGNED;
            } else if (bt1 == VT_PTR || bt2 == VT_PTR) {
        /* If one is a null ptr constant the result type
           is the other.  */
        if (is_null_pointer (s1->vtop))
          type = type1;
        else if (is_null_pointer (&sv))
          type = type2;
                /* XXX: test pointer compatibility, C99 has more elaborate
           rules here.  */
        else
          type = type1;
            } else if (bt1 == VT_FUNC || bt2 == VT_FUNC) {
                /* XXX: test function pointer compatibility */
                type = bt1 == VT_FUNC ? type1 : type2;
            } else if (bt1 == VT_STRUCT || bt2 == VT_STRUCT) {
                /* XXX: test structure compatibility */
                type = bt1 == VT_STRUCT ? type1 : type2;
            } else if (bt1 == VT_VOID || bt2 == VT_VOID) {
                /* NOTE: as an extension, we accept void on only one side */
                type.t = VT_VOID;
            } else {
                /* integer operations */
                type.t = VT_INT | (VT_LONG & (t1 | t2));
                /* convert to unsigned if it does not fit in an integer */
                if ((t1 & (VT_BTYPE | VT_UNSIGNED | VT_BITFIELD)) == (VT_INT | VT_UNSIGNED) ||
                    (t2 & (VT_BTYPE | VT_UNSIGNED | VT_BITFIELD)) == (VT_INT | VT_UNSIGNED))
                    type.t |= VT_UNSIGNED;
            }
            /* keep structs lvalue by transforming `(expr ? a : b)` to `*(expr ? &a : &b)` so
               that `(expr ? a : b).mem` does not error  with "lvalue expected" */
            islv = (s1->vtop->r & VT_LVAL) && (sv.r & VT_LVAL) && VT_STRUCT == (type.t & VT_BTYPE);
            islv &= c < 0;

            /* now we convert second operand */
            if (c != 1) {
                gen_cast(s1, &type);
                if (islv) {
                    mk_pointer(s1, &s1->vtop->type);
                    gaddrof(s1);
                } else if (VT_STRUCT == (s1->vtop->type.t & VT_BTYPE))
                    gaddrof(s1);
            }

            rc = RC_INT;
            if (is_float(type.t)) {
                rc = RC_FLOAT;
#ifdef TCC_TARGET_X86_64
                if ((type.t & VT_BTYPE) == VT_LDOUBLE) {
                    rc = RC_ST0;
                }
#endif
            } else if ((type.t & VT_BTYPE) == VT_LLONG) {
                /* for long longs, we use fixed registers to avoid having
                   to handle a complicated move */
                rc = RC_IRET;
            }

            tt = r2 = 0;
            if (c < 0) {
                r2 = gv(s1, rc);
                tt = gjmp(s1, 0);
            }
            gsym(s1, u);

            /* this is horrible, but we must also convert first
               operand */
            if (c != 0) {
                *s1->vtop = sv;
                gen_cast(s1, &type);
                if (islv) {
                    mk_pointer(s1, &s1->vtop->type);
                    gaddrof(s1);
                } else if (VT_STRUCT == (s1->vtop->type.t & VT_BTYPE))
                    gaddrof(s1);
            }

            if (c < 0) {
                r1 = gv(s1, rc);
                move_reg(s1, r2, r1, type.t);
                s1->vtop->r = r2;
                gsym(s1, tt);
                if (islv)
                    indir(s1);
            }
        }
    }
}

static void expr_eq(TCCState *s1)
{
    int t;

    expr_cond(s1);
    if (s1->tok == '=' ||
        (s1->tok >= TOK_A_MOD && s1->tok <= TOK_A_DIV) ||
        s1->tok == TOK_A_XOR || s1->tok == TOK_A_OR ||
        s1->tok == TOK_A_SHL || s1->tok == TOK_A_SAR) {
        test_lvalue(s1);
        t = s1->tok;
        next(s1);
        if (t == '=') {
            expr_eq(s1);
        } else {
            vdup(s1);
            expr_eq(s1);
            gen_op(s1, t & 0x7f);
        }
        vstore(s1);
    }
}

ST_FUNC void gexpr(TCCState *s1)
{
    while (1) {
        expr_eq(s1);
        if (s1->tok != ',')
            break;
        vpop(s1);
        next(s1);
    }
}

/* parse a constant expression and return value in s1->vtop.  */
static void expr_const1(TCCState *s1)
{
    s1->const_wanted++;
    s1->nocode_wanted++;
    expr_cond(s1);
    s1->nocode_wanted--;
    s1->const_wanted--;
}

/* parse an integer constant and return its value. */
static inline int64_t expr_const64(TCCState *s1)
{
    int64_t c;
    expr_const1(s1);
    if ((s1->vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) != VT_CONST)
        expect(s1, "constant expression");
    c = s1->vtop->c.i;
    vpop(s1);
    return c;
}

/* parse an integer constant and return its value.
   Complain if it doesn't fit 32bit (signed or unsigned).  */
ST_FUNC int expr_const(TCCState *s1)
{
    int c;
    int64_t wc = expr_const64(s1);
    c = wc;
    if (c != wc && (unsigned)c != wc)
        tcc_error(s1, "constant exceeds 32 bit");
    return c;
}

/* return the label token if current token is a label, otherwise
   return zero */
static int is_label(TCCState *s1)
{
    int last_tok;

    /* fast test first */
    if (s1->tok < TOK_UIDENT)
        return 0;
    /* no need to save s1->tokc because s1->tok is an identifier */
    last_tok = s1->tok;
    next(s1);
    if (s1->tok == ':') {
        return last_tok;
    } else {
        unget_tok(s1, last_tok);
        return 0;
    }
}

#ifndef TCC_TARGET_ARM64
static void gfunc_return(TCCState *s1, CType *func_type)
{
    if ((func_type->t & VT_BTYPE) == VT_STRUCT) {
        CType type, ret_type;
        int ret_align, ret_nregs, regsize;
        ret_nregs = gfunc_sret(func_type, s1->func_var, &ret_type,
                               &ret_align, &regsize);
        if (0 == ret_nregs) {
            /* if returning structure, must copy it to implicit
               first pointer arg location */
            type = *func_type;
            mk_pointer(s1, &type);
            vset(s1, &type, VT_LOCAL | VT_LVAL, s1->func_vc);
            indir(s1);
            vswap(s1);
            /* copy structure value to pointer */
            vstore(s1);
        } else {
            /* returning structure packed into registers */
            int r, size, addr, align;
            size = type_size(func_type,&align);
            if ((s1->vtop->r != (VT_LOCAL | VT_LVAL) ||
                 (s1->vtop->c.i & (ret_align-1)))
                && (align & (ret_align-1))) {
                s1->loc = (s1->loc - size) & -ret_align;
                addr = s1->loc;
                type = *func_type;
                vset(s1, &type, VT_LOCAL | VT_LVAL, addr);
                vswap(s1);
                vstore(s1);
                vpop(s1);
                vset(s1, &ret_type, VT_LOCAL | VT_LVAL, addr);
            }
            s1->vtop->type = ret_type;
            if (is_float(ret_type.t))
                r = rc_fret(ret_type.t);
            else
                r = RC_IRET;

            if (ret_nregs == 1)
                gv(s1, r);
            else {
                for (;;) {
                    vdup(s1);
                    gv(s1, r);
                    vpop(s1);
                    if (--ret_nregs == 0)
                      break;
                    /* We assume that when a structure is returned in multiple
                       registers, their classes are consecutive values of the
                       suite s(n) = 2^n */
                    r <<= 1;
                    s1->vtop->c.i += regsize;
                }
            }
        }
    } else if (is_float(func_type->t)) {
        gv(s1, rc_fret(func_type->t));
    } else {
        gv(s1, RC_IRET);
    }
    s1->vtop--; /* NOT vpop(s1) because on x86 it would flush the fp stack */
}
#endif

static int case_cmp(const void *pa, const void *pb)
{
    int64_t a = (*(struct case_t**) pa)->v1;
    int64_t b = (*(struct case_t**) pb)->v1;
    return a < b ? -1 : a > b;
}

static void gcase(TCCState *s1, struct case_t **base, int len, int *bsym)
{
    struct case_t *p;
    int e;
    int ll = (s1->vtop->type.t & VT_BTYPE) == VT_LLONG;
    gv(s1, RC_INT);
    while (len > 4) {
        /* binary search */
        p = base[len/2];
        vdup(s1);
    if (ll)
        vpushll(s1, p->v2);
    else
        vpushi(s1, p->v2);
        gen_op(s1, TOK_LE);
        e = gtst(s1, 1, 0);
        vdup(s1);
    if (ll)
        vpushll(s1, p->v1);
    else
        vpushi(s1, p->v1);
        gen_op(s1, TOK_GE);
        gtst_addr(s1, 0, p->sym); /* v1 <= x <= v2 */
        /* x < v1 */
        gcase(s1, base, len/2, bsym);
        if (s1->cur_switch->def_sym)
            gjmp_addr(s1, s1->cur_switch->def_sym);
        else
            *bsym = gjmp(s1, *bsym);
        /* x > v2 */
        gsym(s1, e);
        e = len/2 + 1;
        base += e; len -= e;
    }
    /* linear scan */
    while (len--) {
        p = *base++;
        vdup(s1);
    if (ll)
        vpushll(s1, p->v2);
    else
        vpushi(s1, p->v2);
        if (p->v1 == p->v2) {
            gen_op(s1, TOK_EQ);
            gtst_addr(s1, 0, p->sym);
        } else {
            gen_op(s1, TOK_LE);
            e = gtst(s1, 1, 0);
            vdup(s1);
        if (ll)
            vpushll(s1, p->v1);
        else
            vpushi(s1, p->v1);
            gen_op(s1, TOK_GE);
            gtst_addr(s1, 0, p->sym);
            gsym(s1, e);
        }
    }
}

static void block(TCCState *s1, int *bsym, int *csym, int is_expr)
{
    int a, b, c, d, cond;
    Sym *s;

    /* generate line number info */
    if (s1->do_debug)
        tcc_debug_line(s1);

    if (is_expr) {
        /* default return value is (void) */
        vpushi(s1, 0);
        s1->vtop->type.t = VT_VOID;
    }

    if (s1->tok == TOK_IF) {
        /* if test */
    int saved_nocode_wanted = s1->nocode_wanted;
        next(s1);
        skip(s1, '(');
        gexpr(s1);
        skip(s1, ')');
    cond = condition_3way(s1);
        if (cond == 1)
            a = 0, vpop(s1);
        else
            a = gvtst(s1, 1, 0);
        if (cond == 0)
        s1->nocode_wanted |= 0x20000000;
        block(s1, bsym, csym, 0);
    if (cond != 1)
        s1->nocode_wanted = saved_nocode_wanted;
        c = s1->tok;
        if (c == TOK_ELSE) {
            next(s1);
            d = gjmp(s1, 0);
            gsym(s1, a);
        if (cond == 1)
            s1->nocode_wanted |= 0x20000000;
            block(s1, bsym, csym, 0);
            gsym(s1, d); /* patch else jmp */
        if (cond != 0)
        s1->nocode_wanted = saved_nocode_wanted;
        } else
            gsym(s1, a);
    } else if (s1->tok == TOK_WHILE) {
    int saved_nocode_wanted;
    s1->nocode_wanted &= ~0x20000000;
        next(s1);
        d = s1->ind;
        vla_sp_restore(s1);
        skip(s1, '(');
        gexpr(s1);
        skip(s1, ')');
        a = gvtst(s1, 1, 0);
        b = 0;
        ++s1->local_scope;
    saved_nocode_wanted = s1->nocode_wanted;
        block(s1, &a, &b, 0);
    s1->nocode_wanted = saved_nocode_wanted;
        --s1->local_scope;
        gjmp_addr(s1, d);
        gsym(s1, a);
        gsym_addr(s1, b, d);
    } else if (s1->tok == '{') {
        Sym *llabel;
        int block_vla_sp_loc = s1->vla_sp_loc, saved_vlas_in_scope = s1->vlas_in_scope;

        next(s1);
        /* record local declaration stack position */
        s = s1->local_stack;
        llabel = s1->local_label_stack;
        ++s1->local_scope;

        /* handle local labels declarations */
        if (s1->tok == TOK_LABEL) {
            next(s1);
            for(;;) {
                if (s1->tok < TOK_UIDENT)
                    expect(s1, "label identifier");
                label_push(s1, &s1->local_label_stack, s1->tok, LABEL_DECLARED);
                next(s1);
                if (s1->tok == ',') {
                    next(s1);
                } else {
                    skip(s1, ';');
                    break;
                }
            }
        }
        while (s1->tok != '}') {
        if ((a = is_label(s1)))
        unget_tok(s1, a);
        else
            decl(s1, VT_LOCAL);
            if (s1->tok != '}') {
                if (is_expr)
                    vpop(s1);
                block(s1, bsym, csym, is_expr);
            }
        }
        /* pop locally defined labels */
        label_pop(s1, &s1->local_label_stack, llabel, is_expr);
        /* pop locally defined symbols */
        --s1->local_scope;
    /* In the is_expr case (a statement expression is finished here),
       s1->vtop might refer to symbols on the s1->local_stack.  Either via the
       type or via s1->vtop->sym.  We can't pop those nor any that in turn
       might be referred to.  To make it easier we don't roll back
       any symbols in that case; some upper level call to block() will
       do that.  We do have to remove such symbols from the lookup
       tables, though.  sym_pop will do that.  */
    sym_pop(s1, &s1->local_stack, s, is_expr);

        /* Pop VLA frames and restore stack pointer if required */
        if (s1->vlas_in_scope > saved_vlas_in_scope) {
            s1->vla_sp_loc = saved_vlas_in_scope ? block_vla_sp_loc : s1->vla_sp_root_loc;
            vla_sp_restore(s1);
        }
        s1->vlas_in_scope = saved_vlas_in_scope;

        next(s1);
    } else if (s1->tok == TOK_RETURN) {
        next(s1);
        if (s1->tok != ';') {
            gexpr(s1);
            gen_assign_cast(s1, &s1->func_vt);
            if ((s1->func_vt.t & VT_BTYPE) == VT_VOID)
                s1->vtop--;
            else
                gfunc_return(s1, &s1->func_vt);
        }
        skip(s1, ';');
        /* jump unless last stmt in top-level block */
        if (s1->tok != '}' || s1->local_scope != 1)
            s1->rsym = gjmp(s1, s1->rsym);
    s1->nocode_wanted |= 0x20000000;
    } else if (s1->tok == TOK_BREAK) {
        /* compute jump */
        if (!bsym)
            tcc_error(s1, "cannot break");
        *bsym = gjmp(s1, *bsym);
        next(s1);
        skip(s1, ';');
    s1->nocode_wanted |= 0x20000000;
    } else if (s1->tok == TOK_CONTINUE) {
        /* compute jump */
        if (!csym)
            tcc_error(s1, "cannot continue");
        vla_sp_restore_root(s1);
        *csym = gjmp(s1, *csym);
        next(s1);
        skip(s1, ';');
    } else if (s1->tok == TOK_FOR) {
        int e;
    int saved_nocode_wanted;
    s1->nocode_wanted &= ~0x20000000;
        next(s1);
        skip(s1, '(');
        s = s1->local_stack;
        ++s1->local_scope;
        if (s1->tok != ';') {
            /* c99 for-loop init decl? */
            if (!decl0(s1, VT_LOCAL, 1, NULL)) {
                /* no, regular for-loop init expr */
                gexpr(s1);
                vpop(s1);
            }
        }
        skip(s1, ';');
        d = s1->ind;
        c = s1->ind;
        vla_sp_restore(s1);
        a = 0;
        b = 0;
        if (s1->tok != ';') {
            gexpr(s1);
            a = gvtst(s1, 1, 0);
        }
        skip(s1, ';');
        if (s1->tok != ')') {
            e = gjmp(s1, 0);
            c = s1->ind;
            vla_sp_restore(s1);
            gexpr(s1);
            vpop(s1);
            gjmp_addr(s1, d);
            gsym(s1, e);
        }
        skip(s1, ')');
    saved_nocode_wanted = s1->nocode_wanted;
        block(s1, &a, &b, 0);
    s1->nocode_wanted = saved_nocode_wanted;
        gjmp_addr(s1, c);
        gsym(s1, a);
        gsym_addr(s1, b, c);
        --s1->local_scope;
        sym_pop(s1, &s1->local_stack, s, 0);

    } else
    if (s1->tok == TOK_DO) {
    int saved_nocode_wanted;
    s1->nocode_wanted &= ~0x20000000;
        next(s1);
        a = 0;
        b = 0;
        d = s1->ind;
        vla_sp_restore(s1);
    saved_nocode_wanted = s1->nocode_wanted;
        block(s1, &a, &b, 0);
        skip(s1, TOK_WHILE);
        skip(s1, '(');
        gsym(s1, b);
    gexpr(s1);
    c = gvtst(s1, 0, 0);
    gsym_addr(s1, c, d);
    s1->nocode_wanted = saved_nocode_wanted;
        skip(s1, ')');
        gsym(s1, a);
        skip(s1, ';');
    } else
    if (s1->tok == TOK_SWITCH) {
        struct switch_t *saved, sw;
    int saved_nocode_wanted = s1->nocode_wanted;
    SValue switchval;
        next(s1);
        skip(s1, '(');
        gexpr(s1);
        skip(s1, ')');
    switchval = *s1->vtop--;
        a = 0;
        b = gjmp(s1, 0); /* jump to first case */
        sw.p = NULL; sw.n = 0; sw.def_sym = 0;
        saved = s1->cur_switch;
        s1->cur_switch = &sw;
        block(s1, &a, csym, 0);
    s1->nocode_wanted = saved_nocode_wanted;
        a = gjmp(s1, a); /* add implicit break */
        /* case lookup */
        gsym(s1, b);
        qsort(sw.p, sw.n, sizeof(void*), case_cmp);
        for (b = 1; b < sw.n; b++)
            if (sw.p[b - 1]->v2 >= sw.p[b]->v1)
                tcc_error(s1, "duplicate case value");
        /* Our switch table sorting is signed, so the compared
           value needs to be as well when it's 64bit.  */
        if ((switchval.type.t & VT_BTYPE) == VT_LLONG)
            switchval.type.t &= ~VT_UNSIGNED;
        vpushv(s1, &switchval);
        gcase(s1, sw.p, sw.n, &a);
        vpop(s1);
        if (sw.def_sym)
          gjmp_addr(s1, sw.def_sym);
        dynarray_reset(s1, &sw.p, &sw.n);
        s1->cur_switch = saved;
        /* break label */
        gsym(s1, a);
    } else
    if (s1->tok == TOK_CASE) {
        struct case_t *cr = tcc_malloc(s1, sizeof(struct case_t));
        if (!s1->cur_switch)
            expect(s1, "switch");
    s1->nocode_wanted &= ~0x20000000;
        next(s1);
        cr->v1 = cr->v2 = expr_const64(s1);
        if (s1->gnu_ext && s1->tok == TOK_DOTS) {
            next(s1);
            cr->v2 = expr_const64(s1);
            if (cr->v2 < cr->v1)
                tcc_warning(s1, "empty case range");
        }
        cr->sym = s1->ind;
        dynarray_add(s1, &s1->cur_switch->p, &s1->cur_switch->n, cr);
        skip(s1, ':');
        is_expr = 0;
        goto block_after_label;
    } else
    if (s1->tok == TOK_DEFAULT) {
        next(s1);
        skip(s1, ':');
        if (!s1->cur_switch)
            expect(s1, "switch");
        if (s1->cur_switch->def_sym)
            tcc_error(s1, "too many 'default'");
        s1->cur_switch->def_sym = s1->ind;
        is_expr = 0;
        goto block_after_label;
    } else
    if (s1->tok == TOK_GOTO) {
        next(s1);
        if (s1->tok == '*' && s1->gnu_ext) {
            /* computed goto */
            next(s1);
            gexpr(s1);
            if ((s1->vtop->type.t & VT_BTYPE) != VT_PTR)
                expect(s1, "pointer");
            ggoto(s1);
        } else if (s1->tok >= TOK_UIDENT) {
            s = label_find(s1, s1->tok);
            /* put forward definition if needed */
            if (!s) {
                s = label_push(s1, &s1->global_label_stack, s1->tok, LABEL_FORWARD);
            } else {
                if (s->r == LABEL_DECLARED)
                    s->r = LABEL_FORWARD;
            }
            vla_sp_restore_root(s1);
        if (s->r & LABEL_FORWARD)
                s->jnext = gjmp(s1, s->jnext);
            else
                gjmp_addr(s1, s->jnext);
            next(s1);
        } else {
            expect(s1, "label identifier");
        }
        skip(s1, ';');
    } else if (s1->tok == TOK_ASM1 || s1->tok == TOK_ASM2 || s1->tok == TOK_ASM3) {
        asm_instr(s1);
    } else {
        b = is_label(s1);
        if (b) {
            /* label case */
        next(s1);
            s = label_find(s1, b);
            if (s) {
                if (s->r == LABEL_DEFINED)
                    tcc_error(s1, "duplicate label '%s'", get_tok_str(s1, s->v, NULL));
                gsym(s1, s->jnext);
                s->r = LABEL_DEFINED;
            } else {
                s = label_push(s1, &s1->global_label_stack, b, LABEL_DEFINED);
            }
            s->jnext = s1->ind;
            vla_sp_restore(s1);
            /* we accept this, but it is a mistake */
        block_after_label:
        s1->nocode_wanted &= ~0x20000000;
            if (s1->tok == '}') {
                tcc_warning(s1, "deprecated use of label at end of compound statement");
            } else {
                if (is_expr)
                    vpop(s1);
                block(s1, bsym, csym, is_expr);
            }
        } else {
            /* expression case */
            if (s1->tok != ';') {
                if (is_expr) {
                    vpop(s1);
                    gexpr(s1);
                } else {
                    gexpr(s1);
                    vpop(s1);
                }
            }
            skip(s1, ';');
        }
    }
}

/* This skips over a stream of tokens containing balanced {} and ()
   pairs, stopping at outer ',' ';' and '}' (or matching '}' if we started
   with a '{').  If STR then allocates and stores the skipped tokens
   in *STR.  This doesn't check if () and {} are nested correctly,
   i.e. "({)}" is accepted.  */
static void skip_or_save_block(TCCState *s1, TokenString **str)
{
    int braces = s1->tok == '{';
    int level = 0;
    if (str)
        *str = tok_str_alloc(s1);

    while ((level > 0 || (s1->tok != '}' && s1->tok != ',' && s1->tok != ';' && s1->tok != ')'))) {
        int t;
        if (s1->tok == TOK_EOF) {
             if (str || level > 0)
                tcc_error(s1, "unexpected end of file");
             else
                break;
        }
        if (str)
            tok_str_add_tok(s1, *str);
        t = s1->tok;
        next(s1);
        if (t == '{' || t == '(') {
            level++;
        } else if (t == '}' || t == ')') {
            level--;
            if (level == 0 && braces && t == '}')
                break;
        }
    }
    if (str) {
        tok_str_add(s1, *str, -1);
        tok_str_add(s1, *str, 0);
    }
}

#define EXPR_CONST 1
#define EXPR_ANY   2

static void parse_init_elem(TCCState *s1, int expr_type)
{
    int saved_global_expr;
    switch(expr_type) {
    case EXPR_CONST:
        /* compound literals must be allocated globally in this case */
        saved_global_expr = s1->global_expr;
        s1->global_expr = 1;
        expr_const1(s1);
        s1->global_expr = saved_global_expr;
        /* NOTE: symbols are accepted, as well as lvalue for anon symbols
       (compound literals).  */
        if (((s1->vtop->r & (VT_VALMASK | VT_LVAL)) != VT_CONST
         && ((s1->vtop->r & (VT_SYM|VT_LVAL)) != (VT_SYM|VT_LVAL)
         || s1->vtop->sym->v < SYM_FIRST_ANOM))
#ifdef TCC_TARGET_PE
                 || ((s1->vtop->r & VT_SYM) && s1->vtop->sym->a.dllimport)
#endif
            )
            tcc_error(s1, "initializer element is not constant");
        break;
    case EXPR_ANY:
        expr_eq(s1);
        break;
    }
}

/* put zeros for variable based init */
static void init_putz(TCCState *s1, Section *sec, unsigned long c, int size)
{
    if (sec) {
        /* nothing to do because globals are already set to zero */
    } else {
        vpush_global_sym(s1, &s1->func_old_type, TOK_memset);
        vseti(s1, VT_LOCAL, c);
#ifdef TCC_TARGET_ARM
        vpushs(s1, size);
        vpushi(s1, 0);
#else
        vpushi(s1, 0);
        vpushs(s1, size);
#endif
        gfunc_call(s1, 3);
    }
}

/* t is the array or struct type. c is the array or struct
   address. cur_field is the pointer to the current
   field, for arrays the 'c' member contains the current start
   index.  'size_only' is true if only size info is needed (only used
   in arrays).  al contains the already initialized length of the
   current container (starting at c).  This returns the new length of that.  */
static int decl_designator(TCCState *s1, CType *type, Section *sec, unsigned long c,
                           Sym **cur_field, int size_only, int al)
{
    Sym *s, *f;
    int index, index_last, align, l, nb_elems, elem_size;
    unsigned long corig = c;

    elem_size = 0;
    nb_elems = 1;
    if (s1->gnu_ext && (l = is_label(s1)) != 0)
        goto struct_field;
    /* NOTE: we only support ranges for last designator */
    while (nb_elems == 1 && (s1->tok == '[' || s1->tok == '.')) {
        if (s1->tok == '[') {
            if (!(type->t & VT_ARRAY))
                expect(s1, "array type");
            next(s1);
            index = index_last = expr_const(s1);
            if (s1->tok == TOK_DOTS && s1->gnu_ext) {
                next(s1);
                index_last = expr_const(s1);
            }
            skip(s1, ']');
            s = type->ref;
        if (index < 0 || (s->c >= 0 && index_last >= s->c) ||
        index_last < index)
            tcc_error(s1, "invalid index");
            if (cur_field)
        (*cur_field)->c = index_last;
            type = pointed_type(type);
            elem_size = type_size(type, &align);
            c += index * elem_size;
            nb_elems = index_last - index + 1;
        } else {
            next(s1);
            l = s1->tok;
        struct_field:
            next(s1);
            if ((type->t & VT_BTYPE) != VT_STRUCT)
                expect(s1, "struct/union type");
        f = find_field(type, l);
            if (!f)
                expect(s1, "field");
            if (cur_field)
                *cur_field = f;
        type = &f->type;
            c += f->c;
        }
        cur_field = NULL;
    }
    if (!cur_field) {
        if (s1->tok == '=') {
            next(s1);
        } else if (!s1->gnu_ext) {
        expect(s1, "=");
        }
    } else {
        if (type->t & VT_ARRAY) {
        index = (*cur_field)->c;
        if (type->ref->c >= 0 && index >= type->ref->c)
            tcc_error(s1, "index too large");
            type = pointed_type(type);
            c += index * type_size(type, &align);
        } else {
            f = *cur_field;
        while (f && (f->v & SYM_FIRST_ANOM) && (f->type.t & VT_BITFIELD))
            *cur_field = f = f->next;
            if (!f)
                tcc_error(s1, "too many field init");
        type = &f->type;
            c += f->c;
        }
    }
    /* must put zero in holes (note that doing it that way
       ensures that it even works with designators) */
    if (!size_only && c - corig > al)
    init_putz(s1, sec, corig + al, c - corig - al);
    decl_initializer(s1, type, sec, c, 0, size_only);

    /* XXX: make it more general */
    if (!size_only && nb_elems > 1) {
        unsigned long c_end;
        uint8_t *src, *dst;
        int i;

        if (!sec) {
        vset(s1, type, VT_LOCAL|VT_LVAL, c);
        for (i = 1; i < nb_elems; i++) {
        vset(s1, type, VT_LOCAL|VT_LVAL, c + elem_size * i);
        vswap(s1);
        vstore(s1);
        }
        vpop(s1);
        } else if (!NODATA_WANTED) {
        c_end = c + nb_elems * elem_size;
        if (c_end > sec->data_allocated)
            section_realloc(s1, sec, c_end);
        src = sec->data + c;
        dst = src;
        for(i = 1; i < nb_elems; i++) {
        dst += elem_size;
        memcpy(dst, src, elem_size);
        }
    }
    }
    c += nb_elems * type_size(type, &align);
    if (c - corig > al)
      al = c - corig;
    return al;
}

/* store a value or an expression directly in global data or in local array */
static void init_putv(TCCState *s1, CType *type, Section *sec, unsigned long c)
{
    int bt;
    void *ptr;
    CType dtype;

    dtype = *type;
    dtype.t &= ~VT_CONSTANT; /* need to do that to avoid false warning */

    if (sec) {
    int size, align;
        /* XXX: not portable */
        /* XXX: generate error if incorrect relocation */
        gen_assign_cast(s1, &dtype);
        bt = type->t & VT_BTYPE;

        if ((s1->vtop->r & VT_SYM)
            && bt != VT_PTR
            && bt != VT_FUNC
            && (bt != (PTR_SIZE == 8 ? VT_LLONG : VT_INT)
                || (type->t & VT_BITFIELD))
            && !((s1->vtop->r & VT_CONST) && s1->vtop->sym->v >= SYM_FIRST_ANOM)
            )
            tcc_error(s1, "initializer element is not computable at load time");

        if (NODATA_WANTED) {
            s1->vtop--;
            return;
        }

    size = type_size(type, &align);
    section_reserve(s1, sec, c + size);
        ptr = sec->data + c;

        /* XXX: make code faster ? */
    if ((s1->vtop->r & (VT_SYM|VT_CONST)) == (VT_SYM|VT_CONST) &&
        s1->vtop->sym->v >= SYM_FIRST_ANOM &&
        /* XXX This rejects compound literals like
           '(void *){ptr}'.  The problem is that '&sym' is
           represented the same way, which would be ruled out
           by the SYM_FIRST_ANOM check above, but also '"string"'
           in 'char *p = "string"' is represented the same
           with the type being VT_PTR and the symbol being an
           anonymous one.  That is, there's no difference in s1->vtop
           between '(void *){x}' and '&(void *){x}'.  Ignore
           pointer typed entities here.  Hopefully no real code
           will every use compound literals with scalar type.  */
        (s1->vtop->type.t & VT_BTYPE) != VT_PTR) {
        /* These come from compound literals, memcpy stuff over.  */
        Section *ssec;
        ElfSym *esym;
        ElfW_Rel *rel;
        esym = elfsym(s1, s1->vtop->sym);
        ssec = s1->sections[esym->st_shndx];
        memmove (ptr, ssec->data + esym->st_value, size);
        if (ssec->reloc) {
        /* We need to copy over all memory contents, and that
           includes relocations.  Use the fact that relocs are
           created it order, so look from the end of relocs
           until we hit one before the copied region.  */
        int num_relocs = ssec->reloc->data_offset / sizeof(*rel);
        rel = (ElfW_Rel*)(ssec->reloc->data + ssec->reloc->data_offset);
        while (num_relocs--) {
            rel--;
            if (rel->r_offset >= esym->st_value + size)
              continue;
            if (rel->r_offset < esym->st_value)
              break;
            /* Note: if the same fields are initialized multiple
               times (possible with designators) then we possibly
               add multiple relocations for the same offset here.
               That would lead to wrong code, the last reloc needs
               to win.  We clean this up later after the whole
               initializer is parsed.  */
            put_elf_reloca(s1, s1->symtab_section, sec,
                   c + rel->r_offset - esym->st_value,
                   ELFW(R_TYPE)(rel->r_info),
                   ELFW(R_SYM)(rel->r_info),
#if PTR_SIZE == 8
                   rel->r_addend
#else
                   0
#endif
                  );
        }
        }
    } else {
            if (type->t & VT_BITFIELD) {
                int bit_pos, bit_size, bits, n;
                unsigned char *p, v, m;
                bit_pos = BIT_POS(s1->vtop->type.t);
                bit_size = BIT_SIZE(s1->vtop->type.t);
                p = (unsigned char*)ptr + (bit_pos >> 3);
                bit_pos &= 7, bits = 0;
                while (bit_size) {
                    n = 8 - bit_pos;
                    if (n > bit_size)
                        n = bit_size;
                    v = s1->vtop->c.i >> bits << bit_pos;
                    m = ((1 << n) - 1) << bit_pos;
                    *p = (*p & ~m) | (v & m);
                    bits += n, bit_size -= n, bit_pos = 0, ++p;
                }
            } else
            switch(bt) {
        /* XXX: when cross-compiling we assume that each type has the
           same representation on host and target, which is likely to
           be wrong in the case of long double */
        case VT_BOOL:
        s1->vtop->c.i = s1->vtop->c.i != 0;
        case VT_BYTE:
        *(char *)ptr |= s1->vtop->c.i;
        break;
        case VT_SHORT:
        *(short *)ptr |= s1->vtop->c.i;
        break;
        case VT_FLOAT:
        *(float*)ptr = s1->vtop->c.f;
        break;
        case VT_DOUBLE:
        *(double *)ptr = s1->vtop->c.d;
        break;
        case VT_LDOUBLE:
#if defined TCC_IS_NATIVE_387
                if (sizeof (long double) >= 10) /* zero pad ten-byte LD */
                    memcpy(ptr, &s1->vtop->c.ld, 10);
#ifdef __TINYC__
                else if (sizeof (long double) == sizeof (double))
                    __asm__("fldl %1\nfstpt %0\n" : "=m" (*ptr) : "m" (s1->vtop->c.ld));
#endif
                else if (s1->vtop->c.ld == 0.0)
                    ;
                else
#endif
                if (sizeof(long double) == LDOUBLE_SIZE)
            *(long double*)ptr = s1->vtop->c.ld;
                else if (sizeof(double) == LDOUBLE_SIZE)
            *(double *)ptr = (double)s1->vtop->c.ld;
                else
                    tcc_error(s1, "can't cross compile long double constants");
        break;
#if PTR_SIZE != 8
        case VT_LLONG:
        *(long long *)ptr |= s1->vtop->c.i;
        break;
#else
        case VT_LLONG:
#endif
        case VT_PTR:
        {
            addr_t val = s1->vtop->c.i;
#if PTR_SIZE == 8
            if (s1->vtop->r & VT_SYM)
              greloca(s1, sec, s1->vtop->sym, c, R_DATA_PTR, val);
            else
              *(addr_t *)ptr |= val;
#else
            if (s1->vtop->r & VT_SYM)
              greloc(s1, sec, s1->vtop->sym, c, R_DATA_PTR);
            *(addr_t *)ptr |= val;
#endif
            break;
        }
        default:
        {
            int val = s1->vtop->c.i;
#if PTR_SIZE == 8
            if (s1->vtop->r & VT_SYM)
              greloca(s1, sec, s1->vtop->sym, c, R_DATA_PTR, val);
            else
              *(int *)ptr |= val;
#else
            if (s1->vtop->r & VT_SYM)
              greloc(s1, sec, s1->vtop->sym, c, R_DATA_PTR);
            *(int *)ptr |= val;
#endif
            break;
        }
        }
    }
        s1->vtop--;
    } else {
        vset(s1, &dtype, VT_LOCAL|VT_LVAL, c);
        vswap(s1);
        vstore(s1);
        vpop(s1);
    }
}

/* 't' contains the type and storage info. 'c' is the offset of the
   object in section 'sec'. If 'sec' is NULL, it means stack based
   allocation. 'first' is true if array '{' must be read (multi
   dimension implicit array init handling). 'size_only' is true if
   size only evaluation is wanted (only for arrays). */
static void decl_initializer(TCCState *s1, CType *type, Section *sec, unsigned long c,
                             int first, int size_only)
{
    int len, n, no_oblock, nb, i;
    int size1, align1;
    int have_elem;
    Sym *s, *f;
    Sym indexsym;
    CType *t1;

    /* If we currently are at an '}' or ',' we have read an initializer
       element in one of our callers, and not yet consumed it.  */
    have_elem = s1->tok == '}' || s1->tok == ',';
    if (!have_elem && s1->tok != '{' &&
    /* In case of strings we have special handling for arrays, so
       don't consume them as initializer value (which would commit them
       to some anonymous symbol).  */
    s1->tok != TOK_LSTR && s1->tok != TOK_STR &&
    !size_only) {
    parse_init_elem(s1, !sec ? EXPR_ANY : EXPR_CONST);
    have_elem = 1;
    }

    if (have_elem &&
    !(type->t & VT_ARRAY) &&
    /* Use i_c_parameter_t, to strip toplevel qualifiers.
       The source type might have VT_CONSTANT set, which is
       of course assignable to non-const elements.  */
    is_compatible_unqualified_types(type, &s1->vtop->type)) {
        init_putv(s1, type, sec, c);
    } else if (type->t & VT_ARRAY) {
        s = type->ref;
        n = s->c;
        t1 = pointed_type(type);
        size1 = type_size(t1, &align1);

        no_oblock = 1;
        if ((first && s1->tok != TOK_LSTR && s1->tok != TOK_STR) ||
            s1->tok == '{') {
            if (s1->tok != '{')
                tcc_error(s1, "character array initializer must be a literal,"
                    " optionally enclosed in braces");
            skip(s1, '{');
            no_oblock = 0;
        }

        /* only parse strings here if correct type (otherwise: handle
           them as ((w)char *) expressions */
        if ((s1->tok == TOK_LSTR &&
#ifdef TCC_TARGET_PE
             (t1->t & VT_BTYPE) == VT_SHORT && (t1->t & VT_UNSIGNED)
#else
             (t1->t & VT_BTYPE) == VT_INT
#endif
            ) || (s1->tok == TOK_STR && (t1->t & VT_BTYPE) == VT_BYTE)) {
        len = 0;
            while (s1->tok == TOK_STR || s1->tok == TOK_LSTR) {
                int cstr_len, ch;

                /* compute maximum number of chars wanted */
                if (s1->tok == TOK_STR)
                    cstr_len = s1->tokc.str.size;
                else
                    cstr_len = s1->tokc.str.size / sizeof(nwchar_t);
                cstr_len--;
                nb = cstr_len;
                if (n >= 0 && nb > (n - len))
                    nb = n - len;
                if (!size_only) {
                    if (cstr_len > nb)
                        tcc_warning(s1, "initializer-string for array is too long");
                    /* in order to go faster for common case (char
                       string in global variable, we handle it
                       specifically */
                    if (sec && s1->tok == TOK_STR && size1 == 1) {
                        if (!NODATA_WANTED)
                            memcpy(sec->data + c + len, s1->tokc.str.data, nb);
                    } else {
                        for(i=0;i<nb;i++) {
                            if (s1->tok == TOK_STR)
                                ch = ((unsigned char *)s1->tokc.str.data)[i];
                            else
                                ch = ((nwchar_t *)s1->tokc.str.data)[i];
                vpushi(s1, ch);
                            init_putv(s1, t1, sec, c + (len + i) * size1);
                        }
                    }
                }
                len += nb;
                next(s1);
            }
            /* only add trailing zero if enough storage (no
               warning in this case since it is standard) */
            if (n < 0 || len < n) {
                if (!size_only) {
            vpushi(s1, 0);
                    init_putv(s1, t1, sec, c + (len * size1));
                }
                len++;
            }
        len *= size1;
        } else {
        indexsym.c = 0;
        f = &indexsym;

          do_init_list:
        len = 0;
        while (s1->tok != '}' || have_elem) {
        len = decl_designator(s1, type, sec, c, &f, size_only, len);
        have_elem = 0;
        if (type->t & VT_ARRAY) {
            ++indexsym.c;
            /* special test for multi dimensional arrays (may not
               be strictly correct if designators are used at the
               same time) */
            if (no_oblock && len >= n*size1)
                break;
        } else {
            if (s->type.t == VT_UNION)
                f = NULL;
            else
                f = f->next;
            if (no_oblock && f == NULL)
                break;
        }

        if (s1->tok == '}')
            break;
        skip(s1, ',');
        }
        }
        /* put zeros at the end */
    if (!size_only && len < n*size1)
        init_putz(s1, sec, c + len, n*size1 - len);
        if (!no_oblock)
            skip(s1, '}');
        /* patch type size if needed, which happens only for array types */
        if (n < 0)
            s->c = size1 == 1 ? len : ((len + size1 - 1)/size1);
    } else if ((type->t & VT_BTYPE) == VT_STRUCT) {
    size1 = 1;
        no_oblock = 1;
        if (first || s1->tok == '{') {
            skip(s1, '{');
            no_oblock = 0;
        }
        s = type->ref;
        f = s->next;
        n = s->c;
    goto do_init_list;
    } else if (s1->tok == '{') {
        next(s1);
        decl_initializer(s1, type, sec, c, first, size_only);
        skip(s1, '}');
    } else if (size_only) {
    /* If we supported only ISO C we wouldn't have to accept calling
       this on anything than an array size_only==1 (and even then
       only on the outermost level, so no recursion would be needed),
       because initializing a flex array member isn't supported.
       But GNU C supports it, so we need to recurse even into
       subfields of structs and arrays when size_only is set.  */
        /* just skip expression */
        skip_or_save_block(s1, NULL);
    } else {
    if (!have_elem) {
        /* This should happen only when we haven't parsed
           the init element above for fear of committing a
           string constant to memory too early.  */
        if (s1->tok != TOK_STR && s1->tok != TOK_LSTR)
          expect(s1, "string constant");
        parse_init_elem(s1, !sec ? EXPR_ANY : EXPR_CONST);
    }
        init_putv(s1, type, sec, c);
    }
}

/* parse an initializer for type 't' if 'has_init' is non zero, and
   allocate space in local or global data space ('r' is either
   VT_LOCAL or VT_CONST). If 'v' is non zero, then an associated
   variable 'v' of scope 'scope' is declared before initializers
   are parsed. If 'v' is zero, then a reference to the new object
   is put in the value stack. If 'has_init' is 2, a special parsing
   is done to handle string constants. */
static void decl_initializer_alloc(TCCState *s1, CType *type, AttributeDef *ad, int r,
                                   int has_init, int v, int scope)
{
    int size, align, addr;
    TokenString *init_str = NULL;

    Section *sec;
    Sym *flexible_array;
    Sym *sym = NULL;
    int saved_nocode_wanted = s1->nocode_wanted;
#ifdef CONFIG_TCC_BCHECK
    int bcheck = s1->do_bounds_check && !NODATA_WANTED;
#endif

    if (type->t & VT_STATIC)
        s1->nocode_wanted |= NODATA_WANTED ? 0x40000000 : 0x80000000;

    flexible_array = NULL;
    if ((type->t & VT_BTYPE) == VT_STRUCT) {
        Sym *field = type->ref->next;
        if (field) {
            while (field->next)
                field = field->next;
            if (field->type.t & VT_ARRAY && field->type.ref->c < 0)
                flexible_array = field;
        }
    }

    size = type_size(type, &align);
    /* If unknown size, we must evaluate it before
       evaluating initializers because
       initializers can generate global data too
       (e.g. string pointers or ISOC99 compound
       literals). It also simplifies local
       initializers handling */
    if (size < 0 || (flexible_array && has_init)) {
        if (!has_init)
            tcc_error(s1, "unknown type size");
        /* get all init string */
        if (has_init == 2) {
        init_str = tok_str_alloc(s1);
            /* only get strings */
            while (s1->tok == TOK_STR || s1->tok == TOK_LSTR) {
                tok_str_add_tok(s1, init_str);
                next(s1);
            }
        tok_str_add(s1, init_str, -1);
        tok_str_add(s1, init_str, 0);
        } else {
        skip_or_save_block(s1, &init_str);
        }
        unget_tok(s1, 0);

        /* compute size */
        begin_macro(s1, init_str, 1);
        next(s1);
        decl_initializer(s1, type, NULL, 0, 1, 1);
        /* prepare second initializer parsing */
        s1->macro_ptr = init_str->str;
        next(s1);

        /* if still unknown size, error */
        size = type_size(type, &align);
        if (size < 0)
            tcc_error(s1, "unknown type size");
    }
    /* If there's a flex member and it was used in the initializer
       adjust size.  */
    if (flexible_array &&
    flexible_array->type.ref->c > 0)
        size += flexible_array->type.ref->c
            * pointed_size(&flexible_array->type);
    /* take into account specified alignment if bigger */
    if (ad->a.aligned) {
    int speca = 1 << (ad->a.aligned - 1);
        if (speca > align)
            align = speca;
    } else if (ad->a.packed) {
        align = 1;
    }

    if (NODATA_WANTED)
        size = 0, align = 1;

    if ((r & VT_VALMASK) == VT_LOCAL) {
        sec = NULL;
#ifdef CONFIG_TCC_BCHECK
        if (bcheck && (type->t & VT_ARRAY)) {
            s1->loc--;
        }
#endif
        s1->loc = (s1->loc - size) & -align;
        addr = s1->loc;
#ifdef CONFIG_TCC_BCHECK
        /* handles bounds */
        /* XXX: currently, since we do only one pass, we cannot track
           '&' operators, so we add only arrays */
        if (bcheck && (type->t & VT_ARRAY)) {
            addr_t *bounds_ptr;
            /* add padding between regions */
            s1->loc--;
            /* then add local bound info */
            bounds_ptr = section_ptr_add(s1, s1->lbounds_section, 2 * sizeof(addr_t));
            bounds_ptr[0] = addr;
            bounds_ptr[1] = size;
        }
#endif
        if (v) {
            /* local variable */
#ifdef CONFIG_TCC_ASM
        if (ad->asm_label) {
        int reg = asm_parse_regvar(s1, ad->asm_label);
        if (reg >= 0)
            r = (r & ~VT_VALMASK) | reg;
        }
#endif
            sym = sym_push(s1, v, type, r, addr);
            sym->a = ad->a;
        } else {
            /* push local reference */
            vset(s1, type, r, addr);
        }
    } else {
        if (v && scope == VT_CONST) {
            /* see if the symbol was already defined */
            sym = sym_find(s1, v);
            if (sym) {
                patch_storage(s1, sym, ad, type);
                /* we accept several definitions of the same global variable. */
                if (!has_init && sym->c && elfsym(s1, sym)->st_shndx != SHN_UNDEF)
                    goto no_alloc;
            }
        }

        /* allocate symbol in corresponding section */
        sec = ad->section;
        if (!sec) {
            if (has_init)
                sec = s1->data_section;
            else if (s1->nocommon)
                sec = s1->bss_section;
        }

        if (sec) {
        addr = section_add(s1, sec, size, align);
#ifdef CONFIG_TCC_BCHECK
            /* add padding if bound check */
            if (bcheck)
                section_add(s1, sec, 1, 1);
#endif
        } else {
            addr = align; /* SHN_COMMON is special, symbol value is align */
        sec = s1->common_section;
        }

        if (v) {
            if (!sym) {
                sym = sym_push(s1, v, type, r | VT_SYM, 0);
                patch_storage(s1, sym, ad, NULL);
            }
            /* Local statics have a scope until now (for
               warnings), remove it here.  */
            sym->sym_scope = 0;
            /* update symbol definition */
        put_extern_sym(s1, sym, sec, addr, size);
        } else {
            /* push global reference */
            sym = get_sym_ref(s1, type, sec, addr, size);
        vpushsym(s1, type, sym);
        s1->vtop->r |= r;
        }

#ifdef CONFIG_TCC_BCHECK
        /* handles bounds now because the symbol must be defined
           before for the relocation */
        if (bcheck) {
            addr_t *bounds_ptr;

            greloca(s1, s1->bounds_section, sym, s1->bounds_section->data_offset, R_DATA_PTR, 0);
            /* then add global bound info */
            bounds_ptr = section_ptr_add(s1, s1->bounds_section, 2 * sizeof(addr_t));
            bounds_ptr[0] = 0; /* relocated */
            bounds_ptr[1] = size;
        }
#endif
    }

    if (type->t & VT_VLA) {
        int a;

        if (NODATA_WANTED)
            goto no_alloc;

        /* save current stack pointer */
        if (s1->vlas_in_scope == 0) {
            if (s1->vla_sp_root_loc == -1)
                s1->vla_sp_root_loc = (s1->loc -= PTR_SIZE);
            gen_vla_sp_save(s1, s1->vla_sp_root_loc);
        }

        vla_runtime_type_size(s1, type, &a);
        gen_vla_alloc(s1, type, a);
#if defined TCC_TARGET_PE && defined TCC_TARGET_X86_64
        /* on _WIN64, because of the function args scratch area, the
           result of alloca differs from RSP and is returned in RAX.  */
        gen_vla_result(s1, addr), addr = (s1->loc -= PTR_SIZE);
#endif
        gen_vla_sp_save(s1, addr);
        s1->vla_sp_loc = addr;
        s1->vlas_in_scope++;

    } else if (has_init) {
    size_t oldreloc_offset = 0;
    if (sec && sec->reloc)
      oldreloc_offset = sec->reloc->data_offset;
        decl_initializer(s1, type, sec, addr, 1, 0);
    if (sec && sec->reloc)
      squeeze_multi_relocs(sec, oldreloc_offset);
        /* patch flexible array member size back to -1, */
        /* for possible subsequent similar declarations */
        if (flexible_array)
            flexible_array->type.ref->c = -1;
    }

 no_alloc:
    /* restore parse state if needed */
    if (init_str) {
        end_macro(s1);
        next(s1);
    }

    s1->nocode_wanted = saved_nocode_wanted;
}

/* parse a function defined by symbol 'sym' and generate its code in
   's1->cur_text_section' */
static void gen_function(TCCState *s1, Sym *sym)
{
    s1->nocode_wanted = 0;
    s1->ind = s1->cur_text_section->data_offset;
    /* NOTE: we patch the symbol size later */
    put_extern_sym(s1, sym, s1->cur_text_section, s1->ind, 0);
    s1->funcname = get_tok_str(s1, sym->v, NULL);
    s1->func_ind = s1->ind;
    /* Initialize VLA state */
    s1->vla_sp_loc = -1;
    s1->vla_sp_root_loc = -1;
    /* put debug symbol */
    tcc_debug_funcstart(s1, sym);
    /* push a dummy symbol to enable local sym storage */
    sym_push2(s1, &s1->local_stack, SYM_FIELD, 0, 0);
    s1->local_scope = 1; /* for function parameters */
    gfunc_prolog(s1, &sym->type);
    s1->local_scope = 0;
    s1->rsym = 0;
    block(s1, NULL, NULL, 0);
    s1->nocode_wanted = 0;
    gsym(s1, s1->rsym);
    gfunc_epilog(s1);
    s1->cur_text_section->data_offset = s1->ind;
    label_pop(s1, &s1->global_label_stack, NULL, 0);
    /* reset local stack */
    s1->local_scope = 0;
    sym_pop(s1, &s1->local_stack, NULL, 0);
    /* end of function */
    /* patch symbol size */
    elfsym(s1, sym)->st_size = s1->ind - s1->func_ind;
    tcc_debug_funcend(s1, s1->ind - s1->func_ind);
    /* It's better to crash than to generate wrong code */
    s1->cur_text_section = NULL;
    s1->funcname = ""; /* for safety */
    s1->func_vt.t = VT_VOID; /* for safety */
    s1->func_var = 0; /* for safety */
    s1->ind = 0; /* for safety */
    s1->nocode_wanted = 0x80000000;
    check_vstack(s1);
}

static void gen_inline_functions(TCCState *s)
{
    Sym *sym;
    int inline_generated, i, ln;
    struct InlineFunc *fn;

    ln = s->file->line_num;
    /* iterate while inline function are referenced */
    do {
        inline_generated = 0;
        for (i = 0; i < s->nb_inline_fns; ++i) {
            fn = s->inline_fns[i];
            sym = fn->sym;
            if (sym && sym->c) {
                /* the function was used: generate its code and
                   convert it to a normal function */
                fn->sym = NULL;
                if (s->file)
                    pstrcpy(s->file->filename, sizeof s->file->filename, fn->filename);
                sym->type.t &= ~VT_INLINE;

                begin_macro(s, fn->func_str, 1);
                next(s);
                s->cur_text_section = s->text_section;
                gen_function(s, sym);
                end_macro(s);

                inline_generated = 1;
            }
        }
    } while (inline_generated);
    s->file->line_num = ln;
}

ST_FUNC void free_inline_functions(TCCState *s)
{
    int i;
    /* free tokens of unused inline functions */
    for (i = 0; i < s->nb_inline_fns; ++i) {
        struct InlineFunc *fn = s->inline_fns[i];
        if (fn->sym)
            tok_str_free(s, fn->func_str);
    }
    dynarray_reset(s, &s->inline_fns, &s->nb_inline_fns);
}

static char *make_arg_name(TCCState *s1, int arg) {
    char *s = tcc_mallocz(s1, 16);
    snprintf(s, 16, "__arg%d__", arg);
    return s;
}

/* 'l' is VT_LOCAL or VT_CONST to define default storage type, or VT_CMP
   if parsing old style parameter decl list (and FUNC_SYM is set then) */
static int decl0(TCCState *s1, int l, int is_for_loop_init, Sym *func_sym)
{
    int v, has_init, r, i;
    char *argname;
    CType type, btype;
    Sym *sym;
    AttributeDef ad;
    TCCFunction *func;

    while (1) {
        if (!parse_btype(s1, &btype, &ad)) {
            if (is_for_loop_init)
                return 0;
            /* skip redundant ';' if not in old parameter decl scope */
            if (s1->tok == ';' && l != VT_CMP) {
                next(s1);
                continue;
            }
            if (l != VT_CONST)
                break;
            if (s1->tok == TOK_ASM1 || s1->tok == TOK_ASM2 || s1->tok == TOK_ASM3) {
                /* global asm block */
                asm_global_instr(s1);
                continue;
            }
            if (s1->tok >= TOK_UIDENT) {
               /* special test for old K&R protos without explicit int
                  type. Only accepted when defining global data */
                btype.t = VT_INT;
            } else {
                if (s1->tok != TOK_EOF)
                    expect(s1, "declaration");
                break;
            }
        }
        if (s1->tok == ';') {
            if ((btype.t & VT_BTYPE) == VT_STRUCT) {
                int vt = btype.ref->v;
                if (!(vt & SYM_FIELD) && (vt & ~SYM_STRUCT) >= SYM_FIRST_ANOM)
                    tcc_warning(s1, "unnamed struct/union that defines no instances");
                if (btype.ref->next && !s1->local_scope)
                    tcc_resolver_add_type(s1, &btype);
                next(s1);
                continue;
            }
            if (IS_ENUM(btype.t)) {
                if (btype.ref->next && !s1->local_scope)
                    tcc_resolver_add_type(s1, &btype);
                next(s1);
                continue;
            }
        }
        while (1) { /* iterate thru each declaration */
            type = btype;
            /* If the base type itself was an array type of unspecified
               size (like in 'typedef int arr[]; arr x = {1};') then
               we will overwrite the unknown size by the real one for
               this decl.  We need to unshare the ref symbol holding
               that size.  */
            if ((type.t & VT_ARRAY) && type.ref->c < 0) {
                type.ref = sym_push(s1, SYM_FIELD, &type.ref->type, 0, type.ref->c);
            }
            type_decl(s1, &type, &ad, &v, TYPE_DIRECT);

            if ((type.t & VT_BTYPE) == VT_FUNC) {
                if ((type.t & VT_STATIC) && (l == VT_LOCAL)) {
                    tcc_error(s1, "function without file scope cannot be static");
                }
                /* if old style function prototype, we accept a
                   declaration list */
                sym = type.ref;
                if (sym->f.func_type == FUNC_OLD && l == VT_CONST)
                    decl0(s1, VT_CMP, 0, sym);
            }

            if (s1->gnu_ext && (s1->tok == TOK_ASM1 || s1->tok == TOK_ASM2 || s1->tok == TOK_ASM3)) {
                ad.asm_label = asm_label_instr(s1);
                /* parse one last attribute list, after asm label */
                parse_attribute(s1, &ad);
                if (s1->tok == '{')
                    expect(s1, ";");
            }

#ifdef TCC_TARGET_PE
            if (ad.a.dllimport || ad.a.dllexport) {
                if (type.t & (VT_STATIC|VT_TYPEDEF))
                    tcc_error(s1, "cannot have dll linkage with static or typedef");
                if (ad.a.dllimport) {
                    if ((type.t & VT_BTYPE) == VT_FUNC)
                        ad.a.dllimport = 0;
                    else
                        type.t |= VT_EXTERN;
                }
            }
#endif
            if (s1->tok == '{') {
                if (l != VT_CONST)
                    tcc_error(s1, "cannot use local functions");
                if ((type.t & VT_BTYPE) != VT_FUNC)
                    expect(s1, "function definition");

                /* reject abstract declarators in function definition
                   make old style params without decl have int type */
                sym = type.ref;
                while ((sym = sym->next) != NULL) {
                    if (!(sym->v & ~SYM_FIELD))
                        expect(s1, "identifier");
                    if (sym->type.t == VT_VOID)
                        sym->type = s1->int_type;
                }

                /* XXX: cannot do better now: convert extern line to static inline */
                if ((type.t & (VT_EXTERN | VT_INLINE)) == (VT_EXTERN | VT_INLINE))
                    type.t = (type.t & ~VT_EXTERN) | VT_STATIC;

                /* put function symbol */
                sym = external_global_sym(s1, v, &type, 0);
                type.t &= ~VT_EXTERN;
                patch_storage(s1, sym, &ad, &type);

                /* static inline functions are just recorded as a kind
                   of macro. Their code will be emitted at the end of
                   the compilation unit only if they are used */
                if ((type.t & (VT_INLINE | VT_STATIC)) == (VT_INLINE | VT_STATIC)) {
                    struct InlineFunc *fn;
                    const char *filename;

                    filename = s1->file ? s1->file->filename : "";
                    fn = tcc_malloc(s1, sizeof *fn + strlen(filename));
                    strcpy(fn->filename, filename);
                    fn->sym = sym;
                    skip_or_save_block(s1, &fn->func_str);
                    dynarray_add(s1, &s1->inline_fns, &s1->nb_inline_fns, fn);
                } else {
                    /* compute text section */
                    s1->cur_text_section = ad.section;
                    if (!s1->cur_text_section)
                        s1->cur_text_section = s1->text_section;
                    gen_function(s1, sym);
                }
                break;
            } else {
                if (l == VT_CMP) {
                    /* find parameter in function parameter list */
                    for (sym = func_sym->next; sym; sym = sym->next)
                        if ((sym->v & ~SYM_FIELD) == v)
                            goto found;
                    tcc_error(s1, "declaration for parameter '%s' but no such parameter", get_tok_str(s1, v, NULL));
                found:
                    if (type.t & VT_STORAGE) /* 'register' is okay */
                        tcc_error(s1, "storage class specified for '%s'", get_tok_str(s1, v, NULL));
                    if (sym->type.t != VT_VOID)
                        tcc_error(s1, "redefinition of parameter '%s'", get_tok_str(s1, v, NULL));
                    convert_parameter_type(s1, &type);
                    sym->type = type;
                } else if (type.t & VT_TYPEDEF) {
                    /* save typedefed type  */
                    /* XXX: test storage specifiers ? */
                    sym = sym_find(s1, v);
                    if (sym && sym->sym_scope == s1->local_scope) {
                        if (!is_compatible_types(&sym->type, &type) || !(sym->type.t & VT_TYPEDEF))
                            tcc_error(s1, "incompatible redefinition of '%s'", get_tok_str(s1, v, NULL));
                        sym->type = type;
                    } else {
                        if (!s1->local_scope)
                            tcc_resolver_ref_type(s1, &type, get_tok_str(s1, v, NULL));
                        sym = sym_push(s1, v, &type, 0, 0);
                    }
                    sym->a = ad.a;
                    sym->f = ad.f;
                } else {
                    r = 0;
                    if ((type.t & VT_BTYPE) == VT_FUNC) {
                        /* external function definition */
                        /* specific case for func_call attribute */
                        sym = sym_find(s1, v);
                        type.ref->f = ad.f;
                        if (!(type.t & VT_STATIC) && !(sym && sym->sym_scope == s1->local_scope)) {
                            i = 0;
                            sym = type.ref;
                            func = tcc_resolver_add_func(s1, get_tok_str(s1, v, NULL), (sym->f.func_type == FUNC_ELLIPSIS), &sym->type);
                            while ((sym = sym->next) != NULL) {
                                if (sym->type.t == VT_VOID)
                                    sym->type = s1->int_type;
                                if (!(sym->v & ~SYM_FIELD))
                                    argname = make_arg_name(s1, i);
                                else
                                    argname = tcc_strdup(s1, get_tok_str(s1, sym->v & ~SYM_FIELD, NULL));
                                dynarray_add(s1, &func->arg_names, &func->nb_arg_names, argname);
                                dynarray_add(s1, &func->args, &func->nb_args, tcc_resolver_add_type(s1, &sym->type));
                                i++;
                            }
                        }
                    } else if (!(type.t & VT_ARRAY)) {
                        /* not lvalue if array */
                        r |= lvalue_type(type.t);
                    }
                    has_init = (s1->tok == '=');
                    if (has_init && (type.t & VT_VLA))
                        tcc_error(s1, "variable length array cannot be initialized");
                    if (((type.t & VT_EXTERN) && (!has_init || l != VT_CONST)) ||
                        ((type.t & VT_BTYPE) == VT_FUNC) ||
                        ((type.t & VT_ARRAY) && (type.t & VT_STATIC) &&
                         !has_init && l == VT_CONST && type.ref->c < 0)) {
                        /* external variable or function */
                        /* NOTE: as GCC, uninitialized global static
                           arrays of null size are considered as
                           extern */
                        type.t |= VT_EXTERN;
                        sym = external_sym(s1, v, &type, r, &ad);
                        if (ad.alias_target) {
                            ElfSym *esym;
                            Sym *alias_target;
                            alias_target = sym_find(s1, ad.alias_target);
                            esym = elfsym(s1, alias_target);
                            if (!esym)
                                tcc_error(s1, "unsupported forward __alias__ attribute");
                            /* Local statics have a scope until now (for
                               warnings), remove it here.  */
                            sym->sym_scope = 0;
                            put_extern_sym2(s1, sym, esym->st_shndx, esym->st_value, esym->st_size, 0);
                        }
                    } else {
                        if (type.t & VT_STATIC)
                            r |= VT_CONST;
                        else
                            r |= l;
                        if (has_init)
                            next(s1);
                        else if (l == VT_CONST)
                            /* uninitialized global variables may be overridden */
                            type.t |= VT_EXTERN;
                        decl_initializer_alloc(s1, &type, &ad, r, has_init, v, l);
                    }
                }
                if (s1->tok != ',') {
                    if (is_for_loop_init)
                        return 1;
                    skip(s1, ';');
                    break;
                }
                next(s1);
            }
            ad.a.aligned = 0;
        }
    }
    return 0;
}

static void decl(TCCState *s1, int l)
{
    decl0(s1, l, 0, NULL);
}

/* ------------------------------------------------------------------------- */
