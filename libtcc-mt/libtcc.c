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

/********************************************************/

#if ONE_SOURCE
#include "tccpp.c"
#include "tccgen.c"
#include "tccelf.c"
#include "tccrun.c"
#ifdef TCC_TARGET_I386
#include "i386-gen.c"
#include "i386-link.c"
#include "i386-asm.c"
#endif
#ifdef TCC_TARGET_ARM
#include "arm-gen.c"
#include "arm-link.c"
#include "arm-asm.c"
#endif
#ifdef TCC_TARGET_ARM64
#include "arm64-gen.c"
#include "arm64-link.c"
#endif
#ifdef TCC_TARGET_C67
#include "c67-gen.c"
#include "c67-link.c"
#include "tcccoff.c"
#endif
#ifdef TCC_TARGET_X86_64
#include "x86_64-gen.c"
#include "x86_64-link.c"
#include "i386-asm.c"
#endif
#ifdef CONFIG_TCC_ASM
#include "tccasm.c"
#endif
#ifdef TCC_TARGET_PE
#include "tccpe.c"
#endif
#endif /* ONE_SOURCE */

/********************************************************/
#ifndef CONFIG_TCC_ASM
ST_FUNC void asm_instr(void)
{
    tcc_error("inline asm() not supported");
}
ST_FUNC void asm_global_instr(void)
{
    tcc_error("inline asm() not supported");
}
#endif

/********************************************************/
#ifdef _WIN32
ST_FUNC char *normalize_slashes(char *path)
{
    char *p;
    for (p = path; *p; ++p)
        if (*p == '\\')
            *p = '/';
    return path;
}

static HMODULE tcc_module;

/* on win32, we suppose the lib and includes are at the location of 'tcc.exe' */
static void tcc_set_lib_path_w32(TCCState *s)
{
    char path[1024], *p;
    GetModuleFileNameA(tcc_module, path, sizeof path);
    p = tcc_basename(normalize_slashes(strlwr(path)));
    if (p > path)
        --p;
    *p = 0;
    tcc_set_lib_path(s, path);
}

#ifdef TCC_TARGET_PE
static void tcc_add_systemdir(TCCState *s)
{
    char buf[1000];
    GetSystemDirectory(buf, sizeof buf);
    tcc_add_library_path(s, normalize_slashes(buf));
}
#endif

#ifdef LIBTCC_AS_DLL
BOOL WINAPI DllMain (HINSTANCE hDll, DWORD dwReason, LPVOID lpReserved)
{
    if (DLL_PROCESS_ATTACH == dwReason)
        tcc_module = hDll;
    return TRUE;
}
#endif
#endif

/********************************************************/
/* copy a string and truncate it. */
ST_FUNC char *pstrcpy(char *buf, int buf_size, const char *s)
{
    char *q, *q_end;
    int c;

    if (buf_size > 0) {
        q = buf;
        q_end = buf + buf_size - 1;
        while (q < q_end) {
            c = *s++;
            if (c == '\0')
                break;
            *q++ = c;
        }
        *q = '\0';
    }
    return buf;
}

/* strcat and truncate. */
ST_FUNC char *pstrcat(char *buf, int buf_size, const char *s)
{
    int len;
    len = strlen(buf);
    if (len < buf_size)
        pstrcpy(buf + len, buf_size - len, s);
    return buf;
}

ST_FUNC char *pstrncpy(char *out, const char *in, size_t num)
{
    memcpy(out, in, num);
    out[num] = '\0';
    return out;
}

/* extract the basename of a file */
PUB_FUNC char *tcc_basename(const char *name)
{
    char *p = strchr(name, 0);
    while (p > name && !IS_DIRSEP(p[-1]))
        --p;
    return p;
}

/* extract extension part of a file
 *
 * (if no extension, return pointer to end-of-string)
 */
PUB_FUNC char *tcc_fileextension (const char *name)
{
    char *b = tcc_basename(name);
    char *e = strrchr(b, '.');
    return e ? e : strchr(b, 0);
}

/********************************************************/
/* memory management */

#undef free
#undef malloc
#undef realloc

#ifndef MEM_DEBUG

PUB_FUNC void tcc_free(TCCState *s1, void *ptr)
{
    free(ptr);
}

PUB_FUNC void *tcc_malloc(TCCState *s1, unsigned long size)
{
    void *ptr;
    ptr = malloc(size);
    if (!ptr && size)
        tcc_error(s1, "memory full (malloc)");
    return ptr;
}

PUB_FUNC void *tcc_mallocz(TCCState *s1, unsigned long size)
{
    void *ptr;
    ptr = tcc_malloc(s1, size);
    memset(ptr, 0, size);
    return ptr;
}

PUB_FUNC void *tcc_realloc(TCCState *s1, void *ptr, unsigned long size)
{
    void *ptr1;
    ptr1 = realloc(ptr, size);
    if (!ptr1 && size)
        tcc_error(s1, "memory full (realloc)");
    return ptr1;
}

PUB_FUNC char *tcc_strdup(TCCState *s1, const char *str)
{
    char *ptr;
    ptr = tcc_malloc(s1, strlen(str) + 1);
    strcpy(ptr, str);
    return ptr;
}

#else

#define MEM_DEBUG_MAGIC1 0xFEEDDEB1
#define MEM_DEBUG_MAGIC2 0xFEEDDEB2
#define MEM_DEBUG_MAGIC3 0xFEEDDEB3
#define MEM_DEBUG_FILE_LEN 40
#define MEM_DEBUG_CHECK3(header) \
    ((mem_debug_header_t*)((char*)header + header->size))->magic3
#define MEM_USER_PTR(header) \
    ((char *)header + offsetof(mem_debug_header_t, magic3))
#define MEM_HEADER_PTR(ptr) \
    (mem_debug_header_t *)((char*)ptr - offsetof(mem_debug_header_t, magic3))

struct mem_debug_header {
    unsigned magic1;
    unsigned size;
    struct mem_debug_header *prev;
    struct mem_debug_header *next;
    int line_num;
    char file_name[MEM_DEBUG_FILE_LEN + 1];
    unsigned magic2;
    ALIGNED(16) unsigned magic3;
};

typedef struct mem_debug_header mem_debug_header_t;

static mem_debug_header_t *mem_debug_chain;
static unsigned mem_cur_size;
static unsigned mem_max_size;

static mem_debug_header_t *malloc_check(void *ptr, const char *msg)
{
    mem_debug_header_t * header = MEM_HEADER_PTR(ptr);
    if (header->magic1 != MEM_DEBUG_MAGIC1 ||
        header->magic2 != MEM_DEBUG_MAGIC2 ||
        MEM_DEBUG_CHECK3(header) != MEM_DEBUG_MAGIC3 ||
        header->size == (unsigned)-1) {
        fprintf(stderr, "%s check failed\n", msg);
        if (header->magic1 == MEM_DEBUG_MAGIC1)
            fprintf(stderr, "%s:%u: block allocated here.\n",
                header->file_name, header->line_num);
        exit(1);
    }
    return header;
}

PUB_FUNC void *tcc_malloc_debug(TCCState *s1, unsigned long size, const char *file, int line)
{
    int ofs;
    mem_debug_header_t *header;

    header = malloc(sizeof(mem_debug_header_t) + size);
    if (!header)
        tcc_error(s1, "memory full (malloc)");

    header->magic1 = MEM_DEBUG_MAGIC1;
    header->magic2 = MEM_DEBUG_MAGIC2;
    header->size = size;
    MEM_DEBUG_CHECK3(header) = MEM_DEBUG_MAGIC3;
    header->line_num = line;
    ofs = strlen(file) - MEM_DEBUG_FILE_LEN;
    strncpy(header->file_name, file + (ofs > 0 ? ofs : 0), MEM_DEBUG_FILE_LEN);
    header->file_name[MEM_DEBUG_FILE_LEN] = 0;

    header->next = mem_debug_chain;
    header->prev = NULL;
    if (header->next)
        header->next->prev = header;
    mem_debug_chain = header;

    mem_cur_size += size;
    if (mem_cur_size > mem_max_size)
        mem_max_size = mem_cur_size;

    return MEM_USER_PTR(header);
}

PUB_FUNC void tcc_free_debug(TCCState *s1, void *ptr)
{
    mem_debug_header_t *header;
    if (!ptr)
        return;
    header = malloc_check(ptr, "tcc_free");
    mem_cur_size -= header->size;
    header->size = (unsigned)-1;
    if (header->next)
        header->next->prev = header->prev;
    if (header->prev)
        header->prev->next = header->next;
    if (header == mem_debug_chain)
        mem_debug_chain = header->next;
    free(header);
}

PUB_FUNC void *tcc_mallocz_debug(TCCState *s1, unsigned long size, const char *file, int line)
{
    void *ptr;
    ptr = tcc_malloc_debug(s1, size,file,line);
    memset(ptr, 0, size);
    return ptr;
}

PUB_FUNC void *tcc_realloc_debug(TCCState *s1, void *ptr, unsigned long size, const char *file, int line)
{
    mem_debug_header_t *header;
    int mem_debug_chain_update = 0;
    if (!ptr)
        return tcc_malloc_debug(s1, size, file, line);
    header = malloc_check(ptr, "tcc_realloc");
    mem_cur_size -= header->size;
    mem_debug_chain_update = (header == mem_debug_chain);
    header = realloc(header, sizeof(mem_debug_header_t) + size);
    if (!header)
        tcc_error(s1, "memory full (realloc)");
    header->size = size;
    MEM_DEBUG_CHECK3(header) = MEM_DEBUG_MAGIC3;
    if (header->next)
        header->next->prev = header;
    if (header->prev)
        header->prev->next = header;
    if (mem_debug_chain_update)
        mem_debug_chain = header;
    mem_cur_size += size;
    if (mem_cur_size > mem_max_size)
        mem_max_size = mem_cur_size;
    return MEM_USER_PTR(header);
}

PUB_FUNC char *tcc_strdup_debug(TCCState *s1, const char *str, const char *file, int line)
{
    char *ptr;
    ptr = tcc_malloc_debug(s1, strlen(str) + 1, file, line);
    strcpy(ptr, str);
    return ptr;
}

PUB_FUNC void tcc_memcheck(void)
{
    if (mem_cur_size) {
        mem_debug_header_t *header = mem_debug_chain;
        fprintf(stderr, "MEM_DEBUG: mem_leak= %d bytes, mem_max_size= %d bytes\n",
            mem_cur_size, mem_max_size);
        while (header) {
            fprintf(stderr, "%s:%u: error: %u bytes leaked\n",
                header->file_name, header->line_num, header->size);
            header = header->next;
        }
#if MEM_DEBUG-0 == 2
        exit(2);
#endif
    }
}
#endif /* MEM_DEBUG */

