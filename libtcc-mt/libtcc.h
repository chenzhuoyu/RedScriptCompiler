#ifndef LIBTCC_H
#define LIBTCC_H

#ifndef LIBTCCAPI
# define LIBTCCAPI
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct TCCType;
struct TCCState;
struct TCCFunction;

typedef struct TCCType TCCType;
typedef struct TCCState TCCState;
typedef struct TCCFunction TCCFunction;

/* create a new TCC compilation context */
LIBTCCAPI TCCState *tcc_new(void);

/* free a TCC compilation context */
LIBTCCAPI void tcc_delete(TCCState *s);

/* set CONFIG_TCCDIR at runtime */
LIBTCCAPI void tcc_set_lib_path(TCCState *s, const char *path);

/* set error/warning display callback */
LIBTCCAPI void tcc_set_error_func(TCCState *s, void *error_opaque,
    void (*error_func)(void *opaque, const char *msg));

/* set options as from command line (multiple supported) */
LIBTCCAPI void tcc_set_options(TCCState *s, const char *str);

/*****************************/
/* preprocessor */

/* add include path */
LIBTCCAPI int tcc_add_include_path(TCCState *s, const char *pathname);

/* add in system include path */
LIBTCCAPI int tcc_add_sysinclude_path(TCCState *s, const char *pathname);

/* define preprocessor symbol 'sym'. Can put optional value */
LIBTCCAPI void tcc_define_symbol(TCCState *s, const char *sym, const char *value);

/* undefine preprocess symbol 'sym' */
LIBTCCAPI void tcc_undefine_symbol(TCCState *s, const char *sym);

/*****************************/
/* compiling */

/* add a file (C file, dll, object, library, ld script). Return -1 if error. */
LIBTCCAPI int tcc_add_file(TCCState *s, const char *filename);

/* compile a string containing a C source. Return -1 if error. */
LIBTCCAPI int tcc_compile_string(TCCState *s, const char *buf);

/*****************************/
/* linking commands */

/* set output type. MUST BE CALLED before any compilation */
LIBTCCAPI int tcc_set_output_type(TCCState *s, int output_type);
#define TCC_OUTPUT_MEMORY   1 /* output will be run in memory (default) */
#define TCC_OUTPUT_EXE      2 /* executable file */
#define TCC_OUTPUT_DLL      3 /* dynamic library */
#define TCC_OUTPUT_OBJ      4 /* object file */
#define TCC_OUTPUT_PREPROCESS 5 /* only preprocess (used internally) */

/* equivalent to -Lpath option */
LIBTCCAPI int tcc_add_library_path(TCCState *s, const char *pathname);

/* the library name is the same as the argument of the '-l' option */
LIBTCCAPI int tcc_add_library(TCCState *s, const char *libraryname);

/* add a symbol to the compiled program */
LIBTCCAPI int tcc_add_symbol(TCCState *s, const char *name, const void *val);

/* output an executable, library or object file. DO NOT call
   tcc_relocate() before. */
LIBTCCAPI int tcc_output_file(TCCState *s, const char *filename);

/* link and run main() function and return its value. DO NOT call
   tcc_relocate() before. */
LIBTCCAPI int tcc_run(TCCState *s, int argc, char **argv);

/* do all relocations (needed before using tcc_get_symbol() or tcc_function_get_addr()) */
LIBTCCAPI int tcc_relocate(TCCState *s);
LIBTCCAPI int tcc_relocate_ex(TCCState *s, void *code_seg, void *data_seg, size_t *cs_size, size_t *ds_size);
/* possible values for 'code_seg' and 'data_seg':
   - NULL              : return required memory size for the step below
   - memory address    : copy code and data to memory passed by the caller
   returns -1 if error. */

/* return symbol value or NULL if not found */
LIBTCCAPI void *tcc_get_symbol(TCCState *s, const char *name);

/* return function info or NULL if not found */
LIBTCCAPI TCCFunction *tcc_find_function(TCCState *s, const char *name);

