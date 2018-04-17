/*
 *  GAS like assembler for TCC
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
#ifdef CONFIG_TCC_ASM

ST_FUNC int asm_get_local_label_name(TCCState *s1, unsigned int n)
{
    char buf[64];
    TokenSym *ts;

    snprintf(buf, sizeof(buf), "L..%u", n);
    ts = tok_alloc(s1, buf, strlen(buf));
    return ts->tok;
}

static int tcc_assemble_internal(TCCState *s1, int do_preprocess, int global);
static Sym* asm_new_label(TCCState *s1, int label, int is_local);
static Sym* asm_new_label1(TCCState *s1, int label, int is_local, int sh_num, int value);

static Sym *asm_label_find(TCCState *s1, int v)
{
    Sym *sym = sym_find(s1, v);
    while (sym && sym->sym_scope)
        sym = sym->prev_tok;
    return sym;
}

static Sym *asm_label_push(TCCState *s1, int v)
{
    /* We always add VT_EXTERN, for sym definition that's tentative
       (for .set, removed for real defs), for mere references it's correct
       as is.  */
    Sym *sym = global_identifier_push(s1, v, VT_ASM | VT_EXTERN | VT_STATIC, 0);
    sym->r = VT_CONST | VT_SYM;
    return sym;
}

/* Return a symbol we can use inside the assembler, having name NAME.
   Symbols from asm and C source share a namespace.  If we generate
   an asm symbol it's also a (file-global) C symbol, but it's
   either not accessible by name (like "L.123"), or its type information
   is such that it's not usable without a proper C declaration.

   Sometimes we need symbols accessible by name from asm, which
   are anonymous in C, in this case CSYM can be used to transfer
   all information from that symbol to the (possibly newly created)
   asm symbol.  */
ST_FUNC Sym* get_asm_sym(TCCState *s1, int name, Sym *csym)
{
    Sym *sym = asm_label_find(s1, name);
    if (!sym) {
	sym = asm_label_push(s1, name);
	if (csym)
	  sym->c = csym->c;
    }
    return sym;
}

static Sym* asm_section_sym(TCCState *s1, Section *sec)
{
    char buf[100];
    int label = tok_alloc(s1, buf,
        snprintf(buf, sizeof buf, "L.%s", sec->name)
        )->tok;
    Sym *sym = asm_label_find(s1, label);
    return sym ? sym : asm_new_label1(s1, label, 1, sec->sh_num, 0);
}

/* We do not use the C expression parser to handle symbols. Maybe the
   C expression parser could be tweaked to do so. */

static void asm_expr_unary(TCCState *s1, ExprValue *pe)
{
    Sym *sym;
    int op, label;
    uint64_t n;
    const char *p;

    switch(s1->tok) {
    case TOK_PPNUM:
        p = s1->tokc.str.data;
        n = strtoull(p, (char **)&p, 0);
        if (*p == 'b' || *p == 'f') {
            /* backward or forward label */
            label = asm_get_local_label_name(s1, n);
            sym = asm_label_find(s1, label);
            if (*p == 'b') {
                /* backward : find the last corresponding defined label */
                if (sym && (!sym->c || elfsym(s1, sym)->st_shndx == SHN_UNDEF))
                    sym = sym->prev_tok;
                if (!sym)
                    tcc_error(s1, "local label '%d' not found backward", n);
            } else {
                /* forward */
                if (!sym || (sym->c && elfsym(s1, sym)->st_shndx != SHN_UNDEF)) {
                    /* if the last label is defined, then define a new one */
		    sym = asm_label_push(s1, label);
                }
            }
	    pe->v = 0;
	    pe->sym = sym;
	    pe->pcrel = 0;
        } else if (*p == '\0') {
            pe->v = n;
            pe->sym = NULL;
	    pe->pcrel = 0;
        } else {
            tcc_error(s1, "invalid number syntax");
        }
        next(s1);
        break;
    case '+':
        next(s1);
        asm_expr_unary(s1, pe);
        break;
    case '-':
    case '~':
        op = s1->tok;
        next(s1);
        asm_expr_unary(s1, pe);
        if (pe->sym)
            tcc_error(s1, "invalid operation with label");
        if (op == '-')
            pe->v = -pe->v;
        else
            pe->v = ~pe->v;
        break;
    case TOK_CCHAR:
    case TOK_LCHAR:
	pe->v = s1->tokc.i;
	pe->sym = NULL;
	pe->pcrel = 0;
	next(s1);
	break;
    case '(':
        next(s1);
        asm_expr(s1, pe);
        skip(s1, ')');
        break;
    case '.':
        pe->v = s1->ind;
        pe->sym = asm_section_sym(s1, s1->cur_text_section);
        pe->pcrel = 0;
        next(s1);
        break;
    default:
        if (s1->tok >= TOK_IDENT) {
	    ElfSym *esym;
            /* label case : if the label was not found, add one */
	    sym = get_asm_sym(s1, s1->tok, NULL);
	    esym = elfsym(s1, sym);
            if (esym && esym->st_shndx == SHN_ABS) {
                /* if absolute symbol, no need to put a symbol value */
                pe->v = esym->st_value;
                pe->sym = NULL;
		pe->pcrel = 0;
            } else {
                pe->v = 0;
                pe->sym = sym;
		pe->pcrel = 0;
            }
            next(s1);
        } else {
            tcc_error(s1, "bad expression syntax [%s]", get_tok_str(s1, s1->tok, &s1->tokc));
        }
        break;
    }
}

