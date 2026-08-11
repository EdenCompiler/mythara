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

static int salvar_projeto_v3_em(const Projeto *p, const char *caminho, char *erro,
                                size_t cap_erro) {
    BufferDados b = {0};
    if (!serializar_projeto(p, &b)) {
        free(b.dados);
        snprintf(erro, cap_erro, "Memoria insuficiente para serializar o projeto.");
        return 0;
    }
    char temporario[MYTHARA_MAX_CAMINHO + 16];
    CabecalhoProjeto h = {{'M', 'Y', 'T', 'H', 'R', 'V', '3', '\0'}, 3, 1};
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
    if (!ok || !substituir_arquivo(temporario, caminho)) {
        unlink(temporario);
        snprintf(erro, cap_erro, "Falha ao concluir o arquivo do projeto.");
        return 0;
    }
    return 1;
}

static int comparar_identificadores(const void *a, const void *b) {
    Identificador x = *(const Identificador *)a, y = *(const Identificador *)b;
    return x < y ? -1 : x > y;
}

static int referencia_indice_valida(int indice, int quantidade, int opcional) {
    if (indice >= 0 && indice < quantidade)
        return 1;
    return opcional && (indice == -1 || (!quantidade && indice == 0));
}

static int texto_terminado(const char *texto, size_t capacidade) {
    return memchr(texto, 0, capacidade) != NULL && texto_utf8_valido(texto);
}

static int validar_evento(const Projeto *p, const Evento *evento, char *erro, size_t cap_erro) {
    int pilha_tipo[32] = {0}, pilha_senao[32] = {0}, topo = 0;
    if (!texto_terminado(evento->nome, sizeof(evento->nome)) || evento->quantidade_comandos < 0 ||
        evento->quantidade_comandos > MYTHARA_MAX_COMANDOS ||
        (evento->quantidade_comandos && !evento->comandos))
        goto estrutura;
    for (int i = 0; i < evento->quantidade_comandos; ++i) {
        const ComandoEvento *c = &evento->comandos[i];
        if (c->tipo < COMANDO_TEXTO || c->tipo > COMANDO_MISSAO || c->profundidade < 0 ||
            c->profundidade > 31 || !texto_terminado(c->texto, sizeof(c->texto)))
            goto estrutura;
        if (c->tipo == COMANDO_SE || c->tipo == COMANDO_REPETIR) {
            if (c->profundidade != topo || topo >= 32)
                goto blocos;
            pilha_tipo[topo] = c->tipo;
            pilha_senao[topo] = 0;
            topo++;
        } else if (c->tipo == COMANDO_SENAO) {
            if (!topo || pilha_tipo[topo - 1] != COMANDO_SE || pilha_senao[topo - 1] ||
                c->profundidade != topo - 1)
                goto blocos;
            pilha_senao[topo - 1] = 1;
        } else if (c->tipo == COMANDO_FIM_BLOCO) {
            if (!topo || c->profundidade != topo - 1)
                goto blocos;
            topo--;
        } else if (c->profundidade != topo)
            goto blocos;
        switch (c->tipo) {
        case COMANDO_ITEM:
            if (!referencia_indice_valida(c->a, p->quantidade_itens, 0))
                goto referencia;
            break;
        case COMANDO_TELEPORTE:
            if (!referencia_indice_valida(c->a, p->quantidade_mapas, 0))
                goto referencia;
            break;
        case COMANDO_BATALHA:
            if (!referencia_indice_valida(c->a, p->quantidade_inimigos, 0))
                goto referencia;
            break;
        case COMANDO_AUDIO:
            if (!referencia_indice_valida(c->a, p->quantidade_recursos, 0))
                goto referencia;
            break;
        case COMANDO_CHAMAR_EVENTO:
            if (!referencia_indice_valida(c->a, p->quantidade_eventos, 0))
                goto referencia;
            break;
        case COMANDO_LOJA:
            if (!referencia_indice_valida(c->a, p->quantidade_lojas, 0))
                goto referencia;
            break;
        case COMANDO_MISSAO:
            if (!referencia_indice_valida(c->a, p->quantidade_missoes, 0))
                goto referencia;
            break;
        default:
            break;
        }
    }
    if (topo)
        goto blocos;
    return 1;
estrutura:
    snprintf(erro, cap_erro, "Evento %.48s contem dados invalidos.", evento->nome);
    return 0;
blocos:
    snprintf(erro, cap_erro, "Evento %.48s contem blocos SE/REPETIR malformados.", evento->nome);
    return 0;
referencia:
    snprintf(erro, cap_erro, "Evento %.48s contem uma referencia ausente.", evento->nome);
    return 0;
}

