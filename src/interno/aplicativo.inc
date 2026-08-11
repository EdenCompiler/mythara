/*
 * Modulo: aplicativo
 * Responsabilidade: estado compartilhado, configuracao e operacoes do projeto.
 * Depende de: persistencia, recursos e interface.
 */

/* Estados de alto nivel ----------------------------------------------------- */

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
    MODAL_BOAS_VINDAS,
    MODAL_MIGRACAO,
    MODAL_VALIDACAO,
    MODAL_DESCARTAR
} TipoModal;
typedef enum { ACAO_ABRIR, ACAO_SALVAR_COMO, ACAO_EXPORTAR } AcaoCaminho;
typedef enum { DESCARTE_NENHUM, DESCARTE_ABRIR, DESCARTE_SAIR } AcaoDescarte;
typedef enum {
    LAYOUT_MAPA,
    LAYOUT_EVENTOS,
    LAYOUT_BANCO,
    LAYOUT_RECURSOS,
    LAYOUT_TEMA,
    LAYOUT_PROJETO,
    LAYOUT_AJUDA,
    QUANTIDADE_LAYOUTS
} LayoutEditor;

typedef struct {
    Projeto projeto;
    size_t bytes;
    uint32_t resumo;
    char descricao[64];
} EstadoHistorico;

typedef struct {
    /* Navegacao do mapa. */
    int mapa_atual, camada, tile, zoom;
    int camera_x, camera_y;
    int ferramenta;
    int entidade_selecionada;

    /* Selecoes dos editores auxiliares. */
    int recurso_selecionado;
    int clipe_recurso;
    int evento_selecionado;
    int comando_selecionado;
    int aba_banco, registro_banco, subregistro_banco;

    /* Layout responsivo. */
    int largura_arvore, largura_inspetor, altura_inferior, divisor_ativo;
    int layout_ativo, gaveta_arvore, gaveta_inspetor;
    int barra_lateral_recolhida, tema_avancado, painel_inferior_aberto;
    int largura_barra_animada;
    int rolagem_workspace_x[QUANTIDADE_LAYOUTS], rolagem_workspace_y[QUANTIDADE_LAYOUTS];

    /* Selecao e ferramentas de area. */
    int selecao_x0, selecao_y0, selecao_x1, selecao_y1, selecao_ativa;
    int cursor_tile_x, cursor_tile_y, canvas_em_foco;
    int arrastando_forma, autotile;
    int largura_carimbo, altura_carimbo, camadas_carimbo;
    uint16_t carimbo[MYTHARA_MAX_CAMADAS][16 * 16];

    /* Clipboard estruturado. */
    int clipboard_largura, clipboard_altura, clipboard_camadas, clipboard_entidades;
    uint16_t clipboard_tiles[MYTHARA_MAX_CAMADAS][64 * 64];
    uint8_t clipboard_colisoes[64 * 64];
    Entidade clipboard_lista_entidades[256];

    /* Filtros e rolagens independentes. */
    char busca_arvore[64];
    char busca_comandos[64];
    char busca_banco[64];
    char busca_recursos[64];
    char busca_eventos[64];
    int rolagem_arvore, rolagem_banco, rolagem_eventos, rolagem_recursos, rolagem_comandos_evento;
    int cor_tema_selecionada;

    /* Historico local e snapshots profundos. */
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
    /* Estado persistente e interpretador de eventos. */
    EstadoJogo estado;
    int evento_ativo, proximo_comando;
    int aguardando_texto, aguardando_escolha;
    char texto[320];
    char escolha_a[80], escolha_b[80];

    /* Batalha e selecao de acoes. */
    int inimigo, vida_inimigo;
    int quantidade_inimigos_batalha;
    int inimigos_batalha[8], vidas_inimigos[8];
    int membro_escolhendo, acoes_grupo[4], defesas_grupo[4];

    /* Telas auxiliares e pilhas de controle. */
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
    /* Dominios principais. */
    Projeto projeto;
    Editor editor;
    Jogo jogo;
    ModoAplicativo modo;
    TipoModal modal;
    AcaoCaminho acao_caminho;
    AcaoDescarte acao_descarte;

    /* Documento e navegadores de arquivo. */
    char caminho_projeto[MYTHARA_MAX_CAMINHO];
    char caminho_recente[MYTHARA_MAX_CAMINHO];
    char campo_caminho[MYTHARA_MAX_CAMINHO];
    char pasta_navegador[MYTHARA_MAX_CAMINHO];
    char arquivo_navegador[256];
    char campo_tema[MYTHARA_MAX_CAMINHO];
    char caminho_migracao_origem[MYTHARA_MAX_CAMINHO];
    char caminho_migracao_destino[MYTHARA_MAX_CAMINHO];
    char caminho_pendente[MYTHARA_MAX_CAMINHO];
    char erro_validacao[256];
    char mensagem[256];

    /* Recursos carregados e estado da sessao. */
    Imagem imagens[MYTHARA_MAX_RECURSOS];
    int projeto_carregado;
    int foco_interface;
    double ultimo_autosalvamento;
    char caminho_autosave[MYTHARA_MAX_CAMINHO + 32];
    int oferecer_recuperacao;
    int mostrar_boas_vindas, dicas_ativas, idioma_editor;
    int encerrar_confirmado;
    atomic_int autosave_em_andamento;
} Aplicativo;

/* Configuracao e temas portateis ------------------------------------------- */

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