static void asm_expr_prod(TCCState *s1, ExprValue *pe)
{
    int op;
    ExprValue e2;

    asm_expr_unary(s1, pe);
    for(;;) {
        op = s1->tok;
        if (op != '*' && op != '/' && op != '%' &&
            op != TOK_SHL && op != TOK_SAR)
            break;
        next(s1);
        asm_expr_unary(s1, &e2);
        if (pe->sym || e2.sym)
            tcc_error(s1, "invalid operation with label");
        switch(op) {
        case '*':
            pe->v *= e2.v;
            break;
        case '/':
            if (e2.v == 0) {
            div_error:
                tcc_error(s1, "division by zero");
            }
            pe->v /= e2.v;
            break;
        case '%':
            if (e2.v == 0)
                goto div_error;
            pe->v %= e2.v;
            break;
        case TOK_SHL:
            pe->v <<= e2.v;
            break;
        default:
        case TOK_SAR:
            pe->v >>= e2.v;
            break;
        }
    }
}

static void asm_expr_logic(TCCState *s1, ExprValue *pe)
{
    int op;
    ExprValue e2;

    asm_expr_prod(s1, pe);
    for(;;) {
        op = s1->tok;
        if (op != '&' && op != '|' && op != '^')
            break;
        next(s1);
        asm_expr_prod(s1, &e2);
        if (pe->sym || e2.sym)
            tcc_error(s1, "invalid operation with label");
        switch(op) {
        case '&':
            pe->v &= e2.v;
            break;
        case '|':
            pe->v |= e2.v;
            break;
        default:
        case '^':
            pe->v ^= e2.v;
            break;
        }
    }
}

static inline void asm_expr_sum(TCCState *s1, ExprValue *pe)
{
    int op;
    ExprValue e2;

    asm_expr_logic(s1, pe);
    for(;;) {
        op = s1->tok;
        if (op != '+' && op != '-')
            break;
        next(s1);
        asm_expr_logic(s1, &e2);
        if (op == '+') {
            if (pe->sym != NULL && e2.sym != NULL)
                goto cannot_relocate;
            pe->v += e2.v;
            if (pe->sym == NULL && e2.sym != NULL)
                pe->sym = e2.sym;
        } else {
            pe->v -= e2.v;
            /* NOTE: we are less powerful than gas in that case
               because we store only one symbol in the expression */
	    if (!e2.sym) {
		/* OK */
	    } else if (pe->sym == e2.sym) {
		/* OK */
		pe->sym = NULL; /* same symbols can be subtracted to NULL */
	    } else {
		ElfSym *esym1, *esym2;
		esym1 = elfsym(s1, pe->sym);
		esym2 = elfsym(s1, e2.sym);
		if (esym1 && esym1->st_shndx == esym2->st_shndx
		    && esym1->st_shndx != SHN_UNDEF) {
		    /* we also accept defined symbols in the same section */
		    pe->v += esym1->st_value - esym2->st_value;
		    pe->sym = NULL;
		} else if (esym2->st_shndx == s1->cur_text_section->sh_num) {
		    /* When subtracting a defined symbol in current section
		       this actually makes the value PC-relative.  */
		    pe->v -= esym2->st_value - s1->ind - 4;
		    pe->pcrel = 1;
		    e2.sym = NULL;
		} else {
cannot_relocate:
		    tcc_error(s1, "invalid operation with label");
		}
	    }
        }
    }
}

static inline void asm_expr_cmp(TCCState *s1, ExprValue *pe)
{
    int op;
    ExprValue e2;

    asm_expr_sum(s1, pe);
    for(;;) {
        op = s1->tok;
	if (op != TOK_EQ && op != TOK_NE
	    && (op > TOK_GT || op < TOK_ULE))
            break;
        next(s1);
        asm_expr_sum(s1, &e2);
        if (pe->sym || e2.sym)
            tcc_error(s1, "invalid operation with label");
        switch(op) {
	case TOK_EQ:
	    pe->v = pe->v == e2.v;
	    break;
	case TOK_NE:
	    pe->v = pe->v != e2.v;
	    break;
	case TOK_LT:
	    pe->v = (int64_t)pe->v < (int64_t)e2.v;
	    break;
	case TOK_GE:
	    pe->v = (int64_t)pe->v >= (int64_t)e2.v;
	    break;
	case TOK_LE:
	    pe->v = (int64_t)pe->v <= (int64_t)e2.v;
	    break;
	case TOK_GT:
	    pe->v = (int64_t)pe->v > (int64_t)e2.v;
	    break;
        default:
            break;
        }
	/* GAS compare results are -1/0 not 1/0.  */
	pe->v = -(int64_t)pe->v;
    }
}

