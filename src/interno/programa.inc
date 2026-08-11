/*
 * Modulo: programa
 * Responsabilidade: verificacao integrada, CLI e ponto de entrada.
 * Este e o ultimo modulo da unidade de composicao.
 */

/* Fixtures e autotestes ----------------------------------------------------- */

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
    FILE *arquivo_v4 = fopen(caminho, "rb");
    char assinatura_v4[8] = {0};
    int assinatura_portatil = arquivo_v4 &&
                              fread(assinatura_v4, sizeof(assinatura_v4), 1, arquivo_v4) == 1 &&
                              !memcmp(assinatura_v4, "MYTHRV4", 7);
    if (arquivo_v4)
        fclose(arquivo_v4);
    VERIFICAR(assinatura_portatil, "cabecalho portatil do projeto v4");
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
    char caminho_v3[160];
    snprintf(caminho_v3, sizeof(caminho_v3), "/tmp/mythara_legado_%ld.myr", (long)getpid());
    Projeto legado = {0};
    int importou_v3 = salvar_projeto_v3_em(&p, caminho_v3, erro, sizeof(erro)) &&
                      carregar_projeto_de(&legado, caminho_v3, erro, sizeof(erro));
    VERIFICAR(importou_v3 && legado.id && !strcmp(legado.nome, p.nome),
              "ler projeto v3 para migracao explicita");
    liberar_projeto(&legado);
    unlink(caminho_v3);
    FILE *com_bloco_opcional = fopen(caminho, "r+b");
    if (com_bloco_opcional) {
        uint8_t dois_blocos[4] = {2, 0, 0, 0};
        fseek(com_bloco_opcional, 12, SEEK_SET);
        fwrite(dois_blocos, sizeof(dois_blocos), 1, com_bloco_opcional);
        fseek(com_bloco_opcional, 0, SEEK_END);
        EscritorBinario bloco_opcional = {0};
        escritor_binario_bytes(&bloco_opcional, "TEST", 4);
        escritor_binario_u32(&bloco_opcional, 0);
        escritor_binario_u32(&bloco_opcional, 1);
        escritor_binario_u64(&bloco_opcional, 0);
        escritor_binario_u32(&bloco_opcional, crc32_binario(NULL, 0));
        fwrite(bloco_opcional.dados, bloco_opcional.tamanho, 1, com_bloco_opcional);
        escritor_binario_liberar(&bloco_opcional);
        fclose(com_bloco_opcional);
    }
    Projeto com_extensao = {0};
    VERIFICAR(carregar_projeto_de(&com_extensao, caminho, erro, sizeof(erro)),
              "ignorar chunk v4 opcional desconhecido");
    liberar_projeto(&com_extensao);
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
                  strstr(erro, "nao reconhecido") != NULL,
              "rejeitar formato v2 com mensagem clara");
    unlink(caminho);
    iniciar_estado_jogo(&p, &j);
    snprintf(caminho, sizeof(caminho), "/tmp/mythara_teste_%ld.mys", (long)getpid());
    VERIFICAR(salvar_estado(&p, &j, caminho), "salvar estado portatil do jogo");
    memset(&k, 0, sizeof(k));
    VERIFICAR(carregar_estado(&p, &k, caminho) && k.vida == j.vida,
              "carregar estado portatil do jogo");
    Projeto projeto_alheio;
    iniciar_projeto(&projeto_alheio);
    VERIFICAR(!carregar_estado(&projeto_alheio, &k, caminho),
              "rejeitar save pertencente a outro projeto");
    liberar_projeto(&projeto_alheio);
    unlink(caminho);
    snprintf(caminho, sizeof(caminho), "/tmp/mythara_tema_%ld.myt", (long)getpid());
    Cor fundo_tema = tema_ativo.fundo;
    int tema_portatil = salvar_tema_arquivo(caminho);
    FILE *arquivo_tema_v5 = fopen(caminho, "rb");
    char assinatura_tema_v5[8] = {0};
    tema_portatil =
        tema_portatil && arquivo_tema_v5 &&
        fread(assinatura_tema_v5, sizeof(assinatura_tema_v5), 1, arquivo_tema_v5) == 1 &&
        !memcmp(assinatura_tema_v5, "MYTTEM5", 7);
    if (arquivo_tema_v5)
        fclose(arquivo_tema_v5);
    tema_ativo.fundo = 0;
    tema_portatil =
        tema_portatil && carregar_tema_arquivo(caminho) && tema_ativo.fundo == fundo_tema;
    VERIFICAR(tema_portatil, "tema v5 semantico e portatil com CRC32");
    unlink(caminho);
    snprintf(caminho, sizeof(caminho), "/tmp/mythara_tema_v4_%ld.myt", (long)getpid());
    EscritorBinario tema_legado = {0};
    serializar_tema(&tema_legado, &tema_ativo);
    const char magia_tema_v4[8] = {'M', 'Y', 'T', 'T', 'E', 'M', '4', '\0'};
    int escreveu_tema_v4 = salvar_documento_portatil(magia_tema_v4, 4, &tema_legado, caminho);
    escritor_binario_liberar(&tema_legado);
    VERIFICAR(escreveu_tema_v4 && !carregar_tema_arquivo(caminho) && tema_arquivo_legado,
              "rejeitar tema v4 com diagnostico de incompatibilidade");
    unlink(caminho);
    snprintf(caminho, sizeof(caminho), "/tmp/mythara_config_v4_%ld.myc", (long)getpid());
    EscritorBinario config_legada = {0};
    escritor_binario_i32(&config_legada, 235);
    const char magia_config_v4[8] = {'M', 'Y', 'T', 'C', 'F', 'G', '4', '\0'};
    const char magia_config_v5[8] = {'M', 'Y', 'T', 'C', 'F', 'G', '5', '\0'};
    int escreveu_config_v4 = salvar_documento_portatil(magia_config_v4, 4, &config_legada, caminho);
    escritor_binario_liberar(&config_legada);
    uint8_t *config_rejeitada = NULL;
    size_t tamanho_config_rejeitada = 0;
    VERIFICAR(escreveu_config_v4 &&
                  !carregar_documento_portatil(magia_config_v5, 5, caminho, &config_rejeitada,
                                               &tamanho_config_rejeitada),
              "ignorar configuracao local v4 no carregador v5");
    free(config_rejeitada);
    unlink(caminho);
    EscritorBinario numeros = {0};
    escritor_binario_u32(&numeros, UINT32_C(0x78563412));
    LeitorBinario leitor_numeros = {numeros.dados, numeros.tamanho, 0, 0};
    VERIFICAR(numeros.tamanho == 4 && numeros.dados[0] == 0x12 &&
                  leitor_binario_u32(&leitor_numeros) == UINT32_C(0x78563412),
              "codec little-endian independente da arquitetura");
    escritor_binario_liberar(&numeros);
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
              "persistir mapa grande, camadas e IDs v4");
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
        VERIFICAR(criou && girou && recuperou, "autosave v4 rotativo, assincrono e recuperavel");
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
    uint32_t pixels_referencia[96 * 40] = {0};
    Tela tela_referencia = {pixels_referencia, 96, 40};
    Entrada clique = {0};
    clique.mouse_x = 14;
    clique.mouse_y = 12;
    clique.mouse_pressionado = 1;
    Interface interface_referencia = {.tela = &tela_referencia, .entrada = &clique};
    idioma_definir(IDIOMA_PORTUGUES);
    limpar_tela(&tela_referencia, cor_rgb(18, 22, 30));
    int acionou = ui_botao(&interface_referencia, (Retangulo){8, 6, 80, 24}, "SALVAR", 0);
    uint32_t quadro_referencia = crc32_binario(pixels_referencia, sizeof(pixels_referencia));
    VERIFICAR(acionou && quadro_referencia == UINT32_C(0xe0bbb475),
              "interacao headless e framebuffer de referencia");
    Entrada teclado = {0};
    teclado.teclas_pressionadas[9] = 1;
    Interface interface_teclado = {.tela = &tela_referencia, .entrada = &teclado};
    ui_preparar_quadro(&interface_teclado);
    ui_botao(&interface_teclado, (Retangulo){8, 6, 80, 24}, "SALVAR", 0);
    ui_finalizar_quadro(&interface_teclado);
    teclado.teclas_pressionadas[9] = 0;
    teclado.teclas_pressionadas[13] = 1;
    Interface interface_ativacao = {
        .tela = &tela_referencia, .entrada = &teclado, .id_foco = interface_teclado.id_foco};
    ui_preparar_quadro(&interface_ativacao);
    int acionou_teclado = ui_botao(&interface_ativacao, (Retangulo){8, 6, 80, 24}, "SALVAR", 0);
    VERIFICAR(interface_teclado.id_foco != 0 && acionou_teclado,
              "navegacao por Tab e ativacao por teclado");
    Aplicativo *visual = calloc(1, sizeof(*visual));
    uint32_t *pixels_editor = calloc((size_t)1280 * 720, sizeof(*pixels_editor));
    int referencias_ui = visual && pixels_editor && clonar_projeto(&p, &visual->projeto);
    uint32_t crc_arcano = 0, crc_pergaminho = 0, crc_tema_compacto = 0, crc_banco_compacto = 0,
             crc_banco_rolado = 0;
    if (referencias_ui) {
        visual->editor.zoom = 32;
        visual->editor.tile = 1;
        visual->editor.entidade_selecionada = -1;
        visual->editor.largura_arvore = 235;
        visual->editor.largura_inspetor = 280;
        visual->editor.altura_inferior = 150;
        visual->editor.painel_inferior_aberto = 1;
        copiar_texto(visual->caminho_projeto, sizeof(visual->caminho_projeto),
                     "projeto_mythara/projeto.myr");
        iniciar_tema_arcano();
        Tela tela_editor = {pixels_editor, 1280, 720};
        Entrada entrada_editor = {0};
        Interface ui_editor = {.tela = &tela_editor, .entrada = &entrada_editor};
        ui_preparar_quadro(&ui_editor);
        limpar_tela(&tela_editor, tema_ativo.fundo);
        desenhar_editor(visual, &ui_editor);
        ui_finalizar_quadro(&ui_editor);
        crc_arcano = crc32_binario(pixels_editor, (size_t)1280 * 720 * sizeof(*pixels_editor));

        free(pixels_editor);
        pixels_editor = calloc((size_t)640 * 360, sizeof(*pixels_editor));
        referencias_ui = pixels_editor != NULL;
        if (referencias_ui) {
            iniciar_tema_pergaminho();
            visual->editor.layout_ativo = LAYOUT_PROJETO;
            visual->editor.largura_barra_animada = 0;
            Tela tela_compacta = {pixels_editor, 640, 360};
            Interface ui_compacta = {.tela = &tela_compacta, .entrada = &entrada_editor};
            ui_preparar_quadro(&ui_compacta);
            limpar_tela(&tela_compacta, tema_ativo.fundo);
            desenhar_editor(visual, &ui_compacta);
            ui_finalizar_quadro(&ui_compacta);
            crc_pergaminho =
                crc32_binario(pixels_editor, (size_t)640 * 360 * sizeof(*pixels_editor));
            visual->editor.layout_ativo = LAYOUT_TEMA;
            Interface ui_tema_compacto = {.tela = &tela_compacta, .entrada = &entrada_editor};
            ui_preparar_quadro(&ui_tema_compacto);
            limpar_tela(&tela_compacta, tema_ativo.fundo);
            desenhar_editor(visual, &ui_tema_compacto);
            ui_finalizar_quadro(&ui_tema_compacto);
            crc_tema_compacto =
                crc32_binario(pixels_editor, (size_t)640 * 360 * sizeof(*pixels_editor));
            visual->editor.layout_ativo = LAYOUT_BANCO;
            Interface ui_banco_compacto = {.tela = &tela_compacta, .entrada = &entrada_editor};
            ui_preparar_quadro(&ui_banco_compacto);
            limpar_tela(&tela_compacta, tema_ativo.fundo);
            desenhar_editor(visual, &ui_banco_compacto);
            ui_finalizar_quadro(&ui_banco_compacto);
            crc_banco_compacto =
                crc32_binario(pixels_editor, (size_t)640 * 360 * sizeof(*pixels_editor));
            visual->editor.rolagem_workspace_x[LAYOUT_BANCO] = 192;
            visual->editor.rolagem_workspace_y[LAYOUT_BANCO] = 128;
            Interface ui_banco_rolado = {.tela = &tela_compacta, .entrada = &entrada_editor};
            ui_preparar_quadro(&ui_banco_rolado);
            limpar_tela(&tela_compacta, tema_ativo.fundo);
            desenhar_editor(visual, &ui_banco_rolado);
            ui_finalizar_quadro(&ui_banco_rolado);
            crc_banco_rolado =
                crc32_binario(pixels_editor, (size_t)640 * 360 * sizeof(*pixels_editor));
        }
    }
    VERIFICAR(referencias_ui && crc_arcano == UINT32_C(0x2f3a4ee0) &&
                  crc_pergaminho == UINT32_C(0xafc638df) &&
                  crc_tema_compacto == UINT32_C(0xb26fc8d9),
              "framebuffers completos dos temas em 1280x720 e 640x360");
    VERIFICAR(referencias_ui && crc_banco_compacto != 0 && crc_banco_compacto != crc_banco_rolado,
              "workspace compacto do banco permite rolagem bidimensional");
    int navegou_barra = 0;
    if (visual && pixels_editor) {
        Entrada seta_barra = {0};
        seta_barra.teclas_pressionadas[131] = 1;
        Tela tela_barra = {pixels_editor, 640, 360};
        Interface ui_barra = {
            .tela = &tela_barra, .entrada = &seta_barra, .id_foco = (3 << 12) | 2};
        editor_barra_lateral(visual, &ui_barra);
        navegou_barra = ui_barra.id_foco == ((3 << 12) | 3);
    }
    VERIFICAR(navegou_barra, "setas navegam pela barra lateral com IDs estaveis");
    if (visual) {
        liberar_projeto(&visual->projeto);
        free(visual);
    }
    free(pixels_editor);
    liberar_workspace_virtual();
    iniciar_tema_arcano();
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