static int validar_projeto(Projeto *p, char *erro, size_t cap_erro) {
#define INVALIDAR(...)                                                                             \
    do {                                                                                           \
        snprintf(erro, cap_erro, __VA_ARGS__);                                                     \
        goto invalido;                                                                             \
    } while (0)
    Identificador *ids = NULL;
    size_t quantidade_ids = 0,
           capacidade_ids = 1u + (size_t)p->quantidade_mapas + (size_t)p->quantidade_eventos +
                            (size_t)p->quantidade_herois + (size_t)p->quantidade_classes +
                            (size_t)p->quantidade_habilidades + (size_t)p->quantidade_itens +
                            (size_t)p->quantidade_inimigos + (size_t)p->quantidade_estados +
                            (size_t)p->quantidade_lojas + (size_t)p->quantidade_missoes +
                            (size_t)p->quantidade_recursos;
    if (p->quantidade_mapas < 1 || p->quantidade_mapas > MYTHARA_MAX_MAPAS ||
        p->quantidade_eventos < 0 || p->quantidade_eventos > MYTHARA_MAX_EVENTOS ||
        p->quantidade_herois < 1 || p->quantidade_herois > 16 || p->quantidade_classes < 0 ||
        p->quantidade_classes > 32 || p->quantidade_habilidades < 0 ||
        p->quantidade_habilidades > 128 || p->quantidade_itens < 0 ||
        p->quantidade_itens > MYTHARA_MAX_ITENS || p->quantidade_inimigos < 0 ||
        p->quantidade_inimigos > MYTHARA_MAX_INIMIGOS || p->quantidade_estados < 0 ||
        p->quantidade_estados > 64 || p->quantidade_lojas < 0 || p->quantidade_lojas > 64 ||
        p->quantidade_missoes < 0 || p->quantidade_missoes > 128 || p->quantidade_recursos < 0 ||
        p->quantidade_recursos > MYTHARA_MAX_RECURSOS)
        INVALIDAR("O projeto contem quantidades fora dos limites v4.");
    if (!texto_terminado(p->nome, sizeof(p->nome)) ||
        !texto_terminado(p->autor, sizeof(p->autor)) || !p->id)
        INVALIDAR("Metadados ou identificador do projeto invalidos.");
    if (!referencia_indice_valida(p->mapa_inicial, p->quantidade_mapas, 0))
        INVALIDAR("Mapa inicial ausente.");
    for (int i = 0; i < p->quantidade_mapas; ++i) {
        Mapa *m = &p->mapas[i];
        if (m->quantidade_camadas > 0)
            capacidade_ids += (size_t)m->quantidade_camadas;
        if (m->quantidade_entidades > 0)
            capacidade_ids += (size_t)m->quantidade_entidades;
    }
    ids = malloc(capacidade_ids * sizeof(*ids));
    if (!ids)
        INVALIDAR("Memoria insuficiente para validar IDs.");
#define ADICIONAR_ID(valor)                                                                        \
    do {                                                                                           \
        if (!(valor) || quantidade_ids >= capacidade_ids)                                          \
            INVALIDAR("Objeto persistente sem ID valido.");                                        \
        ids[quantidade_ids++] = (valor);                                                           \
    } while (0)
    for (int i = 0; i < p->quantidade_mapas; ++i) {
        Mapa *m = &p->mapas[i];
        if (m->largura < 4 || m->largura > MYTHARA_MAX_LARGURA_MAPA || m->altura < 4 ||
            m->altura > MYTHARA_MAX_ALTURA_MAPA || m->quantidade_camadas < 1 ||
            m->quantidade_camadas > MYTHARA_MAX_CAMADAS || !m->colisoes ||
            m->quantidade_entidades < 0 || m->quantidade_entidades > MYTHARA_MAX_ENTIDADES ||
            !texto_terminado(m->nome, sizeof(m->nome)) ||
            !referencia_indice_valida(m->regiao_encontro, p->quantidade_inimigos, 1))
            INVALIDAR("Mapa %d contem dimensoes, memoria ou referencias invalidas.", i + 1);
        ADICIONAR_ID(m->id);
        for (int c = 0; c < m->quantidade_camadas; ++c) {
            if (!m->camadas[c].tiles ||
                !texto_terminado(m->camadas[c].nome, sizeof(m->camadas[c].nome)))
                INVALIDAR("Mapa %.48s contem uma camada invalida.", m->nome);
            ADICIONAR_ID(m->camadas[c].id);
        }
        for (int n = 0; n < m->quantidade_entidades; ++n) {
            Entidade *e = &m->entidades[n];
            if (!texto_terminado(e->nome, sizeof(e->nome)) || e->x < 0 || e->x >= m->largura ||
                e->y < 0 || e->y >= m->altura || e->tipo < TIPO_ENTIDADE_NPC ||
                e->tipo > TIPO_ENTIDADE_INIMIGO ||
                !referencia_indice_valida(e->evento, p->quantidade_eventos, 1))
                INVALIDAR("Mapa %.48s contem uma entidade invalida.", m->nome);
            ADICIONAR_ID(e->id);
        }
    }
    Mapa *inicial = &p->mapas[p->mapa_inicial];
    if (p->inicio_x < 0 || p->inicio_x >= inicial->largura || p->inicio_y < 0 ||
        p->inicio_y >= inicial->altura)
        INVALIDAR("Posicao inicial fora do mapa inicial.");
    for (int i = 0; i < p->quantidade_eventos; ++i) {
        ADICIONAR_ID(p->eventos[i].id);
        if (!validar_evento(p, &p->eventos[i], erro, cap_erro))
            goto invalido;
    }
    for (int i = 0; i < p->quantidade_herois; ++i) {
        Heroi *h = &p->herois[i];
        ADICIONAR_ID(h->id);
        if (!texto_terminado(h->nome, sizeof(h->nome)) ||
            !referencia_indice_valida(h->classe, p->quantidade_classes, 1) ||
            !referencia_indice_valida(h->sprite, p->quantidade_recursos, 1))
            INVALIDAR("Heroi %d contem uma referencia invalida.", i + 1);
    }
    for (int i = 0; i < p->quantidade_classes; ++i) {
        ADICIONAR_ID(p->classes[i].id);
        if (!texto_terminado(p->classes[i].nome, sizeof(p->classes[i].nome)))
            INVALIDAR("Classe %d possui nome invalido.", i + 1);
        for (int nivel = 0; nivel < 32; ++nivel)
            if (!referencia_indice_valida(p->classes[i].habilidade_por_nivel[nivel],
                                          p->quantidade_habilidades, 1))
                INVALIDAR("Classe %.48s referencia uma habilidade ausente.", p->classes[i].nome);
    }
    for (int i = 0; i < p->quantidade_habilidades; ++i) {
        Habilidade *h = &p->habilidades[i];
        ADICIONAR_ID(h->id);
        if (!texto_terminado(h->nome, sizeof(h->nome)) ||
            !texto_terminado(h->descricao, sizeof(h->descricao)) ||
            !referencia_indice_valida(h->estado, p->quantidade_estados, 1) ||
            !referencia_indice_valida(h->animacao, p->quantidade_recursos, 1))
            INVALIDAR("Habilidade %d contem uma referencia invalida.", i + 1);
    }
    for (int i = 0; i < p->quantidade_itens; ++i) {
        ADICIONAR_ID(p->itens[i].id);
        if (!texto_terminado(p->itens[i].nome, sizeof(p->itens[i].nome)) ||
            !texto_terminado(p->itens[i].descricao, sizeof(p->itens[i].descricao)))
            INVALIDAR("Item %d contem texto invalido.", i + 1);
    }
    for (int i = 0; i < p->quantidade_inimigos; ++i) {
        ADICIONAR_ID(p->inimigos[i].id);
        if (!texto_terminado(p->inimigos[i].nome, sizeof(p->inimigos[i].nome)))
            INVALIDAR("Inimigo %d contem nome invalido.", i + 1);
    }
    for (int i = 0; i < p->quantidade_estados; ++i) {
        ADICIONAR_ID(p->estados[i].id);
        if (!texto_terminado(p->estados[i].nome, sizeof(p->estados[i].nome)))
            INVALIDAR("Estado %d contem nome invalido.", i + 1);
    }
    for (int i = 0; i < p->quantidade_lojas; ++i) {
        Loja *loja = &p->lojas[i];
        ADICIONAR_ID(loja->id);
        if (!texto_terminado(loja->nome, sizeof(loja->nome)) || loja->quantidade_itens < 0 ||
            loja->quantidade_itens > 32)
            INVALIDAR("Loja %d contem estrutura invalida.", i + 1);
        for (int item = 0; item < loja->quantidade_itens; ++item)
            if (!referencia_indice_valida(loja->itens[item], p->quantidade_itens, 0))
                INVALIDAR("Loja %.48s referencia um item ausente.", loja->nome);
    }
    for (int i = 0; i < p->quantidade_missoes; ++i) {
        Missao *missao = &p->missoes[i];
        ADICIONAR_ID(missao->id);
        if (!texto_terminado(missao->nome, sizeof(missao->nome)) ||
            !texto_terminado(missao->descricao, sizeof(missao->descricao)) ||
            missao->quantidade_etapas < 0 || missao->quantidade_etapas > 16 ||
            !referencia_indice_valida(missao->recompensa_item, p->quantidade_itens, 1))
            INVALIDAR("Missao %d contem estrutura invalida.", i + 1);
        for (int etapa = 0; etapa < missao->quantidade_etapas; ++etapa)
            if (!texto_terminado(missao->etapas[etapa].descricao,
                                 sizeof(missao->etapas[etapa].descricao)) ||
                (missao->etapas[etapa].tipo == 1 &&
                 !referencia_indice_valida(missao->etapas[etapa].alvo, p->quantidade_inimigos, 0)))
                INVALIDAR("Missao %.48s contem uma etapa invalida.", missao->nome);
    }
    for (int i = 0; i < p->quantidade_recursos; ++i) {
        Recurso *r = &p->recursos[i];
        ADICIONAR_ID(r->id);
        if (!texto_terminado(r->nome, sizeof(r->nome)) ||
            !texto_terminado(r->caminho, sizeof(r->caminho)) || r->tipo < RECURSO_BMP ||
            r->tipo > RECURSO_TGA || !caminho_relativo_seguro(r->caminho) ||
            r->quantidade_clipes < 0 || r->quantidade_clipes > 16)
            INVALIDAR("Recurso %d possui caminho ou estrutura insegura.", i + 1);
        if (r->tipo != RECURSO_WAV && (r->largura_quadro < 1 || r->altura_quadro < 1 ||
                                       r->quantidade_quadros < 1 || r->duracao_quadro_ms < 1))
            INVALIDAR("Recurso %.48s possui quadros invalidos.", r->nome);
        for (int clipe = 0; clipe < r->quantidade_clipes; ++clipe) {
            ClipeAnimacao *c = &r->clipes[clipe];
            if (!texto_terminado(c->nome, sizeof(c->nome)) || c->primeiro_quadro < 0 ||
                c->quantidade_quadros < 1 || c->duracao_ms < 1 || c->direcoes < 0 ||
                c->direcoes > 8 ||
                (r->tipo != RECURSO_WAV &&
                 c->primeiro_quadro + c->quantidade_quadros > r->quantidade_quadros))
                INVALIDAR("Recurso %.48s possui um clipe invalido.", r->nome);
        }
    }
    qsort(ids, quantidade_ids, sizeof(*ids), comparar_identificadores);
    for (size_t i = 1; i < quantidade_ids; ++i)
        if (ids[i - 1] == ids[i])
            INVALIDAR("O projeto contem IDs persistentes duplicados.");
    if (p->proximo_id <= ids[quantidade_ids - 1])
        p->proximo_id = ids[quantidade_ids - 1] + 1;
    free(ids);
#undef ADICIONAR_ID
#undef INVALIDAR
    return 1;
invalido:
    free(ids);
#undef ADICIONAR_ID
#undef INVALIDAR
    return 0;
}

static int avisos_projeto(const Projeto *p, char *primeiro, size_t capacidade) {
    int avisos = 0;
    if (primeiro && capacidade)
        primeiro[0] = 0;
    for (int i = 0; i < p->quantidade_recursos; ++i) {
        char caminho[MYTHARA_MAX_CAMINHO];
        caminho_do_recurso(p, p->recursos[i].caminho, caminho, sizeof(caminho));
        if (p->recursos[i].ativo && access(caminho, F_OK) != 0) {
            if (!avisos && primeiro)
                snprintf(primeiro, capacidade, "Recurso ausente no disco: %.120s",
                         p->recursos[i].nome);
            avisos++;
        }
    }
    if (!p->quantidade_eventos) {
        if (!avisos && primeiro)
            copiar_texto(primeiro, capacidade, "O projeto ainda nao possui eventos.");
        avisos++;
    }
    return avisos;
}