ST_FUNC void asm_expr(TCCState *s1, ExprValue *pe)
{
    asm_expr_cmp(s1, pe);
}

ST_FUNC int asm_int_expr(TCCState *s1)
{
    ExprValue e;
    asm_expr(s1, &e);
    if (e.sym)
        expect(s1, "constant");
    return e.v;
}

static Sym* asm_new_label1(TCCState *s1, int label, int is_local,
                           int sh_num, int value)
{
    Sym *sym;
    ElfSym *esym;

    sym = asm_label_find(s1, label);
    if (sym) {
	esym = elfsym(s1, sym);
	/* A VT_EXTERN symbol, even if it has a section is considered
	   overridable.  This is how we "define" .set targets.  Real
	   definitions won't have VT_EXTERN set.  */
        if (esym && esym->st_shndx != SHN_UNDEF) {
            /* the label is already defined */
            if (IS_ASM_SYM(sym)
                && (is_local == 1 || (sym->type.t & VT_EXTERN)))
                goto new_label;
            if (!(sym->type.t & VT_EXTERN))
                tcc_error(s1, "assembler label '%s' already defined",
                          get_tok_str(s1, label, NULL));
        }
    } else {
    new_label:
        sym = asm_label_push(s1, label);
    }
    if (!sym->c)
      put_extern_sym2(s1, sym, SHN_UNDEF, 0, 0, 0);
    esym = elfsym(s1, sym);
    esym->st_shndx = sh_num;
    esym->st_value = value;
    if (is_local != 2)
        sym->type.t &= ~VT_EXTERN;
    return sym;
}

static Sym* asm_new_label(TCCState *s1, int label, int is_local)
{
    return asm_new_label1(s1, label, is_local, s1->cur_text_section->sh_num, s1->ind);
}

/* Set the value of LABEL to that of some expression (possibly
   involving other symbols).  LABEL can be overwritten later still.  */
static Sym* set_symbol(TCCState *s1, int label)
{
    long n;
    ExprValue e;
    Sym *sym;
    ElfSym *esym;
    next(s1);
    asm_expr(s1, &e);
    n = e.v;
    esym = elfsym(s1, e.sym);
    if (esym)
	n += esym->st_value;
    sym = asm_new_label1(s1, label, 2, esym ? esym->st_shndx : SHN_ABS, n);
    elfsym(s1, sym)->st_other |= ST_ASM_SET;
    return sym;
}

static void use_section1(TCCState *s1, Section *sec)
{
    s1->cur_text_section->data_offset = s1->ind;
    s1->cur_text_section = sec;
    s1->ind = s1->cur_text_section->data_offset;
}

static void use_section(TCCState *s1, const char *name)
{
    Section *sec;
    sec = find_section(s1, name);
    use_section1(s1, sec);
}

static void push_section(TCCState *s1, const char *name)
{
    Section *sec = find_section(s1, name);
    sec->prev = s1->cur_text_section;
    use_section1(s1, sec);
}

static void pop_section(TCCState *s1)
{
    Section *prev = s1->cur_text_section->prev;
    if (!prev)
        tcc_error(s1, ".popsection without .pushsection");
    s1->cur_text_section->prev = NULL;
    use_section1(s1, prev);
}