/********************************************************/
/* dynarrays */

ST_FUNC void dynarray_add(TCCState *s1, void *ptab, int *nb_ptr, void *data)
{
    int nb, nb_alloc;
    void **pp;

    nb = *nb_ptr;
    pp = *(void ***)ptab;
    /* every power of two we double array size */
    if ((nb & (nb - 1)) == 0) {
        if (!nb)
            nb_alloc = 1;
        else
            nb_alloc = nb * 2;
        pp = tcc_realloc(s1, pp, nb_alloc * sizeof(void *));
        *(void***)ptab = pp;
    }
    pp[nb++] = data;
    *nb_ptr = nb;
}

ST_FUNC void dynarray_reset(TCCState *s1, void *pp, int *n)
{
    void **p;
    for (p = *(void***)pp; *n; ++p, --*n)
        if (*p)
            tcc_free(s1, *p);
    tcc_free(s1, *(void**)pp);
    *(void**)pp = NULL;
}

static int djb_hash(const char *s)
{
    int x = 5381;
	while (*s) x = (x << 5) + x + (*s++);
	return x;
}

static size_t hmp_linear(TCCState *s1,
                         const char *key,
                         size_t p,
                         size_t size)
{
    p += 1;
    p %= size;
    return p;
}

static struct hashitem_t *hm_rehash(TCCState *s1,
                                    hashprobe_t prob_func,
                                    struct hashitem_t *list,
                                    size_t size)
{
    size_t cap = size * sizeof(struct hashitem_t);
    struct hashitem_t *node = list->next;
    struct hashitem_t *bucket = tcc_mallocz(s1, cap);

    list->prev = list;
    list->next = list;

    while (node != list) {
        size_t p = node->hash % size;

        while (bucket[p].use)
            p = prob_func(s1, node->key, p, size);

        bucket[p].key = node->key;
        bucket[p].dtor = node->dtor;
        bucket[p].hash = node->hash;
        bucket[p].value = node->value;

        bucket[p].use = 1;
        bucket[p].next = list;
        bucket[p].prev = list->prev;

        list->prev->next = &(bucket[p]);
        list->prev = &(bucket[p]);
        node = node->next;
    }

    return bucket;
}

ST_FUNC void hashmap_new(TCCState *s1, struct hashmap_t *map, hashprobe_t prob_func)
{
    if (!prob_func)
        prob_func = hmp_linear;

    map->count = 0;
    map->prob_func = prob_func;
    map->bucket_size = HASHMAP_INIT;

    map->bucket = tcc_mallocz(s1, map->bucket_size * sizeof(struct hashitem_t));
    map->list.prev = &map->list;
    map->list.next = &map->list;
}

ST_FUNC void hashmap_free(TCCState *s1, struct hashmap_t *map)
{
    if (!map->bucket)
        return;

    struct hashitem_t *node = map->list.next;
    while (node != &map->list) {
        if (node->use && node->dtor) {
            tcc_free(s1, node->key);
            node->dtor(s1, node->value);
        }
        node = node->next;
    }

    tcc_free(s1, map->bucket);
    map->bucket = NULL;
    map->list.prev = &map->list;
    map->list.next = &map->list;
}

ST_FUNC void hashmap_insert(TCCState *s1, struct hashmap_t *map, const char *key, void *value, hashdtor_t dtor)
{
    int hash = djb_hash(key);
    size_t pos = hash % map->bucket_size;
    struct hashitem_t *bucket = map->bucket;

    while (bucket[pos].use) {
        if ((bucket[pos].hash == hash) && !strcmp(bucket[pos].key, key)) {
            bucket[pos].dtor(s1, bucket[pos].value);
            bucket[pos].dtor = dtor;
            bucket[pos].value = value;
            return;
        }
        pos = map->prob_func(s1, key, pos, map->bucket_size);
    }

    bucket[pos].use = 1;
    bucket[pos].key = tcc_strdup(s1, key);
    bucket[pos].dtor = dtor;
    bucket[pos].hash = hash;
    bucket[pos].value = value;

    bucket[pos].use = 1;
    bucket[pos].next = &map->list;
    bucket[pos].prev = map->list.prev;

    map->list.prev->next = &(bucket[pos]);
    map->list.prev = &(bucket[pos]);

    if (++map->count >= map->bucket_size * HASHMAP_LOAD_FAC) {
        struct hashitem_t *b = map->bucket;
        struct hashitem_t *nb = hm_rehash(s1, map->prob_func, &map->list, map->bucket_size * 2);

        map->bucket = nb;
        map->bucket_size *= 2;
        tcc_free(s1, b);
    }
}

ST_FUNC void **hashmap_lookup(TCCState *s1, struct hashmap_t *map, const char *key)
{
    int hash = djb_hash(key);
    size_t pos = hash % map->bucket_size;
    struct hashitem_t *bucket = map->bucket;

    while (bucket[pos].use) {
        if ((bucket[pos].hash == hash) && !strcmp(bucket[pos].key, key))
            return &(bucket[pos].value);
        pos = map->prob_func(s1, key, pos, map->bucket_size);
    }

    return NULL;
}

static void tcc_split_path(TCCState *s, void *p_ary, int *p_nb_ary, const char *in)
{
    const char *p;
    do {
        int c;
        CString str;

        cstr_new(s, &str);
        for (p = in; c = *p, c != '\0' && c != PATHSEP[0]; ++p) {
            if (c == '{' && p[1] && p[2] == '}') {
                c = p[1], p += 2;
                if (c == 'B')
                    cstr_cat(s, &str, s->tcc_lib_path, -1);
            } else {
                cstr_ccat(s, &str, c);
            }
        }
        if (str.size) {
            cstr_ccat(s, &str, '\0');
            dynarray_add(s, p_ary, p_nb_ary, tcc_strdup(s, str.data));
        }
        cstr_free(s, &str);
        in = p+1;
    } while (*p);
}

/********************************************************/

static void strcat_vprintf(char *buf, int buf_size, const char *fmt, va_list ap)
{
    size_t len;
    len = strlen(buf);
    vsnprintf(buf + len, buf_size - len, fmt, ap);
}

static void strcat_printf(char *buf, int buf_size, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    strcat_vprintf(buf, buf_size, fmt, ap);
    va_end(ap);
}

static void error1(TCCState *s1, int is_warning, const char *fmt, va_list ap)
{
    int line;
    const char *fn;
    char buf[2048];
    BufferedFile **pf, *f;

    buf[0] = '\0';
    /* use upper file if inline ":asm:" or token ":paste:" */
    for (f = s1->file; f && f->filename[0] == ':'; f = f->prev);
    if (!s1->error_func) {
        if (f) {
            for(pf = s1->include_stack; pf < s1->include_stack_ptr; pf++)
                strcat_printf(buf, sizeof(buf), "In file included from %s:%d:\n",
                    (*pf)->filename, (*pf)->line_num);
            if (s1->error_set_jmp_enabled) {
                strcat_printf(buf, sizeof(buf), "%s:%d: ",
                    f->filename, f->line_num - ((s1->tok_flags & TOK_FLAG_BOL) != 0));
            } else {
                strcat_printf(buf, sizeof(buf), "%s: ",
                    f->filename);
            }
        } else {
            strcat_printf(buf, sizeof(buf), "tcc: ");
        }
        if (is_warning)
            strcat_printf(buf, sizeof(buf), "warning: ");
        else
            strcat_printf(buf, sizeof(buf), "error: ");
        strcat_vprintf(buf, sizeof(buf), fmt, ap);

        /* default case: stderr */
        if (s1->output_type == TCC_OUTPUT_PREPROCESS && s1->ppfp == stdout)
            /* print a newline during tcc -E */
            printf("\n"), fflush(stdout);
        fflush(stdout); /* flush -v output */
        fprintf(stderr, "%s\n", buf);
        fflush(stderr); /* print error/warning now (win32) */
    } else {
        strcat_vprintf(buf, sizeof(buf), fmt, ap);
        if (f) {
            for(pf = s1->include_stack; pf < s1->include_stack_ptr; pf++)
                strcat_printf(buf, sizeof(buf), "In file included from %s:%d:\n",
                    (*pf)->filename, (*pf)->line_num);
            if (s1->error_set_jmp_enabled) {
                line = f->line_num - ((s1->tok_flags & TOK_FLAG_BOL) != 0);
            } else {
                line = -1;
            }
            fn = f->filename;
        } else {
            line = -1;
            fn = NULL;
        }
        s1->error_func(s1, is_warning, fn, line, buf, s1->error_opaque);
    }
    if (!is_warning || s1->warn_error)
        s1->nb_errors++;
}

LIBTCCAPI void tcc_set_error_func(TCCState *s, void *error_opaque,
    void (*error_func)(TCCState *s, int is_warning, const char *file,
                       int lineno, const char *msg, void *opaque))
{
    s->error_opaque = error_opaque;
    s->error_func = error_func;
}

/* error without aborting current compilation */
PUB_FUNC void tcc_error_noabort(TCCState *s1, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    error1(s1, 0, fmt, ap);
    va_end(ap);
}

PUB_FUNC void tcc_error(TCCState *s1, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    error1(s1, 0, fmt, ap);
    va_end(ap);
    /* better than nothing: in some cases, we accept to handle errors */
    if (s1->error_set_jmp_enabled) {
        longjmp(s1->error_jmp_buf, 1);
    } else {
        /* XXX: eliminate this someday */
        abort();
    }
}

PUB_FUNC void tcc_warning(TCCState *s1, const char *fmt, ...)
{
    va_list ap;

    if (s1->warn_none)
        return;

    va_start(ap, fmt);
    error1(s1, 1, fmt, ap);
    va_end(ap);
}

/********************************************************/
/* I/O layer */

ST_FUNC void tcc_open_bf(TCCState *s1, const char *filename, int initlen)
{
    BufferedFile *bf;
    int buflen = initlen ? initlen : IO_BUF_SIZE;

    bf = tcc_mallocz(s1, sizeof(BufferedFile) + buflen);
    bf->buf_ptr = bf->buffer;
    bf->buf_end = bf->buffer + initlen;
    bf->buf_end[0] = CH_EOB; /* put eob symbol */
    pstrcpy(bf->filename, sizeof(bf->filename), filename);
    bf->true_filename = bf->filename;
    bf->line_num = 1;
    bf->ifdef_stack_ptr = s1->ifdef_stack_ptr;
    bf->fd = -1;
    bf->prev = s1->file;
    s1->file = bf;
    s1->tok_flags = TOK_FLAG_BOL | TOK_FLAG_BOF;
}

