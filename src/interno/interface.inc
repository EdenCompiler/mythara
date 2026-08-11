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

/* Estado e identificacao dos controles ------------------------------------- */

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
