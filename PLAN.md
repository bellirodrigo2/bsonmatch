# Plano de Migração: PCRE1 → wregex (PCRE2)

## Objetivo
Migrar o uso de PCRE1 para o wrapper `wregex` (baseado em PCRE2) e ajustar CMakeLists.txt para compilar a biblioteca completa com libbson.

---

## Arquivos Afetados

### Código fonte (migração PCRE)
| Arquivo | Alterações |
|---------|------------|
| `src/bsoncompare.h` | Remover `#include <pcre.h>`, remover `struct pattern_to_regex` (cache antigo) |
| `src/bsoncompare.c` | Atualizar `regex_destroy()` → `wregex_cache_destroy()`, remover `regex_print()` ou adaptar |
| `src/mongoc-matcher-op.c` | Substituir bloco PCRE (linhas ~1352-1406) por chamadas ao wregex |

### Build
| Arquivo | Alterações |
|---------|------------|
| `CMakeLists.txt` | Adicionar compilação de libbson, fonte principal, e testes existentes |

---

## Etapas de Migração

### 1. Atualizar `src/bsoncompare.h`
- Remover `#include <pcre.h>`
- Remover declaração de `struct pattern_to_regex` e `extern global_compiled_regexes`
- Incluir `#include "wregex.h"` (se necessário expor algo do wregex)

### 2. Atualizar `src/bsoncompare.c`
- Remover variável global `global_compiled_regexes`
- Modificar `regex_destroy()`:
  ```c
  int regex_destroy() {
      wregex_cache_destroy();
      return 0;
  }
  ```
- Remover ou adaptar `regex_print()` (usar `wregex_cache_stats()` se quiser debug)

### 3. Atualizar `src/mongoc-matcher-op.c`
- Remover `#include <pcre.h>`
- Adicionar `#include "wregex.h"`
- Substituir bloco de matching regex (~linhas 1352-1406):

**Código atual (PCRE1):**
```c
pcre *re;
const char *error;
int erroffset;
int OVECCOUNT = 3;
int ovector[OVECCOUNT];
int rc;
int pcre_options = 0;
if (0 == memcmp (options, "i", 1)){
   pcre_options = PCRE_CASELESS;
}
// ... cache manual + pcre_compile + pcre_exec
```

**Código novo (wregex):**
```c
unsigned int wregex_opts = 0;
if (options && strchr(options, 'i')) {
    wregex_opts |= WREGEX_CASELESS;
}
if (options && strchr(options, 'm')) {
    wregex_opts |= WREGEX_MULTILINE;
}
if (options && strchr(options, 's')) {
    wregex_opts |= WREGEX_DOTALL;
}

wregex_t *re = wregex_compile(pattern, wregex_opts);
if (re == NULL) {
    return false;
}

bool match = wregex_match(re, rstr, rlen);
wregex_free(re);
return match;
```

**Notas:**
- O cache é gerenciado internamente pelo wregex (não precisa de `HASH_FIND_STR`)
- `wregex_free()` libera apenas o wrapper, o regex compilado permanece no cache

### 4. Atualizar `CMakeLists.txt`

Adicionar:

#### 4.1 Compilação de libbson (separada)
```cmake
# Opção para usar libbson pré-compilado ou compilar
option(BUILD_LIBBSON "Build libbson from source" OFF)
set(LIBBSON_LIBRARY "" CACHE PATH "Path to libbson.a if not building")
set(LIBBSON_INCLUDE "" CACHE PATH "Path to libbson headers")

if(BUILD_LIBBSON)
    # ... código comentado existente para ExternalProject
else()
    # Usar libbson fornecido
    add_library(bson STATIC IMPORTED)
    set_target_properties(bson PROPERTIES
        IMPORTED_LOCATION ${LIBBSON_LIBRARY}
    )
    include_directories(${LIBBSON_INCLUDE})
endif()
```

#### 4.2 Biblioteca principal bsonmatch
```cmake
set(BSONMATCH_SOURCES
    src/bsoncompare.c
    src/mongoc-matcher.c
    src/mongoc-matcher-op.c
    src/mongoc-matcher-op-geojson.c
    src/mongoc-bson-descendants.c
    src/wregex.c
    # Adicionar outros conforme features ativas
)

add_library(bsonmatch STATIC ${BSONMATCH_SOURCES})

target_include_directories(bsonmatch PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${UTHASH_DIR}
)

target_link_libraries(bsonmatch PUBLIC
    wregex
    bson
    m
)
```

#### 4.3 Compilação dos testes existentes
```cmake
function(bsonmatch_add_test test_name source_file)
    add_executable(${test_name} ${source_file})
    target_link_libraries(${test_name} PRIVATE bsonmatch)
    add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

# Testes básicos (sempre compilados)
bsonmatch_add_test(test_simple tests/simple.c)
bsonmatch_add_test(test_regex tests/bsoncompare_regex.c)
bsonmatch_add_test(test_leak tests/bsoncompare_leak_test.c)
# ... adicionar demais testes conforme CMakeLists_OLD
```

---

## Ordem de Execução

1. **Migrar código** (bsoncompare.h → bsoncompare.c → mongoc-matcher-op.c)
2. **Atualizar CMakeLists.txt** (adicionar bsonmatch lib + testes)
3. **Testar** compilação do wregex (já funciona)
4. **Testar** compilação da lib completa (com libbson externo)
5. **Rodar testes** existentes, especialmente `bsoncompare_regex.c`

---

## Dependências Externas

| Dependência | Localização | Uso |
|-------------|-------------|-----|
| PCRE2 | `external/pcre2` | Compilado como subprojeto |
| uthash | `external/uthash/src` | Header-only |
| libbson | `external/mongoc` | Compilar separadamente ou fornecer `.a` |

---

## Notas

- O wrapper `wregex` já tem cache thread-safe com uthash
- O código antigo tinha um bug: opções não eram incluídas na chave do cache (`TODO` no código)
- O `wregex` resolve isso: chave = pattern + options
- Funções `regex_destroy()` e `regex_print()` são expostas na API pública, manter compatibilidade
- Manter `bsonsearch_startup()` e `bsonsearch_shutdown()` funcionando (shutdown chama `regex_destroy`)