ST_FUNC void tcc_close(TCCState *s1)
{
    BufferedFile *bf = s1->file;
    if (bf->fd > 0) {
        close(bf->fd);
        s1->total_lines += bf->line_num;
    }
    if (bf->true_filename != bf->filename)
        tcc_free(s1, bf->true_filename);
    s1->file = bf->prev;
    tcc_free(s1, bf);
}

ST_FUNC int tcc_open(TCCState *s1, const char *filename)
{
    int fd;
    if (strcmp(filename, "-") == 0)
        fd = 0, filename = "<stdin>";
    else
        fd = open(filename, O_RDONLY | O_BINARY);
    if ((s1->verbose == 2 && fd >= 0) || s1->verbose == 3)
        printf("%s %*s%s\n", fd < 0 ? "nf":"->",
               (int)(s1->include_stack_ptr - s1->include_stack), "", filename);
    if (fd < 0)
        return -1;
    tcc_open_bf(s1, filename, 0);
#ifdef _WIN32
    normalize_slashes(s1->file->filename);
#endif
    s1->file->fd = fd;
    return fd;
}

/* compile the file opened in 'file'. Return non zero if errors. */
static int tcc_compile(TCCState *s1)
{
    Sym *define_start;
    int filetype, is_asm;

    define_start = s1->define_stack;
    filetype = s1->filetype;
    is_asm = filetype == AFF_TYPE_ASM || filetype == AFF_TYPE_ASMPP;

    tcc_resolver_reset(s1);
    tccelf_begin_file(s1);

    if (setjmp(s1->error_jmp_buf) == 0) {
        s1->nb_errors = 0;
        s1->error_set_jmp_enabled = 1;

        preprocess_start(s1, is_asm);
        if (s1->output_type == TCC_OUTPUT_PREPROCESS) {
            tcc_preprocess(s1);
        } else if (is_asm) {
#ifdef CONFIG_TCC_ASM
            tcc_assemble(s1, filetype == AFF_TYPE_ASMPP);
#else
            tcc_error_noabort("asm not supported");
#endif
        } else {
            tccgen_compile(s1);
        }
    }
    s1->error_set_jmp_enabled = 0;

    preprocess_end(s1);
    free_inline_functions(s1);
    /* reset define stack, but keep -D and built-ins */
    free_defines(s1, define_start);
    sym_pop(s1, &s1->global_stack, NULL, 0);
    sym_pop(s1, &s1->local_stack, NULL, 0);
    tccelf_end_file(s1);
    return s1->nb_errors != 0 ? -1 : 0;
}

LIBTCCAPI int tcc_compile_string(TCCState *s, const char *str)
{
    size_t len = strlen(str);
    return tcc_compile_string_ex(s, "<string>", str, len);
}

LIBTCCAPI int tcc_compile_string_ex(TCCState *s, const char *name, const char *str, size_t len)
{
    int ret;
    tcc_open_bf(s, name, (int)len);
    memcpy(s->file->buffer, str, len);
    ret = tcc_compile(s);
    tcc_close(s);
    return ret;
}

/* define a preprocessor symbol. A value can also be provided with the '=' operator */
LIBTCCAPI void tcc_define_symbol(TCCState *s1, const char *sym, const char *value)
{
    int len1, len2;
    /* default value */
    if (!value)
        value = "1";
    len1 = strlen(sym);
    len2 = strlen(value);

    /* init file structure */
    tcc_open_bf(s1, "<define>", len1 + len2 + 1);
    memcpy(s1->file->buffer, sym, len1);
    s1->file->buffer[len1] = ' ';
    memcpy(s1->file->buffer + len1 + 1, value, len2);

    /* parse with define parser */
    next_nomacro(s1);
    parse_define(s1);
    tcc_close(s1);
}

/* undefine a preprocessor symbol */
LIBTCCAPI void tcc_undefine_symbol(TCCState *s1, const char *sym)
{
    TokenSym *ts;
    Sym *s;
    ts = tok_alloc(s1, sym, strlen(sym));
    s = define_find(s1, ts->tok);
    /* undefine symbol by putting an invalid name */
    if (s)
        define_undef(s1, s);
}

/* cleanup all static data used during compilation */
static void tcc_cleanup(TCCState *s1)
{
    if (NULL == s1)
        return;
    while (s1->file)
        tcc_close(s1);
    tccpp_delete(s1);
    /* free sym_pools */
    dynarray_reset(s1, &s1->sym_pools, &s1->nb_sym_pools);
    /* reset symbol stack */
    s1->sym_free_first = NULL;
}

LIBTCCAPI TCCState *tcc_new(void)
{
    TCCState *s;
    s = tcc_mallocz(NULL, sizeof(TCCState));
    if (!s)
        return NULL;

    s->gnu_ext = 1;
    s->tcc_ext = 1;
    s->rt_num_callers = 6;
    s->alacarte_link = 1;
    s->nocommon = 1;
    s->warn_implicit_function_declaration = 1;
    s->ms_extensions = 1;

#ifdef CHAR_IS_UNSIGNED
    s->char_is_unsigned = 1;
#endif
#ifdef TCC_TARGET_I386
    s->seg_size = 32;
#endif
    /* enable this if you want symbols with leading underscore on windows: */
#if 0 /* def TCC_TARGET_PE */
    s->leading_underscore = 1;
#endif
#ifdef _WIN32
    tcc_set_lib_path_w32(s);
#else
    tcc_set_lib_path(s, CONFIG_TCCDIR);
#endif
    tccelf_new(s);
    tccpp_new(s);

    /* we add dummy defines for some special macros to speed up tests
       and to have working defined() */
    define_push(s, TOK___LINE__, MACRO_OBJ, NULL, NULL);
    define_push(s, TOK___FILE__, MACRO_OBJ, NULL, NULL);
    define_push(s, TOK___DATE__, MACRO_OBJ, NULL, NULL);
    define_push(s, TOK___TIME__, MACRO_OBJ, NULL, NULL);
    define_push(s, TOK___COUNTER__, MACRO_OBJ, NULL, NULL);
    {
        /* define __TINYC__ 92X  */
        char buffer[32]; int a,b,c;
        sscanf(TCC_VERSION, "%d.%d.%d", &a, &b, &c);
        sprintf(buffer, "%d", a*10000 + b*100 + c);
        tcc_define_symbol(s, "__TINYC__", buffer);
    }

    /* standard defines */
    tcc_define_symbol(s, "__STDC__", NULL);
    tcc_define_symbol(s, "__STDC_VERSION__", "199901L");
    tcc_define_symbol(s, "__STDC_HOSTED__", NULL);

    /* target defines */
#if defined(TCC_TARGET_I386)
    tcc_define_symbol(s, "__i386__", NULL);
    tcc_define_symbol(s, "__i386", NULL);
    tcc_define_symbol(s, "i386", NULL);
#elif defined(TCC_TARGET_X86_64)
    tcc_define_symbol(s, "__x86_64__", NULL);
#elif defined(TCC_TARGET_ARM)
    tcc_define_symbol(s, "__ARM_ARCH_4__", NULL);
    tcc_define_symbol(s, "__arm_elf__", NULL);
    tcc_define_symbol(s, "__arm_elf", NULL);
    tcc_define_symbol(s, "arm_elf", NULL);
    tcc_define_symbol(s, "__arm__", NULL);
    tcc_define_symbol(s, "__arm", NULL);
    tcc_define_symbol(s, "arm", NULL);
    tcc_define_symbol(s, "__APCS_32__", NULL);
    tcc_define_symbol(s, "__ARMEL__", NULL);
#if defined(TCC_ARM_EABI)
    tcc_define_symbol(s, "__ARM_EABI__", NULL);
#endif
#if defined(TCC_ARM_HARDFLOAT)
    s->float_abi = ARM_HARD_FLOAT;
    tcc_define_symbol(s, "__ARM_PCS_VFP", NULL);
#else
    s->float_abi = ARM_SOFTFP_FLOAT;
#endif
#elif defined(TCC_TARGET_ARM64)
    tcc_define_symbol(s, "__aarch64__", NULL);
#elif defined TCC_TARGET_C67
    tcc_define_symbol(s, "__C67__", NULL);
#endif

#ifdef TCC_TARGET_PE
    tcc_define_symbol(s, "_WIN32", NULL);
# ifdef TCC_TARGET_X86_64
    tcc_define_symbol(s, "_WIN64", NULL);
# endif
#else
    tcc_define_symbol(s, "__unix__", NULL);
    tcc_define_symbol(s, "__unix", NULL);
    tcc_define_symbol(s, "unix", NULL);
# if defined(__linux__)
    tcc_define_symbol(s, "__linux__", NULL);
    tcc_define_symbol(s, "__linux", NULL);
# endif
# if defined(__FreeBSD__)
    tcc_define_symbol(s, "__FreeBSD__", "__FreeBSD__");
    /* No 'Thread Storage Local' on FreeBSD with tcc */
    tcc_define_symbol(s, "__NO_TLS", NULL);
# endif
# if defined(__FreeBSD_kernel__)
    tcc_define_symbol(s, "__FreeBSD_kernel__", NULL);
# endif
# if defined(__NetBSD__)
    tcc_define_symbol(s, "__NetBSD__", "__NetBSD__");
# endif
# if defined(__OpenBSD__)
    tcc_define_symbol(s, "__OpenBSD__", "__OpenBSD__");
# endif
#endif

    /* TinyCC & gcc defines */
#if PTR_SIZE == 4
    /* 32bit systems. */
    tcc_define_symbol(s, "__SIZE_TYPE__", "unsigned int");
    tcc_define_symbol(s, "__PTRDIFF_TYPE__", "int");
    tcc_define_symbol(s, "__ILP32__", NULL);
#elif LONG_SIZE == 4
    /* 64bit Windows. */
    tcc_define_symbol(s, "__SIZE_TYPE__", "unsigned long long");
    tcc_define_symbol(s, "__PTRDIFF_TYPE__", "long long");
    tcc_define_symbol(s, "__LLP64__", NULL);
#else
    /* Other 64bit systems. */
    tcc_define_symbol(s, "__SIZE_TYPE__", "unsigned long");
    tcc_define_symbol(s, "__PTRDIFF_TYPE__", "long");
    tcc_define_symbol(s, "__LP64__", NULL);
#endif

#ifdef TCC_TARGET_PE
    tcc_define_symbol(s, "__WCHAR_TYPE__", "unsigned short");
    tcc_define_symbol(s, "__WINT_TYPE__", "unsigned short");
#else
    tcc_define_symbol(s, "__WCHAR_TYPE__", "int");
    /* wint_t is unsigned int by default, but (signed) int on BSDs
       and unsigned short on windows.  Other OSes might have still
       other conventions, sigh.  */
# if defined(__FreeBSD__) || defined (__FreeBSD_kernel__) \
  || defined(__NetBSD__) || defined(__OpenBSD__)
    tcc_define_symbol(s, "__WINT_TYPE__", "int");
#  ifdef __FreeBSD__
    /* define __GNUC__ to have some useful stuff from sys/cdefs.h
       that are unconditionally used in FreeBSDs other system headers :/ */
    tcc_define_symbol(s, "__GNUC__", "2");
    tcc_define_symbol(s, "__GNUC_MINOR__", "7");
    tcc_define_symbol(s, "__builtin_alloca", "alloca");
#  endif
# else
    tcc_define_symbol(s, "__WINT_TYPE__", "unsigned int");
    /* glibc defines */
    tcc_define_symbol(s, "__REDIRECT(name, proto, alias)",
        "name proto __asm__ (#alias)");
    tcc_define_symbol(s, "__REDIRECT_NTH(name, proto, alias)",
        "name proto __asm__ (#alias) __THROW");
# endif
# if defined(TCC_MUSL)
    tcc_define_symbol(s, "__DEFINED_va_list", "");
    tcc_define_symbol(s, "__DEFINED___isoc_va_list", "");
    tcc_define_symbol(s, "__isoc_va_list", "void *");
# endif /* TCC_MUSL */
    /* Some GCC builtins that are simple to express as macros.  */
    tcc_define_symbol(s, "__builtin_extract_return_addr(x)", "x");
#endif /* ndef TCC_TARGET_PE */
    return s;
}

