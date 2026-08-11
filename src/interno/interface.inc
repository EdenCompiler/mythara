/*
 * Modulo: interface
 * Responsabilidade: tema, entrada logica e controles de UI imediata.
 * Depende de: base e renderizador.
 */

/* Tema ---------------------------------------------------------------------- */

typedef struct {
    char nome[48];
    Cor fundo, painel, painel_elevado, controle, controle_sobre, selecao, borda, texto, texto_suave,
        destaque, perigo, aviso, sucesso;
    Cor barra, lateral, lateral_sobre, canvas, sobreposicao, foco, desabilitado;
    int escala_percentual, espacamento, altura_controle, espessura_borda, raio_visual;
    int preset, movimento_reduzido;
} TemaInterface;

static TemaInterface tema_ativo;

typedef enum { TEMA_ARCANO, TEMA_PERGAMINHO, TEMA_PERSONALIZADO } PresetTema;

static void iniciar_tema_arcano(void) {
    memset(&tema_ativo, 0, sizeof(tema_ativo));
    copiar_texto(tema_ativo.nome, sizeof(tema_ativo.nome), "Arcano Noturno");
    tema_ativo.fundo = cor_rgb(12, 13, 22);
    tema_ativo.painel = cor_rgb(22, 24, 38);
    tema_ativo.painel_elevado = cor_rgb(31, 34, 52);
    tema_ativo.controle = cor_rgb(40, 43, 64);
    tema_ativo.controle_sobre = cor_rgb(55, 52, 83);
    tema_ativo.selecao = cor_rgb(73, 56, 118);
    tema_ativo.borda = cor_rgb(72, 68, 99);
    tema_ativo.texto = cor_rgb(242, 236, 221);
    tema_ativo.texto_suave = cor_rgb(174, 166, 187);
    tema_ativo.destaque = cor_rgb(183, 132, 255);
    tema_ativo.perigo = cor_rgb(225, 82, 112);
    tema_ativo.aviso = cor_rgb(225, 174, 79);
    tema_ativo.sucesso = cor_rgb(92, 192, 151);
    tema_ativo.barra = cor_rgb(17, 18, 30);
    tema_ativo.lateral = cor_rgb(18, 19, 32);
    tema_ativo.lateral_sobre = cor_rgb(46, 40, 68);
    tema_ativo.canvas = cor_rgb(9, 10, 17);
    tema_ativo.sobreposicao = cor_rgb(8, 9, 15);
    tema_ativo.foco = cor_rgb(235, 190, 104);
    tema_ativo.desabilitado = cor_rgb(82, 81, 96);
    tema_ativo.escala_percentual = 100;
    tema_ativo.espacamento = 7;
    tema_ativo.altura_controle = 29;
    tema_ativo.espessura_borda = 1;
    tema_ativo.raio_visual = 3;
    tema_ativo.preset = TEMA_ARCANO;
}

static void iniciar_tema_pergaminho(void) {
    memset(&tema_ativo, 0, sizeof(tema_ativo));
    copiar_texto(tema_ativo.nome, sizeof(tema_ativo.nome), "Pergaminho Dourado");
    tema_ativo.fundo = cor_rgb(211, 195, 160);
    tema_ativo.painel = cor_rgb(231, 218, 184);
    tema_ativo.painel_elevado = cor_rgb(244, 233, 204);
    tema_ativo.controle = cor_rgb(218, 199, 158);
    tema_ativo.controle_sobre = cor_rgb(232, 211, 164);
    tema_ativo.selecao = cor_rgb(190, 155, 104);
    tema_ativo.borda = cor_rgb(116, 88, 65);
    tema_ativo.texto = cor_rgb(48, 35, 32);
    tema_ativo.texto_suave = cor_rgb(100, 77, 66);
    tema_ativo.destaque = cor_rgb(116, 62, 126);
    tema_ativo.perigo = cor_rgb(157, 49, 57);
    tema_ativo.aviso = cor_rgb(166, 103, 31);
    tema_ativo.sucesso = cor_rgb(50, 116, 78);
    tema_ativo.barra = cor_rgb(67, 43, 50);
    tema_ativo.lateral = cor_rgb(78, 51, 56);
    tema_ativo.lateral_sobre = cor_rgb(105, 69, 69);
    tema_ativo.canvas = cor_rgb(174, 157, 127);
    tema_ativo.sobreposicao = cor_rgb(50, 35, 39);
    tema_ativo.foco = cor_rgb(116, 62, 126);
    tema_ativo.desabilitado = cor_rgb(149, 135, 115);
    tema_ativo.escala_percentual = 100;
    tema_ativo.espacamento = 7;
    tema_ativo.altura_controle = 29;
    tema_ativo.espessura_borda = 1;
    tema_ativo.raio_visual = 3;
    tema_ativo.preset = TEMA_PERGAMINHO;
}