static void asm_parse_directive(TCCState *s1, int global)
{
    int n, offset, v, size, tok1;
    Section *sec;
    uint8_t *ptr;

    /* assembler directive */
    sec = s1->cur_text_section;
    switch(s1->tok) {
    case TOK_ASMDIR_align:
    case TOK_ASMDIR_balign:
    case TOK_ASMDIR_p2align:
    case TOK_ASMDIR_skip:
    case TOK_ASMDIR_space:
        tok1 = s1->tok;
        next(s1);
        n = asm_int_expr(s1);
        if (tok1 == TOK_ASMDIR_p2align)
        {
            if (n < 0 || n > 30)
                tcc_error(s1, "invalid p2align, must be between 0 and 30");
            n = 1 << n;
            tok1 = TOK_ASMDIR_align;
        }
        if (tok1 == TOK_ASMDIR_align || tok1 == TOK_ASMDIR_balign) {
            if (n < 0 || (n & (n-1)) != 0)
                tcc_error(s1, "alignment must be a positive power of two");
            offset = (s1->ind + n - 1) & -n;
            size = offset - s1->ind;
            /* the section must have a compatible alignment */
            if (sec->sh_addralign < n)
                sec->sh_addralign = n;
        } else {
	    if (n < 0)
	        n = 0;
            size = n;
        }
        v = 0;
        if (s1->tok == ',') {
            next(s1);
            v = asm_int_expr(s1);
        }
    zero_pad:
        if (sec->sh_type != SHT_NOBITS) {
            sec->data_offset = s1->ind;
            ptr = section_ptr_add(s1, sec, size);
            memset(ptr, v, size);
        }
        s1->ind += size;
        break;
    case TOK_ASMDIR_quad:
#ifdef TCC_TARGET_X86_64
	size = 8;
	goto asm_data;
#else
        next(s1);
        for(;;) {
            uint64_t vl;
            const char *p;

            p = s1->tokc.str.data;
            if (s1->tok != TOK_PPNUM) {
            error_constant:
                tcc_error(s1, "64 bit constant");
            }
            vl = strtoll(p, (char **)&p, 0);
            if (*p != '\0')
                goto error_constant;
            next(s1);
            if (sec->sh_type != SHT_NOBITS) {
                /* XXX: endianness */
                gen_le32(vl);
                gen_le32(vl >> 32);
            } else {
                ind += 8;
            }
            if (s1->tok != ',')
                break;
            next(s1);
        }
        break;
#endif
    case TOK_ASMDIR_byte:
        size = 1;
        goto asm_data;
    case TOK_ASMDIR_word:
    case TOK_ASMDIR_short:
        size = 2;
        goto asm_data;
    case TOK_ASMDIR_long:
    case TOK_ASMDIR_int:
        size = 4;
    asm_data:
        next(s1);
        for(;;) {
            ExprValue e;
            asm_expr(s1, &e);
            if (sec->sh_type != SHT_NOBITS) {
                if (size == 4) {
                    gen_expr32(s1, &e);
#ifdef TCC_TARGET_X86_64
		} else if (size == 8) {
		    gen_expr64(s1, &e);
#endif
                } else {
                    if (e.sym)
                        expect(s1, "constant");
                    if (size == 1)
                        g(s1, e.v);
                    else
                        gen_le16(s1, e.v);
                }
            } else {
                s1->ind += size;
            }
            if (s1->tok != ',')
                break;
            next(s1);
        }
        break;
    case TOK_ASMDIR_fill:
        {
            int repeat, size, val, i, j;
            uint8_t repeat_buf[8];
            next(s1);
            repeat = asm_int_expr(s1);
            if (repeat < 0) {
                tcc_error(s1, "repeat < 0; .fill ignored");
                break;
            }
            size = 1;
            val = 0;
            if (s1->tok == ',') {
                next(s1);
                size = asm_int_expr(s1);
                if (size < 0) {
                    tcc_error(s1, "size < 0; .fill ignored");
                    break;
                }
                if (size > 8)
                    size = 8;
                if (s1->tok == ',') {
                    next(s1);
                    val = asm_int_expr(s1);
                }
            }
            /* XXX: endianness */
            repeat_buf[0] = val;
            repeat_buf[1] = val >> 8;
            repeat_buf[2] = val >> 16;
            repeat_buf[3] = val >> 24;
            repeat_buf[4] = 0;
            repeat_buf[5] = 0;
            repeat_buf[6] = 0;
            repeat_buf[7] = 0;
            for(i = 0; i < repeat; i++) {
                for(j = 0; j < size; j++) {
                    g(s1, repeat_buf[j]);
                }
            }
        }
        break;
    case TOK_ASMDIR_rept:
        {
            int repeat;
            TokenString *init_str;
            next(s1);
            repeat = asm_int_expr(s1);
            init_str = tok_str_alloc(s1);
            while (next(s1), s1->tok != TOK_ASMDIR_endr) {
                if (s1->tok == CH_EOF)
                    tcc_error(s1, "we at end of file, .endr not found");
                tok_str_add_tok(s1, init_str);
            }
            tok_str_add(s1, init_str, -1);
            tok_str_add(s1, init_str, 0);
            begin_macro(s1, init_str, 1);
            while (repeat-- > 0) {
                tcc_assemble_internal(s1, (s1->parse_flags & PARSE_FLAG_PREPROCESS),
				      global);
                s1->macro_ptr = init_str->str;
            }
            end_macro(s1);
            next(s1);
            break;
        }
    case TOK_ASMDIR_org:
        {
            unsigned long n;
	    ExprValue e;
	    ElfSym *esym;
            next(s1);
	    asm_expr(s1, &e);
	    n = e.v;
	    esym = elfsym(s1, e.sym);
	    if (esym) {
		if (esym->st_shndx != s1->cur_text_section->sh_num)
		  expect(s1, "constant or same-section symbol");
		n += esym->st_value;
	    }
            if (n < s1->ind)
                tcc_error(s1, "attempt to .org backwards");
            v = 0;
            size = n - s1->ind;
            goto zero_pad;
        }
        break;
    case TOK_ASMDIR_set:
	next(s1);
	tok1 = s1->tok;
	next(s1);
	/* Also accept '.set stuff', but don't do anything with this.
	   It's used in GAS to set various features like '.set mips16'.  */
	if (s1->tok == ',')
	    set_symbol(s1, tok1);
	break;
    case TOK_ASMDIR_globl:
    case TOK_ASMDIR_global:
    case TOK_ASMDIR_weak:
    case TOK_ASMDIR_hidden:
	tok1 = s1->tok;
	do {
            Sym *sym;
            next(s1);
            sym = get_asm_sym(s1, s1->tok, NULL);
	    if (tok1 != TOK_ASMDIR_hidden)
                sym->type.t &= ~VT_STATIC;
            if (tok1 == TOK_ASMDIR_weak)
                sym->a.weak = 1;
	    else if (tok1 == TOK_ASMDIR_hidden)
	        sym->a.visibility = STV_HIDDEN;
            update_storage(s1, sym);
            next(s1);
	} while (s1->tok == ',');
	break;
    case TOK_ASMDIR_string:
    case TOK_ASMDIR_ascii:
    case TOK_ASMDIR_asciz:
        {
            const uint8_t *p;
            int i, size, t;

            t = s1->tok;
            next(s1);
            for(;;) {
                if (s1->tok != TOK_STR)
                    expect(s1, "string constant");
                p = s1->tokc.str.data;
                size = s1->tokc.str.size;
                if (t == TOK_ASMDIR_ascii && size > 0)
                    size--;
                for(i = 0; i < size; i++)
                    g(s1, p[i]);
                next(s1);
                if (s1->tok == ',') {
                    next(s1);
                } else if (s1->tok != TOK_STR) {
                    break;
                }
            }
	}
	break;
    case TOK_ASMDIR_text:
    case TOK_ASMDIR_data:
    case TOK_ASMDIR_bss:
	{
            char sname[64];
            tok1 = s1->tok;
            n = 0;
            next(s1);
            if (s1->tok != ';' && s1->tok != TOK_LINEFEED) {
		n = asm_int_expr(s1);
		next(s1);
            }
            if (n)
                sprintf(sname, "%s%d", get_tok_str(s1, tok1, NULL), n);
            else
                sprintf(sname, "%s", get_tok_str(s1, tok1, NULL));
            use_section(s1, sname);
	}
	break;
    case TOK_ASMDIR_file:
        {
            char filename[512];

            filename[0] = '\0';
            next(s1);

            if (s1->tok == TOK_STR)
                pstrcat(filename, sizeof(filename), s1->tokc.str.data);
            else
                pstrcat(filename, sizeof(filename), get_tok_str(s1, s1->tok, NULL));

            if (s1->warn_unsupported)
                tcc_warning(s1, "ignoring .file %s", filename);

            next(s1);
        }
        break;
    case TOK_ASMDIR_ident:
        {
            char ident[256];

            ident[0] = '\0';
            next(s1);

            if (s1->tok == TOK_STR)
                pstrcat(ident, sizeof(ident), s1->tokc.str.data);
            else
                pstrcat(ident, sizeof(ident), get_tok_str(s1, s1->tok, NULL));

            if (s1->warn_unsupported)
                tcc_warning(s1, "ignoring .ident %s", ident);

            next(s1);
        }
        break;
    case TOK_ASMDIR_size:
        {
            Sym *sym;

            next(s1);
            sym = asm_label_find(s1, s1->tok);
            if (!sym) {
                tcc_error(s1, "label not found: %s", get_tok_str(s1, s1->tok, NULL));
            }

            /* XXX .size name,label2-label1 */
            if (s1->warn_unsupported)
                tcc_warning(s1, "ignoring .size %s,*", get_tok_str(s1, s1->tok, NULL));

            next(s1);
            skip(s1, ',');
            while (s1->tok != TOK_LINEFEED && s1->tok != ';' && s1->tok != CH_EOF) {
                next(s1);
            }
        }
        break;
    case TOK_ASMDIR_type:
        {
            Sym *sym;
            const char *newtype;

            next(s1);
            sym = get_asm_sym(s1, s1->tok, NULL);
            next(s1);
            skip(s1, ',');
            if (s1->tok == TOK_STR) {
                newtype = s1->tokc.str.data;
            } else {
                if (s1->tok == '@' || s1->tok == '%')
                    next(s1);
                newtype = get_tok_str(s1, s1->tok, NULL);
            }

            if (!strcmp(newtype, "function") || !strcmp(newtype, "STT_FUNC")) {
                sym->type.t = (sym->type.t & ~VT_BTYPE) | VT_FUNC;
            }
            else if (s1->warn_unsupported)
                tcc_warning(s1, "change type of '%s' from 0x%x to '%s' ignored",
                    get_tok_str(s1, sym->v, NULL), sym->type.t, newtype);

            next(s1);
        }
        break;
    case TOK_ASMDIR_pushsection:
    case TOK_ASMDIR_section:
        {
            char sname[256];
	    int old_nb_section = s1->nb_sections;

	    tok1 = s1->tok;
            /* XXX: support more options */
            next(s1);
            sname[0] = '\0';
            while (s1->tok != ';' && s1->tok != TOK_LINEFEED && s1->tok != ',') {
                if (s1->tok == TOK_STR)
                    pstrcat(sname, sizeof(sname), s1->tokc.str.data);
                else
                    pstrcat(sname, sizeof(sname), get_tok_str(s1, s1->tok, NULL));
                next(s1);
            }
            if (s1->tok == ',') {
                /* skip section options */
                next(s1);
                if (s1->tok != TOK_STR)
                    expect(s1, "string constant");
                next(s1);
                if (s1->tok == ',') {
                    next(s1);
                    if (s1->tok == '@' || s1->tok == '%')
                        next(s1);
                    next(s1);
                }
            }
            s1->last_text_section = s1->cur_text_section;
	    if (tok1 == TOK_ASMDIR_section)
	        use_section(s1, sname);
	    else
	        push_section(s1, sname);
	    /* If we just allocated a new section reset its alignment to
	       1.  new_section normally acts for GCC compatibility and
	       sets alignment to PTR_SIZE.  The assembler behaves different. */
	    if (old_nb_section != s1->nb_sections)
	        s1->cur_text_section->sh_addralign = 1;
        }
        break;
    case TOK_ASMDIR_previous:
        {
            Section *sec;
            next(s1);
            if (!s1->last_text_section)
                tcc_error(s1, "no previous section referenced");
            sec = s1->cur_text_section;
            use_section1(s1, s1->last_text_section);
            s1->last_text_section = sec;
        }
        break;
    case TOK_ASMDIR_popsection:
	next(s1);
	pop_section(s1);
	break;
#ifdef TCC_TARGET_I386
    case TOK_ASMDIR_code16:
        {
            next(s1);
            s1->seg_size = 16;
        }
        break;
    case TOK_ASMDIR_code32:
        {
            next(s1);
            s1->seg_size = 32;
        }
        break;
#endif
#ifdef TCC_TARGET_X86_64
    /* added for compatibility with GAS */
    case TOK_ASMDIR_code64:
        next(s1);
        break;
#endif
    default:
        tcc_error(s1, "unknown assembler directive '.%s'", get_tok_str(s1, s1->tok, NULL));
        break;
    }
}