LIBTCCAPI void tcc_delete(TCCState *s1)
{
    tcc_cleanup(s1);

    /* free sections */
    tccelf_delete(s1);

    /* free library paths */
    dynarray_reset(s1, &s1->library_paths, &s1->nb_library_paths);
    dynarray_reset(s1, &s1->crt_paths, &s1->nb_crt_paths);

    /* free include paths */
    dynarray_reset(s1, &s1->cached_includes, &s1->nb_cached_includes);
    dynarray_reset(s1, &s1->include_paths, &s1->nb_include_paths);
    dynarray_reset(s1, &s1->sysinclude_paths, &s1->nb_sysinclude_paths);
    dynarray_reset(s1, &s1->cmd_include_files, &s1->nb_cmd_include_files);

    tcc_free(s1, s1->tcc_lib_path);
    tcc_free(s1, s1->soname);
    tcc_free(s1, s1->rpath);
    tcc_free(s1, s1->init_symbol);
    tcc_free(s1, s1->fini_symbol);
    tcc_free(s1, s1->outfile);
    tcc_free(s1, s1->deps_outfile);
    dynarray_reset(s1, &s1->files, &s1->nb_files);
    dynarray_reset(s1, &s1->target_deps, &s1->nb_target_deps);
    dynarray_reset(s1, &s1->pragma_libs, &s1->nb_pragma_libs);
    dynarray_reset(s1, &s1->argv, &s1->argc);

#ifdef TCC_IS_NATIVE
    /* free runtime memory */
    tcc_run_free(s1);
#endif

    tcc_resolver_free(s1);
    tcc_free(s1, s1);
}

LIBTCCAPI int tcc_set_output_type(TCCState *s, int output_type)
{
    s->output_type = output_type;

    /* always elf for objects */
    if (output_type == TCC_OUTPUT_OBJ)
        s->output_format = TCC_OUTPUT_FORMAT_ELF;

    if (s->char_is_unsigned)
        tcc_define_symbol(s, "__CHAR_UNSIGNED__", NULL);

    if (!s->nostdinc) {
        /* default include paths */
        /* -isystem paths have already been handled */
        tcc_add_sysinclude_path(s, CONFIG_TCC_SYSINCLUDEPATHS);
    }

#ifdef CONFIG_TCC_BCHECK
    if (s->do_bounds_check) {
        /* if bound checking, then add corresponding sections */
        tccelf_bounds_new(s);
        /* define symbol */
        tcc_define_symbol(s, "__BOUNDS_CHECKING_ON", NULL);
    }
#endif
    if (s->do_debug) {
        /* add debug sections */
        tccelf_stab_new(s);
    }

    tcc_add_library_path(s, CONFIG_TCC_LIBPATHS);

#ifdef TCC_TARGET_PE
# ifdef _WIN32
    if (!s->nostdlib && output_type != TCC_OUTPUT_OBJ)
        tcc_add_systemdir(s);
# endif
#else
    /* paths for crt objects */
    tcc_split_path(s, &s->crt_paths, &s->nb_crt_paths, CONFIG_TCC_CRTPREFIX);
    /* add libc crt1/crti objects */
    if ((output_type == TCC_OUTPUT_EXE || output_type == TCC_OUTPUT_DLL) &&
        !s->nostdlib) {
        if (output_type != TCC_OUTPUT_DLL)
            tcc_add_crt(s, "crt1.o");
        tcc_add_crt(s, "crti.o");
    }
#endif
    return 0;
}

LIBTCCAPI int tcc_add_include_path(TCCState *s, const char *pathname)
{
    tcc_split_path(s, &s->include_paths, &s->nb_include_paths, pathname);
    return 0;
}

LIBTCCAPI int tcc_add_sysinclude_path(TCCState *s, const char *pathname)
{
    tcc_split_path(s, &s->sysinclude_paths, &s->nb_sysinclude_paths, pathname);
    return 0;
}

ST_FUNC int tcc_add_file_internal(TCCState *s1, const char *filename, int flags)
{
    int ret;

    /* open the file */
    ret = tcc_open(s1, filename);
    if (ret < 0) {
        if (flags & AFF_PRINT_ERROR)
            tcc_error_noabort(s1, "file '%s' not found", filename);
        return ret;
    }

    /* update target deps */
    dynarray_add(s1, &s1->target_deps, &s1->nb_target_deps,
            tcc_strdup(s1, filename));

    if (flags & AFF_TYPE_BIN) {
        ElfW(Ehdr) ehdr;
        int fd, obj_type;

        fd = s1->file->fd;
        obj_type = tcc_object_type(fd, &ehdr);
        lseek(fd, 0, SEEK_SET);

#ifdef TCC_TARGET_MACHO
        if (0 == obj_type && 0 == strcmp(tcc_fileextension(filename), ".dylib"))
            obj_type = AFF_BINTYPE_DYN;
#endif

        switch (obj_type) {
        case AFF_BINTYPE_REL:
            ret = tcc_load_object_file(s1, fd, 0);
            break;
#ifndef TCC_TARGET_PE
        case AFF_BINTYPE_DYN:
            if (s1->output_type == TCC_OUTPUT_MEMORY) {
                ret = 0;
#ifdef TCC_IS_NATIVE
                if (NULL == dlopen(filename, RTLD_GLOBAL | RTLD_LAZY))
                    ret = -1;
#endif
            } else {
                ret = tcc_load_dll(s1, fd, filename,
                                   (flags & AFF_REFERENCED_DLL) != 0);
            }
            break;
#endif
        case AFF_BINTYPE_AR:
            ret = tcc_load_archive(s1, fd);
            break;
#ifdef TCC_TARGET_COFF
        case AFF_BINTYPE_C67:
            ret = tcc_load_coff(s1, fd);
            break;
#endif
        default:
#ifdef TCC_TARGET_PE
            ret = pe_load_file(s1, filename, fd);
#else
            /* as GNU ld, consider it is an ld script if not recognized */
            ret = tcc_load_ldscript(s1);
#endif
            if (ret < 0)
                tcc_error_noabort(s1, "unrecognized file type");
            break;
        }
    } else {
        ret = tcc_compile(s1);
    }
    tcc_close(s1);
    return ret;
}

LIBTCCAPI int tcc_add_file(TCCState *s, const char *filename)
{
    int filetype = s->filetype;
    int flags = AFF_PRINT_ERROR;
    if (filetype == 0) {
        /* use a file extension to detect a filetype */
        const char *ext = tcc_fileextension(filename);
        if (ext[0]) {
            ext++;
            if (!strcmp(ext, "S"))
                filetype = AFF_TYPE_ASMPP;
            else if (!strcmp(ext, "s"))
                filetype = AFF_TYPE_ASM;
            else if (!PATHCMP(ext, "c") || !PATHCMP(ext, "i"))
                filetype = AFF_TYPE_C;
            else
                flags |= AFF_TYPE_BIN;
        } else {
            filetype = AFF_TYPE_C;
        }
        s->filetype = filetype;
    }
    return tcc_add_file_internal(s, filename, flags);
}

static void del_type(TCCState *s1, TCCType *type)
{
    int i;
    if (IS_ENUM(type->t)) {
        for (i = 0; i < type->nb_values; i++)
            type->values[i] = 0;
    }
    else if ((type->t & VT_FUNC) || ((type->t & VT_BTYPE) == VT_STRUCT)) {
        for (i = 0; i < type->nb_values; i++)
            type->types[i] = NULL;
    }

    tcc_free(s1, type->name);
    dynarray_reset(s1, &type->names, &type->nb_names);
    dynarray_reset(s1, &type->values, &type->nb_values);
}

static void free_type(TCCState *s1, TCCType *type)
{
    if (--type->rc == 0) {
        del_type(s1, type);
        tcc_free(s1, type);
    }
}

static void free_function(TCCState *s1, TCCFunction *func)
{
    int i;
    for (i = 0; i < func->nb_args; i++)
        func->args[i] = NULL;

    tcc_free(s1, func->name);
    dynarray_reset(s1, &func->args, &func->nb_args);
    dynarray_reset(s1, &func->arg_names, &func->nb_arg_names);
    tcc_free(s1, func);
}

static char *pstrcatresize(TCCState *s1, char *s, const char *t) {
    size_t a = strlen(s);
    size_t b = strlen(t);
    char *p = tcc_realloc(s1, s, a + b + 1);
    strcpy(p + a, t);
    p[a + b] = 0;
    return p;
}

ST_FUNC void tcc_resolver_free(TCCState *s1)
{
    hashmap_free(s1, &s1->funcs);
    hashmap_free(s1, &s1->types);
}

ST_FUNC void tcc_resolver_reset(TCCState *s1)
{
    tcc_resolver_free(s1);
    hashmap_new(s1, &s1->funcs, NULL);
    hashmap_new(s1, &s1->types, NULL);
}

