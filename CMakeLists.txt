cmake_minimum_required(VERSION 3.15)
project(bsonmatch C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Diretórios principais
set(SRC_DIR   "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(TEST_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/tests")

# Evita problemas com try_compile no Windows/MSVC
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Desativar componentes extras do PCRE2 (opcionais)
set(PCRE2_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(PCRE2_BUILD_PCRE2GREP OFF CACHE BOOL "" FORCE)
set(PCRE2_SUPPORT_LIBREADLINE OFF CACHE BOOL "" FORCE)
set(PCRE2_SUPPORT_LIBEDIT OFF CACHE BOOL "" FORCE)
set(PCRE2_SUPPORT_LIBZ OFF CACHE BOOL "" FORCE)
set(PCRE2_SUPPORT_LIBBZ2 OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)

# PCRE2 oficial
add_subdirectory(external/pcre2)

# -------------------------------
# Biblioteca WPCR (wrapper)
# -------------------------------
add_library(wpcre STATIC
    ${SRC_DIR}/wpcre.c
    ${SRC_DIR}/wpcre.h
)

target_include_directories(wpcre PUBLIC
    ${SRC_DIR}
    external/pcre2          # inclui headers do PCRE2
)

target_link_libraries(wpcre PUBLIC pcre2-8)

# -------------------------------
# Teste (em tests/)
# -------------------------------
add_executable(test_wpcre
    ${TEST_DIR}/test_wpcre.c
)

target_link_libraries(test_wpcre wpcre)
