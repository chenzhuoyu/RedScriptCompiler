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

#ifndef _TCC_H
#define _TCC_H

#define _GNU_SOURCE

#ifndef CONFIG_TCCDIR
# define CONFIG_TCCDIR "/usr/local/lib/tcc"
#endif
#define TCC_VERSION "0.9.27"

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <fcntl.h>
#include <setjmp.h>
#include <time.h>

#ifndef _WIN32
# include <unistd.h>
# include <sys/time.h>
# ifndef CONFIG_TCC_STATIC
#  include <dlfcn.h>
# endif
/* XXX: need to define this to use them in non ISOC99 context */
extern float strtof (const char *__nptr, char **__endptr);
extern long double strtold (const char *__nptr, char **__endptr);
#endif

#ifdef _WIN32
# include <windows.h>
# include <io.h> /* open, close etc. */
# include <direct.h> /* getcwd */
# ifdef __GNUC__
#  include <stdint.h>
# endif
# define inline __inline
# define snprintf _snprintf
# define vsnprintf _vsnprintf
# ifndef __GNUC__
#  define strtold (long double)strtod
#  define strtof (float)strtod
#  define strtoll _strtoi64
#  define strtoull _strtoui64
# endif
# ifdef LIBTCC_AS_DLL
#  define LIBTCCAPI __declspec(dllexport)
#  define PUB_FUNC LIBTCCAPI
# endif
# define inp next_inp /* inp is an intrinsic on msvc/mingw */
# ifdef _MSC_VER
#  pragma warning (disable : 4244)  // conversion from 'uint64_t' to 'int', possible loss of data
#  pragma warning (disable : 4267)  // conversion from 'size_t' to 'int', possible loss of data
#  pragma warning (disable : 4996)  // The POSIX name for this item is deprecated. Instead, use the ISO C and C++ conformant name
#  pragma warning (disable : 4018)  // signed/unsigned mismatch
#  pragma warning (disable : 4146)  // unary minus operator applied to unsigned type, result still unsigned
#  define ssize_t intptr_t
# endif
# undef CONFIG_TCC_STATIC
#endif

#ifndef O_BINARY
# define O_BINARY 0
#endif

#ifndef offsetof
#define offsetof(type, field) ((size_t) &((type *)0)->field)
#endif

#ifndef countof
#define countof(tab) (sizeof(tab) / sizeof((tab)[0]))
#endif

#ifdef _MSC_VER
# define NORETURN __declspec(noreturn)
# define ALIGNED(x) __declspec(align(x))
#else
# define NORETURN __attribute__((noreturn))
# define ALIGNED(x) __attribute__((aligned(x)))
#endif

#ifdef _WIN32
# define IS_DIRSEP(c) (c == '/' || c == '\\')
# define IS_ABSPATH(p) (IS_DIRSEP(p[0]) || (p[0] && p[1] == ':' && IS_DIRSEP(p[2])))
# define PATHCMP stricmp
# define PATHSEP ";"
#else
# define IS_DIRSEP(c) (c == '/')
# define IS_ABSPATH(p) IS_DIRSEP(p[0])
# define PATHCMP strcmp
# define PATHSEP ":"
#endif

/* -------------------------------------------- */

/* parser debug */
/* #define PARSE_DEBUG */
/* preprocessor debug */
/* #define PP_DEBUG */
/* include file debug */
/* #define INC_DEBUG */
/* memory leak debug */
/* #define MEM_DEBUG */
/* assembler debug */
/* #define ASM_DEBUG */

/* target selection */
/* #define TCC_TARGET_I386   *//* i386 code generator */
/* #define TCC_TARGET_X86_64 *//* x86-64 code generator */
/* #define TCC_TARGET_ARM    *//* ARMv4 code generator */
/* #define TCC_TARGET_ARM64  *//* ARMv8 code generator */
/* #define TCC_TARGET_C67    *//* TMS320C67xx code generator */

/* default target is I386 */
#if !defined(TCC_TARGET_I386) && !defined(TCC_TARGET_ARM) && \
    !defined(TCC_TARGET_ARM64) && !defined(TCC_TARGET_C67) && \
    !defined(TCC_TARGET_X86_64)
# if defined __x86_64__ || defined _AMD64_
#  define TCC_TARGET_X86_64
# elif defined __arm__
#  define TCC_TARGET_ARM
#  define TCC_ARM_EABI
#  define TCC_ARM_HARDFLOAT
# elif defined __aarch64__
#  define TCC_TARGET_ARM64
# else
#  define TCC_TARGET_I386
# endif
# ifdef _WIN32
#  define TCC_TARGET_PE 1
# endif
#endif

/* only native compiler supports -run */
#if defined _WIN32 == defined TCC_TARGET_PE
# if (defined __i386__ || defined _X86_) && defined TCC_TARGET_I386
#  define TCC_IS_NATIVE
# elif (defined __x86_64__ || defined _AMD64_) && defined TCC_TARGET_X86_64
#  define TCC_IS_NATIVE
# elif defined __arm__ && defined TCC_TARGET_ARM
#  define TCC_IS_NATIVE
# elif defined __aarch64__ && defined TCC_TARGET_ARM64
#  define TCC_IS_NATIVE
# endif
#endif

#if defined TCC_IS_NATIVE && !defined CONFIG_TCCBOOT
# define CONFIG_TCC_BACKTRACE
# if (defined TCC_TARGET_I386 || defined TCC_TARGET_X86_64) \
  && !defined TCC_UCLIBC && !defined TCC_MUSL
# define CONFIG_TCC_BCHECK /* enable bound checking code */
# endif
#endif

/* ------------ path configuration ------------ */

#ifndef CONFIG_SYSROOT
# define CONFIG_SYSROOT ""
#endif
#ifndef CONFIG_TCCDIR
# define CONFIG_TCCDIR "/usr/local/lib/tcc"
#endif
#ifndef CONFIG_LDDIR
# define CONFIG_LDDIR "lib"
#endif
#ifdef CONFIG_TRIPLET
# define USE_TRIPLET(s) s "/" CONFIG_TRIPLET
# define ALSO_TRIPLET(s) USE_TRIPLET(s) ":" s
#else
# define USE_TRIPLET(s) s
# define ALSO_TRIPLET(s) s
#endif

/* path to find crt1.o, crti.o and crtn.o */
#ifndef CONFIG_TCC_CRTPREFIX
# define CONFIG_TCC_CRTPREFIX USE_TRIPLET(CONFIG_SYSROOT "/usr/" CONFIG_LDDIR)
#endif

/* Below: {B} is substituted by CONFIG_TCCDIR (rsp. -B option) */

/* system include paths */
#ifndef CONFIG_TCC_SYSINCLUDEPATHS
# ifdef TCC_TARGET_PE
#  define CONFIG_TCC_SYSINCLUDEPATHS "{B}/include"PATHSEP"{B}/include/winapi"
# else
#  define CONFIG_TCC_SYSINCLUDEPATHS \
        "{B}/include" \
    ":" ALSO_TRIPLET(CONFIG_SYSROOT "/usr/local/include") \
    ":" ALSO_TRIPLET(CONFIG_SYSROOT "/usr/include")
# endif
#endif

/* library search paths */
#ifndef CONFIG_TCC_LIBPATHS
# ifdef TCC_TARGET_PE
#  define CONFIG_TCC_LIBPATHS "{B}/lib"
# else
#  define CONFIG_TCC_LIBPATHS \
        ALSO_TRIPLET(CONFIG_SYSROOT "/usr/" CONFIG_LDDIR) \
    ":" ALSO_TRIPLET(CONFIG_SYSROOT "/" CONFIG_LDDIR) \
    ":" ALSO_TRIPLET(CONFIG_SYSROOT "/usr/local/" CONFIG_LDDIR)
# endif
#endif

/* name of ELF interpreter */
#ifndef CONFIG_TCC_ELFINTERP
# if defined __FreeBSD__
#  define CONFIG_TCC_ELFINTERP "/libexec/ld-elf.so.1"
# elif defined __FreeBSD_kernel__
#  if defined(TCC_TARGET_X86_64)
#   define CONFIG_TCC_ELFINTERP "/lib/ld-kfreebsd-x86-64.so.1"
#  else
#   define CONFIG_TCC_ELFINTERP "/lib/ld.so.1"
#  endif
# elif defined __DragonFly__
#  define CONFIG_TCC_ELFINTERP "/usr/libexec/ld-elf.so.2"
# elif defined __NetBSD__
#  define CONFIG_TCC_ELFINTERP "/usr/libexec/ld.elf_so"
# elif defined __GNU__
#  define CONFIG_TCC_ELFINTERP "/lib/ld.so"
# elif defined(TCC_TARGET_PE)
#  define CONFIG_TCC_ELFINTERP "-"
# elif defined(TCC_UCLIBC)
#  define CONFIG_TCC_ELFINTERP "/lib/ld-uClibc.so.0" /* is there a uClibc for x86_64 ? */
# elif defined TCC_TARGET_ARM64
#  if defined(TCC_MUSL)
#   define CONFIG_TCC_ELFINTERP "/lib/ld-musl-aarch64.so.1"
#  else
#   define CONFIG_TCC_ELFINTERP "/lib/ld-linux-aarch64.so.1"
#  endif
# elif defined(TCC_TARGET_X86_64)
#  if defined(TCC_MUSL)
#   define CONFIG_TCC_ELFINTERP "/lib/ld-musl-x86_64.so.1"
#  else
#   define CONFIG_TCC_ELFINTERP "/lib64/ld-linux-x86-64.so.2"
#  endif
# elif !defined(TCC_ARM_EABI)
#  if defined(TCC_MUSL)
#   define CONFIG_TCC_ELFINTERP "/lib/ld-musl-arm.so.1"
#  else
#   define CONFIG_TCC_ELFINTERP "/lib/ld-linux.so.2"
#  endif
# endif
#endif