ST_FUNC void tcc_resolver_ref_type(TCCState *s1, CType *type, const char *name)
{
    TCCType *ptype = tcc_resolver_add_type(s1, type);
    ptype->rc++;
    hashmap_insert(s1, &s1->types, name, ptype, (hashdtor_t)free_type);
}

ST_FUNC TCCType *tcc_resolver_add_type(TCCState *s1, CType *type)
{
    int bt = type->t & VT_BTYPE;
    TCCType *vtype = tcc_mallocz(s1, sizeof(TCCType));
    TCCType **ptype;

    static const char *type_names[] = {
        "void",
        "char",
        "short",
        "int",
        NULL,
        NULL,
        NULL,
        NULL,
        "float",
        "double",
        "long double",
        "_Bool",
        "__int128",
        "__float128",
    };

    vtype->t = type->t;
    vtype->rc = 1;

    switch (bt) {
        case VT_BYTE: {
            if (!(type->t & VT_DEFSIGN))
                vtype->name = tcc_strdup(s1, "char");
            else if (!(type->t & VT_UNSIGNED))
                vtype->name = tcc_strdup(s1, "signed char");
            else
                vtype->name = tcc_strdup(s1, "unsigned char");

            break;
        }

        case VT_SHORT: {
            if (!(type->t & VT_UNSIGNED))
                vtype->name = tcc_strdup(s1, "short");
            else
                vtype->name = tcc_strdup(s1, "unsigned short");

            break;
        }

        case VT_VOID:
        case VT_FLOAT:
        case VT_DOUBLE:
        case VT_LDOUBLE:
        case VT_BOOL:
        case VT_QLONG:
        case VT_QFLOAT: {
            vtype->name = tcc_strdup(s1, type_names[bt]);
            break;
        }

        case VT_INT:
        case VT_LLONG: {
            if (!IS_ENUM(type->t)) {
                if (!(type->t & VT_UNSIGNED)) {
                    if (type->t & VT_LONG)
                        vtype->name = tcc_strdup(s1, "long");
                    else if (bt == VT_INT)
                        vtype->name = tcc_strdup(s1, "int");
                    else
                        vtype->name = tcc_strdup(s1, "long long");
                }
                else {
                    if (type->t & VT_LONG)
                        vtype->name = tcc_strdup(s1, "unsigned long");
                    else if (bt == VT_INT)
                        vtype->name = tcc_strdup(s1, "unsigned int");
                    else
                        vtype->name = tcc_strdup(s1, "unsigned long long");
                }
            }
            else {
                Sym *sym = type->ref->next;
                vtype->name = tcc_strdup(s1, get_tok_str(s1, type->ref->v & ~SYM_STRUCT, NULL));

                if (!sym) {
                    vtype->t |= VT_FORWARD;
                    break;
                }

                while (sym) {
                    long long val = sym->enum_val;
                    const char *key = get_tok_str(s1, sym->v & ~SYM_STRUCT, NULL);

                    dynarray_add(s1, &vtype->names, &vtype->nb_names, tcc_strdup(s1, key));
                    dynarray_add(s1, &vtype->values, &vtype->nb_values, (void *)val);
                    sym = sym->next;
                }
            }

            break;
        }

        case VT_PTR: {
            vtype->ref = tcc_resolver_add_type(s1, &type->ref->type);
            vtype->name = tcc_mallocz(s1, strlen(vtype->ref->name) + 2);
            strcat(strcpy(vtype->name, vtype->ref->name), "*");
            break;
        }

        case VT_FUNC: {
            Sym *sym = type->ref->next;
            char *name = NULL;
            TCCType *pt;

            vtype->ref = tcc_resolver_add_type(s1, &type->ref->type);
            name = pstrcatresize(s1, tcc_strdup(s1, vtype->ref->name), "()(");

            while (sym) {
                pt = tcc_resolver_add_type(s1, &sym->type);
                sym = sym->next;
                name = pstrcatresize(s1, name, pt->name);
                name = pstrcatresize(s1, name, ",");
                dynarray_add(s1, &vtype->types, &vtype->nb_values, pt);
            }

            vtype->name = name;
            name[strlen(name) - 1] = ')';
            break;
        }

        case VT_STRUCT: {
            Sym *sym = type->ref->next;
            vtype->name = tcc_strdup(s1, get_tok_str(s1, type->ref->v & ~SYM_STRUCT, NULL));

            if (!(ptype = (TCCType **)hashmap_lookup(s1, &s1->types, vtype->name))) {
                vtype->t |= VT_FORWARD;
                hashmap_insert(s1, &s1->types, vtype->name, vtype, (hashdtor_t)free_type);

                if (sym)
                    vtype->t |= VT_PARTIAL;
            }
            else {
                free_type(s1, vtype);
                vtype = *ptype;

                if (!sym || (vtype->t & VT_PARTIAL) || !(vtype->t & VT_FORWARD))
                    return vtype;
            }

            while (sym) {
                int id = sym->v & ~(SYM_STRUCT | SYM_FIELD);
                const char *name = get_tok_str(s1, id, NULL);

                dynarray_add(s1, &vtype->names, &vtype->nb_names, tcc_strdup(s1, name));
                dynarray_add(s1, &vtype->types, &vtype->nb_values, tcc_resolver_add_type(s1, &sym->type));
                sym = sym->next;
            }

            if (type->ref->next) {
                vtype->t &= ~VT_FORWARD;
                vtype->t &= ~VT_PARTIAL;
            }

            return vtype;
        }

        default:
            tcc_error(s1, "unknown type");
    }

    if ((ptype = (TCCType **)hashmap_lookup(s1, &s1->types, vtype->name))) {
        free_type(s1, vtype);
        return *ptype;
    }

    hashmap_insert(s1, &s1->types, vtype->name, vtype, (hashdtor_t)free_type);
    return vtype;
}

ST_FUNC TCCFunction *tcc_resolver_add_func(TCCState *s1, const char *funcname, char is_variadic, CType *ret)
{
    TCCFunction *func = tcc_mallocz(s1, sizeof(TCCFunction));
    func->ret = tcc_resolver_add_type(s1, ret);
    func->name = tcc_strdup(s1, funcname);
    func->is_variadic = is_variadic;
    hashmap_insert(s1, &s1->funcs, funcname, func, (hashdtor_t)free_function);
    return func;
}

LIBTCCAPI TCCFunction *tcc_find_function(TCCState *s, const char *name)
{
    void **pf = hashmap_lookup(s, &s->funcs, name);
    return pf ? *(TCCFunction **)pf : NULL;
}

LIBTCCAPI size_t tcc_list_functions(TCCState *s, tcc_function_enum_t enum_cb, void *opaque)
{
    size_t n = 0;
    struct hashitem_t *node = s->funcs.list.next;

    while ((node != &s->funcs.list) &&
           enum_cb(s, node->key, node->value, opaque)) {
        n++;
        node = node->next;
    }

    return n;
}

LIBTCCAPI void *tcc_function_get_addr(TCCState *s, TCCFunction *f)
{
    if (!f->addr)
        f->addr = tcc_get_symbol(s, f->name);

#ifndef _WIN64
    if (!f->addr)
        f->addr = dlsym(RTLD_DEFAULT, f->name);
#endif

    return f->addr;
}

/* function return type */
LIBTCCAPI TCCType *tcc_function_get_return_type(TCCFunction *f)
{
    return f->ret;
}

LIBTCCAPI const char *tcc_function_get_name(TCCFunction *f)
{
    return f->name;
}

LIBTCCAPI char tcc_function_is_variadic(TCCFunction *f)
{
    return f->is_variadic;
}

LIBTCCAPI size_t tcc_function_get_nargs(TCCFunction *f)
{
    return (size_t)f->nb_args;
}

LIBTCCAPI TCCType *tcc_function_get_arg_type(TCCFunction *f, size_t index)
{
    return index < f->nb_args ? f->args[index] : NULL;
}

LIBTCCAPI const char *tcc_function_get_arg_name(TCCFunction *f, size_t index)
{
    return index < f->nb_arg_names ? f->arg_names[index] : NULL;
}

LIBTCCAPI TCCType *tcc_find_type(TCCState *s, const char *name)
{
    void **pf = hashmap_lookup(s, &s->types, name);
    return pf ? *(TCCType **)pf : NULL;
}

LIBTCCAPI size_t tcc_list_types(TCCState *s, tcc_type_enum_t enum_cb, void *opaque)
{
    size_t n = 0;
    struct hashitem_t *node = s->types.list.next;

    while ((node != &s->types.list) &&
           enum_cb(s, node->key, node->value, opaque)) {
        n++;
        node = node->next;
    }

    return n;
}

LIBTCCAPI int tcc_type_get_id(TCCType *t)
{
    return t->t;
}

LIBTCCAPI TCCType *tcc_type_get_ref(TCCType *t)
{
    return IS_PTR(t->t) || IS_FUNC(t->t) ? t->ref : NULL;
}

LIBTCCAPI const char *tcc_type_get_name(TCCType *t)
{
    return t->name;
}

LIBTCCAPI ssize_t tcc_type_get_nkeys(TCCType *t)
{
    return IS_ENUM(t->t) || IS_STRUCT(t->t) ? t->nb_names : -1;
}

LIBTCCAPI ssize_t tcc_type_get_nvalues(TCCType *t)
{
    return IS_ENUM(t->t) || IS_STRUCT(t->t) || IS_FUNC(t->t) ? t->nb_values : -1;
}

LIBTCCAPI ssize_t tcc_type_list_args(TCCState *s, TCCType *t, tcc_type_arg_enum_t enum_cb, void *opaque)
{
    if (!IS_FUNC(t->t))
        return -1;

    int i;
    for (i = 0; i < t->nb_values; i++)
        if (!enum_cb(s, t, t->types[i], opaque))
            break;

    return i;
}

LIBTCCAPI ssize_t tcc_type_list_items(TCCState *s, TCCType *t, tcc_type_item_enum_t enum_cb, void *opaque)
{
    if (!IS_ENUM(t->t) || (t->nb_names != t->nb_values))
        return -1;

    int i;
    for (i = 0; i < t->nb_names; i++)
        if (!enum_cb(s, t, t->names[i], t->values[i], opaque))
            break;

    return i;
}

LIBTCCAPI ssize_t tcc_type_list_fields(TCCState *s, TCCType *t, tcc_type_field_enum_t enum_cb, void *opaque)
{
    if (!IS_STRUCT(t->t) || (t->nb_names != t->nb_values))
        return -1;

    int i;
    for (i = 0; i < t->nb_names; i++)
        if (!enum_cb(s, t, t->names[i], t->types[i], opaque))
            break;

    return i;
}

