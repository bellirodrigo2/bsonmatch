#ifndef WPCRE_H
#define WPCRE_H

#include <stdbool.h>
#include <stddef.h>

#ifndef PCRE_WRAPPER_THREAD_SAFE
#define PCRE_WRAPPER_THREAD_SAFE 1
#endif

typedef struct pcre_regex pcre_regex_t;

pcre_regex_t *pcre_regex_compile(const char *pattern, int options);
bool pcre_regex_match(pcre_regex_t *re, const char *subject, size_t len);
void pcre_regex_free(pcre_regex_t *re);

int regex_print(void);
int regex_stat(void);
void pcre_regex_cache_destroy(void);

#endif