/* list all functions, return `false` in `tcc_function_enum_t` to stop enumeration */
typedef char (*tcc_function_enum_t)(TCCState *s, const char *name, TCCFunction *f, void *opaque);
LIBTCCAPI size_t tcc_list_functions(TCCState *s, tcc_function_enum_t enum_cb, void *opaque);

/* return function address or NULL if not found */
LIBTCCAPI void *tcc_function_get_addr(TCCState *s, TCCFunction *f);

/* function inspection */
LIBTCCAPI TCCType *tcc_function_get_return_type(TCCFunction *f);
LIBTCCAPI const char *tcc_function_get_name(TCCFunction *f);
LIBTCCAPI size_t tcc_function_get_nargs(TCCFunction *f);
LIBTCCAPI TCCType *tcc_function_get_arg_type(TCCFunction *f, size_t index);
LIBTCCAPI const char *tcc_function_get_arg_name(TCCFunction *f, size_t index);

/* return type info or NULL if not found */
LIBTCCAPI TCCType *tcc_find_type(TCCState *s, const char *name);

/* list all types, return `false` in `tcc_type_enum_t` to stop enumeration */
typedef char (*tcc_type_enum_t)(TCCState *s, const char *name, TCCType *t, void *opaque);
LIBTCCAPI size_t tcc_list_types(TCCState *s, tcc_type_enum_t enum_cb, void *opaque);

/* type inspection */
LIBTCCAPI int tcc_type_get_id(TCCType *t);
LIBTCCAPI TCCType *tcc_type_get_ref(TCCType *t);
LIBTCCAPI const char *tcc_type_get_name(TCCType *t);

/* return field count / enum item count */
LIBTCCAPI ssize_t tcc_type_get_nkeys(TCCType *t);

/* return field count / enum item count / function argument count */
LIBTCCAPI ssize_t tcc_type_get_nvalues(TCCType *t);

/* list function args, return `false` in `tcc_type_arg_enum_t` to stop enumeration */
typedef char (*tcc_type_arg_enum_t)(TCCState *s, TCCType *t, TCCType *type, void *opaque);
LIBTCCAPI ssize_t tcc_type_list_args(TCCState *s, TCCType *t, tcc_type_arg_enum_t enum_cb, void *opaque);

/* list enum items, return `false` in `tcc_type_item_enum_t` to stop enumeration */
typedef char (*tcc_type_item_enum_t)(TCCState *s, TCCType *t, const char *key, long long val, void *opaque);
LIBTCCAPI ssize_t tcc_type_list_items(TCCState *s, TCCType *t, tcc_type_item_enum_t enum_cb, void *opaque);

/* list struct fields, return `false` in `tcc_type_field_enum_t` to stop enumeration */
typedef char (*tcc_type_field_enum_t)(TCCState *s, TCCType *t, const char *name, TCCType *type, void *opaque);
LIBTCCAPI ssize_t tcc_type_list_fields(TCCState *s, TCCType *t, tcc_type_field_enum_t enum_cb, void *opaque);

/* The current type ID value can be: */
#define VT_VALMASK   0x003f  /* mask for value location, register or: */
#define VT_CONST     0x0030  /* constant in vc (must be first non register value) */
#define VT_LLOCAL    0x0031  /* lvalue, offset on stack */
#define VT_LOCAL     0x0032  /* offset on stack */
#define VT_CMP       0x0033  /* the value is stored in processor flags (in vc) */
#define VT_JMP       0x0034  /* value is the consequence of jmp true (even) */
#define VT_JMPI      0x0035  /* value is the consequence of jmp false (odd) */
#define VT_LVAL      0x0100  /* var is an lvalue */
#define VT_SYM       0x0200  /* a symbol value is added */
#define VT_MUSTCAST  0x0400  /* value must be casted to be correct (used for
                                char/short stored in integer registers) */
#define VT_MUSTBOUND 0x0800  /* bound checking must be done before
                                dereferencing value */
#define VT_BOUNDED   0x8000  /* value is bounded. The address of the
                                bounding function call point is in vc */