LIBTCCAPI int tcc_add_library_path(TCCState *s, const char *pathname)
{
    tcc_split_path(s, &s->library_paths, &s->nb_library_paths, pathname);
    return 0;
}

static int tcc_add_library_internal(TCCState *s, const char *fmt,
    const char *filename, int flags, char **paths, int nb_paths)
{
    char buf[1024];
    int i;

    for(i = 0; i < nb_paths; i++) {
        snprintf(buf, sizeof(buf), fmt, paths[i], filename);
        if (tcc_add_file_internal(s, buf, flags | AFF_TYPE_BIN) == 0)
            return 0;
    }
    return -1;
}

/* find and load a dll. Return non zero if not found */
/* XXX: add '-rpath' option support ? */
ST_FUNC int tcc_add_dll(TCCState *s, const char *filename, int flags)
{
    return tcc_add_library_internal(s, "%s/%s", filename, flags,
        s->library_paths, s->nb_library_paths);
}

ST_FUNC int tcc_add_crt(TCCState *s, const char *filename)
{
    if (-1 == tcc_add_library_internal(s, "%s/%s",
        filename, 0, s->crt_paths, s->nb_crt_paths))
        tcc_error_noabort(s, "file '%s' not found", filename);
    return 0;
}

/* the library name is the same as the argument of the '-l' option */
LIBTCCAPI int tcc_add_library(TCCState *s, const char *libraryname)
{
#if defined TCC_TARGET_PE
    const char *libs[] = { "%s/%s.def", "%s/lib%s.def", "%s/%s.dll", "%s/lib%s.dll", "%s/lib%s.a", NULL };
    const char **pp = s->static_link ? libs + 4 : libs;
#elif defined TCC_TARGET_MACHO
    const char *libs[] = { "%s/lib%s.dylib", "%s/lib%s.a", NULL };
    const char **pp = s->static_link ? libs + 1 : libs;
#else
    const char *libs[] = { "%s/lib%s.so", "%s/lib%s.a", NULL };
    const char **pp = s->static_link ? libs + 1 : libs;
#endif
    while (*pp) {
        if (0 == tcc_add_library_internal(s, *pp,
            libraryname, 0, s->library_paths, s->nb_library_paths))
            return 0;
        ++pp;
    }
    return -1;
}

PUB_FUNC int tcc_add_library_err(TCCState *s, const char *libname)
{
    int ret = tcc_add_library(s, libname);
    if (ret < 0)
        tcc_error_noabort(s, "library '%s' not found", libname);
    return ret;
}

/* handle #pragma comment(lib,) */
ST_FUNC void tcc_add_pragma_libs(TCCState *s1)
{
    int i;
    for (i = 0; i < s1->nb_pragma_libs; i++)
        tcc_add_library_err(s1, s1->pragma_libs[i]);
}

LIBTCCAPI int tcc_add_symbol(TCCState *s, const char *name, const void *val)
{
#ifdef TCC_TARGET_PE
    /* On x86_64 'val' might not be reachable with a 32bit offset.
       So it is handled here as if it were in a DLL. */
    pe_putimport(s, 0, name, (uintptr_t)val);
#else
    set_elf_sym(s, s->symtab_section, (uintptr_t)val, 0,
        ELFW(ST_INFO)(STB_GLOBAL, STT_NOTYPE), 0,
        SHN_ABS, name);
#endif
    return 0;
}

LIBTCCAPI void tcc_set_lib_path(TCCState *s, const char *path)
{
    tcc_free(s, s->tcc_lib_path);
    s->tcc_lib_path = tcc_strdup(s, path);
}

#define WD_ALL    0x0001 /* warning is activated when using -Wall */
#define FD_INVERT 0x0002 /* invert value before storing */

typedef struct FlagDef {
    uint16_t offset;
    uint16_t flags;
    const char *name;
} FlagDef;

static int no_flag(const char **pp)
{
    const char *p = *pp;
    if (*p != 'n' || *++p != 'o' || *++p != '-')
        return 0;
    *pp = p + 1;
    return 1;
}

ST_FUNC int set_flag(TCCState *s, const FlagDef *flags, const char *name)
{
    int value, ret;
    const FlagDef *p;
    const char *r;

    value = 1;
    r = name;
    if (no_flag(&r))
        value = 0;

    for (ret = -1, p = flags; p->name; ++p) {
        if (ret) {
            if (strcmp(r, p->name))
                continue;
        } else {
            if (0 == (p->flags & WD_ALL))
                continue;
        }
        if (p->offset) {
            *(int*)((char *)s + p->offset) =
                p->flags & FD_INVERT ? !value : value;
            if (ret)
                return 0;
        } else {
            ret = 0;
        }
    }
    return ret;
}

static int strstart(const char *val, const char **str)
{
    const char *p, *q;
    p = *str;
    q = val;
    while (*q) {
        if (*p != *q)
            return 0;
        p++;
        q++;
    }
    *str = p;
    return 1;
}

/* Like strstart, but automatically takes into account that ld options can
 *
 * - start with double or single dash (e.g. '--soname' or '-soname')
 * - arguments can be given as separate or after '=' (e.g. '-Wl,-soname,x.so'
 *   or '-Wl,-soname=x.so')
 *
 * you provide `val` always in 'option[=]' form (no leading -)
 */
static int link_option(const char *str, const char *val, const char **ptr)
{
    const char *p, *q;
    int ret;

    /* there should be 1 or 2 dashes */
    if (*str++ != '-')
        return 0;
    if (*str == '-')
        str++;

    /* then str & val should match (potentially up to '=') */
    p = str;
    q = val;

    ret = 1;
    if (q[0] == '?') {
        ++q;
        if (no_flag(&p))
            ret = -1;
    }

    while (*q != '\0' && *q != '=') {
        if (*p != *q)
            return 0;
        p++;
        q++;
    }

    /* '=' near eos means ',' or '=' is ok */
    if (*q == '=') {
        if (*p == 0)
            *ptr = p;
        if (*p != ',' && *p != '=')
            return 0;
        p++;
    } else if (*p) {
        return 0;
    }
    *ptr = p;
    return ret;
}

static const char *skip_linker_arg(const char **str)
{
    const char *s1 = *str;
    const char *s2 = strchr(s1, ',');
    *str = s2 ? s2++ : (s2 = s1 + strlen(s1));
    return s2;
}

static void copy_linker_arg(TCCState *s1, char **pp, const char *s, int sep)
{
    const char *q = s;
    char *p = *pp;
    int l = 0;
    if (p && sep)
        p[l = strlen(p)] = sep, ++l;
    skip_linker_arg(&q);
    pstrncpy(l + (*pp = tcc_realloc(s1, p, q - s + l + 1)), s, q - s);
}