/* var elf_interp dans *-gen.c */
#ifdef CONFIG_TCC_ELFINTERP
# define DEFAULT_ELFINTERP(s) CONFIG_TCC_ELFINTERP
#else
# define DEFAULT_ELFINTERP(s) default_elfinterp(s)
#endif

/* (target specific) libtcc1.a */
#ifndef TCC_LIBTCC1
# define TCC_LIBTCC1 "libtcc1.a"
#endif

/* library to use with CONFIG_USE_LIBGCC instead of libtcc1.a */
#if defined CONFIG_USE_LIBGCC && !defined TCC_LIBGCC
#define TCC_LIBGCC USE_TRIPLET(CONFIG_SYSROOT "/" CONFIG_LDDIR) "/libgcc_s.so.1"
#endif

/* -------------------------------------------- */

#include "libtcc.h"
#include "tccelf.h"
#include "stab.h"

/* -------------------------------------------- */

#ifndef PUB_FUNC /* functions used by tcc.c but not in libtcc.h */
# define PUB_FUNC
#endif

#ifndef ONE_SOURCE
# define ONE_SOURCE 1
#endif

#if ONE_SOURCE
#define ST_INLN static inline
#define ST_FUNC static
#define ST_DATA static
#else
#define ST_INLN
#define ST_FUNC
#define ST_DATA extern
#endif

#ifdef TCC_PROFILE /* profile all functions */
# define static
#endif

/* -------------------------------------------- */
/* include the target specific definitions */

#ifdef TCC_TARGET_I386
# include "i386-defs.h"
#endif
#ifdef TCC_TARGET_X86_64
# include "x86_64-defs.h"
#endif
#ifdef TCC_TARGET_ARM
# include "arm-defs.h"
#endif
#ifdef TCC_TARGET_ARM64
# include "arm64-defs.h"
#endif
#ifdef TCC_TARGET_C67
# define TCC_TARGET_COFF
# include "tcccoff.h"
# include "c67-defs.h"
#endif

/* -------------------------------------------- */

#if PTR_SIZE == 8
# define ELFCLASSW ELFCLASS64
# define ElfW(type) Elf##64##_##type
# define ELFW(type) ELF##64##_##type
# define ElfW_Rel ElfW(Rela)
# define SHT_RELX SHT_RELA
# define REL_SECTION_FMT ".rela%s"
#else
# define ELFCLASSW ELFCLASS32
# define ElfW(type) Elf##32##_##type
# define ELFW(type) ELF##32##_##type
# define ElfW_Rel ElfW(Rel)
# define SHT_RELX SHT_REL
# define REL_SECTION_FMT ".rel%s"
#endif
/* target address type */
#define addr_t ElfW(Addr)
#define ElfSym ElfW(Sym)

#if PTR_SIZE == 8 && !defined TCC_TARGET_PE
# define LONG_SIZE 8
#else
# define LONG_SIZE 4
#endif

/* -------------------------------------------- */

#define INCLUDE_STACK_SIZE  32
#define IFDEF_STACK_SIZE    64
#define VSTACK_SIZE         256
#define STRING_MAX_SIZE     1024
#define TOKSTR_MAX_SIZE     256
#define PACK_STACK_SIZE     8

#define TOK_HASH_SIZE       16384 /* must be a power of two */
#define TOK_ALLOC_INCR      512  /* must be a power of two */
#define TOK_MAX_SIZE        4 /* token max size in int unit when stored in string */

/* token symbol management */
typedef struct TokenSym {
    struct TokenSym *hash_next;
    struct Sym *sym_define; /* direct pointer to define */
    struct Sym *sym_label; /* direct pointer to label */
    struct Sym *sym_struct; /* direct pointer to structure */
    struct Sym *sym_identifier; /* direct pointer to identifier */
    int tok; /* token number */
    int len;
    char str[1];
} TokenSym;

#ifdef TCC_TARGET_PE
typedef unsigned short nwchar_t;
#else
typedef int nwchar_t;
#endif

typedef struct CString {
    int size; /* size in bytes */
    void *data; /* either 'char *' or 'nwchar_t *' */
    int size_allocated;
} CString;

/* type definition */
typedef struct CType {
    int t;
    struct Sym *ref;
} CType;

/* constant value */
typedef union CValue {
    long double ld;
    double d;
    float f;
    uint64_t i;
    struct {
        int size;
        const void *data;
    } str;
    int tab[LDOUBLE_SIZE/4];
} CValue;

/* value on stack */
typedef struct SValue {
    CType type;      /* type */
    unsigned short r;      /* register + flags */
    unsigned short r2;     /* second register, used for 'long long'
                              type. If not used, set to VT_CONST */
    CValue c;              /* constant, if VT_CONST */
    struct Sym *sym;       /* symbol, if (VT_SYM | VT_CONST), or if
    			      result of unary() for an identifier. */
} SValue;

/* symbol attributes */
struct SymAttr {
    unsigned short
    aligned     : 5, /* alignment as log2+1 (0 == unspecified) */
    packed      : 1,
    weak        : 1,
    visibility  : 2,
    dllexport   : 1,
    dllimport   : 1,
    unused      : 5;
};

/* function attributes or temporary attributes for parsing */
struct FuncAttr {
    unsigned
    func_call   : 3, /* calling convention (0..5), see below */
    func_type   : 2, /* FUNC_OLD/NEW/ELLIPSIS */
    func_args   : 8; /* PE __stdcall args */
};

/* GNUC attribute definition */
typedef struct AttributeDef {
    struct SymAttr a;
    struct FuncAttr f;
    struct Section *section;
    int alias_target; /* token */
    int asm_label; /* associated asm label */
    char attr_mode; /* __attribute__((__mode__(...))) */
} AttributeDef;

/* symbol management */
typedef struct Sym {
    int v; /* symbol token */
    unsigned short r; /* associated register or VT_CONST/VT_LOCAL and LVAL type */
    struct SymAttr a; /* symbol attributes */
    union {
        struct {
            int c; /* associated number or Elf symbol index */
            union {
                int sym_scope; /* scope level for locals */
                int jnext; /* next jump label */
                struct FuncAttr f; /* function attributes */
                int auxtype; /* bitfield access type */
            };
        };
        long long enum_val; /* enum constant if IS_ENUM_VAL */
        int *d; /* define token stream */
    };
    CType type; /* associated type */
    union {
        struct Sym *next; /* next related symbol (for fields and anoms) */
        int asm_label; /* associated asm label */
    };
    struct Sym *prev; /* prev symbol in stack */
    struct Sym *prev_tok; /* previous symbol for this token */
} Sym;

/* section definition */
typedef struct Section {
    unsigned long data_offset; /* current data offset */
    unsigned char *data;       /* section data */
    unsigned long data_allocated; /* used for realloc() handling */
    int sh_name;             /* elf section name (only used during output) */
    int sh_num;              /* elf section number */
    int sh_type;             /* elf section type */
    int sh_flags;            /* elf section flags */
    int sh_info;             /* elf section info */
    int sh_addralign;        /* elf section alignment */
    int sh_entsize;          /* elf entry size */
    unsigned long sh_size;   /* section size (only used during output) */
    addr_t sh_addr;          /* address at which the section is relocated */
    unsigned long sh_offset; /* file offset */
    int nb_hashed_syms;      /* used to resize the hash table */
    struct Section *link;    /* link to another section */
    struct Section *reloc;   /* corresponding section for relocation, if any */
    struct Section *hash;    /* hash table for symbols */
    struct Section *prev;    /* previous section on section stack */
    char name[1];           /* section name */
} Section;

typedef struct DLLReference {
    int level;
    void *handle;
    char name[1];
} DLLReference;

/* -------------------------------------------------- */

