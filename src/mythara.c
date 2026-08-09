/*
 * Mythara - motor e editor visual de JRPG 2D em um unico arquivo C.
 *
 * Compilar no Linux (com audio):
 *   cc -std=c11 -O2 -Wall -Wextra -Wpedantic src/mythara.c -o mythara -lX11 -lasound -lm -pthread
 * Compilar (sem audio):
 *   cc -std=c11 -O2 -Wall -Wextra -Wpedantic -DMYTHARA_SEM_AUDIO src/mythara.c -o mythara -lX11 -lm
 * Compilar no Windows com MinGW (audio nativo WinMM):
 *   gcc -std=c11 -O2 src/mythara.c -o mythara.exe -lgdi32 -luser32 -lshell32 -lwinmm -lm
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
#ifndef O_BINARY
#define O_BINARY 0
#endif

/* ========================================================================== */
/* Configuracao e tipos basicos                                                */
/* ========================================================================== */

#define MYTHARA_VERSAO 3u
#define MYTHARA_LARGURA_INICIAL 1280
#define MYTHARA_ALTURA_INICIAL 720
#define MYTHARA_TAMANHO_TILE 32
#define MYTHARA_MAX_CAMADAS 8
#define MYTHARA_MAX_MAPAS 256
#define MYTHARA_MAX_LARGURA_MAPA 256
#define MYTHARA_MAX_ALTURA_MAPA 256
#define MYTHARA_MAX_ENTIDADES 2048
#define MYTHARA_MAX_EVENTOS 2048
#define MYTHARA_MAX_COMANDOS 512
#define MYTHARA_MAX_ITENS 512
#define MYTHARA_MAX_INIMIGOS 512
#define MYTHARA_MAX_RECURSOS 1024
#define MYTHARA_MAX_EDICOES 10000
#define MYTHARA_MAX_TEXTOS 512
#define MYTHARA_MAX_FLAGS 512
#define MYTHARA_MAX_VARIAVEIS 512
#define MYTHARA_MAX_CAMINHO 1024
#define MYTHARA_MAX_HISTORICO_BYTES (256u * 1024u * 1024u)

typedef uint32_t Cor;
typedef int32_t Inteiro;
typedef uint64_t Identificador;

typedef struct {
    int x, y, largura, altura;
} Retangulo;
typedef struct {
    uint32_t *pixels;
    int largura, altura;
} Tela;

static Cor cor_rgb(unsigned r, unsigned g, unsigned b) {
    return 0xff000000u | ((r & 255u) << 16) | ((g & 255u) << 8) | (b & 255u);
}

static int limitar_int(int valor, int minimo, int maximo) {
    return valor < minimo ? minimo : (valor > maximo ? maximo : valor);
}

static int ponto_em_retangulo(int x, int y, Retangulo r) {
    return x >= r.x && y >= r.y && x < r.x + r.largura && y < r.y + r.altura;
}

static void copiar_texto(char *destino, size_t capacidade, const char *origem) {
    if (!capacidade)
        return;
    if (!origem)
        origem = "";
    size_t n = strlen(origem);
    if (n >= capacidade)
        n = capacidade - 1;
    memmove(destino, origem, n);
    destino[n] = 0;
}

static const char *nome_base(const char *caminho) {
    const char *barra = strrchr(caminho, '/');
#ifdef _WIN32
    const char *barra_windows = strrchr(caminho, '\\');
    if (!barra || barra_windows > barra)
        barra = barra_windows;
#endif
    return barra ? barra + 1 : caminho;
}

static double agora_segundos(void) {
#ifdef _WIN32
    LARGE_INTEGER frequencia, contador;
    QueryPerformanceFrequency(&frequencia);
    QueryPerformanceCounter(&contador);
    return (double)contador.QuadPart / (double)frequencia.QuadPart;
#else
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec / 1000000000.0;
#endif
}

static void pausar_milissegundos(int ms) {
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    struct timespec pausa = {ms / 1000, (long)(ms % 1000) * 1000000L};
    nanosleep(&pausa, NULL);
#endif
}

static int caminho_do_executavel(char *destino, size_t capacidade) {
#ifdef _WIN32
    DWORD n = GetModuleFileNameA(NULL, destino, (DWORD)capacidade);
    return n > 0 && n < capacidade;
#else
    ssize_t n = readlink("/proc/self/exe", destino, capacidade - 1);
    if (n <= 0)
        return 0;
    destino[n] = 0;
    return 1;
#endif
}

#ifdef _WIN32
static FILE *abrir_arquivo_utf8(const char *caminho, const char *modo) {
    wchar_t wcaminho[MYTHARA_MAX_CAMINHO], wmodo[16];
    if (!MultiByteToWideChar(CP_UTF8, 0, caminho, -1, wcaminho,
                             (int)(sizeof(wcaminho) / sizeof(wcaminho[0]))))
        return NULL;
    if (!MultiByteToWideChar(CP_UTF8, 0, modo, -1, wmodo, (int)(sizeof(wmodo) / sizeof(wmodo[0]))))
        return NULL;
    return _wfopen(wcaminho, wmodo);
}
#define fopen abrir_arquivo_utf8
#endif

static void normalizar_caminho(char *caminho) {
    for (char *p = caminho; *p; ++p)
        if (*p == '\\')
            *p = '/';
}

static int caminho_absoluto(const char *caminho) {
#ifdef _WIN32
    return (isalpha((unsigned char)caminho[0]) && caminho[1] == ':') || caminho[0] == '/' ||
           caminho[0] == '\\';
#else
    return caminho[0] == '/';
#endif
}

static void pasta_do_arquivo(const char *caminho, char *destino, size_t capacidade) {
    copiar_texto(destino, capacidade, caminho);
    normalizar_caminho(destino);
    char *barra = strrchr(destino, '/');
    if (barra)
        *barra = 0;
    else
        copiar_texto(destino, capacidade, ".");
}

static int juntar_caminho(char *destino, size_t capacidade, const char *pasta, const char *nome) {
    int n = snprintf(destino, capacidade, "%s%s%s", pasta,
                     pasta[0] && pasta[strlen(pasta) - 1] != '/' ? "/" : "", nome);
    if (n < 0 || (size_t)n >= capacidade)
        return 0;
    normalizar_caminho(destino);
    return 1;
}

static int criar_diretorios(const char *caminho) {
    char copia[MYTHARA_MAX_CAMINHO];
    copiar_texto(copia, sizeof(copia), caminho);
    normalizar_caminho(copia);
    size_t inicio = 1;
#ifdef _WIN32
    if (isalpha((unsigned char)copia[0]) && copia[1] == ':')
        inicio = 3;
#endif
    for (size_t i = inicio; copia[i]; ++i)
        if (copia[i] == '/') {
            char salvo = copia[i];
            copia[i] = 0;
            if (copia[0] && mkdir(copia, 0755) != 0 && errno != EEXIST)
                return 0;
            copia[i] = salvo;
        }
    return !copia[0] || mkdir(copia, 0755) == 0 || errno == EEXIST;
}

typedef struct {
    char nome[256];
    int diretorio;
} ItemDiretorio;

static int comparar_itens_diretorio(const void *a, const void *b) {
    const ItemDiretorio *x = a, *y = b;
    if (x->diretorio != y->diretorio)
        return y->diretorio - x->diretorio;
    return strcasecmp(x->nome, y->nome);
}

static int listar_diretorio(const char *pasta, ItemDiretorio *itens, int maximo) {
    int quantidade = 0;
#ifdef _WIN32
    char padrao[MYTHARA_MAX_CAMINHO];
    juntar_caminho(padrao, sizeof(padrao), pasta, "*");
    wchar_t wpadrao[MYTHARA_MAX_CAMINHO];
    if (!MultiByteToWideChar(CP_UTF8, 0, padrao, -1, wpadrao, MYTHARA_MAX_CAMINHO))
        return 0;
    WIN32_FIND_DATAW d;
    HANDLE busca = FindFirstFileW(wpadrao, &d);
    if (busca == INVALID_HANDLE_VALUE)
        return 0;
    do {
        char nome[256];
        if (!WideCharToMultiByte(CP_UTF8, 0, d.cFileName, -1, nome, sizeof(nome), NULL, NULL))
            continue;
        if (!strcmp(nome, ".") || !strcmp(nome, ".."))
            continue;
        copiar_texto(itens[quantidade].nome, sizeof(itens[quantidade].nome), nome);
        itens[quantidade].diretorio = (d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        quantidade++;
    } while (quantidade < maximo && FindNextFileW(busca, &d));
    FindClose(busca);
#else
    DIR *dir = opendir(pasta);
    if (!dir)
        return 0;
    struct dirent *d;
    while (quantidade < maximo && (d = readdir(dir))) {
        if (!strcmp(d->d_name, ".") || !strcmp(d->d_name, ".."))
            continue;
        copiar_texto(itens[quantidade].nome, sizeof(itens[quantidade].nome), d->d_name);
        char completo[MYTHARA_MAX_CAMINHO];
        struct stat st;
        juntar_caminho(completo, sizeof(completo), pasta, d->d_name);
        itens[quantidade].diretorio = stat(completo, &st) == 0 && S_ISDIR(st.st_mode);
        quantidade++;
    }
    closedir(dir);
#endif
    qsort(itens, (size_t)quantidade, sizeof(*itens), comparar_itens_diretorio);
    return quantidade;
}

static void subir_diretorio(char *pasta) {
    normalizar_caminho(pasta);
    size_t n = strlen(pasta);
    while (n > 1 && pasta[n - 1] == '/')
        pasta[--n] = 0;
    char *barra = strrchr(pasta, '/');
    if (barra && barra != pasta)
        *barra = 0;
    else if (barra)
        pasta[1] = 0;
}

/* ========================================================================== */
/* Modelo persistente                                                          */
/* ========================================================================== */

typedef enum {
    TIPO_ENTIDADE_NPC,
    TIPO_ENTIDADE_BAU,
    TIPO_ENTIDADE_PORTAL,
    TIPO_ENTIDADE_INIMIGO
} TipoEntidade;

typedef enum {
    COMANDO_TEXTO,
    COMANDO_ESCOLHA,
    COMANDO_FLAG,
    COMANDO_VARIAVEL,
    COMANDO_ITEM,
    COMANDO_TELEPORTE,
    COMANDO_BATALHA,
    COMANDO_CURAR,
    COMANDO_AUDIO,
    COMANDO_ESPERAR,
    COMANDO_SE,
    COMANDO_SENAO,
    COMANDO_FIM_BLOCO,
    COMANDO_REPETIR,
    COMANDO_CHAMAR_EVENTO,
    COMANDO_LOJA,
    COMANDO_MISSAO
} TipoComando;

typedef enum { RECURSO_BMP, RECURSO_WAV, RECURSO_QOI, RECURSO_TGA } TipoRecurso;

typedef struct {
    int tipo;
    int a, b, c;
    int profundidade;
    char texto[160];
} ComandoEvento;

typedef struct {
    char nome[32];
    int primeiro_quadro, quantidade_quadros, duracao_ms, repetir, direcoes;
} ClipeAnimacao;

typedef struct {
    Identificador id;
    int ativo;
    char nome[48];
    int condicao_flag;
    int condicao_valor;
    int quantidade_comandos;
    int capacidade_comandos;
    ComandoEvento *comandos;
} Evento;

typedef struct {
    Identificador id;
    int ativo;
    char nome[48];
    int tipo;
    int x, y;
    int evento;
    Identificador evento_id;
    int cor;
} Entidade;

typedef struct {
    Identificador id;
    char nome[48];
    int visivel;
    int bloqueada;
    uint16_t *tiles;
} CamadaMapa;

typedef struct {
    Identificador id;
    char nome[48];
    int largura, altura;
    int quantidade_camadas;
    CamadaMapa camadas[MYTHARA_MAX_CAMADAS];
    uint8_t *colisoes;
    int quantidade_entidades;
    int capacidade_entidades;
    Entidade *entidades;
    int regiao_encontro;
} Mapa;

typedef struct {
    Identificador id;
    char nome[48];
    char descricao[120];
    int valor;
    int cura;
    int tipo_equipamento;
    int ataque, defesa, magia, resistencia, velocidade;
} Item;

typedef struct {
    Identificador id;
    char nome[48];
    int vida, magia, ataque, defesa, poder_magico, resistencia, velocidade, sorte;
    int experiencia, ouro;
} Inimigo;

typedef struct {
    Identificador id;
    char nome[48];
    int classe;
    int vida_maxima, magia_maxima, ataque, defesa, poder_magico, resistencia, velocidade, sorte;
    int sprite;
} Heroi;

typedef struct {
    Identificador id;
    char nome[48];
    int vida_base, magia_base, ataque_base, defesa_base, poder_magico_base, resistencia_base,
        velocidade_base, sorte_base;
    int habilidade_por_nivel[32];
} Classe;

typedef struct {
    Identificador id;
    char nome[48], descricao[120];
    int custo_mp, poder, elemento, alvo, estado, animacao;
} Habilidade;

typedef struct {
    Identificador id;
    char nome[48];
    int duracao, dano_turno;
    int ataque_pct, defesa_pct, velocidade_pct;
} EstadoCombate;

typedef struct {
    Identificador id;
    char nome[48];
    int quantidade_itens;
    int itens[32];
    int precos[32];
} Loja;

typedef struct {
    char descricao[96];
    int tipo, alvo, quantidade;
} EtapaMissao;

typedef struct {
    Identificador id;
    char nome[48], descricao[160];
    int quantidade_etapas;
    EtapaMissao etapas[16];
    int recompensa_item, recompensa_quantidade, recompensa_ouro, recompensa_experiencia;
} Missao;

typedef struct {
    Identificador id;
    int ativo;
    int tipo;
    char nome[48];
    char caminho[MYTHARA_MAX_CAMINHO];
    int largura_quadro, altura_quadro;
    int quantidade_quadros;
    int duracao_quadro_ms;
    int quantidade_clipes;
    ClipeAnimacao clipes[16];
} Recurso;

typedef struct {
    Identificador proximo_id;
    char nome[64];
    char autor[64];
    int tamanho_tile;
    int batalha_lateral;
    int mapa_inicial;
    int inicio_x, inicio_y;
    int quantidade_mapas;
    int capacidade_mapas;
    Mapa *mapas;
    int quantidade_eventos;
    int capacidade_eventos;
    Evento *eventos;
    int quantidade_herois;
    Heroi herois[16];
    int quantidade_itens;
    int capacidade_itens;
    Item *itens;
    int quantidade_inimigos;
    int capacidade_inimigos;
    Inimigo *inimigos;
    int quantidade_recursos;
    int capacidade_recursos;
    Recurso *recursos;
    int quantidade_classes;
    Classe classes[32];
    int quantidade_habilidades;
    Habilidade habilidades[128];
    int quantidade_estados;
    EstadoCombate estados[64];
    int quantidade_lojas;
    Loja lojas[64];
    int quantidade_missoes;
    Missao missoes[128];
    char pasta_base[MYTHARA_MAX_CAMINHO];
} Projeto;

static int caminho_do_recurso(const Projeto *p, const char *caminho, char *destino,
                              size_t capacidade) {
    if (caminho_absoluto(caminho)) {
        copiar_texto(destino, capacidade, caminho);
        return 1;
    }
    return juntar_caminho(destino, capacidade, p->pasta_base[0] ? p->pasta_base : ".", caminho);
}

typedef struct {
    uint32_t mapa, camada, indice, anterior, novo;
} Edicao;

typedef struct {
    int heroi, nivel, vida, vida_maxima, magia, magia_maxima, ataque, defesa, poder_magico,
        resistencia, velocidade, sorte;
    int equipamentos[4];
    int estados[8], duracao_estados[8];
} MembroGrupo;

typedef struct {
    int mapa;
    int x, y;
    int vida, vida_maxima, ataque, defesa, nivel, experiencia, ouro;
    int magia, magia_maxima, poder_magico, resistencia, velocidade, sorte;
    int inventario[MYTHARA_MAX_ITENS];
    int equipamentos[4];
    int progresso_missoes[128];
    int estado_missoes[128];
    int quantidade_grupo;
    MembroGrupo grupo[4];
    int flags[MYTHARA_MAX_FLAGS];
    int variaveis[MYTHARA_MAX_VARIAVEIS];
} EstadoJogo;

static int reservar_memoria(void **ponteiro, int *capacidade, int necessario, size_t tamanho,
                            int limite) {
    if (necessario <= *capacidade)
        return 1;
    int nova = *capacidade ? *capacidade * 2 : 4;
    while (nova < necessario && nova < limite)
        nova *= 2;
    if (nova > limite)
        nova = limite;
    if (nova < necessario)
        return 0;
    void *p = realloc(*ponteiro, (size_t)nova * tamanho);
    if (!p)
        return 0;
    memset((char *)p + (size_t)(*capacidade) * tamanho, 0, (size_t)(nova - *capacidade) * tamanho);
    *ponteiro = p;
    *capacidade = nova;
    return 1;
}

static void liberar_mapa(Mapa *m) {
    for (int i = 0; i < MYTHARA_MAX_CAMADAS; ++i)
        free(m->camadas[i].tiles);
    free(m->colisoes);
    free(m->entidades);
    memset(m, 0, sizeof(*m));
}

static void liberar_projeto(Projeto *p) {
    if (!p)
        return;
    for (int i = 0; i < p->quantidade_mapas; ++i)
        liberar_mapa(&p->mapas[i]);
    for (int i = 0; i < p->quantidade_eventos; ++i)
        free(p->eventos[i].comandos);
    free(p->mapas);
    free(p->eventos);
    free(p->itens);
    free(p->inimigos);
    free(p->recursos);
    memset(p, 0, sizeof(*p));
}

static int iniciar_mapa(Mapa *m, const char *nome, int largura, int altura) {
    memset(m, 0, sizeof(*m));
    m->largura = largura;
    m->altura = altura;
    m->quantidade_camadas = 3;
    m->regiao_encontro = -1;
    copiar_texto(m->nome, sizeof(m->nome), nome);
    size_t total = (size_t)largura * (size_t)altura;
    m->colisoes = calloc(total, 1);
    const char *nomes[] = {"Chao", "Decoracao", "Objetos"};
    for (int i = 0; i < 3; ++i) {
        copiar_texto(m->camadas[i].nome, sizeof(m->camadas[i].nome), nomes[i]);
        m->camadas[i].visivel = 1;
        m->camadas[i].tiles = calloc(total, sizeof(uint16_t));
    }
    return m->colisoes && m->camadas[0].tiles && m->camadas[1].tiles && m->camadas[2].tiles;
}

static int reservar_entidades(Mapa *m, int necessario) {
    return reservar_memoria((void **)&m->entidades, &m->capacidade_entidades, necessario,
                            sizeof(Entidade), MYTHARA_MAX_ENTIDADES);
}
static int reservar_comandos(Evento *e, int necessario) {
    return reservar_memoria((void **)&e->comandos, &e->capacidade_comandos, necessario,
                            sizeof(ComandoEvento), MYTHARA_MAX_COMANDOS);
}

static Identificador novo_identificador(Projeto *p) {
    if (p->proximo_id < 1)
        p->proximo_id = 1;
    return p->proximo_id++;
}

static int indice_evento_por_id(const Projeto *p, Identificador id) {
    for (int i = 0; i < p->quantidade_eventos; ++i)
        if (p->eventos[i].id == id)
            return i;
    return -1;
}

static void atribuir_ids_ausentes(Projeto *p) {
    if (p->proximo_id < 1)
        p->proximo_id = 1;
    for (int i = 0; i < p->quantidade_mapas; ++i) {
        Mapa *m = &p->mapas[i];
        if (!m->id)
            m->id = novo_identificador(p);
        for (int c = 0; c < m->quantidade_camadas; ++c)
            if (!m->camadas[c].id)
                m->camadas[c].id = novo_identificador(p);
    }
    for (int i = 0; i < p->quantidade_eventos; ++i)
        if (!p->eventos[i].id)
            p->eventos[i].id = novo_identificador(p);
    for (int i = 0; i < p->quantidade_mapas; ++i)
        for (int n = 0; n < p->mapas[i].quantidade_entidades; ++n) {
            Entidade *e = &p->mapas[i].entidades[n];
            if (!e->id)
                e->id = novo_identificador(p);
            if (e->evento_id) {
                int indice = indice_evento_por_id(p, e->evento_id);
                e->evento = indice;
            } else if (e->evento >= 0 && e->evento < p->quantidade_eventos)
                e->evento_id = p->eventos[e->evento].id;
            else
                e->evento = -1;
        }
    for (int i = 0; i < p->quantidade_herois; ++i)
        if (!p->herois[i].id)
            p->herois[i].id = novo_identificador(p);
    for (int i = 0; i < p->quantidade_classes; ++i)
        if (!p->classes[i].id)
            p->classes[i].id = novo_identificador(p);
    for (int i = 0; i < p->quantidade_habilidades; ++i)
        if (!p->habilidades[i].id)
            p->habilidades[i].id = novo_identificador(p);
    for (int i = 0; i < p->quantidade_itens; ++i)
        if (!p->itens[i].id)
            p->itens[i].id = novo_identificador(p);
    for (int i = 0; i < p->quantidade_inimigos; ++i)
        if (!p->inimigos[i].id)
            p->inimigos[i].id = novo_identificador(p);
    for (int i = 0; i < p->quantidade_estados; ++i)
        if (!p->estados[i].id)
            p->estados[i].id = novo_identificador(p);
    for (int i = 0; i < p->quantidade_lojas; ++i)
        if (!p->lojas[i].id)
            p->lojas[i].id = novo_identificador(p);
    for (int i = 0; i < p->quantidade_missoes; ++i)
        if (!p->missoes[i].id)
            p->missoes[i].id = novo_identificador(p);
    for (int i = 0; i < p->quantidade_recursos; ++i)
        if (!p->recursos[i].id)
            p->recursos[i].id = novo_identificador(p);
}

static void iniciar_projeto(Projeto *p) {
    Mapa *m;
    Evento *e;
    memset(p, 0, sizeof(*p));
    p->proximo_id = 1;
    copiar_texto(p->nome, sizeof(p->nome), "Minha Lenda");
    copiar_texto(p->autor, sizeof(p->autor), "Criado com Mythara");
    p->tamanho_tile = 32;
    p->batalha_lateral = 1;
    reservar_memoria((void **)&p->mapas, &p->capacidade_mapas, 1, sizeof(Mapa), MYTHARA_MAX_MAPAS);
    p->quantidade_mapas = 1;
    reservar_memoria((void **)&p->eventos, &p->capacidade_eventos, 3, sizeof(Evento),
                     MYTHARA_MAX_EVENTOS);
    p->quantidade_eventos = 3;
    reservar_memoria((void **)&p->itens, &p->capacidade_itens, 2, sizeof(Item), MYTHARA_MAX_ITENS);
    p->quantidade_itens = 2;
    reservar_memoria((void **)&p->inimigos, &p->capacidade_inimigos, 1, sizeof(Inimigo),
                     MYTHARA_MAX_INIMIGOS);
    p->quantidade_inimigos = 1;
    p->mapa_inicial = 0;
    p->inicio_x = 4;
    p->inicio_y = 4;
    m = &p->mapas[0];
    iniciar_mapa(m, "Vila Inicial", 24, 18);
    m->regiao_encontro = 0;
    reservar_entidades(m, 3);
    m->quantidade_entidades = 3;
    for (int y = 0; y < m->altura; ++y) {
        for (int x = 0; x < m->largura; ++x) {
            int i = y * m->largura + x;
            m->camadas[0].tiles[i] = (uint16_t)((x + y) % 7 == 0 ? 2 : 1);
            if (!x || !y || x == m->largura - 1 || y == m->altura - 1) {
                m->camadas[2].tiles[i] = 4;
                m->colisoes[i] = 1;
            }
        }
    }
    e = &p->eventos[0];
    e->ativo = 1;
    copiar_texto(e->nome, sizeof(e->nome), "Boas-vindas");
    e->condicao_flag = -1;
    reservar_comandos(e, 3);
    e->quantidade_comandos = 3;
    e->comandos[0].tipo = COMANDO_TEXTO;
    copiar_texto(e->comandos[0].texto, sizeof(e->comandos[0].texto),
                 "Bem-vindo a Mythara! Este mapa ja pode ser editado.");
    e->comandos[1].tipo = COMANDO_ITEM;
    e->comandos[1].a = 0;
    e->comandos[1].b = 1;
    e->comandos[2].tipo = COMANDO_MISSAO;
    e->comandos[2].a = 0;
    e->comandos[2].b = 1;
    e->comandos[2].c = 0;
    m->entidades[0].ativo = 1;
    m->entidades[0].tipo = TIPO_ENTIDADE_NPC;
    m->entidades[0].x = 7;
    m->entidades[0].y = 6;
    m->entidades[0].evento = 0;
    m->entidades[0].cor = 9;
    copiar_texto(m->entidades[0].nome, sizeof(m->entidades[0].nome), "Guardia");
    e = &p->eventos[1];
    e->ativo = 1;
    copiar_texto(e->nome, sizeof(e->nome), "Encontro com slime");
    e->condicao_flag = -1;
    reservar_comandos(e, 2);
    e->quantidade_comandos = 2;
    e->comandos[0].tipo = COMANDO_TEXTO;
    copiar_texto(e->comandos[0].texto, sizeof(e->comandos[0].texto),
                 "Um slime bloqueia o caminho!");
    e->comandos[1].tipo = COMANDO_BATALHA;
    e->comandos[1].a = 0;
    m->entidades[1].ativo = 1;
    m->entidades[1].tipo = TIPO_ENTIDADE_INIMIGO;
    m->entidades[1].x = 12;
    m->entidades[1].y = 8;
    m->entidades[1].evento = 1;
    m->entidades[1].cor = 2;
    copiar_texto(m->entidades[1].nome, sizeof(m->entidades[1].nome), "Slime");
    e = &p->eventos[2];
    e->ativo = 1;
    e->condicao_flag = -1;
    copiar_texto(e->nome, sizeof(e->nome), "Mercador");
    reservar_comandos(e, 2);
    e->quantidade_comandos = 2;
    e->comandos[0].tipo = COMANDO_TEXTO;
    copiar_texto(e->comandos[0].texto, sizeof(e->comandos[0].texto), "Veja minhas mercadorias!");
    e->comandos[1].tipo = COMANDO_LOJA;
    e->comandos[1].a = 0;
    m->entidades[2].ativo = 1;
    m->entidades[2].tipo = TIPO_ENTIDADE_NPC;
    m->entidades[2].x = 9;
    m->entidades[2].y = 10;
    m->entidades[2].evento = 2;
    m->entidades[2].cor = 10;
    copiar_texto(m->entidades[2].nome, sizeof(m->entidades[2].nome), "Mercador");
    p->quantidade_herois = 2;
    copiar_texto(p->herois[0].nome, sizeof(p->herois[0].nome), "Lume");
    p->herois[0].vida_maxima = 30;
    p->herois[0].magia_maxima = 12;
    p->herois[0].ataque = 8;
    p->herois[0].defesa = 3;
    p->herois[0].poder_magico = 5;
    p->herois[0].resistencia = 3;
    p->herois[0].velocidade = 6;
    p->herois[0].sorte = 4;
    copiar_texto(p->herois[1].nome, sizeof(p->herois[1].nome), "Nara");
    p->herois[1].vida_maxima = 24;
    p->herois[1].magia_maxima = 20;
    p->herois[1].ataque = 5;
    p->herois[1].defesa = 2;
    p->herois[1].poder_magico = 9;
    p->herois[1].resistencia = 5;
    p->herois[1].velocidade = 8;
    p->herois[1].sorte = 6;
    copiar_texto(p->itens[0].nome, sizeof(p->itens[0].nome), "Pocao");
    copiar_texto(p->itens[0].descricao, sizeof(p->itens[0].descricao),
                 "Recupera 15 pontos de vida.");
    p->itens[0].valor = 10;
    p->itens[0].cura = 15;
    copiar_texto(p->itens[1].nome, sizeof(p->itens[1].nome), "Espada de Cobre");
    copiar_texto(p->itens[1].descricao, sizeof(p->itens[1].descricao),
                 "Arma simples que concede +3 de ataque.");
    p->itens[1].valor = 25;
    p->itens[1].tipo_equipamento = 1;
    p->itens[1].ataque = 3;
    copiar_texto(p->inimigos[0].nome, sizeof(p->inimigos[0].nome), "Slime Verde");
    p->inimigos[0].vida = 18;
    p->inimigos[0].magia = 0;
    p->inimigos[0].ataque = 5;
    p->inimigos[0].defesa = 1;
    p->inimigos[0].experiencia = 8;
    p->inimigos[0].ouro = 4;
    p->inimigos[0].velocidade = 4;
    p->quantidade_classes = 1;
    copiar_texto(p->classes[0].nome, sizeof(p->classes[0].nome), "Aventureiro");
    p->classes[0].vida_base = 30;
    p->classes[0].magia_base = 12;
    p->classes[0].ataque_base = 8;
    p->classes[0].defesa_base = 3;
    p->classes[0].poder_magico_base = 5;
    p->classes[0].resistencia_base = 3;
    p->classes[0].velocidade_base = 6;
    p->classes[0].sorte_base = 4;
    p->quantidade_habilidades = 1;
    copiar_texto(p->habilidades[0].nome, sizeof(p->habilidades[0].nome), "Luz");
    copiar_texto(p->habilidades[0].descricao, sizeof(p->habilidades[0].descricao),
                 "Dano magico leve em um inimigo.");
    p->habilidades[0].custo_mp = 3;
    p->habilidades[0].poder = 9;
    p->quantidade_lojas = 1;
    copiar_texto(p->lojas[0].nome, sizeof(p->lojas[0].nome), "Mercado da Vila");
    p->lojas[0].quantidade_itens = 2;
    p->lojas[0].itens[0] = 0;
    p->lojas[0].precos[0] = 10;
    p->lojas[0].itens[1] = 1;
    p->lojas[0].precos[1] = 25;
    p->quantidade_missoes = 1;
    copiar_texto(p->missoes[0].nome, sizeof(p->missoes[0].nome), "Primeiros passos");
    copiar_texto(p->missoes[0].descricao, sizeof(p->missoes[0].descricao),
                 "Converse com o guardia e derrote o slime.");
    p->missoes[0].quantidade_etapas = 1;
    copiar_texto(p->missoes[0].etapas[0].descricao, sizeof(p->missoes[0].etapas[0].descricao),
                 "Derrote um slime");
    p->missoes[0].etapas[0].tipo = 1;
    p->missoes[0].etapas[0].alvo = 0;
    p->missoes[0].etapas[0].quantidade = 1;
    p->missoes[0].recompensa_ouro = 20;
    atribuir_ids_ausentes(p);
}

static void iniciar_estado_jogo(const Projeto *p, EstadoJogo *j) {
    memset(j, 0, sizeof(*j));
    j->mapa = limitar_int(p->mapa_inicial, 0, p->quantidade_mapas - 1);
    j->x = p->inicio_x;
    j->y = p->inicio_y;
    const Heroi *principal = &p->herois[0];
    j->vida_maxima = principal->vida_maxima;
    j->vida = j->vida_maxima;
    j->ataque = principal->ataque;
    j->defesa = principal->defesa;
    j->magia_maxima = principal->magia_maxima ? principal->magia_maxima : 12;
    j->magia = j->magia_maxima;
    j->poder_magico = principal->poder_magico ? principal->poder_magico : 5;
    j->resistencia = principal->resistencia ? principal->resistencia : 3;
    j->velocidade = principal->velocidade ? principal->velocidade : 6;
    j->sorte = principal->sorte ? principal->sorte : 4;
    j->quantidade_grupo = limitar_int(p->quantidade_herois, 1, 4);
    for (int i = 0; i < j->quantidade_grupo; ++i) {
        const Heroi *h = &p->herois[i];
        MembroGrupo *g = &j->grupo[i];
        g->heroi = i;
        g->nivel = 1;
        g->vida_maxima = h->vida_maxima;
        g->vida = g->vida_maxima;
        g->magia_maxima = h->magia_maxima;
        g->magia = g->magia_maxima;
        g->ataque = h->ataque;
        g->defesa = h->defesa;
        g->poder_magico = h->poder_magico;
        g->resistencia = h->resistencia;
        g->velocidade = h->velocidade;
        g->sorte = h->sorte;
        for (int s = 0; s < 4; ++s)
            g->equipamentos[s] = -1;
    }
    j->nivel = 1;
}

/* ========================================================================== */
/* Renderizador por software e fonte bitmap                                   */
/* ========================================================================== */

static const Cor paleta_tiles[16] = {0xff151923u, 0xff4b9b4bu, 0xff65ad58u, 0xffc59b55u,
                                     0xff5a4333u, 0xff4078a8u, 0xff79b6d2u, 0xffb0b6bfu,
                                     0xff704c93u, 0xffd06565u, 0xffe0b85a,  0xffefe3c2u,
                                     0xff3c5c3fu, 0xff865e3fu, 0xff53606fu, 0xffd889beu};

static void limpar_tela(Tela *t, Cor cor) {
    size_t total = (size_t)t->largura * (size_t)t->altura;
    for (size_t i = 0; i < total; ++i)
        t->pixels[i] = cor;
}

static void preencher_retangulo(Tela *t, Retangulo r, Cor cor) {
    int x0 = limitar_int(r.x, 0, t->largura);
    int y0 = limitar_int(r.y, 0, t->altura);
    int x1 = limitar_int(r.x + r.largura, 0, t->largura);
    int y1 = limitar_int(r.y + r.altura, 0, t->altura);
    for (int y = y0; y < y1; ++y) {
        uint32_t *linha = t->pixels + (size_t)y * (size_t)t->largura;
        for (int x = x0; x < x1; ++x)
            linha[x] = cor;
    }
}

static void contornar_retangulo(Tela *t, Retangulo r, Cor cor) {
    preencher_retangulo(t, (Retangulo){r.x, r.y, r.largura, 1}, cor);
    preencher_retangulo(t, (Retangulo){r.x, r.y + r.altura - 1, r.largura, 1}, cor);
    preencher_retangulo(t, (Retangulo){r.x, r.y, 1, r.altura}, cor);
    preencher_retangulo(t, (Retangulo){r.x + r.largura - 1, r.y, 1, r.altura}, cor);
}

static int redimensionar_tela_memoria(Tela *t, int largura, int altura) {
    if (t->pixels && t->largura == largura && t->altura == altura)
        return 1;
    uint32_t *pixels = realloc(t->pixels, (size_t)largura * altura * sizeof(uint32_t));
    if (!pixels)
        return 0;
    t->pixels = pixels;
    t->largura = largura;
    t->altura = altura;
    return 1;
}

static void copiar_tela_escalada(const Tela *origem, Tela *destino) {
    for (int y = 0; y < destino->altura; ++y) {
        int sy = y * origem->altura / destino->altura;
        for (int x = 0; x < destino->largura; ++x) {
            int sx = x * origem->largura / destino->largura;
            destino->pixels[(size_t)y * destino->largura + x] =
                origem->pixels[(size_t)sy * origem->largura + sx];
        }
    }
}

/* Cada linha usa cinco bits. Letras minusculas compartilham os glifos maiores. */
static const uint8_t fonte_digitos[10][7] = {
    {14, 17, 19, 21, 25, 17, 14}, {4, 12, 4, 4, 4, 4, 14},  {14, 17, 1, 2, 4, 8, 31},
    {30, 1, 1, 14, 1, 1, 30},     {2, 6, 10, 18, 31, 2, 2}, {31, 16, 16, 30, 1, 1, 30},
    {14, 16, 16, 30, 17, 17, 14}, {31, 1, 2, 4, 8, 8, 8},   {14, 17, 17, 14, 17, 17, 14},
    {14, 17, 17, 15, 1, 1, 14}};

static const uint8_t fonte_letras[26][7] = {
    {14, 17, 17, 31, 17, 17, 17}, {30, 17, 17, 30, 17, 17, 30}, {14, 17, 16, 16, 16, 17, 14},
    {30, 17, 17, 17, 17, 17, 30}, {31, 16, 16, 30, 16, 16, 31}, {31, 16, 16, 30, 16, 16, 16},
    {14, 17, 16, 23, 17, 17, 15}, {17, 17, 17, 31, 17, 17, 17}, {14, 4, 4, 4, 4, 4, 14},
    {7, 2, 2, 2, 18, 18, 12},     {17, 18, 20, 24, 20, 18, 17}, {16, 16, 16, 16, 16, 16, 31},
    {17, 27, 21, 21, 17, 17, 17}, {17, 25, 21, 19, 17, 17, 17}, {14, 17, 17, 17, 17, 17, 14},
    {30, 17, 17, 30, 16, 16, 16}, {14, 17, 17, 17, 21, 18, 13}, {30, 17, 17, 30, 20, 18, 17},
    {15, 16, 16, 14, 1, 1, 30},   {31, 4, 4, 4, 4, 4, 4},       {17, 17, 17, 17, 17, 17, 14},
    {17, 17, 17, 17, 17, 10, 4},  {17, 17, 17, 21, 21, 21, 10}, {17, 17, 10, 4, 10, 17, 17},
    {17, 17, 10, 4, 4, 4, 4},     {31, 1, 2, 4, 8, 16, 31}};

static void glifo_para(char c, uint8_t linhas[7]) {
    memset(linhas, 0, 7);
    if (c >= 'a' && c <= 'z')
        c = (char)(c - 'a' + 'A');
    if (c >= 'A' && c <= 'Z') {
        memcpy(linhas, fonte_letras[c - 'A'], 7);
        return;
    }
    if (c >= '0' && c <= '9') {
        memcpy(linhas, fonte_digitos[c - '0'], 7);
        return;
    }
    switch (c) {
    case '.':
        linhas[6] = 4;
        break;
    case ',':
        linhas[5] = 4;
        linhas[6] = 8;
        break;
    case ':':
        linhas[2] = 4;
        linhas[5] = 4;
        break;
    case ';':
        linhas[2] = 4;
        linhas[5] = 4;
        linhas[6] = 8;
        break;
    case '!':
        linhas[0] = 4;
        linhas[1] = 4;
        linhas[2] = 4;
        linhas[3] = 4;
        linhas[5] = 4;
        break;
    case '?':
        linhas[0] = 14;
        linhas[1] = 17;
        linhas[2] = 2;
        linhas[3] = 4;
        linhas[5] = 4;
        break;
    case '-':
        linhas[3] = 14;
        break;
    case '_':
        linhas[6] = 31;
        break;
    case '+':
        linhas[2] = 4;
        linhas[3] = 14;
        linhas[4] = 4;
        break;
    case '/':
        linhas[0] = 1;
        linhas[1] = 2;
        linhas[2] = 2;
        linhas[3] = 4;
        linhas[4] = 8;
        linhas[5] = 8;
        linhas[6] = 16;
        break;
    case '\\':
        linhas[0] = 16;
        linhas[1] = 8;
        linhas[2] = 8;
        linhas[3] = 4;
        linhas[4] = 2;
        linhas[5] = 2;
        linhas[6] = 1;
        break;
    case '(':
        linhas[0] = 2;
        linhas[1] = 4;
        linhas[2] = 8;
        linhas[3] = 8;
        linhas[4] = 8;
        linhas[5] = 4;
        linhas[6] = 2;
        break;
    case ')':
        linhas[0] = 8;
        linhas[1] = 4;
        linhas[2] = 2;
        linhas[3] = 2;
        linhas[4] = 2;
        linhas[5] = 4;
        linhas[6] = 8;
        break;
    case '<':
        linhas[1] = 2;
        linhas[2] = 4;
        linhas[3] = 8;
        linhas[4] = 4;
        linhas[5] = 2;
        break;
    case '>':
        linhas[1] = 8;
        linhas[2] = 4;
        linhas[3] = 2;
        linhas[4] = 4;
        linhas[5] = 8;
        break;
    case '=':
        linhas[2] = 14;
        linhas[4] = 14;
        break;
    case '"':
        linhas[0] = 10;
        linhas[1] = 10;
        break;
    case '#':
        linhas[1] = 10;
        linhas[2] = 31;
        linhas[3] = 10;
        linhas[4] = 31;
        linhas[5] = 10;
        break;
    default:
        break;
    }
}

static unsigned proximo_codigo_utf8(const char **texto) {
    const unsigned char *p = (const unsigned char *)*texto;
    unsigned c;
    if (*p < 128) {
        *texto += 1;
        return *p;
    }
    if ((*p & 0xe0) == 0xc0 && p[1]) {
        c = ((p[0] & 31u) << 6) | (p[1] & 63u);
        *texto += 2;
        return c;
    }
    if ((*p & 0xf0) == 0xe0 && p[1] && p[2]) {
        c = ((p[0] & 15u) << 12) | ((p[1] & 63u) << 6) | (p[2] & 63u);
        *texto += 3;
        return c;
    }
    *texto += 1;
    return '?';
}

static char simplificar_codigo(unsigned c) {
    if ((c >= 0x00c0 && c <= 0x00c5) || (c >= 0x00e0 && c <= 0x00e5))
        return 'A';
    if (c == 0x00c7 || c == 0x00e7)
        return 'C';
    if ((c >= 0x00c8 && c <= 0x00cb) || (c >= 0x00e8 && c <= 0x00eb))
        return 'E';
    if ((c >= 0x00cc && c <= 0x00cf) || (c >= 0x00ec && c <= 0x00ef))
        return 'I';
    if (c == 0x00d1 || c == 0x00f1)
        return 'N';
    if ((c >= 0x00d2 && c <= 0x00d6) || (c >= 0x00f2 && c <= 0x00f6))
        return 'O';
    if ((c >= 0x00d9 && c <= 0x00dc) || (c >= 0x00f9 && c <= 0x00fc))
        return 'U';
    return c < 128 ? (char)c : '?';
}

static void desenhar_texto(Tela *t, int x, int y, const char *texto, Cor cor, int escala) {
    int origem_x = x;
    while (*texto) {
        unsigned codigo = proximo_codigo_utf8(&texto);
        char c = simplificar_codigo(codigo);
        uint8_t linhas[7];
        if (c == '\n') {
            x = origem_x;
            y += 9 * escala;
            continue;
        }
        glifo_para(c, linhas);
        for (int l = 0; l < 7; ++l)
            for (int coluna = 0; coluna < 5; ++coluna)
                if (linhas[l] & (1u << (4 - coluna)))
                    preencher_retangulo(
                        t, (Retangulo){x + coluna * escala, y + l * escala, escala, escala}, cor);
        x += 6 * escala;
    }
}

static void desenhar_grade_tile(Tela *t, Retangulo r, int tile, int colisao) {
    Cor base = paleta_tiles[tile & 15];
    preencher_retangulo(t, r, base);
    if (tile == 2) {
        for (int y = 4; y < r.altura; y += 9)
            preencher_retangulo(t, (Retangulo){r.x + (y % 7), r.y + y, 2, 2}, cor_rgb(43, 120, 54));
    } else if (tile == 4) {
        preencher_retangulo(t, (Retangulo){r.x + 3, r.y + 3, r.largura - 6, r.altura - 6},
                            cor_rgb(54, 72, 48));
    } else if (tile == 5 || tile == 6) {
        for (int y = 3; y < r.altura; y += 7)
            preencher_retangulo(t, (Retangulo){r.x, r.y + y, r.largura, 1}, cor_rgb(115, 180, 206));
    }
    if (colisao) {
        for (int i = 0; i < r.largura; i += 8) {
            preencher_retangulo(t, (Retangulo){r.x + i, r.y + i % r.altura, 5, 2},
                                cor_rgb(225, 70, 78));
        }
        contornar_retangulo(t, r, cor_rgb(245, 78, 86));
    }
}

/* ========================================================================== */
/* Entrada e toolkit imediato                                                  */
/* ========================================================================== */

typedef struct {
    char nome[48];
    Cor fundo, painel, painel_elevado, controle, controle_sobre, selecao, borda, texto, texto_suave,
        destaque, perigo, aviso, sucesso;
    int escala_percentual, espacamento, altura_controle, espessura_borda, raio_visual;
} TemaInterface;

static TemaInterface tema_ativo;

static void iniciar_tema_padrao(void) {
    memset(&tema_ativo, 0, sizeof(tema_ativo));
    copiar_texto(tema_ativo.nome, sizeof(tema_ativo.nome), "Arcana Moderna");
    tema_ativo.fundo = cor_rgb(16, 19, 26);
    tema_ativo.painel = cor_rgb(27, 33, 48);
    tema_ativo.painel_elevado = cor_rgb(38, 45, 64);
    tema_ativo.controle = cor_rgb(48, 56, 78);
    tema_ativo.controle_sobre = cor_rgb(64, 72, 103);
    tema_ativo.selecao = cor_rgb(83, 76, 160);
    tema_ativo.borda = cor_rgb(75, 81, 112);
    tema_ativo.texto = cor_rgb(232, 234, 246);
    tema_ativo.texto_suave = cor_rgb(162, 169, 196);
    tema_ativo.destaque = cor_rgb(123, 115, 232);
    tema_ativo.perigo = cor_rgb(224, 91, 118);
    tema_ativo.aviso = cor_rgb(235, 188, 84);
    tema_ativo.sucesso = cor_rgb(91, 194, 155);
    tema_ativo.escala_percentual = 100;
    tema_ativo.espacamento = 6;
    tema_ativo.altura_controle = 27;
    tema_ativo.espessura_borda = 1;
    tema_ativo.raio_visual = 2;
}

typedef struct {
    int mouse_x, mouse_y;
    int mouse_baixo, mouse_pressionado, mouse_solto;
    int roda;
    int teclas[256];
    int teclas_pressionadas[256];
    char texto[32];
    int tamanho_texto;
    int controle, shift;
} Entrada;

typedef struct {
    Tela *tela;
    Entrada *entrada;
    int id_quente;
    int id_ativo;
    int id_foco;
    int proximo_id;
    const char *rotulo_quente;
} Interface;

typedef struct {
    int foco;
    size_t cursor, ancora, rolagem;
} EstadoCampoTexto;
static EstadoCampoTexto estado_campo_texto;
static int plataforma_clipboard_copiar(const char *texto);
static int plataforma_clipboard_colar(char *destino, size_t capacidade);

static int ui_novo_id(Interface *ui) {
    return ++ui->proximo_id;
}

static int ui_botao(Interface *ui, Retangulo r, const char *rotulo, int selecionado) {
    int id = ui_novo_id(ui);
    int sobre = ponto_em_retangulo(ui->entrada->mouse_x, ui->entrada->mouse_y, r);
    Cor fundo = selecionado ? tema_ativo.selecao : tema_ativo.controle;
    if (sobre) {
        ui->id_quente = id;
        ui->rotulo_quente = rotulo;
        fundo = tema_ativo.controle_sobre;
    }
    if (sobre && ui->entrada->mouse_pressionado)
        ui->id_ativo = id;
    preencher_retangulo(ui->tela, r, fundo);
    contornar_retangulo(ui->tela, r, sobre ? tema_ativo.destaque : tema_ativo.borda);
    desenhar_texto(ui->tela, r.x + 7, r.y + (r.altura - 7) / 2, rotulo, tema_ativo.texto, 1);
    return sobre && ui->entrada->mouse_pressionado;
}

static size_t utf8_anterior(const char *texto, size_t pos) {
    if (!pos)
        return 0;
    pos--;
    while (pos > 0 && ((unsigned char)texto[pos] & 0xc0) == 0x80)
        pos--;
    return pos;
}
static size_t utf8_proximo(const char *texto, size_t pos) {
    if (!texto[pos])
        return pos;
    pos++;
    while (texto[pos] && ((unsigned char)texto[pos] & 0xc0) == 0x80)
        pos++;
    return pos;
}
static int utf8_quantidade(const char *texto, size_t inicio, size_t fim) {
    int n = 0;
    for (size_t p = inicio; p < fim && texto[p]; p = utf8_proximo(texto, p))
        n++;
    return n;
}
static void campo_apagar_selecao(char *texto, size_t *cursor, size_t *ancora) {
    size_t a = *cursor < *ancora ? *cursor : *ancora, b = *cursor > *ancora ? *cursor : *ancora;
    if (a != b) {
        memmove(texto + a, texto + b, strlen(texto + b) + 1);
        *cursor = *ancora = a;
    }
}
static void campo_inserir(char *texto, size_t capacidade, size_t *cursor, size_t *ancora,
                          const char *dados, size_t tamanho) {
    campo_apagar_selecao(texto, cursor, ancora);
    size_t n = strlen(texto);
    if (tamanho > capacidade - 1 - n)
        tamanho = capacidade - 1 - n;
    if (!tamanho)
        return;
    memmove(texto + *cursor + tamanho, texto + *cursor, n - *cursor + 1);
    memcpy(texto + *cursor, dados, tamanho);
    *cursor += tamanho;
    *ancora = *cursor;
}

static int ui_campo_base(Interface *ui, Retangulo r, char *texto, size_t capacidade,
                         int multilinha) {
    int id = ui_novo_id(ui);
    int sobre = ponto_em_retangulo(ui->entrada->mouse_x, ui->entrada->mouse_y, r);
    if (sobre && ui->entrada->mouse_pressionado) {
        ui->id_foco = id;
        estado_campo_texto.foco = id;
        estado_campo_texto.cursor = estado_campo_texto.ancora = strlen(texto);
        estado_campo_texto.rolagem = 0;
    }
    preencher_retangulo(ui->tela, r, tema_ativo.fundo);
    contornar_retangulo(ui->tela, r, ui->id_foco == id ? tema_ativo.destaque : tema_ativo.borda);
    if (ui->id_foco == id) {
        if (estado_campo_texto.foco != id) {
            estado_campo_texto.foco = id;
            estado_campo_texto.cursor = estado_campo_texto.ancora = strlen(texto);
            estado_campo_texto.rolagem = 0;
        }
        size_t *cursor = &estado_campo_texto.cursor, *ancora = &estado_campo_texto.ancora,
               n = strlen(texto);
        if (*cursor > n)
            *cursor = n;
        if (*ancora > n)
            *ancora = n;
        if (ui->entrada->controle && ui->entrada->teclas_pressionadas['A']) {
            *ancora = 0;
            *cursor = n;
        }
        if (ui->entrada->controle && ui->entrada->teclas_pressionadas['C'] && *cursor != *ancora) {
            size_t a = *cursor < *ancora ? *cursor : *ancora,
                   b = *cursor > *ancora ? *cursor : *ancora;
            char copia[2048];
            size_t tam = b - a;
            if (tam >= sizeof(copia))
                tam = sizeof(copia) - 1;
            memcpy(copia, texto + a, tam);
            copia[tam] = 0;
            plataforma_clipboard_copiar(copia);
        }
        if (ui->entrada->controle && ui->entrada->teclas_pressionadas['X'] && *cursor != *ancora) {
            size_t a = *cursor < *ancora ? *cursor : *ancora,
                   b = *cursor > *ancora ? *cursor : *ancora;
            char copia[2048];
            size_t tam = b - a;
            if (tam >= sizeof(copia))
                tam = sizeof(copia) - 1;
            memcpy(copia, texto + a, tam);
            copia[tam] = 0;
            plataforma_clipboard_copiar(copia);
            campo_apagar_selecao(texto, cursor, ancora);
        }
        if (ui->entrada->controle && ui->entrada->teclas_pressionadas['V']) {
            char cola[2048];
            if (plataforma_clipboard_colar(cola, sizeof(cola))) {
                if (!multilinha)
                    for (char *p = cola; *p; ++p)
                        if (*p == '\r' || *p == '\n')
                            *p = ' ';
                campo_inserir(texto, capacidade, cursor, ancora, cola, strlen(cola));
            }
        }
        if (ui->entrada->teclas_pressionadas[128]) {
            *cursor = utf8_anterior(texto, *cursor);
            if (!ui->entrada->shift)
                *ancora = *cursor;
        }
        if (ui->entrada->teclas_pressionadas[129]) {
            *cursor = utf8_proximo(texto, *cursor);
            if (!ui->entrada->shift)
                *ancora = *cursor;
        }
        if (ui->entrada->teclas_pressionadas[136]) {
            *cursor = 0;
            if (!ui->entrada->shift)
                *ancora = *cursor;
        }
        if (ui->entrada->teclas_pressionadas[137]) {
            *cursor = strlen(texto);
            if (!ui->entrada->shift)
                *ancora = *cursor;
        }
        if (ui->entrada->teclas_pressionadas[8]) {
            if (*cursor != *ancora)
                campo_apagar_selecao(texto, cursor, ancora);
            else if (*cursor) {
                size_t anterior = utf8_anterior(texto, *cursor);
                memmove(texto + anterior, texto + *cursor, strlen(texto + *cursor) + 1);
                *cursor = *ancora = anterior;
            }
        }
        if (ui->entrada->teclas_pressionadas[127]) {
            if (*cursor != *ancora)
                campo_apagar_selecao(texto, cursor, ancora);
            else if (texto[*cursor]) {
                size_t proximo = utf8_proximo(texto, *cursor);
                memmove(texto + *cursor, texto + proximo, strlen(texto + proximo) + 1);
            }
        }
        if (multilinha && ui->entrada->teclas_pressionadas[13])
            campo_inserir(texto, capacidade, cursor, ancora, "\n", 1);
        if (!ui->entrada->controle && ui->entrada->tamanho_texto > 0) {
            char entrada[32];
            int tam = 0;
            for (int i = 0; i < ui->entrada->tamanho_texto && tam < (int)sizeof(entrada); ++i) {
                unsigned char c = (unsigned char)ui->entrada->texto[i];
                if (c >= 32 && c != 127)
                    entrada[tam++] = (char)c;
            }
            campo_inserir(texto, capacidade, cursor, ancora, entrada, (size_t)tam);
        }
        int maximo = (r.largura - 10) / 6;
        if (maximo < 1)
            maximo = 1;
        while (utf8_quantidade(texto, estado_campo_texto.rolagem, *cursor) > maximo)
            estado_campo_texto.rolagem = utf8_proximo(texto, estado_campo_texto.rolagem);
        if (*cursor < estado_campo_texto.rolagem)
            estado_campo_texto.rolagem = *cursor;
        size_t inicio = estado_campo_texto.rolagem, fim = inicio;
        for (int i = 0; i < maximo && texto[fim] && (!multilinha || texto[fim] != '\n'); ++i)
            fim = utf8_proximo(texto, fim);
        char visivel[1024];
        size_t tamanho = fim - inicio;
        if (tamanho >= sizeof(visivel))
            tamanho = sizeof(visivel) - 1;
        memcpy(visivel, texto + inicio, tamanho);
        visivel[tamanho] = 0;
        if (*cursor != *ancora) {
            size_t a = *cursor < *ancora ? *cursor : *ancora,
                   b = *cursor > *ancora ? *cursor : *ancora;
            if (b > inicio && a < fim) {
                if (a < inicio)
                    a = inicio;
                if (b > fim)
                    b = fim;
                int x0 = r.x + 5 + utf8_quantidade(texto, inicio, a) * 6,
                    x1 = r.x + 5 + utf8_quantidade(texto, inicio, b) * 6;
                preencher_retangulo(ui->tela, (Retangulo){x0, r.y + 4, x1 - x0, 13},
                                    tema_ativo.selecao);
            }
        }
        desenhar_texto(ui->tela, r.x + 5, r.y + 7, visivel, tema_ativo.texto, 1);
        int cursor_x = r.x + 5 + utf8_quantidade(texto, inicio, *cursor) * 6;
        preencher_retangulo(ui->tela, (Retangulo){cursor_x, r.y + 5, 1, 11}, tema_ativo.texto);
        return 1;
    } else {
        char visivel[1024];
        copiar_texto(visivel, sizeof(visivel), texto);
        int maximo = (r.largura - 10) / 6;
        if ((int)strlen(visivel) > maximo)
            visivel[maximo > 0 ? maximo : 0] = 0;
        desenhar_texto(ui->tela, r.x + 5, r.y + 7, visivel, tema_ativo.texto, 1);
    }
    return 0;
}

static int ui_campo(Interface *ui, Retangulo r, char *texto, size_t capacidade) {
    return ui_campo_base(ui, r, texto, capacidade, 0);
}

static int ui_campo_multilinha(Interface *ui, Retangulo r, char *texto, size_t capacidade) {
    int mudou = ui_campo_base(ui, r, texto, capacidade, 1);
    if (strchr(texto, '\n')) {
        char copia[1024];
        copiar_texto(copia, sizeof(copia), texto);
        char *linha = strchr(copia, '\n');
        if (linha) {
            *linha++ = 0;
            desenhar_texto(ui->tela, r.x + 5, r.y + 20, linha, tema_ativo.texto_suave, 1);
        }
    }
    return mudou;
}

static int ui_numero(Interface *ui, Retangulo r, const char *rotulo, int valor, int minimo,
                     int maximo) {
    char texto[64];
    snprintf(texto, sizeof(texto), "%s: %d", rotulo, valor);
    if (ui_botao(ui, (Retangulo){r.x, r.y, 22, r.altura}, "-", 0))
        valor--;
    preencher_retangulo(ui->tela, (Retangulo){r.x + 23, r.y, r.largura - 46, r.altura},
                        tema_ativo.painel_elevado);
    desenhar_texto(ui->tela, r.x + 29, r.y + 7, texto, tema_ativo.texto, 1);
    if (ui_botao(ui, (Retangulo){r.x + r.largura - 22, r.y, 22, r.altura}, "+", 0))
        valor++;
    return limitar_int(valor, minimo, maximo);
}

static void ui_painel(Tela *t, Retangulo r, const char *titulo) {
    preencher_retangulo(t, r, tema_ativo.painel);
    contornar_retangulo(t, r, tema_ativo.borda);
    preencher_retangulo(t, (Retangulo){r.x, r.y, r.largura, 26}, tema_ativo.painel_elevado);
    desenhar_texto(t, r.x + 8, r.y + 9, titulo, tema_ativo.texto, 1);
}

static int ui_checkbox(Interface *ui, Retangulo r, const char *rotulo, int valor) {
    int id = ui_novo_id(ui);
    Retangulo caixa = {r.x, r.y, 18, 18};
    int sobre = ponto_em_retangulo(ui->entrada->mouse_x, ui->entrada->mouse_y, r);
    preencher_retangulo(ui->tela, caixa, tema_ativo.fundo);
    contornar_retangulo(ui->tela, caixa, sobre ? tema_ativo.destaque : tema_ativo.borda);
    if (valor) {
        preencher_retangulo(ui->tela, (Retangulo){caixa.x + 4, caixa.y + 4, 10, 10},
                            tema_ativo.destaque);
    }
    desenhar_texto(ui->tela, r.x + 25, r.y + 5, rotulo, tema_ativo.texto, 1);
    if (sobre && ui->entrada->mouse_pressionado) {
        ui->id_ativo = id;
        return !valor;
    }
    return valor;
}

static void ui_barra_progresso(Tela *t, Retangulo r, int valor, int maximo, Cor cor) {
    preencher_retangulo(t, r, tema_ativo.fundo);
    contornar_retangulo(t, r, tema_ativo.borda);
    if (maximo > 0)
        preencher_retangulo(t,
                            (Retangulo){r.x + 2, r.y + 2,
                                        (r.largura - 4) * limitar_int(valor, 0, maximo) / maximo,
                                        r.altura - 4},
                            cor);
}

/* ========================================================================== */
/* Plataforma X11                                                              */
/* ========================================================================== */

#ifndef _WIN32
static Display *exibidor_clipboard_x11;
typedef struct {
    Display *exibidor;
    Window janela;
    GC gc;
    Atom apagar_janela;
    XImage *imagem;
    Tela tela;
    Entrada entrada;
    int executando;
    int largura, altura;
} Plataforma;

static int plataforma_redimensionar(Plataforma *p, int largura, int altura) {
    if (largura < 640)
        largura = 640;
    if (altura < 480)
        altura = 480;
    if (p->imagem) {
        p->imagem->data = (char *)p->tela.pixels;
        XDestroyImage(p->imagem);
        p->imagem = NULL;
        p->tela.pixels = NULL;
    }
    p->tela.pixels = calloc((size_t)largura * (size_t)altura, sizeof(uint32_t));
    if (!p->tela.pixels)
        return 0;
    p->imagem = XCreateImage(p->exibidor, DefaultVisual(p->exibidor, DefaultScreen(p->exibidor)),
                             24, ZPixmap, 0, (char *)p->tela.pixels, (unsigned)largura,
                             (unsigned)altura, 32, 0);
    if (!p->imagem) {
        free(p->tela.pixels);
        p->tela.pixels = NULL;
        return 0;
    }
    p->largura = p->tela.largura = largura;
    p->altura = p->tela.altura = altura;
    return 1;
}

static int plataforma_iniciar(Plataforma *p) {
    memset(p, 0, sizeof(*p));
    p->exibidor = XOpenDisplay(NULL);
    if (!p->exibidor) {
        fprintf(stderr, "Mythara: nao foi possivel abrir o servidor X11.\n");
        return 0;
    }
    exibidor_clipboard_x11 = p->exibidor;
    int tela = DefaultScreen(p->exibidor);
    p->janela = XCreateSimpleWindow(p->exibidor, RootWindow(p->exibidor, tela), 0, 0,
                                    MYTHARA_LARGURA_INICIAL, MYTHARA_ALTURA_INICIAL, 0,
                                    BlackPixel(p->exibidor, tela), BlackPixel(p->exibidor, tela));
    XStoreName(p->exibidor, p->janela, "Mythara - Motor de JRPG");
    XSelectInput(p->exibidor, p->janela,
                 ExposureMask | KeyPressMask | KeyReleaseMask | ButtonPressMask |
                     ButtonReleaseMask | PointerMotionMask | StructureNotifyMask);
    p->apagar_janela = XInternAtom(p->exibidor, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(p->exibidor, p->janela, &p->apagar_janela, 1);
    p->gc = XCreateGC(p->exibidor, p->janela, 0, NULL);
    XMapWindow(p->exibidor, p->janela);
    if (!plataforma_redimensionar(p, MYTHARA_LARGURA_INICIAL, MYTHARA_ALTURA_INICIAL))
        return 0;
    p->executando = 1;
    return 1;
}

static int codigo_tecla(KeySym s) {
    if (s >= XK_a && s <= XK_z)
        return (int)(s - XK_a + 'A');
    if (s >= XK_A && s <= XK_Z)
        return (int)s;
    if (s >= XK_0 && s <= XK_9)
        return (int)s;
    if (s == XK_BackSpace)
        return 8;
    if (s == XK_Return)
        return 13;
    if (s == XK_Escape)
        return 27;
    if (s == XK_space)
        return 32;
    if (s == XK_Left)
        return 128;
    if (s == XK_Right)
        return 129;
    if (s == XK_Up)
        return 130;
    if (s == XK_Down)
        return 131;
    if (s == XK_F5)
        return 132;
    if (s == XK_F1)
        return 133;
    if (s == XK_F2)
        return 134;
    if (s == XK_Control_L || s == XK_Control_R)
        return 135;
    if (s == XK_Home)
        return 136;
    if (s == XK_End)
        return 137;
    if (s == XK_Delete)
        return 127;
    return 0;
}

static void plataforma_eventos(Plataforma *p) {
    Entrada *e = &p->entrada;
    memset(e->teclas_pressionadas, 0, sizeof(e->teclas_pressionadas));
    e->mouse_pressionado = e->mouse_solto = e->roda = e->tamanho_texto = 0;
    e->controle = e->teclas[135];
    e->shift = 0;
    while (XPending(p->exibidor)) {
        XEvent evento;
        XNextEvent(p->exibidor, &evento);
        if (evento.type == ClientMessage && (Atom)evento.xclient.data.l[0] == p->apagar_janela)
            p->executando = 0;
        else if (evento.type == ConfigureNotify &&
                 (evento.xconfigure.width != p->largura || evento.xconfigure.height != p->altura))
            plataforma_redimensionar(p, evento.xconfigure.width, evento.xconfigure.height);
        else if (evento.type == MotionNotify) {
            e->mouse_x = evento.xmotion.x;
            e->mouse_y = evento.xmotion.y;
        } else if (evento.type == ButtonPress) {
            e->mouse_x = evento.xbutton.x;
            e->mouse_y = evento.xbutton.y;
            if (evento.xbutton.button == Button1)
                e->mouse_baixo = e->mouse_pressionado = 1;
            if (evento.xbutton.button == Button4)
                e->roda = 1;
            if (evento.xbutton.button == Button5)
                e->roda = -1;
        } else if (evento.type == ButtonRelease) {
            e->mouse_x = evento.xbutton.x;
            e->mouse_y = evento.xbutton.y;
            if (evento.xbutton.button == Button1) {
                e->mouse_baixo = 0;
                e->mouse_solto = 1;
            }
        } else if (evento.type == KeyPress || evento.type == KeyRelease) {
            KeySym simbolo = NoSymbol;
            char buffer[16];
            int n = XLookupString(&evento.xkey, buffer, sizeof(buffer), &simbolo, NULL);
            int c = codigo_tecla(simbolo);
            int baixo = evento.type == KeyPress;
            e->controle = (evento.xkey.state & ControlMask) != 0;
            e->shift = (evento.xkey.state & ShiftMask) != 0;
            if (c >= 0 && c < 256) {
                if (baixo && !e->teclas[c])
                    e->teclas_pressionadas[c] = 1;
                e->teclas[c] = baixo;
            }
            if (baixo && n > 0 && e->tamanho_texto + n < (int)sizeof(e->texto)) {
                memcpy(e->texto + e->tamanho_texto, buffer, (size_t)n);
                e->tamanho_texto += n;
            }
        }
    }
}

static void plataforma_apresentar(Plataforma *p) {
    XPutImage(p->exibidor, p->janela, p->gc, p->imagem, 0, 0, 0, 0, (unsigned)p->largura,
              (unsigned)p->altura);
    XFlush(p->exibidor);
}

static void plataforma_encerrar(Plataforma *p) {
    if (!p->exibidor)
        return;
    if (p->imagem) {
        p->imagem->data = (char *)p->tela.pixels;
        XDestroyImage(p->imagem);
    }
    if (p->gc)
        XFreeGC(p->exibidor, p->gc);
    if (p->janela)
        XDestroyWindow(p->exibidor, p->janela);
    XCloseDisplay(p->exibidor);
    exibidor_clipboard_x11 = NULL;
    memset(p, 0, sizeof(*p));
}
#else
typedef struct {
    HWND janela;
    BITMAPINFO bitmap;
    Tela tela;
    Entrada entrada;
    int executando;
    int largura, altura;
} Plataforma;

static Plataforma *plataforma_win32_ativa;

static int plataforma_redimensionar(Plataforma *p, int largura, int altura) {
    if (largura < 640)
        largura = 640;
    if (altura < 480)
        altura = 480;
    uint32_t *pixels = calloc((size_t)largura * (size_t)altura, sizeof(uint32_t));
    if (!pixels)
        return 0;
    free(p->tela.pixels);
    p->tela.pixels = pixels;
    p->largura = p->tela.largura = largura;
    p->altura = p->tela.altura = altura;
    memset(&p->bitmap, 0, sizeof(p->bitmap));
    p->bitmap.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    p->bitmap.bmiHeader.biWidth = largura;
    p->bitmap.bmiHeader.biHeight = -altura;
    p->bitmap.bmiHeader.biPlanes = 1;
    p->bitmap.bmiHeader.biBitCount = 32;
    p->bitmap.bmiHeader.biCompression = BI_RGB;
    return 1;
}

static int codigo_tecla_win32(WPARAM tecla) {
    if (tecla >= 'A' && tecla <= 'Z')
        return (int)tecla;
    if (tecla >= '0' && tecla <= '9')
        return (int)tecla;
    if (tecla == VK_BACK)
        return 8;
    if (tecla == VK_RETURN)
        return 13;
    if (tecla == VK_ESCAPE)
        return 27;
    if (tecla == VK_SPACE)
        return 32;
    if (tecla == VK_DELETE)
        return 127;
    if (tecla == VK_LEFT)
        return 128;
    if (tecla == VK_RIGHT)
        return 129;
    if (tecla == VK_UP)
        return 130;
    if (tecla == VK_DOWN)
        return 131;
    if (tecla == VK_F5)
        return 132;
    if (tecla == VK_F1)
        return 133;
    if (tecla == VK_F2)
        return 134;
    if (tecla == VK_CONTROL)
        return 135;
    if (tecla == VK_HOME)
        return 136;
    if (tecla == VK_END)
        return 137;
    return 0;
}

static LRESULT CALLBACK janela_mythara_proc(HWND janela, UINT mensagem, WPARAM wparam,
                                            LPARAM lparam) {
    Plataforma *p = plataforma_win32_ativa;
    Entrada *e = p ? &p->entrada : NULL;
    switch (mensagem) {
    case WM_CLOSE:
        if (p)
            p->executando = 0;
        DestroyWindow(janela);
        return 0;
    case WM_DESTROY:
        if (p)
            p->executando = 0;
        PostQuitMessage(0);
        return 0;
    case WM_SIZE:
        if (p && wparam != SIZE_MINIMIZED) {
            int w = LOWORD(lparam), h = HIWORD(lparam);
            if (w != p->largura || h != p->altura)
                plataforma_redimensionar(p, w, h);
        }
        return 0;
    case WM_MOUSEMOVE:
        if (e) {
            e->mouse_x = (short)LOWORD(lparam);
            e->mouse_y = (short)HIWORD(lparam);
        }
        return 0;
    case WM_LBUTTONDOWN:
        if (e) {
            SetCapture(janela);
            e->mouse_x = (short)LOWORD(lparam);
            e->mouse_y = (short)HIWORD(lparam);
            e->mouse_baixo = e->mouse_pressionado = 1;
        }
        return 0;
    case WM_LBUTTONUP:
        if (e) {
            ReleaseCapture();
            e->mouse_x = (short)LOWORD(lparam);
            e->mouse_y = (short)HIWORD(lparam);
            e->mouse_baixo = 0;
            e->mouse_solto = 1;
        }
        return 0;
    case WM_MOUSEWHEEL:
        if (e)
            e->roda = GET_WHEEL_DELTA_WPARAM(wparam) > 0 ? 1 : -1;
        return 0;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (e) {
            int c = codigo_tecla_win32(wparam);
            if (c && c < 256) {
                if (!e->teclas[c])
                    e->teclas_pressionadas[c] = 1;
                e->teclas[c] = 1;
            }
        }
        return 0;
    case WM_KEYUP:
    case WM_SYSKEYUP:
        if (e) {
            int c = codigo_tecla_win32(wparam);
            if (c && c < 256)
                e->teclas[c] = 0;
        }
        return 0;
    case WM_CHAR:
        if (e && wparam >= 32 && wparam != 127) {
            wchar_t wc = (wchar_t)wparam;
            char utf8[8];
            int n = WideCharToMultiByte(CP_UTF8, 0, &wc, 1, utf8, sizeof(utf8), NULL, NULL);
            if (n > 0 && e->tamanho_texto + n < (int)sizeof(e->texto)) {
                memcpy(e->texto + e->tamanho_texto, utf8, (size_t)n);
                e->tamanho_texto += n;
            }
        }
        return 0;
    default:
        return DefWindowProcW(janela, mensagem, wparam, lparam);
    }
}

static int plataforma_iniciar(Plataforma *p) {
    memset(p, 0, sizeof(*p));
    plataforma_win32_ativa = p;
    HINSTANCE instancia = GetModuleHandleW(NULL);
    WNDCLASSW classe = {0};
    classe.lpfnWndProc = janela_mythara_proc;
    classe.hInstance = instancia;
    classe.hCursor = LoadCursor(NULL, IDC_ARROW);
    classe.lpszClassName = L"MytharaJanelaV3";
    RegisterClassW(&classe);
    RECT r = {0, 0, MYTHARA_LARGURA_INICIAL, MYTHARA_ALTURA_INICIAL};
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    p->janela = CreateWindowW(classe.lpszClassName, L"Mythara 3 - Motor de JRPG",
                              WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
                              r.right - r.left, r.bottom - r.top, NULL, NULL, instancia, NULL);
    if (!p->janela)
        return 0;
    if (!plataforma_redimensionar(p, MYTHARA_LARGURA_INICIAL, MYTHARA_ALTURA_INICIAL))
        return 0;
    p->executando = 1;
    return 1;
}

static void plataforma_eventos(Plataforma *p) {
    Entrada *e = &p->entrada;
    memset(e->teclas_pressionadas, 0, sizeof(e->teclas_pressionadas));
    e->mouse_pressionado = e->mouse_solto = e->roda = e->tamanho_texto = 0;
    e->controle = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    e->shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    MSG m;
    while (PeekMessageW(&m, NULL, 0, 0, PM_REMOVE)) {
        if (m.message == WM_QUIT)
            p->executando = 0;
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
}

static void plataforma_apresentar(Plataforma *p) {
    HDC dc = GetDC(p->janela);
    StretchDIBits(dc, 0, 0, p->largura, p->altura, 0, 0, p->largura, p->altura, p->tela.pixels,
                  &p->bitmap, DIB_RGB_COLORS, SRCCOPY);
    ReleaseDC(p->janela, dc);
}
static void plataforma_encerrar(Plataforma *p) {
    if (p->janela && IsWindow(p->janela))
        DestroyWindow(p->janela);
    free(p->tela.pixels);
    memset(p, 0, sizeof(*p));
    plataforma_win32_ativa = NULL;
}
#endif

static int plataforma_clipboard_copiar(const char *texto) {
#ifdef _WIN32
    int n = MultiByteToWideChar(CP_UTF8, 0, texto, -1, NULL, 0);
    if (n <= 0 || !OpenClipboard(NULL))
        return 0;
    EmptyClipboard();
    HGLOBAL bloco = GlobalAlloc(GMEM_MOVEABLE, (size_t)n * sizeof(wchar_t));
    if (!bloco) {
        CloseClipboard();
        return 0;
    }
    wchar_t *p = GlobalLock(bloco);
    MultiByteToWideChar(CP_UTF8, 0, texto, -1, p, n);
    GlobalUnlock(bloco);
    if (!SetClipboardData(CF_UNICODETEXT, bloco)) {
        GlobalFree(bloco);
        CloseClipboard();
        return 0;
    }
    CloseClipboard();
    return 1;
#else
    if (!exibidor_clipboard_x11)
        return 0;
    XStoreBuffer(exibidor_clipboard_x11, texto, (int)strlen(texto), 0);
    XFlush(exibidor_clipboard_x11);
    return 1;
#endif
}

static int plataforma_clipboard_colar(char *destino, size_t capacidade) {
#ifdef _WIN32
    if (!OpenClipboard(NULL))
        return 0;
    HANDLE bloco = GetClipboardData(CF_UNICODETEXT);
    if (!bloco) {
        CloseClipboard();
        return 0;
    }
    const wchar_t *p = GlobalLock(bloco);
    int n = p ? WideCharToMultiByte(CP_UTF8, 0, p, -1, destino, (int)capacidade, NULL, NULL) : 0;
    if (p)
        GlobalUnlock(bloco);
    CloseClipboard();
    if (n <= 0) {
        if (capacidade)
            destino[0] = 0;
        return 0;
    }
    destino[capacidade - 1] = 0;
    return 1;
#else
    if (!exibidor_clipboard_x11)
        return 0;
    int n = 0;
    char *texto = XFetchBuffer(exibidor_clipboard_x11, &n, 0);
    if (!texto || n <= 0) {
        if (texto)
            XFree(texto);
        return 0;
    }
    size_t tam = (size_t)n;
    if (tam >= capacidade)
        tam = capacidade - 1;
    memcpy(destino, texto, tam);
    destino[tam] = 0;
    XFree(texto);
    return 1;
#endif
}

/* ========================================================================== */
/* Persistencia versionada                                                     */
/* ========================================================================== */

typedef struct {
    char magia[8];
    uint32_t versao, quantidade_blocos;
} CabecalhoProjeto;
typedef struct {
    char id[4];
    uint32_t tamanho, soma;
} CabecalhoBloco;
typedef struct {
    Identificador proximo_id;
    char nome[64], autor[64];
    int tamanho_tile, batalha_lateral, mapa_inicial, inicio_x, inicio_y;
    int quantidade_herois;
    Heroi herois[16];
    int quantidade_classes;
    Classe classes[32];
    int quantidade_habilidades;
    Habilidade habilidades[128];
    int quantidade_estados;
    EstadoCombate estados[64];
    int quantidade_lojas;
    Loja lojas[64];
    int quantidade_missoes;
    Missao missoes[128];
} DadosProjetoFixo;
typedef struct {
    Identificador id;
    char nome[48];
    int largura, altura, quantidade_camadas, quantidade_entidades, regiao_encontro;
} MapaDisco;
typedef struct {
    Identificador id;
    char nome[48];
    int visivel, bloqueada;
} CamadaDisco;
typedef struct {
    Identificador id;
    int ativo;
    char nome[48];
    int condicao_flag, condicao_valor, quantidade_comandos;
} EventoDisco;
typedef struct {
    uint8_t *dados;
    size_t tamanho, capacidade, posicao;
} BufferDados;

static uint32_t soma_fnv1a(const void *dados, size_t tamanho) {
    const uint8_t *p = (const uint8_t *)dados;
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < tamanho; ++i) {
        h ^= p[i];
        h *= 16777619u;
    }
    return h;
}

static int escrever_bloco(FILE *f, const char id[4], const void *dados, size_t tamanho) {
    CabecalhoBloco b;
    memcpy(b.id, id, 4);
    b.tamanho = (uint32_t)tamanho;
    b.soma = soma_fnv1a(dados, tamanho);
    return fwrite(&b, sizeof(b), 1, f) == 1 && fwrite(dados, tamanho, 1, f) == 1;
}

static int buffer_adicionar(BufferDados *b, const void *dados, size_t tamanho) {
    if (!tamanho)
        return 1;
    if (tamanho > SIZE_MAX - b->tamanho)
        return 0;
    size_t necessario = b->tamanho + tamanho;
    if (necessario > b->capacidade) {
        size_t nova = b->capacidade ? b->capacidade * 2 : 4096;
        while (nova < necessario) {
            if (nova > SIZE_MAX / 2) {
                nova = necessario;
                break;
            }
            nova *= 2;
        }
        void *p = realloc(b->dados, nova);
        if (!p)
            return 0;
        b->dados = p;
        b->capacidade = nova;
    }
    memcpy(b->dados + b->tamanho, dados, tamanho);
    b->tamanho = necessario;
    return 1;
}

static int buffer_ler(BufferDados *b, void *destino, size_t tamanho) {
    if (!tamanho)
        return 1;
    if (b->posicao > b->tamanho || tamanho > b->tamanho - b->posicao)
        return 0;
    memcpy(destino, b->dados + b->posicao, tamanho);
    b->posicao += tamanho;
    return 1;
}

static int serializar_projeto(const Projeto *p, BufferDados *b) {
    DadosProjetoFixo d;
    memset(&d, 0, sizeof(d));
    d.proximo_id = p->proximo_id;
    copiar_texto(d.nome, sizeof(d.nome), p->nome);
    copiar_texto(d.autor, sizeof(d.autor), p->autor);
    d.tamanho_tile = p->tamanho_tile;
    d.batalha_lateral = p->batalha_lateral;
    d.mapa_inicial = p->mapa_inicial;
    d.inicio_x = p->inicio_x;
    d.inicio_y = p->inicio_y;
    d.quantidade_herois = p->quantidade_herois;
    memcpy(d.herois, p->herois, sizeof(d.herois));
    d.quantidade_classes = p->quantidade_classes;
    memcpy(d.classes, p->classes, sizeof(d.classes));
    d.quantidade_habilidades = p->quantidade_habilidades;
    memcpy(d.habilidades, p->habilidades, sizeof(d.habilidades));
    d.quantidade_estados = p->quantidade_estados;
    memcpy(d.estados, p->estados, sizeof(d.estados));
    d.quantidade_lojas = p->quantidade_lojas;
    memcpy(d.lojas, p->lojas, sizeof(d.lojas));
    d.quantidade_missoes = p->quantidade_missoes;
    memcpy(d.missoes, p->missoes, sizeof(d.missoes));
    uint32_t contagens[5] = {(uint32_t)p->quantidade_mapas, (uint32_t)p->quantidade_eventos,
                             (uint32_t)p->quantidade_itens, (uint32_t)p->quantidade_inimigos,
                             (uint32_t)p->quantidade_recursos};
    if (!buffer_adicionar(b, &d, sizeof(d)) || !buffer_adicionar(b, contagens, sizeof(contagens)))
        return 0;
    if (!buffer_adicionar(b, p->itens, (size_t)p->quantidade_itens * sizeof(Item)) ||
        !buffer_adicionar(b, p->inimigos, (size_t)p->quantidade_inimigos * sizeof(Inimigo)) ||
        !buffer_adicionar(b, p->recursos, (size_t)p->quantidade_recursos * sizeof(Recurso)))
        return 0;
    for (int i = 0; i < p->quantidade_mapas; ++i) {
        const Mapa *m = &p->mapas[i];
        MapaDisco md;
        memset(&md, 0, sizeof(md));
        md.id = m->id;
        copiar_texto(md.nome, sizeof(md.nome), m->nome);
        md.largura = m->largura;
        md.altura = m->altura;
        md.quantidade_camadas = m->quantidade_camadas;
        md.quantidade_entidades = m->quantidade_entidades;
        md.regiao_encontro = m->regiao_encontro;
        if (!buffer_adicionar(b, &md, sizeof(md)))
            return 0;
        size_t total = (size_t)m->largura * m->altura;
        for (int c = 0; c < m->quantidade_camadas; ++c) {
            CamadaDisco cd;
            memset(&cd, 0, sizeof(cd));
            cd.id = m->camadas[c].id;
            copiar_texto(cd.nome, sizeof(cd.nome), m->camadas[c].nome);
            cd.visivel = m->camadas[c].visivel;
            cd.bloqueada = m->camadas[c].bloqueada;
            if (!buffer_adicionar(b, &cd, sizeof(cd)) ||
                !buffer_adicionar(b, m->camadas[c].tiles, total * sizeof(uint16_t)))
                return 0;
        }
        if (!buffer_adicionar(b, m->colisoes, total) ||
            !buffer_adicionar(b, m->entidades, (size_t)m->quantidade_entidades * sizeof(Entidade)))
            return 0;
    }
    for (int i = 0; i < p->quantidade_eventos; ++i) {
        const Evento *e = &p->eventos[i];
        EventoDisco ed;
        memset(&ed, 0, sizeof(ed));
        ed.id = e->id;
        ed.ativo = e->ativo;
        copiar_texto(ed.nome, sizeof(ed.nome), e->nome);
        ed.condicao_flag = e->condicao_flag;
        ed.condicao_valor = e->condicao_valor;
        ed.quantidade_comandos = e->quantidade_comandos;
        if (!buffer_adicionar(b, &ed, sizeof(ed)) ||
            !buffer_adicionar(b, e->comandos,
                              (size_t)e->quantidade_comandos * sizeof(ComandoEvento)))
            return 0;
    }
    return 1;
}

static int salvar_projeto_em(const Projeto *p, const char *caminho, char *erro, size_t cap_erro) {
    BufferDados b = {0};
    if (!serializar_projeto(p, &b)) {
        free(b.dados);
        snprintf(erro, cap_erro, "Memoria insuficiente para serializar o projeto.");
        return 0;
    }
    char temporario[MYTHARA_MAX_CAMINHO + 16];
    CabecalhoProjeto h = {{'M', 'Y', 'T', 'H', 'R', 'V', '3', '\0'}, MYTHARA_VERSAO, 1};
    snprintf(temporario, sizeof(temporario), "%s.tmp", caminho);
    FILE *f = fopen(temporario, "wb");
    if (!f) {
        free(b.dados);
        snprintf(erro, cap_erro, "Nao foi possivel salvar: %s", strerror(errno));
        return 0;
    }
    int ok = fwrite(&h, sizeof(h), 1, f) == 1 && b.tamanho <= UINT32_MAX &&
             escrever_bloco(f, "DADO", b.dados, b.tamanho);
    free(b.dados);
    if (fflush(f) != 0 || fsync(fileno(f)) != 0)
        ok = 0;
    if (fclose(f) != 0)
        ok = 0;
    if (!ok || rename(temporario, caminho) != 0) {
        unlink(temporario);
        snprintf(erro, cap_erro, "Falha ao concluir o arquivo do projeto.");
        return 0;
    }
    return 1;
}

static int salvar_buffer_projeto(const BufferDados *b, const char *caminho) {
    char temporario[MYTHARA_MAX_CAMINHO + 16];
    CabecalhoProjeto h = {{'M', 'Y', 'T', 'H', 'R', 'V', '3', '\0'}, MYTHARA_VERSAO, 1};
    snprintf(temporario, sizeof(temporario), "%s.tmp", caminho);
    FILE *f = fopen(temporario, "wb");
    if (!f)
        return 0;
    int ok = fwrite(&h, sizeof(h), 1, f) == 1 && b->tamanho <= UINT32_MAX &&
             escrever_bloco(f, "DADO", b->dados, b->tamanho);
    if (fflush(f) != 0 || fsync(fileno(f)) != 0)
        ok = 0;
    if (fclose(f) != 0)
        ok = 0;
    if (!ok || rename(temporario, caminho) != 0) {
        unlink(temporario);
        return 0;
    }
    return 1;
}

static int validar_projeto(Projeto *p, char *erro, size_t cap_erro) {
    if (p->quantidade_mapas < 1 || p->quantidade_mapas > MYTHARA_MAX_MAPAS)
        goto invalido;
    if (p->quantidade_eventos < 0 || p->quantidade_eventos > MYTHARA_MAX_EVENTOS)
        goto invalido;
    if (p->quantidade_itens < 0 || p->quantidade_itens > MYTHARA_MAX_ITENS)
        goto invalido;
    if (p->quantidade_inimigos < 0 || p->quantidade_inimigos > MYTHARA_MAX_INIMIGOS)
        goto invalido;
    if (p->quantidade_recursos < 0 || p->quantidade_recursos > MYTHARA_MAX_RECURSOS)
        goto invalido;
    p->mapa_inicial = limitar_int(p->mapa_inicial, 0, p->quantidade_mapas - 1);
    for (int i = 0; i < p->quantidade_mapas; ++i) {
        Mapa *m = &p->mapas[i];
        if (m->largura < 4 || m->largura > MYTHARA_MAX_LARGURA_MAPA || m->altura < 4 ||
            m->altura > MYTHARA_MAX_ALTURA_MAPA)
            goto invalido;
        if (m->quantidade_camadas < 1 || m->quantidade_camadas > MYTHARA_MAX_CAMADAS ||
            !m->colisoes)
            goto invalido;
        if (m->quantidade_entidades < 0 || m->quantidade_entidades > MYTHARA_MAX_ENTIDADES)
            goto invalido;
        for (int n = 0; n < m->quantidade_entidades; ++n) {
            m->entidades[n].x = limitar_int(m->entidades[n].x, 0, m->largura - 1);
            m->entidades[n].y = limitar_int(m->entidades[n].y, 0, m->altura - 1);
        }
    }
    return 1;
invalido:
    snprintf(erro, cap_erro, "O projeto contem limites ou dimensoes invalidas.");
    return 0;
}

static int carregar_projeto_de(Projeto *p, const char *caminho, char *erro, size_t cap_erro) {
    CabecalhoProjeto h;
    CabecalhoBloco bloco;
    Projeto novo;
    BufferDados b = {0};
    DadosProjetoFixo d;
    uint32_t contagens[5];
    FILE *f = fopen(caminho, "rb");
    if (!f) {
        snprintf(erro, cap_erro, "Nao foi possivel abrir: %s", strerror(errno));
        return 0;
    }
    memset(&novo, 0, sizeof(novo));
    int ok = fread(&h, sizeof(h), 1, f) == 1 && !memcmp(h.magia, "MYTHRV3", 7) &&
             h.versao == MYTHARA_VERSAO && h.quantidade_blocos == 1;
    if (!ok) {
        fclose(f);
        snprintf(erro, cap_erro, "Projeto incompativel: a Mythara 3 abre somente arquivos v3.");
        return 0;
    }
    if (fread(&bloco, sizeof(bloco), 1, f) != 1 || memcmp(bloco.id, "DADO", 4) ||
        bloco.tamanho > 512u * 1024u * 1024u) {
        fclose(f);
        snprintf(erro, cap_erro, "Bloco principal ausente ou grande demais.");
        return 0;
    }
    b.dados = malloc(bloco.tamanho);
    b.tamanho = b.capacidade = bloco.tamanho;
    if (!b.dados || fread(b.dados, b.tamanho, 1, f) != 1 ||
        soma_fnv1a(b.dados, b.tamanho) != bloco.soma) {
        free(b.dados);
        fclose(f);
        snprintf(erro, cap_erro, "Projeto truncado ou corrompido.");
        return 0;
    }
    fclose(f);
    ok = buffer_ler(&b, &d, sizeof(d)) && buffer_ler(&b, contagens, sizeof(contagens));
    if (!ok || contagens[0] < 1 || contagens[0] > MYTHARA_MAX_MAPAS ||
        contagens[1] > MYTHARA_MAX_EVENTOS || contagens[2] > MYTHARA_MAX_ITENS ||
        contagens[3] > MYTHARA_MAX_INIMIGOS || contagens[4] > MYTHARA_MAX_RECURSOS)
        goto falha;
    novo.proximo_id = d.proximo_id;
    copiar_texto(novo.nome, sizeof(novo.nome), d.nome);
    copiar_texto(novo.autor, sizeof(novo.autor), d.autor);
    novo.tamanho_tile = d.tamanho_tile;
    novo.batalha_lateral = d.batalha_lateral;
    novo.mapa_inicial = d.mapa_inicial;
    novo.inicio_x = d.inicio_x;
    novo.inicio_y = d.inicio_y;
    novo.quantidade_herois = limitar_int(d.quantidade_herois, 1, 16);
    memcpy(novo.herois, d.herois, sizeof(novo.herois));
    novo.quantidade_classes = limitar_int(d.quantidade_classes, 0, 32);
    memcpy(novo.classes, d.classes, sizeof(novo.classes));
    novo.quantidade_habilidades = limitar_int(d.quantidade_habilidades, 0, 128);
    memcpy(novo.habilidades, d.habilidades, sizeof(novo.habilidades));
    novo.quantidade_estados = limitar_int(d.quantidade_estados, 0, 64);
    memcpy(novo.estados, d.estados, sizeof(novo.estados));
    novo.quantidade_lojas = limitar_int(d.quantidade_lojas, 0, 64);
    memcpy(novo.lojas, d.lojas, sizeof(novo.lojas));
    novo.quantidade_missoes = limitar_int(d.quantidade_missoes, 0, 128);
    memcpy(novo.missoes, d.missoes, sizeof(novo.missoes));
    novo.quantidade_mapas = (int)contagens[0];
    novo.quantidade_eventos = (int)contagens[1];
    novo.quantidade_itens = (int)contagens[2];
    novo.quantidade_inimigos = (int)contagens[3];
    novo.quantidade_recursos = (int)contagens[4];
    if (!reservar_memoria((void **)&novo.mapas, &novo.capacidade_mapas, novo.quantidade_mapas,
                          sizeof(Mapa), MYTHARA_MAX_MAPAS) ||
        !reservar_memoria((void **)&novo.eventos, &novo.capacidade_eventos, novo.quantidade_eventos,
                          sizeof(Evento), MYTHARA_MAX_EVENTOS) ||
        !reservar_memoria((void **)&novo.itens, &novo.capacidade_itens, novo.quantidade_itens,
                          sizeof(Item), MYTHARA_MAX_ITENS) ||
        !reservar_memoria((void **)&novo.inimigos, &novo.capacidade_inimigos,
                          novo.quantidade_inimigos, sizeof(Inimigo), MYTHARA_MAX_INIMIGOS) ||
        !reservar_memoria((void **)&novo.recursos, &novo.capacidade_recursos,
                          novo.quantidade_recursos, sizeof(Recurso), MYTHARA_MAX_RECURSOS))
        goto falha;
    if (!buffer_ler(&b, novo.itens, (size_t)novo.quantidade_itens * sizeof(Item)) ||
        !buffer_ler(&b, novo.inimigos, (size_t)novo.quantidade_inimigos * sizeof(Inimigo)) ||
        !buffer_ler(&b, novo.recursos, (size_t)novo.quantidade_recursos * sizeof(Recurso)))
        goto falha;
    for (int i = 0; i < novo.quantidade_mapas; ++i) {
        MapaDisco md;
        if (!buffer_ler(&b, &md, sizeof(md)) || md.largura < 4 ||
            md.largura > MYTHARA_MAX_LARGURA_MAPA || md.altura < 4 ||
            md.altura > MYTHARA_MAX_ALTURA_MAPA || md.quantidade_camadas < 1 ||
            md.quantidade_camadas > MYTHARA_MAX_CAMADAS || md.quantidade_entidades < 0 ||
            md.quantidade_entidades > MYTHARA_MAX_ENTIDADES)
            goto falha;
        Mapa *m = &novo.mapas[i];
        memset(m, 0, sizeof(*m));
        m->id = md.id;
        copiar_texto(m->nome, sizeof(m->nome), md.nome);
        m->largura = md.largura;
        m->altura = md.altura;
        m->quantidade_camadas = md.quantidade_camadas;
        m->quantidade_entidades = md.quantidade_entidades;
        m->regiao_encontro = md.regiao_encontro;
        size_t total = (size_t)m->largura * m->altura;
        for (int c = 0; c < m->quantidade_camadas; ++c) {
            CamadaDisco cd;
            if (!buffer_ler(&b, &cd, sizeof(cd)))
                goto falha;
            m->camadas[c].id = cd.id;
            m->camadas[c].tiles = malloc(total * sizeof(uint16_t));
            if (!m->camadas[c].tiles ||
                !buffer_ler(&b, m->camadas[c].tiles, total * sizeof(uint16_t)))
                goto falha;
            copiar_texto(m->camadas[c].nome, sizeof(m->camadas[c].nome), cd.nome);
            m->camadas[c].visivel = cd.visivel;
            m->camadas[c].bloqueada = cd.bloqueada;
        }
        m->colisoes = malloc(total);
        if (!m->colisoes || !buffer_ler(&b, m->colisoes, total) ||
            !reservar_entidades(m, m->quantidade_entidades) ||
            !buffer_ler(&b, m->entidades, (size_t)m->quantidade_entidades * sizeof(Entidade)))
            goto falha;
    }
    for (int i = 0; i < novo.quantidade_eventos; ++i) {
        EventoDisco ed;
        if (!buffer_ler(&b, &ed, sizeof(ed)) || ed.quantidade_comandos < 0 ||
            ed.quantidade_comandos > MYTHARA_MAX_COMANDOS)
            goto falha;
        Evento *e = &novo.eventos[i];
        e->id = ed.id;
        e->ativo = ed.ativo;
        copiar_texto(e->nome, sizeof(e->nome), ed.nome);
        e->condicao_flag = ed.condicao_flag;
        e->condicao_valor = ed.condicao_valor;
        e->quantidade_comandos = ed.quantidade_comandos;
        if (!reservar_comandos(e, e->quantidade_comandos) ||
            !buffer_ler(&b, e->comandos, (size_t)e->quantidade_comandos * sizeof(ComandoEvento)))
            goto falha;
    }
    free(b.dados);
    atribuir_ids_ausentes(&novo);
    if (!validar_projeto(&novo, erro, cap_erro)) {
        liberar_projeto(&novo);
        return 0;
    }
    *p = novo;
    return 1;
falha:
    free(b.dados);
    liberar_projeto(&novo);
    snprintf(erro, cap_erro, "Estrutura v3 invalida ou memoria insuficiente.");
    return 0;
}

static int clonar_projeto(const Projeto *origem, Projeto *destino) {
    memset(destino, 0, sizeof(*destino));
    *destino = *origem;
    destino->mapas = NULL;
    destino->eventos = NULL;
    destino->itens = NULL;
    destino->inimigos = NULL;
    destino->recursos = NULL;
    destino->capacidade_mapas = destino->capacidade_eventos = destino->capacidade_itens =
        destino->capacidade_inimigos = destino->capacidade_recursos = 0;
    if (!reservar_memoria((void **)&destino->mapas, &destino->capacidade_mapas,
                          origem->quantidade_mapas, sizeof(Mapa), MYTHARA_MAX_MAPAS) ||
        !reservar_memoria((void **)&destino->eventos, &destino->capacidade_eventos,
                          origem->quantidade_eventos, sizeof(Evento), MYTHARA_MAX_EVENTOS) ||
        !reservar_memoria((void **)&destino->itens, &destino->capacidade_itens,
                          origem->quantidade_itens, sizeof(Item), MYTHARA_MAX_ITENS) ||
        !reservar_memoria((void **)&destino->inimigos, &destino->capacidade_inimigos,
                          origem->quantidade_inimigos, sizeof(Inimigo), MYTHARA_MAX_INIMIGOS) ||
        !reservar_memoria((void **)&destino->recursos, &destino->capacidade_recursos,
                          origem->quantidade_recursos, sizeof(Recurso), MYTHARA_MAX_RECURSOS))
        goto falha_clone;
    if (origem->quantidade_itens)
        memcpy(destino->itens, origem->itens, (size_t)origem->quantidade_itens * sizeof(Item));
    if (origem->quantidade_inimigos)
        memcpy(destino->inimigos, origem->inimigos,
               (size_t)origem->quantidade_inimigos * sizeof(Inimigo));
    if (origem->quantidade_recursos)
        memcpy(destino->recursos, origem->recursos,
               (size_t)origem->quantidade_recursos * sizeof(Recurso));
    for (int i = 0; i < origem->quantidade_mapas; ++i) {
        const Mapa *om = &origem->mapas[i];
        Mapa *dm = &destino->mapas[i];
        *dm = *om;
        dm->entidades = NULL;
        dm->colisoes = NULL;
        dm->capacidade_entidades = 0;
        for (int c = 0; c < MYTHARA_MAX_CAMADAS; ++c)
            dm->camadas[c].tiles = NULL;
        size_t total = (size_t)om->largura * om->altura;
        dm->colisoes = malloc(total);
        if (!dm->colisoes)
            goto falha_clone;
        memcpy(dm->colisoes, om->colisoes, total);
        for (int c = 0; c < om->quantidade_camadas; ++c) {
            dm->camadas[c] = om->camadas[c];
            dm->camadas[c].tiles = malloc(total * sizeof(uint16_t));
            if (!dm->camadas[c].tiles)
                goto falha_clone;
            memcpy(dm->camadas[c].tiles, om->camadas[c].tiles, total * sizeof(uint16_t));
        }
        if (!reservar_entidades(dm, om->quantidade_entidades))
            goto falha_clone;
        if (om->quantidade_entidades)
            memcpy(dm->entidades, om->entidades,
                   (size_t)om->quantidade_entidades * sizeof(Entidade));
    }
    for (int i = 0; i < origem->quantidade_eventos; ++i) {
        const Evento *oe = &origem->eventos[i];
        Evento *de = &destino->eventos[i];
        *de = *oe;
        de->comandos = NULL;
        de->capacidade_comandos = 0;
        if (!reservar_comandos(de, oe->quantidade_comandos))
            goto falha_clone;
        if (oe->quantidade_comandos)
            memcpy(de->comandos, oe->comandos,
                   (size_t)oe->quantidade_comandos * sizeof(ComandoEvento));
    }
    return 1;
falha_clone:
    liberar_projeto(destino);
    return 0;
}

static uint32_t resumo_projeto(const Projeto *p, size_t *bytes) {
    BufferDados b = {0};
    if (!serializar_projeto(p, &b)) {
        if (bytes)
            *bytes = 0;
        return 0;
    }
    uint32_t soma = soma_fnv1a(b.dados, b.tamanho);
    if (bytes)
        *bytes = b.tamanho;
    free(b.dados);
    return soma;
}

static int salvar_estado(const EstadoJogo *j, const char *caminho) {
    FILE *f = fopen(caminho, "wb");
    char magia[8] = "MYTSAVE";
    uint32_t soma = soma_fnv1a(j, sizeof(*j));
    if (!f)
        return 0;
    int ok = fwrite(magia, 8, 1, f) == 1 && fwrite(&soma, sizeof(soma), 1, f) == 1 &&
             fwrite(j, sizeof(*j), 1, f) == 1;
    fclose(f);
    return ok;
}

static int carregar_estado(EstadoJogo *j, const char *caminho) {
    FILE *f = fopen(caminho, "rb");
    char magia[8];
    uint32_t soma;
    EstadoJogo novo;
    if (!f)
        return 0;
    int ok = fread(magia, 8, 1, f) == 1 && !memcmp(magia, "MYTSAVE", 7) &&
             fread(&soma, sizeof(soma), 1, f) == 1 && fread(&novo, sizeof(novo), 1, f) == 1;
    fclose(f);
    if (!ok || soma_fnv1a(&novo, sizeof(novo)) != soma)
        return 0;
    *j = novo;
    return 1;
}

/* ========================================================================== */
/* Recursos BMP/WAV e exportacao                                               */
/* ========================================================================== */

typedef struct {
    uint32_t *pixels;
    int largura, altura;
} Imagem;

static uint16_t ler_u16(const uint8_t *p) {
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}
static uint32_t ler_u32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int carregar_bmp(const char *caminho, Imagem *img, char *erro, size_t cap_erro) {
    uint8_t cab[54];
    FILE *f = fopen(caminho, "rb");
    if (!f) {
        snprintf(erro, cap_erro, "Nao foi possivel abrir o BMP.");
        return 0;
    }
    if (fread(cab, sizeof(cab), 1, f) != 1 || cab[0] != 'B' || cab[1] != 'M')
        goto invalido;
    uint32_t inicio = ler_u32(cab + 10), comp = ler_u32(cab + 30);
    int32_t largura = (int32_t)ler_u32(cab + 18), altura = (int32_t)ler_u32(cab + 22);
    uint16_t bits = ler_u16(cab + 28);
    if (largura <= 0 || altura == 0 || largura > 4096 || abs(altura) > 4096 ||
        (bits != 24 && bits != 32) || comp != 0)
        goto invalido;
    int invertida = altura > 0;
    int h = abs(altura);
    size_t passo = ((size_t)largura * (bits / 8u) + 3u) & ~3u;
    uint8_t *linha = malloc(passo);
    uint32_t *pixels = malloc((size_t)largura * (size_t)h * 4u);
    if (!linha || !pixels) {
        free(linha);
        free(pixels);
        fclose(f);
        snprintf(erro, cap_erro, "Memoria insuficiente para BMP.");
        return 0;
    }
    if (fseek(f, (long)inicio, SEEK_SET) != 0)
        goto bmp_falha;
    for (int y = 0; y < h; ++y) {
        if (fread(linha, passo, 1, f) != 1)
            goto bmp_falha;
        int dy = invertida ? h - 1 - y : y;
        for (int x = 0; x < largura; ++x) {
            uint8_t *q = linha + (size_t)x * (bits / 8u);
            pixels[(size_t)dy * largura + x] = cor_rgb(q[2], q[1], q[0]);
        }
    }
    free(linha);
    fclose(f);
    img->pixels = pixels;
    img->largura = largura;
    img->altura = h;
    return 1;
bmp_falha:
    free(linha);
    free(pixels);
invalido:
    fclose(f);
    snprintf(erro, cap_erro, "BMP invalido; use 24/32 bits sem compressao.");
    return 0;
}

static uint32_t ler_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static int carregar_qoi(const char *caminho, Imagem *img, char *erro, size_t cap_erro) {
    FILE *f = fopen(caminho, "rb");
    uint8_t h[14];
    if (!f || fread(h, 14, 1, f) != 1 || memcmp(h, "qoif", 4)) {
        if (f)
            fclose(f);
        snprintf(erro, cap_erro, "QOI invalido.");
        return 0;
    }
    uint32_t w = ler_be32(h + 4), alt = ler_be32(h + 8);
    if (!w || !alt || w > 4096 || alt > 4096 || (h[12] != 3 && h[12] != 4)) {
        fclose(f);
        snprintf(erro, cap_erro, "Dimensoes QOI invalidas.");
        return 0;
    }
    uint32_t *pixels = malloc((size_t)w * alt * 4);
    if (!pixels) {
        fclose(f);
        return 0;
    }
    uint8_t indice[64][4] = {{0}}, r = 0, g = 0, b = 0, a = 255;
    int repeticoes = 0;
    size_t total = (size_t)w * alt;
    for (size_t i = 0; i < total; ++i) {
        if (repeticoes > 0)
            repeticoes--;
        else {
            int op = fgetc(f);
            if (op == EOF)
                goto falha;
            if (op == 0xfe) {
                int vr = fgetc(f), vg = fgetc(f), vb = fgetc(f);
                if (vb == EOF)
                    goto falha;
                r = (uint8_t)vr;
                g = (uint8_t)vg;
                b = (uint8_t)vb;
            } else if (op == 0xff) {
                int vr = fgetc(f), vg = fgetc(f), vb = fgetc(f), va = fgetc(f);
                if (va == EOF)
                    goto falha;
                r = (uint8_t)vr;
                g = (uint8_t)vg;
                b = (uint8_t)vb;
                a = (uint8_t)va;
            } else if ((op & 0xc0) == 0) {
                r = indice[op][0];
                g = indice[op][1];
                b = indice[op][2];
                a = indice[op][3];
            } else if ((op & 0xc0) == 0x40) {
                r = (uint8_t)(r + ((op >> 4) & 3) - 2);
                g = (uint8_t)(g + ((op >> 2) & 3) - 2);
                b = (uint8_t)(b + (op & 3) - 2);
            } else if ((op & 0xc0) == 0x80) {
                int op2 = fgetc(f);
                if (op2 == EOF)
                    goto falha;
                int dg = (op & 0x3f) - 32;
                r = (uint8_t)(r + dg + ((op2 >> 4) & 15) - 8);
                g = (uint8_t)(g + dg);
                b = (uint8_t)(b + dg + (op2 & 15) - 8);
            } else
                repeticoes = op & 0x3f;
            uint8_t hash = (uint8_t)((r * 3u + g * 5u + b * 7u + a * 11u) % 64u);
            indice[hash][0] = r;
            indice[hash][1] = g;
            indice[hash][2] = b;
            indice[hash][3] = a;
        }
        pixels[i] = ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    }
    fclose(f);
    img->pixels = pixels;
    img->largura = (int)w;
    img->altura = (int)alt;
    return 1;
falha:
    free(pixels);
    fclose(f);
    snprintf(erro, cap_erro, "QOI truncado.");
    return 0;
}

static int carregar_tga(const char *caminho, Imagem *img, char *erro, size_t cap_erro) {
    FILE *f = fopen(caminho, "rb");
    uint8_t h[18];
    if (!f || fread(h, 18, 1, f) != 1 || h[1] != 0 || h[2] != 2) {
        if (f)
            fclose(f);
        snprintf(erro, cap_erro, "TGA invalido; use imagem sem compressao.");
        return 0;
    }
    int w = ler_u16(h + 12), alt = ler_u16(h + 14), bits = h[16];
    if (w <= 0 || alt <= 0 || w > 4096 || alt > 4096 || (bits != 24 && bits != 32)) {
        fclose(f);
        snprintf(erro, cap_erro, "TGA deve ter 24 ou 32 bits.");
        return 0;
    }
    if (fseek(f, h[0], SEEK_CUR) != 0) {
        fclose(f);
        return 0;
    }
    uint32_t *pixels = malloc((size_t)w * alt * 4);
    if (!pixels) {
        fclose(f);
        return 0;
    }
    int origem_superior = (h[17] & 0x20) != 0;
    for (int y = 0; y < alt; ++y)
        for (int x = 0; x < w; ++x) {
            uint8_t px[4] = {0, 0, 0, 255};
            if (fread(px, (size_t)bits / 8, 1, f) != 1) {
                free(pixels);
                fclose(f);
                snprintf(erro, cap_erro, "TGA truncado.");
                return 0;
            }
            int dy = origem_superior ? y : alt - 1 - y;
            pixels[(size_t)dy * w + x] =
                ((uint32_t)px[3] << 24) | ((uint32_t)px[2] << 16) | ((uint32_t)px[1] << 8) | px[0];
        }
    fclose(f);
    img->pixels = pixels;
    img->largura = w;
    img->altura = alt;
    return 1;
}

static int carregar_imagem(int tipo, const char *caminho, Imagem *img, char *erro,
                           size_t cap_erro) {
    if (tipo == RECURSO_BMP)
        return carregar_bmp(caminho, img, erro, cap_erro);
    if (tipo == RECURSO_QOI)
        return carregar_qoi(caminho, img, erro, cap_erro);
    if (tipo == RECURSO_TGA)
        return carregar_tga(caminho, img, erro, cap_erro);
    snprintf(erro, cap_erro, "Formato de imagem desconhecido.");
    return 0;
}

static void liberar_imagem(Imagem *img) {
    free(img->pixels);
    memset(img, 0, sizeof(*img));
}

static void desenhar_imagem(Tela *t, const Imagem *img, Retangulo destino, Retangulo origem) {
    if (!img->pixels || destino.largura <= 0 || destino.altura <= 0)
        return;
    for (int y = 0; y < destino.altura; ++y) {
        int dy = destino.y + y;
        if (dy < 0 || dy >= t->altura)
            continue;
        int sy = origem.y + y * origem.altura / destino.altura;
        if (sy < 0 || sy >= img->altura)
            continue;
        for (int x = 0; x < destino.largura; ++x) {
            int dx = destino.x + x;
            if (dx < 0 || dx >= t->largura)
                continue;
            int sx = origem.x + x * origem.largura / destino.largura;
            if (sx >= 0 && sx < img->largura) {
                Cor fonte = img->pixels[(size_t)sy * img->largura + sx];
                unsigned alfa = fonte >> 24;
                if (alfa == 255)
                    t->pixels[(size_t)dy * t->largura + dx] = fonte;
                else if (alfa) {
                    Cor fundo = t->pixels[(size_t)dy * t->largura + dx];
                    unsigned rr =
                        (((fonte >> 16) & 255) * alfa + ((fundo >> 16) & 255) * (255 - alfa)) / 255;
                    unsigned gg =
                        (((fonte >> 8) & 255) * alfa + ((fundo >> 8) & 255) * (255 - alfa)) / 255;
                    unsigned bb = ((fonte & 255) * alfa + (fundo & 255) * (255 - alfa)) / 255;
                    t->pixels[(size_t)dy * t->largura + dx] = cor_rgb(rr, gg, bb);
                }
            }
        }
    }
}

#ifndef MYTHARA_SEM_AUDIO
#ifdef _WIN32
static int tocar_wav(const char *caminho) {
    return PlaySoundA(caminho, NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT) != 0;
}
#else
static int tocar_wav_sincrono(const char *caminho) {
    FILE *f = fopen(caminho, "rb");
    uint8_t h[12];
    uint8_t *dados = NULL;
    uint32_t tamanho = 0, taxa = 0;
    uint16_t canais = 0, bits = 0, formato = 0;
    if (!f || fread(h, 12, 1, f) != 1 || memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVE", 4)) {
        if (f)
            fclose(f);
        return 0;
    }
    while (!feof(f)) {
        uint8_t ch[8];
        if (fread(ch, 8, 1, f) != 1)
            break;
        uint32_t n = ler_u32(ch + 4);
        if (!memcmp(ch, "fmt ", 4)) {
            uint8_t fmt[40] = {0};
            if (n > 40 || fread(fmt, n, 1, f) != 1)
                break;
            formato = ler_u16(fmt);
            canais = ler_u16(fmt + 2);
            taxa = ler_u32(fmt + 4);
            bits = ler_u16(fmt + 14);
        } else if (!memcmp(ch, "data", 4)) {
            dados = malloc(n);
            if (!dados || fread(dados, n, 1, f) != 1) {
                free(dados);
                dados = NULL;
                break;
            }
            tamanho = n;
        } else
            fseek(f, (long)n, SEEK_CUR);
        if (n & 1u)
            fseek(f, 1, SEEK_CUR);
    }
    fclose(f);
    if (!dados || formato != 1 || (bits != 8 && bits != 16) || (canais != 1 && canais != 2) ||
        !taxa) {
        free(dados);
        return 0;
    }
    snd_pcm_t *pcm = NULL;
    snd_pcm_format_t pf = bits == 16 ? SND_PCM_FORMAT_S16_LE : SND_PCM_FORMAT_U8;
    if (snd_pcm_open(&pcm, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0) {
        free(dados);
        return 0;
    }
    if (snd_pcm_set_params(pcm, pf, SND_PCM_ACCESS_RW_INTERLEAVED, canais, taxa, 1, 100000) < 0) {
        snd_pcm_close(pcm);
        free(dados);
        return 0;
    }
    snd_pcm_sframes_t quadros = (snd_pcm_sframes_t)(tamanho / (canais * (bits / 8u)));
    snd_pcm_writei(pcm, dados, (snd_pcm_uframes_t)quadros);
    snd_pcm_drain(pcm);
    snd_pcm_close(pcm);
    free(dados);
    return 1;
}
static void *thread_audio(void *dados) {
    char *caminho = (char *)dados;
    tocar_wav_sincrono(caminho);
    free(caminho);
    return NULL;
}
static int tocar_wav(const char *caminho) {
    char *copia = strdup(caminho);
    pthread_t thread;
    if (!copia || pthread_create(&thread, NULL, thread_audio, copia) != 0) {
        free(copia);
        return 0;
    }
    pthread_detach(thread);
    return 1;
}
#endif
#else
static int tocar_wav(const char *caminho) {
    (void)caminho;
    return 0;
}
#endif

static int copiar_arquivo(const char *origem, const char *destino) {
    int entrada = open(origem, O_RDONLY | O_BINARY);
    if (entrada < 0)
        return 0;
    int saida = open(destino, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0755);
    if (saida < 0) {
        close(entrada);
        return 0;
    }
    char bloco[65536];
    ssize_t n;
    int ok = 1;
    while ((n = read(entrada, bloco, sizeof(bloco))) > 0) {
        char *p = bloco;
        ssize_t restante = n;
        while (restante > 0) {
            ssize_t w = write(saida, p, (size_t)restante);
            if (w <= 0) {
                ok = 0;
                break;
            }
            p += w;
            restante -= w;
        }
        if (!ok)
            break;
    }
    if (n < 0)
        ok = 0;
    close(entrada);
    if (close(saida) != 0)
        ok = 0;
    return ok;
}

static int importar_arquivo_projeto(const Projeto *p, const char *origem, int audio, char *relativo,
                                    size_t capacidade) {
    const char *base = nome_base(origem);
    char subpasta[64], pasta[MYTHARA_MAX_CAMINHO], destino[MYTHARA_MAX_CAMINHO];
    copiar_texto(subpasta, sizeof(subpasta), audio ? "recursos/audio" : "recursos/imagens");
    if (!juntar_caminho(pasta, sizeof(pasta), p->pasta_base, subpasta) || !criar_diretorios(pasta))
        return 0;
    char nome[768];
    copiar_texto(nome, sizeof(nome), base);
    char *ponto = strrchr(nome, '.');
    char extensao[64] = "";
    if (ponto) {
        copiar_texto(extensao, sizeof(extensao), ponto);
        *ponto = 0;
    }
    for (int tentativa = 1; tentativa < 1000; ++tentativa) {
        char candidato[MYTHARA_MAX_CAMINHO];
        if (tentativa == 1)
            snprintf(candidato, sizeof(candidato), "%.700s%.63s", nome, extensao);
        else
            snprintf(candidato, sizeof(candidato), "%.700s_%d%.63s", nome, tentativa, extensao);
        if (!juntar_caminho(destino, sizeof(destino), pasta, candidato))
            return 0;
        if (access(destino, 0) != 0) {
            if (!copiar_arquivo(origem, destino))
                return 0;
            int n = snprintf(relativo, capacidade, "%s/%s", subpasta, candidato);
            return n > 0 && (size_t)n < capacidade;
        }
    }
    return 0;
}

static int exportar_projeto(const Projeto *p, const char *pasta, char *erro, size_t cap_erro) {
    char executavel[MYTHARA_MAX_CAMINHO], destino[MYTHARA_MAX_CAMINHO * 2],
        recursos[MYTHARA_MAX_CAMINHO * 2];
    if (!caminho_do_executavel(executavel, sizeof(executavel))) {
        snprintf(erro, cap_erro, "Nao foi possivel localizar o executavel.");
        return 0;
    }
    if (mkdir(pasta, 0755) != 0 && errno != EEXIST) {
        snprintf(erro, cap_erro, "Nao foi possivel criar a pasta de exportacao.");
        return 0;
    }
    snprintf(destino, sizeof(destino), "%s/jogo%s", pasta,
#ifdef _WIN32
             ".exe"
#else
             ""
#endif
    );
    if (!copiar_arquivo(executavel, destino)) {
        snprintf(erro, cap_erro, "Falha ao copiar o jogador.");
        return 0;
    }
    chmod(destino, 0755);
    Projeto copia = *p;
    copia.recursos = NULL;
    copia.capacidade_recursos = 0;
    if (!reservar_memoria((void **)&copia.recursos, &copia.capacidade_recursos,
                          copia.quantidade_recursos, sizeof(Recurso), MYTHARA_MAX_RECURSOS)) {
        snprintf(erro, cap_erro, "Memoria insuficiente para exportar.");
        return 0;
    }
    if (copia.quantidade_recursos > 0)
        memcpy(copia.recursos, p->recursos, (size_t)copia.quantidade_recursos * sizeof(Recurso));
    snprintf(recursos, sizeof(recursos), "%s/recursos", pasta);
    mkdir(recursos, 0755);
    snprintf(destino, sizeof(destino), "%s/recursos/imagens", pasta);
    criar_diretorios(destino);
    snprintf(destino, sizeof(destino), "%s/recursos/audio", pasta);
    criar_diretorios(destino);
    for (int i = 0; i < copia.quantidade_recursos; ++i)
        if (copia.recursos[i].ativo) {
            char alvo[MYTHARA_MAX_CAMINHO * 3], origem[MYTHARA_MAX_CAMINHO];
            const char *base = nome_base(copia.recursos[i].caminho);
            caminho_do_recurso(p, copia.recursos[i].caminho, origem, sizeof(origem));
            juntar_caminho(alvo, sizeof(alvo), pasta, copia.recursos[i].caminho);
            if (!copiar_arquivo(origem, alvo)) {
                snprintf(erro, cap_erro, "Falha ao copiar recurso %.150s.", base);
                free(copia.recursos);
                return 0;
            }
        }
    snprintf(destino, sizeof(destino), "%s/jogo.myr", pasta);
    int ok = salvar_projeto_em(&copia, destino, erro, cap_erro);
    free(copia.recursos);
    return ok;
}

/* ========================================================================== */
/* Estado do aplicativo e operacoes do editor                                 */
/* ========================================================================== */

typedef enum { MODO_EDITOR, MODO_JOGO, MODO_BATALHA } ModoAplicativo;
typedef enum {
    FERRAMENTA_PINCEL,
    FERRAMENTA_BORRACHA,
    FERRAMENTA_COLISAO,
    FERRAMENTA_ENTIDADE,
    FERRAMENTA_SELECAO,
    FERRAMENTA_RETANGULO,
    FERRAMENTA_LINHA,
    FERRAMENTA_PREENCHER,
    FERRAMENTA_CONTA_GOTAS,
    FERRAMENTA_CARIMBO
} Ferramenta;
typedef enum {
    MODAL_NENHUM,
    MODAL_CAMINHO,
    MODAL_BANCO,
    MODAL_EVENTOS,
    MODAL_RECURSOS,
    MODAL_AJUDA,
    MODAL_NOVO,
    MODAL_TEMA,
    MODAL_COMANDOS,
    MODAL_RECUPERACAO,
    MODAL_BOAS_VINDAS
} TipoModal;
typedef enum { ACAO_ABRIR, ACAO_SALVAR_COMO, ACAO_EXPORTAR } AcaoCaminho;
typedef enum {
    LAYOUT_MAPA,
    LAYOUT_EVENTOS,
    LAYOUT_BANCO,
    LAYOUT_RECURSOS,
    LAYOUT_PLAYTEST
} LayoutEditor;

typedef struct {
    Projeto projeto;
    size_t bytes;
    uint32_t resumo;
    char descricao[64];
} EstadoHistorico;

typedef struct {
    int mapa_atual, camada, tile, zoom;
    int camera_x, camera_y;
    int ferramenta;
    int entidade_selecionada;
    int recurso_selecionado;
    int evento_selecionado;
    int comando_selecionado;
    int aba_banco, registro_banco;
    int largura_arvore, largura_inspetor, altura_inferior, divisor_ativo;
    int layout_ativo, gaveta_arvore, gaveta_inspetor;
    int selecao_x0, selecao_y0, selecao_x1, selecao_y1, selecao_ativa;
    int cursor_tile_x, cursor_tile_y, canvas_em_foco;
    int arrastando_forma, autotile;
    int largura_carimbo, altura_carimbo, camadas_carimbo;
    uint16_t carimbo[MYTHARA_MAX_CAMADAS][16 * 16];
    int clipboard_largura, clipboard_altura, clipboard_camadas, clipboard_entidades;
    uint16_t clipboard_tiles[MYTHARA_MAX_CAMADAS][64 * 64];
    uint8_t clipboard_colisoes[64 * 64];
    Entidade clipboard_lista_entidades[256];
    char busca_arvore[64];
    char busca_comandos[64];
    int cor_tema_selecionada;
    Edicao historico[MYTHARA_MAX_EDICOES];
    int quantidade_historico, posicao_historico;
    EstadoHistorico *estados_historico;
    int quantidade_estados_historico, capacidade_estados_historico, posicao_estado_historico;
    size_t bytes_historico;
    uint32_t resumo_historico;
    int historico_global_ativo, aguardando_transacao, ignorar_captura;
    int alterado;
} Editor;

typedef struct {
    EstadoJogo estado;
    int evento_ativo, proximo_comando;
    int aguardando_texto, aguardando_escolha;
    char texto[320];
    char escolha_a[80], escolha_b[80];
    int inimigo, vida_inimigo;
    int quantidade_inimigos_batalha;
    int inimigos_batalha[8], vidas_inimigos[8];
    int membro_escolhendo, acoes_grupo[4], defesas_grupo[4];
    int tela_sobreposta, loja_ativa;
    int pilha_eventos[8], pilha_comandos[8], topo_pilha;
    int pilha_repeticao_pc[8], pilha_repeticao_restante[8], pilha_repeticao_nivel[8],
        topo_repeticao;
    int passos_encontro;
    double proximo_movimento;
    double esperar_evento_ate;
    char aviso[128];
    int slot;
} Jogo;

typedef struct {
    Projeto projeto;
    Editor editor;
    Jogo jogo;
    ModoAplicativo modo;
    TipoModal modal;
    AcaoCaminho acao_caminho;
    char caminho_projeto[MYTHARA_MAX_CAMINHO];
    char caminho_recente[MYTHARA_MAX_CAMINHO];
    char campo_caminho[MYTHARA_MAX_CAMINHO];
    char pasta_navegador[MYTHARA_MAX_CAMINHO];
    char arquivo_navegador[256];
    char campo_tema[MYTHARA_MAX_CAMINHO];
    char mensagem[256];
    Imagem imagens[MYTHARA_MAX_RECURSOS];
    int projeto_carregado;
    int foco_interface;
    double ultimo_autosalvamento;
    char caminho_autosave[MYTHARA_MAX_CAMINHO + 32];
    int oferecer_recuperacao;
    int mostrar_boas_vindas, dicas_ativas;
    atomic_int autosave_em_andamento;
} Aplicativo;

typedef struct {
    char magia[8];
    uint32_t versao;
    TemaInterface tema;
    int largura_arvore, largura_inspetor, altura_inferior, layout_ativo, mostrar_boas_vindas,
        dicas_ativas;
    char caminho_recente[MYTHARA_MAX_CAMINHO];
} ConfiguracaoUsuario;

static int caminho_configuracao(char *destino, size_t capacidade) {
    const char *casa = getenv("HOME");
    char pasta[MYTHARA_MAX_CAMINHO];
#ifdef _WIN32
    const char *appdata = getenv("APPDATA");
    if (appdata && appdata[0])
        copiar_texto(pasta, sizeof(pasta), appdata);
    else if (casa && casa[0])
        copiar_texto(pasta, sizeof(pasta), casa);
    else
        return 0;
#else
    const char *xdg = getenv("XDG_CONFIG_HOME");
    if (xdg && xdg[0])
        copiar_texto(pasta, sizeof(pasta), xdg);
    else if (casa && casa[0])
        snprintf(pasta, sizeof(pasta), "%s/.config", casa);
    else
        return 0;
#endif
    mkdir(pasta, 0755);
    size_t n = strlen(pasta);
    if (n + 9 >= sizeof(pasta))
        return 0;
    memcpy(pasta + n, "/mythara", 9);
    mkdir(pasta, 0755);
    snprintf(destino, capacidade, "%s/config.myc", pasta);
    return 1;
}

static void carregar_configuracao(Aplicativo *a) {
    char caminho[MYTHARA_MAX_CAMINHO];
    ConfiguracaoUsuario c;
    if (!caminho_configuracao(caminho, sizeof(caminho)))
        return;
    FILE *f = fopen(caminho, "rb");
    if (!f)
        return;
    int ok = fread(&c, sizeof(c), 1, f) == 1 && !memcmp(c.magia, "MYTCONF", 7) && c.versao == 3;
    fclose(f);
    if (ok) {
        tema_ativo = c.tema;
        tema_ativo.escala_percentual =
            limitar_int(((tema_ativo.escala_percentual + 12) / 25) * 25, 100, 200);
        a->editor.largura_arvore = limitar_int(c.largura_arvore, 190, 360);
        a->editor.largura_inspetor = limitar_int(c.largura_inspetor, 250, 420);
        a->editor.altura_inferior = limitar_int(c.altura_inferior, 95, 240);
        a->editor.layout_ativo = limitar_int(c.layout_ativo, 0, 4);
        a->mostrar_boas_vindas = c.mostrar_boas_vindas;
        a->dicas_ativas = c.dicas_ativas;
        if (!a->caminho_recente[0])
            copiar_texto(a->caminho_recente, sizeof(a->caminho_recente), c.caminho_recente);
    }
}

static void salvar_configuracao(const Aplicativo *a) {
    char caminho[MYTHARA_MAX_CAMINHO];
    if (!caminho_configuracao(caminho, sizeof(caminho)))
        return;
    ConfiguracaoUsuario c;
    memset(&c, 0, sizeof(c));
    memcpy(c.magia, "MYTCONF", 7);
    c.versao = 3;
    c.tema = tema_ativo;
    c.largura_arvore = a->editor.largura_arvore;
    c.largura_inspetor = a->editor.largura_inspetor;
    c.altura_inferior = a->editor.altura_inferior;
    c.layout_ativo = a->editor.layout_ativo;
    c.mostrar_boas_vindas = a->mostrar_boas_vindas;
    c.dicas_ativas = a->dicas_ativas;
    copiar_texto(c.caminho_recente, sizeof(c.caminho_recente), a->caminho_recente);
    FILE *f = fopen(caminho, "wb");
    if (f) {
        fwrite(&c, sizeof(c), 1, f);
        fclose(f);
    }
}

static int salvar_tema_arquivo(const char *caminho) {
    FILE *f = fopen(caminho, "wb");
    if (!f)
        return 0;
    char magia[8] = "MYTTEMA";
    uint32_t soma = soma_fnv1a(&tema_ativo, sizeof(tema_ativo));
    int ok = fwrite(magia, 8, 1, f) == 1 && fwrite(&soma, 4, 1, f) == 1 &&
             fwrite(&tema_ativo, sizeof(tema_ativo), 1, f) == 1;
    fclose(f);
    return ok;
}
static int carregar_tema_arquivo(const char *caminho) {
    FILE *f = fopen(caminho, "rb");
    char magia[8];
    uint32_t soma;
    TemaInterface t;
    if (!f)
        return 0;
    int ok = fread(magia, 8, 1, f) == 1 && !memcmp(magia, "MYTTEMA", 7) &&
             fread(&soma, 4, 1, f) == 1 && fread(&t, sizeof(t), 1, f) == 1 &&
             soma_fnv1a(&t, sizeof(t)) == soma;
    fclose(f);
    if (ok) {
        t.escala_percentual = limitar_int(t.escala_percentual, 100, 200);
        tema_ativo = t;
    }
    return ok;
}

static const char *nomes_comandos[] = {"Mostrar texto",
                                       "Mostrar escolha",
                                       "Definir flag",
                                       "Alterar variavel",
                                       "Dar/remover item",
                                       "Teleportar",
                                       "Iniciar batalha",
                                       "Curar heroi",
                                       "Tocar audio",
                                       "Esperar",
                                       "Se",
                                       "Senao",
                                       "Fim do bloco",
                                       "Repetir",
                                       "Chamar evento",
                                       "Abrir loja",
                                       "Atualizar missao"};

static const char *nomes_entidades[] = {"NPC", "Bau", "Portal", "Inimigo"};

static void mensagem_aplicativo(Aplicativo *a, const char *texto) {
    copiar_texto(a->mensagem, sizeof(a->mensagem), texto);
}

static int valor_celula_mapa(Mapa *m, int camada, int indice) {
    if (camada == MYTHARA_MAX_CAMADAS)
        return m->colisoes[indice];
    if (camada >= 0 && camada < m->quantidade_camadas && m->camadas[camada].tiles)
        return m->camadas[camada].tiles[indice];
    return 0;
}

static void definir_celula_mapa(Mapa *m, int camada, int indice, int valor) {
    if (camada == MYTHARA_MAX_CAMADAS)
        m->colisoes[indice] = (uint8_t)valor;
    else if (camada >= 0 && camada < m->quantidade_camadas && m->camadas[camada].tiles)
        m->camadas[camada].tiles[indice] = (uint16_t)valor;
}

static void registrar_edicao(Editor *e, int mapa, int camada, int indice, int anterior, int novo) {
    if (anterior == novo)
        return;
    if (e->posicao_historico < e->quantidade_historico)
        e->quantidade_historico = e->posicao_historico;
    if (e->quantidade_historico == MYTHARA_MAX_EDICOES) {
        memmove(e->historico, e->historico + 1,
                sizeof(e->historico[0]) * (MYTHARA_MAX_EDICOES - 1));
        e->quantidade_historico--;
        e->posicao_historico--;
    }
    e->historico[e->quantidade_historico++] = (Edicao){
        (uint32_t)mapa, (uint32_t)camada, (uint32_t)indice, (uint32_t)anterior, (uint32_t)novo};
    e->posicao_historico = e->quantidade_historico;
    e->alterado = 1;
}

static void liberar_recursos(Aplicativo *a);
static void recarregar_recursos(Aplicativo *a);

static void liberar_historico_global(Editor *e) {
    for (int i = 0; i < e->quantidade_estados_historico; ++i)
        liberar_projeto(&e->estados_historico[i].projeto);
    free(e->estados_historico);
    e->estados_historico = NULL;
    e->quantidade_estados_historico = e->capacidade_estados_historico =
        e->posicao_estado_historico = 0;
    e->bytes_historico = 0;
    e->historico_global_ativo = 0;
}

static int adicionar_estado_historico(Aplicativo *a, const char *descricao) {
    Editor *e = &a->editor;
    while (e->quantidade_estados_historico > e->posicao_estado_historico + 1) {
        int i = --e->quantidade_estados_historico;
        e->bytes_historico -= e->estados_historico[i].bytes;
        liberar_projeto(&e->estados_historico[i].projeto);
    }
    if (e->quantidade_estados_historico >= e->capacidade_estados_historico) {
        int nova = e->capacidade_estados_historico ? e->capacidade_estados_historico * 2 : 16;
        if (nova > MYTHARA_MAX_EDICOES)
            nova = MYTHARA_MAX_EDICOES;
        if (nova <= e->capacidade_estados_historico)
            return 0;
        void *p = realloc(e->estados_historico, (size_t)nova * sizeof(EstadoHistorico));
        if (!p)
            return 0;
        e->estados_historico = p;
        memset(e->estados_historico + e->capacidade_estados_historico, 0,
               (size_t)(nova - e->capacidade_estados_historico) * sizeof(EstadoHistorico));
        e->capacidade_estados_historico = nova;
    }
    EstadoHistorico *estado = &e->estados_historico[e->quantidade_estados_historico];
    memset(estado, 0, sizeof(*estado));
    if (!clonar_projeto(&a->projeto, &estado->projeto))
        return 0;
    estado->resumo = resumo_projeto(&a->projeto, &estado->bytes);
    copiar_texto(estado->descricao, sizeof(estado->descricao), descricao);
    e->bytes_historico += estado->bytes;
    e->quantidade_estados_historico++;
    e->posicao_estado_historico = e->quantidade_estados_historico - 1;
    e->resumo_historico = estado->resumo;
    while ((e->quantidade_estados_historico > MYTHARA_MAX_EDICOES ||
            e->bytes_historico > MYTHARA_MAX_HISTORICO_BYTES) &&
           e->quantidade_estados_historico > 1) {
        e->bytes_historico -= e->estados_historico[0].bytes;
        liberar_projeto(&e->estados_historico[0].projeto);
        memmove(e->estados_historico, e->estados_historico + 1,
                (size_t)(e->quantidade_estados_historico - 1) * sizeof(EstadoHistorico));
        e->quantidade_estados_historico--;
        e->posicao_estado_historico--;
        memset(&e->estados_historico[e->quantidade_estados_historico], 0, sizeof(EstadoHistorico));
    }
    return 1;
}

static void reiniciar_historico_global(Aplicativo *a) {
    liberar_historico_global(&a->editor);
    a->editor.historico_global_ativo = 1;
    adicionar_estado_historico(a, "Estado inicial");
}

static int restaurar_estado_historico(Aplicativo *a, int posicao) {
    Editor *e = &a->editor;
    if (posicao < 0 || posicao >= e->quantidade_estados_historico)
        return 0;
    Projeto copia;
    if (!clonar_projeto(&e->estados_historico[posicao].projeto, &copia))
        return 0;
    liberar_recursos(a);
    liberar_projeto(&a->projeto);
    a->projeto = copia;
    recarregar_recursos(a);
    e->posicao_estado_historico = posicao;
    e->resumo_historico = e->estados_historico[posicao].resumo;
    e->ignorar_captura = 1;
    e->alterado = 1;
    return 1;
}

static void verificar_historico_global(Aplicativo *a, const Entrada *entrada) {
    Editor *e = &a->editor;
    if (!e->historico_global_ativo)
        return;
    size_t bytes = 0;
    uint32_t atual = resumo_projeto(&a->projeto, &bytes);
    (void)bytes;
    if (e->ignorar_captura) {
        e->ignorar_captura = 0;
        e->resumo_historico = atual;
        return;
    }
    if (atual == e->resumo_historico) {
        if (entrada->mouse_solto)
            e->aguardando_transacao = 0;
        return;
    }
    if (entrada->mouse_baixo && !entrada->mouse_solto) {
        e->aguardando_transacao = 1;
        return;
    }
    if (adicionar_estado_historico(a,
                                   e->aguardando_transacao ? "Gesto no editor" : "Edicao global")) {
        e->alterado = 1;
        e->aguardando_transacao = 0;
    }
}

static void desfazer_edicao(Aplicativo *a) {
    Editor *e = &a->editor;
    if (e->historico_global_ativo) {
        if (e->posicao_estado_historico > 0)
            restaurar_estado_historico(a, e->posicao_estado_historico - 1);
        return;
    }
    if (e->posicao_historico <= 0)
        return;
    Edicao *d = &e->historico[--e->posicao_historico];
    if (d->mapa < (uint32_t)a->projeto.quantidade_mapas) {
        Mapa *m = &a->projeto.mapas[d->mapa];
        if (d->camada <= MYTHARA_MAX_CAMADAS && d->indice < (uint32_t)(m->largura * m->altura))
            definir_celula_mapa(m, (int)d->camada, (int)d->indice, (int)d->anterior);
    }
    e->alterado = 1;
}

static void refazer_edicao(Aplicativo *a) {
    Editor *e = &a->editor;
    if (e->historico_global_ativo) {
        if (e->posicao_estado_historico + 1 < e->quantidade_estados_historico)
            restaurar_estado_historico(a, e->posicao_estado_historico + 1);
        return;
    }
    if (e->posicao_historico >= e->quantidade_historico)
        return;
    Edicao *d = &e->historico[e->posicao_historico++];
    if (d->mapa < (uint32_t)a->projeto.quantidade_mapas) {
        Mapa *m = &a->projeto.mapas[d->mapa];
        if (d->camada <= MYTHARA_MAX_CAMADAS && d->indice < (uint32_t)(m->largura * m->altura))
            definir_celula_mapa(m, (int)d->camada, (int)d->indice, (int)d->novo);
    }
    e->alterado = 1;
}

static void liberar_recursos(Aplicativo *a) {
    for (int i = 0; i < MYTHARA_MAX_RECURSOS; ++i)
        liberar_imagem(&a->imagens[i]);
}

static int preparar_pasta_projeto(Projeto *p, const char *arquivo) {
    char caminho[MYTHARA_MAX_CAMINHO];
    pasta_do_arquivo(arquivo, p->pasta_base, sizeof(p->pasta_base));
    if (!criar_diretorios(p->pasta_base))
        return 0;
    const char *pastas[] = {"recursos", "recursos/imagens", "recursos/audio",
                            ".mythara", ".mythara/backups", "exportacoes"};
    for (size_t i = 0; i < sizeof(pastas) / sizeof(pastas[0]); ++i) {
        if (!juntar_caminho(caminho, sizeof(caminho), p->pasta_base, pastas[i]) ||
            !criar_diretorios(caminho))
            return 0;
    }
    return 1;
}

static void definir_caminho_autosave(Aplicativo *a) {
    char pasta[MYTHARA_MAX_CAMINHO];
    juntar_caminho(pasta, sizeof(pasta), a->projeto.pasta_base, ".mythara/backups");
    juntar_caminho(a->caminho_autosave, sizeof(a->caminho_autosave), pasta, "autosave_0.myr");
}

static void recarregar_recursos(Aplicativo *a) {
    char erro[128];
    liberar_recursos(a);
    for (int i = 0; i < a->projeto.quantidade_recursos; ++i)
        if (a->projeto.recursos[i].ativo && a->projeto.recursos[i].tipo != RECURSO_WAV) {
            char caminho[MYTHARA_MAX_CAMINHO];
            caminho_do_recurso(&a->projeto, a->projeto.recursos[i].caminho, caminho,
                               sizeof(caminho));
            carregar_imagem(a->projeto.recursos[i].tipo, caminho, &a->imagens[i], erro,
                            sizeof(erro));
        }
}

static void novo_projeto(Aplicativo *a) {
    liberar_historico_global(&a->editor);
    liberar_recursos(a);
    liberar_projeto(&a->projeto);
    iniciar_projeto(&a->projeto);
    memset(&a->editor, 0, sizeof(a->editor));
    a->editor.zoom = 32;
    a->editor.tile = 1;
    a->editor.entidade_selecionada = -1;
    a->editor.evento_selecionado = 0;
    a->editor.largura_arvore = 235;
    a->editor.largura_inspetor = 280;
    a->editor.altura_inferior = 150;
    copiar_texto(a->caminho_projeto, sizeof(a->caminho_projeto), "projeto_mythara/projeto.myr");
    preparar_pasta_projeto(&a->projeto, a->caminho_projeto);
    definir_caminho_autosave(a);
    a->ultimo_autosalvamento = agora_segundos();
    a->oferecer_recuperacao = 0;
    a->projeto_carregado = 1;
    a->editor.alterado = 1;
    reiniciar_historico_global(a);
    mensagem_aplicativo(a, "Novo projeto Mythara v3 criado.");
}

static int salvar_projeto_atual(Aplicativo *a) {
    char erro[192];
    atribuir_ids_ausentes(&a->projeto);
    if (!preparar_pasta_projeto(&a->projeto, a->caminho_projeto)) {
        mensagem_aplicativo(a, "Nao foi possivel preparar a pasta do projeto.");
        return 0;
    }
    if (salvar_projeto_em(&a->projeto, a->caminho_projeto, erro, sizeof(erro))) {
        definir_caminho_autosave(a);
        copiar_texto(a->caminho_recente, sizeof(a->caminho_recente), a->caminho_projeto);
        a->ultimo_autosalvamento = agora_segundos();
        a->editor.alterado = 0;
        mensagem_aplicativo(a, "Projeto salvo com sucesso.");
        return 1;
    }
    mensagem_aplicativo(a, erro);
    return 0;
}

static int abrir_projeto_atual(Aplicativo *a, const char *caminho) {
    char erro[192];
    Projeto p;
    memset(&p, 0, sizeof(p));
    if (!carregar_projeto_de(&p, caminho, erro, sizeof(erro))) {
        mensagem_aplicativo(a, erro);
        return 0;
    }
    liberar_historico_global(&a->editor);
    liberar_recursos(a);
    liberar_projeto(&a->projeto);
    a->projeto = p;
    memset(&a->editor, 0, sizeof(a->editor));
    a->editor.zoom = 32;
    a->editor.tile = 1;
    a->editor.entidade_selecionada = -1;
    a->editor.largura_arvore = 235;
    a->editor.largura_inspetor = 280;
    a->editor.altura_inferior = 150;
    copiar_texto(a->caminho_projeto, sizeof(a->caminho_projeto), caminho);
    preparar_pasta_projeto(&a->projeto, caminho);
    recarregar_recursos(a);
    definir_caminho_autosave(a);
    copiar_texto(a->caminho_recente, sizeof(a->caminho_recente), caminho);
    struct stat original, auto_salvo;
    a->oferecer_recuperacao = stat(caminho, &original) == 0 &&
                              stat(a->caminho_autosave, &auto_salvo) == 0 &&
                              auto_salvo.st_mtime > original.st_mtime;
    if (a->oferecer_recuperacao)
        a->modal = MODAL_RECUPERACAO;
    a->ultimo_autosalvamento = agora_segundos();
    a->projeto_carregado = 1;
    reiniciar_historico_global(a);
    mensagem_aplicativo(a, "Projeto Mythara v3 carregado.");
    return 1;
}

typedef struct {
    BufferDados buffer;
    char caminho[MYTHARA_MAX_CAMINHO];
    atomic_int *em_andamento;
} TrabalhoAutosave;

#ifdef _WIN32
static DWORD WINAPI executar_autosave_thread(LPVOID dados) {
    TrabalhoAutosave *t = dados;
    salvar_buffer_projeto(&t->buffer, t->caminho);
    free(t->buffer.dados);
    atomic_store(t->em_andamento, 0);
    free(t);
    return 0;
}
static int iniciar_autosave_thread(TrabalhoAutosave *t) {
    HANDLE h = CreateThread(NULL, 0, executar_autosave_thread, t, 0, NULL);
    if (!h)
        return 0;
    CloseHandle(h);
    return 1;
}
#else
static void *executar_autosave_thread(void *dados) {
    TrabalhoAutosave *t = dados;
    salvar_buffer_projeto(&t->buffer, t->caminho);
    free(t->buffer.dados);
    atomic_store(t->em_andamento, 0);
    free(t);
    return NULL;
}
static int iniciar_autosave_thread(TrabalhoAutosave *t) {
    pthread_t thread;
    if (pthread_create(&thread, NULL, executar_autosave_thread, t) != 0)
        return 0;
    pthread_detach(thread);
    return 1;
}
#endif

static void realizar_autosave(Aplicativo *a) {
    if (!a->editor.alterado || agora_segundos() - a->ultimo_autosalvamento < 60.0 ||
        atomic_load(&a->autosave_em_andamento))
        return;
    if (!a->caminho_autosave[0])
        definir_caminho_autosave(a);
    char origem[MYTHARA_MAX_CAMINHO + 40], destino[MYTHARA_MAX_CAMINHO + 40];
    char pasta[MYTHARA_MAX_CAMINHO];
    juntar_caminho(pasta, sizeof(pasta), a->projeto.pasta_base, ".mythara/backups");
    for (int i = 8; i >= 0; --i) {
        char nome_origem[32], nome_destino[32];
        snprintf(nome_origem, sizeof(nome_origem), "autosave_%d.myr", i);
        snprintf(nome_destino, sizeof(nome_destino), "autosave_%d.myr", i + 1);
        juntar_caminho(origem, sizeof(origem), pasta, nome_origem);
        juntar_caminho(destino, sizeof(destino), pasta, nome_destino);
        rename(origem, destino);
    }
    TrabalhoAutosave *trabalho = calloc(1, sizeof(*trabalho));
    if (!trabalho || !serializar_projeto(&a->projeto, &trabalho->buffer)) {
        free(trabalho);
        mensagem_aplicativo(a, "Memoria insuficiente para o autosave.");
        a->ultimo_autosalvamento = agora_segundos();
        return;
    }
    copiar_texto(trabalho->caminho, sizeof(trabalho->caminho), a->caminho_autosave);
    trabalho->em_andamento = &a->autosave_em_andamento;
    atomic_store(&a->autosave_em_andamento, 1);
    if (!iniciar_autosave_thread(trabalho)) {
        atomic_store(&a->autosave_em_andamento, 0);
        free(trabalho->buffer.dados);
        free(trabalho);
        mensagem_aplicativo(a, "Nao foi possivel iniciar o autosave.");
    } else
        mensagem_aplicativo(a, "Snapshot v3 agendado em segundo plano.");
    a->ultimo_autosalvamento = agora_segundos();
}

static int recuperar_autosave(Aplicativo *a) {
    Projeto recuperado;
    char erro[192];
    memset(&recuperado, 0, sizeof(recuperado));
    if (!carregar_projeto_de(&recuperado, a->caminho_autosave, erro, sizeof(erro))) {
        mensagem_aplicativo(a, erro);
        return 0;
    }
    copiar_texto(recuperado.pasta_base, sizeof(recuperado.pasta_base), a->projeto.pasta_base);
    liberar_historico_global(&a->editor);
    liberar_recursos(a);
    liberar_projeto(&a->projeto);
    a->projeto = recuperado;
    recarregar_recursos(a);
    a->editor.alterado = 1;
    a->oferecer_recuperacao = 0;
    reiniciar_historico_global(a);
    mensagem_aplicativo(a, "Sessao recuperada do autosave.");
    return 1;
}

static int adicionar_evento(Aplicativo *a, const char *nome) {
    Projeto *p = &a->projeto;
    if (p->quantidade_eventos >= MYTHARA_MAX_EVENTOS ||
        !reservar_memoria((void **)&p->eventos, &p->capacidade_eventos, p->quantidade_eventos + 1,
                          sizeof(Evento), MYTHARA_MAX_EVENTOS))
        return -1;
    int i = p->quantidade_eventos++;
    Evento *e = &p->eventos[i];
    memset(e, 0, sizeof(*e));
    e->id = novo_identificador(p);
    e->ativo = 1;
    e->condicao_flag = -1;
    copiar_texto(e->nome, sizeof(e->nome), nome);
    a->editor.alterado = 1;
    return i;
}

static void adicionar_entidade(Aplicativo *a, int x, int y) {
    Mapa *m = &a->projeto.mapas[a->editor.mapa_atual];
    if (m->quantidade_entidades >= MYTHARA_MAX_ENTIDADES ||
        !reservar_entidades(m, m->quantidade_entidades + 1))
        return;
    int ev = adicionar_evento(a, "Novo evento");
    if (ev < 0)
        return;
    Entidade *n = &m->entidades[m->quantidade_entidades++];
    memset(n, 0, sizeof(*n));
    n->id = novo_identificador(&a->projeto);
    n->ativo = 1;
    n->x = x;
    n->y = y;
    n->evento = ev;
    n->evento_id = a->projeto.eventos[ev].id;
    n->cor = 9;
    copiar_texto(n->nome, sizeof(n->nome), "Novo NPC");
    a->editor.entidade_selecionada = m->quantidade_entidades - 1;
    a->editor.ferramenta = FERRAMENTA_SELECAO;
}

static Entidade *entidade_em(Mapa *m, int x, int y, int *indice) {
    for (int i = m->quantidade_entidades - 1; i >= 0; --i)
        if (m->entidades[i].ativo && m->entidades[i].x == x && m->entidades[i].y == y) {
            if (indice)
                *indice = i;
            return &m->entidades[i];
        }
    return NULL;
}

/* ========================================================================== */
/* Desenho e interacao do editor                                               */
/* ========================================================================== */

static void desenhar_tile_recurso(Aplicativo *a, Tela *t, Retangulo r, int tile, int colisao) {
    Imagem *img = NULL;
    for (int i = 0; i < a->projeto.quantidade_recursos; ++i)
        if (a->projeto.recursos[i].ativo && a->projeto.recursos[i].tipo != RECURSO_WAV &&
            a->imagens[i].pixels) {
            img = &a->imagens[i];
            break;
        }
    int tamanho_fonte = a->projeto.tamanho_tile ? a->projeto.tamanho_tile : MYTHARA_TAMANHO_TILE;
    if (img && img->largura >= tamanho_fonte && img->altura >= tamanho_fonte) {
        int colunas = img->largura / tamanho_fonte;
        int sx = (tile % colunas) * tamanho_fonte;
        int sy = (tile / colunas) * tamanho_fonte;
        if (sy + tamanho_fonte <= img->altura)
            desenhar_imagem(t, img, r, (Retangulo){sx, sy, tamanho_fonte, tamanho_fonte});
        else
            desenhar_grade_tile(t, r, tile, colisao);
        if (colisao)
            contornar_retangulo(t, r, cor_rgb(245, 75, 85));
    } else
        desenhar_grade_tile(t, r, tile, colisao);
}

static void editor_barra_superior(Aplicativo *a, Interface *ui) {
    Tela *t = ui->tela;
    int x = 8;
    preencher_retangulo(t, (Retangulo){0, 0, t->largura, 70}, tema_ativo.fundo);
    desenhar_texto(t, 12, 10, "MYTHARA 3", tema_ativo.destaque, 2);
    desenhar_texto(t, 112, 15, a->projeto.nome, cor_rgb(220, 226, 234), 1);
    const char *layouts[] = {"MAPA", "EVENTOS", "BANCO", "RECURSOS", "PLAY"};
    int lx = t->largura - 348;
    for (int i = 0; i < 5; ++i) {
        if (ui_botao(ui, (Retangulo){lx + i * 68, 7, 64, 24}, layouts[i],
                     a->editor.layout_ativo == i)) {
            a->editor.layout_ativo = i;
            if (i == LAYOUT_MAPA)
                a->modal = MODAL_NENHUM;
            else if (i == LAYOUT_EVENTOS)
                a->modal = MODAL_EVENTOS;
            else if (i == LAYOUT_BANCO)
                a->modal = MODAL_BANCO;
            else if (i == LAYOUT_RECURSOS)
                a->modal = MODAL_RECURSOS;
            else {
                memset(&a->jogo, 0, sizeof(a->jogo));
                iniciar_estado_jogo(&a->projeto, &a->jogo.estado);
                a->jogo.evento_ativo = -1;
                a->modo = MODO_JOGO;
            }
        }
    }
    struct {
        const char *nome;
        int largura;
    } botoes[] = {{"NOVO", 54},     {"ABRIR", 60},    {"SALVAR", 66},  {"SALVAR COMO", 92},
                  {"EXPORTAR", 78}, {"BANCO", 60},    {"EVENTOS", 66}, {"RECURSOS", 72},
                  {"TEMA", 54},     {"JOGAR F5", 78}, {"AJUDA", 60}};
    x = 8;
    for (size_t i = 0; i < sizeof(botoes) / sizeof(botoes[0]); ++i) {
        if (x + botoes[i].largura > t->largura)
            break;
        if (ui_botao(ui, (Retangulo){x, 38, botoes[i].largura, 25}, botoes[i].nome, 0)) {
            if (i == 0)
                a->modal = MODAL_NOVO;
            if (i == 1) {
                a->modal = MODAL_CAMINHO;
                a->acao_caminho = ACAO_ABRIR;
                copiar_texto(a->campo_caminho, sizeof(a->campo_caminho), a->caminho_projeto);
            }
            if (i == 2)
                salvar_projeto_atual(a);
            if (i == 3) {
                a->modal = MODAL_CAMINHO;
                a->acao_caminho = ACAO_SALVAR_COMO;
                copiar_texto(a->campo_caminho, sizeof(a->campo_caminho), a->caminho_projeto);
            }
            if (i == 4) {
                a->modal = MODAL_CAMINHO;
                a->acao_caminho = ACAO_EXPORTAR;
                copiar_texto(a->campo_caminho, sizeof(a->campo_caminho), "exportacao_mythara");
            }
            if (i == 5)
                a->modal = MODAL_BANCO;
            if (i == 6)
                a->modal = MODAL_EVENTOS;
            if (i == 7)
                a->modal = MODAL_RECURSOS;
            if (i == 8)
                a->modal = MODAL_TEMA;
            if (i == 9) {
                memset(&a->jogo, 0, sizeof(a->jogo));
                iniciar_estado_jogo(&a->projeto, &a->jogo.estado);
                a->jogo.evento_ativo = -1;
                a->modo = MODO_JOGO;
            }
            if (i == 10)
                a->modal = MODAL_AJUDA;
        }
        x += botoes[i].largura + 4;
    }
    if (ui->entrada->teclas_pressionadas[132]) {
        memset(&a->jogo, 0, sizeof(a->jogo));
        iniciar_estado_jogo(&a->projeto, &a->jogo.estado);
        a->jogo.evento_ativo = -1;
        a->modo = MODO_JOGO;
    }
    if (ui->entrada->controle && ui->entrada->teclas_pressionadas['S']) {
        if (ui->entrada->shift) {
            a->modal = MODAL_CAMINHO;
            a->acao_caminho = ACAO_SALVAR_COMO;
            copiar_texto(a->campo_caminho, sizeof(a->campo_caminho), a->caminho_projeto);
        } else
            salvar_projeto_atual(a);
    }
    if (ui->entrada->controle && ui->entrada->teclas_pressionadas['N'])
        a->modal = MODAL_NOVO;
    if (ui->entrada->controle && ui->entrada->teclas_pressionadas['O']) {
        a->modal = MODAL_CAMINHO;
        a->acao_caminho = ACAO_ABRIR;
        copiar_texto(a->campo_caminho, sizeof(a->campo_caminho), a->projeto.pasta_base);
    }
    if (ui->entrada->controle && ui->entrada->teclas_pressionadas['P']) {
        a->modal = MODAL_COMANDOS;
        a->editor.busca_comandos[0] = 0;
    }
}

static int texto_contem_sem_caixa(const char *texto, const char *busca) {
    if (!busca[0])
        return 1;
    for (size_t i = 0; texto[i]; ++i) {
        size_t j = 0;
        while (busca[j] && texto[i + j] &&
               tolower((unsigned char)texto[i + j]) == tolower((unsigned char)busca[j]))
            j++;
        if (!busca[j])
            return 1;
    }
    return 0;
}

static void editor_painel_esquerdo(Aplicativo *a, Interface *ui, Retangulo r) {
    Editor *e = &a->editor;
    Projeto *p = &a->projeto;
    int y = r.y + 34;
    ui_painel(ui->tela, r, "ARVORE DO PROJETO");
    ui_campo(ui, (Retangulo){r.x + 8, y, r.largura - 16, 27}, e->busca_arvore,
             sizeof(e->busca_arvore));
    y += 36;
    desenhar_texto(ui->tela, r.x + 9, y, "V MAPAS", tema_ativo.destaque, 1);
    y += 18;
    int mostrados = 0;
    for (int i = 0; i < p->quantidade_mapas && mostrados < 6; ++i)
        if (texto_contem_sem_caixa(p->mapas[i].nome, e->busca_arvore)) {
            char linha[64];
            snprintf(linha, sizeof(linha), "  # %s", p->mapas[i].nome);
            if (ui_botao(ui, (Retangulo){r.x + 8, y, r.largura - 16, 23}, linha,
                         e->mapa_atual == i)) {
                e->mapa_atual = i;
                e->entidade_selecionada = -1;
                e->camera_x = e->camera_y = 0;
            }
            y += 25;
            mostrados++;
        }
    if (ui_botao(ui, (Retangulo){r.x + 8, y, 88, 23}, "+ MAPA", 0) &&
        p->quantidade_mapas < MYTHARA_MAX_MAPAS &&
        reservar_memoria((void **)&p->mapas, &p->capacidade_mapas, p->quantidade_mapas + 1,
                         sizeof(Mapa), MYTHARA_MAX_MAPAS)) {
        int i = p->quantidade_mapas++;
        char nome[48];
        snprintf(nome, sizeof(nome), "Mapa %d", i + 1);
        iniciar_mapa(&p->mapas[i], nome, 20, 15);
        p->mapas[i].id = novo_identificador(p);
        for (int c = 0; c < p->mapas[i].quantidade_camadas; ++c)
            p->mapas[i].camadas[c].id = novo_identificador(p);
        e->mapa_atual = i;
        e->alterado = 1;
    }
    if (ui_botao(ui, (Retangulo){r.x + 102, y, 88, 23}, "- MAPA", 0) && p->quantidade_mapas > 1) {
        liberar_mapa(&p->mapas[e->mapa_atual]);
        memmove(&p->mapas[e->mapa_atual], &p->mapas[e->mapa_atual + 1],
                (size_t)(p->quantidade_mapas - e->mapa_atual - 1) * sizeof(Mapa));
        p->quantidade_mapas--;
        memset(&p->mapas[p->quantidade_mapas], 0, sizeof(Mapa));
        e->mapa_atual = limitar_int(e->mapa_atual, 0, p->quantidade_mapas - 1);
        p->mapa_inicial = limitar_int(p->mapa_inicial, 0, p->quantidade_mapas - 1);
        e->alterado = 1;
    }
    y += 32;
    char resumo[96];
    snprintf(resumo, sizeof(resumo), "> EVENTOS (%d)", p->quantidade_eventos);
    if (ui_botao(ui, (Retangulo){r.x + 8, y, r.largura - 16, 24}, resumo, 0))
        a->modal = MODAL_EVENTOS;
    y += 28;
    snprintf(resumo, sizeof(resumo), "> RECURSOS (%d)", p->quantidade_recursos);
    if (ui_botao(ui, (Retangulo){r.x + 8, y, r.largura - 16, 24}, resumo, 0))
        a->modal = MODAL_RECURSOS;
    y += 28;
    if (ui_botao(ui, (Retangulo){r.x + 8, y, r.largura - 16, 24}, "> BANCO DE DADOS", 0))
        a->modal = MODAL_BANCO;
    const char *ferr[] = {"PINCEL",    "BORRACHA", "COLISAO",   "ENTIDADE",    "SELECAO",
                          "RETANGULO", "LINHA",    "PREENCHER", "CONTA-GOTAS", "CARIMBO"};
    y = r.y + r.altura - 178;
    desenhar_texto(ui->tela, r.x + 8, y, "FERRAMENTAS", tema_ativo.texto_suave, 1);
    y += 15;
    int largura = (r.largura - 22) / 2;
    for (int i = 0; i < 10; ++i) {
        int bx = r.x + 8 + (i % 2) * (largura + 6), by = y + (i / 2) * 25;
        if (ui_botao(ui, (Retangulo){bx, by, largura, 22}, ferr[i], e->ferramenta == i))
            e->ferramenta = i;
    }
    if (ui_botao(ui, (Retangulo){r.x + 8, r.y + r.altura - 31, 88, 23}, "DESFAZER", 0))
        desfazer_edicao(a);
    if (ui_botao(ui, (Retangulo){r.x + 102, r.y + r.altura - 31, 88, 23}, "REFAZER", 0))
        refazer_edicao(a);
}

static void editar_celula(Aplicativo *a, int x, int y, int camada, int valor) {
    Editor *e = &a->editor;
    Mapa *m = &a->projeto.mapas[e->mapa_atual];
    if (x < 0 || y < 0 || x >= m->largura || y >= m->altura)
        return;
    if (camada < MYTHARA_MAX_CAMADAS && m->camadas[camada].bloqueada)
        return;
    int idx = y * m->largura + x;
    int anterior = valor_celula_mapa(m, camada, idx);
    definir_celula_mapa(m, camada, idx, valor);
    registrar_edicao(e, e->mapa_atual, camada, idx, anterior, valor);
}

static void preencher_conectado(Aplicativo *a, int sx, int sy, int camada, int novo) {
    Mapa *m = &a->projeto.mapas[a->editor.mapa_atual];
    int total = m->largura * m->altura, alvo = valor_celula_mapa(m, camada, sy * m->largura + sx);
    if (alvo == novo)
        return;
    int *fila = malloc((size_t)total * sizeof(int));
    uint8_t *visto = calloc((size_t)total, 1);
    if (!fila || !visto) {
        free(fila);
        free(visto);
        return;
    }
    int inicio = 0, fim = 0;
    fila[fim++] = sy * m->largura + sx;
    visto[sy * m->largura + sx] = 1;
    while (inicio < fim) {
        int idx = fila[inicio++], x = idx % m->largura, y = idx / m->largura;
        if (valor_celula_mapa(m, camada, idx) != alvo)
            continue;
        editar_celula(a, x, y, camada, novo);
        const int dx[4] = {-1, 1, 0, 0}, dy[4] = {0, 0, -1, 1};
        for (int k = 0; k < 4; ++k) {
            int nx = x + dx[k], ny = y + dy[k];
            if (nx >= 0 && ny >= 0 && nx < m->largura && ny < m->altura) {
                int ni = ny * m->largura + nx;
                if (!visto[ni] && valor_celula_mapa(m, camada, ni) == alvo) {
                    visto[ni] = 1;
                    fila[fim++] = ni;
                }
            }
        }
    }
    free(fila);
    free(visto);
}

static void pintar_linha(Aplicativo *a, int x0, int y0, int x1, int y1, int camada, int valor) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1,
        erro = dx + dy;
    for (;;) {
        editar_celula(a, x0, y0, camada, valor);
        if (x0 == x1 && y0 == y1)
            break;
        int e2 = 2 * erro;
        if (e2 >= dy) {
            erro += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            erro += dx;
            y0 += sy;
        }
    }
}

static void pintar_retangulo_area(Aplicativo *a, int x0, int y0, int x1, int y1, int camada,
                                  int valor) {
    if (x0 > x1) {
        int t = x0;
        x0 = x1;
        x1 = t;
    }
    if (y0 > y1) {
        int t = y0;
        y0 = y1;
        y1 = t;
    }
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x)
            editar_celula(a, x, y, camada, valor);
}

static void atualizar_autotile(Aplicativo *a, int cx, int cy) {
    Editor *e = &a->editor;
    Mapa *m = &a->projeto.mapas[e->mapa_atual];
    int base = e->tile, camada = e->camada;
    for (int y = cy - 1; y <= cy + 1; ++y)
        for (int x = cx - 1; x <= cx + 1; ++x) {
            if (x < 0 || y < 0 || x >= m->largura || y >= m->altura)
                continue;
            int idx = y * m->largura + x, v = valor_celula_mapa(m, camada, idx);
            if (v < base || v > base + 15)
                continue;
            int mascara = 0;
            if (y > 0) {
                int n = valor_celula_mapa(m, camada, (y - 1) * m->largura + x);
                if (n >= base && n <= base + 15)
                    mascara |= 1;
            }
            if (x < m->largura - 1) {
                int n = valor_celula_mapa(m, camada, y * m->largura + x + 1);
                if (n >= base && n <= base + 15)
                    mascara |= 2;
            }
            if (y < m->altura - 1) {
                int n = valor_celula_mapa(m, camada, (y + 1) * m->largura + x);
                if (n >= base && n <= base + 15)
                    mascara |= 4;
            }
            if (x > 0) {
                int n = valor_celula_mapa(m, camada, y * m->largura + x - 1);
                if (n >= base && n <= base + 15)
                    mascara |= 8;
            }
            editar_celula(a, x, y, camada, base + mascara);
        }
}

static void copiar_selecao_para_carimbo(Aplicativo *a) {
    Editor *e = &a->editor;
    Mapa *m = &a->projeto.mapas[e->mapa_atual];
    if (!e->selecao_ativa)
        return;
    int x0 = e->selecao_x0, x1 = e->selecao_x1, y0 = e->selecao_y0, y1 = e->selecao_y1;
    if (x0 > x1) {
        int t = x0;
        x0 = x1;
        x1 = t;
    }
    if (y0 > y1) {
        int t = y0;
        y0 = y1;
        y1 = t;
    }
    e->largura_carimbo = limitar_int(x1 - x0 + 1, 1, 16);
    e->altura_carimbo = limitar_int(y1 - y0 + 1, 1, 16);
    e->camadas_carimbo = m->quantidade_camadas;
    for (int c = 0; c < m->quantidade_camadas; ++c)
        for (int y = 0; y < e->altura_carimbo; ++y)
            for (int x = 0; x < e->largura_carimbo; ++x)
                e->carimbo[c][y * 16 + x] = m->camadas[c].tiles[(y0 + y) * m->largura + x0 + x];
    e->ferramenta = FERRAMENTA_CARIMBO;
}

static void copiar_area_transferencia(Aplicativo *a, int recortar) {
    Editor *e = &a->editor;
    Mapa *m = &a->projeto.mapas[e->mapa_atual];
    if (!e->selecao_ativa)
        return;
    int x0 = e->selecao_x0, x1 = e->selecao_x1, y0 = e->selecao_y0, y1 = e->selecao_y1;
    if (x0 > x1) {
        int t = x0;
        x0 = x1;
        x1 = t;
    }
    if (y0 > y1) {
        int t = y0;
        y0 = y1;
        y1 = t;
    }
    x0 = limitar_int(x0, 0, m->largura - 1);
    x1 = limitar_int(x1, 0, m->largura - 1);
    y0 = limitar_int(y0, 0, m->altura - 1);
    y1 = limitar_int(y1, 0, m->altura - 1);
    e->clipboard_largura = limitar_int(x1 - x0 + 1, 1, 64);
    e->clipboard_altura = limitar_int(y1 - y0 + 1, 1, 64);
    e->clipboard_camadas = m->quantidade_camadas;
    e->clipboard_entidades = 0;
    for (int y = 0; y < e->clipboard_altura; ++y)
        for (int x = 0; x < e->clipboard_largura; ++x) {
            int origem = (y0 + y) * m->largura + x0 + x, destino = y * 64 + x;
            e->clipboard_colisoes[destino] = m->colisoes[origem];
            for (int c = 0; c < m->quantidade_camadas; ++c)
                e->clipboard_tiles[c][destino] = m->camadas[c].tiles[origem];
            if (recortar) {
                for (int c = 0; c < m->quantidade_camadas; ++c)
                    editar_celula(a, x0 + x, y0 + y, c, 0);
                editar_celula(a, x0 + x, y0 + y, MYTHARA_MAX_CAMADAS, 0);
            }
        }
    for (int i = 0; i < m->quantidade_entidades; ++i) {
        Entidade *n = &m->entidades[i];
        if (n->x >= x0 && n->x <= x1 && n->y >= y0 && n->y <= y1 && e->clipboard_entidades < 256) {
            Entidade copia = *n;
            copia.x -= x0;
            copia.y -= y0;
            e->clipboard_lista_entidades[e->clipboard_entidades++] = copia;
        }
    }
    if (recortar) {
        for (int i = m->quantidade_entidades - 1; i >= 0; --i) {
            Entidade *n = &m->entidades[i];
            if (n->x >= x0 && n->x <= x1 && n->y >= y0 && n->y <= y1) {
                memmove(n, n + 1, (size_t)(m->quantidade_entidades - i - 1) * sizeof(*n));
                m->quantidade_entidades--;
            }
        }
        e->alterado = 1;
    }
    mensagem_aplicativo(a, recortar ? "Selecao recortada." : "Selecao copiada.");
}

static void colar_area_transferencia(Aplicativo *a, int destino_x, int destino_y) {
    Editor *e = &a->editor;
    Mapa *m = &a->projeto.mapas[e->mapa_atual];
    if (e->clipboard_largura <= 0)
        return;
    for (int y = 0; y < e->clipboard_altura; ++y)
        for (int x = 0; x < e->clipboard_largura; ++x) {
            int dx = destino_x + x, dy = destino_y + y;
            if (dx < 0 || dy < 0 || dx >= m->largura || dy >= m->altura)
                continue;
            int origem = y * 64 + x;
            for (int c = 0; c < e->clipboard_camadas && c < m->quantidade_camadas; ++c)
                editar_celula(a, dx, dy, c, e->clipboard_tiles[c][origem]);
            editar_celula(a, dx, dy, MYTHARA_MAX_CAMADAS, e->clipboard_colisoes[origem]);
        }
    for (int i = 0; i < e->clipboard_entidades && m->quantidade_entidades < MYTHARA_MAX_ENTIDADES;
         ++i) {
        Entidade copia = e->clipboard_lista_entidades[i];
        copia.x += destino_x;
        copia.y += destino_y;
        if (copia.x < 0 || copia.y < 0 || copia.x >= m->largura || copia.y >= m->altura)
            continue;
        if (!reservar_entidades(m, m->quantidade_entidades + 1))
            break;
        copia.id = novo_identificador(&a->projeto);
        m->entidades[m->quantidade_entidades++] = copia;
    }
    e->selecao_x0 = destino_x;
    e->selecao_y0 = destino_y;
    e->selecao_x1 = limitar_int(destino_x + e->clipboard_largura - 1, 0, m->largura - 1);
    e->selecao_y1 = limitar_int(destino_y + e->clipboard_altura - 1, 0, m->altura - 1);
    e->selecao_ativa = 1;
    e->alterado = 1;
    mensagem_aplicativo(a, "Conteudo colado com novos IDs de entidade.");
}

static void apagar_selecao_editor(Aplicativo *a) {
    Editor *e = &a->editor;
    if (!e->selecao_ativa)
        return;
    copiar_area_transferencia(a, 1);
    e->clipboard_largura = 0;
    mensagem_aplicativo(a, "Selecao apagada.");
}

static void editor_canvas(Aplicativo *a, Interface *ui, Retangulo area) {
    Editor *e = &a->editor;
    Mapa *m = &a->projeto.mapas[e->mapa_atual];
    Entrada *in = ui->entrada;
    preencher_retangulo(ui->tela, area, cor_rgb(20, 23, 29));
    if (in->teclas_pressionadas[128] || in->teclas_pressionadas['A'])
        e->camera_x += 32;
    if (in->teclas_pressionadas[129] || in->teclas_pressionadas['D'])
        e->camera_x -= 32;
    if (in->teclas_pressionadas[130] || in->teclas_pressionadas['W'])
        e->camera_y += 32;
    if (in->teclas_pressionadas[131] || in->teclas_pressionadas['S'])
        e->camera_y -= 32;
    int tamanho = e->zoom;
    int ox = area.x + e->camera_x, oy = area.y + e->camera_y;
    if (ponto_em_retangulo(in->mouse_x, in->mouse_y, area) && in->roda) {
        int antigo = tamanho;
        e->zoom = limitar_int(e->zoom + in->roda * 4, 16, 64);
        e->camera_x -= (in->mouse_x - ox) * (e->zoom - antigo) / antigo;
        e->camera_y -= (in->mouse_y - oy) * (e->zoom - antigo) / antigo;
        tamanho = e->zoom;
    }
    for (int y = 0; y < m->altura; ++y)
        for (int x = 0; x < m->largura; ++x) {
            int indice = y * m->largura + x;
            Retangulo q = {ox + x * tamanho, oy + y * tamanho, tamanho, tamanho};
            if (q.x + q.largura < area.x || q.y + q.altura < area.y ||
                q.x >= area.x + area.largura || q.y >= area.y + area.altura)
                continue;
            if (m->camadas[0].visivel)
                desenhar_tile_recurso(a, ui->tela, q, m->camadas[0].tiles[indice], 0);
            else
                preencher_retangulo(ui->tela, q, cor_rgb(24, 27, 34));
            for (int c = 1; c < m->quantidade_camadas; ++c)
                if (m->camadas[c].visivel && m->camadas[c].tiles[indice]) {
                    Retangulo d = {q.x + 2 + c, q.y + 2 + c, tamanho - 4 - c * 2,
                                   tamanho - 4 - c * 2};
                    contornar_retangulo(ui->tela, d,
                                        paleta_tiles[m->camadas[c].tiles[indice] & 15]);
                }
            if (e->ferramenta == FERRAMENTA_COLISAO && m->colisoes[indice]) {
                for (int k = 0; k < tamanho; k += 8)
                    preencher_retangulo(ui->tela, (Retangulo){q.x + k, q.y + k % tamanho, 5, 2},
                                        tema_ativo.perigo);
            }
            contornar_retangulo(ui->tela, q, cor_rgb(37, 43, 49));
        }
    for (int i = 0; i < m->quantidade_entidades; ++i)
        if (m->entidades[i].ativo) {
            Entidade *n = &m->entidades[i];
            Retangulo q = {ox + n->x * tamanho + 3, oy + n->y * tamanho + 3, tamanho - 6,
                           tamanho - 6};
            preencher_retangulo(ui->tela, q, paleta_tiles[n->cor & 15]);
            contornar_retangulo(ui->tela, q,
                                i == e->entidade_selecionada ? cor_rgb(255, 224, 93)
                                                             : cor_rgb(235, 238, 244));
            desenhar_texto(ui->tela, q.x + q.largura / 2 - 3, q.y + q.altura / 2 - 3, "E",
                           cor_rgb(20, 23, 29), 1);
        }
    if (ponto_em_retangulo(in->mouse_x, in->mouse_y, area)) {
        int tx = (in->mouse_x - ox) / tamanho, ty = (in->mouse_y - oy) / tamanho;
        if (in->mouse_x < ox)
            tx = -1;
        if (in->mouse_y < oy)
            ty = -1;
        if (tx >= 0 && ty >= 0 && tx < m->largura && ty < m->altura) {
            e->cursor_tile_x = tx;
            e->cursor_tile_y = ty;
            if (in->mouse_pressionado) {
                e->canvas_em_foco = 1;
                ui->id_foco = 0;
            }
            Retangulo cursor = {ox + tx * tamanho, oy + ty * tamanho, tamanho, tamanho};
            contornar_retangulo(ui->tela, cursor, cor_rgb(245, 214, 100));
            if (in->mouse_baixo &&
                (e->ferramenta == FERRAMENTA_PINCEL || e->ferramenta == FERRAMENTA_BORRACHA ||
                 e->ferramenta == FERRAMENTA_COLISAO)) {
                int camada = e->ferramenta == FERRAMENTA_COLISAO ? MYTHARA_MAX_CAMADAS : e->camada;
                int anterior = valor_celula_mapa(m, camada, ty * m->largura + tx);
                int novo = e->ferramenta == FERRAMENTA_BORRACHA
                               ? 0
                               : (camada == MYTHARA_MAX_CAMADAS ? !anterior : e->tile);
                editar_celula(a, tx, ty, camada, novo);
                if (e->autotile && camada < MYTHARA_MAX_CAMADAS)
                    atualizar_autotile(a, tx, ty);
            } else if (in->mouse_pressionado && e->ferramenta == FERRAMENTA_ENTIDADE)
                adicionar_entidade(a, tx, ty);
            else if (e->ferramenta == FERRAMENTA_SELECAO) {
                if (in->mouse_pressionado) {
                    e->selecao_x0 = e->selecao_x1 = tx;
                    e->selecao_y0 = e->selecao_y1 = ty;
                    e->selecao_ativa = 1;
                    e->arrastando_forma = 1;
                    int idx = -1;
                    entidade_em(m, tx, ty, &idx);
                    e->entidade_selecionada = idx;
                }
                if (e->arrastando_forma && in->mouse_baixo) {
                    e->selecao_x1 = tx;
                    e->selecao_y1 = ty;
                }
                if (in->mouse_solto)
                    e->arrastando_forma = 0;
            } else if (in->mouse_pressionado && e->ferramenta == FERRAMENTA_PREENCHER)
                preencher_conectado(a, tx, ty, e->camada, e->tile);
            else if (in->mouse_pressionado && e->ferramenta == FERRAMENTA_CONTA_GOTAS) {
                e->tile = valor_celula_mapa(m, e->camada, ty * m->largura + tx);
                e->ferramenta = FERRAMENTA_PINCEL;
            } else if (e->ferramenta == FERRAMENTA_RETANGULO || e->ferramenta == FERRAMENTA_LINHA) {
                if (in->mouse_pressionado) {
                    e->selecao_x0 = e->selecao_x1 = tx;
                    e->selecao_y0 = e->selecao_y1 = ty;
                    e->arrastando_forma = 1;
                }
                if (e->arrastando_forma && in->mouse_baixo) {
                    e->selecao_x1 = tx;
                    e->selecao_y1 = ty;
                }
                if (e->arrastando_forma && in->mouse_solto) {
                    if (e->ferramenta == FERRAMENTA_RETANGULO)
                        pintar_retangulo_area(a, e->selecao_x0, e->selecao_y0, tx, ty, e->camada,
                                              e->tile);
                    else
                        pintar_linha(a, e->selecao_x0, e->selecao_y0, tx, ty, e->camada, e->tile);
                    e->arrastando_forma = 0;
                }
            } else if (in->mouse_pressionado && e->ferramenta == FERRAMENTA_CARIMBO &&
                       e->largura_carimbo > 0) {
                for (int c = 0; c < e->camadas_carimbo && c < m->quantidade_camadas; ++c)
                    for (int yy = 0; yy < e->altura_carimbo; ++yy)
                        for (int xx = 0; xx < e->largura_carimbo; ++xx)
                            editar_celula(a, tx + xx, ty + yy, c, e->carimbo[c][yy * 16 + xx]);
            }
        }
    }
    if (e->clipboard_largura > 0 && e->canvas_em_foco && e->cursor_tile_x >= 0 &&
        e->cursor_tile_y >= 0) {
        Retangulo previa = {ox + e->cursor_tile_x * tamanho, oy + e->cursor_tile_y * tamanho,
                            e->clipboard_largura * tamanho, e->clipboard_altura * tamanho};
        contornar_retangulo(ui->tela, previa, tema_ativo.destaque);
    }
    if (e->selecao_ativa || e->arrastando_forma) {
        int x0 = e->selecao_x0, x1 = e->selecao_x1, y0 = e->selecao_y0, y1 = e->selecao_y1;
        if (x0 > x1) {
            int v = x0;
            x0 = x1;
            x1 = v;
        }
        if (y0 > y1) {
            int v = y0;
            y0 = y1;
            y1 = v;
        }
        contornar_retangulo(ui->tela,
                            (Retangulo){ox + x0 * tamanho, oy + y0 * tamanho,
                                        (x1 - x0 + 1) * tamanho, (y1 - y0 + 1) * tamanho},
                            tema_ativo.aviso);
    }
    contornar_retangulo(ui->tela, area, cor_rgb(68, 76, 91));
}

static int redimensionar_mapa(Mapa *m, int largura, int altura) {
    if (largura == m->largura && altura == m->altura)
        return 1;
    size_t total = (size_t)largura * altura;
    uint16_t *novas[MYTHARA_MAX_CAMADAS] = {0};
    uint8_t *colisoes = calloc(total, 1);
    if (!colisoes)
        return 0;
    for (int c = 0; c < m->quantidade_camadas; ++c) {
        novas[c] = calloc(total, sizeof(uint16_t));
        if (!novas[c]) {
            for (int k = 0; k < c; ++k)
                free(novas[k]);
            free(colisoes);
            return 0;
        }
    }
    int cw = largura < m->largura ? largura : m->largura,
        ch = altura < m->altura ? altura : m->altura;
    for (int y = 0; y < ch; ++y) {
        for (int c = 0; c < m->quantidade_camadas; ++c)
            memcpy(novas[c] + (size_t)y * largura, m->camadas[c].tiles + (size_t)y * m->largura,
                   (size_t)cw * sizeof(uint16_t));
        memcpy(colisoes + (size_t)y * largura, m->colisoes + (size_t)y * m->largura, (size_t)cw);
    }
    for (int c = 0; c < m->quantidade_camadas; ++c) {
        free(m->camadas[c].tiles);
        m->camadas[c].tiles = novas[c];
    }
    free(m->colisoes);
    m->colisoes = colisoes;
    m->largura = largura;
    m->altura = altura;
    for (int i = 0; i < m->quantidade_entidades; ++i) {
        m->entidades[i].x = limitar_int(m->entidades[i].x, 0, largura - 1);
        m->entidades[i].y = limitar_int(m->entidades[i].y, 0, altura - 1);
    }
    return 1;
}

static void editor_inspetor(Aplicativo *a, Interface *ui, Retangulo r) {
    Editor *e = &a->editor;
    Mapa *m = &a->projeto.mapas[e->mapa_atual];
    int y = r.y + 34;
    e->camada = limitar_int(e->camada, 0, m->quantidade_camadas - 1);
    ui_painel(ui->tela, r, "INSPETOR");
    desenhar_texto(ui->tela, r.x + 10, y, "CAMADAS", tema_ativo.destaque, 1);
    y += 16;
    for (int c = 0; c < m->quantidade_camadas && c < 8; ++c) {
        char linha[64];
        snprintf(linha, sizeof(linha), "%s%s %s", m->camadas[c].visivel ? "O" : "-",
                 m->camadas[c].bloqueada ? "X" : " ", m->camadas[c].nome);
        if (ui_botao(ui, (Retangulo){r.x + 10, y, r.largura - 20, 22}, linha, e->camada == c))
            e->camada = c;
        y += 24;
    }
    if (ui_botao(ui, (Retangulo){r.x + 10, y, 52, 22}, "+", 0) &&
        m->quantidade_camadas < MYTHARA_MAX_CAMADAS) {
        int c = m->quantidade_camadas++;
        CamadaMapa *nova = &m->camadas[c];
        memset(nova, 0, sizeof(*nova));
        nova->id = novo_identificador(&a->projeto);
        snprintf(nova->nome, sizeof(nova->nome), "Camada %d", c + 1);
        nova->visivel = 1;
        nova->tiles = calloc((size_t)m->largura * m->altura, sizeof(uint16_t));
        e->camada = c;
        e->alterado = 1;
    }
    if (ui_botao(ui, (Retangulo){r.x + 68, y, 52, 22}, "-", 0) && m->quantidade_camadas > 1) {
        int c = limitar_int(e->camada, 0, m->quantidade_camadas - 1);
        free(m->camadas[c].tiles);
        memmove(&m->camadas[c], &m->camadas[c + 1],
                (size_t)(m->quantidade_camadas - c - 1) * sizeof(CamadaMapa));
        m->quantidade_camadas--;
        memset(&m->camadas[m->quantidade_camadas], 0, sizeof(CamadaMapa));
        e->camada = limitar_int(c, 0, m->quantidade_camadas - 1);
        e->alterado = 1;
    }
    if (ui_botao(ui, (Retangulo){r.x + 126, y, 52, 22}, "VIS", m->camadas[e->camada].visivel))
        m->camadas[e->camada].visivel = !m->camadas[e->camada].visivel;
    if (ui_botao(ui, (Retangulo){r.x + 184, y, 52, 22}, "TRAVA", m->camadas[e->camada].bloqueada))
        m->camadas[e->camada].bloqueada = !m->camadas[e->camada].bloqueada;
    y += 31;
    ui_campo(ui, (Retangulo){r.x + 10, y, r.largura - 20, 25}, m->camadas[e->camada].nome,
             sizeof(m->camadas[e->camada].nome));
    y += 34;
    if (e->entidade_selecionada >= 0 && e->entidade_selecionada < m->quantidade_entidades) {
        Entidade *n = &m->entidades[e->entidade_selecionada];
        desenhar_texto(ui->tela, r.x + 10, y, "ENTIDADE", cor_rgb(139, 196, 231), 1);
        y += 18;
        ui_campo(ui, (Retangulo){r.x + 10, y, r.largura - 20, 27}, n->nome, sizeof(n->nome));
        y += 34;
        n->tipo =
            ui_numero(ui, (Retangulo){r.x + 10, y, r.largura - 20, 27}, "TIPO", n->tipo, 0, 3);
        y += 33;
        desenhar_texto(ui->tela, r.x + 12, y, nomes_entidades[n->tipo], cor_rgb(211, 220, 230), 1);
        y += 21;
        n->x = ui_numero(ui, (Retangulo){r.x + 10, y, r.largura - 20, 27}, "X", n->x, 0,
                         m->largura - 1);
        y += 32;
        n->y = ui_numero(ui, (Retangulo){r.x + 10, y, r.largura - 20, 27}, "Y", n->y, 0,
                         m->altura - 1);
        y += 32;
        n->evento = ui_numero(ui, (Retangulo){r.x + 10, y, r.largura - 20, 27}, "EVENTO", n->evento,
                              0, MYTHARA_MAX_EVENTOS - 1);
        y += 32;
        n->cor = ui_numero(ui, (Retangulo){r.x + 10, y, r.largura - 20, 27}, "COR", n->cor, 0, 15);
        y += 35;
        if (ui_botao(ui, (Retangulo){r.x + 10, y, r.largura - 20, 27}, "EDITAR EVENTO", 0)) {
            e->evento_selecionado = n->evento;
            a->modal = MODAL_EVENTOS;
        }
        y += 34;
        if (ui_botao(ui, (Retangulo){r.x + 10, y, r.largura - 20, 27}, "EXCLUIR ENTIDADE", 0)) {
            memmove(n, n + 1,
                    (size_t)(m->quantidade_entidades - e->entidade_selecionada - 1) * sizeof(*n));
            m->quantidade_entidades--;
            e->entidade_selecionada = -1;
            e->alterado = 1;
        }
    } else {
        desenhar_texto(ui->tela, r.x + 10, y, "PROJETO", cor_rgb(139, 196, 231), 1);
        y += 18;
        ui_campo(ui, (Retangulo){r.x + 10, y, r.largura - 20, 27}, a->projeto.nome,
                 sizeof(a->projeto.nome));
        y += 34;
        ui_campo(ui, (Retangulo){r.x + 10, y, r.largura - 20, 27}, a->projeto.autor,
                 sizeof(a->projeto.autor));
        y += 39;
        desenhar_texto(ui->tela, r.x + 10, y, "MAPA ATUAL", cor_rgb(139, 196, 231), 1);
        y += 17;
        ui_campo(ui, (Retangulo){r.x + 10, y, r.largura - 20, 27}, m->nome, sizeof(m->nome));
        y += 34;
        char tile_rotulo[48];
        snprintf(tile_rotulo, sizeof(tile_rotulo), "TILE DO PROJETO: %d", a->projeto.tamanho_tile);
        if (ui_botao(ui, (Retangulo){r.x + 10, y, r.largura - 20, 27}, tile_rotulo, 0)) {
            int opcoes[] = {16, 24, 32, 48};
            int atual = 0;
            for (int k = 0; k < 4; ++k)
                if (opcoes[k] == a->projeto.tamanho_tile)
                    atual = k;
            a->projeto.tamanho_tile = opcoes[(atual + 1) % 4];
            e->alterado = 1;
        }
        y += 32;
        a->projeto.batalha_lateral = ui_checkbox(ui, (Retangulo){r.x + 10, y, r.largura - 20, 20},
                                                 "BATALHA LATERAL", a->projeto.batalha_lateral);
        y += 27;
        m->regiao_encontro =
            ui_numero(ui, (Retangulo){r.x + 10, y, r.largura - 20, 27}, "ENCONTRO (-1 SEM)",
                      m->regiao_encontro, -1,
                      a->projeto.quantidade_inimigos ? a->projeto.quantidade_inimigos - 1 : -1);
        y += 32;
        int nova_largura = ui_numero(ui, (Retangulo){r.x + 10, y, r.largura - 20, 27}, "LARGURA",
                                     m->largura, 4, MYTHARA_MAX_LARGURA_MAPA);
        y += 32;
        int nova_altura = ui_numero(ui, (Retangulo){r.x + 10, y, r.largura - 20, 27}, "ALTURA",
                                    m->altura, 4, MYTHARA_MAX_ALTURA_MAPA);
        y += 32;
        if (nova_largura != m->largura || nova_altura != m->altura) {
            if (redimensionar_mapa(m, nova_largura, nova_altura))
                e->alterado = 1;
        }
        a->projeto.inicio_x = ui_numero(ui, (Retangulo){r.x + 10, y, r.largura - 20, 27},
                                        "INICIO X", a->projeto.inicio_x, 0, m->largura - 1);
        y += 32;
        a->projeto.inicio_y = ui_numero(ui, (Retangulo){r.x + 10, y, r.largura - 20, 27},
                                        "INICIO Y", a->projeto.inicio_y, 0, m->altura - 1);
        y += 32;
        if (ui_botao(ui, (Retangulo){r.x + 10, y, r.largura - 20, 27}, "USAR COMO MAPA INICIAL",
                     a->projeto.mapa_inicial == e->mapa_atual)) {
            a->projeto.mapa_inicial = e->mapa_atual;
            e->alterado = 1;
        }
        y += 38;
        desenhar_texto(ui->tela, r.x + 10, y, "CLIQUE EM UMA ENTIDADE", cor_rgb(150, 158, 172), 1);
        desenhar_texto(ui->tela, r.x + 10, y + 13, "PARA EDITAR SEUS DADOS.",
                       cor_rgb(150, 158, 172), 1);
    }
}

/* ========================================================================== */
/* Janelas modais do editor                                                    */
/* ========================================================================== */

static Retangulo iniciar_modal(Interface *ui, const char *titulo, int largura, int altura) {
    preencher_retangulo(ui->tela, (Retangulo){0, 0, ui->tela->largura, ui->tela->altura},
                        cor_rgb(14, 17, 23));
    largura = limitar_int(largura, 240, ui->tela->largura - 20);
    altura = limitar_int(altura, 160, ui->tela->altura - 20);
    Retangulo r = {(ui->tela->largura - largura) / 2, (ui->tela->altura - altura) / 2, largura,
                   altura};
    ui_painel(ui->tela, r, titulo);
    return r;
}

static void modal_caminho(Aplicativo *a, Interface *ui) {
    const char *titulo = a->acao_caminho == ACAO_ABRIR
                             ? "ABRIR PROJETO"
                             : (a->acao_caminho == ACAO_EXPORTAR ? "EXPORTAR JOGO" : "SALVAR COMO");
    int largura = limitar_int(ui->tela->largura - 30, 520, 780),
        altura = limitar_int(ui->tela->altura - 30, 390, 540);
    Retangulo r = iniciar_modal(ui, titulo, largura, altura);
    if (!a->pasta_navegador[0]) {
        if (strstr(a->campo_caminho, ".myr")) {
            pasta_do_arquivo(a->campo_caminho, a->pasta_navegador, sizeof(a->pasta_navegador));
            copiar_texto(a->arquivo_navegador, sizeof(a->arquivo_navegador),
                         nome_base(a->campo_caminho));
        } else {
            copiar_texto(a->pasta_navegador, sizeof(a->pasta_navegador), ".");
            copiar_texto(a->arquivo_navegador, sizeof(a->arquivo_navegador),
                         a->campo_caminho[0]
                             ? nome_base(a->campo_caminho)
                             : (a->acao_caminho == ACAO_EXPORTAR ? "meu_jogo" : "projeto.myr"));
        }
    }
    desenhar_texto(ui->tela, r.x + 18, r.y + 42, "PASTA", tema_ativo.texto_suave, 1);
    ui_campo(ui, (Retangulo){r.x + 18, r.y + 57, r.largura - 132, 29}, a->pasta_navegador,
             sizeof(a->pasta_navegador));
    if (ui_botao(ui, (Retangulo){r.x + r.largura - 106, r.y + 57, 88, 29}, "SUBIR", 0))
        subir_diretorio(a->pasta_navegador);
    ItemDiretorio itens[128];
    int quantidade = listar_diretorio(a->pasta_navegador, itens, 128), y = r.y + 101,
        visiveis = (r.altura - 210) / 27;
    if (visiveis < 5)
        visiveis = 5;
    for (int i = 0; i < quantidade && i < visiveis; ++i) {
        char linha[300];
        snprintf(linha, sizeof(linha), "%s %.255s", itens[i].diretorio ? ">" : " ", itens[i].nome);
        if (ui_botao(ui, (Retangulo){r.x + 18, y, r.largura - 36, 24}, linha,
                     !itens[i].diretorio && !strcmp(a->arquivo_navegador, itens[i].nome))) {
            if (itens[i].diretorio) {
                char proxima[MYTHARA_MAX_CAMINHO];
                if (juntar_caminho(proxima, sizeof(proxima), a->pasta_navegador, itens[i].nome))
                    copiar_texto(a->pasta_navegador, sizeof(a->pasta_navegador), proxima);
            } else
                copiar_texto(a->arquivo_navegador, sizeof(a->arquivo_navegador), itens[i].nome);
        }
        y += 27;
    }
    y = r.y + r.altura - 94;
    desenhar_texto(ui->tela, r.x + 18, y, "NOME", tema_ativo.texto_suave, 1);
    ui_campo(ui, (Retangulo){r.x + 70, y - 7, r.largura - 300, 29}, a->arquivo_navegador,
             sizeof(a->arquivo_navegador));
    if (ui_botao(ui, (Retangulo){r.x + r.largura - 215, y - 7, 92, 29}, "NOVA PASTA", 0)) {
        char nova[MYTHARA_MAX_CAMINHO];
        if (juntar_caminho(nova, sizeof(nova), a->pasta_navegador, a->arquivo_navegador) &&
            criar_diretorios(nova))
            copiar_texto(a->pasta_navegador, sizeof(a->pasta_navegador), nova);
    }
    if (ui_botao(ui, (Retangulo){r.x + r.largura - 218, r.y + r.altura - 46, 92, 28}, "CANCELAR",
                 0)) {
        a->modal = MODAL_NENHUM;
        a->pasta_navegador[0] = 0;
    }
    if (ui_botao(ui, (Retangulo){r.x + r.largura - 116, r.y + r.altura - 46, 96, 28}, "CONFIRMAR",
                 0)) {
        char escolhido[MYTHARA_MAX_CAMINHO];
        juntar_caminho(escolhido, sizeof(escolhido), a->pasta_navegador, a->arquivo_navegador);
        copiar_texto(a->campo_caminho, sizeof(a->campo_caminho), escolhido);
        if (a->acao_caminho == ACAO_ABRIR)
            abrir_projeto_atual(a, escolhido);
        else if (a->acao_caminho == ACAO_SALVAR_COMO) {
            if (!strstr(escolhido, ".myr"))
                strncat(escolhido, ".myr", sizeof(escolhido) - strlen(escolhido) - 1);
            copiar_texto(a->caminho_projeto, sizeof(a->caminho_projeto), escolhido);
            salvar_projeto_atual(a);
        } else {
            char erro[192];
            if (exportar_projeto(&a->projeto, escolhido, erro, sizeof(erro)))
                mensagem_aplicativo(a, "Jogo nativo exportado com sucesso.");
            else
                mensagem_aplicativo(a, erro);
        }
        a->modal = MODAL_NENHUM;
        a->pasta_navegador[0] = 0;
    }
}

static void modal_novo(Aplicativo *a, Interface *ui) {
    Retangulo r = iniciar_modal(ui, "NOVO PROJETO", 470, 170);
    desenhar_texto(ui->tela, r.x + 22, r.y + 52, "SUBSTITUIR O PROJETO ATUAL?",
                   cor_rgb(226, 230, 237), 1);
    if (a->editor.alterado)
        desenhar_texto(ui->tela, r.x + 22, r.y + 72, "HA ALTERACOES NAO SALVAS.",
                       cor_rgb(235, 174, 84), 1);
    if (ui_botao(ui, (Retangulo){r.x + 250, r.y + 120, 90, 28}, "CANCELAR", 0))
        a->modal = MODAL_NENHUM;
    if (ui_botao(ui, (Retangulo){r.x + 350, r.y + 120, 96, 28}, "CRIAR", 0)) {
        novo_projeto(a);
        a->modal = MODAL_NENHUM;
    }
}

static void modal_banco(Aplicativo *a, Interface *ui) {
    Retangulo r = iniciar_modal(ui, "BANCO DE DADOS JRPG", 1040, 620);
    Editor *e = &a->editor;
    Projeto *p = &a->projeto;
    const char *abas[] = {"HEROI",    "CLASSES", "HABILIDADES", "ITENS",
                          "INIMIGOS", "ESTADOS", "LOJAS",       "MISSOES"};
    for (int i = 0; i < 8; ++i) {
        int bx = r.x + 18 + (i % 4) * 126, by = r.y + 40 + (i / 4) * 31;
        if (ui_botao(ui, (Retangulo){bx, by, 118, 26}, abas[i], e->aba_banco == i)) {
            e->aba_banco = i;
            e->registro_banco = 0;
        }
    }
    int y = r.y + 112, total = p->quantidade_herois;
    const char *nome = "Heroi";
    if (e->aba_banco == 1)
        total = p->quantidade_classes;
    else if (e->aba_banco == 2)
        total = p->quantidade_habilidades;
    else if (e->aba_banco == 3)
        total = p->quantidade_itens;
    else if (e->aba_banco == 4)
        total = p->quantidade_inimigos;
    else if (e->aba_banco == 5)
        total = p->quantidade_estados;
    else if (e->aba_banco == 6)
        total = p->quantidade_lojas;
    else if (e->aba_banco == 7)
        total = p->quantidade_missoes;
    for (int i = 0; i < total && i < 15; ++i) {
        if (e->aba_banco == 0)
            nome = p->herois[i].nome;
        else if (e->aba_banco == 1)
            nome = p->classes[i].nome;
        else if (e->aba_banco == 2)
            nome = p->habilidades[i].nome;
        else if (e->aba_banco == 3)
            nome = p->itens[i].nome;
        else if (e->aba_banco == 4)
            nome = p->inimigos[i].nome;
        else if (e->aba_banco == 5)
            nome = p->estados[i].nome;
        else if (e->aba_banco == 6)
            nome = p->lojas[i].nome;
        else
            nome = p->missoes[i].nome;
        if (ui_botao(ui, (Retangulo){r.x + 18, y + i * 29, 230, 25}, nome, e->registro_banco == i))
            e->registro_banco = i;
    }
    if (ui_botao(ui, (Retangulo){r.x + 18, r.y + r.altura - 74, 120, 26}, "+ REGISTRO", 0)) {
        if (e->aba_banco == 0 && p->quantidade_herois < 16) {
            int i = p->quantidade_herois++;
            memset(&p->herois[i], 0, sizeof(Heroi));
            snprintf(p->herois[i].nome, 48, "Heroi %d", i + 1);
            p->herois[i].vida_maxima = 20;
            p->herois[i].velocidade = 5;
            e->registro_banco = i;
        } else if (e->aba_banco == 1 && p->quantidade_classes < 32) {
            int i = p->quantidade_classes++;
            memset(&p->classes[i], 0, sizeof(Classe));
            snprintf(p->classes[i].nome, 48, "Classe %d", i + 1);
            e->registro_banco = i;
        } else if (e->aba_banco == 2 && p->quantidade_habilidades < 128) {
            int i = p->quantidade_habilidades++;
            memset(&p->habilidades[i], 0, sizeof(Habilidade));
            snprintf(p->habilidades[i].nome, 48, "Habilidade %d", i + 1);
            e->registro_banco = i;
        } else if (e->aba_banco == 3 && p->quantidade_itens < MYTHARA_MAX_ITENS &&
                   reservar_memoria((void **)&p->itens, &p->capacidade_itens,
                                    p->quantidade_itens + 1, sizeof(Item), MYTHARA_MAX_ITENS)) {
            int i = p->quantidade_itens++;
            memset(&p->itens[i], 0, sizeof(Item));
            snprintf(p->itens[i].nome, 48, "Item %d", i + 1);
            e->registro_banco = i;
        } else if (e->aba_banco == 4 && p->quantidade_inimigos < MYTHARA_MAX_INIMIGOS &&
                   reservar_memoria((void **)&p->inimigos, &p->capacidade_inimigos,
                                    p->quantidade_inimigos + 1, sizeof(Inimigo),
                                    MYTHARA_MAX_INIMIGOS)) {
            int i = p->quantidade_inimigos++;
            memset(&p->inimigos[i], 0, sizeof(Inimigo));
            snprintf(p->inimigos[i].nome, 48, "Inimigo %d", i + 1);
            p->inimigos[i].vida = 10;
            e->registro_banco = i;
        } else if (e->aba_banco == 5 && p->quantidade_estados < 64) {
            int i = p->quantidade_estados++;
            memset(&p->estados[i], 0, sizeof(EstadoCombate));
            snprintf(p->estados[i].nome, 48, "Estado %d", i + 1);
            e->registro_banco = i;
        } else if (e->aba_banco == 6 && p->quantidade_lojas < 64) {
            int i = p->quantidade_lojas++;
            memset(&p->lojas[i], 0, sizeof(Loja));
            snprintf(p->lojas[i].nome, 48, "Loja %d", i + 1);
            e->registro_banco = i;
        } else if (e->aba_banco == 7 && p->quantidade_missoes < 128) {
            int i = p->quantidade_missoes++;
            memset(&p->missoes[i], 0, sizeof(Missao));
            snprintf(p->missoes[i].nome, 48, "Missao %d", i + 1);
            e->registro_banco = i;
        }
        atribuir_ids_ausentes(p);
        if (e->aba_banco == 0)
            total = p->quantidade_herois;
        else if (e->aba_banco == 1)
            total = p->quantidade_classes;
        else if (e->aba_banco == 2)
            total = p->quantidade_habilidades;
        else if (e->aba_banco == 3)
            total = p->quantidade_itens;
        else if (e->aba_banco == 4)
            total = p->quantidade_inimigos;
        else if (e->aba_banco == 5)
            total = p->quantidade_estados;
        else if (e->aba_banco == 6)
            total = p->quantidade_lojas;
        else if (e->aba_banco == 7)
            total = p->quantidade_missoes;
    }
    int i = limitar_int(e->registro_banco, 0, total ? total - 1 : 0), x = r.x + 280;
    y = r.y + 112;
    if (e->aba_banco == 0) {
        Heroi *h = &p->herois[i];
        ui_campo(ui, (Retangulo){x, y, 400, 28}, h->nome, sizeof(h->nome));
        y += 37;
        h->classe = ui_numero(ui, (Retangulo){x, y, 400, 27}, "CLASSE", h->classe, 0,
                              p->quantidade_classes ? p->quantidade_classes - 1 : 0);
        y += 34;
        h->vida_maxima = ui_numero(ui, (Retangulo){x, y, 400, 27}, "VIDA", h->vida_maxima, 1, 9999);
        y += 34;
        h->magia_maxima =
            ui_numero(ui, (Retangulo){x, y, 400, 27}, "MAGIA", h->magia_maxima, 0, 9999);
        y += 34;
        h->ataque = ui_numero(ui, (Retangulo){x, y, 400, 27}, "ATAQUE", h->ataque, 1, 999);
        y += 34;
        h->defesa = ui_numero(ui, (Retangulo){x, y, 400, 27}, "DEFESA", h->defesa, 0, 999);
        y += 34;
        h->poder_magico =
            ui_numero(ui, (Retangulo){x, y, 400, 27}, "PODER MAGICO", h->poder_magico, 0, 999);
        y += 34;
        h->resistencia =
            ui_numero(ui, (Retangulo){x, y, 400, 27}, "RESISTENCIA", h->resistencia, 0, 999);
        y += 34;
        h->velocidade =
            ui_numero(ui, (Retangulo){x, y, 400, 27}, "VELOCIDADE", h->velocidade, 1, 999);
    } else if (total > 0 && e->aba_banco == 1) {
        Classe *c = &p->classes[i];
        ui_campo(ui, (Retangulo){x, y, 400, 28}, c->nome, sizeof(c->nome));
        y += 38;
        c->vida_base =
            ui_numero(ui, (Retangulo){x, y, 400, 27}, "VIDA BASE", c->vida_base, 1, 9999);
        y += 34;
        c->magia_base =
            ui_numero(ui, (Retangulo){x, y, 400, 27}, "MAGIA BASE", c->magia_base, 0, 9999);
        y += 34;
        c->ataque_base =
            ui_numero(ui, (Retangulo){x, y, 400, 27}, "ATAQUE BASE", c->ataque_base, 1, 999);
        y += 34;
        c->defesa_base =
            ui_numero(ui, (Retangulo){x, y, 400, 27}, "DEFESA BASE", c->defesa_base, 0, 999);
        y += 34;
        c->velocidade_base = ui_numero(ui, (Retangulo){x, y, 400, 27}, "VELOCIDADE BASE",
                                       c->velocidade_base, 1, 999);
    } else if (total > 0 && e->aba_banco == 2) {
        Habilidade *h = &p->habilidades[i];
        ui_campo(ui, (Retangulo){x, y, 400, 28}, h->nome, sizeof(h->nome));
        y += 38;
        ui_campo(ui, (Retangulo){x, y, 620, 28}, h->descricao, sizeof(h->descricao));
        y += 38;
        h->custo_mp = ui_numero(ui, (Retangulo){x, y, 400, 27}, "CUSTO MP", h->custo_mp, 0, 999);
        y += 34;
        h->poder = ui_numero(ui, (Retangulo){x, y, 400, 27}, "PODER", h->poder, -999, 9999);
        y += 34;
        h->elemento = ui_numero(ui, (Retangulo){x, y, 400, 27}, "ELEMENTO", h->elemento, 0, 15);
        y += 34;
        h->alvo = ui_numero(ui, (Retangulo){x, y, 400, 27}, "ALVO", h->alvo, 0, 4);
    } else if (total > 0 && e->aba_banco == 3) {
        Item *it = &p->itens[i];
        ui_campo(ui, (Retangulo){x, y, 400, 28}, it->nome, sizeof(it->nome));
        y += 38;
        ui_campo(ui, (Retangulo){x, y, 620, 28}, it->descricao, sizeof(it->descricao));
        y += 38;
        it->valor = ui_numero(ui, (Retangulo){x, y, 400, 27}, "VALOR", it->valor, 0, 99999);
        y += 34;
        it->cura = ui_numero(ui, (Retangulo){x, y, 400, 27}, "CURA", it->cura, 0, 9999);
        y += 34;
        it->tipo_equipamento = ui_numero(ui, (Retangulo){x, y, 400, 27}, "SLOT EQUIPAMENTO",
                                         it->tipo_equipamento, 0, 4);
        y += 34;
        it->ataque =
            ui_numero(ui, (Retangulo){x, y, 400, 27}, "BONUS ATAQUE", it->ataque, -999, 999);
        y += 34;
        it->defesa =
            ui_numero(ui, (Retangulo){x, y, 400, 27}, "BONUS DEFESA", it->defesa, -999, 999);
    } else if (total > 0 && e->aba_banco == 4) {
        Inimigo *n = &p->inimigos[i];
        ui_campo(ui, (Retangulo){x, y, 400, 28}, n->nome, sizeof(n->nome));
        y += 38;
        n->vida = ui_numero(ui, (Retangulo){x, y, 400, 27}, "VIDA", n->vida, 1, 99999);
        y += 34;
        n->magia = ui_numero(ui, (Retangulo){x, y, 400, 27}, "MAGIA", n->magia, 0, 9999);
        y += 34;
        n->ataque = ui_numero(ui, (Retangulo){x, y, 400, 27}, "ATAQUE", n->ataque, 1, 9999);
        y += 34;
        n->defesa = ui_numero(ui, (Retangulo){x, y, 400, 27}, "DEFESA", n->defesa, 0, 9999);
        y += 34;
        n->velocidade =
            ui_numero(ui, (Retangulo){x, y, 400, 27}, "VELOCIDADE", n->velocidade, 1, 999);
        y += 34;
        n->experiencia =
            ui_numero(ui, (Retangulo){x, y, 400, 27}, "EXPERIENCIA", n->experiencia, 0, 99999);
        y += 34;
        n->ouro = ui_numero(ui, (Retangulo){x, y, 400, 27}, "OURO", n->ouro, 0, 99999);
    } else if (total > 0 && e->aba_banco == 5) {
        EstadoCombate *s = &p->estados[i];
        ui_campo(ui, (Retangulo){x, y, 400, 28}, s->nome, sizeof(s->nome));
        y += 38;
        s->duracao = ui_numero(ui, (Retangulo){x, y, 400, 27}, "DURACAO TURNOS", s->duracao, 1, 99);
        y += 34;
        s->dano_turno =
            ui_numero(ui, (Retangulo){x, y, 400, 27}, "DANO POR TURNO", s->dano_turno, -999, 999);
        y += 34;
        s->ataque_pct =
            ui_numero(ui, (Retangulo){x, y, 400, 27}, "ATAQUE %", s->ataque_pct, -100, 500);
        y += 34;
        s->defesa_pct =
            ui_numero(ui, (Retangulo){x, y, 400, 27}, "DEFESA %", s->defesa_pct, -100, 500);
    } else if (total > 0 && e->aba_banco == 6) {
        Loja *l = &p->lojas[i];
        ui_campo(ui, (Retangulo){x, y, 400, 28}, l->nome, sizeof(l->nome));
        y += 38;
        l->quantidade_itens = ui_numero(ui, (Retangulo){x, y, 400, 27}, "QUANTIDADE ITENS",
                                        l->quantidade_itens, 0, 32);
        y += 34;
        if (l->quantidade_itens) {
            l->itens[0] = ui_numero(ui, (Retangulo){x, y, 400, 27}, "ITEM PRINCIPAL", l->itens[0],
                                    0, p->quantidade_itens ? p->quantidade_itens - 1 : 0);
            y += 34;
            l->precos[0] =
                ui_numero(ui, (Retangulo){x, y, 400, 27}, "PRECO", l->precos[0], 0, 99999);
        }
    } else if (total > 0 && e->aba_banco == 7) {
        Missao *m = &p->missoes[i];
        ui_campo(ui, (Retangulo){x, y, 400, 28}, m->nome, sizeof(m->nome));
        y += 38;
        ui_campo(ui, (Retangulo){x, y, 620, 28}, m->descricao, sizeof(m->descricao));
        y += 38;
        m->quantidade_etapas =
            ui_numero(ui, (Retangulo){x, y, 400, 27}, "ETAPAS", m->quantidade_etapas, 0, 16);
        y += 34;
        if (m->quantidade_etapas) {
            ui_campo(ui, (Retangulo){x, y, 620, 28}, m->etapas[0].descricao,
                     sizeof(m->etapas[0].descricao));
            y += 38;
            m->etapas[0].tipo =
                ui_numero(ui, (Retangulo){x, y, 400, 27}, "TIPO OBJETIVO", m->etapas[0].tipo, 0, 4);
            y += 34;
            m->etapas[0].alvo =
                ui_numero(ui, (Retangulo){x, y, 400, 27}, "ALVO", m->etapas[0].alvo, 0, 999);
            y += 34;
            m->etapas[0].quantidade = ui_numero(ui, (Retangulo){x, y, 400, 27}, "QUANTIDADE",
                                                m->etapas[0].quantidade, 1, 999);
        }
        m->recompensa_ouro = ui_numero(ui, (Retangulo){x, y + 40, 400, 27}, "RECOMPENSA OURO",
                                       m->recompensa_ouro, 0, 99999);
    }
    if (ui_botao(ui, (Retangulo){r.x + r.largura - 112, r.y + r.altura - 48, 90, 28}, "CONCLUIR",
                 0)) {
        e->alterado = 1;
        a->modal = MODAL_NENHUM;
    }
}

static void modal_eventos(Aplicativo *a, Interface *ui) {
    Retangulo r = iniciar_modal(ui, "EDITOR VISUAL DE EVENTOS", 1040, 610);
    Editor *ed = &a->editor;
    int y = r.y + 42;
    if (r.largura < 900) {
        if (a->projeto.quantidade_eventos <= 0)
            adicionar_evento(a, "Novo evento");
        int ev = limitar_int(ed->evento_selecionado, 0, a->projeto.quantidade_eventos - 1);
        if (ui_botao(ui, (Retangulo){r.x + 16, r.y + 40, 68, 25}, "< EVENTO", 0)) {
            ev = limitar_int(ev - 1, 0, a->projeto.quantidade_eventos - 1);
            ed->evento_selecionado = ev;
            ed->comando_selecionado = 0;
        }
        if (ui_botao(ui, (Retangulo){r.x + 90, r.y + 40, 68, 25}, "EVENTO >", 0)) {
            ev = limitar_int(ev + 1, 0, a->projeto.quantidade_eventos - 1);
            ed->evento_selecionado = ev;
            ed->comando_selecionado = 0;
        }
        if (ui_botao(ui, (Retangulo){r.x + 164, r.y + 40, 86, 25}, "+ EVENTO", 0)) {
            int novo = adicionar_evento(a, "Novo evento");
            if (novo >= 0) {
                ev = novo;
                ed->evento_selecionado = novo;
                ed->comando_selecionado = 0;
            }
        }
        Evento *evento = &a->projeto.eventos[ev];
        int painel_x = r.x + 258, painel_largura = r.largura - 274;
        ui_campo(ui, (Retangulo){painel_x, r.y + 40, painel_largura, 25}, evento->nome,
                 sizeof(evento->nome));
        evento->condicao_flag =
            ui_numero(ui, (Retangulo){painel_x, r.y + 70, painel_largura / 2 - 3, 24}, "FLAG",
                      evento->condicao_flag, -1, MYTHARA_MAX_FLAGS - 1);
        evento->condicao_valor = ui_numero(
            ui,
            (Retangulo){painel_x + painel_largura / 2 + 3, r.y + 70, painel_largura / 2 - 3, 24},
            "VALOR", evento->condicao_valor, -999, 999);
        int inicio_y = r.y + 104, visiveis = limitar_int((r.altura - 154) / 25, 4, 12);
        for (int i = 0; i < evento->quantidade_comandos && i < visiveis; ++i) {
            char linha[96];
            snprintf(linha, sizeof(linha), "%02d %s", i + 1,
                     nomes_comandos[limitar_int(evento->comandos[i].tipo, 0, 16)]);
            if (ui_botao(ui, (Retangulo){r.x + 16, inicio_y + i * 25, 234, 22}, linha,
                         ed->comando_selecionado == i))
                ed->comando_selecionado = i;
        }
        if (ui_botao(ui, (Retangulo){r.x + 16, r.y + r.altura - 44, 112, 24}, "+ COMANDO", 0) &&
            evento->quantidade_comandos < MYTHARA_MAX_COMANDOS &&
            reservar_comandos(evento, evento->quantidade_comandos + 1)) {
            int i = evento->quantidade_comandos++;
            memset(&evento->comandos[i], 0, sizeof(ComandoEvento));
            evento->comandos[i].tipo = COMANDO_TEXTO;
            copiar_texto(evento->comandos[i].texto, sizeof(evento->comandos[i].texto),
                         "Novo dialogo");
            ed->comando_selecionado = i;
        }
        if (evento->quantidade_comandos > 0) {
            int ci = limitar_int(ed->comando_selecionado, 0, evento->quantidade_comandos - 1);
            ComandoEvento *c = &evento->comandos[ci];
            int cy = r.y + 104;
            c->tipo = ui_numero(ui, (Retangulo){painel_x, cy, painel_largura, 24}, "TIPO", c->tipo,
                                0, 16);
            cy += 28;
            ui_campo_multilinha(ui, (Retangulo){painel_x, cy, painel_largura, 46}, c->texto,
                                sizeof(c->texto));
            cy += 50;
            c->a = ui_numero(ui, (Retangulo){painel_x, cy, painel_largura, 24}, "PARAMETRO A", c->a,
                             -1, 999);
            cy += 28;
            c->b = ui_numero(ui, (Retangulo){painel_x, cy, painel_largura, 24}, "PARAMETRO B", c->b,
                             -999, 999);
            cy += 28;
            c->c = ui_numero(ui, (Retangulo){painel_x, cy, painel_largura, 24}, "PARAMETRO C", c->c,
                             -999, 999);
            cy += 28;
            c->profundidade = ui_numero(ui, (Retangulo){painel_x, cy, painel_largura, 24}, "NIVEL",
                                        c->profundidade, 0, 8);
            if (ui_botao(ui, (Retangulo){painel_x, r.y + r.altura - 44, 130, 24}, "EXCLUIR", 0)) {
                memmove(c, c + 1, (size_t)(evento->quantidade_comandos - ci - 1) * sizeof(*c));
                evento->quantidade_comandos--;
                ed->comando_selecionado = limitar_int(
                    ci - 1, 0, evento->quantidade_comandos ? evento->quantidade_comandos - 1 : 0);
            }
        }
        if (ui_botao(ui, (Retangulo){r.x + r.largura - 108, r.y + r.altura - 44, 88, 24},
                     "CONCLUIR", 0)) {
            ed->alterado = 1;
            a->modal = MODAL_NENHUM;
        }
        return;
    }
    for (int i = 0; i < a->projeto.quantidade_eventos && i < 18; ++i)
        if (a->projeto.eventos[i].ativo) {
            if (ui_botao(ui, (Retangulo){r.x + 16, y, 200, 25}, a->projeto.eventos[i].nome,
                         ed->evento_selecionado == i)) {
                ed->evento_selecionado = i;
                ed->comando_selecionado = 0;
            }
            y += 27;
        }
    if (ui_botao(ui, (Retangulo){r.x + 16, r.y + r.altura - 76, 94, 25}, "+ EVENTO", 0)) {
        int i = adicionar_evento(a, "Novo evento");
        if (i >= 0)
            ed->evento_selecionado = i;
    }
    if (a->projeto.quantidade_eventos <= 0) {
        desenhar_texto(ui->tela, r.x + 236, r.y + 48, "NENHUM EVENTO. USE + EVENTO PARA CRIAR.",
                       tema_ativo.texto_suave, 1);
        if (ui_botao(ui, (Retangulo){r.x + r.largura - 112, r.y + r.altura - 44, 90, 27},
                     "CONCLUIR", 0))
            a->modal = MODAL_NENHUM;
        return;
    }
    int ev = limitar_int(ed->evento_selecionado, 0, a->projeto.quantidade_eventos - 1);
    Evento *e = &a->projeto.eventos[ev];
    if (e->ativo) {
        ui_campo(ui, (Retangulo){r.x + 236, r.y + 42, 360, 28}, e->nome, sizeof(e->nome));
        e->condicao_flag = ui_numero(ui, (Retangulo){r.x + 610, r.y + 42, 190, 28}, "FLAG",
                                     e->condicao_flag, -1, MYTHARA_MAX_FLAGS - 1);
        e->condicao_valor = ui_numero(ui, (Retangulo){r.x + 808, r.y + 42, 190, 28}, "VALOR",
                                      e->condicao_valor, -999, 999);
        y = r.y + 86;
        for (int i = 0; i < e->quantidade_comandos && i < 17; ++i) {
            char linha[128], recuo[18];
            int profundidade = limitar_int(e->comandos[i].profundidade, 0, 8);
            memset(recuo, ' ', (size_t)profundidade * 2);
            recuo[profundidade * 2] = 0;
            snprintf(linha, sizeof(linha), "%02d %s%s", i + 1, recuo,
                     nomes_comandos[limitar_int(e->comandos[i].tipo, 0, 16)]);
            if (ui_botao(ui, (Retangulo){r.x + 236, y, 370, 25}, linha,
                         ed->comando_selecionado == i))
                ed->comando_selecionado = i;
            y += 27;
        }
        if (ui_botao(ui, (Retangulo){r.x + 236, r.y + r.altura - 76, 116, 25}, "+ COMANDO", 0) &&
            e->quantidade_comandos < MYTHARA_MAX_COMANDOS &&
            reservar_comandos(e, e->quantidade_comandos + 1)) {
            int i = e->quantidade_comandos++;
            memset(&e->comandos[i], 0, sizeof(ComandoEvento));
            e->comandos[i].tipo = COMANDO_TEXTO;
            copiar_texto(e->comandos[i].texto, sizeof(e->comandos[i].texto), "Novo dialogo");
            ed->comando_selecionado = i;
        }
        if (e->quantidade_comandos > 0) {
            int ci = limitar_int(ed->comando_selecionado, 0, e->quantidade_comandos - 1);
            ComandoEvento *c = &e->comandos[ci];
            int x = r.x + 630;
            y = r.y + 100;
            c->tipo = ui_numero(ui, (Retangulo){x, y, 370, 28}, "TIPO", c->tipo, 0, 16);
            y += 36;
            desenhar_texto(ui->tela, x, y, nomes_comandos[c->tipo], tema_ativo.destaque, 1);
            y += 24;
            desenhar_texto(ui->tela, x, y, "TEXTO / OPCOES SEPARADAS POR |", tema_ativo.texto_suave,
                           1);
            y += 16;
            ui_campo_multilinha(ui, (Retangulo){x, y, 370, 58}, c->texto, sizeof(c->texto));
            y += 68;
            c->a = ui_numero(ui, (Retangulo){x, y, 370, 28}, "PARAMETRO A", c->a, -1, 999);
            y += 36;
            c->b = ui_numero(ui, (Retangulo){x, y, 370, 28}, "PARAMETRO B", c->b, -999, 999);
            y += 36;
            c->c = ui_numero(ui, (Retangulo){x, y, 370, 28}, "PARAMETRO C", c->c, -999, 999);
            y += 36;
            c->profundidade =
                ui_numero(ui, (Retangulo){x, y, 370, 28}, "NIVEL DO BLOCO", c->profundidade, 0, 8);
            y += 38;
            desenhar_texto(ui->tela, x, y, "A/B/C: INDICES OU VALORES DO COMANDO",
                           cor_rgb(145, 156, 171), 1);
            y += 30;
            if (ui_botao(ui, (Retangulo){x, y, 160, 27}, "EXCLUIR COMANDO", 0)) {
                memmove(c, c + 1, (size_t)(e->quantidade_comandos - ci - 1) * sizeof(*c));
                e->quantidade_comandos--;
                ed->comando_selecionado =
                    limitar_int(ci - 1, 0, e->quantidade_comandos ? e->quantidade_comandos - 1 : 0);
            }
        }
    }
    if (ui_botao(ui, (Retangulo){r.x + r.largura - 112, r.y + r.altura - 44, 90, 27}, "CONCLUIR",
                 0)) {
        ed->alterado = 1;
        a->modal = MODAL_NENHUM;
    }
}

static void modal_recursos(Aplicativo *a, Interface *ui) {
    Retangulo r = iniciar_modal(ui, "RECURSOS E ANIMACOES", 900, 570);
    Editor *e = &a->editor;
    int y = r.y + 44, compacto = r.largura < 760;
    const char *extensoes[] = {"BMP", "WAV", "QOI", "TGA"};
    for (int i = 0; i < a->projeto.quantidade_recursos && i < 15; ++i)
        if (a->projeto.recursos[i].ativo) {
            char linha[96];
            snprintf(linha, sizeof(linha), "%s  %s",
                     extensoes[limitar_int(a->projeto.recursos[i].tipo, 0, 3)],
                     a->projeto.recursos[i].nome);
            if (ui_botao(ui, (Retangulo){r.x + 18, y, 270, 26}, linha, e->recurso_selecionado == i))
                e->recurso_selecionado = i;
            y += 29;
        }
    desenhar_texto(ui->tela, r.x + 320, r.y + 45, "CAMINHO PARA IMPORTAR", tema_ativo.texto_suave,
                   1);
    ui_campo(ui, (Retangulo){r.x + 320, r.y + 64, compacto ? r.largura - 340 : 550, 30},
             a->campo_caminho, sizeof(a->campo_caminho));
    int tipos[] = {RECURSO_BMP, RECURSO_QOI, RECURSO_TGA, RECURSO_WAV};
    const char *rotulos[] = {"IMPORTAR BMP", "IMPORTAR QOI", "IMPORTAR TGA", "IMPORTAR WAV"};
    for (int botao = 0; botao < 4; ++botao)
        if (ui_botao(
                ui,
                (Retangulo){r.x + 320 + (botao % 2) * 145, r.y + 103 + (botao / 2) * 33, 136, 28},
                rotulos[botao], 0) &&
            a->projeto.quantidade_recursos < MYTHARA_MAX_RECURSOS &&
            reservar_memoria((void **)&a->projeto.recursos, &a->projeto.capacidade_recursos,
                             a->projeto.quantidade_recursos + 1, sizeof(Recurso),
                             MYTHARA_MAX_RECURSOS)) {
            int tipo = tipos[botao], valido = 1;
            Imagem teste = {0};
            char erro[128];
            if (tipo != RECURSO_WAV) {
                valido = carregar_imagem(tipo, a->campo_caminho, &teste, erro, sizeof(erro));
                liberar_imagem(&teste);
            } else {
                FILE *f = fopen(a->campo_caminho, "rb");
                char h[12];
                valido = f && fread(h, 12, 1, f) == 1 && !memcmp(h, "RIFF", 4) &&
                         !memcmp(h + 8, "WAVE", 4);
                if (f)
                    fclose(f);
                if (!valido)
                    copiar_texto(erro, sizeof(erro), "WAV PCM invalido.");
            }
            if (valido) {
                char relativo[MYTHARA_MAX_CAMINHO];
                if (!importar_arquivo_projeto(&a->projeto, a->campo_caminho, tipo == RECURSO_WAV,
                                              relativo, sizeof(relativo))) {
                    mensagem_aplicativo(a, "Falha ao copiar o recurso para a pasta do projeto.");
                    continue;
                }
                int i = a->projeto.quantidade_recursos++;
                Recurso *rec = &a->projeto.recursos[i];
                memset(rec, 0, sizeof(*rec));
                rec->id = novo_identificador(&a->projeto);
                rec->ativo = 1;
                rec->tipo = tipo;
                copiar_texto(rec->nome, sizeof(rec->nome), nome_base(relativo));
                copiar_texto(rec->caminho, sizeof(rec->caminho), relativo);
                rec->largura_quadro = rec->altura_quadro = a->projeto.tamanho_tile;
                rec->quantidade_quadros = 1;
                rec->duracao_quadro_ms = 160;
                rec->quantidade_clipes = 1;
                copiar_texto(rec->clipes[0].nome, sizeof(rec->clipes[0].nome), "Parado");
                rec->clipes[0].quantidade_quadros = 1;
                rec->clipes[0].duracao_ms = 160;
                rec->clipes[0].repetir = 1;
                e->recurso_selecionado = i;
                e->alterado = 1;
                recarregar_recursos(a);
                mensagem_aplicativo(a, "Recurso copiado para o projeto.");
            } else
                mensagem_aplicativo(a, erro);
        }
    if (e->recurso_selecionado >= 0 && e->recurso_selecionado < a->projeto.quantidade_recursos) {
        Recurso *rec = &a->projeto.recursos[e->recurso_selecionado];
        int x = r.x + (compacto ? 320 : 620), y2 = r.y + 190;
        if (rec->tipo != RECURSO_WAV && a->imagens[e->recurso_selecionado].pixels) {
            Imagem *img = &a->imagens[e->recurso_selecionado];
            int quadro = rec->quantidade_quadros > 0
                             ? (int)(agora_segundos() * 1000.0 /
                                     limitar_int(rec->duracao_quadro_ms, 30, 5000)) %
                                   rec->quantidade_quadros
                             : 0;
            int colunas = img->largura / limitar_int(rec->largura_quadro, 1, img->largura);
            if (colunas < 1)
                colunas = 1;
            Retangulo origem = {(quadro % colunas) * rec->largura_quadro,
                                (quadro / colunas) * rec->altura_quadro, rec->largura_quadro,
                                rec->altura_quadro};
            if (!compacto) {
                desenhar_imagem(ui->tela, img, (Retangulo){r.x + 320, r.y + 190, 260, 220}, origem);
                contornar_retangulo(ui->tela, (Retangulo){r.x + 320, r.y + 190, 260, 220},
                                    tema_ativo.borda);
            }
            int largura_controle = compacto ? r.largura - 340 : 240;
            rec->largura_quadro = ui_numero(ui, (Retangulo){x, y2, largura_controle, 27},
                                            "LARGURA QUADRO", rec->largura_quadro, 1, img->largura);
            y2 += 34;
            rec->altura_quadro = ui_numero(ui, (Retangulo){x, y2, largura_controle, 27},
                                           "ALTURA QUADRO", rec->altura_quadro, 1, img->altura);
            y2 += 34;
            rec->quantidade_quadros = ui_numero(ui, (Retangulo){x, y2, largura_controle, 27},
                                                "QUADROS", rec->quantidade_quadros, 1, 256);
            y2 += 34;
            rec->duracao_quadro_ms = ui_numero(ui, (Retangulo){x, y2, largura_controle, 27},
                                               "DURACAO MS", rec->duracao_quadro_ms, 30, 5000);
            y2 += 38;
            ui_campo(ui, (Retangulo){x, y2, largura_controle, 27}, rec->clipes[0].nome,
                     sizeof(rec->clipes[0].nome));
        } else if (rec->tipo == RECURSO_WAV &&
                   ui_botao(ui, (Retangulo){r.x + 320, r.y + 195, 130, 28}, "OUVIR AUDIO", 0)) {
            char caminho[MYTHARA_MAX_CAMINHO];
            caminho_do_recurso(&a->projeto, rec->caminho, caminho, sizeof(caminho));
            tocar_wav(caminho);
        }
    }
    if (ui_botao(ui, (Retangulo){r.x + r.largura - 112, r.y + r.altura - 44, 90, 27}, "CONCLUIR",
                 0))
        a->modal = MODAL_NENHUM;
}

static void modal_ajuda(Aplicativo *a, Interface *ui) {
    Retangulo r = iniciar_modal(ui, "MYTHARA - AJUDA", 720, 470);
    int x = r.x + 25, y = r.y + 50;
    const char *linhas[] = {"FLUXO RAPIDO",
                            "1. ESCOLHA UM TILE E PINTE O MAPA.",
                            "2. MARQUE PAREDES COM A FERRAMENTA COLISAO.",
                            "3. CRIE ENTIDADES E EDITE SEUS EVENTOS.",
                            "4. PRESSIONE F5 PARA TESTAR.",
                            "5. SALVE E USE EXPORTAR PARA DISTRIBUIR.",
                            "",
                            "ATALHOS DO EDITOR",
                            "CTRL+P: PALETA DE COMANDOS   F5: PLAYTEST",
                            "SETAS OU WASD: MOVER   ESPACO/E: INTERAGIR",
                            "M: GRUPO/EQUIPAMENTO   J: MISSOES",
                            "F1/F2: SALVAR/CARREGAR   ESC: EDITOR",
                            "",
                            "IMAGENS: BMP, QOI E TGA   AUDIO: WAV PCM",
                            "PROJETOS: .MYR   SAVES: .MYS   TEMAS: .MYT"};
    for (size_t i = 0; i < sizeof(linhas) / sizeof(linhas[0]); ++i) {
        desenhar_texto(ui->tela, x, y, linhas[i],
                       i == 0 || i == 7 ? cor_rgb(135, 198, 235) : cor_rgb(211, 220, 231), 1);
        y += 24;
    }
    if (ui_botao(ui, (Retangulo){r.x + r.largura - 112, r.y + r.altura - 44, 90, 27}, "FECHAR", 0))
        a->modal = MODAL_NENHUM;
}

static void editor_painel_inferior(Aplicativo *a, Interface *ui, Retangulo r) {
    Editor *e = &a->editor;
    Mapa *m = &a->projeto.mapas[e->mapa_atual];
    ui_painel(ui->tela, r, "TILES, AUTOTILE, CARIMBOS E MINIMAPA");
    int y = r.y + 35;
    for (int i = 0; i < 32; ++i) {
        Retangulo q = {r.x + 10 + (i % 16) * 30, y + (i / 16) * 30, 26, 26};
        desenhar_tile_recurso(a, ui->tela, q, i, 0);
        if (e->tile == i)
            contornar_retangulo(ui->tela, (Retangulo){q.x - 2, q.y - 2, 30, 30}, tema_ativo.aviso);
        if (ponto_em_retangulo(ui->entrada->mouse_x, ui->entrada->mouse_y, q) &&
            ui->entrada->mouse_pressionado)
            e->tile = i;
    }
    e->autotile =
        ui_checkbox(ui, (Retangulo){r.x + 10, y + 68, 130, 20}, "AUTOTILE 4 LADOS", e->autotile);
    if (ui_botao(ui, (Retangulo){r.x + 160, y + 64, 146, 25}, "CRIAR CARIMBO", 0))
        copiar_selecao_para_carimbo(a);
    char info[128];
    snprintf(info, sizeof(info), "TILE %d  |  %s  |  %dx%d", e->tile, m->camadas[e->camada].nome,
             m->largura, m->altura);
    desenhar_texto(ui->tela, r.x + 325, y + 72, info, tema_ativo.texto_suave, 1);
    int mw = 170, mh = r.altura - 38, mx = r.x + r.largura - mw - 12, my = r.y + 31;
    preencher_retangulo(ui->tela, (Retangulo){mx, my, mw, mh}, tema_ativo.fundo);
    int sx = mw / m->largura, sy = mh / m->altura, escala = sx < sy ? sx : sy;
    if (escala < 1)
        escala = 1;
    for (int yy = 0; yy < m->altura && yy * escala < mh; ++yy)
        for (int xx = 0; xx < m->largura && xx * escala < mw; ++xx) {
            int tile = m->camadas[0].tiles[yy * m->largura + xx];
            preencher_retangulo(ui->tela,
                                (Retangulo){mx + xx * escala, my + yy * escala, escala, escala},
                                paleta_tiles[tile & 15]);
        }
    contornar_retangulo(ui->tela, (Retangulo){mx, my, mw, mh}, tema_ativo.borda);
}

static Cor *cor_tema_por_indice(int indice) {
    Cor *cores[] = {&tema_ativo.fundo,    &tema_ativo.painel,         &tema_ativo.painel_elevado,
                    &tema_ativo.controle, &tema_ativo.controle_sobre, &tema_ativo.selecao,
                    &tema_ativo.borda,    &tema_ativo.texto,          &tema_ativo.texto_suave,
                    &tema_ativo.destaque, &tema_ativo.perigo,         &tema_ativo.aviso,
                    &tema_ativo.sucesso};
    return cores[limitar_int(indice, 0, 12)];
}

static void modal_tema(Aplicativo *a, Interface *ui) {
    Retangulo r = iniciar_modal(ui, "EDITOR COMPLETO DE TEMA", 900, 590);
    const char *nomes[] = {"FUNDO",   "PAINEL", "PAINEL ELEVADO", "CONTROLE",    "CONTROLE SOBRE",
                           "SELECAO", "BORDA",  "TEXTO",          "TEXTO SUAVE", "DESTAQUE",
                           "PERIGO",  "AVISO",  "SUCESSO"};
    int y = r.y + 44, compacto = r.largura < 760;
    for (int i = 0; i < 13; ++i) {
        Cor *cor = cor_tema_por_indice(i);
        int coluna = compacto ? i / 7 : 0, linha = compacto ? i % 7 : i,
            base_x = r.x + 20 + coluna * 135;
        Retangulo amostra = {base_x, y + linha * 32, 22, 24};
        preencher_retangulo(ui->tela, amostra, *cor);
        contornar_retangulo(ui->tela, amostra, tema_ativo.borda);
        if (ui_botao(ui, (Retangulo){base_x + 27, y + linha * 32, compacto ? 102 : 210, 24},
                     nomes[i], a->editor.cor_tema_selecionada == i))
            a->editor.cor_tema_selecionada = i;
    }
    Cor *cor = cor_tema_por_indice(a->editor.cor_tema_selecionada);
    int rr = (*cor >> 16) & 255, gg = (*cor >> 8) & 255, bb = *cor & 255;
    int x = r.x + 300;
    y = r.y + 58;
    desenhar_texto(ui->tela, x, y, "COR SELECIONADA", tema_ativo.destaque, 1);
    y += 24;
    rr = ui_numero(ui, (Retangulo){x, y, 300, 28}, "VERMELHO", rr, 0, 255);
    y += 38;
    gg = ui_numero(ui, (Retangulo){x, y, 300, 28}, "VERDE", gg, 0, 255);
    y += 38;
    bb = ui_numero(ui, (Retangulo){x, y, 300, 28}, "AZUL", bb, 0, 255);
    y += 42;
    *cor = cor_rgb((unsigned)rr, (unsigned)gg, (unsigned)bb);
    tema_ativo.escala_percentual = ui_numero(ui, (Retangulo){x, y, 300, 28}, "ESCALA %",
                                             tema_ativo.escala_percentual, 100, 200);
    y += 38;
    tema_ativo.espacamento =
        ui_numero(ui, (Retangulo){x, y, 300, 28}, "ESPACAMENTO", tema_ativo.espacamento, 2, 16);
    y += 38;
    tema_ativo.altura_controle =
        ui_numero(ui, (Retangulo){x, y, 300, 28}, "ALTURA", tema_ativo.altura_controle, 22, 44);
    y += 48;
    if (!compacto) {
        preencher_retangulo(ui->tela, (Retangulo){x, y, 520, 110}, tema_ativo.painel);
        desenhar_texto(ui->tela, x + 14, y + 14, "PRE-VISUALIZACAO AO VIVO", tema_ativo.texto, 1);
        ui_botao(ui, (Retangulo){x + 14, y + 42, 150, 30}, "BOTAO NORMAL", 0);
        ui_botao(ui, (Retangulo){x + 174, y + 42, 150, 30}, "SELECIONADO", 1);
        ui_campo(ui, (Retangulo){x + 14, y + 78, 310, 25}, tema_ativo.nome,
                 sizeof(tema_ativo.nome));
    }
    if (ui_botao(ui, (Retangulo){r.x + 20, r.y + r.altura - 45, 130, 27}, "RESTAURAR", 0))
        iniciar_tema_padrao();
    if (!a->campo_tema[0])
        copiar_texto(a->campo_tema, sizeof(a->campo_tema), "tema_mythara.myt");
    if (!compacto)
        ui_campo(ui, (Retangulo){r.x + 165, r.y + r.altura - 45, 285, 27}, a->campo_tema,
                 sizeof(a->campo_tema));
    if (!compacto &&
        ui_botao(ui, (Retangulo){r.x + 460, r.y + r.altura - 45, 105, 27}, "IMPORTAR", 0)) {
        mensagem_aplicativo(a, carregar_tema_arquivo(a->campo_tema)
                                   ? "Tema importado."
                                   : "Nao foi possivel importar o tema.");
    }
    if (!compacto &&
        ui_botao(ui, (Retangulo){r.x + 575, r.y + r.altura - 45, 105, 27}, "EXPORTAR", 0)) {
        mensagem_aplicativo(a, salvar_tema_arquivo(a->campo_tema)
                                   ? "Tema exportado."
                                   : "Nao foi possivel exportar o tema.");
    }
    if (ui_botao(ui, (Retangulo){r.x + r.largura - 112, r.y + r.altura - 45, 90, 27}, "CONCLUIR",
                 0))
        a->modal = MODAL_NENHUM;
}

static void modal_recuperacao(Aplicativo *a, Interface *ui) {
    Retangulo r = iniciar_modal(ui, "RECUPERACAO DE SESSAO", 620, 230);
    desenhar_texto(ui->tela, r.x + 24, r.y + 55,
                   "FOI ENCONTRADO UM AUTOSAVE MAIS NOVO QUE O PROJETO.", tema_ativo.aviso, 1);
    desenhar_texto(ui->tela, r.x + 24, r.y + 82,
                   "Recupere a sessao ou continue com o ultimo salvamento manual.",
                   tema_ativo.texto_suave, 1);
    desenhar_texto(ui->tela, r.x + 24, r.y + 110, a->caminho_autosave, tema_ativo.texto_suave, 1);
    if (ui_botao(ui, (Retangulo){r.x + r.largura - 240, r.y + r.altura - 55, 100, 30}, "IGNORAR",
                 0)) {
        a->oferecer_recuperacao = 0;
        a->modal = MODAL_NENHUM;
    }
    if (ui_botao(ui, (Retangulo){r.x + r.largura - 130, r.y + r.altura - 55, 105, 30}, "RECUPERAR",
                 0)) {
        if (recuperar_autosave(a))
            a->modal = MODAL_NENHUM;
    }
}

static void modal_boas_vindas(Aplicativo *a, Interface *ui) {
    int largura = limitar_int(ui->tela->largura - 40, 420, 760),
        altura = limitar_int(ui->tela->altura - 40, 360, 500);
    Retangulo r = iniciar_modal(ui, "BEM-VINDO A MYTHARA 3", largura, altura);
    int x = r.x + 28, y = r.y + 50;
    desenhar_texto(ui->tela, x, y, "CRIE SUA LENDA, UMA CENA DE CADA VEZ.", tema_ativo.destaque, 1);
    y += 28;
    desenhar_texto(ui->tela, x, y,
                   "O projeto de exemplo ja inclui herois, NPCs, batalha, loja e missao.",
                   tema_ativo.texto_suave, 1);
    y += 42;
    if (a->caminho_recente[0] && r.largura >= 570) {
        int rx = r.x + 270, rw = r.largura - 298;
        desenhar_texto(ui->tela, rx, y, "PROJETO RECENTE", tema_ativo.texto_suave, 1);
        char recente[48];
        copiar_texto(recente, sizeof(recente), nome_base(a->caminho_recente));
        desenhar_texto(ui->tela, rx, y + 22, recente, tema_ativo.texto, 1);
        if (ui_botao(ui, (Retangulo){rx, y + 48, rw, 34}, "ABRIR ULTIMO", 0) &&
            abrir_projeto_atual(a, a->caminho_recente)) {
            a->modal = MODAL_NENHUM;
            a->mostrar_boas_vindas = 0;
        }
    }
    if (ui_botao(ui, (Retangulo){x, y, 210, 34}, "CONTINUAR NO EXEMPLO", 0)) {
        a->modal = MODAL_NENHUM;
        a->mostrar_boas_vindas = 0;
    }
    y += 44;
    if (ui_botao(ui, (Retangulo){x, y, 210, 34}, "CRIAR PROJETO LIMPO", 0)) {
        novo_projeto(a);
        a->modal = MODAL_NENHUM;
        a->mostrar_boas_vindas = 0;
    }
    y += 44;
    if (ui_botao(ui, (Retangulo){x, y, 210, 34}, "ABRIR PROJETO V3", 0)) {
        a->acao_caminho = ACAO_ABRIR;
        copiar_texto(a->campo_caminho, sizeof(a->campo_caminho), ".");
        a->modal = MODAL_CAMINHO;
        a->mostrar_boas_vindas = 0;
    }
    y += 54;
    a->dicas_ativas =
        ui_checkbox(ui, (Retangulo){x, y, 310, 22}, "MOSTRAR DICAS CONTEXTUAIS", a->dicas_ativas);
    desenhar_texto(ui->tela, r.x + 28, r.y + r.altura - 42,
                   "ATALHOS: CTRL+P COMANDOS  |  F5 PLAYTEST  |  CTRL+S SALVAR",
                   tema_ativo.texto_suave, 1);
}

static void executar_acao_paleta(Aplicativo *a, int acao) {
    if (acao == 0)
        salvar_projeto_atual(a);
    else if (acao == 1) {
        a->modal = MODAL_EVENTOS;
        return;
    } else if (acao == 2) {
        a->modal = MODAL_BANCO;
        return;
    } else if (acao == 3) {
        a->modal = MODAL_RECURSOS;
        return;
    } else if (acao == 4) {
        a->modal = MODAL_TEMA;
        return;
    } else if (acao == 5)
        desfazer_edicao(a);
    else if (acao == 6)
        refazer_edicao(a);
    else if (acao == 7) {
        memset(&a->jogo, 0, sizeof(a->jogo));
        iniciar_estado_jogo(&a->projeto, &a->jogo.estado);
        a->jogo.evento_ativo = -1;
        a->modo = MODO_JOGO;
    }
    a->modal = MODAL_NENHUM;
}

static void modal_comandos(Aplicativo *a, Interface *ui) {
    Retangulo r = iniciar_modal(ui, "PALETA DE COMANDOS  CTRL+P", 620, 430);
    ui_campo(ui, (Retangulo){r.x + 20, r.y + 44, r.largura - 40, 30}, a->editor.busca_comandos,
             sizeof(a->editor.busca_comandos));
    const char *acoes[] = {"Salvar projeto",
                           "Abrir editor de eventos",
                           "Abrir banco de dados",
                           "Abrir recursos",
                           "Editar tema",
                           "Desfazer",
                           "Refazer",
                           "Iniciar playtest"};
    int y = r.y + 88;
    for (int i = 0; i < 8; ++i)
        if (texto_contem_sem_caixa(acoes[i], a->editor.busca_comandos)) {
            if (ui_botao(ui, (Retangulo){r.x + 20, y, r.largura - 40, 30}, acoes[i], 0))
                executar_acao_paleta(a, i);
            y += 34;
        }
}

static void desenhar_editor(Aplicativo *a, Interface *ui) {
    Editor *e = &a->editor;
    if (a->modal == MODAL_NENHUM && e->layout_ativo != LAYOUT_MAPA)
        e->layout_ativo = LAYOUT_MAPA;
    editor_barra_superior(a, ui);
    e->largura_arvore = limitar_int(e->largura_arvore, 190, 360);
    e->largura_inspetor = limitar_int(e->largura_inspetor, 250, 420);
    e->altura_inferior = limitar_int(e->altura_inferior, 95, 240);
    int base = ui->tela->altura - 28, compacto = ui->tela->largura < 900,
        centro_x = compacto ? 0 : e->largura_arvore + 4,
        centro_largura = compacto ? ui->tela->largura
                                  : ui->tela->largura - e->largura_arvore - e->largura_inspetor - 8;
    if (!compacto && centro_largura < 320) {
        compacto = 1;
        centro_x = 0;
        centro_largura = ui->tela->largura;
    }
    int divisor_direito = centro_x + centro_largura, divisor_baixo = base - e->altura_inferior;
    Retangulo div_esq = {e->largura_arvore, 70, 4, base - 70},
              div_dir = {divisor_direito, 70, 4, base - 70},
              div_baixo = {centro_x, divisor_baixo - 4, centro_largura, 4};
    Entrada *in = ui->entrada;
    if (in->mouse_pressionado) {
        if (!compacto && ponto_em_retangulo(in->mouse_x, in->mouse_y, div_esq))
            e->divisor_ativo = 1;
        else if (!compacto && ponto_em_retangulo(in->mouse_x, in->mouse_y, div_dir))
            e->divisor_ativo = 2;
        else if (ponto_em_retangulo(in->mouse_x, in->mouse_y, div_baixo))
            e->divisor_ativo = 3;
    }
    if (in->mouse_baixo) {
        if (e->divisor_ativo == 1)
            e->largura_arvore = in->mouse_x;
        if (e->divisor_ativo == 2)
            e->largura_inspetor = ui->tela->largura - in->mouse_x;
        if (e->divisor_ativo == 3)
            e->altura_inferior = base - in->mouse_y;
    }
    if (in->mouse_solto)
        e->divisor_ativo = 0;
    if (!compacto) {
        editor_painel_esquerdo(a, ui, (Retangulo){0, 70, e->largura_arvore, base - 70});
        editor_inspetor(a, ui,
                        (Retangulo){divisor_direito + 4, 70, e->largura_inspetor, base - 70});
    }
    preencher_retangulo(ui->tela, (Retangulo){centro_x, 70, centro_largura, 30},
                        tema_ativo.painel_elevado);
    int tx = centro_x + 5;
    if (compacto) {
        if (ui_botao(ui, (Retangulo){5, 73, 62, 24}, "ARVORE", e->gaveta_arvore))
            e->gaveta_arvore = !e->gaveta_arvore;
        if (ui_botao(ui, (Retangulo){71, 73, 68, 24}, "INSPETOR", e->gaveta_inspetor))
            e->gaveta_inspetor = !e->gaveta_inspetor;
        tx = 144;
    }
    for (int i = 0; i < a->projeto.quantidade_mapas && i < 8; ++i) {
        int w = limitar_int((int)strlen(a->projeto.mapas[i].nome) * 6 + 24, 70, 150);
        if (tx + w > divisor_direito)
            break;
        if (ui_botao(ui, (Retangulo){tx, 73, w, 24}, a->projeto.mapas[i].nome,
                     e->mapa_atual == i)) {
            e->mapa_atual = i;
            e->entidade_selecionada = -1;
            e->camada = 0;
        }
        tx += w + 3;
    }
    editor_canvas(a, ui, (Retangulo){centro_x, 100, centro_largura, divisor_baixo - 104});
    editor_painel_inferior(
        a, ui, (Retangulo){centro_x, divisor_baixo, centro_largura, e->altura_inferior});
    if (compacto) {
        if (e->gaveta_arvore)
            editor_painel_esquerdo(
                a, ui,
                (Retangulo){0, 100, limitar_int(e->largura_arvore, 190, ui->tela->largura - 40),
                            base - 100});
        if (e->gaveta_inspetor) {
            int w = limitar_int(e->largura_inspetor, 250, ui->tela->largura - 40);
            editor_inspetor(a, ui, (Retangulo){ui->tela->largura - w, 100, w, base - 100});
        }
    } else {
        preencher_retangulo(ui->tela, div_esq,
                            e->divisor_ativo == 1 ? tema_ativo.destaque : tema_ativo.borda);
        preencher_retangulo(ui->tela, div_dir,
                            e->divisor_ativo == 2 ? tema_ativo.destaque : tema_ativo.borda);
    }
    preencher_retangulo(ui->tela, div_baixo,
                        e->divisor_ativo == 3 ? tema_ativo.destaque : tema_ativo.borda);
    preencher_retangulo(ui->tela, (Retangulo){0, base, ui->tela->largura, 28},
                        tema_ativo.painel_elevado);
    char status[800];
    snprintf(
        status, sizeof(status), "%s%.360s   |   %.47s   |   %.47s   |   ZOOM %d%%   |   %.250s",
        e->alterado ? "* " : "", a->caminho_projeto, a->projeto.mapas[e->mapa_atual].nome,
        a->projeto.mapas[e->mapa_atual].camadas[e->camada].nome, e->zoom * 100 / 32, a->mensagem);
    desenhar_texto(ui->tela, 8, base + 10, status, tema_ativo.texto_suave, 1);
    if (a->modal == MODAL_NENHUM && ui->id_foco == 0) {
        Entrada *atalho = ui->entrada;
        if (atalho->controle && atalho->teclas_pressionadas['Z'])
            desfazer_edicao(a);
        if (atalho->controle && atalho->teclas_pressionadas['Y'])
            refazer_edicao(a);
        if (e->canvas_em_foco && atalho->controle && atalho->teclas_pressionadas['A']) {
            Mapa *m = &a->projeto.mapas[e->mapa_atual];
            e->selecao_x0 = e->selecao_y0 = 0;
            e->selecao_x1 = m->largura - 1;
            e->selecao_y1 = m->altura - 1;
            e->selecao_ativa = 1;
        }
        if (e->canvas_em_foco && atalho->controle && atalho->teclas_pressionadas['C'])
            copiar_area_transferencia(a, 0);
        if (e->canvas_em_foco && atalho->controle && atalho->teclas_pressionadas['X'])
            copiar_area_transferencia(a, 1);
        if (e->canvas_em_foco && atalho->controle && atalho->teclas_pressionadas['V'])
            colar_area_transferencia(a, e->cursor_tile_x, e->cursor_tile_y);
        if (e->canvas_em_foco && atalho->controle && atalho->teclas_pressionadas['D']) {
            copiar_area_transferencia(a, 0);
            colar_area_transferencia(a, e->selecao_x0 + 1, e->selecao_y0 + 1);
        }
        if (e->canvas_em_foco && atalho->teclas_pressionadas[127])
            apagar_selecao_editor(a);
    }
    if (a->modal == MODAL_CAMINHO)
        modal_caminho(a, ui);
    else if (a->modal == MODAL_NOVO)
        modal_novo(a, ui);
    else if (a->modal == MODAL_BANCO)
        modal_banco(a, ui);
    else if (a->modal == MODAL_EVENTOS)
        modal_eventos(a, ui);
    else if (a->modal == MODAL_RECURSOS)
        modal_recursos(a, ui);
    else if (a->modal == MODAL_AJUDA)
        modal_ajuda(a, ui);
    else if (a->modal == MODAL_TEMA)
        modal_tema(a, ui);
    else if (a->modal == MODAL_COMANDOS)
        modal_comandos(a, ui);
    else if (a->modal == MODAL_RECUPERACAO)
        modal_recuperacao(a, ui);
    else if (a->modal == MODAL_BOAS_VINDAS)
        modal_boas_vindas(a, ui);
    if (a->dicas_ativas && ui->rotulo_quente && !ui->entrada->mouse_baixo) {
        int w = limitar_int((int)strlen(ui->rotulo_quente) * 6 + 16, 60, 260),
            x = limitar_int(ui->entrada->mouse_x + 14, 0, ui->tela->largura - w),
            y = limitar_int(ui->entrada->mouse_y + 18, 0, ui->tela->altura - 24);
        preencher_retangulo(ui->tela, (Retangulo){x, y, w, 22}, tema_ativo.painel_elevado);
        contornar_retangulo(ui->tela, (Retangulo){x, y, w, 22}, tema_ativo.destaque);
        desenhar_texto(ui->tela, x + 8, y + 8, ui->rotulo_quente, tema_ativo.texto, 1);
    }
    if (ui->entrada->teclas_pressionadas[27])
        a->modal = MODAL_NENHUM;
}

/* ========================================================================== */
/* Runtime de jogo, eventos e combate                                          */
/* ========================================================================== */

static void iniciar_evento_jogo(Aplicativo *a, int indice) {
    if (indice < 0 || indice >= a->projeto.quantidade_eventos || !a->projeto.eventos[indice].ativo)
        return;
    Evento *e = &a->projeto.eventos[indice];
    if (e->condicao_flag >= 0 && e->condicao_flag < MYTHARA_MAX_FLAGS &&
        a->jogo.estado.flags[e->condicao_flag] != e->condicao_valor)
        return;
    a->jogo.evento_ativo = indice;
    a->jogo.proximo_comando = 0;
    a->jogo.aguardando_texto = 0;
    a->jogo.aguardando_escolha = 0;
}

static void separar_escolhas(const char *texto, char *a, size_t ca, char *b, size_t cb) {
    const char *barra = strchr(texto, '|');
    if (!barra) {
        copiar_texto(a, ca, texto[0] ? texto : "Sim");
        copiar_texto(b, cb, "Nao");
        return;
    }
    size_t n = (size_t)(barra - texto);
    if (n >= ca)
        n = ca - 1;
    memcpy(a, texto, n);
    a[n] = 0;
    copiar_texto(b, cb, barra + 1);
}

static void preparar_batalha(Aplicativo *a, int inimigo, int quantidade) {
    Jogo *j = &a->jogo;
    if (!a->projeto.quantidade_inimigos)
        return;
    j->quantidade_inimigos_batalha = limitar_int(quantidade ? quantidade : 1, 1, 8);
    for (int i = 0; i < j->quantidade_inimigos_batalha; ++i) {
        int id = limitar_int(inimigo + i, 0, a->projeto.quantidade_inimigos - 1);
        j->inimigos_batalha[i] = id;
        j->vidas_inimigos[i] = a->projeto.inimigos[id].vida;
    }
    j->inimigo = j->inimigos_batalha[0];
    j->vida_inimigo = j->vidas_inimigos[0];
    j->membro_escolhendo = 0;
    memset(j->acoes_grupo, 0, sizeof(j->acoes_grupo));
    memset(j->defesas_grupo, 0, sizeof(j->defesas_grupo));
    a->modo = MODO_BATALHA;
}

static int procurar_limite_bloco(Evento *e, int inicio, int nivel, int aceitar_senao) {
    for (int i = inicio; i < e->quantidade_comandos; ++i) {
        ComandoEvento *c = &e->comandos[i];
        if (c->profundidade == nivel &&
            ((aceitar_senao && c->tipo == COMANDO_SENAO) || c->tipo == COMANDO_FIM_BLOCO))
            return i;
    }
    return e->quantidade_comandos;
}

static void executar_evento(Aplicativo *a) {
    Jogo *j = &a->jogo;
    if (j->esperar_evento_ate > 0.0) {
        if (agora_segundos() < j->esperar_evento_ate)
            return;
        j->esperar_evento_ate = 0.0;
    }
    while (j->evento_ativo >= 0 && !j->aguardando_texto && !j->aguardando_escolha &&
           a->modo == MODO_JOGO) {
        Evento *e = &a->projeto.eventos[j->evento_ativo];
        if (j->proximo_comando >= e->quantidade_comandos) {
            if (j->topo_pilha > 0) {
                j->topo_pilha--;
                j->evento_ativo = j->pilha_eventos[j->topo_pilha];
                j->proximo_comando = j->pilha_comandos[j->topo_pilha];
                continue;
            }
            j->evento_ativo = -1;
            break;
        }
        ComandoEvento *c = &e->comandos[j->proximo_comando++];
        switch (c->tipo) {
        case COMANDO_TEXTO:
            copiar_texto(j->texto, sizeof(j->texto), c->texto);
            j->aguardando_texto = 1;
            break;
        case COMANDO_ESCOLHA:
            separar_escolhas(c->texto, j->escolha_a, sizeof(j->escolha_a), j->escolha_b,
                             sizeof(j->escolha_b));
            j->aguardando_escolha = 1;
            break;
        case COMANDO_FLAG:
            if (c->a >= 0 && c->a < MYTHARA_MAX_FLAGS)
                j->estado.flags[c->a] = c->b;
            break;
        case COMANDO_VARIAVEL:
            if (c->a >= 0 && c->a < MYTHARA_MAX_VARIAVEIS)
                j->estado.variaveis[c->a] += c->b;
            break;
        case COMANDO_ITEM:
            if (c->a >= 0 && c->a < a->projeto.quantidade_itens)
                j->estado.inventario[c->a] = limitar_int(j->estado.inventario[c->a] + c->b, 0, 99);
            break;
        case COMANDO_TELEPORTE:
            if (c->a >= 0 && c->a < a->projeto.quantidade_mapas) {
                Mapa *m = &a->projeto.mapas[c->a];
                j->estado.mapa = c->a;
                j->estado.x = limitar_int(c->b, 0, m->largura - 1);
                j->estado.y = limitar_int(c->c, 0, m->altura - 1);
            }
            break;
        case COMANDO_BATALHA:
            preparar_batalha(a, c->a, c->b);
            break;
        case COMANDO_CURAR:
            j->estado.vida = limitar_int(j->estado.vida + (c->a ? c->a : j->estado.vida_maxima), 0,
                                         j->estado.vida_maxima);
            break;
        case COMANDO_AUDIO:
            if (c->a >= 0 && c->a < a->projeto.quantidade_recursos &&
                a->projeto.recursos[c->a].tipo == RECURSO_WAV) {
                char caminho[MYTHARA_MAX_CAMINHO];
                caminho_do_recurso(&a->projeto, a->projeto.recursos[c->a].caminho, caminho,
                                   sizeof(caminho));
                tocar_wav(caminho);
            }
            break;
        case COMANDO_ESPERAR:
            j->esperar_evento_ate = agora_segundos() + (double)limitar_int(c->a, 0, 10000) / 1000.0;
            return;
        case COMANDO_SE:
            if (c->a < 0 || c->a >= MYTHARA_MAX_FLAGS || j->estado.flags[c->a] != c->b)
                j->proximo_comando =
                    procurar_limite_bloco(e, j->proximo_comando, c->profundidade, 1) + 1;
            break;
        case COMANDO_SENAO:
            j->proximo_comando =
                procurar_limite_bloco(e, j->proximo_comando, c->profundidade, 0) + 1;
            break;
        case COMANDO_FIM_BLOCO:
            if (j->topo_repeticao > 0 &&
                j->pilha_repeticao_nivel[j->topo_repeticao - 1] == c->profundidade) {
                int topo = j->topo_repeticao - 1;
                if (--j->pilha_repeticao_restante[topo] > 0)
                    j->proximo_comando = j->pilha_repeticao_pc[topo];
                else
                    j->topo_repeticao--;
            }
            break;
        case COMANDO_REPETIR:
            if (j->topo_repeticao < 8) {
                int topo = j->topo_repeticao++;
                j->pilha_repeticao_pc[topo] = j->proximo_comando;
                j->pilha_repeticao_restante[topo] = limitar_int(c->a, 1, 100);
                j->pilha_repeticao_nivel[topo] = c->profundidade;
            }
            break;
        case COMANDO_CHAMAR_EVENTO:
            if (c->a >= 0 && c->a < a->projeto.quantidade_eventos && j->topo_pilha < 8) {
                j->pilha_eventos[j->topo_pilha] = j->evento_ativo;
                j->pilha_comandos[j->topo_pilha] = j->proximo_comando;
                j->topo_pilha++;
                j->evento_ativo = c->a;
                j->proximo_comando = 0;
            }
            break;
        case COMANDO_LOJA:
            if (c->a >= 0 && c->a < a->projeto.quantidade_lojas) {
                j->loja_ativa = c->a;
                j->tela_sobreposta = 3;
                return;
            }
            break;
        case COMANDO_MISSAO:
            if (c->a >= 0 && c->a < a->projeto.quantidade_missoes) {
                j->estado.estado_missoes[c->a] = limitar_int(c->b, 0, 2);
                j->estado.progresso_missoes[c->a] = c->c;
            }
            break;
        default:
            break;
        }
    }
}

static int posicao_bloqueada(Aplicativo *a, int x, int y) {
    Mapa *m = &a->projeto.mapas[a->jogo.estado.mapa];
    if (x < 0 || y < 0 || x >= m->largura || y >= m->altura)
        return 1;
    if (m->colisoes[y * m->largura + x])
        return 1;
    for (int i = 0; i < m->quantidade_entidades; ++i)
        if (m->entidades[i].ativo && m->entidades[i].x == x && m->entidades[i].y == y)
            return 1;
    return 0;
}

static void atualizar_jogo(Aplicativo *a, Entrada *in) {
    Jogo *j = &a->jogo;
    char save[32];
    if (in->teclas_pressionadas[27]) {
        if (j->tela_sobreposta) {
            j->tela_sobreposta = 0;
            executar_evento(a);
            return;
        }
        a->modo = MODO_EDITOR;
        mensagem_aplicativo(a, "Teste encerrado.");
        return;
    }
    if (in->teclas_pressionadas['M'])
        j->tela_sobreposta = j->tela_sobreposta == 1 ? 0 : 1;
    if (in->teclas_pressionadas['J'])
        j->tela_sobreposta = j->tela_sobreposta == 2 ? 0 : 2;
    if (j->tela_sobreposta)
        return;
    if (j->slot < 1 || j->slot > 3)
        j->slot = 1;
    if (!j->aguardando_escolha) {
        if (in->teclas_pressionadas['1'])
            j->slot = 1;
        if (in->teclas_pressionadas['2'])
            j->slot = 2;
        if (in->teclas_pressionadas['3'])
            j->slot = 3;
    }
    snprintf(save, sizeof(save), "save%d.mys", j->slot);
    if (in->teclas_pressionadas['K'] || in->teclas_pressionadas[133]) {
        if (salvar_estado(&j->estado, save))
            snprintf(j->aviso, sizeof(j->aviso), "Jogo salvo no slot %d.", j->slot);
    }
    if (in->teclas_pressionadas['L'] || in->teclas_pressionadas[134]) {
        if (carregar_estado(&j->estado, save))
            snprintf(j->aviso, sizeof(j->aviso), "Slot %d carregado.", j->slot);
        else
            copiar_texto(j->aviso, sizeof(j->aviso), "Nenhum save valido.");
    }
    if (j->aguardando_texto) {
        if (in->teclas_pressionadas[32] || in->teclas_pressionadas[13] ||
            in->teclas_pressionadas['E']) {
            j->aguardando_texto = 0;
            executar_evento(a);
        }
        return;
    }
    if (j->aguardando_escolha) {
        if (in->teclas_pressionadas['1'] || in->teclas_pressionadas[128]) {
            Evento *e = &a->projeto.eventos[j->evento_ativo];
            ComandoEvento *c = &e->comandos[j->proximo_comando - 1];
            if (c->a >= 0 && c->a < MYTHARA_MAX_VARIAVEIS)
                j->estado.variaveis[c->a] = 0;
            j->aguardando_escolha = 0;
            executar_evento(a);
        }
        if (in->teclas_pressionadas['2'] || in->teclas_pressionadas[129]) {
            Evento *e = &a->projeto.eventos[j->evento_ativo];
            ComandoEvento *c = &e->comandos[j->proximo_comando - 1];
            if (c->a >= 0 && c->a < MYTHARA_MAX_VARIAVEIS)
                j->estado.variaveis[c->a] = 1;
            j->aguardando_escolha = 0;
            executar_evento(a);
        }
        return;
    }
    if (j->evento_ativo >= 0) {
        executar_evento(a);
        return;
    }
    double agora = agora_segundos();
    if (agora < j->proximo_movimento)
        return;
    int dx = 0, dy = 0;
    if (in->teclas[128] || in->teclas['A'] || in->teclas_pressionadas[128] ||
        in->teclas_pressionadas['A'])
        dx = -1;
    else if (in->teclas[129] || in->teclas['D'] || in->teclas_pressionadas[129] ||
             in->teclas_pressionadas['D'])
        dx = 1;
    else if (in->teclas[130] || in->teclas['W'] || in->teclas_pressionadas[130] ||
             in->teclas_pressionadas['W'])
        dy = -1;
    else if (in->teclas[131] || in->teclas['S'] || in->teclas_pressionadas[131] ||
             in->teclas_pressionadas['S'])
        dy = 1;
    if (dx || dy) {
        int nx = j->estado.x + dx, ny = j->estado.y + dy;
        if (!posicao_bloqueada(a, nx, ny)) {
            j->estado.x = nx;
            j->estado.y = ny;
            j->passos_encontro++;
            Mapa *m = &a->projeto.mapas[j->estado.mapa];
            if (m->regiao_encontro >= 0 && j->passos_encontro >= 12 && rand() % 10 == 0) {
                j->passos_encontro = 0;
                preparar_batalha(a, m->regiao_encontro, 1);
                return;
            }
        }
        j->proximo_movimento = agora + 0.12;
    }
    if (in->teclas_pressionadas[32] || in->teclas_pressionadas['E']) {
        Mapa *m = &a->projeto.mapas[j->estado.mapa];
        for (int i = 0; i < m->quantidade_entidades; ++i) {
            Entidade *n = &m->entidades[i];
            int distancia = abs(n->x - j->estado.x) + abs(n->y - j->estado.y);
            if (n->ativo && distancia <= 1) {
                iniciar_evento_jogo(a, n->evento);
                executar_evento(a);
                break;
            }
        }
    }
}

static void desenhar_mapa_jogo(Aplicativo *a, Tela *t, Retangulo area) {
    EstadoJogo *j = &a->jogo.estado;
    Mapa *m = &a->projeto.mapas[j->mapa];
    int tamanho = a->projeto.tamanho_tile ? a->projeto.tamanho_tile : MYTHARA_TAMANHO_TILE;
    int ox = area.x + area.largura / 2 - j->x * tamanho - tamanho / 2,
        oy = area.y + area.altura / 2 - j->y * tamanho - tamanho / 2;
    preencher_retangulo(t, area, cor_rgb(12, 16, 21));
    for (int y = 0; y < m->altura; ++y)
        for (int x = 0; x < m->largura; ++x) {
            int idx = y * m->largura + x;
            Retangulo q = {ox + x * tamanho, oy + y * tamanho, tamanho, tamanho};
            if (q.x + tamanho < area.x || q.y + tamanho < area.y || q.x >= area.x + area.largura ||
                q.y >= area.y + area.altura)
                continue;
            desenhar_tile_recurso(a, t, q, m->camadas[0].tiles[idx], 0);
            for (int c = 1; c < m->quantidade_camadas; ++c)
                if (m->camadas[c].visivel && m->camadas[c].tiles[idx])
                    contornar_retangulo(t,
                                        (Retangulo){q.x + 3 + c, q.y + 3 + c, tamanho - 6 - c * 2,
                                                    tamanho - 6 - c * 2},
                                        paleta_tiles[m->camadas[c].tiles[idx] & 15]);
        }
    for (int i = 0; i < m->quantidade_entidades; ++i)
        if (m->entidades[i].ativo) {
            Entidade *n = &m->entidades[i];
            Retangulo q = {ox + n->x * tamanho + 5, oy + n->y * tamanho + 3, 22, 27};
            preencher_retangulo(t, q, paleta_tiles[n->cor & 15]);
            contornar_retangulo(t, q, cor_rgb(237, 240, 245));
        }
    Retangulo heroi = {ox + j->x * tamanho + 5, oy + j->y * tamanho + 3, 22, 27};
    preencher_retangulo(t, heroi, cor_rgb(245, 204, 78));
    contornar_retangulo(t, heroi, cor_rgb(255, 244, 190));
    desenhar_texto(t, heroi.x + 8, heroi.y + 9, "H", cor_rgb(45, 39, 28), 1);
}

static void desenhar_menu_grupo(Aplicativo *a, Interface *ui) {
    Jogo *j = &a->jogo;
    Tela *t = ui->tela;
    Retangulo r = {70, 55, t->largura - 370, t->altura - 110};
    ui_painel(t, r, "GRUPO, ATRIBUTOS E EQUIPAMENTOS  [M/ESC FECHA]");
    int y = r.y + 42;
    for (int i = 0; i < j->estado.quantidade_grupo; ++i) {
        MembroGrupo *g = &j->estado.grupo[i];
        Heroi *h = &a->projeto.herois[g->heroi];
        char linha[160];
        snprintf(linha, sizeof(linha), "%s  NIVEL %d  ATQ %d  DEF %d  VEL %d", h->nome, g->nivel,
                 g->ataque, g->defesa, g->velocidade);
        desenhar_texto(t, r.x + 22, y, linha, tema_ativo.texto, 1);
        y += 17;
        ui_barra_progresso(t, (Retangulo){r.x + 22, y, 250, 12}, g->vida, g->vida_maxima,
                           tema_ativo.perigo);
        snprintf(linha, sizeof(linha), "PV %d/%d", g->vida, g->vida_maxima);
        desenhar_texto(t, r.x + 282, y + 2, linha, tema_ativo.texto_suave, 1);
        y += 17;
        ui_barra_progresso(t, (Retangulo){r.x + 22, y, 250, 10}, g->magia, g->magia_maxima,
                           tema_ativo.destaque);
        y += 26;
    }
    y = r.y + r.altura - 120;
    desenhar_texto(t, r.x + 22, y, "EQUIPAR PRIMEIRO ITEM DISPONIVEL", tema_ativo.texto_suave, 1);
    y += 18;
    if (ui_botao(ui, (Retangulo){r.x + 22, y, 220, 28}, "EQUIPAR NO HEROI 1", 0)) {
        for (int it = 0; it < a->projeto.quantidade_itens; ++it)
            if (j->estado.inventario[it] > 0 && a->projeto.itens[it].tipo_equipamento > 0) {
                int slot = limitar_int(a->projeto.itens[it].tipo_equipamento - 1, 0, 3);
                MembroGrupo *g = &j->estado.grupo[0];
                if (g->equipamentos[slot] >= 0)
                    j->estado.inventario[g->equipamentos[slot]]++;
                g->equipamentos[slot] = it;
                j->estado.inventario[it]--;
                g->ataque = a->projeto.herois[g->heroi].ataque + a->projeto.itens[it].ataque;
                g->defesa = a->projeto.herois[g->heroi].defesa + a->projeto.itens[it].defesa;
                copiar_texto(j->aviso, sizeof(j->aviso), "Equipamento atualizado.");
                break;
            }
    }
}

static void desenhar_diario_missoes(Aplicativo *a, Interface *ui) {
    Jogo *j = &a->jogo;
    Tela *t = ui->tela;
    Retangulo r = {100, 70, t->largura - 430, t->altura - 140};
    ui_painel(t, r, "DIARIO DE MISSOES  [J/ESC FECHA]");
    int y = r.y + 45;
    for (int i = 0; i < a->projeto.quantidade_missoes; ++i) {
        Missao *m = &a->projeto.missoes[i];
        const char *estado = j->estado.estado_missoes[i] == 2
                                 ? "CONCLUIDA"
                                 : (j->estado.estado_missoes[i] == 1 ? "ATIVA" : "DISPONIVEL");
        char linha[180];
        snprintf(linha, sizeof(linha), "%s  [%s]", m->nome, estado);
        desenhar_texto(t, r.x + 24, y, linha,
                       j->estado.estado_missoes[i] == 2 ? tema_ativo.sucesso : tema_ativo.destaque,
                       1);
        y += 18;
        desenhar_texto(t, r.x + 36, y, m->descricao, tema_ativo.texto_suave, 1);
        y += 30;
        if (m->quantidade_etapas) {
            snprintf(linha, sizeof(linha), "- %s  %d/%d", m->etapas[0].descricao,
                     j->estado.progresso_missoes[i], m->etapas[0].quantidade);
            desenhar_texto(t, r.x + 36, y, linha, tema_ativo.texto, 1);
            y += 28;
        }
    }
}

static void desenhar_loja_jogo(Aplicativo *a, Interface *ui) {
    Jogo *j = &a->jogo;
    Tela *t = ui->tela;
    Loja *loja = &a->projeto.lojas[limitar_int(j->loja_ativa, 0, a->projeto.quantidade_lojas - 1)];
    Retangulo r = {120, 70, t->largura - 470, t->altura - 140};
    ui_painel(t, r, loja->nome);
    char linha[128];
    snprintf(linha, sizeof(linha), "OURO: %d", j->estado.ouro);
    desenhar_texto(t, r.x + 25, r.y + 43, linha, tema_ativo.aviso, 1);
    int y = r.y + 75;
    for (int i = 0; i < loja->quantidade_itens; ++i) {
        int id = loja->itens[i];
        if (id < 0 || id >= a->projeto.quantidade_itens)
            continue;
        int preco = loja->precos[i] ? loja->precos[i] : a->projeto.itens[id].valor;
        snprintf(linha, sizeof(linha), "%s  -  %d OURO", a->projeto.itens[id].nome, preco);
        if (ui_botao(ui, (Retangulo){r.x + 24, y, 420, 30}, linha, 0)) {
            if (j->estado.ouro >= preco) {
                j->estado.ouro -= preco;
                j->estado.inventario[id] = limitar_int(j->estado.inventario[id] + 1, 0, 99);
                copiar_texto(j->aviso, sizeof(j->aviso), "Compra realizada.");
            } else
                copiar_texto(j->aviso, sizeof(j->aviso), "Ouro insuficiente.");
        }
        if (j->estado.inventario[id] > 0 &&
            ui_botao(ui, (Retangulo){r.x + 455, y, 150, 30}, "VENDER METADE", 0)) {
            j->estado.inventario[id]--;
            j->estado.ouro += preco / 2;
        }
        y += 38;
    }
    if (ui_botao(ui, (Retangulo){r.x + r.largura - 130, r.y + r.altura - 48, 105, 28},
                 "SAIR DA LOJA", 0)) {
        j->tela_sobreposta = 0;
        executar_evento(a);
    }
}

static void desenhar_jogo(Aplicativo *a, Interface *ui) {
    Jogo *j = &a->jogo;
    Tela *t = ui->tela;
    Retangulo mundo = {0, 0, t->largura - 230, t->altura};
    desenhar_mapa_jogo(a, t, mundo);
    ui_painel(t, (Retangulo){t->largura - 230, 0, 230, t->altura}, "MYTHARA - JOGO");
    char linha[100];
    int y = 42;
    snprintf(linha, sizeof(linha), "GRUPO: %s", a->projeto.herois[0].nome);
    desenhar_texto(t, t->largura - 216, y, linha, tema_ativo.texto, 1);
    y += 24;
    snprintf(linha, sizeof(linha), "VIDA: %d / %d", j->estado.vida, j->estado.vida_maxima);
    desenhar_texto(t, t->largura - 216, y, linha, cor_rgb(231, 117, 117), 1);
    y += 22;
    snprintf(linha, sizeof(linha), "NIVEL %d   XP %d", j->estado.nivel, j->estado.experiencia);
    desenhar_texto(t, t->largura - 216, y, linha, cor_rgb(181, 196, 216), 1);
    y += 22;
    snprintf(linha, sizeof(linha), "OURO: %d", j->estado.ouro);
    desenhar_texto(t, t->largura - 216, y, linha, cor_rgb(235, 205, 99), 1);
    y += 32;
    desenhar_texto(t, t->largura - 216, y, "INVENTARIO", cor_rgb(132, 194, 231), 1);
    y += 20;
    for (int i = 0; i < a->projeto.quantidade_itens; ++i)
        if (j->estado.inventario[i]) {
            snprintf(linha, sizeof(linha), "%s x%d", a->projeto.itens[i].nome,
                     j->estado.inventario[i]);
            desenhar_texto(t, t->largura - 216, y, linha, cor_rgb(205, 214, 224), 1);
            y += 18;
        }
    y = t->altura - 196;
    desenhar_texto(t, t->largura - 216, y, "SLOT DE SAVE", cor_rgb(132, 194, 231), 1);
    y += 18;
    for (int s = 1; s <= 3; ++s) {
        char slot[24];
        snprintf(slot, sizeof(slot), "SLOT %d", s);
        if (ui_botao(ui, (Retangulo){t->largura - 218 + (s - 1) * 69, y, 64, 25}, slot,
                     j->slot == s))
            j->slot = s;
    }
    y += 34;
    desenhar_texto(t, t->largura - 216, y, "WASD/SETAS: MOVER", cor_rgb(157, 169, 185), 1);
    desenhar_texto(t, t->largura - 216, y + 17, "E/ESPACO: INTERAGIR", cor_rgb(157, 169, 185), 1);
    desenhar_texto(t, t->largura - 216, y + 34, "F1/K SALVA  F2/L CARREGA", cor_rgb(157, 169, 185),
                   1);
    desenhar_texto(t, t->largura - 216, y + 51, "ESC: VOLTAR AO EDITOR", cor_rgb(157, 169, 185), 1);
    desenhar_texto(t, t->largura - 216, y + 78, j->aviso, cor_rgb(235, 191, 93), 1);
    if (j->aguardando_texto) {
        Retangulo caixa = {32, t->altura - 164, t->largura - 294, 132};
        preencher_retangulo(t, caixa, cor_rgb(24, 29, 39));
        contornar_retangulo(t, caixa, cor_rgb(187, 204, 225));
        desenhar_texto(t, caixa.x + 18, caixa.y + 18, j->texto, cor_rgb(235, 239, 244), 1);
        desenhar_texto(t, caixa.x + 18, caixa.y + 100, "ESPACO PARA CONTINUAR",
                       cor_rgb(137, 190, 225), 1);
    }
    if (j->aguardando_escolha) {
        Retangulo caixa = {t->largura / 2 - 220, t->altura / 2 - 85, 440, 170};
        preencher_retangulo(t, caixa, cor_rgb(24, 29, 39));
        contornar_retangulo(t, caixa, cor_rgb(187, 204, 225));
        desenhar_texto(t, caixa.x + 20, caixa.y + 20, "ESCOLHA UMA OPCAO", cor_rgb(225, 231, 239),
                       1);
        if (ui_botao(ui, (Retangulo){caixa.x + 20, caixa.y + 58, 400, 32}, j->escolha_a, 0)) {
            Evento *e = &a->projeto.eventos[j->evento_ativo];
            ComandoEvento *c = &e->comandos[j->proximo_comando - 1];
            if (c->a >= 0 && c->a < MYTHARA_MAX_VARIAVEIS)
                j->estado.variaveis[c->a] = 0;
            j->aguardando_escolha = 0;
            executar_evento(a);
        }
        if (ui_botao(ui, (Retangulo){caixa.x + 20, caixa.y + 102, 400, 32}, j->escolha_b, 0)) {
            Evento *e = &a->projeto.eventos[j->evento_ativo];
            ComandoEvento *c = &e->comandos[j->proximo_comando - 1];
            if (c->a >= 0 && c->a < MYTHARA_MAX_VARIAVEIS)
                j->estado.variaveis[c->a] = 1;
            j->aguardando_escolha = 0;
            executar_evento(a);
        }
    }
    if (j->tela_sobreposta == 1)
        desenhar_menu_grupo(a, ui);
    else if (j->tela_sobreposta == 2)
        desenhar_diario_missoes(a, ui);
    else if (j->tela_sobreposta == 3)
        desenhar_loja_jogo(a, ui);
}

static int primeiro_inimigo_vivo(const Jogo *j) {
    for (int i = 0; i < j->quantidade_inimigos_batalha; ++i)
        if (j->vidas_inimigos[i] > 0)
            return i;
    return -1;
}
static int primeiro_heroi_vivo(const EstadoJogo *e) {
    for (int i = 0; i < e->quantidade_grupo; ++i)
        if (e->grupo[i].vida > 0)
            return i;
    return -1;
}

static void perder_batalha(Aplicativo *a) {
    Jogo *j = &a->jogo;
    for (int i = 0; i < j->estado.quantidade_grupo; ++i)
        j->estado.grupo[i].vida = j->estado.grupo[i].vida_maxima;
    j->estado.vida = j->estado.vida_maxima;
    j->estado.mapa = a->projeto.mapa_inicial;
    j->estado.x = a->projeto.inicio_x;
    j->estado.y = a->projeto.inicio_y;
    j->evento_ativo = -1;
    a->modo = MODO_JOGO;
    copiar_texto(j->aviso, sizeof(j->aviso), "Derrota! O grupo retornou ao inicio.");
}

static void vencer_batalha(Aplicativo *a) {
    Jogo *j = &a->jogo;
    int xp = 0, ouro = 0;
    for (int i = 0; i < j->quantidade_inimigos_batalha; ++i) {
        Inimigo *n = &a->projeto.inimigos[j->inimigos_batalha[i]];
        xp += n->experiencia;
        ouro += n->ouro;
        for (int q = 0; q < a->projeto.quantidade_missoes; ++q)
            if (j->estado.estado_missoes[q] == 1 && a->projeto.missoes[q].quantidade_etapas &&
                a->projeto.missoes[q].etapas[0].tipo == 1 &&
                a->projeto.missoes[q].etapas[0].alvo == j->inimigos_batalha[i]) {
                j->estado.progresso_missoes[q]++;
                if (j->estado.progresso_missoes[q] >= a->projeto.missoes[q].etapas[0].quantidade) {
                    j->estado.estado_missoes[q] = 2;
                    ouro += a->projeto.missoes[q].recompensa_ouro;
                    xp += a->projeto.missoes[q].recompensa_experiencia;
                }
            }
    }
    j->estado.experiencia += xp;
    j->estado.ouro += ouro;
    if (j->estado.experiencia >= j->estado.nivel * 20) {
        j->estado.nivel++;
        for (int i = 0; i < j->estado.quantidade_grupo; ++i) {
            j->estado.grupo[i].nivel++;
            j->estado.grupo[i].vida_maxima += 5;
            j->estado.grupo[i].vida = j->estado.grupo[i].vida_maxima;
            j->estado.grupo[i].ataque += 2;
        }
    }
    j->estado.vida = j->estado.grupo[0].vida;
    j->estado.vida_maxima = j->estado.grupo[0].vida_maxima;
    snprintf(j->aviso, sizeof(j->aviso), "Vitoria! +%d XP, +%d ouro.", xp, ouro);
    a->modo = MODO_JOGO;
    executar_evento(a);
}

typedef struct {
    int lado, indice, velocidade;
} ParticipanteTurno;
static void resolver_rodada(Aplicativo *a) {
    Jogo *j = &a->jogo;
    ParticipanteTurno ordem[12];
    int total = 0;
    memset(j->defesas_grupo, 0, sizeof(j->defesas_grupo));
    for (int i = 0; i < j->estado.quantidade_grupo; ++i)
        if (j->estado.grupo[i].vida > 0)
            ordem[total++] = (ParticipanteTurno){0, i, j->estado.grupo[i].velocidade};
    for (int i = 0; i < j->quantidade_inimigos_batalha; ++i)
        if (j->vidas_inimigos[i] > 0)
            ordem[total++] =
                (ParticipanteTurno){1, i, a->projeto.inimigos[j->inimigos_batalha[i]].velocidade};
    for (int i = 0; i < total; ++i)
        for (int k = i + 1; k < total; ++k)
            if (ordem[k].velocidade > ordem[i].velocidade) {
                ParticipanteTurno t = ordem[i];
                ordem[i] = ordem[k];
                ordem[k] = t;
            }
    for (int o = 0; o < total; ++o) {
        if (primeiro_inimigo_vivo(j) < 0 || primeiro_heroi_vivo(&j->estado) < 0)
            break;
        if (!ordem[o].lado) {
            MembroGrupo *g = &j->estado.grupo[ordem[o].indice];
            if (g->vida <= 0)
                continue;
            int alvo = primeiro_inimigo_vivo(j);
            Inimigo *n = &a->projeto.inimigos[j->inimigos_batalha[alvo]];
            int acao = j->acoes_grupo[ordem[o].indice];
            if (acao == 3) {
                j->defesas_grupo[ordem[o].indice] = 1;
                continue;
            }
            if (acao == 2) {
                for (int it = 0; it < a->projeto.quantidade_itens; ++it)
                    if (j->estado.inventario[it] > 0 && a->projeto.itens[it].cura > 0) {
                        j->estado.inventario[it]--;
                        g->vida =
                            limitar_int(g->vida + a->projeto.itens[it].cura, 0, g->vida_maxima);
                        break;
                    }
                continue;
            }
            int dano;
            if (acao == 1 && a->projeto.quantidade_habilidades &&
                g->magia >= a->projeto.habilidades[0].custo_mp) {
                Habilidade *h = &a->projeto.habilidades[0];
                g->magia -= h->custo_mp;
                dano = limitar_int(g->poder_magico + h->poder - n->resistencia / 2, 1, 99999);
            } else
                dano = limitar_int(g->ataque - n->defesa / 2, 1, 99999);
            j->vidas_inimigos[alvo] -= dano;
        } else {
            int id = ordem[o].indice;
            if (j->vidas_inimigos[id] <= 0)
                continue;
            int alvo = primeiro_heroi_vivo(&j->estado);
            MembroGrupo *g = &j->estado.grupo[alvo];
            Inimigo *n = &a->projeto.inimigos[j->inimigos_batalha[id]];
            int dano = limitar_int(n->ataque - g->defesa / 2, 1, 99999);
            if (j->defesas_grupo[alvo])
                dano = (dano + 1) / 2;
            g->vida -= dano;
        }
    }
    if (primeiro_inimigo_vivo(j) < 0) {
        vencer_batalha(a);
        return;
    }
    if (primeiro_heroi_vivo(&j->estado) < 0) {
        perder_batalha(a);
        return;
    }
    j->membro_escolhendo = 0;
    while (j->membro_escolhendo < j->estado.quantidade_grupo &&
           j->estado.grupo[j->membro_escolhendo].vida <= 0)
        j->membro_escolhendo++;
    j->estado.vida = j->estado.grupo[0].vida;
    j->estado.magia = j->estado.grupo[0].magia;
    copiar_texto(j->aviso, sizeof(j->aviso), "Nova rodada: escolha as acoes por velocidade.");
}

static void escolher_acao_batalha(Aplicativo *a, int acao) {
    Jogo *j = &a->jogo;
    if (j->membro_escolhendo >= j->estado.quantidade_grupo)
        return;
    j->acoes_grupo[j->membro_escolhendo] = acao;
    do {
        j->membro_escolhendo++;
    } while (j->membro_escolhendo < j->estado.quantidade_grupo &&
             j->estado.grupo[j->membro_escolhendo].vida <= 0);
    if (j->membro_escolhendo >= j->estado.quantidade_grupo)
        resolver_rodada(a);
}

static void desenhar_batalha(Aplicativo *a, Interface *ui) {
    Tela *t = ui->tela;
    Jogo *j = &a->jogo;
    limpar_tela(t, cor_rgb(22, 20, 32));
    desenhar_texto(t, 35, 28,
                   a->projeto.batalha_lateral ? "BATALHA - VISAO LATERAL"
                                              : "BATALHA - VISAO FRONTAL",
                   tema_ativo.aviso, 2);
    char linha[128];
    for (int i = 0; i < j->estado.quantidade_grupo; ++i) {
        MembroGrupo *g = &j->estado.grupo[i];
        int x = a->projeto.batalha_lateral ? 80 + i * 120 : 60,
            y = a->projeto.batalha_lateral ? 170 + i * 65 : t->altura - 260 + i * 42;
        Retangulo corpo = {x, y, 58, 58};
        preencher_retangulo(t, corpo, paleta_tiles[(10 + i) & 15]);
        contornar_retangulo(t, corpo,
                            i == j->membro_escolhendo ? tema_ativo.aviso : tema_ativo.borda);
        snprintf(linha, sizeof(linha), "%s %d/%d", a->projeto.herois[g->heroi].nome, g->vida,
                 g->vida_maxima);
        desenhar_texto(t, x, y + 66, linha, tema_ativo.texto, 1);
        ui_barra_progresso(t, (Retangulo){x, y + 80, 100, 9}, g->vida, g->vida_maxima,
                           tema_ativo.perigo);
    }
    for (int i = 0; i < j->quantidade_inimigos_batalha; ++i) {
        Inimigo *n = &a->projeto.inimigos[j->inimigos_batalha[i]];
        int x = a->projeto.batalha_lateral
                    ? t->largura - 180 - i * 120
                    : t->largura / 2 - ((j->quantidade_inimigos_batalha * 110) / 2) + i * 110,
            y = a->projeto.batalha_lateral ? 155 + i * 58 : 145;
        Retangulo corpo = {x, y, 72, 72};
        preencher_retangulo(t, corpo, paleta_tiles[(2 + i) & 15]);
        contornar_retangulo(t, corpo, tema_ativo.perigo);
        snprintf(linha, sizeof(linha), "%s %d", n->nome,
                 j->vidas_inimigos[i] > 0 ? j->vidas_inimigos[i] : 0);
        desenhar_texto(t, x, y + 80, linha, tema_ativo.texto, 1);
        ui_barra_progresso(t, (Retangulo){x, y + 94, 95, 9}, j->vidas_inimigos[i], n->vida,
                           tema_ativo.perigo);
    }
    if (j->membro_escolhendo < j->estado.quantidade_grupo) {
        MembroGrupo *g = &j->estado.grupo[j->membro_escolhendo];
        snprintf(linha, sizeof(linha), "ACAO DE %s", a->projeto.herois[g->heroi].nome);
        desenhar_texto(t, 45, t->altura - 145, linha, tema_ativo.destaque, 1);
    }
    int y = t->altura - 110, x = 40;
    if (ui_botao(ui, (Retangulo){x, y, 130, 38}, "ATACAR", 0))
        escolher_acao_batalha(a, 0);
    if (ui_botao(ui, (Retangulo){x + 140, y, 130, 38}, "HABILIDADE", 0))
        escolher_acao_batalha(a, 1);
    if (ui_botao(ui, (Retangulo){x + 280, y, 130, 38}, "ITEM", 0))
        escolher_acao_batalha(a, 2);
    if (ui_botao(ui, (Retangulo){x + 420, y, 130, 38}, "DEFENDER", 0))
        escolher_acao_batalha(a, 3);
    if (ui_botao(ui, (Retangulo){x + 560, y, 130, 38}, "FUGIR", 0)) {
        if (rand() % 2) {
            a->modo = MODO_JOGO;
            j->evento_ativo = -1;
            copiar_texto(j->aviso, sizeof(j->aviso), "O grupo fugiu.");
        } else
            resolver_rodada(a);
    }
    desenhar_texto(t, 45, t->altura - 45, j->aviso, tema_ativo.aviso, 1);
}

/* ========================================================================== */
/* Autotestes, linha de comando e programa principal                           */
/* ========================================================================== */

static int criar_qoi_teste(const char *caminho) {
    const uint8_t dados[] = {'q',  'o',  'i',  'f',  0,    0, 0, 1, 0, 0, 0, 1, 4, 0,
                             0xff, 0x22, 0x88, 0xdd, 0xff, 0, 0, 0, 0, 0, 0, 0, 1};
    FILE *f = fopen(caminho, "wb");
    if (!f)
        return 0;
    int ok = fwrite(dados, sizeof(dados), 1, f) == 1;
    fclose(f);
    return ok;
}

static int criar_tga_teste(const char *caminho) {
    uint8_t dados[22] = {0};
    dados[2] = 2;
    dados[12] = 1;
    dados[14] = 1;
    dados[16] = 32;
    dados[17] = 0x20;
    dados[18] = 0xdd;
    dados[19] = 0x88;
    dados[20] = 0x22;
    dados[21] = 0xff;
    FILE *f = fopen(caminho, "wb");
    if (!f)
        return 0;
    int ok = fwrite(dados, sizeof(dados), 1, f) == 1;
    fclose(f);
    return ok;
}

static int executar_autotestes(void) {
    int falhas = 0;
    Projeto p, q;
    EstadoJogo j, k;
    char caminho[128], erro[256];
#define VERIFICAR(condicao, nome)                                                                  \
    do {                                                                                           \
        if (condicao)                                                                              \
            printf("[OK] %s\n", nome);                                                             \
        else {                                                                                     \
            printf("[FALHA] %s\n", nome);                                                          \
            falhas++;                                                                              \
        }                                                                                          \
    } while (0)
    iniciar_projeto(&p);
    VERIFICAR(p.quantidade_mapas == 1 && p.mapas[0].largura == 24, "projeto inicial");
    snprintf(caminho, sizeof(caminho), "/tmp/mythara_teste_%ld.myr", (long)getpid());
    VERIFICAR(salvar_projeto_em(&p, caminho, erro, sizeof(erro)), "salvar projeto atomico");
    memset(&q, 0, sizeof(q));
    VERIFICAR(carregar_projeto_de(&q, caminho, erro, sizeof(erro)) && !strcmp(q.nome, p.nome),
              "carregar e validar projeto");
    Identificador evento_referenciado = q.mapas[0].entidades[0].evento_id;
    Evento troca_evento = q.eventos[0];
    q.eventos[0] = q.eventos[1];
    q.eventos[1] = troca_evento;
    atribuir_ids_ausentes(&q);
    VERIFICAR(q.mapas[0].entidades[0].evento == 1 &&
                  q.eventos[q.mapas[0].entidades[0].evento].id == evento_referenciado,
              "referencia de entidade permanece estavel por ID");
    troca_evento = q.eventos[0];
    q.eventos[0] = q.eventos[1];
    q.eventos[1] = troca_evento;
    atribuir_ids_ausentes(&q);
    FILE *corrompido = fopen(caminho, "r+b");
    int detectou = 0;
    if (corrompido && fseek(corrompido, -1, SEEK_END) == 0) {
        int byte = fgetc(corrompido);
        fseek(corrompido, -1, SEEK_END);
        fputc(byte ^ 0xff, corrompido);
        fclose(corrompido);
        detectou = !carregar_projeto_de(&q, caminho, erro, sizeof(erro));
    } else if (corrompido)
        fclose(corrompido);
    VERIFICAR(detectou, "detectar projeto corrompido");
    unlink(caminho);
    snprintf(caminho, sizeof(caminho), "/tmp/mythara_v2_%ld.myr", (long)getpid());
    FILE *antigo = fopen(caminho, "wb");
    if (antigo) {
        CabecalhoProjeto h = {{'M', 'Y', 'T', 'H', 'R', 'V', '2', '\0'}, 2, 0};
        fwrite(&h, sizeof(h), 1, antigo);
        fclose(antigo);
    }
    VERIFICAR(!carregar_projeto_de(&q, caminho, erro, sizeof(erro)) &&
                  strstr(erro, "somente arquivos v3") != NULL,
              "rejeitar formato v2 com mensagem clara");
    unlink(caminho);
    iniciar_estado_jogo(&p, &j);
    snprintf(caminho, sizeof(caminho), "/tmp/mythara_teste_%ld.mys", (long)getpid());
    VERIFICAR(salvar_estado(&j, caminho), "salvar estado do jogo");
    memset(&k, 0, sizeof(k));
    VERIFICAR(carregar_estado(&k, caminho) && k.vida == j.vida, "carregar estado do jogo");
    unlink(caminho);
    uint8_t dados[] = {1, 2, 3, 4};
    VERIFICAR(soma_fnv1a(dados, sizeof(dados)) != soma_fnv1a(dados, sizeof(dados) - 1),
              "integridade FNV-1a");
    VERIFICAR(p.mapas[0].colisoes[0] && p.mapas[0].colisoes[p.mapas[0].largura + 1] == 0,
              "mapa e colisoes");
    Imagem imagem = {0};
    snprintf(caminho, sizeof(caminho), "/tmp/mythara_imagem_%ld.qoi", (long)getpid());
    criar_qoi_teste(caminho);
    VERIFICAR(carregar_qoi(caminho, &imagem, erro, sizeof(erro)) && imagem.largura == 1 &&
                  imagem.pixels[0] == 0xff2288dd,
              "decodificar imagem QOI");
    liberar_imagem(&imagem);
    unlink(caminho);
    snprintf(caminho, sizeof(caminho), "/tmp/mythara_imagem_%ld.tga", (long)getpid());
    criar_tga_teste(caminho);
    VERIFICAR(carregar_tga(caminho, &imagem, erro, sizeof(erro)) && imagem.altura == 1 &&
                  imagem.pixels[0] == 0xff2288dd,
              "decodificar imagem TGA");
    liberar_imagem(&imagem);
    unlink(caminho);
    Aplicativo *teste = calloc(1, sizeof(*teste));
    if (teste) {
        teste->projeto = p;
        iniciar_estado_jogo(&teste->projeto, &teste->jogo.estado);
        int idx = 2 * p.mapas[0].largura + 2;
        int antes = teste->projeto.mapas[0].camadas[0].tiles[idx];
        teste->projeto.mapas[0].camadas[0].tiles[idx] = 7;
        registrar_edicao(&teste->editor, 0, 0, idx, antes, 7);
        desfazer_edicao(teste);
        int desfez = teste->projeto.mapas[0].camadas[0].tiles[idx] == antes;
        refazer_edicao(teste);
        VERIFICAR(desfez && teste->projeto.mapas[0].camadas[0].tiles[idx] == 7,
                  "historico desfazer/refazer");
        teste->jogo.evento_ativo = -1;
        teste->projeto.eventos[0].condicao_flag = 5;
        teste->projeto.eventos[0].condicao_valor = 1;
        iniciar_evento_jogo(teste, 0);
        int bloqueou = teste->jogo.evento_ativo < 0;
        teste->jogo.estado.flags[5] = 1;
        iniciar_evento_jogo(teste, 0);
        VERIFICAR(bloqueou && teste->jogo.evento_ativo == 0, "condicao de evento por flag");
        teste->editor.tile = 16;
        teste->editor.camada = 0;
        int largura = p.mapas[0].largura;
        int centro = 4 * largura + 4;
        p.mapas[0].camadas[0].tiles[centro] = 16;
        p.mapas[0].camadas[0].tiles[centro - largura] = 16;
        p.mapas[0].camadas[0].tiles[centro + 1] = 16;
        p.mapas[0].camadas[0].tiles[centro + largura] = 16;
        p.mapas[0].camadas[0].tiles[centro - 1] = 16;
        atualizar_autotile(teste, 4, 4);
        VERIFICAR(p.mapas[0].camadas[0].tiles[centro] == 31, "autotile ortogonal de quatro lados");
        Evento *evento = &teste->projeto.eventos[0];
        evento->condicao_flag = -1;
        evento->quantidade_comandos = 8;
        reservar_comandos(evento, 8);
        memset(evento->comandos, 0, 8 * sizeof(*evento->comandos));
        evento->comandos[0] = (ComandoEvento){COMANDO_SE, 10, 1, 0, 0, ""};
        evento->comandos[1] = (ComandoEvento){COMANDO_VARIAVEL, 0, 5, 0, 1, ""};
        evento->comandos[2] = (ComandoEvento){COMANDO_SENAO, 0, 0, 0, 0, ""};
        evento->comandos[3] = (ComandoEvento){COMANDO_VARIAVEL, 0, 2, 0, 1, ""};
        evento->comandos[4] = (ComandoEvento){COMANDO_FIM_BLOCO, 0, 0, 0, 0, ""};
        evento->comandos[5] = (ComandoEvento){COMANDO_REPETIR, 3, 0, 0, 0, ""};
        evento->comandos[6] = (ComandoEvento){COMANDO_VARIAVEL, 1, 1, 0, 1, ""};
        evento->comandos[7] = (ComandoEvento){COMANDO_FIM_BLOCO, 0, 0, 0, 0, ""};
        teste->jogo.estado.flags[10] = 1;
        teste->modo = MODO_JOGO;
        iniciar_evento_jogo(teste, 0);
        executar_evento(teste);
        VERIFICAR(teste->jogo.estado.variaveis[0] == 5 && teste->jogo.estado.variaveis[1] == 3,
                  "blocos condicionais e repeticao de eventos");
        VERIFICAR(teste->projeto.quantidade_herois >= 2 &&
                      teste->jogo.estado.quantidade_grupo >= 2 && teste->projeto.quantidade_lojas &&
                      teste->projeto.quantidade_missoes,
                  "grupo, loja e missoes JRPG");
        int ouro_antes = teste->jogo.estado.ouro;
        preparar_batalha(teste, 0, 2);
        for (int i = 0; i < teste->jogo.quantidade_inimigos_batalha; ++i)
            teste->jogo.vidas_inimigos[i] = 1;
        for (int i = 0; i < teste->jogo.estado.quantidade_grupo; ++i)
            teste->jogo.acoes_grupo[i] = 0;
        resolver_rodada(teste);
        VERIFICAR(teste->modo == MODO_JOGO && teste->jogo.estado.ouro > ouro_antes,
                  "combate por velocidade com grupo e multiplos inimigos");
        free(teste);
    } else {
        VERIFICAR(0, "alocacao do teste de editor");
    }
    Mapa *mapa = &p.mapas[0];
    int camada = mapa->quantidade_camadas++;
    memset(&mapa->camadas[camada], 0, sizeof(mapa->camadas[camada]));
    copiar_texto(mapa->camadas[camada].nome, sizeof(mapa->camadas[camada].nome), "Luz de teste");
    mapa->camadas[camada].visivel = 1;
    mapa->camadas[camada].tiles = calloc((size_t)mapa->largura * mapa->altura, sizeof(uint16_t));
    VERIFICAR(mapa->camadas[camada].tiles && redimensionar_mapa(mapa, 256, 256) &&
                  mapa->camadas[camada].tiles[0] == 0,
              "mapa dinamico 256x256 com camada nomeada");
    liberar_projeto(&q);
    memset(&q, 0, sizeof(q));
    snprintf(caminho, sizeof(caminho), "/tmp/mythara_grande_%ld.myr", (long)getpid());
    int ida_volta = salvar_projeto_em(&p, caminho, erro, sizeof(erro)) &&
                    carregar_projeto_de(&q, caminho, erro, sizeof(erro));
    VERIFICAR(ida_volta && q.mapas[0].largura == 256 &&
                  q.mapas[0].quantidade_camadas == mapa->quantidade_camadas &&
                  !strcmp(q.mapas[0].camadas[camada].nome, "Luz de teste") &&
                  q.mapas[0].id == p.mapas[0].id &&
                  q.mapas[0].camadas[0].id == p.mapas[0].camadas[0].id,
              "persistir mapa grande, camadas e IDs v3");
    unlink(caminho);
    Aplicativo *seguranca = calloc(1, sizeof(*seguranca));
    if (seguranca) {
        char pasta_teste[MYTHARA_MAX_CAMINHO], rotacao[MYTHARA_MAX_CAMINHO];
        iniciar_projeto(&seguranca->projeto);
        snprintf(pasta_teste, sizeof(pasta_teste), "/tmp/mythara_auto_%ld", (long)getpid());
        juntar_caminho(seguranca->caminho_projeto, sizeof(seguranca->caminho_projeto), pasta_teste,
                       "projeto.myr");
        preparar_pasta_projeto(&seguranca->projeto, seguranca->caminho_projeto);
        definir_caminho_autosave(seguranca);
        copiar_texto(seguranca->projeto.nome, sizeof(seguranca->projeto.nome),
                     "Sessao recuperavel");
        seguranca->editor.alterado = 1;
        seguranca->ultimo_autosalvamento = 0;
        realizar_autosave(seguranca);
        while (atomic_load(&seguranca->autosave_em_andamento))
            pausar_milissegundos(1);
        int criou = access(seguranca->caminho_autosave, F_OK) == 0;
        copiar_texto(seguranca->projeto.nome, sizeof(seguranca->projeto.nome), "Segunda revisao");
        seguranca->ultimo_autosalvamento = 0;
        realizar_autosave(seguranca);
        while (atomic_load(&seguranca->autosave_em_andamento))
            pausar_milissegundos(1);
        juntar_caminho(rotacao, sizeof(rotacao), seguranca->projeto.pasta_base,
                       ".mythara/backups/autosave_1.myr");
        int girou = access(rotacao, F_OK) == 0;
        copiar_texto(seguranca->projeto.nome, sizeof(seguranca->projeto.nome), "Descartar");
        int recuperou =
            recuperar_autosave(seguranca) && !strcmp(seguranca->projeto.nome, "Segunda revisao");
        VERIFICAR(criou && girou && recuperou, "autosave v3 rotativo, assincrono e recuperavel");
        for (int i = 0; i < 10; ++i) {
            char arquivo[MYTHARA_MAX_CAMINHO], nome[32];
            snprintf(nome, sizeof(nome), ".mythara/backups/autosave_%d.myr", i);
            juntar_caminho(arquivo, sizeof(arquivo), pasta_teste, nome);
            unlink(arquivo);
        }
        liberar_historico_global(&seguranca->editor);
        liberar_projeto(&seguranca->projeto);
        char pasta_interna[MYTHARA_MAX_CAMINHO];
        const char *pastas_remover[] = {"recursos/imagens", "recursos/audio", "recursos",
                                        ".mythara/backups", ".mythara",       "exportacoes"};
        for (size_t i = 0; i < sizeof(pastas_remover) / sizeof(pastas_remover[0]); ++i) {
            juntar_caminho(pasta_interna, sizeof(pasta_interna), pasta_teste, pastas_remover[i]);
            rmdir(pasta_interna);
        }
        rmdir(pasta_teste);
        free(seguranca);
    } else
        VERIFICAR(0, "alocacao do teste de autosave");
    Aplicativo *fluxo = calloc(1, sizeof(*fluxo));
    if (fluxo) {
        iniciar_projeto(&fluxo->projeto);
        fluxo->editor.mapa_atual = 0;
        reiniciar_historico_global(fluxo);
        Entrada entrada_vazia = {0};
        int indice_historico = 2 * fluxo->projeto.mapas[0].largura + 2;
        uint16_t tile_inicial = fluxo->projeto.mapas[0].camadas[0].tiles[indice_historico];
        fluxo->projeto.mapas[0].camadas[0].tiles[indice_historico] = 77;
        copiar_texto(fluxo->projeto.autor, sizeof(fluxo->projeto.autor), "Autora do teste");
        verificar_historico_global(fluxo, &entrada_vazia);
        desfazer_edicao(fluxo);
        int historico_desfez =
            fluxo->projeto.mapas[0].camadas[0].tiles[indice_historico] == tile_inicial &&
            strcmp(fluxo->projeto.autor, "Autora do teste");
        refazer_edicao(fluxo);
        VERIFICAR(historico_desfez &&
                      fluxo->projeto.mapas[0].camadas[0].tiles[indice_historico] == 77 &&
                      !strcmp(fluxo->projeto.autor, "Autora do teste"),
                  "historico global entre mapa e propriedades");
        Mapa *mapa_fluxo = &fluxo->projeto.mapas[0];
        int entidades_antes = mapa_fluxo->quantidade_entidades;
        Identificador id_origem = mapa_fluxo->entidades[0].id;
        fluxo->editor.selecao_x0 = fluxo->editor.selecao_x1 = 7;
        fluxo->editor.selecao_y0 = fluxo->editor.selecao_y1 = 6;
        fluxo->editor.selecao_ativa = 1;
        copiar_area_transferencia(fluxo, 0);
        colar_area_transferencia(fluxo, 15, 12);
        Entidade *entidade_colada = &mapa_fluxo->entidades[mapa_fluxo->quantidade_entidades - 1];
        VERIFICAR(mapa_fluxo->quantidade_entidades == entidades_antes + 1 &&
                      entidade_colada->x == 15 && entidade_colada->y == 12 &&
                      entidade_colada->id != id_origem,
                  "clipboard estruturado com entidade e novo ID");
        liberar_historico_global(&fluxo->editor);
        liberar_projeto(&fluxo->projeto);
        free(fluxo);
    } else
        VERIFICAR(0, "alocacao dos testes de fluxo do editor");
    char texto_utf8[64] = "acao";
    size_t cursor_utf8 = strlen(texto_utf8), ancora_utf8 = cursor_utf8;
    campo_inserir(texto_utf8, sizeof(texto_utf8), &cursor_utf8, &ancora_utf8, " ção",
                  strlen(" ção"));
    size_t posicao_anterior = utf8_anterior(texto_utf8, cursor_utf8);
    VERIFICAR(!strcmp(texto_utf8, "acao ção") && posicao_anterior == cursor_utf8 - 1 &&
                  utf8_quantidade(texto_utf8, 0, cursor_utf8) == 8,
              "edicao de texto e navegacao UTF-8");
    uint32_t pixels_origem[4] = {1, 2, 3, 4}, pixels_destino[16] = {0};
    Tela tela_origem = {pixels_origem, 2, 2}, tela_destino = {pixels_destino, 4, 4};
    copiar_tela_escalada(&tela_origem, &tela_destino);
    VERIFICAR(pixels_destino[0] == 1 && pixels_destino[3] == 2 && pixels_destino[12] == 3 &&
                  pixels_destino[15] == 4,
              "escala real da interface preserva os cantos");
    char pasta_exportada[128];
    snprintf(pasta_exportada, sizeof(pasta_exportada), "/tmp/mythara_exportacao_%ld",
             (long)getpid());
    int exportou = exportar_projeto(&p, pasta_exportada, erro, sizeof(erro));
    char jogo_exportado[180];
    snprintf(jogo_exportado, sizeof(jogo_exportado), "%s/jogo.myr", pasta_exportada);
    Projeto distribuido = {0};
    int abriu_exportado =
        exportou && carregar_projeto_de(&distribuido, jogo_exportado, erro, sizeof(erro));
    VERIFICAR(abriu_exportado && !strcmp(distribuido.nome, p.nome),
              "exportar jogo independente com projeto valido");
    liberar_projeto(&distribuido);
    char arquivo_exportado[180];
    snprintf(arquivo_exportado, sizeof(arquivo_exportado), "%s/jogo", pasta_exportada);
    unlink(arquivo_exportado);
    unlink(jogo_exportado);
    snprintf(arquivo_exportado, sizeof(arquivo_exportado), "%s/recursos", pasta_exportada);
    rmdir(arquivo_exportado);
    rmdir(pasta_exportada);
    liberar_projeto(&q);
    liberar_projeto(&p);
    printf("%s: %d falha(s).\n", falhas ? "Autotestes concluidos" : "Todos os autotestes passaram",
           falhas);
#undef VERIFICAR
    return falhas ? 1 : 0;
}

static void mostrar_ajuda_terminal(void) {
    puts("Mythara - motor e editor visual de JRPG 2D\n"
         "Uso:\n"
         "  ./mythara [projeto.myr]       Abre o editor\n"
         "  ./mythara --jogar projeto.myr Executa o jogo\n"
         "  ./mythara --novo projeto.myr  Cria e abre um projeto\n"
         "  ./mythara --recuperar projeto.myr Recupera o autosave mais recente\n"
         "  ./mythara --resetar-layout     Restaura tema e paineis padrao\n"
         "  ./mythara --autoteste          Executa testes internos\n"
         "  ./mythara --ajuda              Mostra esta ajuda\n\n"
         "Linux: requer X11 e, por padrao, ALSA; MYTHARA_SEM_AUDIO dispensa ALSA.\n"
         "Windows: usa somente Win32/GDI/WinMM do proprio sistema.");
}

static void mudar_para_pasta_executavel(void) {
    char caminho[MYTHARA_MAX_CAMINHO];
    if (!caminho_do_executavel(caminho, sizeof(caminho)))
        return;
    char *barra = strrchr(caminho, '/');
#ifdef _WIN32
    char *barra_win = strrchr(caminho, '\\');
    if (!barra || barra_win > barra)
        barra = barra_win;
#endif
    if (barra) {
        *barra = 0;
        chdir(caminho);
    }
}

int main(int argc, char **argv) {
    srand((unsigned)time(NULL));
    iniciar_tema_padrao();
    if (argc > 1 && !strcmp(argv[1], "--autoteste"))
        return executar_autotestes();
    if (argc > 1 && (!strcmp(argv[1], "--ajuda") || !strcmp(argv[1], "-h"))) {
        mostrar_ajuda_terminal();
        return 0;
    }
    Aplicativo *a = calloc(1, sizeof(*a));
    if (!a) {
        fprintf(stderr, "Mythara: memoria insuficiente.\n");
        return 1;
    }
    a->mostrar_boas_vindas = 1;
    a->dicas_ativas = 1;
    novo_projeto(a);
    a->editor.alterado = 0;
    int somente_jogo = 0, resetar_layout = argc > 1 && !strcmp(argv[1], "--resetar-layout");
    if (argc > 2 && !strcmp(argv[1], "--novo")) {
        copiar_texto(a->caminho_projeto, sizeof(a->caminho_projeto), argv[2]);
        salvar_projeto_atual(a);
    } else if (argc > 2 && !strcmp(argv[1], "--jogar")) {
        somente_jogo = 1;
        if (!abrir_projeto_atual(a, argv[2])) {
            fprintf(stderr, "Mythara: %s\n", a->mensagem);
            liberar_projeto(&a->projeto);
            free(a);
            return 1;
        }
    } else if (argc > 2 && !strcmp(argv[1], "--recuperar")) {
        if (!abrir_projeto_atual(a, argv[2]) || !recuperar_autosave(a)) {
            fprintf(stderr, "Mythara: %s\n", a->mensagem);
            liberar_projeto(&a->projeto);
            free(a);
            return 1;
        }
        a->modal = MODAL_NENHUM;
    } else if (argc > 1 && argv[1][0] != '-') {
        if (!abrir_projeto_atual(a, argv[1]))
            fprintf(stderr, "Mythara: %s; usando projeto novo.\n", a->mensagem);
    } else if (argc == 1 && !strcmp(nome_base(argv[0]), "jogo")) {
        mudar_para_pasta_executavel();
        somente_jogo = 1;
        if (!abrir_projeto_atual(a, "jogo.myr")) {
            fprintf(stderr, "Mythara: %s\n", a->mensagem);
            liberar_projeto(&a->projeto);
            free(a);
            return 1;
        }
    }
    if (!somente_jogo && !resetar_layout)
        carregar_configuracao(a);
    const char *escala_ambiente = getenv("MYTHARA_ESCALA");
    if (!somente_jogo && escala_ambiente && escala_ambiente[0])
        tema_ativo.escala_percentual = limitar_int(atoi(escala_ambiente), 100, 200);
    if (!somente_jogo && argc == 1 && a->mostrar_boas_vindas && !a->oferecer_recuperacao)
        a->modal = MODAL_BOAS_VINDAS;
    if (somente_jogo) {
        memset(&a->jogo, 0, sizeof(a->jogo));
        iniciar_estado_jogo(&a->projeto, &a->jogo.estado);
        a->jogo.evento_ativo = -1;
        a->modo = MODO_JOGO;
    } else
        a->modo = MODO_EDITOR;
    Plataforma plataforma;
    Tela tela_editor = {0};
    if (!plataforma_iniciar(&plataforma)) {
        liberar_historico_global(&a->editor);
        liberar_recursos(a);
        liberar_projeto(&a->projeto);
        free(a);
        return 1;
    }
    while (plataforma.executando) {
        plataforma_eventos(&plataforma);
        if (a->modo == MODO_EDITOR) {
            int escala = limitar_int(((tema_ativo.escala_percentual + 12) / 25) * 25, 100, 200),
                limite_horizontal = plataforma.tela.largura * 100 / 640,
                limite_vertical = plataforma.tela.altura * 100 / 360;
            if (escala > limite_horizontal)
                escala = limite_horizontal;
            if (escala > limite_vertical)
                escala = limite_vertical;
            if (escala < 100)
                escala = 100;
            int lw = plataforma.tela.largura * 100 / escala,
                lh = plataforma.tela.altura * 100 / escala;
            if (!redimensionar_tela_memoria(&tela_editor, lw, lh)) {
                plataforma.executando = 0;
                break;
            }
            Entrada entrada_logica = plataforma.entrada;
            entrada_logica.mouse_x = plataforma.entrada.mouse_x * lw / plataforma.tela.largura;
            entrada_logica.mouse_y = plataforma.entrada.mouse_y * lh / plataforma.tela.altura;
            Interface ui = {&tela_editor, &entrada_logica, 0, 0, a->foco_interface, 0, NULL};
            limpar_tela(&tela_editor, tema_ativo.fundo);
            desenhar_editor(a, &ui);
            verificar_historico_global(a, &entrada_logica);
            realizar_autosave(a);
            a->foco_interface = ui.id_foco;
            copiar_tela_escalada(&tela_editor, &plataforma.tela);
        } else {
            Interface ui = {
                &plataforma.tela, &plataforma.entrada, 0, 0, a->foco_interface, 0, NULL};
            limpar_tela(&plataforma.tela, tema_ativo.fundo);
            if (a->modo == MODO_JOGO) {
                atualizar_jogo(a, &plataforma.entrada);
                desenhar_jogo(a, &ui);
            } else
                desenhar_batalha(a, &ui);
            a->foco_interface = ui.id_foco;
        }
        if (somente_jogo && a->modo == MODO_EDITOR)
            plataforma.executando = 0;
        plataforma_apresentar(&plataforma);
        pausar_milissegundos(16);
    }
    plataforma_encerrar(&plataforma);
    while (atomic_load(&a->autosave_em_andamento))
        pausar_milissegundos(10);
    free(tela_editor.pixels);
    if (!somente_jogo)
        salvar_configuracao(a);
    liberar_historico_global(&a->editor);
    liberar_recursos(a);
    liberar_projeto(&a->projeto);
    free(a);
    return 0;
}