static void serializar_tema(EscritorBinario *w, const TemaInterface *tema) {
    escritor_binario_texto(w, tema->nome);
    const Cor cores[] = {tema->fundo,          tema->painel,   tema->painel_elevado, tema->controle,
                         tema->controle_sobre, tema->selecao,  tema->borda,          tema->texto,
                         tema->texto_suave,    tema->destaque, tema->perigo,         tema->aviso,
                         tema->sucesso};
    for (size_t i = 0; i < sizeof(cores) / sizeof(cores[0]); ++i)
        escritor_binario_u32(w, cores[i]);
    const Cor cores_semanticas[] = {tema->barra,       tema->lateral,      tema->lateral_sobre,
                                    tema->canvas,      tema->sobreposicao, tema->foco,
                                    tema->desabilitado};
    for (size_t i = 0; i < sizeof(cores_semanticas) / sizeof(cores_semanticas[0]); ++i)
        escritor_binario_u32(w, cores_semanticas[i]);
    escritor_binario_i32(w, tema->escala_percentual);
    escritor_binario_i32(w, tema->espacamento);
    escritor_binario_i32(w, tema->altura_controle);
    escritor_binario_i32(w, tema->espessura_borda);
    escritor_binario_i32(w, tema->raio_visual);
    escritor_binario_i32(w, tema->preset);
    escritor_binario_i32(w, tema->movimento_reduzido);
}

static int deserializar_tema(LeitorBinario *r, TemaInterface *tema) {
    memset(tema, 0, sizeof(*tema));
    leitor_binario_texto(r, tema->nome, sizeof(tema->nome));
    Cor *cores[] = {&tema->fundo,          &tema->painel,   &tema->painel_elevado, &tema->controle,
                    &tema->controle_sobre, &tema->selecao,  &tema->borda,          &tema->texto,
                    &tema->texto_suave,    &tema->destaque, &tema->perigo,         &tema->aviso,
                    &tema->sucesso};
    for (size_t i = 0; i < sizeof(cores) / sizeof(cores[0]); ++i)
        *cores[i] = leitor_binario_u32(r);
    Cor *cores_semanticas[] = {&tema->barra,       &tema->lateral,      &tema->lateral_sobre,
                               &tema->canvas,      &tema->sobreposicao, &tema->foco,
                               &tema->desabilitado};
    for (size_t i = 0; i < sizeof(cores_semanticas) / sizeof(cores_semanticas[0]); ++i)
        *cores_semanticas[i] = leitor_binario_u32(r);
    tema->escala_percentual = leitor_binario_i32(r);
    tema->espacamento = leitor_binario_i32(r);
    tema->altura_controle = leitor_binario_i32(r);
    tema->espessura_borda = leitor_binario_i32(r);
    tema->raio_visual = leitor_binario_i32(r);
    tema->preset = leitor_binario_i32(r);
    tema->movimento_reduzido = leitor_binario_i32(r);
    if (r->erro || tema->escala_percentual < 100 || tema->escala_percentual > 200 ||
        tema->espacamento < 2 || tema->espacamento > 16 || tema->altura_controle < 22 ||
        tema->altura_controle > 44 || tema->espessura_borda < 0 || tema->espessura_borda > 8 ||
        tema->raio_visual < 0 || tema->raio_visual > 16 || tema->preset < TEMA_ARCANO ||
        tema->preset > TEMA_PERSONALIZADO ||
        (tema->movimento_reduzido != 0 && tema->movimento_reduzido != 1))
        return 0;
    return 1;
}

static int salvar_documento_portatil(const char magia[8], uint32_t versao,
                                     const EscritorBinario *payload, const char *caminho) {
    EscritorBinario arquivo = {0};
    escritor_binario_bytes(&arquivo, magia, 8);
    escritor_binario_u32(&arquivo, versao);
    escritor_binario_u64(&arquivo, (uint64_t)payload->tamanho);
    escritor_binario_u32(&arquivo, crc32_binario(payload->dados, payload->tamanho));
    escritor_binario_bytes(&arquivo, payload->dados, payload->tamanho);
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
    escritor_binario_liberar(&arquivo);
    return 1;
falha:
    escritor_binario_liberar(&arquivo);
    return 0;
}

static int carregar_documento_portatil(const char magia[8], uint32_t versao_esperada,
                                       const char *caminho, uint8_t **dados, size_t *tamanho) {
    uint8_t *arquivo = NULL;
    size_t tamanho_arquivo = 0;
    if (!carregar_arquivo_limitado(caminho, &arquivo, &tamanho_arquivo))
        return 0;
    LeitorBinario r = {arquivo, tamanho_arquivo, 0, 0};
    char assinatura[8];
    leitor_binario_bytes(&r, assinatura, sizeof(assinatura));
    uint32_t versao = leitor_binario_u32(&r);
    uint64_t tamanho_payload = leitor_binario_u64(&r);
    uint32_t soma = leitor_binario_u32(&r);
    if (r.erro || memcmp(assinatura, magia, 8) || versao != versao_esperada ||
        tamanho_payload != r.tamanho - r.posicao ||
        crc32_binario(r.dados + r.posicao, (size_t)tamanho_payload) != soma) {
        free(arquivo);
        return 0;
    }
    *tamanho = (size_t)tamanho_payload;
    *dados = malloc(*tamanho ? *tamanho : 1);
    if (!*dados) {
        free(arquivo);
        return 0;
    }
    memcpy(*dados, r.dados + r.posicao, *tamanho);
    free(arquivo);
    return 1;
}