/* assemble a file */
static int tcc_assemble_internal(TCCState *s1, int do_preprocess, int global)
{
    int opcode;
    int saved_parse_flags = s1->parse_flags;

    s1->parse_flags = PARSE_FLAG_ASM_FILE | PARSE_FLAG_TOK_STR;
    if (do_preprocess)
        s1->parse_flags |= PARSE_FLAG_PREPROCESS;
    for(;;) {
        next(s1);
        if (s1->tok == TOK_EOF)
            break;
        /* generate line number info */
        if (global && s1->do_debug)
            tcc_debug_line(s1);
        s1->parse_flags |= PARSE_FLAG_LINEFEED; /* XXX: suppress that hack */
    redo:
        if (s1->tok == '#') {
            /* horrible gas comment */
            while (s1->tok != TOK_LINEFEED)
                next(s1);
        } else if (s1->tok >= TOK_ASMDIR_FIRST && s1->tok <= TOK_ASMDIR_LAST) {
            asm_parse_directive(s1, global);
        } else if (s1->tok == TOK_PPNUM) {
            const char *p;
            int n;
            p = s1->tokc.str.data;
            n = strtoul(p, (char **)&p, 10);
            if (*p != '\0')
                expect(s1, "':'");
            /* new local label */
            asm_new_label(s1, asm_get_local_label_name(s1, n), 1);
            next(s1);
            skip(s1, ':');
            goto redo;
        } else if (s1->tok >= TOK_IDENT) {
            /* instruction or label */
            opcode = s1->tok;
            next(s1);
            if (s1->tok == ':') {
                /* new label */
                asm_new_label(s1, opcode, 0);
                next(s1);
                goto redo;
            } else if (s1->tok == '=') {
		set_symbol(s1, opcode);
                goto redo;
            } else {
                asm_opcode(s1, opcode);
            }
        }
        /* end of line */
        if (s1->tok != ';' && s1->tok != TOK_LINEFEED)
            expect(s1, "end of line");
        s1->parse_flags &= ~PARSE_FLAG_LINEFEED; /* XXX: suppress that hack */
    }

    s1->parse_flags = saved_parse_flags;
    return 0;
}

