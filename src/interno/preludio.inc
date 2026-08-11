/*
 * Mythara - motor e editor visual de JRPG 2D em C11.
 *
 * Compilar no Linux (com audio):
 *   cc -std=c11 -O2 -Wall -Wextra -Wpedantic src/mythara.c src/formato_binario.c src/idioma.c
 *      -o mythara -lX11 -lasound -lm -pthread
 * Compilar (sem audio):
 *   cc -std=c11 -O2 -Wall -Wextra -Wpedantic -DMYTHARA_SEM_AUDIO src/mythara.c
 *      src/formato_binario.c src/idioma.c -o mythara -lX11 -lm
 * Compilar no Windows com MinGW (audio nativo WinMM):
 *   gcc -std=c11 -O2 src/mythara.c src/formato_binario.c src/idioma.c -o mythara.exe
 *      -lgdi32 -luser32 -lshell32 -lwinmm -lm
 *
 * Uso:
 *   ./mythara [projeto.myr]
 *   ./mythara --jogar projeto.myr
 *   ./mythara --novo projeto.myr
 *   ./mythara --autoteste
 *   ./mythara --ajuda
 *
 * O codigo e organizado em modulos internos, marcados por cabecalhos. Nomes de
 * APIs externas mantem a grafia original; todo o restante esta em portugues.
 */

#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#ifndef MYTHARA_SEM_AUDIO
#include <alsa/asoundlib.h>
#endif
#include <dirent.h>
#include <pthread.h>
#include <strings.h>
#include <unistd.h>
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <direct.h>
#include <io.h>
#include <process.h>
#define mkdir(caminho, modo) _mkdir(caminho)
#define chmod(caminho, modo) _chmod(caminho, modo)
#define unlink _unlink
#define access _access
#define chdir _chdir
#define getpid _getpid
#define fsync _commit
#define fileno _fileno
#define open _open
#define read _read
#define write _write
#define close _close
#define strcasecmp _stricmp
#ifndef O_BINARY
#define O_BINARY 0
#endif
#endif
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include "../formato_binario.h"
#include "../idioma.h"
#include "../modelo.h"
#ifndef O_BINARY
#define O_BINARY 0
#endif