#define SYM_STRUCT     0x40000000 /* struct/union/enum symbol space */
#define SYM_FIELD      0x20000000 /* struct/union field symbol space */
#define SYM_FIRST_ANOM 0x10000000 /* first anonymous sym */

/* stored in 'Sym->f.func_type' field */
#define FUNC_NEW       1 /* ansi function prototype */
#define FUNC_OLD       2 /* old function prototype */
#define FUNC_ELLIPSIS  3 /* ansi function prototype with ... */

/* stored in 'Sym->f.func_call' field */
#define FUNC_CDECL     0 /* standard c call */
#define FUNC_STDCALL   1 /* pascal c call */
#define FUNC_FASTCALL1 2 /* first param in %eax */
#define FUNC_FASTCALL2 3 /* first parameters in %eax, %edx */
#define FUNC_FASTCALL3 4 /* first parameter in %eax, %edx, %ecx */
#define FUNC_FASTCALLW 5 /* first parameter in %ecx, %edx */

/* field 'Sym.t' for macros */
#define MACRO_OBJ      0 /* object like macro */
#define MACRO_FUNC     1 /* function like macro */

/* field 'Sym.r' for C labels */
#define LABEL_DEFINED  0 /* label is defined */
#define LABEL_FORWARD  1 /* label is forward defined */
#define LABEL_DECLARED 2 /* label is declared but never used */

/* type_decl() types */
#define TYPE_ABSTRACT  1 /* type without variable */
#define TYPE_DIRECT    2 /* type with variable */

#define IO_BUF_SIZE 8192

typedef struct BufferedFile {
    uint8_t *buf_ptr;
    uint8_t *buf_end;
    int fd;
    struct BufferedFile *prev;
    int line_num;    /* current line number - here to simplify code */
    int line_ref;    /* tcc -E: last printed line */
    int ifndef_macro;  /* #ifndef macro / #endif search */
    int ifndef_macro_saved; /* saved ifndef_macro */
    int *ifdef_stack_ptr; /* ifdef_stack value at the start of the file */
    int include_next_index; /* next search path */
    char filename[1024];    /* filename */
    char *true_filename; /* filename not modified by # line directive */
    unsigned char unget[4];
    unsigned char buffer[1]; /* extra size for CH_EOB char */
} BufferedFile;

#define CH_EOB   '\\'       /* end of buffer or '\0' char in file */
#define CH_EOF   (-1)   /* end of file */

/* used to record tokens */
typedef struct TokenString {
    int *str;
    int len;
    int lastlen;
    int allocated_len;
    int last_line_num;
    int save_line_num;
    /* used to chain token-strings with begin/end_macro() */
    struct TokenString *prev;
    const int *prev_ptr;
    char alloc;
} TokenString;

/* inline functions */
typedef struct InlineFunc {
    TokenString *func_str;
    Sym *sym;
    char filename[1];
} InlineFunc;

/* include file cache, used to find files faster and also to eliminate
   inclusion if the include file is protected by #ifndef ... #endif */
typedef struct CachedInclude {
    int ifndef_macro;
    int once;
    int hash_next; /* -1 if none */
    char filename[1]; /* path specified in #include */
} CachedInclude;

#define CACHED_INCLUDES_HASH_SIZE 32

#ifdef CONFIG_TCC_ASM
typedef struct ExprValue {
    uint64_t v;
    Sym *sym;
    int pcrel;
} ExprValue;

#define MAX_ASM_OPERANDS 30
typedef struct ASMOperand {
    int id; /* GCC 3 optional identifier (0 if number only supported */
    char *constraint;
    char asm_str[16]; /* computed asm string for operand */
    SValue *vt; /* C value of the expression */
    int ref_index; /* if >= 0, gives reference to a output constraint */
    int input_index; /* if >= 0, gives reference to an input constraint */
    int priority; /* priority, used to assign registers */
    int reg; /* if >= 0, register number used for this operand */
    int is_llong; /* true if double register value */
    int is_memory; /* true if memory operand */
    int is_rw;     /* for '+' modifier */
} ASMOperand;
#endif

/* extra symbol attributes (not in symbol table) */
struct sym_attr {
    unsigned got_offset;
    unsigned plt_offset;
    int plt_sym;
    int dyn_index;
#ifdef TCC_TARGET_ARM
    unsigned char plt_thumb_stub:1;
#endif
};

struct case_t {
    int64_t v1, v2;
    int sym;
};

struct switch_t {
    struct case_t **p; int n; /* list of case ranges */
    int def_sym; /* default symbol */
};

/* -------------------------------------------------- */

#define HASHMAP_INIT        64
#define HASHMAP_LOAD_FAC    0.75

typedef void (*hashdtor_t)(TCCState *, void *);
typedef size_t (*hashprobe_t)(TCCState *, const char *, size_t, size_t);

struct hashitem_t {
    struct hashitem_t *prev;
    struct hashitem_t *next;
    int use;
    int hash;
    char *key;
    void *value;
    hashdtor_t dtor;
};

struct hashmap_t {
    size_t count;
    size_t bucket_size;
    hashprobe_t prob_func;
    struct hashitem_t list;
    struct hashitem_t *bucket;
};

struct TCCState {

    int verbose; /* if true, display some information during compilation */
    int nostdinc; /* if true, no standard headers are added */
    int nostdlib; /* if true, no standard libraries are added */
    int nocommon; /* if true, do not use common symbols for .bss data */
    int static_link; /* if true, static linking is performed */
    int rdynamic; /* if true, all symbols are exported */
    int symbolic; /* if true, resolve symbols in the current module first */
    int alacarte_link; /* if true, only link in referenced objects from archive */

    char *tcc_lib_path; /* CONFIG_TCCDIR or -B option */
    char *soname; /* as specified on the command line (-soname) */
    char *rpath; /* as specified on the command line (-Wl,-rpath=) */
    int enable_new_dtags; /* ditto, (-Wl,--enable-new-dtags) */

    /* output type, see TCC_OUTPUT_XXX */
    int output_type;
    /* output format, see TCC_OUTPUT_FORMAT_xxx */
    int output_format;

    /* C language options */
    int char_is_unsigned;
    int leading_underscore;
    int ms_extensions;	/* allow nested named struct w/o identifier behave like unnamed */
    int dollars_in_identifiers;	/* allows '$' char in identifiers */
    int ms_bitfields; /* if true, emulate MS algorithm for aligning bitfields */

    /* warning switches */
    int warn_write_strings;
    int warn_unsupported;
    int warn_error;
    int warn_none;
    int warn_implicit_function_declaration;
    int warn_gcc_compat;

    /* compile with debug symbol (and use them if error during execution) */
    int do_debug;
#ifdef CONFIG_TCC_BCHECK
    /* compile with built-in memory and bounds checker */
    int do_bounds_check;
#endif
#ifdef TCC_TARGET_ARM
    enum float_abi float_abi; /* float ABI of the generated code*/
#endif
    int run_test; /* nth test to run with -dt -run */

    addr_t text_addr; /* address of text section */
    int has_text_addr;

    unsigned section_align; /* section alignment */

    char *init_symbol; /* symbols to call at load-time (not used currently) */
    char *fini_symbol; /* symbols to call at unload-time (not used currently) */

#ifdef TCC_TARGET_I386
    int seg_size; /* 32. Can be 16 with i386 assembler (.code16) */
#endif
#ifdef TCC_TARGET_X86_64
    int nosse; /* For -mno-sse support. */
#endif

    /* array of all loaded dlls (including those referenced by loaded dlls) */
    DLLReference **loaded_dlls;
    int nb_loaded_dlls;

    /* include paths */
    char **include_paths;
    int nb_include_paths;

    char **sysinclude_paths;
    int nb_sysinclude_paths;

    /* library paths */
    char **library_paths;
    int nb_library_paths;

    /* crt?.o object path */
    char **crt_paths;
    int nb_crt_paths;

    /* -include files */
    char **cmd_include_files;
    int nb_cmd_include_files;

    /* error handling */
    void *error_opaque;
    void (*error_func)(TCCState *s, int is_warning, const char *file,
                       int lineno, const char *msg, void *opaque);
    int error_set_jmp_enabled;
    jmp_buf error_jmp_buf;
    int nb_errors;

    /* output file for preprocessing (-E) */
    FILE *ppfp;
    enum {
	LINE_MACRO_OUTPUT_FORMAT_GCC,
	LINE_MACRO_OUTPUT_FORMAT_NONE,
	LINE_MACRO_OUTPUT_FORMAT_STD,
    LINE_MACRO_OUTPUT_FORMAT_P10 = 11
    } Pflag; /* -P switch */
    char dflag; /* -dX value */

    /* for -MD/-MF: collected dependencies for this compilation */
    char **target_deps;
    int nb_target_deps;

    /* compilation */
    BufferedFile *include_stack[INCLUDE_STACK_SIZE];
    BufferedFile **include_stack_ptr;