static int carregar_projeto_v3_de(Projeto *p, const char *caminho, char *erro, size_t cap_erro) {
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
    int ok = fread(&h, sizeof(h), 1, f) == 1 && !memcmp(h.magia, "MYTHRV3", 7) && h.versao == 3 &&
             h.quantidade_blocos == 1;
    if (!ok) {
        fclose(f);
        snprintf(erro, cap_erro, "Arquivo incompativel com o importador legado v3.");
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

/* O formato v4 nunca grava estruturas C diretamente. Todos os campos usam
 * inteiros little-endian de tamanho fixo e textos UTF-8 prefixados pelo tamanho. */

static Identificador id_por_indice(const void *dados, int quantidade, size_t tamanho, int indice) {
    if (!dados || indice < 0 || indice >= quantidade)
        return 0;
    const uint8_t *registro = (const uint8_t *)dados + (size_t)indice * tamanho;
    Identificador id;
    memcpy(&id, registro, sizeof(id));
    return id;
}

static int indice_por_id(const void *dados, int quantidade, size_t tamanho, Identificador id) {
    if (!id)
        return -1;
    for (int i = 0; i < quantidade; ++i) {
        const uint8_t *registro = (const uint8_t *)dados + (size_t)i * tamanho;
        Identificador atual;
        memcpy(&atual, registro, sizeof(atual));
        if (atual == id)
            return i;
    }
    return -1;
}

static Identificador referencia_comando(const Projeto *p, const ComandoEvento *c) {
    switch (c->tipo) {
    case COMANDO_ITEM:
        return id_por_indice(p->itens, p->quantidade_itens, sizeof(Item), c->a);
    case COMANDO_TELEPORTE:
        return id_por_indice(p->mapas, p->quantidade_mapas, sizeof(Mapa), c->a);
    case COMANDO_BATALHA:
        return id_por_indice(p->inimigos, p->quantidade_inimigos, sizeof(Inimigo), c->a);
    case COMANDO_AUDIO:
        return id_por_indice(p->recursos, p->quantidade_recursos, sizeof(Recurso), c->a);
    case COMANDO_CHAMAR_EVENTO:
        return id_por_indice(p->eventos, p->quantidade_eventos, sizeof(Evento), c->a);
    case COMANDO_LOJA:
        return id_por_indice(p->lojas, p->quantidade_lojas, sizeof(Loja), c->a);
    case COMANDO_MISSAO:
        return id_por_indice(p->missoes, p->quantidade_missoes, sizeof(Missao), c->a);
    default:
        return 0;
    }
}

static int resolver_referencia_comando(const Projeto *p, ComandoEvento *c, Identificador id) {
    if (!id)
        return 1;
    int indice = -1;
    switch (c->tipo) {
    case COMANDO_ITEM:
        indice = indice_por_id(p->itens, p->quantidade_itens, sizeof(Item), id);
        break;
    case COMANDO_TELEPORTE:
        indice = indice_por_id(p->mapas, p->quantidade_mapas, sizeof(Mapa), id);
        break;
    case COMANDO_BATALHA:
        indice = indice_por_id(p->inimigos, p->quantidade_inimigos, sizeof(Inimigo), id);
        break;
    case COMANDO_AUDIO:
        indice = indice_por_id(p->recursos, p->quantidade_recursos, sizeof(Recurso), id);
        break;
    case COMANDO_CHAMAR_EVENTO:
        indice = indice_por_id(p->eventos, p->quantidade_eventos, sizeof(Evento), id);
        break;
    case COMANDO_LOJA:
        indice = indice_por_id(p->lojas, p->quantidade_lojas, sizeof(Loja), id);
        break;
    case COMANDO_MISSAO:
        indice = indice_por_id(p->missoes, p->quantidade_missoes, sizeof(Missao), id);
        break;
    default:
        return 1;
    }
    if (indice < 0)
        return 0;
    c->a = indice;
    return 1;
}

static void v4_escrever_recurso(EscritorBinario *w, const Recurso *r) {
    escritor_binario_u64(w, r->id);
    escritor_binario_i32(w, r->ativo);
    escritor_binario_i32(w, r->tipo);
    escritor_binario_texto(w, r->nome);
    escritor_binario_texto(w, r->caminho);
    escritor_binario_i32(w, r->largura_quadro);
    escritor_binario_i32(w, r->altura_quadro);
    escritor_binario_i32(w, r->quantidade_quadros);
    escritor_binario_i32(w, r->duracao_quadro_ms);
    escritor_binario_u32(w, (uint32_t)r->quantidade_clipes);
    for (int i = 0; i < r->quantidade_clipes; ++i) {
        const ClipeAnimacao *c = &r->clipes[i];
        escritor_binario_texto(w, c->nome);
        escritor_binario_i32(w, c->primeiro_quadro);
        escritor_binario_i32(w, c->quantidade_quadros);
        escritor_binario_i32(w, c->duracao_ms);
        escritor_binario_i32(w, c->repetir);
        escritor_binario_i32(w, c->direcoes);
    }
}

static void v4_escrever_item(EscritorBinario *w, const Item *item) {
    escritor_binario_u64(w, item->id);
    escritor_binario_texto(w, item->nome);
    escritor_binario_texto(w, item->descricao);
    escritor_binario_i32(w, item->valor);
    escritor_binario_i32(w, item->cura);
    escritor_binario_i32(w, item->tipo_equipamento);
    escritor_binario_i32(w, item->ataque);
    escritor_binario_i32(w, item->defesa);
    escritor_binario_i32(w, item->magia);
    escritor_binario_i32(w, item->resistencia);
    escritor_binario_i32(w, item->velocidade);
}

static void v4_escrever_inimigo(EscritorBinario *w, const Inimigo *inimigo) {
    escritor_binario_u64(w, inimigo->id);
    escritor_binario_texto(w, inimigo->nome);
    escritor_binario_i32(w, inimigo->vida);
    escritor_binario_i32(w, inimigo->magia);
    escritor_binario_i32(w, inimigo->ataque);
    escritor_binario_i32(w, inimigo->defesa);
    escritor_binario_i32(w, inimigo->poder_magico);
    escritor_binario_i32(w, inimigo->resistencia);
    escritor_binario_i32(w, inimigo->velocidade);
    escritor_binario_i32(w, inimigo->sorte);
    escritor_binario_i32(w, inimigo->experiencia);
    escritor_binario_i32(w, inimigo->ouro);
}

static void v4_escrever_estado(EscritorBinario *w, const EstadoCombate *estado) {
    escritor_binario_u64(w, estado->id);
    escritor_binario_texto(w, estado->nome);
    escritor_binario_i32(w, estado->duracao);
    escritor_binario_i32(w, estado->dano_turno);
    escritor_binario_i32(w, estado->ataque_pct);
    escritor_binario_i32(w, estado->defesa_pct);
    escritor_binario_i32(w, estado->velocidade_pct);
}

static void v4_escrever_habilidade(EscritorBinario *w, const Projeto *p,
                                   const Habilidade *habilidade) {
    escritor_binario_u64(w, habilidade->id);
    escritor_binario_texto(w, habilidade->nome);
    escritor_binario_texto(w, habilidade->descricao);
    escritor_binario_i32(w, habilidade->custo_mp);
    escritor_binario_i32(w, habilidade->poder);
    escritor_binario_i32(w, habilidade->elemento);
    escritor_binario_i32(w, habilidade->alvo);
    escritor_binario_u64(w, id_por_indice(p->estados, p->quantidade_estados, sizeof(EstadoCombate),
                                          habilidade->estado));
    escritor_binario_u64(w, id_por_indice(p->recursos, p->quantidade_recursos, sizeof(Recurso),
                                          habilidade->animacao));
}

static void v4_escrever_classe(EscritorBinario *w, const Projeto *p, const Classe *classe) {
    escritor_binario_u64(w, classe->id);
    escritor_binario_texto(w, classe->nome);
    escritor_binario_i32(w, classe->vida_base);
    escritor_binario_i32(w, classe->magia_base);
    escritor_binario_i32(w, classe->ataque_base);
    escritor_binario_i32(w, classe->defesa_base);
    escritor_binario_i32(w, classe->poder_magico_base);
    escritor_binario_i32(w, classe->resistencia_base);
    escritor_binario_i32(w, classe->velocidade_base);
    escritor_binario_i32(w, classe->sorte_base);
    for (int i = 0; i < 32; ++i)
        escritor_binario_u64(w, id_por_indice(p->habilidades, p->quantidade_habilidades,
                                              sizeof(Habilidade), classe->habilidade_por_nivel[i]));
}

static void v4_escrever_heroi(EscritorBinario *w, const Projeto *p, const Heroi *heroi) {
    escritor_binario_u64(w, heroi->id);
    escritor_binario_texto(w, heroi->nome);
    escritor_binario_u64(
        w, id_por_indice(p->classes, p->quantidade_classes, sizeof(Classe), heroi->classe));
    escritor_binario_i32(w, heroi->vida_maxima);
    escritor_binario_i32(w, heroi->magia_maxima);
    escritor_binario_i32(w, heroi->ataque);
    escritor_binario_i32(w, heroi->defesa);
    escritor_binario_i32(w, heroi->poder_magico);
    escritor_binario_i32(w, heroi->resistencia);
    escritor_binario_i32(w, heroi->velocidade);
    escritor_binario_i32(w, heroi->sorte);
    escritor_binario_u64(
        w, id_por_indice(p->recursos, p->quantidade_recursos, sizeof(Recurso), heroi->sprite));
}

static void v4_escrever_loja(EscritorBinario *w, const Projeto *p, const Loja *loja) {
    escritor_binario_u64(w, loja->id);
    escritor_binario_texto(w, loja->nome);
    escritor_binario_u32(w, (uint32_t)loja->quantidade_itens);
    for (int i = 0; i < loja->quantidade_itens; ++i) {
        escritor_binario_u64(
            w, id_por_indice(p->itens, p->quantidade_itens, sizeof(Item), loja->itens[i]));
        escritor_binario_i32(w, loja->precos[i]);
    }
}

static void v4_escrever_missao(EscritorBinario *w, const Projeto *p, const Missao *missao) {
    escritor_binario_u64(w, missao->id);
    escritor_binario_texto(w, missao->nome);
    escritor_binario_texto(w, missao->descricao);
    escritor_binario_u32(w, (uint32_t)missao->quantidade_etapas);
    for (int i = 0; i < missao->quantidade_etapas; ++i) {
        const EtapaMissao *etapa = &missao->etapas[i];
        escritor_binario_texto(w, etapa->descricao);
        escritor_binario_i32(w, etapa->tipo);
        escritor_binario_i32(w, etapa->alvo);
        Identificador alvo = etapa->tipo == 1 ? id_por_indice(p->inimigos, p->quantidade_inimigos,
                                                              sizeof(Inimigo), etapa->alvo)
                                              : 0;
        escritor_binario_u64(w, alvo);
        escritor_binario_i32(w, etapa->quantidade);
    }
    escritor_binario_u64(
        w, id_por_indice(p->itens, p->quantidade_itens, sizeof(Item), missao->recompensa_item));
    escritor_binario_i32(w, missao->recompensa_quantidade);
    escritor_binario_i32(w, missao->recompensa_ouro);
    escritor_binario_i32(w, missao->recompensa_experiencia);
}

static void v4_escrever_evento(EscritorBinario *w, const Projeto *p, const Evento *evento) {
    escritor_binario_u64(w, evento->id);
    escritor_binario_i32(w, evento->ativo);
    escritor_binario_texto(w, evento->nome);
    escritor_binario_i32(w, evento->condicao_flag);
    escritor_binario_i32(w, evento->condicao_valor);
    escritor_binario_u32(w, (uint32_t)evento->quantidade_comandos);
    for (int i = 0; i < evento->quantidade_comandos; ++i) {
        const ComandoEvento *c = &evento->comandos[i];
        escritor_binario_i32(w, c->tipo);
        escritor_binario_i32(w, c->a);
        escritor_binario_i32(w, c->b);
        escritor_binario_i32(w, c->c);
        escritor_binario_i32(w, c->profundidade);
        escritor_binario_texto(w, c->texto);
        escritor_binario_u64(w, referencia_comando(p, c));
    }
}

static void v4_escrever_mapa(EscritorBinario *w, const Projeto *p, const Mapa *mapa) {
    escritor_binario_u64(w, mapa->id);
    escritor_binario_texto(w, mapa->nome);
    escritor_binario_i32(w, mapa->largura);
    escritor_binario_i32(w, mapa->altura);
    escritor_binario_u32(w, (uint32_t)mapa->quantidade_camadas);
    escritor_binario_u32(w, (uint32_t)mapa->quantidade_entidades);
    escritor_binario_u64(w, id_por_indice(p->inimigos, p->quantidade_inimigos, sizeof(Inimigo),
                                          mapa->regiao_encontro));
    size_t total = (size_t)mapa->largura * (size_t)mapa->altura;
    for (int c = 0; c < mapa->quantidade_camadas; ++c) {
        const CamadaMapa *camada = &mapa->camadas[c];
        escritor_binario_u64(w, camada->id);
        escritor_binario_texto(w, camada->nome);
        escritor_binario_i32(w, camada->visivel);
        escritor_binario_i32(w, camada->bloqueada);
        for (size_t i = 0; i < total; ++i)
            escritor_binario_u16(w, camada->tiles[i]);
    }
    escritor_binario_bytes(w, mapa->colisoes, total);
    for (int i = 0; i < mapa->quantidade_entidades; ++i) {
        const Entidade *entidade = &mapa->entidades[i];
        escritor_binario_u64(w, entidade->id);
        escritor_binario_i32(w, entidade->ativo);
        escritor_binario_texto(w, entidade->nome);
        escritor_binario_i32(w, entidade->tipo);
        escritor_binario_i32(w, entidade->x);
        escritor_binario_i32(w, entidade->y);
        escritor_binario_u64(w, entidade->evento_id
                                    ? entidade->evento_id
                                    : id_por_indice(p->eventos, p->quantidade_eventos,
                                                    sizeof(Evento), entidade->evento));
        escritor_binario_i32(w, entidade->cor);
    }
}

static int serializar_projeto_v4(const Projeto *p, EscritorBinario *w) {
    escritor_binario_u64(w, p->id);
    escritor_binario_u64(w, p->proximo_id);
    escritor_binario_texto(w, p->nome);
    escritor_binario_texto(w, p->autor);
    escritor_binario_i32(w, p->tamanho_tile);
    escritor_binario_i32(w, p->batalha_lateral);
    escritor_binario_i32(w, p->idioma_interface);
    escritor_binario_i32(w, p->inicio_x);
    escritor_binario_i32(w, p->inicio_y);

#define ESCREVER_LISTA(quantidade, dados, funcao)                                                  \
    do {                                                                                           \
        escritor_binario_u32(w, (uint32_t)(quantidade));                                           \
        for (int indice_v4 = 0; indice_v4 < (quantidade); ++indice_v4)                             \
            funcao(w, &(dados)[indice_v4]);                                                        \
    } while (0)
    ESCREVER_LISTA(p->quantidade_recursos, p->recursos, v4_escrever_recurso);
    ESCREVER_LISTA(p->quantidade_itens, p->itens, v4_escrever_item);
    ESCREVER_LISTA(p->quantidade_inimigos, p->inimigos, v4_escrever_inimigo);
    ESCREVER_LISTA(p->quantidade_estados, p->estados, v4_escrever_estado);
#undef ESCREVER_LISTA

    escritor_binario_u32(w, (uint32_t)p->quantidade_habilidades);
    for (int i = 0; i < p->quantidade_habilidades; ++i)
        v4_escrever_habilidade(w, p, &p->habilidades[i]);
    escritor_binario_u32(w, (uint32_t)p->quantidade_classes);
    for (int i = 0; i < p->quantidade_classes; ++i)
        v4_escrever_classe(w, p, &p->classes[i]);
    escritor_binario_u32(w, (uint32_t)p->quantidade_herois);
    for (int i = 0; i < p->quantidade_herois; ++i)
        v4_escrever_heroi(w, p, &p->herois[i]);
    escritor_binario_u32(w, (uint32_t)p->quantidade_lojas);
    for (int i = 0; i < p->quantidade_lojas; ++i)
        v4_escrever_loja(w, p, &p->lojas[i]);
    escritor_binario_u32(w, (uint32_t)p->quantidade_missoes);
    for (int i = 0; i < p->quantidade_missoes; ++i)
        v4_escrever_missao(w, p, &p->missoes[i]);
    escritor_binario_u32(w, (uint32_t)p->quantidade_eventos);
    for (int i = 0; i < p->quantidade_eventos; ++i)
        v4_escrever_evento(w, p, &p->eventos[i]);
    escritor_binario_u32(w, (uint32_t)p->quantidade_mapas);
    for (int i = 0; i < p->quantidade_mapas; ++i)
        v4_escrever_mapa(w, p, &p->mapas[i]);
    escritor_binario_u64(
        w, id_por_indice(p->mapas, p->quantidade_mapas, sizeof(Mapa), p->mapa_inicial));
    return !w->erro;
}

static int v4_ler_contagem(LeitorBinario *r, int maximo, int *quantidade) {
    uint32_t valor = leitor_binario_u32(r);
    if (r->erro || valor > (uint32_t)maximo) {
        r->erro = 1;
        return 0;
    }
    *quantidade = (int)valor;
    return 1;
}

static int v4_ler_recurso(LeitorBinario *r, Recurso *recurso) {
    recurso->id = leitor_binario_u64(r);
    recurso->ativo = leitor_binario_i32(r);
    recurso->tipo = leitor_binario_i32(r);
    leitor_binario_texto(r, recurso->nome, sizeof(recurso->nome));
    leitor_binario_texto(r, recurso->caminho, sizeof(recurso->caminho));
    recurso->largura_quadro = leitor_binario_i32(r);
    recurso->altura_quadro = leitor_binario_i32(r);
    recurso->quantidade_quadros = leitor_binario_i32(r);
    recurso->duracao_quadro_ms = leitor_binario_i32(r);
    if (!v4_ler_contagem(r, 16, &recurso->quantidade_clipes))
        return 0;
    for (int i = 0; i < recurso->quantidade_clipes; ++i) {
        ClipeAnimacao *c = &recurso->clipes[i];
        leitor_binario_texto(r, c->nome, sizeof(c->nome));
        c->primeiro_quadro = leitor_binario_i32(r);
        c->quantidade_quadros = leitor_binario_i32(r);
        c->duracao_ms = leitor_binario_i32(r);
        c->repetir = leitor_binario_i32(r);
        c->direcoes = leitor_binario_i32(r);
    }
    return !r->erro;
}

static int v4_ler_item(LeitorBinario *r, Item *item) {
    item->id = leitor_binario_u64(r);
    leitor_binario_texto(r, item->nome, sizeof(item->nome));
    leitor_binario_texto(r, item->descricao, sizeof(item->descricao));
    item->valor = leitor_binario_i32(r);
    item->cura = leitor_binario_i32(r);
    item->tipo_equipamento = leitor_binario_i32(r);
    item->ataque = leitor_binario_i32(r);
    item->defesa = leitor_binario_i32(r);
    item->magia = leitor_binario_i32(r);
    item->resistencia = leitor_binario_i32(r);
    item->velocidade = leitor_binario_i32(r);
    return !r->erro;
}

static int v4_ler_inimigo(LeitorBinario *r, Inimigo *inimigo) {
    inimigo->id = leitor_binario_u64(r);
    leitor_binario_texto(r, inimigo->nome, sizeof(inimigo->nome));
    inimigo->vida = leitor_binario_i32(r);
    inimigo->magia = leitor_binario_i32(r);
    inimigo->ataque = leitor_binario_i32(r);
    inimigo->defesa = leitor_binario_i32(r);
    inimigo->poder_magico = leitor_binario_i32(r);
    inimigo->resistencia = leitor_binario_i32(r);
    inimigo->velocidade = leitor_binario_i32(r);
    inimigo->sorte = leitor_binario_i32(r);
    inimigo->experiencia = leitor_binario_i32(r);
    inimigo->ouro = leitor_binario_i32(r);
    return !r->erro;
}

static int v4_ler_estado(LeitorBinario *r, EstadoCombate *estado) {
    estado->id = leitor_binario_u64(r);
    leitor_binario_texto(r, estado->nome, sizeof(estado->nome));
    estado->duracao = leitor_binario_i32(r);
    estado->dano_turno = leitor_binario_i32(r);
    estado->ataque_pct = leitor_binario_i32(r);
    estado->defesa_pct = leitor_binario_i32(r);
    estado->velocidade_pct = leitor_binario_i32(r);
    return !r->erro;
}

static int resolver_id_obrigatorio(const void *dados, int quantidade, size_t tamanho,
                                   Identificador id, int *destino) {
    if (!id) {
        *destino = -1;
        return 1;
    }
    int indice = indice_por_id(dados, quantidade, tamanho, id);
    if (indice < 0)
        return 0;
    *destino = indice;
    return 1;
}

static int deserializar_projeto_v4(Projeto *p, LeitorBinario *r) {
    Projeto novo = {0};
    Identificador habilidade_estado[128] = {0}, habilidade_animacao[128] = {0};
    Identificador classe_habilidades[32][32] = {{0}};
    Identificador heroi_classe[16] = {0}, heroi_sprite[16] = {0};
    Identificador loja_itens[64][32] = {{0}};
    Identificador missao_alvos[128][16] = {{0}}, missao_recompensa[128] = {0};
    Identificador **evento_referencias = NULL, *mapa_regioes = NULL;
    Identificador mapa_inicial = 0;
    int ok = 0;

    novo.id = leitor_binario_u64(r);
    novo.proximo_id = leitor_binario_u64(r);
    leitor_binario_texto(r, novo.nome, sizeof(novo.nome));
    leitor_binario_texto(r, novo.autor, sizeof(novo.autor));
    novo.tamanho_tile = leitor_binario_i32(r);
    novo.batalha_lateral = leitor_binario_i32(r);
    novo.idioma_interface = leitor_binario_i32(r);
    novo.inicio_x = leitor_binario_i32(r);
    novo.inicio_y = leitor_binario_i32(r);

    if (!v4_ler_contagem(r, MYTHARA_MAX_RECURSOS, &novo.quantidade_recursos) ||
        !reservar_memoria((void **)&novo.recursos, &novo.capacidade_recursos,
                          novo.quantidade_recursos, sizeof(Recurso), MYTHARA_MAX_RECURSOS))
        goto fim;
    for (int i = 0; i < novo.quantidade_recursos; ++i)
        if (!v4_ler_recurso(r, &novo.recursos[i]))
            goto fim;

    if (!v4_ler_contagem(r, MYTHARA_MAX_ITENS, &novo.quantidade_itens) ||
        !reservar_memoria((void **)&novo.itens, &novo.capacidade_itens, novo.quantidade_itens,
                          sizeof(Item), MYTHARA_MAX_ITENS))
        goto fim;
    for (int i = 0; i < novo.quantidade_itens; ++i)
        if (!v4_ler_item(r, &novo.itens[i]))
            goto fim;

    if (!v4_ler_contagem(r, MYTHARA_MAX_INIMIGOS, &novo.quantidade_inimigos) ||
        !reservar_memoria((void **)&novo.inimigos, &novo.capacidade_inimigos,
                          novo.quantidade_inimigos, sizeof(Inimigo), MYTHARA_MAX_INIMIGOS))
        goto fim;
    for (int i = 0; i < novo.quantidade_inimigos; ++i)
        if (!v4_ler_inimigo(r, &novo.inimigos[i]))
            goto fim;

    if (!v4_ler_contagem(r, 64, &novo.quantidade_estados))
        goto fim;
    for (int i = 0; i < novo.quantidade_estados; ++i)
        if (!v4_ler_estado(r, &novo.estados[i]))
            goto fim;

    if (!v4_ler_contagem(r, 128, &novo.quantidade_habilidades))
        goto fim;
    for (int i = 0; i < novo.quantidade_habilidades; ++i) {
        Habilidade *h = &novo.habilidades[i];
        h->id = leitor_binario_u64(r);
        leitor_binario_texto(r, h->nome, sizeof(h->nome));
        leitor_binario_texto(r, h->descricao, sizeof(h->descricao));
        h->custo_mp = leitor_binario_i32(r);
        h->poder = leitor_binario_i32(r);
        h->elemento = leitor_binario_i32(r);
        h->alvo = leitor_binario_i32(r);
        habilidade_estado[i] = leitor_binario_u64(r);
        habilidade_animacao[i] = leitor_binario_u64(r);
    }

    if (!v4_ler_contagem(r, 32, &novo.quantidade_classes))
        goto fim;
    for (int i = 0; i < novo.quantidade_classes; ++i) {
        Classe *c = &novo.classes[i];
        c->id = leitor_binario_u64(r);
        leitor_binario_texto(r, c->nome, sizeof(c->nome));
        c->vida_base = leitor_binario_i32(r);
        c->magia_base = leitor_binario_i32(r);
        c->ataque_base = leitor_binario_i32(r);
        c->defesa_base = leitor_binario_i32(r);
        c->poder_magico_base = leitor_binario_i32(r);
        c->resistencia_base = leitor_binario_i32(r);
        c->velocidade_base = leitor_binario_i32(r);
        c->sorte_base = leitor_binario_i32(r);
        for (int nivel = 0; nivel < 32; ++nivel)
            classe_habilidades[i][nivel] = leitor_binario_u64(r);
    }

    if (!v4_ler_contagem(r, 16, &novo.quantidade_herois) || !novo.quantidade_herois)
        goto fim;
    for (int i = 0; i < novo.quantidade_herois; ++i) {
        Heroi *h = &novo.herois[i];
        h->id = leitor_binario_u64(r);
        leitor_binario_texto(r, h->nome, sizeof(h->nome));
        heroi_classe[i] = leitor_binario_u64(r);
        h->vida_maxima = leitor_binario_i32(r);
        h->magia_maxima = leitor_binario_i32(r);
        h->ataque = leitor_binario_i32(r);
        h->defesa = leitor_binario_i32(r);
        h->poder_magico = leitor_binario_i32(r);
        h->resistencia = leitor_binario_i32(r);
        h->velocidade = leitor_binario_i32(r);
        h->sorte = leitor_binario_i32(r);
        heroi_sprite[i] = leitor_binario_u64(r);
    }

    if (!v4_ler_contagem(r, 64, &novo.quantidade_lojas))
        goto fim;
    for (int i = 0; i < novo.quantidade_lojas; ++i) {
        Loja *loja = &novo.lojas[i];
        loja->id = leitor_binario_u64(r);
        leitor_binario_texto(r, loja->nome, sizeof(loja->nome));
        if (!v4_ler_contagem(r, 32, &loja->quantidade_itens))
            goto fim;
        for (int item = 0; item < loja->quantidade_itens; ++item) {
            loja_itens[i][item] = leitor_binario_u64(r);
            loja->precos[item] = leitor_binario_i32(r);
        }
    }

    if (!v4_ler_contagem(r, 128, &novo.quantidade_missoes))
        goto fim;
    for (int i = 0; i < novo.quantidade_missoes; ++i) {
        Missao *missao = &novo.missoes[i];
        missao->id = leitor_binario_u64(r);
        leitor_binario_texto(r, missao->nome, sizeof(missao->nome));
        leitor_binario_texto(r, missao->descricao, sizeof(missao->descricao));
        if (!v4_ler_contagem(r, 16, &missao->quantidade_etapas))
            goto fim;
        for (int etapa = 0; etapa < missao->quantidade_etapas; ++etapa) {
            EtapaMissao *e = &missao->etapas[etapa];
            leitor_binario_texto(r, e->descricao, sizeof(e->descricao));
            e->tipo = leitor_binario_i32(r);
            e->alvo = leitor_binario_i32(r);
            missao_alvos[i][etapa] = leitor_binario_u64(r);
            e->quantidade = leitor_binario_i32(r);
        }
        missao_recompensa[i] = leitor_binario_u64(r);
        missao->recompensa_quantidade = leitor_binario_i32(r);
        missao->recompensa_ouro = leitor_binario_i32(r);
        missao->recompensa_experiencia = leitor_binario_i32(r);
    }

    if (!v4_ler_contagem(r, MYTHARA_MAX_EVENTOS, &novo.quantidade_eventos) ||
        !reservar_memoria((void **)&novo.eventos, &novo.capacidade_eventos, novo.quantidade_eventos,
                          sizeof(Evento), MYTHARA_MAX_EVENTOS))
        goto fim;
    evento_referencias = calloc((size_t)novo.quantidade_eventos, sizeof(*evento_referencias));
    if (novo.quantidade_eventos && !evento_referencias)
        goto fim;
    for (int i = 0; i < novo.quantidade_eventos; ++i) {
        Evento *evento = &novo.eventos[i];
        evento->id = leitor_binario_u64(r);
        evento->ativo = leitor_binario_i32(r);
        leitor_binario_texto(r, evento->nome, sizeof(evento->nome));
        evento->condicao_flag = leitor_binario_i32(r);
        evento->condicao_valor = leitor_binario_i32(r);
        if (!v4_ler_contagem(r, MYTHARA_MAX_COMANDOS, &evento->quantidade_comandos) ||
            !reservar_comandos(evento, evento->quantidade_comandos))
            goto fim;
        evento_referencias[i] =
            calloc((size_t)evento->quantidade_comandos, sizeof(**evento_referencias));
        if (evento->quantidade_comandos && !evento_referencias[i])
            goto fim;
        for (int comando = 0; comando < evento->quantidade_comandos; ++comando) {
            ComandoEvento *c = &evento->comandos[comando];
            c->tipo = leitor_binario_i32(r);
            c->a = leitor_binario_i32(r);
            c->b = leitor_binario_i32(r);
            c->c = leitor_binario_i32(r);
            c->profundidade = leitor_binario_i32(r);
            leitor_binario_texto(r, c->texto, sizeof(c->texto));
            evento_referencias[i][comando] = leitor_binario_u64(r);
        }
    }

    if (!v4_ler_contagem(r, MYTHARA_MAX_MAPAS, &novo.quantidade_mapas) || !novo.quantidade_mapas ||
        !reservar_memoria((void **)&novo.mapas, &novo.capacidade_mapas, novo.quantidade_mapas,
                          sizeof(Mapa), MYTHARA_MAX_MAPAS))
        goto fim;
    mapa_regioes = calloc((size_t)novo.quantidade_mapas, sizeof(*mapa_regioes));
    if (!mapa_regioes)
        goto fim;
    for (int i = 0; i < novo.quantidade_mapas; ++i) {
        Mapa *mapa = &novo.mapas[i];
        mapa->id = leitor_binario_u64(r);
        leitor_binario_texto(r, mapa->nome, sizeof(mapa->nome));
        mapa->largura = leitor_binario_i32(r);
        mapa->altura = leitor_binario_i32(r);
        if (mapa->largura < 4 || mapa->largura > MYTHARA_MAX_LARGURA_MAPA || mapa->altura < 4 ||
            mapa->altura > MYTHARA_MAX_ALTURA_MAPA ||
            !v4_ler_contagem(r, MYTHARA_MAX_CAMADAS, &mapa->quantidade_camadas) ||
            !mapa->quantidade_camadas ||
            !v4_ler_contagem(r, MYTHARA_MAX_ENTIDADES, &mapa->quantidade_entidades))
            goto fim;
        mapa_regioes[i] = leitor_binario_u64(r);
        size_t total = (size_t)mapa->largura * (size_t)mapa->altura;
        for (int camada = 0; camada < mapa->quantidade_camadas; ++camada) {
            CamadaMapa *c = &mapa->camadas[camada];
            c->id = leitor_binario_u64(r);
            leitor_binario_texto(r, c->nome, sizeof(c->nome));
            c->visivel = leitor_binario_i32(r);
            c->bloqueada = leitor_binario_i32(r);
            c->tiles = malloc(total * sizeof(uint16_t));
            if (!c->tiles)
                goto fim;
            for (size_t tile = 0; tile < total; ++tile)
                c->tiles[tile] = leitor_binario_u16(r);
        }
        mapa->colisoes = malloc(total);
        if (!mapa->colisoes || !leitor_binario_bytes(r, mapa->colisoes, total) ||
            !reservar_entidades(mapa, mapa->quantidade_entidades))
            goto fim;
        for (int entidade = 0; entidade < mapa->quantidade_entidades; ++entidade) {
            Entidade *e = &mapa->entidades[entidade];
            e->id = leitor_binario_u64(r);
            e->ativo = leitor_binario_i32(r);
            leitor_binario_texto(r, e->nome, sizeof(e->nome));
            e->tipo = leitor_binario_i32(r);
            e->x = leitor_binario_i32(r);
            e->y = leitor_binario_i32(r);
            e->evento_id = leitor_binario_u64(r);
            e->cor = leitor_binario_i32(r);
        }
    }
    mapa_inicial = leitor_binario_u64(r);
    if (!leitor_binario_fim(r))
        goto fim;

    for (int i = 0; i < novo.quantidade_habilidades; ++i)
        if (!resolver_id_obrigatorio(novo.estados, novo.quantidade_estados, sizeof(EstadoCombate),
                                     habilidade_estado[i], &novo.habilidades[i].estado) ||
            !resolver_id_obrigatorio(novo.recursos, novo.quantidade_recursos, sizeof(Recurso),
                                     habilidade_animacao[i], &novo.habilidades[i].animacao))
            goto fim;
    for (int i = 0; i < novo.quantidade_classes; ++i)
        for (int nivel = 0; nivel < 32; ++nivel)
            if (!resolver_id_obrigatorio(novo.habilidades, novo.quantidade_habilidades,
                                         sizeof(Habilidade), classe_habilidades[i][nivel],
                                         &novo.classes[i].habilidade_por_nivel[nivel]))
                goto fim;
    for (int i = 0; i < novo.quantidade_herois; ++i)
        if (!resolver_id_obrigatorio(novo.classes, novo.quantidade_classes, sizeof(Classe),
                                     heroi_classe[i], &novo.herois[i].classe) ||
            !resolver_id_obrigatorio(novo.recursos, novo.quantidade_recursos, sizeof(Recurso),
                                     heroi_sprite[i], &novo.herois[i].sprite))
            goto fim;
    for (int i = 0; i < novo.quantidade_lojas; ++i)
        for (int item = 0; item < novo.lojas[i].quantidade_itens; ++item)
            if (!resolver_id_obrigatorio(novo.itens, novo.quantidade_itens, sizeof(Item),
                                         loja_itens[i][item], &novo.lojas[i].itens[item]))
                goto fim;
    for (int i = 0; i < novo.quantidade_missoes; ++i) {
        if (!resolver_id_obrigatorio(novo.itens, novo.quantidade_itens, sizeof(Item),
                                     missao_recompensa[i], &novo.missoes[i].recompensa_item))
            goto fim;
        for (int etapa = 0; etapa < novo.missoes[i].quantidade_etapas; ++etapa)
            if (missao_alvos[i][etapa] &&
                !resolver_id_obrigatorio(novo.inimigos, novo.quantidade_inimigos, sizeof(Inimigo),
                                         missao_alvos[i][etapa],
                                         &novo.missoes[i].etapas[etapa].alvo))
                goto fim;
    }
    for (int i = 0; i < novo.quantidade_eventos; ++i)
        for (int comando = 0; comando < novo.eventos[i].quantidade_comandos; ++comando)
            if (!resolver_referencia_comando(&novo, &novo.eventos[i].comandos[comando],
                                             evento_referencias[i][comando]))
                goto fim;
    for (int i = 0; i < novo.quantidade_mapas; ++i) {
        if (!resolver_id_obrigatorio(novo.inimigos, novo.quantidade_inimigos, sizeof(Inimigo),
                                     mapa_regioes[i], &novo.mapas[i].regiao_encontro))
            goto fim;
        for (int entidade = 0; entidade < novo.mapas[i].quantidade_entidades; ++entidade) {
            Entidade *e = &novo.mapas[i].entidades[entidade];
            if (!resolver_id_obrigatorio(novo.eventos, novo.quantidade_eventos, sizeof(Evento),
                                         e->evento_id, &e->evento))
                goto fim;
        }
    }
    if (!resolver_id_obrigatorio(novo.mapas, novo.quantidade_mapas, sizeof(Mapa), mapa_inicial,
                                 &novo.mapa_inicial))
        goto fim;
    atribuir_ids_ausentes(&novo);
    ok = 1;

fim:
    if (evento_referencias) {
        for (int i = 0; i < novo.quantidade_eventos; ++i)
            free(evento_referencias[i]);
    }
    free(evento_referencias);
    free(mapa_regioes);
    if (!ok || r->erro) {
        liberar_projeto(&novo);
        return 0;
    }
    *p = novo;
    return 1;
}

static int salvar_payload_v4(const uint8_t *dados, size_t tamanho, const char *caminho, char *erro,
                             size_t cap_erro) {
    if (tamanho > UINT32_MAX) {
        snprintf(erro, cap_erro, "Projeto grande demais para o formato v4.");
        return 0;
    }
    EscritorBinario arquivo = {0};
    const char magia[8] = {'M', 'Y', 'T', 'H', 'R', 'V', '4', '\0'};
    escritor_binario_bytes(&arquivo, magia, sizeof(magia));
    escritor_binario_u32(&arquivo, MYTHARA_VERSAO);
    escritor_binario_u32(&arquivo, 1);
    escritor_binario_bytes(&arquivo, "DADO", 4);
    escritor_binario_u32(&arquivo, 1); /* chunk obrigatorio */
    escritor_binario_u32(&arquivo, 1); /* versao do chunk */
    escritor_binario_u64(&arquivo, (uint64_t)tamanho);
    escritor_binario_u32(&arquivo, crc32_binario(dados, tamanho));
    escritor_binario_bytes(&arquivo, dados, tamanho);
    if (arquivo.erro) {
        escritor_binario_liberar(&arquivo);
        snprintf(erro, cap_erro, "Memoria insuficiente para concluir o projeto.");
        return 0;
    }
    char temporario[MYTHARA_MAX_CAMINHO + 16];
    snprintf(temporario, sizeof(temporario), "%s.tmp", caminho);
    FILE *f = fopen(temporario, "wb");
    int ok_arquivo = f && fwrite(arquivo.dados, arquivo.tamanho, 1, f) == 1;
    if (f && (fflush(f) != 0 || fsync(fileno(f)) != 0))
        ok_arquivo = 0;
    if (f && fclose(f) != 0)
        ok_arquivo = 0;
    escritor_binario_liberar(&arquivo);
    if (!ok_arquivo || !substituir_arquivo(temporario, caminho)) {
        unlink(temporario);
        snprintf(erro, cap_erro, "Falha ao concluir o arquivo v4.");
        return 0;
    }
    return 1;
}

static int salvar_projeto_em(const Projeto *p, const char *caminho, char *erro, size_t cap_erro) {
    EscritorBinario payload = {0};
    if (!serializar_projeto_v4(p, &payload)) {
        escritor_binario_liberar(&payload);
        snprintf(erro, cap_erro, "Memoria insuficiente para serializar o projeto v4.");
        return 0;
    }
    int ok = salvar_payload_v4(payload.dados, payload.tamanho, caminho, erro, cap_erro);
    escritor_binario_liberar(&payload);
    return ok;
}

static int carregar_arquivo_limitado(const char *caminho, uint8_t **dados, size_t *tamanho) {
    FILE *f = fopen(caminho, "rb");
    if (!f || fseek(f, 0, SEEK_END) != 0) {
        if (f)
            fclose(f);
        return 0;
    }
    long fim = ftell(f);
    if (fim < 0 || (uint64_t)fim > 512u * 1024u * 1024u || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return 0;
    }
    *tamanho = (size_t)fim;
    *dados = malloc(*tamanho ? *tamanho : 1);
    int ok = *dados && (!*tamanho || fread(*dados, *tamanho, 1, f) == 1);
    fclose(f);
    if (!ok) {
        free(*dados);
        *dados = NULL;
        return 0;
    }
    return 1;
}

static int carregar_projeto_v4_de(Projeto *p, const char *caminho, char *erro, size_t cap_erro) {
    uint8_t *dados = NULL;
    size_t tamanho = 0;
    if (!carregar_arquivo_limitado(caminho, &dados, &tamanho)) {
        snprintf(erro, cap_erro, "Nao foi possivel abrir o projeto v4.");
        return 0;
    }
    LeitorBinario r = {dados, tamanho, 0, 0};
    char magia[8];
    leitor_binario_bytes(&r, magia, sizeof(magia));
    uint32_t versao = leitor_binario_u32(&r), blocos = leitor_binario_u32(&r);
    if (r.erro || memcmp(magia, "MYTHRV4", 7) || versao != 4 || !blocos || blocos > 64) {
        free(dados);
        snprintf(erro, cap_erro, "Projeto v4 truncado, corrompido ou incompativel.");
        return 0;
    }
    Projeto novo = {0};
    int encontrou_dados = 0, ok = 1;
    for (uint32_t bloco = 0; bloco < blocos && ok; ++bloco) {
        char id[4];
        leitor_binario_bytes(&r, id, sizeof(id));
        uint32_t flags = leitor_binario_u32(&r), versao_bloco = leitor_binario_u32(&r);
        uint64_t tamanho_payload = leitor_binario_u64(&r);
        uint32_t soma = leitor_binario_u32(&r);
        if (r.erro || tamanho_payload > r.tamanho - r.posicao ||
            crc32_binario(r.dados + r.posicao, (size_t)tamanho_payload) != soma) {
            ok = 0;
            break;
        }
        if (!memcmp(id, "DADO", 4)) {
            if (encontrou_dados || !(flags & 1u) || versao_bloco != 1) {
                ok = 0;
                break;
            }
            LeitorBinario payload = {r.dados + r.posicao, (size_t)tamanho_payload, 0, 0};
            encontrou_dados = deserializar_projeto_v4(&novo, &payload);
            ok = encontrou_dados;
        } else if (flags & 1u) {
            ok = 0;
            break;
        }
        r.posicao += (size_t)tamanho_payload;
    }
    ok = ok && encontrou_dados && leitor_binario_fim(&r) && validar_projeto(&novo, erro, cap_erro);
    free(dados);
    if (!ok) {
        liberar_projeto(&novo);
        if (!erro[0])
            snprintf(erro, cap_erro, "Estrutura v4 invalida ou referencia ausente.");
        return 0;
    }
    *p = novo;
    return 1;
}

static int versao_arquivo_projeto(const char *caminho) {
    char magia[8] = {0};
    FILE *f = fopen(caminho, "rb");
    if (!f)
        return 0;
    size_t lidos = fread(magia, 1, sizeof(magia), f);
    fclose(f);
    if (lidos == sizeof(magia) && !memcmp(magia, "MYTHRV4", 7))
        return 4;
    if (lidos == sizeof(magia) && !memcmp(magia, "MYTHRV3", 7))
        return 3;
    return 0;
}

static int carregar_projeto_de(Projeto *p, const char *caminho, char *erro, size_t cap_erro) {
    int versao = versao_arquivo_projeto(caminho);
    if (versao == 4)
        return carregar_projeto_v4_de(p, caminho, erro, cap_erro);
    if (versao == 3)
        return carregar_projeto_v3_de(p, caminho, erro, cap_erro);
    snprintf(erro, cap_erro, "Arquivo nao reconhecido como projeto Mythara v3 ou v4.");
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
    EscritorBinario b = {0};
    if (!serializar_projeto_v4(p, &b)) {
        if (bytes)
            *bytes = 0;
        return 0;
    }
    uint32_t soma = crc32_binario(b.dados, b.tamanho);
    if (bytes)
        *bytes = b.tamanho;
    escritor_binario_liberar(&b);
    return soma;
}

static void v4_escrever_membro(EscritorBinario *w, const Projeto *p, const MembroGrupo *m) {
    escritor_binario_u64(w,
                         id_por_indice(p->herois, p->quantidade_herois, sizeof(Heroi), m->heroi));
    escritor_binario_i32(w, m->nivel);
    escritor_binario_i32(w, m->vida);
    escritor_binario_i32(w, m->vida_maxima);
    escritor_binario_i32(w, m->magia);
    escritor_binario_i32(w, m->magia_maxima);
    escritor_binario_i32(w, m->ataque);
    escritor_binario_i32(w, m->defesa);
    escritor_binario_i32(w, m->poder_magico);
    escritor_binario_i32(w, m->resistencia);
    escritor_binario_i32(w, m->velocidade);
    escritor_binario_i32(w, m->sorte);
    for (int i = 0; i < 4; ++i)
        escritor_binario_u64(
            w, id_por_indice(p->itens, p->quantidade_itens, sizeof(Item), m->equipamentos[i]));
    for (int i = 0; i < 8; ++i) {
        Identificador estado = m->duracao_estados[i] > 0
                                   ? id_por_indice(p->estados, p->quantidade_estados,
                                                   sizeof(EstadoCombate), m->estados[i])
                                   : 0;
        escritor_binario_u64(w, estado);
        escritor_binario_i32(w, m->duracao_estados[i]);
    }
}

static int salvar_estado(const Projeto *p, const EstadoJogo *j, const char *caminho) {
    EscritorBinario payload = {0}, arquivo = {0};
    escritor_binario_u64(&payload,
                         id_por_indice(p->mapas, p->quantidade_mapas, sizeof(Mapa), j->mapa));
    escritor_binario_i32(&payload, j->x);
    escritor_binario_i32(&payload, j->y);
    escritor_binario_i32(&payload, j->vida);
    escritor_binario_i32(&payload, j->vida_maxima);
    escritor_binario_i32(&payload, j->ataque);
    escritor_binario_i32(&payload, j->defesa);
    escritor_binario_i32(&payload, j->nivel);
    escritor_binario_i32(&payload, j->experiencia);
    escritor_binario_i32(&payload, j->ouro);
    escritor_binario_i32(&payload, j->magia);
    escritor_binario_i32(&payload, j->magia_maxima);
    escritor_binario_i32(&payload, j->poder_magico);
    escritor_binario_i32(&payload, j->resistencia);
    escritor_binario_i32(&payload, j->velocidade);
    escritor_binario_i32(&payload, j->sorte);
    int inventario = 0;
    for (int i = 0; i < p->quantidade_itens; ++i)
        if (j->inventario[i])
            inventario++;
    escritor_binario_u32(&payload, (uint32_t)inventario);
    for (int i = 0; i < p->quantidade_itens; ++i)
        if (j->inventario[i]) {
            escritor_binario_u64(&payload, p->itens[i].id);
            escritor_binario_i32(&payload, j->inventario[i]);
        }
    for (int i = 0; i < 4; ++i)
        escritor_binario_u64(&payload, id_por_indice(p->itens, p->quantidade_itens, sizeof(Item),
                                                     j->equipamentos[i]));
    escritor_binario_u32(&payload, (uint32_t)p->quantidade_missoes);
    for (int i = 0; i < p->quantidade_missoes; ++i) {
        escritor_binario_u64(&payload, p->missoes[i].id);
        escritor_binario_i32(&payload, j->progresso_missoes[i]);
        escritor_binario_i32(&payload, j->estado_missoes[i]);
    }
    escritor_binario_u32(&payload, (uint32_t)j->quantidade_grupo);
    for (int i = 0; i < j->quantidade_grupo; ++i)
        v4_escrever_membro(&payload, p, &j->grupo[i]);
    for (int i = 0; i < MYTHARA_MAX_FLAGS; ++i)
        escritor_binario_i32(&payload, j->flags[i]);
    for (int i = 0; i < MYTHARA_MAX_VARIAVEIS; ++i)
        escritor_binario_i32(&payload, j->variaveis[i]);
    if (payload.erro)
        goto falha;

    escritor_binario_bytes(&arquivo, "MYTSAV4", 8);
    escritor_binario_u32(&arquivo, 4);
    escritor_binario_u64(&arquivo, p->id);
    escritor_binario_u64(&arquivo, (uint64_t)payload.tamanho);
    escritor_binario_u32(&arquivo, crc32_binario(payload.dados, payload.tamanho));
    escritor_binario_bytes(&arquivo, payload.dados, payload.tamanho);
    if (arquivo.erro)
        goto falha;
    char temporario[MYTHARA_MAX_CAMINHO + 16];
    snprintf(temporario, sizeof(temporario), "%s.tmp", caminho);
    FILE *f = fopen(temporario, "wb");
    int ok = f && fwrite(arquivo.dados, arquivo.tamanho, 1, f) == 1;
    if (f && (fflush(f) != 0 || fsync(fileno(f)) != 0))
        ok = 0;
    if (f && fclose(f) != 0)
        ok = 0;
    if (!ok || !substituir_arquivo(temporario, caminho)) {
        unlink(temporario);
        goto falha;
    }
    escritor_binario_liberar(&payload);
    escritor_binario_liberar(&arquivo);
    return 1;
falha:
    escritor_binario_liberar(&payload);
    escritor_binario_liberar(&arquivo);
    return 0;
}

static int v4_ler_membro(LeitorBinario *r, const Projeto *p, MembroGrupo *m) {
    Identificador heroi = leitor_binario_u64(r);
    if (!resolver_id_obrigatorio(p->herois, p->quantidade_herois, sizeof(Heroi), heroi, &m->heroi))
        return 0;
    m->nivel = leitor_binario_i32(r);
    m->vida = leitor_binario_i32(r);
    m->vida_maxima = leitor_binario_i32(r);
    m->magia = leitor_binario_i32(r);
    m->magia_maxima = leitor_binario_i32(r);
    m->ataque = leitor_binario_i32(r);
    m->defesa = leitor_binario_i32(r);
    m->poder_magico = leitor_binario_i32(r);
    m->resistencia = leitor_binario_i32(r);
    m->velocidade = leitor_binario_i32(r);
    m->sorte = leitor_binario_i32(r);
    for (int i = 0; i < 4; ++i) {
        Identificador item = leitor_binario_u64(r);
        if (!resolver_id_obrigatorio(p->itens, p->quantidade_itens, sizeof(Item), item,
                                     &m->equipamentos[i]))
            return 0;
    }
    for (int i = 0; i < 8; ++i) {
        Identificador estado = leitor_binario_u64(r);
        m->duracao_estados[i] = leitor_binario_i32(r);
        if (!resolver_id_obrigatorio(p->estados, p->quantidade_estados, sizeof(EstadoCombate),
                                     estado, &m->estados[i]))
            return 0;
    }
    return !r->erro;
}

static int carregar_estado(const Projeto *p, EstadoJogo *j, const char *caminho) {
    uint8_t *dados = NULL;
    size_t tamanho = 0;
    if (!carregar_arquivo_limitado(caminho, &dados, &tamanho))
        return 0;
    LeitorBinario cabecalho = {dados, tamanho, 0, 0};
    char magia[8];
    leitor_binario_bytes(&cabecalho, magia, sizeof(magia));
    uint32_t versao = leitor_binario_u32(&cabecalho);
    Identificador projeto_id = leitor_binario_u64(&cabecalho);
    uint64_t tamanho_payload = leitor_binario_u64(&cabecalho);
    uint32_t soma = leitor_binario_u32(&cabecalho);
    if (cabecalho.erro || memcmp(magia, "MYTSAV4", 7) || versao != 4 || projeto_id != p->id ||
        tamanho_payload != cabecalho.tamanho - cabecalho.posicao ||
        crc32_binario(cabecalho.dados + cabecalho.posicao, (size_t)tamanho_payload) != soma) {
        free(dados);
        return 0;
    }
    LeitorBinario r = {cabecalho.dados + cabecalho.posicao, (size_t)tamanho_payload, 0, 0};
    EstadoJogo novo = {0};
    for (int i = 0; i < 4; ++i)
        novo.equipamentos[i] = -1;
    Identificador mapa = leitor_binario_u64(&r);
    if (!resolver_id_obrigatorio(p->mapas, p->quantidade_mapas, sizeof(Mapa), mapa, &novo.mapa))
        goto falha_estado;
    novo.x = leitor_binario_i32(&r);
    novo.y = leitor_binario_i32(&r);
    novo.vida = leitor_binario_i32(&r);
    novo.vida_maxima = leitor_binario_i32(&r);
    novo.ataque = leitor_binario_i32(&r);
    novo.defesa = leitor_binario_i32(&r);
    novo.nivel = leitor_binario_i32(&r);
    novo.experiencia = leitor_binario_i32(&r);
    novo.ouro = leitor_binario_i32(&r);
    novo.magia = leitor_binario_i32(&r);
    novo.magia_maxima = leitor_binario_i32(&r);
    novo.poder_magico = leitor_binario_i32(&r);
    novo.resistencia = leitor_binario_i32(&r);
    novo.velocidade = leitor_binario_i32(&r);
    novo.sorte = leitor_binario_i32(&r);
    int quantidade = 0;
    if (!v4_ler_contagem(&r, MYTHARA_MAX_ITENS, &quantidade))
        goto falha_estado;
    for (int i = 0; i < quantidade; ++i) {
        Identificador item = leitor_binario_u64(&r);
        int indice = indice_por_id(p->itens, p->quantidade_itens, sizeof(Item), item);
        int valor = leitor_binario_i32(&r);
        if (indice < 0 || valor < 0 || valor > 99)
            goto falha_estado;
        novo.inventario[indice] = valor;
    }
    for (int i = 0; i < 4; ++i) {
        Identificador item = leitor_binario_u64(&r);
        if (!resolver_id_obrigatorio(p->itens, p->quantidade_itens, sizeof(Item), item,
                                     &novo.equipamentos[i]))
            goto falha_estado;
    }
    if (!v4_ler_contagem(&r, 128, &quantidade))
        goto falha_estado;
    for (int i = 0; i < quantidade; ++i) {
        Identificador missao = leitor_binario_u64(&r);
        int indice = indice_por_id(p->missoes, p->quantidade_missoes, sizeof(Missao), missao);
        int progresso = leitor_binario_i32(&r), estado = leitor_binario_i32(&r);
        if (indice < 0 || progresso < 0 || estado < 0 || estado > 2)
            goto falha_estado;
        novo.progresso_missoes[indice] = progresso;
        novo.estado_missoes[indice] = estado;
    }
    if (!v4_ler_contagem(&r, 4, &novo.quantidade_grupo) || !novo.quantidade_grupo)
        goto falha_estado;
    for (int i = 0; i < novo.quantidade_grupo; ++i)
        if (!v4_ler_membro(&r, p, &novo.grupo[i]))
            goto falha_estado;
    for (int i = 0; i < MYTHARA_MAX_FLAGS; ++i)
        novo.flags[i] = leitor_binario_i32(&r);
    for (int i = 0; i < MYTHARA_MAX_VARIAVEIS; ++i)
        novo.variaveis[i] = leitor_binario_i32(&r);
    if (!leitor_binario_fim(&r))
        goto falha_estado;
    Mapa *m = &p->mapas[novo.mapa];
    novo.x = limitar_int(novo.x, 0, m->largura - 1);
    novo.y = limitar_int(novo.y, 0, m->altura - 1);
    free(dados);
    *j = novo;
    return 1;
falha_estado:
    free(dados);
    return 0;
}