static void carregar_configuracao(Aplicativo *a) {
    char caminho[MYTHARA_MAX_CAMINHO];
    if (!caminho_configuracao(caminho, sizeof(caminho)))
        return;
    uint8_t *dados = NULL;
    size_t tamanho = 0;
    const char magia[8] = {'M', 'Y', 'T', 'C', 'F', 'G', '5', '\0'};
    if (!carregar_documento_portatil(magia, 5, caminho, &dados, &tamanho))
        return;
    LeitorBinario r = {dados, tamanho, 0, 0};
    TemaInterface tema;
    int largura_arvore, largura_inspetor, altura_inferior, layout_ativo, mostrar_boas_vindas,
        dicas_ativas, idioma_editor, barra_lateral_recolhida, painel_inferior_aberto;
    char caminho_recente[MYTHARA_MAX_CAMINHO];
    int ok = deserializar_tema(&r, &tema);
    largura_arvore = leitor_binario_i32(&r);
    largura_inspetor = leitor_binario_i32(&r);
    altura_inferior = leitor_binario_i32(&r);
    layout_ativo = leitor_binario_i32(&r);
    mostrar_boas_vindas = leitor_binario_i32(&r);
    dicas_ativas = leitor_binario_i32(&r);
    idioma_editor = leitor_binario_i32(&r);
    barra_lateral_recolhida = leitor_binario_i32(&r);
    painel_inferior_aberto = leitor_binario_i32(&r);
    leitor_binario_texto(&r, caminho_recente, sizeof(caminho_recente));
    ok = ok && leitor_binario_fim(&r);
    free(dados);
    if (ok) {
        tema_ativo = tema;
        tema_ativo.escala_percentual =
            limitar_int(((tema_ativo.escala_percentual + 12) / 25) * 25, 100, 200);
        a->editor.largura_arvore = limitar_int(largura_arvore, 190, 360);
        a->editor.largura_inspetor = limitar_int(largura_inspetor, 250, 420);
        a->editor.altura_inferior = limitar_int(altura_inferior, 95, 240);
        a->editor.layout_ativo = limitar_int(layout_ativo, 0, QUANTIDADE_LAYOUTS - 1);
        a->editor.barra_lateral_recolhida = barra_lateral_recolhida != 0;
        a->editor.painel_inferior_aberto = painel_inferior_aberto != 0;
        a->mostrar_boas_vindas = mostrar_boas_vindas != 0;
        a->dicas_ativas = dicas_ativas != 0;
        a->idioma_editor = limitar_int(idioma_editor, IDIOMA_PORTUGUES, IDIOMA_INGLES);
        idioma_definir((IdiomaMythara)a->idioma_editor);
        if (!a->caminho_recente[0])
            copiar_texto(a->caminho_recente, sizeof(a->caminho_recente), caminho_recente);
    }
}

static void salvar_configuracao(const Aplicativo *a) {
    char caminho[MYTHARA_MAX_CAMINHO];
    if (!caminho_configuracao(caminho, sizeof(caminho)))
        return;
    EscritorBinario payload = {0};
    serializar_tema(&payload, &tema_ativo);
    escritor_binario_i32(&payload, a->editor.largura_arvore);
    escritor_binario_i32(&payload, a->editor.largura_inspetor);
    escritor_binario_i32(&payload, a->editor.altura_inferior);
    escritor_binario_i32(&payload, a->editor.layout_ativo);
    escritor_binario_i32(&payload, a->mostrar_boas_vindas);
    escritor_binario_i32(&payload, a->dicas_ativas);
    escritor_binario_i32(&payload, a->idioma_editor);
    escritor_binario_i32(&payload, a->editor.barra_lateral_recolhida);
    escritor_binario_i32(&payload, a->editor.painel_inferior_aberto);
    escritor_binario_texto(&payload, a->caminho_recente);
    const char magia[8] = {'M', 'Y', 'T', 'C', 'F', 'G', '5', '\0'};
    if (!payload.erro)
        salvar_documento_portatil(magia, 5, &payload, caminho);
    escritor_binario_liberar(&payload);
}

static int salvar_tema_arquivo(const char *caminho) {
    EscritorBinario payload = {0};
    serializar_tema(&payload, &tema_ativo);
    const char magia[8] = {'M', 'Y', 'T', 'T', 'E', 'M', '5', '\0'};
    int ok = !payload.erro && salvar_documento_portatil(magia, 5, &payload, caminho);
    escritor_binario_liberar(&payload);
    return ok;
}
static int tema_arquivo_legado;
static int carregar_tema_arquivo(const char *caminho) {
    uint8_t *dados = NULL;
    size_t tamanho = 0;
    tema_arquivo_legado = 0;
    const char magia[8] = {'M', 'Y', 'T', 'T', 'E', 'M', '5', '\0'};
    if (!carregar_documento_portatil(magia, 5, caminho, &dados, &tamanho)) {
        FILE *f = fopen(caminho, "rb");
        char assinatura[8] = {0};
        if (f && fread(assinatura, sizeof(assinatura), 1, f) == 1 &&
            !memcmp(assinatura, "MYTTEM4", 7))
            tema_arquivo_legado = 1;
        if (f)
            fclose(f);
        return 0;
    }
    LeitorBinario r = {dados, tamanho, 0, 0};
    TemaInterface t;
    int ok = deserializar_tema(&r, &t) && leitor_binario_fim(&r);
    free(dados);
    if (ok) {
        tema_ativo = t;
    }
    return ok;
}

/* Referencias nomeadas na interface ---------------------------------------- */

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

typedef enum {
    REFERENCIA_MAPA,
    REFERENCIA_EVENTO,
    REFERENCIA_HEROI,
    REFERENCIA_CLASSE,
    REFERENCIA_HABILIDADE,
    REFERENCIA_ITEM,
    REFERENCIA_INIMIGO,
    REFERENCIA_ESTADO,
    REFERENCIA_LOJA,
    REFERENCIA_MISSAO,
    REFERENCIA_RECURSO
} TipoReferencia;