/* Assemble the current file */
ST_FUNC int tcc_assemble(TCCState *s1, int do_preprocess)
{
    int ret;
    tcc_debug_start(s1);
    /* default section is text */
    s1->cur_text_section = s1->text_section;
    s1->ind = s1->cur_text_section->data_offset;
    s1->nocode_wanted = 0;
    ret = tcc_assemble_internal(s1, do_preprocess, 1);
    s1->cur_text_section->data_offset = s1->ind;
    tcc_debug_end(s1);
    return ret;
}

/********************************************************************/
/* GCC inline asm support */

/* assemble the string 'str' in the current C compilation unit without
   C preprocessing. NOTE: str is modified by modifying the '\0' at the
   end */
static void tcc_assemble_inline(TCCState *s1, char *str, int len, int global)
{
    const int *saved_macro_ptr = s1->macro_ptr;
    int dotid = set_idnum(s1, '.', IS_ID);

    tcc_open_bf(s1, ":asm:", len);
    memcpy(s1->file->buffer, str, len);
    s1->macro_ptr = NULL;
    tcc_assemble_internal(s1, 0, global);
    tcc_close(s1);

    set_idnum(s1, '.', dotid);
    s1->macro_ptr = saved_macro_ptr;
}

/* find a constraint by its number or id (gcc 3 extended
   syntax). return -1 if not found. Return in *pp in char after the
   constraint */