    int ifdef_stack[IFDEF_STACK_SIZE];
    int *ifdef_stack_ptr;

    /* included files enclosed with #ifndef MACRO */
    int cached_includes_hash[CACHED_INCLUDES_HASH_SIZE];
    CachedInclude **cached_includes;
    int nb_cached_includes;

    /* #pragma pack stack */
    int pack_stack[PACK_STACK_SIZE];
    int *pack_stack_ptr;
    char **pragma_libs;
    int nb_pragma_libs;

    /* inline functions are stored as token lists and compiled last
       only if referenced */
    struct InlineFunc **inline_fns;
    int nb_inline_fns;

    /* sections */
    Section **sections;
    int nb_sections; /* number of sections, including first dummy section */

    Section **priv_sections;
    int nb_priv_sections; /* number of private sections */

    /* got & plt handling */
    Section *got;
    Section *plt;

    /* temporary dynamic symbol sections (for dll loading) */
    Section *dynsymtab_section;
    /* exported dynamic symbol section */
    Section *dynsym;
    /* copy of the global symtab_section variable */
    Section *symtab;
    /* extra attributes (eg. GOT/PLT value) for symtab symbols */
    struct sym_attr *sym_attrs;
    int nb_sym_attrs;

#ifdef TCC_TARGET_PE
    /* PE info */
    int pe_subsystem;
    unsigned pe_characteristics;
    unsigned pe_file_align;
    unsigned pe_stack_size;
    addr_t pe_imagebase;
# ifdef TCC_TARGET_X86_64
    Section *uw_pdata;
    int uw_sym;
    unsigned uw_offs;
# endif
#endif

#ifdef _WIN64
    void **function_table;
    int nb_function_table;
#endif

#ifdef TCC_IS_NATIVE
    const char *runtime_main;
    void **runtime_mem;
    int nb_runtime_mem;
#endif

    /* used by main and tcc_parse_args only */
    struct filespec **files; /* files seen on command line */
    int nb_files; /* number thereof */
    int nb_libraries; /* number of libs thereof */
    int filetype;
    char *outfile; /* output filename */
    int option_r; /* option -r */
    int do_bench; /* option -bench */
    int gen_deps; /* option -MD  */
    char *deps_outfile; /* option -MF */
    int option_pthread; /* -pthread option */
    int argc;
    char **argv;

    /* all the global variables */
    int rsym, anon_sym, ind, loc;
    Sym *sym_free_first;
    void **sym_pools;
    int nb_sym_pools;
    Sym *global_stack;
    Sym *local_stack;
    Sym *define_stack;
    Sym *global_label_stack;
    Sym *local_label_stack;
    int vlas_in_scope; /* number of VLAs that are currently in scope */
    int vla_sp_root_loc; /* vla_sp_loc for SP before any VLAs were pushed */
    int vla_sp_loc; /* Pointer to variable holding location to store stack pointer on the stack when modifying stack pointer */
    SValue __vstack[1+VSTACK_SIZE], *vtop, *pvtop;
    int const_wanted; /* true if constant wanted */
    int nocode_wanted; /* no code generation wanted */
    int global_expr;  /* true if compound literals must be allocated globally (used during initializers parsing */
    CType func_vt; /* current function return type (used by return instruction) */
    int func_var; /* true if current function is variadic (used by return instruction) */
    int func_vc;
    int last_line_num, last_ind, func_ind; /* debug last line number and pc */
    const char *funcname;
    int g_debug;
    CType char_pointer_type, func_old_type, int_type, size_type, ptrdiff_type;
    Section *text_section, *data_section, *bss_section; /* predefined sections */
    Section *common_section;
    Section *cur_text_section; /* current section where function code is generated */
    Section *last_text_section; /* to handle .previous asm directive */
    Section *bounds_section; /* contains global data bound description */
    Section *lbounds_section; /* contains local data bound description */
    Section *symtab_section;
    Section *stab_section, *stabstr_section;
    int gnu_ext;
    int tcc_ext;
    int tok_flags;
    int parse_flags;
    struct BufferedFile *file;
    int ch, tok;
    CValue tokc;
    const int *macro_ptr;
    CString tokcstr; /* current parsed string, if any */
    int total_lines;
    int total_bytes;
    int tok_ident;
    TokenSym **table_ident;
    int rt_num_callers;
    const char **rt_bound_error_msg;
    void *rt_prog_main;
    int local_scope;
    int in_sizeof;
    int section_sym;
    int new_undef_sym;
    struct switch_t *cur_switch; /* current switch */
    TokenSym *hash_ident[TOK_HASH_SIZE];
    char token_buf[STRING_MAX_SIZE + 1];
    CString cstr_buf;
    CString macro_equal_buf;
    TokenString tokstr_buf;
    unsigned char isidnum_table[256 - CH_EOF];
    int pp_debug_tok, pp_debug_symv;
    int pp_once;
    int pp_expr;
    int pp_counter;
    struct TinyAlloc *toksym_alloc;
    struct TinyAlloc *tokstr_alloc;
    struct TinyAlloc *cstr_alloc;
    TokenString *macro_stack;
    void **dlls;
    int nb_dlls;

    /* types and functions */
    struct hashmap_t funcs;
    struct hashmap_t types;
};

struct TCCType {
    int t;
    int rc;
    char *name;
    TCCType *ref;
    union {
        TCCType **types;
        long long *values;
    };
    char **names;
    int nb_names;
    int nb_values;
    unsigned short alignment;
};

struct TCCFunction {
    char *name;
    void *addr;
    TCCType *ret;
    TCCType **args;
    char **arg_names;
    int nb_args;
    int nb_arg_names;
    char is_variadic;
};

struct filespec {
    char type;
    char alacarte;
    char name[1];
};

/* token values */

/* warning: the following compare tokens depend on i386 asm code */
#define TOK_ULT 0x92
#define TOK_UGE 0x93
#define TOK_EQ  0x94
#define TOK_NE  0x95
#define TOK_ULE 0x96
#define TOK_UGT 0x97
#define TOK_Nset 0x98
#define TOK_Nclear 0x99
#define TOK_LT  0x9c
#define TOK_GE  0x9d
#define TOK_LE  0x9e
#define TOK_GT  0x9f

#define TOK_LAND  0xa0
#define TOK_LOR   0xa1
#define TOK_DEC   0xa2
#define TOK_MID   0xa3 /* inc/dec, to void constant */
#define TOK_INC   0xa4
#define TOK_UDIV  0xb0 /* unsigned division */
#define TOK_UMOD  0xb1 /* unsigned modulo */
#define TOK_PDIV  0xb2 /* fast division with undefined rounding for pointers */

/* tokens that carry values (in additional token string space / tokc) --> */
#define TOK_CCHAR   0xb3 /* char constant in tokc */
#define TOK_LCHAR   0xb4
#define TOK_CINT    0xb5 /* number in tokc */
#define TOK_CUINT   0xb6 /* unsigned int constant */
#define TOK_CLLONG  0xb7 /* long long constant */
#define TOK_CULLONG 0xb8 /* unsigned long long constant */
#define TOK_STR     0xb9 /* pointer to string in tokc */
#define TOK_LSTR    0xba
#define TOK_CFLOAT  0xbb /* float constant */
#define TOK_CDOUBLE 0xbc /* double constant */
#define TOK_CLDOUBLE 0xbd /* long double constant */
#define TOK_PPNUM   0xbe /* preprocessor number */
#define TOK_PPSTR   0xbf /* preprocessor string */
#define TOK_LINENUM 0xc0 /* line number info */
#define TOK_TWODOTS 0xa8 /* C++ token ? */
/* <-- */

#define TOK_UMULL    0xc2 /* unsigned 32x32 -> 64 mul */
#define TOK_ADDC1    0xc3 /* add with carry generation */
#define TOK_ADDC2    0xc4 /* add with carry use */
#define TOK_SUBC1    0xc5 /* add with carry generation */
#define TOK_SUBC2    0xc6 /* add with carry use */
#define TOK_ARROW    0xc7
#define TOK_DOTS     0xc8 /* three dots */
#define TOK_SHR      0xc9 /* unsigned shift right */
#define TOK_TWOSHARPS 0xca /* ## preprocessing token */
#define TOK_PLCHLDR  0xcb /* placeholder token as defined in C99 */
#define TOK_NOSUBST  0xcc /* means following token has already been pp'd */
#define TOK_PPJOIN   0xcd /* A '##' in the right position to mean pasting */
#define TOK_CLONG    0xce /* long constant */
#define TOK_CULONG   0xcf /* unsigned long constant */

#define TOK_SHL   0x01 /* shift left */
#define TOK_SAR   0x02 /* signed shift right */

