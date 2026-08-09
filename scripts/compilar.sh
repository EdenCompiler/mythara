#!/usr/bin/env sh
set -eu

TIPO=${MYTHARA_TIPO_BUILD:-Release}
AUDIO=${MYTHARA_AUDIO:-ON}
PASTA=${MYTHARA_BUILD_DIR:-build/local}

cmake -S . -B "$PASTA" \
    -DCMAKE_BUILD_TYPE="$TIPO" \
    -DMYTHARA_AUDIO="$AUDIO" \
    -DMYTHARA_AVISOS_COMO_ERROS=ON
cmake --build "$PASTA" --parallel

printf 'Mythara compilada em %s/mythara\n' "$PASTA"
