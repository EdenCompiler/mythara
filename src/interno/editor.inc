/*
 * Modulo: editor
 * Responsabilidade: workspace de mapas, navegacao, pintura e inspetor.
 * Depende de: estado do aplicativo e toolkit de interface.
 */

/* Navegacao e paineis principais ------------------------------------------- */

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
    ui_definir_escopo(ui, 1);
    preencher_retangulo(t, (Retangulo){0, 0, t->largura, 44}, tema_ativo.barra);
    preencher_retangulo(t, (Retangulo){0, 43, t->largura, 1}, tema_ativo.borda);
    int brilho_barra = (((tema_ativo.barra >> 16) & 255) * 299 +
                        ((tema_ativo.barra >> 8) & 255) * 587 + (tema_ativo.barra & 255) * 114) /
                       1000;
    int brilho_destaque =
        (((tema_ativo.destaque >> 16) & 255) * 299 + ((tema_ativo.destaque >> 8) & 255) * 587 +
         (tema_ativo.destaque & 255) * 114) /
        1000;
    int diferenca = brilho_barra - brilho_destaque;
    if (diferenca < 0)
        diferenca = -diferenca;
    Cor texto_barra = diferenca >= 90 ? tema_ativo.destaque : tema_ativo.painel_elevado;
    desenhar_texto_forte(t, 14, 13, "MYTHARA", texto_barra, 2);
    if (t->largura >= 760) {
        desenhar_texto(t, 112, 17, "/", texto_barra, 1);
        desenhar_texto(t, 126, 17, a->projeto.nome, texto_barra, 1);
    }
    int x = t->largura - 244;
    if (ui_botao(ui, (Retangulo){x - 48, 7, 42, 30},
                 a->idioma_editor == IDIOMA_INGLES ? "EN" : "PT", 0)) {
        a->idioma_editor = a->idioma_editor == IDIOMA_INGLES ? IDIOMA_PORTUGUES : IDIOMA_INGLES;
        idioma_definir((IdiomaMythara)a->idioma_editor);
    }
    if (ui_botao_icone(ui, (Retangulo){x, 7, 54, 30}, ICONE_SALVAR, "SALVAR", 0, 0))
        salvar_projeto_atual(a);
    x += 60;
    if (ui_botao(ui, (Retangulo){x, 7, 42, 30}, "<-", 0))
        desfazer_edicao(a);
    x += 48;
    if (ui_botao(ui, (Retangulo){x, 7, 42, 30}, "->", 0))
        refazer_edicao(a);
    x += 48;
    if (ui_botao_icone(ui, (Retangulo){x, 7, 82, 30}, ICONE_JOGAR, "F5", 0, 1))
        iniciar_playtest_editor(a);
    if (ui->entrada->teclas_pressionadas[132])
        iniciar_playtest_editor(a);
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

static int editor_largura_barra(Editor *e, int largura_tela) {
    int alvo = e->barra_lateral_recolhida || largura_tela < 900 ? 52 : 192;
    if (!e->largura_barra_animada)
        e->largura_barra_animada = alvo;
    if (tema_ativo.movimento_reduzido)
        e->largura_barra_animada = alvo;
    else if (e->largura_barra_animada < alvo)
        e->largura_barra_animada = limitar_int(e->largura_barra_animada + 18, 52, alvo);
    else if (e->largura_barra_animada > alvo)
        e->largura_barra_animada = limitar_int(e->largura_barra_animada - 18, alvo, 192);
    return e->largura_barra_animada;
}