static int quantidade_referencia(const Projeto *p, TipoReferencia tipo) {
    switch (tipo) {
    case REFERENCIA_MAPA:
        return p->quantidade_mapas;
    case REFERENCIA_EVENTO:
        return p->quantidade_eventos;
    case REFERENCIA_HEROI:
        return p->quantidade_herois;
    case REFERENCIA_CLASSE:
        return p->quantidade_classes;
    case REFERENCIA_HABILIDADE:
        return p->quantidade_habilidades;
    case REFERENCIA_ITEM:
        return p->quantidade_itens;
    case REFERENCIA_INIMIGO:
        return p->quantidade_inimigos;
    case REFERENCIA_ESTADO:
        return p->quantidade_estados;
    case REFERENCIA_LOJA:
        return p->quantidade_lojas;
    case REFERENCIA_MISSAO:
        return p->quantidade_missoes;
    case REFERENCIA_RECURSO:
        return p->quantidade_recursos;
    }
    return 0;
}

static const char *nome_referencia(const Projeto *p, TipoReferencia tipo, int indice) {
    if (indice < 0 || indice >= quantidade_referencia(p, tipo))
        return "(NENHUM)";
    switch (tipo) {
    case REFERENCIA_MAPA:
        return p->mapas[indice].nome;
    case REFERENCIA_EVENTO:
        return p->eventos[indice].nome;
    case REFERENCIA_HEROI:
        return p->herois[indice].nome;
    case REFERENCIA_CLASSE:
        return p->classes[indice].nome;
    case REFERENCIA_HABILIDADE:
        return p->habilidades[indice].nome;
    case REFERENCIA_ITEM:
        return p->itens[indice].nome;
    case REFERENCIA_INIMIGO:
        return p->inimigos[indice].nome;
    case REFERENCIA_ESTADO:
        return p->estados[indice].nome;
    case REFERENCIA_LOJA:
        return p->lojas[indice].nome;
    case REFERENCIA_MISSAO:
        return p->missoes[indice].nome;
    case REFERENCIA_RECURSO:
        return p->recursos[indice].nome;
    }
    return "(NENHUM)";
}

static int ui_referencia(Interface *ui, Retangulo r, const Projeto *p, const char *rotulo,
                         TipoReferencia tipo, int valor, int opcional) {
    int quantidade = quantidade_referencia(p, tipo), minimo = opcional ? -1 : 0;
    if (!quantidade)
        valor = -1;
    else
        valor = limitar_int(valor, minimo, quantidade - 1);
    if (ui_botao(ui, (Retangulo){r.x, r.y, 26, r.altura}, "<", 0))
        valor = valor <= minimo ? quantidade - 1 : valor - 1;
    char texto[160];
    snprintf(texto, sizeof(texto), "%s: %.100s", idioma_traduzir(rotulo),
             idioma_traduzir(nome_referencia(p, tipo, valor)));
    if (ui_botao(ui, (Retangulo){r.x + 29, r.y, r.largura - 58, r.altura}, texto, 0))
        valor = valor >= quantidade - 1 ? minimo : valor + 1;
    if (ui_botao(ui, (Retangulo){r.x + r.largura - 26, r.y, 26, r.altura}, ">", 0))
        valor = valor >= quantidade - 1 ? minimo : valor + 1;
    return valor;
}

static int tipo_referencia_comando(int tipo_comando, TipoReferencia *tipo, const char **rotulo) {
    if (tipo_comando == COMANDO_ITEM) {
        *tipo = REFERENCIA_ITEM;
        *rotulo = "ITEM";
    } else if (tipo_comando == COMANDO_TELEPORTE) {
        *tipo = REFERENCIA_MAPA;
        *rotulo = "MAPA";
    } else if (tipo_comando == COMANDO_BATALHA) {
        *tipo = REFERENCIA_INIMIGO;
        *rotulo = "INIMIGO";
    } else if (tipo_comando == COMANDO_AUDIO) {
        *tipo = REFERENCIA_RECURSO;
        *rotulo = "AUDIO";
    } else if (tipo_comando == COMANDO_CHAMAR_EVENTO) {
        *tipo = REFERENCIA_EVENTO;
        *rotulo = "EVENTO";
    } else if (tipo_comando == COMANDO_LOJA) {
        *tipo = REFERENCIA_LOJA;
        *rotulo = "LOJA";
    } else if (tipo_comando == COMANDO_MISSAO) {
        *tipo = REFERENCIA_MISSAO;
        *rotulo = "MISSAO";
    } else
        return 0;
    return 1;
}

static void mensagem_aplicativo(Aplicativo *a, const char *texto) {
    copiar_texto(a->mensagem, sizeof(a->mensagem), texto);
}

/* Historico de edicao ------------------------------------------------------- */

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

/* Ciclo de vida do documento ------------------------------------------------ */

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

static void reiniciar_editor_preservando_interface(Editor *e) {
    int tinha_interface = e->largura_arvore > 0;
    int largura_arvore = e->largura_arvore, largura_inspetor = e->largura_inspetor,
        altura_inferior = e->altura_inferior, barra_recolhida = e->barra_lateral_recolhida,
        painel_aberto = e->painel_inferior_aberto, tema_avancado = e->tema_avancado;
    memset(e, 0, sizeof(*e));
    e->zoom = 32;
    e->tile = 1;
    e->entidade_selecionada = -1;
    e->evento_selecionado = 0;
    e->largura_arvore = tinha_interface ? largura_arvore : 235;
    e->largura_inspetor = tinha_interface ? largura_inspetor : 280;
    e->altura_inferior = tinha_interface ? altura_inferior : 150;
    e->barra_lateral_recolhida = barra_recolhida;
    e->painel_inferior_aberto = tinha_interface ? painel_aberto : 1;
    e->tema_avancado = tema_avancado;
}

