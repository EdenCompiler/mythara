#!/usr/bin/env sh
set -eu

if command -v clang-format >/dev/null 2>&1; then
    clang-format --dry-run --Werror src/mythara.c
else
    printf 'Aviso: clang-format não encontrado; formatação não verificada.\n' >&2
fi

MYTHARA_BUILD_DIR=${MYTHARA_BUILD_DIR:-build/verificacao}
cmake -S . -B "$MYTHARA_BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DMYTHARA_AUDIO=OFF \
    -DMYTHARA_AVISOS_COMO_ERROS=ON
cmake --build "$MYTHARA_BUILD_DIR" --parallel
ctest --test-dir "$MYTHARA_BUILD_DIR" --output-on-failure

printf 'Verificação da Mythara concluída.\n'