static void iniciar_tema_padrao(void) {
    iniciar_tema_arcano();
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
    int escopo_id;
    const char *rotulo_quente;
    int primeiro_foco, ultimo_foco, foco_anterior;
    int navegar_frente, navegar_tras, aguarda_proximo;
    int campo_texto_focado;
} Interface;

/* Estado e identificacao dos controles ------------------------------------- */

typedef struct {
    int foco;
    size_t cursor, ancora, rolagem;
} EstadoCampoTexto;
static EstadoCampoTexto estado_campo_texto;
static int plataforma_clipboard_copiar(const char *texto);
static int plataforma_clipboard_colar(char *destino, size_t capacidade);

static int ui_novo_id(Interface *ui) {
    return ((ui->escopo_id + 1) << 12) | (++ui->proximo_id & 4095);
}

static int ui_proximo_id_previsto(const Interface *ui) {
    return ((ui->escopo_id + 1) << 12) | ((ui->proximo_id + 1) & 4095);
}

static void ui_definir_escopo(Interface *ui, int escopo) {
    ui->escopo_id = escopo;
    ui->proximo_id = 0;
}

typedef enum {
    ICONE_MENU,
    ICONE_MAPA,
    ICONE_EVENTO,
    ICONE_BANCO,
    ICONE_RECURSO,
    ICONE_TEMA,
    ICONE_PROJETO,
    ICONE_AJUDA,
    ICONE_JOGAR,
    ICONE_SALVAR
} IconeInterface;

static const uint8_t icones_interface[][8] = {
    {0x00, 0x7e, 0x00, 0x7e, 0x00, 0x7e, 0x00, 0x00},
    {0x00, 0x7e, 0x42, 0x5a, 0x5a, 0x42, 0x7e, 0x00},
    {0x18, 0x3c, 0x7e, 0x5a, 0x7e, 0x3c, 0x18, 0x00},
    {0x7e, 0x42, 0x5e, 0x52, 0x5e, 0x42, 0x7e, 0x00},
    {0x3c, 0x42, 0x5a, 0x5a, 0x42, 0x3c, 0x18, 0x18},
    {0x18, 0x5a, 0x3c, 0x7e, 0x3c, 0x5a, 0x18, 0x00},
    {0x3c, 0x42, 0x5e, 0x52, 0x5e, 0x42, 0x3c, 0x00},
    {0x18, 0x00, 0x18, 0x18, 0x18, 0x00, 0x18, 0x00},
    {0x20, 0x30, 0x38, 0x3c, 0x38, 0x30, 0x20, 0x00},
    {0x7e, 0x52, 0x52, 0x7e, 0x42, 0x42, 0x7e, 0x00},
};

static void ui_icone(Tela *t, int x, int y, IconeInterface icone, Cor cor) {
    const uint8_t *linhas = icones_interface[limitar_int(icone, 0, ICONE_SALVAR)];
    for (int linha = 0; linha < 8; ++linha)
        for (int coluna = 0; coluna < 8; ++coluna)
            if (linhas[linha] & (1u << (7 - coluna)))
                preencher_retangulo(t, (Retangulo){x + coluna * 2, y + linha * 2, 2, 2}, cor);
}