static void novo_projeto(Aplicativo *a) {
    liberar_historico_global(&a->editor);
    liberar_recursos(a);
    liberar_projeto(&a->projeto);
    iniciar_projeto(&a->projeto);
    reiniciar_editor_preservando_interface(&a->editor);
    copiar_texto(a->caminho_projeto, sizeof(a->caminho_projeto), "projeto_mythara/projeto.myr");
    preparar_pasta_projeto(&a->projeto, a->caminho_projeto);
    definir_caminho_autosave(a);
    a->ultimo_autosalvamento = agora_segundos();
    a->oferecer_recuperacao = 0;
    a->projeto_carregado = 1;
    a->editor.alterado = 1;
    reiniciar_historico_global(a);
    mensagem_aplicativo(a, "Novo projeto Mythara v4 criado.");
}

static void novo_projeto_limpo(Aplicativo *a) {
    novo_projeto(a);
    Projeto *p = &a->projeto;
    for (int i = 0; i < p->quantidade_eventos; ++i) {
        free(p->eventos[i].comandos);
        p->eventos[i].comandos = NULL;
        p->eventos[i].quantidade_comandos = p->eventos[i].capacidade_comandos = 0;
    }
    p->quantidade_eventos = 0;
    for (int i = 0; i < p->quantidade_mapas; ++i) {
        p->mapas[i].quantidade_entidades = 0;
        p->mapas[i].regiao_encontro = -1;
    }
    p->quantidade_herois = 1;
    p->herois[0].classe = p->quantidade_classes ? 0 : -1;
    p->herois[0].sprite = -1;
    p->quantidade_habilidades = 0;
    for (int nivel = 0; nivel < 32; ++nivel)
        p->classes[0].habilidade_por_nivel[nivel] = -1;
    p->quantidade_itens = 0;
    p->quantidade_inimigos = 0;
    p->quantidade_estados = 0;
    p->quantidade_lojas = 0;
    p->quantidade_missoes = 0;
    p->quantidade_recursos = 0;
    liberar_historico_global(&a->editor);
    a->editor.alterado = 1;
    reiniciar_historico_global(a);
    mensagem_aplicativo(a, "Projeto limpo criado com mapa e heroi iniciais.");
}

static int salvar_projeto_atual(Aplicativo *a) {
    char erro[192];
    atribuir_ids_ausentes(&a->projeto);
    if (!validar_projeto(&a->projeto, erro, sizeof(erro))) {
        copiar_texto(a->erro_validacao, sizeof(a->erro_validacao), erro);
        a->modal = MODAL_VALIDACAO;
        mensagem_aplicativo(a, "Corrija os erros do projeto antes de salvar.");
        return 0;
    }
    if (!preparar_pasta_projeto(&a->projeto, a->caminho_projeto)) {
        mensagem_aplicativo(a, "Nao foi possivel preparar a pasta do projeto.");
        return 0;
    }
    if (salvar_projeto_em(&a->projeto, a->caminho_projeto, erro, sizeof(erro))) {
        char primeiro_aviso[160];
        int avisos = avisos_projeto(&a->projeto, primeiro_aviso, sizeof(primeiro_aviso));
        definir_caminho_autosave(a);
        copiar_texto(a->caminho_recente, sizeof(a->caminho_recente), a->caminho_projeto);
        a->ultimo_autosalvamento = agora_segundos();
        a->editor.alterado = 0;
        if (avisos) {
            char mensagem[256];
            snprintf(mensagem, sizeof(mensagem), "Projeto salvo com %d aviso(s): %s", avisos,
                     primeiro_aviso);
            mensagem_aplicativo(a, mensagem);
        } else
            mensagem_aplicativo(a, "Projeto salvo com sucesso; nenhum aviso.");
        return 1;
    }
    mensagem_aplicativo(a, erro);
    return 0;
}

static int abrir_projeto_atual(Aplicativo *a, const char *caminho) {
    char erro[192];
    if (versao_arquivo_projeto(caminho) == 3) {
        copiar_texto(a->caminho_migracao_origem, sizeof(a->caminho_migracao_origem), caminho);
        copiar_texto(a->caminho_migracao_destino, sizeof(a->caminho_migracao_destino), caminho);
        char *extensao = strrchr(a->caminho_migracao_destino, '.');
        if (extensao && !strcasecmp(extensao, ".myr"))
            *extensao = 0;
        strncat(a->caminho_migracao_destino, "-v4.myr",
                sizeof(a->caminho_migracao_destino) - strlen(a->caminho_migracao_destino) - 1);
        a->modal = MODAL_MIGRACAO;
        mensagem_aplicativo(a, "Projeto v3 detectado; confirme a criacao de uma copia v4.");
        return 1;
    }
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
    reiniciar_editor_preservando_interface(&a->editor);
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
    mensagem_aplicativo(a, "Projeto Mythara v4 carregado.");
    return 1;
}

typedef struct {
    EscritorBinario buffer;
    char caminho[MYTHARA_MAX_CAMINHO];
    atomic_int *em_andamento;
} TrabalhoAutosave;

/* Autosave assincrono ------------------------------------------------------- */