static int editor_barra_lateral(Aplicativo *a, Interface *ui) {
    Editor *e = &a->editor;
    ui_definir_escopo(ui, 2);
    int largura = editor_largura_barra(e, ui->tela->largura), expandida = largura > 120,
        vertical_compacto = ui->tela->altura < 480;
    Retangulo barra = {0, 44, largura, ui->tela->altura - 68};
    preencher_retangulo(ui->tela, barra, tema_ativo.lateral);
    preencher_retangulo(ui->tela, (Retangulo){largura - 1, barra.y, 1, barra.altura},
                        tema_ativo.borda);
    if (ui_botao_icone(
            ui,
            (Retangulo){8, vertical_compacto ? 50 : 52, largura - 16, vertical_compacto ? 28 : 32},
            ICONE_MENU, e->barra_lateral_recolhida ? "EXPANDIR" : "RECOLHER", 0, expandida))
        e->barra_lateral_recolhida = !e->barra_lateral_recolhida;
    const struct {
        const char *rotulo;
        IconeInterface icone;
        int layout;
    } principais[] = {{"MAPAS", ICONE_MAPA, LAYOUT_MAPA},
                      {"EVENTOS", ICONE_EVENTO, LAYOUT_EVENTOS},
                      {"BANCO", ICONE_BANCO, LAYOUT_BANCO},
                      {"RECURSOS", ICONE_RECURSO, LAYOUT_RECURSOS}},
      utilitarios[] = {{"TEMA", ICONE_TEMA, LAYOUT_TEMA},
                       {"PROJETO", ICONE_PROJETO, LAYOUT_PROJETO},
                       {"AJUDA", ICONE_AJUDA, LAYOUT_AJUDA}};
    int ids_navegacao[7] = {0}, quantidade_ids = 0;
    int altura_item = vertical_compacto ? 30 : 38, passo_item = vertical_compacto ? 34 : 44;
    int y = vertical_compacto ? 84 : 98;
    for (size_t i = 0; i < sizeof(principais) / sizeof(principais[0]); ++i) {
        ids_navegacao[quantidade_ids++] = ui_proximo_id_previsto(ui);
        if (ui_botao_icone(ui, (Retangulo){8, y, largura - 16, altura_item}, principais[i].icone,
                           principais[i].rotulo, e->layout_ativo == principais[i].layout,
                           expandida)) {
            e->layout_ativo = principais[i].layout;
            a->modal = MODAL_NENHUM;
            ui->id_foco = 0;
        }
        y += passo_item;
    }
    y = vertical_compacto ? barra.y + barra.altura - 3 * passo_item - 4
                          : barra.y + barra.altura - 3 * passo_item - 10;
    for (size_t i = 0; i < sizeof(utilitarios) / sizeof(utilitarios[0]); ++i) {
        ids_navegacao[quantidade_ids++] = ui_proximo_id_previsto(ui);
        if (ui_botao_icone(ui, (Retangulo){8, y, largura - 16, altura_item}, utilitarios[i].icone,
                           utilitarios[i].rotulo, e->layout_ativo == utilitarios[i].layout,
                           expandida)) {
            e->layout_ativo = utilitarios[i].layout;
            a->modal = MODAL_NENHUM;
            ui->id_foco = 0;
        }
        y += passo_item;
    }
    for (int i = 0; i < quantidade_ids; ++i)
        if (ui->id_foco == ids_navegacao[i]) {
            if (ui->entrada->teclas_pressionadas[130])
                ui->id_foco = ids_navegacao[(i + quantidade_ids - 1) % quantidade_ids];
            if (ui->entrada->teclas_pressionadas[131])
                ui->id_foco = ids_navegacao[(i + 1) % quantidade_ids];
            if (ui->entrada->teclas_pressionadas[128])
                e->barra_lateral_recolhida = 1;
            if (ui->entrada->teclas_pressionadas[129])
                e->barra_lateral_recolhida = 0;
            break;
        }
    return largura;
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
    int inicio_lista = y, filtrados = 0;
    for (int i = 0; i < p->quantidade_mapas; ++i)
        if (texto_contem_sem_caixa(p->mapas[i].nome, e->busca_arvore))
            filtrados++;
    Retangulo area_mapas = {r.x + 8, y, r.largura - 16, 6 * 25};
    if (ponto_em_retangulo(ui->entrada->mouse_x, ui->entrada->mouse_y, area_mapas) &&
        ui->entrada->roda)
        e->rolagem_arvore -= ui->entrada->roda;
    e->rolagem_arvore = limitar_int(e->rolagem_arvore, 0, filtrados > 6 ? filtrados - 6 : 0);
    int mostrados = 0, ordem = 0;
    for (int i = 0; i < p->quantidade_mapas && mostrados < 6; ++i)
        if (texto_contem_sem_caixa(p->mapas[i].nome, e->busca_arvore)) {
            if (ordem++ < e->rolagem_arvore)
                continue;
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
    y = inicio_lista + 6 * 25;
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
        char detalhe[192];
        if (registro_referenciado(p, REFERENCIA_MAPA, e->mapa_atual, detalhe, sizeof(detalhe))) {
            mensagem_aplicativo(a, detalhe);
            return;
        }
        int removido = e->mapa_atual;
        liberar_mapa(&p->mapas[e->mapa_atual]);
        memmove(&p->mapas[e->mapa_atual], &p->mapas[e->mapa_atual + 1],
                (size_t)(p->quantidade_mapas - e->mapa_atual - 1) * sizeof(Mapa));
        p->quantidade_mapas--;
        memset(&p->mapas[p->quantidade_mapas], 0, sizeof(Mapa));
        ajustar_referencias_apos_remocao(p, REFERENCIA_MAPA, removido);
        e->mapa_atual = limitar_int(e->mapa_atual, 0, p->quantidade_mapas - 1);
        e->alterado = 1;
    }
    y += 32;
    char resumo[96];
    snprintf(resumo, sizeof(resumo), "> EVENTOS (%d)", p->quantidade_eventos);
    if (ui_botao(ui, (Retangulo){r.x + 8, y, r.largura - 16, 24}, resumo, 0))
        a->editor.layout_ativo = LAYOUT_EVENTOS;
    y += 28;
    snprintf(resumo, sizeof(resumo), "> RECURSOS (%d)", p->quantidade_recursos);
    if (ui_botao(ui, (Retangulo){r.x + 8, y, r.largura - 16, 24}, resumo, 0))
        a->editor.layout_ativo = LAYOUT_RECURSOS;
    y += 28;
    if (ui_botao(ui, (Retangulo){r.x + 8, y, r.largura - 16, 24}, "> BANCO DE DADOS", 0))
        a->editor.layout_ativo = LAYOUT_BANCO;
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

/* Ferramentas de pintura ---------------------------------------------------- */

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

/* Selecao, clipboard e carimbos -------------------------------------------- */

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

/* Canvas e interacao -------------------------------------------------------- */

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

/* Inspetor ----------------------------------------------------------------- */

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
        n->evento = ui_referencia(ui, (Retangulo){r.x + 10, y, r.largura - 20, 27}, &a->projeto,
                                  "EVENTO", REFERENCIA_EVENTO, n->evento, 1);
        n->evento_id = n->evento >= 0 ? a->projeto.eventos[n->evento].id : 0;
        y += 32;
        n->cor = ui_numero(ui, (Retangulo){r.x + 10, y, r.largura - 20, 27}, "COR", n->cor, 0, 15);
        y += 35;
        if (ui_botao(ui, (Retangulo){r.x + 10, y, r.largura - 20, 27}, "EDITAR EVENTO", 0)) {
            e->evento_selecionado = n->evento;
            a->editor.layout_ativo = LAYOUT_EVENTOS;
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
            ui_referencia(ui, (Retangulo){r.x + 10, y, r.largura - 20, 27}, &a->projeto, "ENCONTRO",
                          REFERENCIA_INIMIGO, m->regiao_encontro, 1);
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