/* assignment operators : normal operator or 0x80 */
#define TOK_A_MOD 0xa5
#define TOK_A_AND 0xa6
#define TOK_A_MUL 0xaa
#define TOK_A_ADD 0xab
#define TOK_A_SUB 0xad
#define TOK_A_DIV 0xaf
#define TOK_A_XOR 0xde
#define TOK_A_OR  0xfc
#define TOK_A_SHL 0x81
#define TOK_A_SAR 0x82

#define TOK_EOF       (-1)  /* end of file */
#define TOK_LINEFEED  10    /* line feed */

/* all identifiers and strings have token above that */
#define TOK_IDENT 256

#define DEF_ASM(x) DEF(TOK_ASM_ ## x, #x)
#define TOK_ASM_int TOK_INT
#define DEF_ASMDIR(x) DEF(TOK_ASMDIR_ ## x, "." #x)
#define TOK_ASMDIR_FIRST TOK_ASMDIR_byte
#define TOK_ASMDIR_LAST TOK_ASMDIR_section

#if defined TCC_TARGET_I386 || defined TCC_TARGET_X86_64
/* only used for i386 asm opcodes definitions */
#define DEF_BWL(x) \
 DEF(TOK_ASM_ ## x ## b, #x "b") \
 DEF(TOK_ASM_ ## x ## w, #x "w") \
 DEF(TOK_ASM_ ## x ## l, #x "l") \
 DEF(TOK_ASM_ ## x, #x)
#define DEF_WL(x) \
 DEF(TOK_ASM_ ## x ## w, #x "w") \
 DEF(TOK_ASM_ ## x ## l, #x "l") \
 DEF(TOK_ASM_ ## x, #x)
#ifdef TCC_TARGET_X86_64
# define DEF_BWLQ(x) \
 DEF(TOK_ASM_ ## x ## b, #x "b") \
 DEF(TOK_ASM_ ## x ## w, #x "w") \
 DEF(TOK_ASM_ ## x ## l, #x "l") \
 DEF(TOK_ASM_ ## x ## q, #x "q") \
 DEF(TOK_ASM_ ## x, #x)
# define DEF_WLQ(x) \
 DEF(TOK_ASM_ ## x ## w, #x "w") \
 DEF(TOK_ASM_ ## x ## l, #x "l") \
 DEF(TOK_ASM_ ## x ## q, #x "q") \
 DEF(TOK_ASM_ ## x, #x)
# define DEF_BWLX DEF_BWLQ
# define DEF_WLX DEF_WLQ
/* number of sizes + 1 */
# define NBWLX 5
#else
# define DEF_BWLX DEF_BWL
# define DEF_WLX DEF_WL
/* number of sizes + 1 */
# define NBWLX 4
#endif

#define DEF_FP1(x) \
 DEF(TOK_ASM_ ## f ## x ## s, "f" #x "s") \
 DEF(TOK_ASM_ ## fi ## x ## l, "fi" #x "l") \
 DEF(TOK_ASM_ ## f ## x ## l, "f" #x "l") \
 DEF(TOK_ASM_ ## fi ## x ## s, "fi" #x "s")

#define DEF_FP(x) \
 DEF(TOK_ASM_ ## f ## x, "f" #x ) \
 DEF(TOK_ASM_ ## f ## x ## p, "f" #x "p") \
 DEF_FP1(x)

#define DEF_ASMTEST(x,suffix) \
 DEF_ASM(x ## o ## suffix) \
 DEF_ASM(x ## no ## suffix) \
 DEF_ASM(x ## b ## suffix) \
 DEF_ASM(x ## c ## suffix) \
 DEF_ASM(x ## nae ## suffix) \
 DEF_ASM(x ## nb ## suffix) \
 DEF_ASM(x ## nc ## suffix) \
 DEF_ASM(x ## ae ## suffix) \
 DEF_ASM(x ## e ## suffix) \
 DEF_ASM(x ## z ## suffix) \
 DEF_ASM(x ## ne ## suffix) \
 DEF_ASM(x ## nz ## suffix) \
 DEF_ASM(x ## be ## suffix) \
 DEF_ASM(x ## na ## suffix) \
 DEF_ASM(x ## nbe ## suffix) \
 DEF_ASM(x ## a ## suffix) \
 DEF_ASM(x ## s ## suffix) \
 DEF_ASM(x ## ns ## suffix) \
 DEF_ASM(x ## p ## suffix) \
 DEF_ASM(x ## pe ## suffix) \
 DEF_ASM(x ## np ## suffix) \
 DEF_ASM(x ## po ## suffix) \
 DEF_ASM(x ## l ## suffix) \
 DEF_ASM(x ## nge ## suffix) \
 DEF_ASM(x ## nl ## suffix) \
 DEF_ASM(x ## ge ## suffix) \
 DEF_ASM(x ## le ## suffix) \
 DEF_ASM(x ## ng ## suffix) \
 DEF_ASM(x ## nle ## suffix) \
 DEF_ASM(x ## g ## suffix)

#endif /* defined TCC_TARGET_I386 || defined TCC_TARGET_X86_64 */

enum tcc_token {
    TOK_LAST = TOK_IDENT - 1
#define DEF(id, str) ,id
#include "tcctok.h"
#undef DEF
};

/* keywords: tok >= TOK_IDENT && tok < TOK_UIDENT */
#define TOK_UIDENT TOK_DEFINE

/* ------------ libtcc.c ------------ */

/* public functions currently used by the tcc main function */
ST_FUNC char *pstrcpy(char *buf, int buf_size, const char *s);
ST_FUNC char *pstrcat(char *buf, int buf_size, const char *s);
ST_FUNC char *pstrncpy(char *out, const char *in, size_t num);
PUB_FUNC char *tcc_basename(const char *name);
PUB_FUNC char *tcc_fileextension (const char *name);

#ifndef MEM_DEBUG
PUB_FUNC void tcc_free(TCCState *s1, void *ptr);
PUB_FUNC void *tcc_malloc(TCCState *s1, unsigned long size);
PUB_FUNC void *tcc_mallocz(TCCState *s1, unsigned long size);
PUB_FUNC void *tcc_realloc(TCCState *s1, void *ptr, unsigned long size);
PUB_FUNC char *tcc_strdup(TCCState *s1, const char *str);
#else
#define tcc_free(s1, ptr)           tcc_free_debug(s1, ptr)
#define tcc_malloc(s1, size)        tcc_malloc_debug(s1, size, __FILE__, __LINE__)
#define tcc_mallocz(s1, size)       tcc_mallocz_debug(s1, size, __FILE__, __LINE__)
#define tcc_realloc(s1, ptr,size)   tcc_realloc_debug(s1, ptr, size, __FILE__, __LINE__)
#define tcc_strdup(s1, str)         tcc_strdup_debug(s1, str, __FILE__, __LINE__)
PUB_FUNC void tcc_free_debug(TCCState *s1, void *ptr);
PUB_FUNC void *tcc_malloc_debug(TCCState *s1, unsigned long size, const char *file, int line);
PUB_FUNC void *tcc_mallocz_debug(TCCState *s1, unsigned long size, const char *file, int line);
PUB_FUNC void *tcc_realloc_debug(TCCState *s1, void *ptr, unsigned long size, const char *file, int line);
PUB_FUNC char *tcc_strdup_debug(TCCState *s1, const char *str, const char *file, int line);
#endif

PUB_FUNC void tcc_error_noabort(TCCState *s1, const char *fmt, ...);
PUB_FUNC NORETURN void tcc_error(TCCState *s1, const char *fmt, ...);
PUB_FUNC void tcc_warning(TCCState *s1, const char *fmt, ...);

/* other utilities */
ST_FUNC void dynarray_add(TCCState *s1, void *ptab, int *nb_ptr, void *data);
ST_FUNC void dynarray_reset(TCCState *s1, void *pp, int *n);
ST_INLN void cstr_ccat(TCCState *s1, CString *cstr, int ch);
ST_FUNC void cstr_cat(TCCState *s1, CString *cstr, const char *str, int len);
ST_FUNC void cstr_wccat(TCCState *s1, CString *cstr, int ch);
ST_FUNC void cstr_new(TCCState *s1, CString *cstr);
ST_FUNC void cstr_free(TCCState *s1, CString *cstr);
ST_FUNC void cstr_reset(TCCState *s1, CString *cstr);
ST_FUNC void hashmap_new(TCCState *s1, struct hashmap_t *map, hashprobe_t prob_func);
ST_FUNC void hashmap_free(TCCState *s1, struct hashmap_t *map);
ST_FUNC void hashmap_insert(TCCState *s1, struct hashmap_t *map, const char *key, void *value, hashdtor_t dtor);
ST_FUNC void **hashmap_lookup(TCCState *s1, struct hashmap_t *map, const char *key);

ST_INLN void sym_free(TCCState *s1, Sym *sym);
ST_FUNC Sym *sym_push2(TCCState *s1, Sym **ps, int v, int t, int c);
ST_FUNC Sym *sym_find2(TCCState *s1, Sym *s, int v);
ST_FUNC Sym *sym_push(TCCState *s1, int v, CType *type, int r, int c);
ST_FUNC void sym_pop(TCCState *s1, Sym **ptop, Sym *b, int keep);
ST_INLN Sym *struct_find(TCCState *s1, int v);
ST_INLN Sym *sym_find(TCCState *s1, int v);
ST_FUNC Sym *global_identifier_push(TCCState *s1, int v, int t, int c);

ST_FUNC void tcc_open_bf(TCCState *s1, const char *filename, int initlen);
ST_FUNC int tcc_open(TCCState *s1, const char *filename);
ST_FUNC void tcc_close(TCCState *s1);

ST_FUNC int tcc_add_file_internal(TCCState *s1, const char *filename, int flags);
/* flags: */
#define AFF_PRINT_ERROR     0x10 /* print error if file not found */
#define AFF_REFERENCED_DLL  0x20 /* load a referenced dll from another dll */
#define AFF_TYPE_BIN        0x40 /* file to add is binary */
/* s->filetype: */
#define AFF_TYPE_NONE   0
#define AFF_TYPE_C      1
#define AFF_TYPE_ASM    2
#define AFF_TYPE_ASMPP  3
#define AFF_TYPE_LIB    4
/* values from tcc_object_type(...) */
#define AFF_BINTYPE_REL 1
#define AFF_BINTYPE_DYN 2
#define AFF_BINTYPE_AR  3
#define AFF_BINTYPE_C67 4

ST_FUNC void tcc_resolver_free(TCCState *s1);
ST_FUNC void tcc_resolver_reset(TCCState *s1);
ST_FUNC void tcc_resolver_ref_type(TCCState *s1, CType *type, const char *name);
ST_FUNC TCCType *tcc_resolver_add_type(TCCState *s1, CType *type);
ST_FUNC TCCFunction *tcc_resolver_add_func(TCCState *s1, const char *funcname, char is_variadic, CType *ret);

ST_FUNC int tcc_add_crt(TCCState *s, const char *filename);
ST_FUNC int tcc_add_dll(TCCState *s, const char *filename, int flags);
ST_FUNC void tcc_add_pragma_libs(TCCState *s1);
PUB_FUNC int tcc_add_library_err(TCCState *s, const char *f);
PUB_FUNC void tcc_print_stats(TCCState *s, unsigned total_time);
PUB_FUNC int tcc_parse_args(TCCState *s, int *argc, char ***argv, int optind);
#ifdef _WIN32
ST_FUNC char *normalize_slashes(char *path);
#endif

/* tcc_parse_args return codes: */
#define OPT_HELP 1
#define OPT_HELP2 2
#define OPT_V 3
#define OPT_PRINT_DIRS 4
#define OPT_AR 5
#define OPT_IMPDEF 6
#define OPT_M32 32
#define OPT_M64 64

/* ------------ tccpp.c ------------ */

#define TOK_FLAG_BOL   0x0001 /* beginning of line before */
#define TOK_FLAG_BOF   0x0002 /* beginning of file before */
#define TOK_FLAG_ENDIF 0x0004 /* a endif was found matching starting #ifdef */
#define TOK_FLAG_EOF   0x0008 /* end of file */

#define PARSE_FLAG_PREPROCESS 0x0001 /* activate preprocessing */
#define PARSE_FLAG_TOK_NUM    0x0002 /* return numbers instead of TOK_PPNUM */
#define PARSE_FLAG_LINEFEED   0x0004 /* line feed is returned as a
                                        token. line feed is also
                                        returned at eof */
#define PARSE_FLAG_ASM_FILE 0x0008 /* we processing an asm file: '#' can be used for line comment, etc. */
#define PARSE_FLAG_SPACES     0x0010 /* next() returns space tokens (for -E) */
#define PARSE_FLAG_ACCEPT_STRAYS 0x0020 /* next() returns '\\' token */
#define PARSE_FLAG_TOK_STR    0x0040 /* return parsed strings instead of TOK_PPSTR */

/* isidnum_table flags: */
#define IS_SPC 1
#define IS_ID  2
#define IS_NUM 4

ST_FUNC TokenSym *tok_alloc(TCCState *s1, const char *str, int len);
ST_FUNC const char *get_tok_str(TCCState *s1, int v, CValue *cv);
ST_FUNC void begin_macro(TCCState *s1, TokenString *str, int alloc);
ST_FUNC void end_macro(TCCState *s1);
ST_FUNC int set_idnum(TCCState *s1, int c, int val);
ST_INLN void tok_str_new(TCCState *s1, TokenString *s);
ST_FUNC TokenString *tok_str_alloc(TCCState *s1);
ST_FUNC void tok_str_free(TCCState *s1, TokenString *s);
ST_FUNC void tok_str_free_str(TCCState *s1, int *str);
ST_FUNC void tok_str_add(TCCState *s1, TokenString *s, int t);
ST_FUNC void tok_str_add_tok(TCCState *s1, TokenString *s);
ST_INLN void define_push(TCCState *s1, int v, int macro_type, int *str, Sym *first_arg);
ST_FUNC void define_undef(TCCState *s1, Sym *s);
ST_INLN Sym *define_find(TCCState *s1, int v);
ST_FUNC void free_defines(TCCState *s1, Sym *b);
ST_FUNC Sym *label_find(TCCState *s1, int v);
ST_FUNC Sym *label_push(TCCState *s1, Sym **ptop, int v, int flags);
ST_FUNC void label_pop(TCCState *s1, Sym **ptop, Sym *slast, int keep);
ST_FUNC void parse_define(TCCState *s1);
ST_FUNC void preprocess(TCCState *s1, int is_bof);
ST_FUNC void next_nomacro(TCCState *s1);
ST_FUNC void next(TCCState *s1);
ST_INLN void unget_tok(TCCState *s1, int last_tok);
ST_FUNC void preprocess_start(TCCState *s1, int is_asm);
ST_FUNC void preprocess_end(TCCState *s1);
ST_FUNC void tccpp_new(TCCState *s);
ST_FUNC void tccpp_delete(TCCState *s);
ST_FUNC int tcc_preprocess(TCCState *s1);
ST_FUNC void skip(TCCState *s1, int c);
ST_FUNC NORETURN void expect(TCCState *s1, const char *msg);

/* space excluding newline */
static inline int is_space(int ch) {
    return ch == ' ' || ch == '\t' || ch == '\v' || ch == '\f' || ch == '\r';
}
static inline int isid(int c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
static inline int isnum(int c) {
    return c >= '0' && c <= '9';
}
static inline int isoct(int c) {
    return c >= '0' && c <= '7';
}
static inline int toup(int c) {
    return (c >= 'a' && c <= 'z') ? c - 'a' + 'A' : c;
}

/* ------------ tccgen.c ------------ */

#define SYM_POOL_NB (8192 / sizeof(Sym))

ST_FUNC void tcc_debug_start(TCCState *s1);
ST_FUNC void tcc_debug_end(TCCState *s1);
ST_FUNC void tcc_debug_funcstart(TCCState *s1, Sym *sym);
ST_FUNC void tcc_debug_funcend(TCCState *s1, int size);
ST_FUNC void tcc_debug_line(TCCState *s1);

ST_FUNC int tccgen_compile(TCCState *s1);
ST_FUNC void free_inline_functions(TCCState *s);
ST_FUNC void check_vstack(TCCState *s1);

ST_INLN int is_float(int t);
ST_FUNC int ieee_finite(double d);
ST_FUNC void test_lvalue(TCCState *s1);
ST_FUNC void vpushi(TCCState *s1, int v);
ST_FUNC ElfSym *elfsym(TCCState *s1, Sym *);
ST_FUNC void update_storage(TCCState *s1, Sym *sym);
ST_FUNC Sym *external_global_sym(TCCState *s1, int v, CType *type, int r);
ST_FUNC void vset(TCCState *s1, CType *type, int r, int v);
ST_FUNC void vswap(TCCState *s1);
ST_FUNC void vpush_global_sym(TCCState *s1, CType *type, int v);
ST_FUNC void vrote(TCCState *s1, SValue *e, int n);
ST_FUNC void vrott(TCCState *s1, int n);
ST_FUNC void vrotb(TCCState *s1, int n);
#ifdef TCC_TARGET_ARM
ST_FUNC int get_reg_ex(TCCState *s1, int rc, int rc2);
ST_FUNC void lexpand_nr(TCCState *s1);
#endif
ST_FUNC void vpushv(TCCState *s1, SValue *v);
ST_FUNC void save_reg(TCCState *s1, int r);
ST_FUNC void save_reg_upstack(TCCState *s1, int r, int n);
ST_FUNC int get_reg(TCCState *s1, int rc);
ST_FUNC void save_regs(TCCState *s1, int n);
ST_FUNC void gaddrof(TCCState *s1);
ST_FUNC int gv(TCCState *s1, int rc);
ST_FUNC void gv2(TCCState *s1, int rc1, int rc2);
ST_FUNC void vpop(TCCState *s1);
ST_FUNC void gen_op(TCCState *s1, int op);
ST_FUNC int type_size(CType *type, int *a);
ST_FUNC void mk_pointer(TCCState *s1, CType *type);
ST_FUNC void type_to_str(TCCState *s1, char *buf, int buf_size, CType *type, const char *varstr);
ST_FUNC void vstore(TCCState *s1);
ST_FUNC void inc(TCCState *s1, int post, int c);
ST_FUNC void parse_mult_str (TCCState *s1, CString *astr, const char *msg);
ST_FUNC void parse_asm_str(TCCState *s1, CString *astr);
ST_FUNC int lvalue_type(int t);
ST_FUNC void indir(TCCState *s1);
ST_FUNC void unary(TCCState *s1);
ST_FUNC void expr_prod(TCCState *s1);
ST_FUNC void expr_sum(TCCState *s1);
ST_FUNC void gexpr(TCCState *s1);
ST_FUNC int expr_const(TCCState *s1);
#if defined CONFIG_TCC_BCHECK || defined TCC_TARGET_C67
ST_FUNC Sym *get_sym_ref(TCCState *s1, CType *type, Section *sec, unsigned long offset, unsigned long size);
#endif
#if defined TCC_TARGET_X86_64 && !defined TCC_TARGET_PE
ST_FUNC int classify_x86_64_va_arg(CType *ty);
#endif

/* ------------ tccelf.c ------------ */

#define TCC_OUTPUT_FORMAT_ELF    0 /* default output format: ELF */
#define TCC_OUTPUT_FORMAT_BINARY 1 /* binary image output */
#define TCC_OUTPUT_FORMAT_COFF   2 /* COFF */

#define ARMAG  "!<arch>\012"    /* For COFF and a.out archives */

typedef struct {
    unsigned int n_strx;         /* index into string table of name */
    unsigned char n_type;         /* type of symbol */
    unsigned char n_other;        /* misc info (usually empty) */
    unsigned short n_desc;        /* description field */
    unsigned int n_value;        /* value of symbol */
} Stab_Sym;

#ifdef CONFIG_TCC_BCHECK
/* bound check related sections */
ST_FUNC void tccelf_bounds_new(TCCState *s);
#endif

ST_FUNC void tccelf_new(TCCState *s);
ST_FUNC void tccelf_delete(TCCState *s);
ST_FUNC void tccelf_stab_new(TCCState *s);
ST_FUNC void tccelf_begin_file(TCCState *s1);
ST_FUNC void tccelf_end_file(TCCState *s1);

ST_FUNC Section *new_section(TCCState *s1, const char *name, int sh_type, int sh_flags);
ST_FUNC void section_realloc(TCCState *s1, Section *sec, unsigned long new_size);
ST_FUNC size_t section_add(TCCState *s1, Section *sec, addr_t size, int align);
ST_FUNC void *section_ptr_add(TCCState *s1, Section *sec, addr_t size);
ST_FUNC void section_reserve(TCCState *s1, Section *sec, unsigned long size);
ST_FUNC Section *find_section(TCCState *s1, const char *name);
ST_FUNC Section *new_symtab(TCCState *s1, const char *symtab_name, int sh_type, int sh_flags, const char *strtab_name, const char *hash_name, int hash_sh_flags);

ST_FUNC void put_extern_sym2(TCCState *s1, Sym *sym, int sh_num, addr_t value, unsigned long size, int can_add_underscore);
ST_FUNC void put_extern_sym(TCCState *s1, Sym *sym, Section *section, addr_t value, unsigned long size);
#if PTR_SIZE == 4
ST_FUNC void greloc(TCCState *s1, Section *s, Sym *sym, unsigned long offset, int type);
#endif
ST_FUNC void greloca(TCCState *s1, Section *s, Sym *sym, unsigned long offset, int type, addr_t addend);

ST_FUNC int put_elf_str(TCCState *s1, Section *s, const char *sym);
ST_FUNC int put_elf_sym(TCCState *s1, Section *s, addr_t value, unsigned long size, int info, int other, int shndx, const char *name);
ST_FUNC int set_elf_sym(TCCState *s1, Section *s, addr_t value, unsigned long size, int info, int other, int shndx, const char *name);
ST_FUNC int find_elf_sym(Section *s, const char *name);
ST_FUNC void put_elf_reloc(TCCState *s1, Section *symtab, Section *s, unsigned long offset, int type, int symbol);
ST_FUNC void put_elf_reloca(TCCState *s1, Section *symtab, Section *s, unsigned long offset, int type, int symbol, addr_t addend);

ST_FUNC void put_stabs(TCCState *s1, const char *str, int type, int other, int desc, unsigned long value);
ST_FUNC void put_stabs_r(TCCState *s1, const char *str, int type, int other, int desc, unsigned long value, Section *sec, int sym_index);
ST_FUNC void put_stabn(TCCState *s1, int type, int other, int desc, int value);
ST_FUNC void put_stabd(TCCState *s1, int type, int other, int desc);

ST_FUNC void resolve_common_syms(TCCState *s1);
ST_FUNC void relocate_syms(TCCState *s1, Section *symtab, int do_resolve);
ST_FUNC void relocate_section(TCCState *s1, Section *s);

ST_FUNC int tcc_object_type(int fd, ElfW(Ehdr) *h);
ST_FUNC int tcc_load_object_file(TCCState *s1, int fd, unsigned long file_offset);
ST_FUNC int tcc_load_archive(TCCState *s1, int fd);
ST_FUNC void tcc_add_bcheck(TCCState *s1);
ST_FUNC void tcc_add_runtime(TCCState *s1);

ST_FUNC void build_got_entries(TCCState *s1);
ST_FUNC struct sym_attr *get_sym_attr(TCCState *s1, int index, int alloc);
ST_FUNC void squeeze_multi_relocs(Section *sec, size_t oldrelocoffset);

ST_FUNC addr_t get_elf_sym_addr(TCCState *s, const char *name, int err);
#if defined TCC_IS_NATIVE || defined TCC_TARGET_PE
ST_FUNC void *tcc_get_symbol_err(TCCState *s, const char *name);
#endif

#ifndef TCC_TARGET_PE
ST_FUNC int tcc_load_dll(TCCState *s1, int fd, const char *filename, int level);
ST_FUNC int tcc_load_ldscript(TCCState *s1);
ST_FUNC uint8_t *parse_comment(TCCState *s1, uint8_t *p);
ST_FUNC void minp(TCCState *s1);
ST_INLN void inp(TCCState *s1);
ST_FUNC int handle_eob(TCCState *s1);
#endif

/* ------------ xxx-link.c ------------ */

/* Whether to generate a GOT/PLT entry and when. NO_GOTPLT_ENTRY is first so
   that unknown relocation don't create a GOT or PLT entry */
enum gotplt_entry {
    NO_GOTPLT_ENTRY,	/* never generate (eg. GLOB_DAT & JMP_SLOT relocs) */
    BUILD_GOT_ONLY,	/* only build GOT (eg. TPOFF relocs) */
    AUTO_GOTPLT_ENTRY,	/* generate if sym is UNDEF */
    ALWAYS_GOTPLT_ENTRY	/* always generate (eg. PLTOFF relocs) */
};

ST_FUNC int code_reloc (TCCState *s1, int reloc_type);
ST_FUNC int gotplt_entry_type (TCCState *s1, int reloc_type);
ST_FUNC unsigned create_plt_entry(TCCState *s1, unsigned got_offset, struct sym_attr *attr);
ST_FUNC void relocate_init(TCCState *s1, Section *sr);
ST_FUNC void relocate(TCCState *s1, ElfW_Rel *rel, int type, unsigned char *ptr, addr_t addr, addr_t val);
ST_FUNC void relocate_plt(TCCState *s1);

/* ------------ xxx-gen.c ------------ */

ST_DATA const int reg_classes[NB_REGS];

ST_FUNC void gsym_addr(TCCState *s1, int t, int a);
ST_FUNC void gsym(TCCState *s1, int t);
ST_FUNC void load(TCCState *s1, int r, SValue *sv);
ST_FUNC void store(TCCState *s1, int r, SValue *v);
ST_FUNC int gfunc_sret(CType *vt, int variadic, CType *ret, int *align, int *regsize);
ST_FUNC void gfunc_call(TCCState *s1, int nb_args);
ST_FUNC void gfunc_prolog(TCCState *s1, CType *func_type);
ST_FUNC void gfunc_epilog(TCCState *s1);
ST_FUNC int gjmp(TCCState *s1, int t);
ST_FUNC void gjmp_addr(TCCState *s1, int a);
ST_FUNC int gtst(TCCState *s1, int inv, int t);
#if defined TCC_TARGET_I386 || defined TCC_TARGET_X86_64
ST_FUNC void gtst_addr(TCCState *s1, int inv, int a);
#else
#define gtst_addr(s1, inv, a) gsym_addr(s1, gtst(s1, inv, 0), a)
#endif
ST_FUNC void gen_opi(TCCState *s1, int op);
ST_FUNC void gen_opf(TCCState *s1, int op);
ST_FUNC void gen_cvt_ftoi(TCCState *s1, int t);
ST_FUNC void gen_cvt_ftof(TCCState *s1, int t);
ST_FUNC void ggoto(TCCState *s1);
#ifndef TCC_TARGET_C67
ST_FUNC void o(TCCState *s1, unsigned int c);
#endif
#ifndef TCC_TARGET_ARM
ST_FUNC void gen_cvt_itof(TCCState *s1, int t);
#endif
ST_FUNC void gen_vla_sp_save(TCCState *s1, int addr);
ST_FUNC void gen_vla_sp_restore(TCCState *s1, int addr);
ST_FUNC void gen_vla_alloc(TCCState *s1, CType *type, int align);

static inline uint16_t read16le(unsigned char *p) {
    return p[0] | (uint16_t)p[1] << 8;
}
static inline void write16le(unsigned char *p, uint16_t x) {
    p[0] = (unsigned char) (x & 255); p[1] = (unsigned char) (x >> 8 & 255);
}
static inline uint32_t read32le(unsigned char *p) {
  return read16le(p) | (uint32_t)read16le(p + 2) << 16;
}
static inline void write32le(unsigned char *p, uint32_t x) {
    write16le(p, (uint16_t) x);  write16le(p + 2, (uint16_t) (x >> 16));
}
static inline void add32le(unsigned char *p, int32_t x) {
    write32le(p, read32le(p) + x);
}
static inline uint64_t read64le(unsigned char *p) {
  return read32le(p) | (uint64_t)read32le(p + 4) << 32;
}
static inline void write64le(unsigned char *p, uint64_t x) {
    write32le(p, (uint32_t) x);  write32le(p + 4, (uint32_t) (x >> 32));
}
static inline void add64le(unsigned char *p, int64_t x) {
    write64le(p, read64le(p) + x);
}

/* ------------ i386-gen.c ------------ */
#if defined TCC_TARGET_I386 || defined TCC_TARGET_X86_64
ST_FUNC void g(TCCState *s1, int c);
ST_FUNC void gen_le16(TCCState *s1, int c);
ST_FUNC void gen_le32(TCCState *s1, int c);
ST_FUNC void gen_addr32(TCCState *s1, int r, Sym *sym, int c);
ST_FUNC void gen_addrpc32(TCCState *s1, int r, Sym *sym, int c);
#endif

#ifdef CONFIG_TCC_BCHECK
ST_FUNC void gen_bounded_ptr_add(TCCState *s1);
ST_FUNC void gen_bounded_ptr_deref(TCCState *s1);
#endif

/* ------------ x86_64-gen.c ------------ */
#ifdef TCC_TARGET_X86_64
ST_FUNC void gen_addr64(TCCState *s1, int r, Sym *sym, int64_t c);
ST_FUNC void gen_opl(TCCState *s1, int op);
#ifdef TCC_TARGET_PE
ST_FUNC void gen_vla_result(TCCState *s1, int addr);
#endif
#endif

/* ------------ arm-gen.c ------------ */
#ifdef TCC_TARGET_ARM
#if defined(TCC_ARM_EABI) && !defined(CONFIG_TCC_ELFINTERP)
PUB_FUNC const char *default_elfinterp(struct TCCState *s);
#endif
ST_FUNC void arm_init(struct TCCState *s);
ST_FUNC void gen_cvt_itof1(TCCState *s1, int t);
#endif

/* ------------ arm64-gen.c ------------ */
#ifdef TCC_TARGET_ARM64
ST_FUNC void gen_cvt_sxtw(TCCState *s1);
ST_FUNC void gen_opl(TCCState *s1, int op);
ST_FUNC void gfunc_return(TCCState *s1, CType *func_type);
ST_FUNC void gen_va_start(TCCState *s1);
ST_FUNC void gen_va_arg(TCCState *s1, CType *t);
ST_FUNC void gen_clear_cache(TCCState *s1);
#endif

/* ------------ c67-gen.c ------------ */
#ifdef TCC_TARGET_C67
#endif

/* ------------ tcccoff.c ------------ */

#ifdef TCC_TARGET_COFF
ST_FUNC int tcc_output_coff(TCCState *s1, FILE *f);
ST_FUNC int tcc_load_coff(TCCState * s1, int fd);
#endif

/* ------------ tccasm.c ------------ */
ST_FUNC void asm_instr(TCCState *s1);
ST_FUNC void asm_global_instr(TCCState *s1);
#ifdef CONFIG_TCC_ASM
ST_FUNC int find_constraint(TCCState *s1, ASMOperand *operands, int nb_operands, const char *name, const char **pp);
ST_FUNC Sym* get_asm_sym(TCCState *s1, int name, Sym *csym);
ST_FUNC void asm_expr(TCCState *s1, ExprValue *pe);
ST_FUNC int asm_int_expr(TCCState *s1);
ST_FUNC int tcc_assemble(TCCState *s1, int do_preprocess);
/* ------------ i386-asm.c ------------ */
ST_FUNC void gen_expr32(TCCState *s1, ExprValue *pe);
#ifdef TCC_TARGET_X86_64
ST_FUNC void gen_expr64(TCCState *s1, ExprValue *pe);
#endif
ST_FUNC void asm_opcode(TCCState *s1, int opcode);
ST_FUNC int asm_parse_regvar(TCCState *s1, int t);
ST_FUNC void asm_compute_constraints(TCCState *s1, ASMOperand *operands, int nb_operands, int nb_outputs, const uint8_t *clobber_regs, int *pout_reg);
ST_FUNC void subst_asm_operand(TCCState *s1, CString *add_str, SValue *sv, int modifier);
ST_FUNC void asm_gen_code(TCCState *s1, ASMOperand *operands, int nb_operands, int nb_outputs, int is_output, uint8_t *clobber_regs, int out_reg);
ST_FUNC void asm_clobber(TCCState *s1, uint8_t *clobber_regs, const char *str);
#endif

/* ------------ tccpe.c -------------- */
#ifdef TCC_TARGET_PE
ST_FUNC int pe_load_file(struct TCCState *s1, const char *filename, int fd);
ST_FUNC int pe_output_file(TCCState * s1, const char *filename);
ST_FUNC int pe_putimport(TCCState *s1, int dllindex, const char *name, addr_t value);
#if defined TCC_TARGET_I386 || defined TCC_TARGET_X86_64
ST_FUNC SValue *pe_getimport(TCCState *s1, SValue *sv, SValue *v2);
#endif
#ifdef TCC_TARGET_X86_64
ST_FUNC void pe_add_unwind_data(TCCState *s1, unsigned start, unsigned end, unsigned stack);
#endif
PUB_FUNC int tcc_get_dllexports(TCCState *s1, const char *filename, char **pp);
/* symbol properties stored in Elf32_Sym->st_other */
# define ST_PE_EXPORT 0x10
# define ST_PE_IMPORT 0x20
# define ST_PE_STDCALL 0x40
#endif
#define ST_ASM_SET 0x04

/* ------------ tccrun.c ----------------- */
#ifdef TCC_IS_NATIVE
#ifdef CONFIG_TCC_STATIC
#define RTLD_LAZY       0x001
#define RTLD_NOW        0x002
#define RTLD_GLOBAL     0x100
#define RTLD_DEFAULT    NULL
/* dummy function for profiling */
ST_FUNC void *dlopen(const char *filename, int flag);
ST_FUNC void dlclose(void *p);
ST_FUNC const char *dlerror(void);
ST_FUNC void *dlsym(void *handle, const char *symbol);
#endif
#ifdef CONFIG_TCC_BACKTRACE
ST_FUNC void tcc_set_num_callers(TCCState *s1, int n);
#endif
ST_FUNC void tcc_run_free(TCCState *s1);
#endif

/* ------------ tcctools.c ----------------- */
#if 0 /* included in tcc.c */
ST_FUNC int tcc_tool_ar(TCCState *s, int argc, char **argv);
#ifdef TCC_TARGET_PE
ST_FUNC int tcc_tool_impdef(TCCState *s, int argc, char **argv);
#endif
ST_FUNC void tcc_tool_cross(TCCState *s, char **argv, int option);
ST_FUNC void gen_makedeps(TCCState *s, const char *target, const char *filename);
#endif

/********************************************************/
#undef ST_DATA
#if ONE_SOURCE
#define ST_DATA static
#else
#define ST_DATA
#endif
/********************************************************/
#endif /* _TCC_H */