#ifdef _WIN32
static DWORD WINAPI executar_autosave_thread(LPVOID dados) {
    TrabalhoAutosave *t = dados;
    char erro[128];
    salvar_payload_v4(t->buffer.dados, t->buffer.tamanho, t->caminho, erro, sizeof(erro));
    escritor_binario_liberar(&t->buffer);
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
    char erro[128];
    salvar_payload_v4(t->buffer.dados, t->buffer.tamanho, t->caminho, erro, sizeof(erro));
    escritor_binario_liberar(&t->buffer);
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
        substituir_arquivo(origem, destino);
    }
    TrabalhoAutosave *trabalho = calloc(1, sizeof(*trabalho));
    if (!trabalho || !serializar_projeto_v4(&a->projeto, &trabalho->buffer)) {
        if (trabalho)
            escritor_binario_liberar(&trabalho->buffer);
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
        escritor_binario_liberar(&trabalho->buffer);
        free(trabalho);
        mensagem_aplicativo(a, "Nao foi possivel iniciar o autosave.");
    } else
        mensagem_aplicativo(a, "Snapshot v4 agendado em segundo plano.");
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

/* Criacao e consultas usadas pelo editor ----------------------------------- */

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

static int projeto_pronto_para_executar(Aplicativo *a) {
    atribuir_ids_ausentes(&a->projeto);
    if (validar_projeto(&a->projeto, a->erro_validacao, sizeof(a->erro_validacao))) {
        char primeiro[160];
        int avisos = avisos_projeto(&a->projeto, primeiro, sizeof(primeiro));
        if (avisos) {
            char mensagem[256];
            snprintf(mensagem, sizeof(mensagem), "Validacao concluida com %d aviso(s): %s", avisos,
                     primeiro);
            mensagem_aplicativo(a, mensagem);
        }
        return 1;
    }
    a->modal = MODAL_VALIDACAO;
    mensagem_aplicativo(a, "O validador encontrou um erro que bloqueia esta acao.");
    return 0;
}

static void iniciar_playtest_editor(Aplicativo *a) {
    if (!projeto_pronto_para_executar(a))
        return;
    memset(&a->jogo, 0, sizeof(a->jogo));
    iniciar_estado_jogo(&a->projeto, &a->jogo.estado);
    a->jogo.evento_ativo = -1;
    a->modo = MODO_JOGO;
}

static int registro_referenciado(const Projeto *p, TipoReferencia tipo, int indice, char *detalhe,
                                 size_t capacidade) {
#define REFERENCIA_ENCONTRADA(...)                                                                 \
    do {                                                                                           \
        snprintf(detalhe, capacidade, __VA_ARGS__);                                                \
        return 1;                                                                                  \
    } while (0)
    if (tipo == REFERENCIA_MAPA && p->mapa_inicial == indice)
        REFERENCIA_ENCONTRADA("O mapa e o mapa inicial do projeto.");
    for (int m = 0; m < p->quantidade_mapas; ++m) {
        const Mapa *mapa = &p->mapas[m];
        if (tipo == REFERENCIA_INIMIGO && mapa->regiao_encontro == indice)
            REFERENCIA_ENCONTRADA("O mapa %.48s usa este inimigo em encontros.", mapa->nome);
        if (tipo == REFERENCIA_EVENTO)
            for (int e = 0; e < mapa->quantidade_entidades; ++e)
                if (mapa->entidades[e].evento == indice)
                    REFERENCIA_ENCONTRADA("A entidade %.48s usa este evento.",
                                          mapa->entidades[e].nome);
    }
    for (int e = 0; e < p->quantidade_eventos; ++e)
        for (int c = 0; c < p->eventos[e].quantidade_comandos; ++c) {
            const ComandoEvento *comando = &p->eventos[e].comandos[c];
            int corresponde =
                (tipo == REFERENCIA_MAPA && comando->tipo == COMANDO_TELEPORTE) ||
                (tipo == REFERENCIA_EVENTO && comando->tipo == COMANDO_CHAMAR_EVENTO) ||
                (tipo == REFERENCIA_ITEM && comando->tipo == COMANDO_ITEM) ||
                (tipo == REFERENCIA_INIMIGO && comando->tipo == COMANDO_BATALHA) ||
                (tipo == REFERENCIA_RECURSO && comando->tipo == COMANDO_AUDIO) ||
                (tipo == REFERENCIA_LOJA && comando->tipo == COMANDO_LOJA) ||
                (tipo == REFERENCIA_MISSAO && comando->tipo == COMANDO_MISSAO);
            if (corresponde && comando->a == indice)
                REFERENCIA_ENCONTRADA("O evento %.48s usa este registro no comando %d.",
                                      p->eventos[e].nome, c + 1);
        }
    if (tipo == REFERENCIA_CLASSE)
        for (int i = 0; i < p->quantidade_herois; ++i)
            if (p->herois[i].classe == indice)
                REFERENCIA_ENCONTRADA("O heroi %.48s usa esta classe.", p->herois[i].nome);
    if (tipo == REFERENCIA_HABILIDADE)
        for (int i = 0; i < p->quantidade_classes; ++i)
            for (int nivel = 0; nivel < 32; ++nivel)
                if (p->classes[i].habilidade_por_nivel[nivel] == indice)
                    REFERENCIA_ENCONTRADA("A classe %.48s usa esta habilidade.",
                                          p->classes[i].nome);
    if (tipo == REFERENCIA_ESTADO || tipo == REFERENCIA_RECURSO)
        for (int i = 0; i < p->quantidade_habilidades; ++i)
            if ((tipo == REFERENCIA_ESTADO && p->habilidades[i].estado == indice) ||
                (tipo == REFERENCIA_RECURSO && p->habilidades[i].animacao == indice))
                REFERENCIA_ENCONTRADA("A habilidade %.48s usa este registro.",
                                      p->habilidades[i].nome);
    if (tipo == REFERENCIA_RECURSO)
        for (int i = 0; i < p->quantidade_herois; ++i)
            if (p->herois[i].sprite == indice)
                REFERENCIA_ENCONTRADA("O heroi %.48s usa este recurso.", p->herois[i].nome);
    if (tipo == REFERENCIA_ITEM)
        for (int i = 0; i < p->quantidade_lojas; ++i)
            for (int item = 0; item < p->lojas[i].quantidade_itens; ++item)
                if (p->lojas[i].itens[item] == indice)
                    REFERENCIA_ENCONTRADA("A loja %.48s vende este item.", p->lojas[i].nome);
    if (tipo == REFERENCIA_ITEM || tipo == REFERENCIA_INIMIGO)
        for (int i = 0; i < p->quantidade_missoes; ++i) {
            if (tipo == REFERENCIA_ITEM && p->missoes[i].recompensa_item == indice)
                REFERENCIA_ENCONTRADA("A missao %.48s recompensa este item.", p->missoes[i].nome);
            if (tipo == REFERENCIA_INIMIGO)
                for (int etapa = 0; etapa < p->missoes[i].quantidade_etapas; ++etapa)
                    if (p->missoes[i].etapas[etapa].tipo == 1 &&
                        p->missoes[i].etapas[etapa].alvo == indice)
                        REFERENCIA_ENCONTRADA("A missao %.48s usa este inimigo.",
                                              p->missoes[i].nome);
        }
#undef REFERENCIA_ENCONTRADA
    return 0;
}