ST_FUNC int find_constraint(TCCState *s1, ASMOperand *operands, int nb_operands,
                           const char *name, const char **pp)
{
    int index;
    TokenSym *ts;
    const char *p;

    if (isnum(*name)) {
        index = 0;
        while (isnum(*name)) {
            index = (index * 10) + (*name) - '0';
            name++;
        }
        if ((unsigned)index >= nb_operands)
            index = -1;
    } else if (*name == '[') {
        name++;
        p = strchr(name, ']');
        if (p) {
            ts = tok_alloc(s1, name, p - name);
            for(index = 0; index < nb_operands; index++) {
                if (operands[index].id == ts->tok)
                    goto found;
            }
            index = -1;
        found:
            name = p + 1;
        } else {
            index = -1;
        }
    } else {
        index = -1;
    }
    if (pp)
        *pp = name;
    return index;
}

static void subst_asm_operands(TCCState *s1, ASMOperand *operands, int nb_operands,
                               CString *out_str, CString *in_str)
{
    int c, index, modifier;
    const char *str;
    ASMOperand *op;
    SValue sv;

    cstr_new(s1, out_str);
    str = in_str->data;
    for(;;) {
        c = *str++;
        if (c == '%') {
            if (*str == '%') {
                str++;
                goto add_char;
            }
            modifier = 0;
            if (*str == 'c' || *str == 'n' ||
                *str == 'b' || *str == 'w' || *str == 'h' || *str == 'k' ||
		*str == 'q' ||
		/* P in GCC would add "@PLT" to symbol refs in PIC mode,
		   and make literal operands not be decorated with '$'.  */
		*str == 'P')
                modifier = *str++;
            index = find_constraint(s1, operands, nb_operands, str, &str);
            if (index < 0)
                tcc_error(s1, "invalid operand reference after %%");
            op = &operands[index];
            sv = *op->vt;
            if (op->reg >= 0) {
                sv.r = op->reg;
                if ((op->vt->r & VT_VALMASK) == VT_LLOCAL && op->is_memory)
                    sv.r |= VT_LVAL;
            }
            subst_asm_operand(s1, out_str, &sv, modifier);
        } else {
        add_char:
            cstr_ccat(s1, out_str, c);
            if (c == '\0')
                break;
        }
    }
}


static void parse_asm_operands(TCCState *s1, ASMOperand *operands, int *nb_operands_ptr,
                               int is_output)
{
    ASMOperand *op;
    int nb_operands;

    if (s1->tok != ':') {
        nb_operands = *nb_operands_ptr;
        for(;;) {
	    CString astr;
            if (nb_operands >= MAX_ASM_OPERANDS)
                tcc_error(s1, "too many asm operands");
            op = &operands[nb_operands++];
            op->id = 0;
            if (s1->tok == '[') {
                next(s1);
                if (s1->tok < TOK_IDENT)
                    expect(s1, "identifier");
                op->id = s1->tok;
                next(s1);
                skip(s1, ']');
            }
	    parse_mult_str(s1, &astr, "string constant");
            op->constraint = tcc_malloc(s1, astr.size);
            strcpy(op->constraint, astr.data);
	    cstr_free(s1, &astr);
            skip(s1, '(');
            gexpr(s1);
            if (is_output) {
                if (!(s1->vtop->type.t & VT_ARRAY))
                    test_lvalue(s1);
            } else {
                /* we want to avoid LLOCAL case, except when the 'm'
                   constraint is used. Note that it may come from
                   register storage, so we need to convert (reg)
                   case */
                if ((s1->vtop->r & VT_LVAL) &&
                    ((s1->vtop->r & VT_VALMASK) == VT_LLOCAL ||
                     (s1->vtop->r & VT_VALMASK) < VT_CONST) &&
                    !strchr(op->constraint, 'm')) {
                    gv(s1, RC_INT);
                }
            }
            op->vt = s1->vtop;
            skip(s1, ')');
            if (s1->tok == ',') {
                next(s1);
            } else {
                break;
            }
        }
        *nb_operands_ptr = nb_operands;
    }
}