/* Linha de comando --------------------------------------------------------- */

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

/* Ponto de entrada --------------------------------------------------------- */

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
        if (versao_arquivo_projeto(argv[2]) == 3) {
            fprintf(stderr,
                    "Mythara: projetos v3 devem ser migrados para uma copia v4 pelo editor.\n");
            liberar_projeto(&a->projeto);
            free(a);
            return 1;
        }
        if (!abrir_projeto_atual(a, argv[2])) {
            fprintf(stderr, "Mythara: %s\n", a->mensagem);
            liberar_projeto(&a->projeto);
            free(a);
            return 1;
        }
    } else if (argc > 2 && !strcmp(argv[1], "--recuperar")) {
        if (versao_arquivo_projeto(argv[2]) == 3) {
            fprintf(
                stderr,
                "Mythara: migre o projeto v3 para uma copia v4 antes de recuperar autosaves.\n");
            liberar_projeto(&a->projeto);
            free(a);
            return 1;
        }
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
        if (plataforma.fechamento_solicitado) {
            plataforma.fechamento_solicitado = 0;
            if (!somente_jogo && a->modo == MODO_EDITOR && a->editor.alterado &&
                !a->encerrar_confirmado) {
                a->acao_descarte = DESCARTE_SAIR;
                a->modal = MODAL_DESCARTAR;
            } else
                plataforma.executando = 0;
        }
        idioma_definir((IdiomaMythara)(a->modo == MODO_EDITOR ? a->idioma_editor
                                                              : a->projeto.idioma_interface));
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
            Interface ui = {
                .tela = &tela_editor, .entrada = &entrada_logica, .id_foco = a->foco_interface};
            ui_preparar_quadro(&ui);
            limpar_tela(&tela_editor, tema_ativo.fundo);
            desenhar_editor(a, &ui);
            ui_finalizar_quadro(&ui);
            verificar_historico_global(a, &entrada_logica);
            realizar_autosave(a);
            a->foco_interface = ui.id_foco;
            copiar_tela_escalada(&tela_editor, &plataforma.tela);
        } else {
            Interface ui = {.tela = &plataforma.tela,
                            .entrada = &plataforma.entrada,
                            .id_foco = a->foco_interface};
            ui_preparar_quadro(&ui);
            limpar_tela(&plataforma.tela, tema_ativo.fundo);
            if (a->modo == MODO_JOGO) {
                atualizar_jogo(a, &plataforma.entrada);
                desenhar_jogo(a, &ui);
            } else
                desenhar_batalha(a, &ui);
            ui_finalizar_quadro(&ui);
            a->foco_interface = ui.id_foco;
        }
        if (somente_jogo && a->modo == MODO_EDITOR)
            plataforma.executando = 0;
        plataforma_apresentar(&plataforma);
        if (a->encerrar_confirmado)
            plataforma.executando = 0;
        pausar_milissegundos(16);
    }
    plataforma_encerrar(&plataforma);
    while (atomic_load(&a->autosave_em_andamento))
        pausar_milissegundos(10);
    free(tela_editor.pixels);
    liberar_workspace_virtual();
    if (!somente_jogo)
        salvar_configuracao(a);
    liberar_historico_global(&a->editor);
    liberar_recursos(a);
    liberar_projeto(&a->projeto);
    free(a);
    return 0;
}
