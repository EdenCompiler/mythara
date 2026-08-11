#!/usr/bin/env sh
set -eu

BINARIO=${MYTHARA_BINARIO:-build/release/mythara}
if [ ! -f "$BINARIO" ]; then
    if [ -f build/make/mythara ]; then
        BINARIO=build/make/mythara
    else
        printf 'Binário não encontrado. Compile uma release ou defina MYTHARA_BINARIO.\n' >&2
        exit 1
    fi
fi

VERSAO=$(sed -n 's/^project(Mythara VERSION \([^ ]*\).*/\1/p' CMakeLists.txt)
PLATAFORMA=$(uname -s | tr '[:upper:]' '[:lower:]')-$(uname -m)
NOME="mythara-${VERSAO}-${PLATAFORMA}"
TEMPORARIO=$(mktemp -d "${TMPDIR:-/tmp}/mythara-pacote-XXXXXX")
trap 'rm -rf "$TEMPORARIO"' EXIT HUP INT TERM

mkdir -p dist "$TEMPORARIO/$NOME"
cp "$BINARIO" "$TEMPORARIO/$NOME/mythara"
cp README.md CHANGELOG.md LICENSE "$TEMPORARIO/$NOME/"
chmod +x "$TEMPORARIO/$NOME/mythara"
tar -C "$TEMPORARIO" -czf "dist/$NOME.tar.gz" "$NOME"

printf 'Pacote criado: dist/%s.tar.gz\n' "$NOME"
