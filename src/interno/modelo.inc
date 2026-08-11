/* ========================================================================== */
/* Modelo persistente                                                          */
/* ========================================================================== */

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

static Identificador novo_identificador_projeto(void) {
    uint64_t valor = (uint64_t)time(NULL) ^ ((uint64_t)(unsigned long)getpid() << 32) ^
                     (uint64_t)(uintptr_t)&valor ^ (uint64_t)(agora_segundos() * 1000000000.0);
    valor ^= valor >> 30;
    valor *= UINT64_C(0xbf58476d1ce4e5b9);
    valor ^= valor >> 27;
    valor *= UINT64_C(0x94d049bb133111eb);
    valor ^= valor >> 31;
    return valor ? valor : UINT64_C(0x4d59544841524134);
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
    if (!p->id)
        p->id = novo_identificador_projeto();
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
    p->id = novo_identificador_projeto();
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
    p->herois[0].sprite = -1;
    copiar_texto(p->herois[1].nome, sizeof(p->herois[1].nome), "Nara");
    p->herois[1].vida_maxima = 24;
    p->herois[1].magia_maxima = 20;
    p->herois[1].ataque = 5;
    p->herois[1].defesa = 2;
    p->herois[1].poder_magico = 9;
    p->herois[1].resistencia = 5;
    p->herois[1].velocidade = 8;
    p->herois[1].sorte = 6;
    p->herois[1].sprite = -1;
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
    for (int nivel = 0; nivel < 32; ++nivel)
        p->classes[0].habilidade_por_nivel[nivel] = -1;
    p->quantidade_habilidades = 1;
    copiar_texto(p->habilidades[0].nome, sizeof(p->habilidades[0].nome), "Luz");
    copiar_texto(p->habilidades[0].descricao, sizeof(p->habilidades[0].descricao),
                 "Dano magico leve em um inimigo.");
    p->habilidades[0].custo_mp = 3;
    p->habilidades[0].poder = 9;
    p->habilidades[0].estado = -1;
    p->habilidades[0].animacao = -1;
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
    p->missoes[0].recompensa_item = -1;
    atribuir_ids_ausentes(p);
}

static void iniciar_estado_jogo(const Projeto *p, EstadoJogo *j) {
    memset(j, 0, sizeof(*j));
    for (int i = 0; i < 4; ++i)
        j->equipamentos[i] = -1;
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