/* set linker options */
static int tcc_set_linker(TCCState *s, const char *option)
{
    while (*option) {

        const char *p = NULL;
        char *end = NULL;
        int ignoring = 0;
        int ret;

        if (link_option(option, "Bsymbolic", &p)) {
            s->symbolic = 1;
        } else if (link_option(option, "nostdlib", &p)) {
            s->nostdlib = 1;
        } else if (link_option(option, "fini=", &p)) {
            copy_linker_arg(s, &s->fini_symbol, p, 0);
            ignoring = 1;
        } else if (link_option(option, "image-base=", &p)
                || link_option(option, "Ttext=", &p)) {
            s->text_addr = strtoull(p, &end, 16);
            s->has_text_addr = 1;
        } else if (link_option(option, "init=", &p)) {
            copy_linker_arg(s, &s->init_symbol, p, 0);
            ignoring = 1;
        } else if (link_option(option, "oformat=", &p)) {
#if defined(TCC_TARGET_PE)
            if (strstart("pe-", &p)) {
#elif PTR_SIZE == 8
            if (strstart("elf64-", &p)) {
#else
            if (strstart("elf32-", &p)) {
#endif
                s->output_format = TCC_OUTPUT_FORMAT_ELF;
            } else if (!strcmp(p, "binary")) {
                s->output_format = TCC_OUTPUT_FORMAT_BINARY;
#ifdef TCC_TARGET_COFF
            } else if (!strcmp(p, "coff")) {
                s->output_format = TCC_OUTPUT_FORMAT_COFF;
#endif
            } else
                goto err;

        } else if (link_option(option, "as-needed", &p)) {
            ignoring = 1;
        } else if (link_option(option, "O", &p)) {
            ignoring = 1;
        } else if (link_option(option, "export-all-symbols", &p)) {
            s->rdynamic = 1;
        } else if (link_option(option, "rpath=", &p)) {
            copy_linker_arg(s, &s->rpath, p, ':');
        } else if (link_option(option, "enable-new-dtags", &p)) {
            s->enable_new_dtags = 1;
        } else if (link_option(option, "section-alignment=", &p)) {
            s->section_align = strtoul(p, &end, 16);
        } else if (link_option(option, "soname=", &p)) {
            copy_linker_arg(s, &s->soname, p, 0);
#ifdef TCC_TARGET_PE
        } else if (link_option(option, "large-address-aware", &p)) {
            s->pe_characteristics |= 0x20;
        } else if (link_option(option, "file-alignment=", &p)) {
            s->pe_file_align = strtoul(p, &end, 16);
        } else if (link_option(option, "stack=", &p)) {
            s->pe_stack_size = strtoul(p, &end, 10);
        } else if (link_option(option, "subsystem=", &p)) {
#if defined(TCC_TARGET_I386) || defined(TCC_TARGET_X86_64)
            if (!strcmp(p, "native")) {
                s->pe_subsystem = 1;
            } else if (!strcmp(p, "console")) {
                s->pe_subsystem = 3;
            } else if (!strcmp(p, "gui") || !strcmp(p, "windows")) {
                s->pe_subsystem = 2;
            } else if (!strcmp(p, "posix")) {
                s->pe_subsystem = 7;
            } else if (!strcmp(p, "efiapp")) {
                s->pe_subsystem = 10;
            } else if (!strcmp(p, "efiboot")) {
                s->pe_subsystem = 11;
            } else if (!strcmp(p, "efiruntime")) {
                s->pe_subsystem = 12;
            } else if (!strcmp(p, "efirom")) {
                s->pe_subsystem = 13;
#elif defined(TCC_TARGET_ARM)
            if (!strcmp(p, "wince")) {
                s->pe_subsystem = 9;
#endif
            } else
                goto err;
#endif
        } else if (ret = link_option(option, "?whole-archive", &p), ret) {
            s->alacarte_link = ret < 0;
        } else if (p) {
            return 0;
        } else {
    err:
            tcc_error(s, "unsupported linker option '%s'", option);
        }

        if (ignoring && s->warn_unsupported)
            tcc_warning(s, "unsupported linker option '%s'", option);

        option = skip_linker_arg(&p);
    }
    return 1;
}

typedef struct TCCOption {
    const char *name;
    uint16_t index;
    uint16_t flags;
} TCCOption;

enum {
    TCC_OPTION_HELP,
    TCC_OPTION_HELP2,
    TCC_OPTION_v,
    TCC_OPTION_I,
    TCC_OPTION_D,
    TCC_OPTION_U,
    TCC_OPTION_P,
    TCC_OPTION_L,
    TCC_OPTION_B,
    TCC_OPTION_l,
    TCC_OPTION_bench,
    TCC_OPTION_bt,
    TCC_OPTION_b,
    TCC_OPTION_g,
    TCC_OPTION_c,
    TCC_OPTION_dumpversion,
    TCC_OPTION_d,
    TCC_OPTION_static,
    TCC_OPTION_std,
    TCC_OPTION_shared,
    TCC_OPTION_soname,
    TCC_OPTION_o,
    TCC_OPTION_r,
    TCC_OPTION_s,
    TCC_OPTION_traditional,
    TCC_OPTION_Wl,
    TCC_OPTION_Wp,
    TCC_OPTION_W,
    TCC_OPTION_O,
    TCC_OPTION_mfloat_abi,
    TCC_OPTION_m,
    TCC_OPTION_f,
    TCC_OPTION_isystem,
    TCC_OPTION_iwithprefix,
    TCC_OPTION_include,
    TCC_OPTION_nostdinc,
    TCC_OPTION_nostdlib,
    TCC_OPTION_print_search_dirs,
    TCC_OPTION_rdynamic,
    TCC_OPTION_param,
    TCC_OPTION_pedantic,
    TCC_OPTION_pthread,
    TCC_OPTION_run,
    TCC_OPTION_w,
    TCC_OPTION_pipe,
    TCC_OPTION_E,
    TCC_OPTION_MD,
    TCC_OPTION_MF,
    TCC_OPTION_x,
    TCC_OPTION_ar,
    TCC_OPTION_impdef
};

#define TCC_OPTION_HAS_ARG 0x0001
#define TCC_OPTION_NOSEP   0x0002 /* cannot have space before option and arg */

static const TCCOption tcc_options[] = {
    { "h", TCC_OPTION_HELP, 0 },
    { "-help", TCC_OPTION_HELP, 0 },
    { "?", TCC_OPTION_HELP, 0 },
    { "hh", TCC_OPTION_HELP2, 0 },
    { "v", TCC_OPTION_v, TCC_OPTION_HAS_ARG | TCC_OPTION_NOSEP },
    { "I", TCC_OPTION_I, TCC_OPTION_HAS_ARG },
    { "D", TCC_OPTION_D, TCC_OPTION_HAS_ARG },
    { "U", TCC_OPTION_U, TCC_OPTION_HAS_ARG },
    { "P", TCC_OPTION_P, TCC_OPTION_HAS_ARG | TCC_OPTION_NOSEP },
    { "L", TCC_OPTION_L, TCC_OPTION_HAS_ARG },
    { "B", TCC_OPTION_B, TCC_OPTION_HAS_ARG },
    { "l", TCC_OPTION_l, TCC_OPTION_HAS_ARG | TCC_OPTION_NOSEP },
    { "bench", TCC_OPTION_bench, 0 },
#ifdef CONFIG_TCC_BACKTRACE
    { "bt", TCC_OPTION_bt, TCC_OPTION_HAS_ARG },
#endif
#ifdef CONFIG_TCC_BCHECK
    { "b", TCC_OPTION_b, 0 },
#endif
    { "g", TCC_OPTION_g, TCC_OPTION_HAS_ARG | TCC_OPTION_NOSEP },
    { "c", TCC_OPTION_c, 0 },
    { "dumpversion", TCC_OPTION_dumpversion, 0},
    { "d", TCC_OPTION_d, TCC_OPTION_HAS_ARG | TCC_OPTION_NOSEP },
    { "static", TCC_OPTION_static, 0 },
    { "std", TCC_OPTION_std, TCC_OPTION_HAS_ARG | TCC_OPTION_NOSEP },
    { "shared", TCC_OPTION_shared, 0 },
    { "soname", TCC_OPTION_soname, TCC_OPTION_HAS_ARG },
    { "o", TCC_OPTION_o, TCC_OPTION_HAS_ARG },
    { "-param", TCC_OPTION_param, TCC_OPTION_HAS_ARG },
    { "pedantic", TCC_OPTION_pedantic, 0},
    { "pthread", TCC_OPTION_pthread, 0},
    { "run", TCC_OPTION_run, TCC_OPTION_HAS_ARG | TCC_OPTION_NOSEP },
    { "rdynamic", TCC_OPTION_rdynamic, 0 },
    { "r", TCC_OPTION_r, 0 },
    { "s", TCC_OPTION_s, 0 },
    { "traditional", TCC_OPTION_traditional, 0 },
    { "Wl,", TCC_OPTION_Wl, TCC_OPTION_HAS_ARG | TCC_OPTION_NOSEP },
    { "Wp,", TCC_OPTION_Wp, TCC_OPTION_HAS_ARG | TCC_OPTION_NOSEP },
    { "W", TCC_OPTION_W, TCC_OPTION_HAS_ARG | TCC_OPTION_NOSEP },
    { "O", TCC_OPTION_O, TCC_OPTION_HAS_ARG | TCC_OPTION_NOSEP },
#ifdef TCC_TARGET_ARM
    { "mfloat-abi", TCC_OPTION_mfloat_abi, TCC_OPTION_HAS_ARG },
#endif
    { "m", TCC_OPTION_m, TCC_OPTION_HAS_ARG | TCC_OPTION_NOSEP },
    { "f", TCC_OPTION_f, TCC_OPTION_HAS_ARG | TCC_OPTION_NOSEP },
    { "isystem", TCC_OPTION_isystem, TCC_OPTION_HAS_ARG },
    { "include", TCC_OPTION_include, TCC_OPTION_HAS_ARG },
    { "nostdinc", TCC_OPTION_nostdinc, 0 },
    { "nostdlib", TCC_OPTION_nostdlib, 0 },
    { "print-search-dirs", TCC_OPTION_print_search_dirs, 0 },
    { "w", TCC_OPTION_w, 0 },
    { "pipe", TCC_OPTION_pipe, 0},
    { "E", TCC_OPTION_E, 0},
    { "MD", TCC_OPTION_MD, 0},
    { "MF", TCC_OPTION_MF, TCC_OPTION_HAS_ARG },
    { "x", TCC_OPTION_x, TCC_OPTION_HAS_ARG },
    { "ar", TCC_OPTION_ar, 0},
#ifdef TCC_TARGET_PE
    { "impdef", TCC_OPTION_impdef, 0},
#endif
    { NULL, 0, 0 },
};

static const FlagDef options_W[] = {
    { 0, 0, "all" },
    { offsetof(TCCState, warn_unsupported), 0, "unsupported" },
    { offsetof(TCCState, warn_write_strings), 0, "write-strings" },
    { offsetof(TCCState, warn_error), 0, "error" },
    { offsetof(TCCState, warn_gcc_compat), 0, "gcc-compat" },
    { offsetof(TCCState, warn_implicit_function_declaration), WD_ALL,
      "implicit-function-declaration" },
    { 0, 0, NULL }
};

static const FlagDef options_f[] = {
    { offsetof(TCCState, char_is_unsigned), 0, "unsigned-char" },
    { offsetof(TCCState, char_is_unsigned), FD_INVERT, "signed-char" },
    { offsetof(TCCState, nocommon), FD_INVERT, "common" },
    { offsetof(TCCState, leading_underscore), 0, "leading-underscore" },
    { offsetof(TCCState, ms_extensions), 0, "ms-extensions" },
    { offsetof(TCCState, dollars_in_identifiers), 0, "dollars-in-identifiers" },
    { 0, 0, NULL }
};

static const FlagDef options_m[] = {
    { offsetof(TCCState, ms_bitfields), 0, "ms-bitfields" },
#ifdef TCC_TARGET_X86_64
    { offsetof(TCCState, nosse), FD_INVERT, "sse" },
#endif
    { 0, 0, NULL }
};

static void parse_option_D(TCCState *s1, const char *optarg)
{
    char *sym = tcc_strdup(s1, optarg);
    char *value = strchr(sym, '=');
    if (value)
        *value++ = '\0';
    tcc_define_symbol(s1, sym, value);
    tcc_free(s1, sym);
}

static void args_parser_add_file(TCCState *s, const char* filename, int filetype)
{
    struct filespec *f = tcc_malloc(s, sizeof *f + strlen(filename));
    f->type = filetype;
    f->alacarte = s->alacarte_link;
    strcpy(f->name, filename);
    dynarray_add(s, &s->files, &s->nb_files, f);
}

static int args_parser_make_argv(TCCState *s1, const char *r, int *argc, char ***argv)
{
    int ret = 0, q, c;
    CString str;
    for(;;) {
        while (c = (unsigned char)*r, c && c <= ' ')
	    ++r;
        if (c == 0)
            break;
        q = 0;
        cstr_new(s1, &str);
        while (c = (unsigned char)*r, c) {
            ++r;
            if (c == '\\' && (*r == '"' || *r == '\\')) {
                c = *r++;
            } else if (c == '"') {
                q = !q;
                continue;
            } else if (q == 0 && c <= ' ') {
                break;
            }
            cstr_ccat(s1, &str, c);
        }
        cstr_ccat(s1, &str, 0);
        //printf("<%s>\n", str.data), fflush(stdout);
        dynarray_add(s1, argv, argc, tcc_strdup(s1, str.data));
        cstr_free(s1, &str);
        ++ret;
    }
    return ret;
}

/* read list file */
static void args_parser_listfile(TCCState *s,
    const char *filename, int optind, int *pargc, char ***pargv)
{
    int fd, i;
    size_t len;
    char *p;
    int argc = 0;
    char **argv = NULL;

    fd = open(filename, O_RDONLY | O_BINARY);
    if (fd < 0)
        tcc_error(s, "listfile '%s' not found", filename);

    len = lseek(fd, 0, SEEK_END);
    p = tcc_malloc(s, len + 1), p[len] = 0;
    lseek(fd, 0, SEEK_SET), read(fd, p, len), close(fd);

    for (i = 0; i < *pargc; ++i)
        if (i == optind)
            args_parser_make_argv(s, p, &argc, &argv);
        else
            dynarray_add(s, &argv, &argc, tcc_strdup(s, (*pargv)[i]));

    tcc_free(s, p);
    dynarray_reset(s, &s->argv, &s->argc);
    *pargc = s->argc = argc, *pargv = s->argv = argv;
}

PUB_FUNC int tcc_parse_args(TCCState *s, int *pargc, char ***pargv, int optind)
{
    const TCCOption *popt;
    const char *optarg, *r;
    const char *run = NULL;
    int last_o = -1;
    int x;
    CString linker_arg; /* collect -Wl options */
    int tool = 0, arg_start = 0, noaction = optind;
    char **argv = *pargv;
    int argc = *pargc;

    cstr_new(s, &linker_arg);

    while (optind < argc) {
        r = argv[optind];
        if (r[0] == '@' && r[1] != '\0') {
            args_parser_listfile(s, r + 1, optind, &argc, &argv);
	    continue;
        }
        optind++;
        if (tool) {
            if (r[0] == '-' && r[1] == 'v' && r[2] == 0)
                ++s->verbose;
            continue;
        }
reparse:
        if (r[0] != '-' || r[1] == '\0') {
            if (r[0] != '@') /* allow "tcc file(s) -run @ args ..." */
                args_parser_add_file(s, r, s->filetype);
            if (run) {
                tcc_set_options(s, run);
                arg_start = optind - 1;
                break;
            }
            continue;
        }

        /* find option in table */
        for(popt = tcc_options; ; ++popt) {
            const char *p1 = popt->name;
            const char *r1 = r + 1;
            if (p1 == NULL)
                tcc_error(s, "invalid option -- '%s'", r);
            if (!strstart(p1, &r1))
                continue;
            optarg = r1;
            if (popt->flags & TCC_OPTION_HAS_ARG) {
                if (*r1 == '\0' && !(popt->flags & TCC_OPTION_NOSEP)) {
                    if (optind >= argc)
                arg_err:
                        tcc_error(s, "argument to '%s' is missing", r);
                    optarg = argv[optind++];
                }
            } else if (*r1 != '\0')
                continue;
            break;
        }

        switch(popt->index) {
        case TCC_OPTION_HELP:
            return OPT_HELP;
        case TCC_OPTION_HELP2:
            return OPT_HELP2;
        case TCC_OPTION_I:
            tcc_add_include_path(s, optarg);
            break;
        case TCC_OPTION_D:
            parse_option_D(s, optarg);
            break;
        case TCC_OPTION_U:
            tcc_undefine_symbol(s, optarg);
            break;
        case TCC_OPTION_L:
            tcc_add_library_path(s, optarg);
            break;
        case TCC_OPTION_B:
            /* set tcc utilities path (mainly for tcc development) */
            tcc_set_lib_path(s, optarg);
            break;
        case TCC_OPTION_l:
            args_parser_add_file(s, optarg, AFF_TYPE_LIB);
            s->nb_libraries++;
            break;
        case TCC_OPTION_pthread:
            parse_option_D(s, "_REENTRANT");
            s->option_pthread = 1;
            break;
        case TCC_OPTION_bench:
            s->do_bench = 1;
            break;
#ifdef CONFIG_TCC_BACKTRACE
        case TCC_OPTION_bt:
            tcc_set_num_callers(s, atoi(optarg));
            break;
#endif
#ifdef CONFIG_TCC_BCHECK
        case TCC_OPTION_b:
            s->do_bounds_check = 1;
            s->do_debug = 1;
            break;
#endif
        case TCC_OPTION_g:
            s->do_debug = 1;
            break;
        case TCC_OPTION_c:
            x = TCC_OUTPUT_OBJ;
        set_output_type:
            if (s->output_type)
                tcc_warning(s, "-%s: overriding compiler action already specified", popt->name);
            s->output_type = x;
            break;
        case TCC_OPTION_d:
            if (*optarg == 'D')
                s->dflag = 3;
            else if (*optarg == 'M')
                s->dflag = 7;
            else if (*optarg == 't')
                s->dflag = 16;
            else if (isnum(*optarg))
                s->g_debug = atoi(optarg);
            else
                goto unsupported_option;
            break;
        case TCC_OPTION_static:
            s->static_link = 1;
            break;
        case TCC_OPTION_std:
    	    /* silently ignore, a current purpose:
    	       allow to use a tcc as a reference compiler for "make test" */
            break;
        case TCC_OPTION_shared:
            x = TCC_OUTPUT_DLL;
            goto set_output_type;
        case TCC_OPTION_soname:
            s->soname = tcc_strdup(s, optarg);
            break;
        case TCC_OPTION_o:
            if (s->outfile) {
                tcc_warning(s, "multiple -o option");
                tcc_free(s, s->outfile);
            }
            s->outfile = tcc_strdup(s, optarg);
            break;
        case TCC_OPTION_r:
            /* generate a .o merging several output files */
            s->option_r = 1;
            x = TCC_OUTPUT_OBJ;
            goto set_output_type;
        case TCC_OPTION_isystem:
            tcc_add_sysinclude_path(s, optarg);
            break;
	case TCC_OPTION_include:
	    dynarray_add(s, &s->cmd_include_files,
			 &s->nb_cmd_include_files, tcc_strdup(s, optarg));
	    break;
        case TCC_OPTION_nostdinc:
            s->nostdinc = 1;
            break;
        case TCC_OPTION_nostdlib:
            s->nostdlib = 1;
            break;
        case TCC_OPTION_run:
#ifndef TCC_IS_NATIVE
            tcc_error("-run is not available in a cross compiler");
#endif
            run = optarg;
            x = TCC_OUTPUT_MEMORY;
            goto set_output_type;
        case TCC_OPTION_v:
            do ++s->verbose; while (*optarg++ == 'v');
            ++noaction;
            break;
        case TCC_OPTION_f:
            if (set_flag(s, options_f, optarg) < 0)
                goto unsupported_option;
            break;
#ifdef TCC_TARGET_ARM
        case TCC_OPTION_mfloat_abi:
            /* tcc doesn't support soft float yet */
            if (!strcmp(optarg, "softfp")) {
                s->float_abi = ARM_SOFTFP_FLOAT;
                tcc_undefine_symbol(s, "__ARM_PCS_VFP");
            } else if (!strcmp(optarg, "hard"))
                s->float_abi = ARM_HARD_FLOAT;
            else
                tcc_error(s, "unsupported float abi '%s'", optarg);
            break;
#endif
        case TCC_OPTION_m:
            if (set_flag(s, options_m, optarg) < 0) {
                if (x = atoi(optarg), x != 32 && x != 64)
                    goto unsupported_option;
                if (PTR_SIZE != x/8)
                    return x;
                ++noaction;
            }
            break;
        case TCC_OPTION_W:
            if (set_flag(s, options_W, optarg) < 0)
                goto unsupported_option;
            break;
        case TCC_OPTION_w:
            s->warn_none = 1;
            break;
        case TCC_OPTION_rdynamic:
            s->rdynamic = 1;
            break;
        case TCC_OPTION_Wl:
            if (linker_arg.size)
                --linker_arg.size, cstr_ccat(s, &linker_arg, ',');
            cstr_cat(s, &linker_arg, optarg, 0);
            if (tcc_set_linker(s, linker_arg.data))
                cstr_free(s, &linker_arg);
            break;
	case TCC_OPTION_Wp:
	    r = optarg;
	    goto reparse;
        case TCC_OPTION_E:
            x = TCC_OUTPUT_PREPROCESS;
            goto set_output_type;
        case TCC_OPTION_P:
            s->Pflag = atoi(optarg) + 1;
            break;
        case TCC_OPTION_MD:
            s->gen_deps = 1;
            break;
        case TCC_OPTION_MF:
            s->deps_outfile = tcc_strdup(s, optarg);
            break;
        case TCC_OPTION_dumpversion:
            printf ("%s\n", TCC_VERSION);
            exit(0);
            break;
        case TCC_OPTION_x:
            if (*optarg == 'c')
                s->filetype = AFF_TYPE_C;
            else if (*optarg == 'a')
                s->filetype = AFF_TYPE_ASMPP;
            else if (*optarg == 'n')
                s->filetype = AFF_TYPE_NONE;
            else
                tcc_warning(s, "unsupported language '%s'", optarg);
            break;
        case TCC_OPTION_O:
            last_o = atoi(optarg);
            break;
        case TCC_OPTION_print_search_dirs:
            x = OPT_PRINT_DIRS;
            goto extra_action;
        case TCC_OPTION_impdef:
            x = OPT_IMPDEF;
            goto extra_action;
        case TCC_OPTION_ar:
            x = OPT_AR;
        extra_action:
            arg_start = optind - 1;
            if (arg_start != noaction)
                tcc_error(s, "cannot parse %s here", r);
            tool = x;
            break;
        case TCC_OPTION_traditional:
        case TCC_OPTION_pedantic:
        case TCC_OPTION_pipe:
        case TCC_OPTION_s:
            /* ignored */
            break;
        default:
unsupported_option:
            if (s->warn_unsupported)
                tcc_warning(s, "unsupported option '%s'", r);
            break;
        }
    }
    if (last_o > 0)
        tcc_define_symbol(s, "__OPTIMIZE__", NULL);
    if (linker_arg.size) {
        r = linker_arg.data;
        goto arg_err;
    }
    *pargc = argc - arg_start;
    *pargv = argv + arg_start;
    if (tool)
        return tool;
    if (optind != noaction)
        return 0;
    if (s->verbose == 2)
        return OPT_PRINT_DIRS;
    if (s->verbose)
        return OPT_V;
    return OPT_HELP;
}

LIBTCCAPI void tcc_set_options(TCCState *s, const char *r)
{
    char **argv = NULL;
    int argc = 0;
    args_parser_make_argv(s, r, &argc, &argv);
    tcc_parse_args(s, &argc, &argv, 0);
    dynarray_reset(s, &argv, &argc);
}

PUB_FUNC void tcc_print_stats(TCCState *s, unsigned total_time)
{
    if (total_time < 1)
        total_time = 1;
    if (s->total_bytes < 1)
        s->total_bytes = 1;
    fprintf(stderr, "* %d idents, %d lines, %d bytes\n"
                    "* %0.3f s, %u lines/s, %0.1f MB/s\n",
           s->tok_ident - TOK_IDENT, s->total_lines, s->total_bytes,
           (double)total_time/1000,
           (unsigned)s->total_lines*1000/total_time,
           (double)s->total_bytes/1000/total_time);
#ifdef MEM_DEBUG
    fprintf(stderr, "* %d bytes memory used\n", mem_max_size);
#endif
}
