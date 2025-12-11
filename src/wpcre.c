#include "wpcre.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <pcre2.h>

#define PCRE2_CODE_UNIT_WIDTH 8
#include "uthash.h"

/* ------------------------------------------------------------------
   CONFIGURAÇÃO VIA MACRO:
   PCRE_WRAPPER_THREAD_SAFE = 1  →  cache global + mutex na criação
   PCRE_WRAPPER_THREAD_SAFE = 0  →  cache por thread, sem mutex
   ------------------------------------------------------------------ */

#if PCRE_WRAPPER_THREAD_SAFE
#include <pthread.h>
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;
#define LOCK()   pthread_mutex_lock(&g_mutex)
#define UNLOCK() pthread_mutex_unlock(&g_mutex)
#else
#define LOCK()
#define UNLOCK()
#endif


/* ------------------------------------------------------------------
   Estruturas internas
   ------------------------------------------------------------------ */

typedef struct pcre_pattern {
    char *pattern;
    pcre2_code *re;
    pcre2_match_data *mdata;   /* match buffer reutilizável */
    UT_hash_handle hh;
} pcre_pattern_t;

#if PCRE_WRAPPER_THREAD_SAFE
static pcre_pattern_t *g_regex_cache = NULL;
#else
/* Cada thread mantém seu próprio cache → sem mutex algum */
static __thread pcre_pattern_t *g_regex_cache = NULL;
#endif

struct pcre_regex {
    pcre2_code *re;
    pcre2_match_data *mdata;
};


/* ------------------------------------------------------------------
   Compilação da regex (cache + JIT + mdata prealocado)
   ------------------------------------------------------------------ */

pcre_regex_t *
pcre_regex_compile(const char *pattern, int options)
{
    pcre_pattern_t *entry;
    PCRE2_SIZE erroffset;
    int errorcode;

    LOCK();
    HASH_FIND_STR(g_regex_cache, pattern, entry);
    if (entry) {
        /* apenas monta wrapper para o usuário */
        pcre_regex_t *wr = malloc(sizeof(*wr));
        wr->re = entry->re;
        wr->mdata = entry->mdata;
        UNLOCK();
        return wr;
    }

    /* compila regex */
    pcre2_code *re = pcre2_compile(
        (PCRE2_UCHAR *)pattern,
        PCRE2_ZERO_TERMINATED,
        options,
        &errorcode,
        &erroffset,
        NULL);

    if (!re) {
        UNLOCK();
        return NULL;
    }

    /* 🔥 sempre ativa JIT em ambos os modos */
    pcre2_jit_compile(re, PCRE2_JIT_COMPLETE);

    /* cria match_data apenas 1x por regex */
    pcre2_match_data *mdata =
        pcre2_match_data_create_from_pattern(re, NULL);

    /* guarda no cache */
    entry = malloc(sizeof(*entry));
    entry->pattern = strdup(pattern);
    entry->re = re;
    entry->mdata = mdata;

    HASH_ADD_KEYPTR(hh, g_regex_cache,
                    entry->pattern, strlen(entry->pattern), entry);
    UNLOCK();

    /* cria wrapper */
    pcre_regex_t *wr = malloc(sizeof(*wr));
    wr->re = re;
    wr->mdata = mdata;
    return wr;
}


/* ------------------------------------------------------------------
   MATCH • super rápido • JIT-enabled • sem mutex
   ------------------------------------------------------------------ */

bool
pcre_regex_match(pcre_regex_t *wr, const char *subject, size_t len)
{
    if (!wr || !wr->re || !wr->mdata)
        return false;

    int rc = pcre2_match(
        wr->re,
        (PCRE2_UCHAR *)subject,
        len,
        0,
        0,
        wr->mdata,
        NULL);

    return rc >= 0;
}


/* ------------------------------------------------------------------
   Libera wrapper (não destrói cache)
   ------------------------------------------------------------------ */

void
pcre_regex_free(pcre_regex_t *wr)
{
    free(wr);
}


/* ------------------------------------------------------------------
   Destruição completa do cache
   ------------------------------------------------------------------ */

void
pcre_regex_cache_destroy(void)
{
    pcre_pattern_t *s, *tmp;

    LOCK();
    HASH_ITER(hh, g_regex_cache, s, tmp) {
        HASH_DEL(g_regex_cache, s);
        pcre2_code_free(s->re);
        pcre2_match_data_free(s->mdata);
        free(s->pattern);
        free(s);
    }
    UNLOCK();
}


/* ------------------------------------------------------------------
   Debug: listar padrões
   ------------------------------------------------------------------ */

int
regex_print(void)
{
    pcre_pattern_t *s, *tmp;

    LOCK();
    HASH_ITER(hh, g_regex_cache, s, tmp) {
        printf("Pattern: %s\n", s->pattern);
    }
    UNLOCK();

    return 0;
}


/* ------------------------------------------------------------------
   Estatísticas básicas
   ------------------------------------------------------------------ */

int
regex_stat(void)
{
    pcre_pattern_t *s, *tmp;
    size_t count = 0, total = 0;

    LOCK();
    HASH_ITER(hh, g_regex_cache, s, tmp) {
        count++;
        total += strlen(s->pattern);
    }
    UNLOCK();

    printf("Cache entries: %zu\n", count);
    if (count)
        printf("Avg. key length: %.2f\n", (double)total / count);

    return 0;
}