/* Remocao segura e ajuste de indices --------------------------------------- */

static void ajustar_indice_removido(int *referencia, int removido) {
    if (*referencia > removido)
        (*referencia)--;
}

static void ajustar_referencias_apos_remocao(Projeto *p, TipoReferencia tipo, int removido) {
    if (tipo == REFERENCIA_MAPA)
        ajustar_indice_removido(&p->mapa_inicial, removido);
    for (int m = 0; m < p->quantidade_mapas; ++m) {
        if (tipo == REFERENCIA_INIMIGO)
            ajustar_indice_removido(&p->mapas[m].regiao_encontro, removido);
        if (tipo == REFERENCIA_EVENTO)
            for (int e = 0; e < p->mapas[m].quantidade_entidades; ++e) {
                ajustar_indice_removido(&p->mapas[m].entidades[e].evento, removido);
                if (p->mapas[m].entidades[e].evento >= 0)
                    p->mapas[m].entidades[e].evento_id =
                        p->eventos[p->mapas[m].entidades[e].evento].id;
            }
    }
    for (int e = 0; e < p->quantidade_eventos; ++e)
        for (int c = 0; c < p->eventos[e].quantidade_comandos; ++c) {
            ComandoEvento *comando = &p->eventos[e].comandos[c];
            int corresponde =
                (tipo == REFERENCIA_MAPA && comando->tipo == COMANDO_TELEPORTE) ||
                (tipo == REFERENCIA_EVENTO && comando->tipo == COMANDO_CHAMAR_EVENTO) ||
                (tipo == REFERENCIA_ITEM && comando->tipo == COMANDO_ITEM) ||
                (tipo == REFERENCIA_INIMIGO && comando->tipo == COMANDO_BATALHA) ||
                (tipo == REFERENCIA_RECURSO && comando->tipo == COMANDO_AUDIO) ||
                (tipo == REFERENCIA_LOJA && comando->tipo == COMANDO_LOJA) ||
                (tipo == REFERENCIA_MISSAO && comando->tipo == COMANDO_MISSAO);
            if (corresponde)
                ajustar_indice_removido(&comando->a, removido);
        }
    if (tipo == REFERENCIA_CLASSE)
        for (int i = 0; i < p->quantidade_herois; ++i)
            ajustar_indice_removido(&p->herois[i].classe, removido);
    if (tipo == REFERENCIA_HABILIDADE)
        for (int i = 0; i < p->quantidade_classes; ++i)
            for (int nivel = 0; nivel < 32; ++nivel)
                ajustar_indice_removido(&p->classes[i].habilidade_por_nivel[nivel], removido);
    for (int i = 0; i < p->quantidade_habilidades; ++i) {
        if (tipo == REFERENCIA_ESTADO)
            ajustar_indice_removido(&p->habilidades[i].estado, removido);
        if (tipo == REFERENCIA_RECURSO)
            ajustar_indice_removido(&p->habilidades[i].animacao, removido);
    }
    if (tipo == REFERENCIA_RECURSO)
        for (int i = 0; i < p->quantidade_herois; ++i)
            ajustar_indice_removido(&p->herois[i].sprite, removido);
    if (tipo == REFERENCIA_ITEM)
        for (int i = 0; i < p->quantidade_lojas; ++i)
            for (int item = 0; item < p->lojas[i].quantidade_itens; ++item)
                ajustar_indice_removido(&p->lojas[i].itens[item], removido);
    for (int i = 0; i < p->quantidade_missoes; ++i) {
        if (tipo == REFERENCIA_ITEM)
            ajustar_indice_removido(&p->missoes[i].recompensa_item, removido);
        if (tipo == REFERENCIA_INIMIGO)
            for (int etapa = 0; etapa < p->missoes[i].quantidade_etapas; ++etapa)
                if (p->missoes[i].etapas[etapa].tipo == 1)
                    ajustar_indice_removido(&p->missoes[i].etapas[etapa].alvo, removido);
    }
}

