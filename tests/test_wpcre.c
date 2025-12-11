#include <stdio.h>
#include <string.h>
#include "wpcre.h"

int main(void)
{
    printf("== Teste do wrapper PCRE ==\n");

    /* padrão simples */
    const char *pattern = "^hello.*world$";
    const char *ok      = "hello awesome world";
    const char *nope    = "bye world";

    printf("Compilando regex: %s\n", pattern);
    pcre_regex_t *re = pcre_regex_compile(pattern, 0);
    if (!re) {
        printf("ERRO: falha ao compilar regex!\n");
        return 1;
    }

    printf("Testando string OK...\n");
    if (pcre_regex_match(re, ok, strlen(ok))) {
        printf("✔ Match OK\n");
    } else {
        printf("✘ ERRO: deveria ter dado match!\n");
    }

    printf("Testando string que NÃO combina...\n");
    if (!pcre_regex_match(re, nope, strlen(nope))) {
        printf("✔ Correto: sem match\n");
    } else {
        printf("✘ ERRO: match inesperado!\n");
    }

    printf("\n== Conteúdo do cache ==\n");
    regex_print();

    printf("\n== Estatísticas ==\n");
    regex_stat();

    printf("\nLimpando cache...\n");
    pcre_regex_cache_destroy();

    pcre_regex_free(re);
    printf("Fim do teste.\n");

    return 0;
}
