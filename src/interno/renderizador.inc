/*
 * Modulo: renderizador
 * Responsabilidade: framebuffer ARGB, primitivas 2D, fonte bitmap e tiles.
 * Depende de: tipos geometricos de base e idioma ativo.
 */

/* Framebuffer e primitivas -------------------------------------------------- */

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

static void copiar_recorte_tela(const Tela *origem, Tela *destino, Retangulo fonte, int destino_x,
                                int destino_y) {
    for (int y = 0; y < fonte.altura; ++y) {
        int sy = fonte.y + y, dy = destino_y + y;
        if (sy < 0 || sy >= origem->altura || dy < 0 || dy >= destino->altura)
            continue;
        for (int x = 0; x < fonte.largura; ++x) {
            int sx = fonte.x + x, dx = destino_x + x;
            if (sx >= 0 && sx < origem->largura && dx >= 0 && dx < destino->largura)
                destino->pixels[(size_t)dy * (size_t)destino->largura + (size_t)dx] =
                    origem->pixels[(size_t)sy * (size_t)origem->largura + (size_t)sx];
        }
    }
}

/* Fonte bitmap -------------------------------------------------------------- */
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

static int acento_codigo(unsigned c) {
    if (c == 0x00c1 || c == 0x00c9 || c == 0x00cd || c == 0x00d3 || c == 0x00da || c == 0x00e1 ||
        c == 0x00e9 || c == 0x00ed || c == 0x00f3 || c == 0x00fa)
        return 1;
    if (c == 0x00c0 || c == 0x00c8 || c == 0x00cc || c == 0x00d2 || c == 0x00d9 || c == 0x00e0 ||
        c == 0x00e8 || c == 0x00ec || c == 0x00f2 || c == 0x00f9)
        return 2;
    if (c == 0x00c2 || c == 0x00ca || c == 0x00d4 || c == 0x00e2 || c == 0x00ea || c == 0x00f4)
        return 3;
    if (c == 0x00c3 || c == 0x00d5 || c == 0x00e3 || c == 0x00f5 || c == 0x00d1 || c == 0x00f1)
        return 4;
    if (c == 0x00c7 || c == 0x00e7)
        return 5;
    return 0;
}

static void desenhar_texto_estilo(Tela *t, int x, int y, const char *texto, Cor cor, int escala,
                                  int forte) {
    texto = idioma_traduzir(texto);
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
        int acento = acento_codigo(codigo), deslocamento = acento && acento != 5 ? escala : 0;
        if (acento == 1)
            preencher_retangulo(t, (Retangulo){x + 3 * escala, y, escala, escala}, cor);
        else if (acento == 2)
            preencher_retangulo(t, (Retangulo){x + escala, y, escala, escala}, cor);
        else if (acento == 3) {
            preencher_retangulo(t, (Retangulo){x + escala, y, escala, escala}, cor);
            preencher_retangulo(t, (Retangulo){x + 3 * escala, y, escala, escala}, cor);
        } else if (acento == 4) {
            preencher_retangulo(t, (Retangulo){x + escala, y, escala, escala}, cor);
            preencher_retangulo(t, (Retangulo){x + 2 * escala, y, escala, escala}, cor);
        }
        for (int l = 0; l < 7; ++l)
            for (int coluna = 0; coluna < 5; ++coluna)
                if (linhas[l] & (1u << (4 - coluna))) {
                    preencher_retangulo(t,
                                        (Retangulo){x + coluna * escala,
                                                    y + deslocamento + l * escala, escala, escala},
                                        cor);
                    if (forte && coluna < 4)
                        preencher_retangulo(t,
                                            (Retangulo){x + coluna * escala + escala,
                                                        y + deslocamento + l * escala, escala,
                                                        escala},
                                            cor);
                }
        if (acento == 5) {
            preencher_retangulo(t, (Retangulo){x + 2 * escala, y + 7 * escala, escala, escala},
                                cor);
            preencher_retangulo(t, (Retangulo){x + escala, y + 8 * escala, escala, escala}, cor);
        }
        x += 6 * escala;
    }
}

static void desenhar_texto(Tela *t, int x, int y, const char *texto, Cor cor, int escala) {
    desenhar_texto_estilo(t, x, y, texto, cor, escala, 0);
}

static void desenhar_texto_forte(Tela *t, int x, int y, const char *texto, Cor cor, int escala) {
    desenhar_texto_estilo(t, x, y, texto, cor, escala, 1);
}

/* Visualizacao de tiles ----------------------------------------------------- */

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
