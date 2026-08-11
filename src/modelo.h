#ifndef MYTHARA_MODELO_H
#define MYTHARA_MODELO_H

#include <stdint.h>

#define MYTHARA_VERSAO 4u
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
#define MYTHARA_MAX_TEXTOS 512
#define MYTHARA_MAX_FLAGS 512
#define MYTHARA_MAX_VARIAVEIS 512
#define MYTHARA_MAX_CAMINHO 1024

typedef uint64_t Identificador;

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
    Identificador id;
    Identificador proximo_id;
    char nome[64];
    char autor[64];
    int tamanho_tile;
    int batalha_lateral;
    int idioma_interface;
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

#endif
