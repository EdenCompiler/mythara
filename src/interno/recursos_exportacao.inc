/*
 * Modulo: recursos e exportacao
 * Responsabilidade: imagens, audio, importacao segura e pacote distribuivel.
 * Formatos incorporados: BMP, QOI, TGA e WAV PCM.
 */

/* Imagens e leitores binarios ---------------------------------------------- */

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

/* Ciclo de vida e desenho de imagens --------------------------------------- */

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
/* Reproducao WAV ------------------------------------------------------------ */

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

/* Importacao e exportacao do projeto --------------------------------------- */

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
            if (!caminho_relativo_seguro(copia.recursos[i].caminho)) {
                snprintf(erro, cap_erro, "Recurso fora do projeto: %.150s.", base);
                free(copia.recursos);
                return 0;
            }
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