/* parse the GCC asm() instruction */
ST_FUNC void asm_instr(TCCState *s1)
{
    CString astr, astr1;
    ASMOperand operands[MAX_ASM_OPERANDS];
    int nb_outputs, nb_operands, i, must_subst, out_reg;
    uint8_t clobber_regs[NB_ASM_REGS];

    next(s1);
    /* since we always generate the asm() instruction, we can ignore
       volatile */
    if (s1->tok == TOK_VOLATILE1 || s1->tok == TOK_VOLATILE2 || s1->tok == TOK_VOLATILE3) {
        next(s1);
    }
    parse_asm_str(s1, &astr);
    nb_operands = 0;
    nb_outputs = 0;
    must_subst = 0;
    memset(clobber_regs, 0, sizeof(clobber_regs));
    if (s1->tok == ':') {
        next(s1);
        must_subst = 1;
        /* output args */
        parse_asm_operands(s1, operands, &nb_operands, 1);
        nb_outputs = nb_operands;
        if (s1->tok == ':') {
            next(s1);
            if (s1->tok != ')') {
                /* input args */
                parse_asm_operands(s1, operands, &nb_operands, 0);
                if (s1->tok == ':') {
                    /* clobber list */
                    /* XXX: handle registers */
                    next(s1);
                    for(;;) {
                        if (s1->tok != TOK_STR)
                            expect(s1, "string constant");
                        asm_clobber(s1, clobber_regs, s1->tokc.str.data);
                        next(s1);
                        if (s1->tok == ',') {
                            next(s1);
                        } else {
                            break;
                        }
                    }
                }
            }
        }
    }
    skip(s1, ')');
    /* NOTE: we do not eat the ';' so that we can restore the current
       token after the assembler parsing */
    if (s1->tok != ';')
        expect(s1, "';'");

    /* save all values in the memory */
    save_regs(s1, 0);

    /* compute constraints */
    asm_compute_constraints(s1, operands, nb_operands, nb_outputs,
                            clobber_regs, &out_reg);

    /* substitute the operands in the asm string. No substitution is
       done if no operands (GCC behaviour) */
#ifdef ASM_DEBUG
    printf("asm: \"%s\"\n", (char *)astr.data);
#endif
    if (must_subst) {
        subst_asm_operands(s1, operands, nb_operands, &astr1, &astr);
        cstr_free(s1, &astr);
    } else {
        astr1 = astr;
    }
#ifdef ASM_DEBUG
    printf("subst_asm: \"%s\"\n", (char *)astr1.data);
#endif

    /* generate loads */
    asm_gen_code(s1, operands, nb_operands, nb_outputs, 0,
                 clobber_regs, out_reg);

    /* assemble the string with tcc internal assembler */
    tcc_assemble_inline(s1, astr1.data, astr1.size - 1, 0);

    /* restore the current C token */
    next(s1);

    /* store the output values if needed */
    asm_gen_code(s1, operands, nb_operands, nb_outputs, 1,
                 clobber_regs, out_reg);

    /* free everything */
    for(i=0;i<nb_operands;i++) {
        ASMOperand *op;
        op = &operands[i];
        tcc_free(s1, op->constraint);
        vpop(s1);
    }
    cstr_free(s1, &astr1);
}

ST_FUNC void asm_global_instr(TCCState *s1)
{
    CString astr;
    int saved_nocode_wanted = s1->nocode_wanted;

    /* Global asm blocks are always emitted.  */
    s1->nocode_wanted = 0;
    next(s1);
    parse_asm_str(s1, &astr);
    skip(s1, ')');
    /* NOTE: we do not eat the ';' so that we can restore the current
       token after the assembler parsing */
    if (s1->tok != ';')
        expect(s1, "';'");

#ifdef ASM_DEBUG
    printf("asm_global: \"%s\"\n", (char *)astr.data);
#endif
    s1->cur_text_section = s1->text_section;
    s1->ind = s1->cur_text_section->data_offset;

    /* assemble the string with tcc internal assembler */
    tcc_assemble_inline(s1, astr.data, astr.size - 1, 1);

    s1->cur_text_section->data_offset = s1->ind;

    /* restore the current C token */
    next(s1);

    cstr_free(s1, &astr);
    s1->nocode_wanted = saved_nocode_wanted;
}
#endif /* CONFIG_TCC_ASM */