typedef struct {
    int id;
    int intensidade;
} AnimacaoControle;

static AnimacaoControle animacoes_controles[128];
static int ultimo_foco_quadro;

static Cor misturar_cor(Cor a, Cor b, int quantidade) {
    quantidade = limitar_int(quantidade, 0, 255);
    unsigned ar = (a >> 16) & 255, ag = (a >> 8) & 255, ab = a & 255;
    unsigned br = (b >> 16) & 255, bg = (b >> 8) & 255, bb = b & 255;
    unsigned r = (ar * (255 - quantidade) + br * quantidade) / 255;
    unsigned g = (ag * (255 - quantidade) + bg * quantidade) / 255;
    unsigned azul = (ab * (255 - quantidade) + bb * quantidade) / 255;
    return cor_rgb(r, g, azul);
}

static int ui_animar_controle(int id, int ativo) {
    AnimacaoControle *livre = &animacoes_controles[0];
    for (size_t i = 0; i < sizeof(animacoes_controles) / sizeof(animacoes_controles[0]); ++i) {
        if (animacoes_controles[i].id == id) {
            livre = &animacoes_controles[i];
            break;
        }
        if (!animacoes_controles[i].id)
            livre = &animacoes_controles[i];
    }
    livre->id = id;
    if (tema_ativo.movimento_reduzido)
        livre->intensidade = ativo ? 255 : 0;
    else if (ativo)
        livre->intensidade = limitar_int(livre->intensidade + 34, 0, 255);
    else
        livre->intensidade = limitar_int(livre->intensidade - 34, 0, 255);
    return livre->intensidade;
}

static int ui_registrar_foco(Interface *ui, int id, int sobre) {
    if (!ui->primeiro_foco)
        ui->primeiro_foco = id;
    if (ui->navegar_tras && ui->id_foco == id)
        ui->id_foco = ui->foco_anterior ? ui->foco_anterior : ultimo_foco_quadro;
    if (ui->aguarda_proximo) {
        ui->id_foco = id;
        ui->aguarda_proximo = 0;
    } else if (ui->navegar_frente && ui->id_foco == id)
        ui->aguarda_proximo = 1;
    if (sobre && ui->entrada->mouse_pressionado)
        ui->id_foco = id;
    ui->foco_anterior = id;
    ui->ultimo_foco = id;
    return ui->id_foco == id;
}

static void ui_preparar_quadro(Interface *ui) {
    ui->navegar_frente = ui->entrada->teclas_pressionadas[9] && !ui->entrada->shift;
    ui->navegar_tras = ui->entrada->teclas_pressionadas[9] && ui->entrada->shift;
    if ((ui->navegar_frente || ui->navegar_tras) && !ui->id_foco)
        ui->aguarda_proximo = 1;
}

static void ui_finalizar_quadro(Interface *ui) {
    if (ui->aguarda_proximo && ui->primeiro_foco)
        ui->id_foco = ui->primeiro_foco;
    ultimo_foco_quadro = ui->ultimo_foco;
}

static int ui_botao(Interface *ui, Retangulo r, const char *rotulo, int selecionado) {
    int id = ui_novo_id(ui);
    int sobre = ponto_em_retangulo(ui->entrada->mouse_x, ui->entrada->mouse_y, r);
    int focado = ui_registrar_foco(ui, id, sobre);
    Cor fundo = selecionado ? tema_ativo.selecao : tema_ativo.controle;
    if (sobre) {
        ui->id_quente = id;
        ui->rotulo_quente = rotulo;
        fundo = misturar_cor(fundo, tema_ativo.controle_sobre, ui_animar_controle(id, 1));
    } else {
        fundo = misturar_cor(fundo, tema_ativo.controle_sobre, ui_animar_controle(id, 0));
    }
    if (sobre && ui->entrada->mouse_pressionado)
        ui->id_ativo = id;
    preencher_retangulo(ui->tela, r, fundo);
    contornar_retangulo(
        ui->tela, r, focado ? tema_ativo.foco : (sobre ? tema_ativo.destaque : tema_ativo.borda));
    desenhar_texto(ui->tela, r.x + 7, r.y + (r.altura - 7) / 2, rotulo, tema_ativo.texto, 1);
    return (sobre && ui->entrada->mouse_pressionado) ||
           (focado &&
            (ui->entrada->teclas_pressionadas[13] || ui->entrada->teclas_pressionadas[32]));
}