#define VT_LVAL_BYTE     0x1000  /* lvalue is a byte */
#define VT_LVAL_SHORT    0x2000  /* lvalue is a short */
#define VT_LVAL_UNSIGNED 0x4000  /* lvalue is unsigned */
#define VT_LVAL_TYPE     (VT_LVAL_BYTE | VT_LVAL_SHORT | VT_LVAL_UNSIGNED)

/* types */
#define VT_BTYPE       0x000f  /* mask for basic type */
#define VT_VOID             0  /* void type */
#define VT_BYTE             1  /* signed byte type */
#define VT_SHORT            2  /* short type */
#define VT_INT              3  /* integer type */
#define VT_LLONG            4  /* 64 bit integer */
#define VT_PTR              5  /* pointer */
#define VT_FUNC             6  /* function type */
#define VT_STRUCT           7  /* struct/union definition */
#define VT_FLOAT            8  /* IEEE float */
#define VT_DOUBLE           9  /* IEEE double */
#define VT_LDOUBLE         10  /* IEEE long double */
#define VT_BOOL            11  /* ISOC99 boolean type */
#define VT_QLONG           13  /* 128-bit integer. Only used for x86-64 ABI */
#define VT_QFLOAT          14  /* 128-bit float. Only used for x86-64 ABI */

#define VT_UNSIGNED    0x0010  /* unsigned type */
#define VT_DEFSIGN     0x0020  /* explicitly signed or unsigned */
#define VT_ARRAY       0x0040  /* array type (also has VT_PTR) */
#define VT_BITFIELD    0x0080  /* bitfield modifier */
#define VT_CONSTANT    0x0100  /* const modifier */
#define VT_VOLATILE    0x0200  /* volatile modifier */
#define VT_VLA         0x0400  /* VLA type (also has VT_PTR and VT_ARRAY) */
#define VT_LONG        0x0800  /* long type (also has VT_INT rsp. VT_LLONG) */

/* storage */
#define VT_EXTERN  0x00001000  /* extern definition */
#define VT_STATIC  0x00002000  /* static variable */
#define VT_TYPEDEF 0x00004000  /* typedef definition */
#define VT_INLINE  0x00008000  /* inline definition */
#define VT_FORWARD 0x00010000  /* forward declaration, Only used for TCCType::t */
/* currently unused: 0x000[124]00000  */

#define VT_STRUCT_SHIFT 20     /* shift for bitfield shift values (32 - 2*6) */
#define VT_STRUCT_MASK (((1 << (6+6)) - 1) << VT_STRUCT_SHIFT | VT_BITFIELD)
#define BIT_POS(t) (((t) >> VT_STRUCT_SHIFT) & 0x3f)
#define BIT_SIZE(t) (((t) >> (VT_STRUCT_SHIFT + 6)) & 0x3f)

#define VT_UNION    (1 << VT_STRUCT_SHIFT | VT_STRUCT)
#define VT_ENUM     (2 << VT_STRUCT_SHIFT) /* integral type is an enum really */
#define VT_ENUM_VAL (3 << VT_STRUCT_SHIFT) /* integral type is an enum constant really */

#define IS_ENUM(t) ((t & VT_STRUCT_MASK) == VT_ENUM)
#define IS_ENUM_VAL(t) ((t & VT_STRUCT_MASK) == VT_ENUM_VAL)
#define IS_UNION(t) ((t & (VT_STRUCT_MASK|VT_BTYPE)) == VT_UNION)

#define IS_PTR(t) ((t & VT_BTYPE) == VT_PTR)
#define IS_FUNC(t) ((t & VT_BTYPE) == VT_FUNC)
#define IS_STRUCT(t) ((t & VT_BTYPE) == VT_STRUCT)

/* type mask (except storage) */
#define VT_STORAGE (VT_EXTERN | VT_STATIC | VT_TYPEDEF | VT_INLINE)
#define VT_TYPE (~(VT_STORAGE|VT_STRUCT_MASK))

/* symbol was created by tccasm.c first */
#define VT_ASM (VT_VOID | VT_UNSIGNED)
#define IS_ASM_SYM(sym) (((sym)->type.t & (VT_BTYPE | VT_ASM)) == VT_ASM)

#ifdef __cplusplus
}
#endif

#endif
