# Compilação e distribuição

## Dependências

### Linux com áudio

- Compilador C11
- X11
- ALSA
- pthread
- CMake 3.16 ou Make

Debian e Ubuntu:

```bash
sudo apt install build-essential cmake libx11-dev libasound2-dev
```

Fedora:

```bash
sudo dnf install gcc cmake libX11-devel alsa-lib-devel
```

Arch Linux:

```bash
sudo pacman -S base-devel cmake libx11 alsa-lib
```

### Linux sem áudio

ALSA não é necessária quando `MYTHARA_AUDIO=OFF` ou `AUDIO=0`.

### Windows

O build validado usa MinGW-w64 e as bibliotecas do próprio Windows: GDI, User32, Shell32 e WinMM.

## Make

```bash
make                       # debug com áudio
make RELEASE=1             # release com áudio
make AUDIO=0 RELEASE=1     # release sem áudio
make test                  # autotestes
make run                   # abre o editor
make clean                 # remove build/ e dist/
```

Variáveis úteis:

```bash
make CC=clang BUILD_DIR=build/clang AUDIO=0 RELEASE=1
```

## CMake

```bash
cmake -S . -B build/local \
    -DCMAKE_BUILD_TYPE=Release \
    -DMYTHARA_AUDIO=ON \
    -DMYTHARA_AVISOS_COMO_ERROS=ON
cmake --build build/local --parallel
ctest --test-dir build/local --output-on-failure
```

Instalação em prefixo local:

```bash
cmake --install build/local --prefix "$PWD/dist/instalacao"
```

## Compilação cruzada para Windows

Em Debian ou Ubuntu:

```bash
sudo apt install gcc-mingw-w64-x86-64
x86_64-w64-mingw32-gcc -std=c11 -O2 -Wall -Wextra -Wpedantic \
    src/mythara.c src/formato_binario.c src/idioma.c -o build/mythara.exe \
    -lgdi32 -luser32 -lshell32 -lwinmm -lm
```

## Sanitizadores

```bash
clang -std=c11 -O1 -g -Wall -Wextra -Wpedantic \
    -DMYTHARA_SEM_AUDIO -fsanitize=address,undefined \
    src/mythara.c src/formato_binario.c src/idioma.c \
    -o build/mythara-sanitizado -lX11 -lm -pthread

ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1 \
    ./build/mythara-sanitizado --autoteste
```

## Pacote

`scripts/empacotar.sh` cria um arquivo em `dist/` contendo o executável, README e changelog. O script
não compila: execute um build release antes dele ou informe o binário em `MYTHARA_BINARIO`.