static int excluir_registro_banco(Aplicativo *a, int aba, int indice) {
    Projeto *p = &a->projeto;
    TipoReferencia tipos[] = {REFERENCIA_HEROI, REFERENCIA_CLASSE,  REFERENCIA_HABILIDADE,
                              REFERENCIA_ITEM,  REFERENCIA_INIMIGO, REFERENCIA_ESTADO,
                              REFERENCIA_LOJA,  REFERENCIA_MISSAO};
    int quantidades[] = {p->quantidade_herois, p->quantidade_classes,  p->quantidade_habilidades,
                         p->quantidade_itens,  p->quantidade_inimigos, p->quantidade_estados,
                         p->quantidade_lojas,  p->quantidade_missoes};
    if (aba < 0 || aba > 7 || indice < 0 || indice >= quantidades[aba] ||
        (aba == 0 && p->quantidade_herois <= 1))
        return 0;
    char detalhe[192];
    if (registro_referenciado(p, tipos[aba], indice, detalhe, sizeof(detalhe))) {
        mensagem_aplicativo(a, detalhe);
        return 0;
    }
#define REMOVER_FIXO(dados, quantidade, Tipo)                                                      \
    do {                                                                                           \
        memmove(&(dados)[indice], &(dados)[indice + 1],                                            \
                (size_t)((quantidade) - indice - 1) * sizeof(Tipo));                               \
        (quantidade)--;                                                                            \
        memset(&(dados)[quantidade], 0, sizeof(Tipo));                                             \
    } while (0)
    if (aba == 0)
        REMOVER_FIXO(p->herois, p->quantidade_herois, Heroi);
    else if (aba == 1)
        REMOVER_FIXO(p->classes, p->quantidade_classes, Classe);
    else if (aba == 2)
        REMOVER_FIXO(p->habilidades, p->quantidade_habilidades, Habilidade);
    else if (aba == 3) {
        memmove(&p->itens[indice], &p->itens[indice + 1],
                (size_t)(p->quantidade_itens - indice - 1) * sizeof(Item));
        p->quantidade_itens--;
        memset(&p->itens[p->quantidade_itens], 0, sizeof(Item));
    } else if (aba == 4) {
        memmove(&p->inimigos[indice], &p->inimigos[indice + 1],
                (size_t)(p->quantidade_inimigos - indice - 1) * sizeof(Inimigo));
        p->quantidade_inimigos--;
        memset(&p->inimigos[p->quantidade_inimigos], 0, sizeof(Inimigo));
    } else if (aba == 5)
        REMOVER_FIXO(p->estados, p->quantidade_estados, EstadoCombate);
    else if (aba == 6)
        REMOVER_FIXO(p->lojas, p->quantidade_lojas, Loja);
    else
        REMOVER_FIXO(p->missoes, p->quantidade_missoes, Missao);
#undef REMOVER_FIXO
    ajustar_referencias_apos_remocao(p, tipos[aba], indice);
    a->editor.registro_banco = limitar_int(indice, 0, quantidades[aba] - 2);
    a->editor.subregistro_banco = 0;
    a->editor.alterado = 1;
    mensagem_aplicativo(a, "Registro excluido com seguranca.");
    return 1;
}

static int excluir_evento_editor(Aplicativo *a, int indice) {
    Projeto *p = &a->projeto;
    if (indice < 0 || indice >= p->quantidade_eventos)
        return 0;
    char detalhe[192];
    if (registro_referenciado(p, REFERENCIA_EVENTO, indice, detalhe, sizeof(detalhe))) {
        mensagem_aplicativo(a, detalhe);
        return 0;
    }
    free(p->eventos[indice].comandos);
    memmove(&p->eventos[indice], &p->eventos[indice + 1],
            (size_t)(p->quantidade_eventos - indice - 1) * sizeof(Evento));
    p->quantidade_eventos--;
    memset(&p->eventos[p->quantidade_eventos], 0, sizeof(Evento));
    ajustar_referencias_apos_remocao(p, REFERENCIA_EVENTO, indice);
    a->editor.evento_selecionado =
        limitar_int(indice, 0, p->quantidade_eventos ? p->quantidade_eventos - 1 : 0);
    a->editor.comando_selecionado = 0;
    a->editor.alterado = 1;
    mensagem_aplicativo(a, "Evento excluido com seguranca.");
    return 1;
}

static int excluir_recurso_editor(Aplicativo *a, int indice) {
    Projeto *p = &a->projeto;
    if (indice < 0 || indice >= p->quantidade_recursos)
        return 0;
    char detalhe[192];
    if (registro_referenciado(p, REFERENCIA_RECURSO, indice, detalhe, sizeof(detalhe))) {
        mensagem_aplicativo(a, detalhe);
        return 0;
    }
    liberar_imagem(&a->imagens[indice]);
    memmove(&p->recursos[indice], &p->recursos[indice + 1],
            (size_t)(p->quantidade_recursos - indice - 1) * sizeof(Recurso));
    memmove(&a->imagens[indice], &a->imagens[indice + 1],
            (size_t)(p->quantidade_recursos - indice - 1) * sizeof(Imagem));
    p->quantidade_recursos--;
    memset(&p->recursos[p->quantidade_recursos], 0, sizeof(Recurso));
    memset(&a->imagens[p->quantidade_recursos], 0, sizeof(Imagem));
    ajustar_referencias_apos_remocao(p, REFERENCIA_RECURSO, indice);
    a->editor.recurso_selecionado =
        limitar_int(indice, 0, p->quantidade_recursos ? p->quantidade_recursos - 1 : 0);
    a->editor.alterado = 1;
    mensagem_aplicativo(a, "Registro removido; o arquivo do recurso foi preservado.");
    return 1;
}