static int ui_botao_icone(Interface *ui, Retangulo r, IconeInterface icone, const char *rotulo,
                          int selecionado, int mostrar_rotulo) {
    int acionado = ui_botao(ui, r, "", selecionado);
    ui_icone(ui->tela, r.x + 9, r.y + (r.altura - 16) / 2, icone, tema_ativo.texto);
    if (mostrar_rotulo)
        desenhar_texto_forte(ui->tela, r.x + 34, r.y + (r.altura - 7) / 2, rotulo, tema_ativo.texto,
                             1);
    if (ponto_em_retangulo(ui->entrada->mouse_x, ui->entrada->mouse_y, r))
        ui->rotulo_quente = rotulo;
    return acionado;
}

/* Edicao de texto UTF-8 ----------------------------------------------------- */

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

static uint32_t resumo_texto_interface(const char *texto) {
    uint32_t resumo = UINT32_C(2166136261);
    while (*texto) {
        resumo ^= (unsigned char)*texto++;
        resumo *= UINT32_C(16777619);
    }
    return resumo;
}

static int ui_campo_base(Interface *ui, Retangulo r, char *texto, size_t capacidade,
                         int multilinha) {
    uint32_t resumo_anterior = resumo_texto_interface(texto);
    int id = ui_novo_id(ui);
    int sobre = ponto_em_retangulo(ui->entrada->mouse_x, ui->entrada->mouse_y, r);
    int focado = ui_registrar_foco(ui, id, sobre);
    if (sobre && ui->entrada->mouse_pressionado) {
        ui->id_foco = id;
        estado_campo_texto.foco = id;
        estado_campo_texto.cursor = estado_campo_texto.ancora = strlen(texto);
        estado_campo_texto.rolagem = 0;
    }
    preencher_retangulo(ui->tela, r, tema_ativo.fundo);
    contornar_retangulo(ui->tela, r, focado ? tema_ativo.foco : tema_ativo.borda);
    if (ui->id_foco == id) {
        ui->campo_texto_focado = 1;
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
        return resumo_texto_interface(texto) != resumo_anterior;
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

/* Controles compostos ------------------------------------------------------- */

static int ui_numero(Interface *ui, Retangulo r, const char *rotulo, int valor, int minimo,
                     int maximo) {
    char texto[64];
    snprintf(texto, sizeof(texto), "%s: %d", idioma_traduzir(rotulo), valor);
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
    int focado = ui_registrar_foco(ui, id, sobre);
    preencher_retangulo(ui->tela, caixa, tema_ativo.fundo);
    contornar_retangulo(ui->tela, caixa,
                        focado ? tema_ativo.foco
                               : (sobre ? tema_ativo.destaque : tema_ativo.borda));
    if (valor) {
        preencher_retangulo(ui->tela, (Retangulo){caixa.x + 4, caixa.y + 4, 10, 10},
                            tema_ativo.destaque);
    }
    desenhar_texto(ui->tela, r.x + 25, r.y + 5, rotulo, tema_ativo.texto, 1);
    if ((sobre && ui->entrada->mouse_pressionado) ||
        (focado &&
         (ui->entrada->teclas_pressionadas[13] || ui->entrada->teclas_pressionadas[32]))) {
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
