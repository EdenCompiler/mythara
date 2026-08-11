/*
 * Modulo: preludio
 * Responsabilidade: selecionar APIs nativas e reunir os cabecalhos usados pela
 * unidade de composicao.
 *
 * Este arquivo nao declara regras de negocio. Instrucoes de compilacao ficam em
 * docs/COMPILACAO.md.
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
