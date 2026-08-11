/*
 * Modulo: modais
 * Responsabilidade: fluxos focados que bloqueiam temporariamente o workspace.
 * Convencao: cada modal recebe Aplicativo e Interface e altera somente o estado
 * confirmado pelo usuario.
 */

/* Estrutura comum e selecao de caminhos ------------------------------------ */

static Retangulo iniciar_modal(Interface *ui, const char *titulo, int largura, int altura) {
    preencher_retangulo(ui->tela, (Retangulo){0, 0, ui->tela->largura, ui->tela->altura},
                        cor_rgb(14, 17, 23));
    largura = limitar_int(largura, 240, ui->tela->largura - 20);
    altura = limitar_int(altura, 160, ui->tela->altura - 20);
    Retangulo r = {(ui->tela->largura - largura) / 2, (ui->tela->altura - altura) / 2, largura,
                   altura};
    ui_painel(ui->tela, r, titulo);
    return r;
}

static void modal_caminho(Aplicativo *a, Interface *ui) {
    const char *titulo = a->acao_caminho == ACAO_ABRIR
                             ? "ABRIR PROJETO"
                             : (a->acao_caminho == ACAO_EXPORTAR ? "EXPORTAR JOGO" : "SALVAR COMO");
    int largura = limitar_int(ui->tela->largura - 30, 520, 780),
        altura = limitar_int(ui->tela->altura - 30, 390, 540);
    Retangulo r = iniciar_modal(ui, titulo, largura, altura);
    if (!a->pasta_navegador[0]) {
        if (strstr(a->campo_caminho, ".myr")) {
            pasta_do_arquivo(a->campo_caminho, a->pasta_navegador, sizeof(a->pasta_navegador));
            copiar_texto(a->arquivo_navegador, sizeof(a->arquivo_navegador),
                         nome_base(a->campo_caminho));
        } else {
            copiar_texto(a->pasta_navegador, sizeof(a->pasta_navegador), ".");
            copiar_texto(a->arquivo_navegador, sizeof(a->arquivo_navegador),
                         a->campo_caminho[0]
                             ? nome_base(a->campo_caminho)
                             : (a->acao_caminho == ACAO_EXPORTAR ? "meu_jogo" : "projeto.myr"));
        }
    }
    desenhar_texto(ui->tela, r.x + 18, r.y + 42, "PASTA", tema_ativo.texto_suave, 1);
    ui_campo(ui, (Retangulo){r.x + 18, r.y + 57, r.largura - 132, 29}, a->pasta_navegador,
             sizeof(a->pasta_navegador));
    if (ui_botao(ui, (Retangulo){r.x + r.largura - 106, r.y + 57, 88, 29}, "SUBIR", 0))
        subir_diretorio(a->pasta_navegador);
    ItemDiretorio itens[128];
    int quantidade = listar_diretorio(a->pasta_navegador, itens, 128), y = r.y + 101,
        visiveis = (r.altura - 210) / 27;
    if (visiveis < 5)
        visiveis = 5;
    for (int i = 0; i < quantidade && i < visiveis; ++i) {
        char linha[300];
        snprintf(linha, sizeof(linha), "%s %.255s", itens[i].diretorio ? ">" : " ", itens[i].nome);
        if (ui_botao(ui, (Retangulo){r.x + 18, y, r.largura - 36, 24}, linha,
                     !itens[i].diretorio && !strcmp(a->arquivo_navegador, itens[i].nome))) {
            if (itens[i].diretorio) {
                char proxima[MYTHARA_MAX_CAMINHO];
                if (juntar_caminho(proxima, sizeof(proxima), a->pasta_navegador, itens[i].nome))
                    copiar_texto(a->pasta_navegador, sizeof(a->pasta_navegador), proxima);
            } else
                copiar_texto(a->arquivo_navegador, sizeof(a->arquivo_navegador), itens[i].nome);
        }
        y += 27;
    }
    y = r.y + r.altura - 94;
    desenhar_texto(ui->tela, r.x + 18, y, "NOME", tema_ativo.texto_suave, 1);
    ui_campo(ui, (Retangulo){r.x + 70, y - 7, r.largura - 300, 29}, a->arquivo_navegador,
             sizeof(a->arquivo_navegador));
    if (ui_botao(ui, (Retangulo){r.x + r.largura - 215, y - 7, 92, 29}, "NOVA PASTA", 0)) {
        char nova[MYTHARA_MAX_CAMINHO];
        if (juntar_caminho(nova, sizeof(nova), a->pasta_navegador, a->arquivo_navegador) &&
            criar_diretorios(nova))
            copiar_texto(a->pasta_navegador, sizeof(a->pasta_navegador), nova);
    }
    if (ui_botao(ui, (Retangulo){r.x + r.largura - 218, r.y + r.altura - 46, 92, 28}, "CANCELAR",
                 0)) {
        if (a->modal == MODAL_CAMINHO)
            a->modal = MODAL_NENHUM;
        a->pasta_navegador[0] = 0;
    }
    if (ui_botao(ui, (Retangulo){r.x + r.largura - 116, r.y + r.altura - 46, 96, 28}, "CONFIRMAR",
                 0)) {
        char escolhido[MYTHARA_MAX_CAMINHO];
        juntar_caminho(escolhido, sizeof(escolhido), a->pasta_navegador, a->arquivo_navegador);
        copiar_texto(a->campo_caminho, sizeof(a->campo_caminho), escolhido);
        if (a->acao_caminho == ACAO_ABRIR) {
            if (a->editor.alterado) {
                copiar_texto(a->caminho_pendente, sizeof(a->caminho_pendente), escolhido);
                a->acao_descarte = DESCARTE_ABRIR;
                a->modal = MODAL_DESCARTAR;
            } else
                abrir_projeto_atual(a, escolhido);
        } else if (a->acao_caminho == ACAO_SALVAR_COMO) {
            if (!strstr(escolhido, ".myr"))
                strncat(escolhido, ".myr", sizeof(escolhido) - strlen(escolhido) - 1);
            copiar_texto(a->caminho_projeto, sizeof(a->caminho_projeto), escolhido);
            salvar_projeto_atual(a);
        } else {
            char erro[192];
            if (!projeto_pronto_para_executar(a)) {
                a->pasta_navegador[0] = 0;
                return;
            }
            if (exportar_projeto(&a->projeto, escolhido, erro, sizeof(erro)))
                mensagem_aplicativo(a, "Jogo nativo exportado com sucesso.");
            else
                mensagem_aplicativo(a, erro);
        }
        if (a->modal == MODAL_CAMINHO)
            a->modal = MODAL_NENHUM;
        a->pasta_navegador[0] = 0;
    }
}

static void modal_novo(Aplicativo *a, Interface *ui) {
    Retangulo r = iniciar_modal(ui, "NOVO PROJETO", 470, 170);
    desenhar_texto(ui->tela, r.x + 22, r.y + 52, "SUBSTITUIR O PROJETO ATUAL?",
                   cor_rgb(226, 230, 237), 1);
    if (a->editor.alterado)
        desenhar_texto(ui->tela, r.x + 22, r.y + 72, "HA ALTERACOES NAO SALVAS.",
                       cor_rgb(235, 174, 84), 1);
    if (a->editor.alterado &&
        ui_botao(ui, (Retangulo){r.x + 18, r.y + 120, 132, 28}, "SALVAR E CRIAR", 0) &&
        salvar_projeto_atual(a)) {
        novo_projeto(a);
        a->modal = MODAL_NENHUM;
    }
    if (ui_botao(ui, (Retangulo){r.x + 250, r.y + 120, 90, 28}, "CANCELAR", 0))
        a->modal = MODAL_NENHUM;
    if (ui_botao(ui, (Retangulo){r.x + 350, r.y + 120, 96, 28}, "DESCARTAR", 0)) {
        novo_projeto(a);
        a->modal = MODAL_NENHUM;
    }
}

static const char *nome_registro_banco(const Projeto *p, int aba, int indice) {
    if (aba == 0)
        return p->herois[indice].nome;
    if (aba == 1)
        return p->classes[indice].nome;
    if (aba == 2)
        return p->habilidades[indice].nome;
    if (aba == 3)
        return p->itens[indice].nome;
    if (aba == 4)
        return p->inimigos[indice].nome;
    if (aba == 5)
        return p->estados[indice].nome;
    if (aba == 6)
        return p->lojas[indice].nome;
    return p->missoes[indice].nome;
}

/* Banco de dados ------------------------------------------------------------ */

static void modal_banco(Aplicativo *a, Interface *ui) {
    Retangulo r = iniciar_modal(ui, "BANCO DE DADOS JRPG", 1040, 620);
    Editor *e = &a->editor;
    Projeto *p = &a->projeto;

    /* Abas e origem da lista. */
    const char *abas[] = {"HEROI",    "CLASSES", "HABILIDADES", "ITENS",
                          "INIMIGOS", "ESTADOS", "LOJAS",       "MISSOES"};
    for (int i = 0; i < 8; ++i) {
        int bx = r.x + 18 + (i % 4) * 126, by = r.y + 40 + (i / 4) * 31;
        if (ui_botao(ui, (Retangulo){bx, by, 118, 26}, abas[i], e->aba_banco == i)) {
            e->aba_banco = i;
            e->registro_banco = 0;
            e->subregistro_banco = 0;
            e->rolagem_banco = 0;
        }
    }
    int y = r.y + 112, total = p->quantidade_herois;
    const char *nome = "Heroi";
    if (e->aba_banco == 1)
        total = p->quantidade_classes;
    else if (e->aba_banco == 2)
        total = p->quantidade_habilidades;
    else if (e->aba_banco == 3)
        total = p->quantidade_itens;
    else if (e->aba_banco == 4)
        total = p->quantidade_inimigos;
    else if (e->aba_banco == 5)
        total = p->quantidade_estados;
    else if (e->aba_banco == 6)
        total = p->quantidade_lojas;
    else if (e->aba_banco == 7)
        total = p->quantidade_missoes;
    ui_campo(ui, (Retangulo){r.x + 18, r.y + r.altura - 111, 230, 27}, e->busca_banco,
             sizeof(e->busca_banco));
    int filtrados = 0;
    for (int i = 0; i < total; ++i)
        if (texto_contem_sem_caixa(nome_registro_banco(p, e->aba_banco, i), e->busca_banco))
            filtrados++;
    Retangulo area_lista = {r.x + 18, y, 230, 13 * 29};
    if (ponto_em_retangulo(ui->entrada->mouse_x, ui->entrada->mouse_y, area_lista) &&
        ui->entrada->roda)
        e->rolagem_banco -= ui->entrada->roda * 2;
    e->rolagem_banco = limitar_int(e->rolagem_banco, 0, filtrados > 13 ? filtrados - 13 : 0);
    int ordem = 0, linha_lista = 0;
    for (int i = 0; i < total && linha_lista < 13; ++i) {
        nome = nome_registro_banco(p, e->aba_banco, i);
        if (!texto_contem_sem_caixa(nome, e->busca_banco))
            continue;
        if (ordem++ < e->rolagem_banco)
            continue;
        if (ui_botao(ui, (Retangulo){r.x + 18, y + linha_lista * 29, 230, 25}, nome,
                     e->registro_banco == i)) {
            e->registro_banco = i;
            e->subregistro_banco = 0;
        }
        linha_lista++;
    }
    if (ui_botao(ui, (Retangulo){r.x + 18, r.y + r.altura - 78, 48, 25}, "SUBIR", 0))
        e->rolagem_banco = limitar_int(e->rolagem_banco - 13, 0, filtrados);
    if (ui_botao(ui, (Retangulo){r.x + 72, r.y + r.altura - 78, 54, 25}, "DESCER", 0))
        e->rolagem_banco =
            limitar_int(e->rolagem_banco + 13, 0, filtrados > 13 ? filtrados - 13 : 0);
    /* Paginacao e criacao de registros. */
    if (ui_botao(ui, (Retangulo){r.x + 132, r.y + r.altura - 78, 55, 25}, "+", 0)) {
        if (e->aba_banco == 0 && p->quantidade_herois < 16) {
            int i = p->quantidade_herois++;
            memset(&p->herois[i], 0, sizeof(Heroi));
            snprintf(p->herois[i].nome, 48, "Heroi %d", i + 1);
            p->herois[i].classe = p->quantidade_classes ? 0 : -1;
            p->herois[i].sprite = -1;
            p->herois[i].vida_maxima = 20;
            p->herois[i].velocidade = 5;
            e->registro_banco = i;
        } else if (e->aba_banco == 1 && p->quantidade_classes < 32) {
            int i = p->quantidade_classes++;
            memset(&p->classes[i], 0, sizeof(Classe));
            snprintf(p->classes[i].nome, 48, "Classe %d", i + 1);
            for (int nivel = 0; nivel < 32; ++nivel)
                p->classes[i].habilidade_por_nivel[nivel] = -1;
            e->registro_banco = i;
        } else if (e->aba_banco == 2 && p->quantidade_habilidades < 128) {
            int i = p->quantidade_habilidades++;
            memset(&p->habilidades[i], 0, sizeof(Habilidade));
            snprintf(p->habilidades[i].nome, 48, "Habilidade %d", i + 1);
            p->habilidades[i].estado = -1;
            p->habilidades[i].animacao = -1;
            e->registro_banco = i;
        } else if (e->aba_banco == 3 && p->quantidade_itens < MYTHARA_MAX_ITENS &&
                   reservar_memoria((void **)&p->itens, &p->capacidade_itens,
                                    p->quantidade_itens + 1, sizeof(Item), MYTHARA_MAX_ITENS)) {
            int i = p->quantidade_itens++;
            memset(&p->itens[i], 0, sizeof(Item));
            snprintf(p->itens[i].nome, 48, "Item %d", i + 1);
            e->registro_banco = i;
        } else if (e->aba_banco == 4 && p->quantidade_inimigos < MYTHARA_MAX_INIMIGOS &&
                   reservar_memoria((void **)&p->inimigos, &p->capacidade_inimigos,
                                    p->quantidade_inimigos + 1, sizeof(Inimigo),
                                    MYTHARA_MAX_INIMIGOS)) {
            int i = p->quantidade_inimigos++;
            memset(&p->inimigos[i], 0, sizeof(Inimigo));
            snprintf(p->inimigos[i].nome, 48, "Inimigo %d", i + 1);
            p->inimigos[i].vida = 10;
            e->registro_banco = i;
        } else if (e->aba_banco == 5 && p->quantidade_estados < 64) {
            int i = p->quantidade_estados++;
            memset(&p->estados[i], 0, sizeof(EstadoCombate));
            snprintf(p->estados[i].nome, 48, "Estado %d", i + 1);
            e->registro_banco = i;
        } else if (e->aba_banco == 6 && p->quantidade_lojas < 64) {
            int i = p->quantidade_lojas++;
            memset(&p->lojas[i], 0, sizeof(Loja));
            snprintf(p->lojas[i].nome, 48, "Loja %d", i + 1);
            e->registro_banco = i;
        } else if (e->aba_banco == 7 && p->quantidade_missoes < 128) {
            int i = p->quantidade_missoes++;
            memset(&p->missoes[i], 0, sizeof(Missao));
            snprintf(p->missoes[i].nome, 48, "Missao %d", i + 1);
            p->missoes[i].recompensa_item = -1;
            e->registro_banco = i;
        }
        atribuir_ids_ausentes(p);
        if (e->aba_banco == 0)
            total = p->quantidade_herois;
        else if (e->aba_banco == 1)
            total = p->quantidade_classes;
        else if (e->aba_banco == 2)
            total = p->quantidade_habilidades;
        else if (e->aba_banco == 3)
            total = p->quantidade_itens;
        else if (e->aba_banco == 4)
            total = p->quantidade_inimigos;
        else if (e->aba_banco == 5)
            total = p->quantidade_estados;
        else if (e->aba_banco == 6)
            total = p->quantidade_lojas;
        else if (e->aba_banco == 7)
            total = p->quantidade_missoes;
    }
    if (ui_botao(ui, (Retangulo){r.x + 193, r.y + r.altura - 78, 55, 25}, "-", 0)) {
        excluir_registro_banco(a, e->aba_banco, e->registro_banco);
        if (e->aba_banco == 0)
            total = p->quantidade_herois;
        else if (e->aba_banco == 1)
            total = p->quantidade_classes;
        else if (e->aba_banco == 2)
            total = p->quantidade_habilidades;
        else if (e->aba_banco == 3)
            total = p->quantidade_itens;
        else if (e->aba_banco == 4)
            total = p->quantidade_inimigos;
        else if (e->aba_banco == 5)
            total = p->quantidade_estados;
        else if (e->aba_banco == 6)
            total = p->quantidade_lojas;
        else
            total = p->quantidade_missoes;
    }
    int i = limitar_int(e->registro_banco, 0, total ? total - 1 : 0), x = r.x + 280, x2 = x + 430,
        y2 = r.y + 112;
    y = r.y + 112;

    /* Formulario especifico da aba selecionada. */
    if (e->aba_banco == 0) {
        Heroi *h = &p->herois[i];
        ui_campo(ui, (Retangulo){x, y, 400, 28}, h->nome, sizeof(h->nome));
        y += 37;
        h->classe = ui_referencia(ui, (Retangulo){x, y, 400, 27}, p, "CLASSE", REFERENCIA_CLASSE,
                                  h->classe, 1);
        y += 34;
        h->vida_maxima = ui_numero(ui, (Retangulo){x, y, 400, 27}, "VIDA", h->vida_maxima, 1, 9999);
        y += 34;
        h->magia_maxima =
            ui_numero(ui, (Retangulo){x, y, 400, 27}, "MAGIA", h->magia_maxima, 0, 9999);
        y += 34;
        h->ataque = ui_numero(ui, (Retangulo){x, y, 400, 27}, "ATAQUE", h->ataque, 1, 999);
        y += 34;
        h->defesa = ui_numero(ui, (Retangulo){x, y, 400, 27}, "DEFESA", h->defesa, 0, 999);
        y += 34;
        h->poder_magico =
            ui_numero(ui, (Retangulo){x, y, 400, 27}, "PODER MAGICO", h->poder_magico, 0, 999);
        y += 34;
        h->resistencia =
            ui_numero(ui, (Retangulo){x, y, 400, 27}, "RESISTENCIA", h->resistencia, 0, 999);
        y += 34;
        h->velocidade =
            ui_numero(ui, (Retangulo){x, y, 400, 27}, "VELOCIDADE", h->velocidade, 1, 999);
        h->sorte = ui_numero(ui, (Retangulo){x2, y2, 280, 27}, "SORTE", h->sorte, 0, 999);
        y2 += 34;
        h->sprite = ui_referencia(ui, (Retangulo){x2, y2, 280, 27}, p, "SPRITE", REFERENCIA_RECURSO,
                                  h->sprite, 1);
    } else if (total > 0 && e->aba_banco == 1) {
        Classe *c = &p->classes[i];
        ui_campo(ui, (Retangulo){x, y, 400, 28}, c->nome, sizeof(c->nome));
        y += 38;
        c->vida_base =
            ui_numero(ui, (Retangulo){x, y, 400, 27}, "VIDA BASE", c->vida_base, 1, 9999);
        y += 34;
        c->magia_base =
            ui_numero(ui, (Retangulo){x, y, 400, 27}, "MAGIA BASE", c->magia_base, 0, 9999);
        y += 34;
        c->ataque_base =
            ui_numero(ui, (Retangulo){x, y, 400, 27}, "ATAQUE BASE", c->ataque_base, 1, 999);
        y += 34;
        c->defesa_base =
            ui_numero(ui, (Retangulo){x, y, 400, 27}, "DEFESA BASE", c->defesa_base, 0, 999);
        y += 34;
        c->velocidade_base = ui_numero(ui, (Retangulo){x, y, 400, 27}, "VELOCIDADE BASE",
                                       c->velocidade_base, 1, 999);
        c->poder_magico_base = ui_numero(ui, (Retangulo){x2, y2, 280, 27}, "PODER MAGICO BASE",
                                         c->poder_magico_base, 0, 999);
        y2 += 34;
        c->resistencia_base = ui_numero(ui, (Retangulo){x2, y2, 280, 27}, "RESISTENCIA BASE",
                                        c->resistencia_base, 0, 999);
        y2 += 34;
        c->sorte_base =
            ui_numero(ui, (Retangulo){x2, y2, 280, 27}, "SORTE BASE", c->sorte_base, 0, 999);
        y += 34;
        e->subregistro_banco = ui_numero(ui, (Retangulo){x, y, 400, 27}, "NIVEL DA HABILIDADE",
                                         e->subregistro_banco, 0, 31);
        y += 34;
        c->habilidade_por_nivel[e->subregistro_banco] =
            ui_referencia(ui, (Retangulo){x, y, 400, 27}, p, "HABILIDADE", REFERENCIA_HABILIDADE,
                          c->habilidade_por_nivel[e->subregistro_banco], 1);
    } else if (total > 0 && e->aba_banco == 2) {
        Habilidade *h = &p->habilidades[i];
        ui_campo(ui, (Retangulo){x, y, 400, 28}, h->nome, sizeof(h->nome));
        y += 38;
        ui_campo(ui, (Retangulo){x, y, 620, 28}, h->descricao, sizeof(h->descricao));
        y += 38;
        h->custo_mp = ui_numero(ui, (Retangulo){x, y, 400, 27}, "CUSTO MP", h->custo_mp, 0, 999);
        y += 34;
        h->poder = ui_numero(ui, (Retangulo){x, y, 400, 27}, "PODER", h->poder, -999, 9999);
        y += 34;
        h->elemento = ui_numero(ui, (Retangulo){x, y, 400, 27}, "ELEMENTO", h->elemento, 0, 15);
        y += 34;
        h->alvo = ui_numero(ui, (Retangulo){x, y, 400, 27}, "ALVO", h->alvo, 0, 4);
        y += 34;
        h->estado = ui_referencia(ui, (Retangulo){x, y, 400, 27}, p, "ESTADO", REFERENCIA_ESTADO,
                                  h->estado, 1);
        y += 34;
        h->animacao = ui_referencia(ui, (Retangulo){x, y, 400, 27}, p, "ANIMACAO",
                                    REFERENCIA_RECURSO, h->animacao, 1);
    } else if (total > 0 && e->aba_banco == 3) {
        Item *it = &p->itens[i];
        ui_campo(ui, (Retangulo){x, y, 400, 28}, it->nome, sizeof(it->nome));
        y += 38;
        ui_campo(ui, (Retangulo){x, y, 620, 28}, it->descricao, sizeof(it->descricao));
        y += 38;
        it->valor = ui_numero(ui, (Retangulo){x, y, 400, 27}, "VALOR", it->valor, 0, 99999);
        y += 34;
        it->cura = ui_numero(ui, (Retangulo){x, y, 400, 27}, "CURA", it->cura, 0, 9999);
        y += 34;
        it->tipo_equipamento = ui_numero(ui, (Retangulo){x, y, 400, 27}, "SLOT EQUIPAMENTO",
                                         it->tipo_equipamento, 0, 4);
        y += 34;
        it->ataque =
            ui_numero(ui, (Retangulo){x, y, 400, 27}, "BONUS ATAQUE", it->ataque, -999, 999);
        y += 34;
        it->defesa =
            ui_numero(ui, (Retangulo){x, y, 400, 27}, "BONUS DEFESA", it->defesa, -999, 999);
        it->magia =
            ui_numero(ui, (Retangulo){x2, y2, 280, 27}, "BONUS MAGIA", it->magia, -999, 999);
        y2 += 34;
        it->resistencia = ui_numero(ui, (Retangulo){x2, y2, 280, 27}, "BONUS RESISTENCIA",
                                    it->resistencia, -999, 999);
        y2 += 34;
        it->velocidade = ui_numero(ui, (Retangulo){x2, y2, 280, 27}, "BONUS VELOCIDADE",
                                   it->velocidade, -999, 999);
    } else if (total > 0 && e->aba_banco == 4) {
        Inimigo *n = &p->inimigos[i];
        ui_campo(ui, (Retangulo){x, y, 400, 28}, n->nome, sizeof(n->nome));
        y += 38;
        n->vida = ui_numero(ui, (Retangulo){x, y, 400, 27}, "VIDA", n->vida, 1, 99999);
        y += 34;
        n->magia = ui_numero(ui, (Retangulo){x, y, 400, 27}, "MAGIA", n->magia, 0, 9999);
        y += 34;
        n->ataque = ui_numero(ui, (Retangulo){x, y, 400, 27}, "ATAQUE", n->ataque, 1, 9999);
        y += 34;
        n->defesa = ui_numero(ui, (Retangulo){x, y, 400, 27}, "DEFESA", n->defesa, 0, 9999);
        y += 34;
        n->velocidade =
            ui_numero(ui, (Retangulo){x, y, 400, 27}, "VELOCIDADE", n->velocidade, 1, 999);
        y += 34;
        n->experiencia =
            ui_numero(ui, (Retangulo){x, y, 400, 27}, "EXPERIENCIA", n->experiencia, 0, 99999);
        y += 34;
        n->ouro = ui_numero(ui, (Retangulo){x, y, 400, 27}, "OURO", n->ouro, 0, 99999);
        n->poder_magico =
            ui_numero(ui, (Retangulo){x2, y2, 280, 27}, "PODER MAGICO", n->poder_magico, 0, 9999);
        y2 += 34;
        n->resistencia =
            ui_numero(ui, (Retangulo){x2, y2, 280, 27}, "RESISTENCIA", n->resistencia, 0, 9999);
        y2 += 34;
        n->sorte = ui_numero(ui, (Retangulo){x2, y2, 280, 27}, "SORTE", n->sorte, 0, 999);
    } else if (total > 0 && e->aba_banco == 5) {
        EstadoCombate *s = &p->estados[i];
        ui_campo(ui, (Retangulo){x, y, 400, 28}, s->nome, sizeof(s->nome));
        y += 38;
        s->duracao = ui_numero(ui, (Retangulo){x, y, 400, 27}, "DURACAO TURNOS", s->duracao, 1, 99);
        y += 34;
        s->dano_turno =
            ui_numero(ui, (Retangulo){x, y, 400, 27}, "DANO POR TURNO", s->dano_turno, -999, 999);
        y += 34;
        s->ataque_pct =
            ui_numero(ui, (Retangulo){x, y, 400, 27}, "ATAQUE %", s->ataque_pct, -100, 500);
        y += 34;
        s->defesa_pct =
            ui_numero(ui, (Retangulo){x, y, 400, 27}, "DEFESA %", s->defesa_pct, -100, 500);
        s->velocidade_pct = ui_numero(ui, (Retangulo){x2, y2, 280, 27}, "VELOCIDADE %",
                                      s->velocidade_pct, -100, 500);
    } else if (total > 0 && e->aba_banco == 6) {
        Loja *l = &p->lojas[i];
        ui_campo(ui, (Retangulo){x, y, 400, 28}, l->nome, sizeof(l->nome));
        y += 38;
        l->quantidade_itens = ui_numero(ui, (Retangulo){x, y, 400, 27}, "QUANTIDADE ITENS",
                                        l->quantidade_itens, 0, 32);
        y += 34;
        if (l->quantidade_itens) {
            e->subregistro_banco = ui_numero(ui, (Retangulo){x, y, 400, 27}, "ITEM DA LOJA",
                                             e->subregistro_banco, 0, l->quantidade_itens - 1);
            y += 34;
            int item = e->subregistro_banco;
            l->itens[item] = ui_referencia(ui, (Retangulo){x, y, 400, 27}, p, "ITEM",
                                           REFERENCIA_ITEM, l->itens[item], 0);
            y += 34;
            l->precos[item] =
                ui_numero(ui, (Retangulo){x, y, 400, 27}, "PRECO", l->precos[item], 0, 99999);
        }
    } else if (total > 0 && e->aba_banco == 7) {
        Missao *m = &p->missoes[i];
        ui_campo(ui, (Retangulo){x, y, 400, 28}, m->nome, sizeof(m->nome));
        y += 38;
        ui_campo(ui, (Retangulo){x, y, 620, 28}, m->descricao, sizeof(m->descricao));
        y += 38;
        m->quantidade_etapas =
            ui_numero(ui, (Retangulo){x, y, 400, 27}, "ETAPAS", m->quantidade_etapas, 0, 16);
        y += 34;
        if (m->quantidade_etapas) {
            e->subregistro_banco = ui_numero(ui, (Retangulo){x, y, 400, 27}, "ETAPA",
                                             e->subregistro_banco, 0, m->quantidade_etapas - 1);
            y += 34;
            EtapaMissao *etapa = &m->etapas[e->subregistro_banco];
            ui_campo(ui, (Retangulo){x, y, 620, 28}, etapa->descricao, sizeof(etapa->descricao));
            y += 38;
            etapa->tipo =
                ui_numero(ui, (Retangulo){x, y, 400, 27}, "TIPO OBJETIVO", etapa->tipo, 0, 4);
            y += 34;
            if (etapa->tipo == 1)
                etapa->alvo = ui_referencia(ui, (Retangulo){x, y, 400, 27}, p, "ALVO",
                                            REFERENCIA_INIMIGO, etapa->alvo, 0);
            else
                etapa->alvo =
                    ui_numero(ui, (Retangulo){x, y, 400, 27}, "ALVO", etapa->alvo, 0, 999);
            y += 34;
            etapa->quantidade =
                ui_numero(ui, (Retangulo){x, y, 400, 27}, "QUANTIDADE", etapa->quantidade, 1, 999);
        }
        y += 40;
        m->recompensa_item = ui_referencia(ui, (Retangulo){x, y, 400, 27}, p, "RECOMPENSA ITEM",
                                           REFERENCIA_ITEM, m->recompensa_item, 1);
        y += 34;
        m->recompensa_ouro = ui_numero(ui, (Retangulo){x, y, 400, 27}, "RECOMPENSA OURO",
                                       m->recompensa_ouro, 0, 99999);
        m->recompensa_quantidade = ui_numero(ui, (Retangulo){x2, y2, 280, 27}, "QTD RECOMPENSA",
                                             m->recompensa_quantidade, 0, 999);
        y2 += 34;
        m->recompensa_experiencia = ui_numero(ui, (Retangulo){x2, y2, 280, 27}, "RECOMPENSA XP",
                                              m->recompensa_experiencia, 0, 999999);
    }
    if (ui_botao(ui, (Retangulo){r.x + r.largura - 112, r.y + r.altura - 48, 90, 28}, "CONCLUIR",
                 0)) {
        e->alterado = 1;
        a->modal = MODAL_NENHUM;
    }
}

/* Eventos e comandos visuais ----------------------------------------------- */

static void modal_eventos(Aplicativo *a, Interface *ui) {
    Retangulo r = iniciar_modal(ui, "EDITOR VISUAL DE EVENTOS", 1040, 610);
    Editor *ed = &a->editor;
    int y = r.y + 42;
    if (r.largura < 900) {
        if (a->projeto.quantidade_eventos <= 0)
            adicionar_evento(a, "Novo evento");
        int ev = limitar_int(ed->evento_selecionado, 0, a->projeto.quantidade_eventos - 1);
        if (ui_botao(ui, (Retangulo){r.x + 16, r.y + 40, 68, 25}, "< EVENTO", 0)) {
            ev = limitar_int(ev - 1, 0, a->projeto.quantidade_eventos - 1);
            ed->evento_selecionado = ev;
            ed->comando_selecionado = 0;
        }
        if (ui_botao(ui, (Retangulo){r.x + 90, r.y + 40, 68, 25}, "EVENTO >", 0)) {
            ev = limitar_int(ev + 1, 0, a->projeto.quantidade_eventos - 1);
            ed->evento_selecionado = ev;
            ed->comando_selecionado = 0;
        }
        if (ui_botao(ui, (Retangulo){r.x + 164, r.y + 40, 86, 25}, "+ EVENTO", 0)) {
            int novo = adicionar_evento(a, "Novo evento");
            if (novo >= 0) {
                ev = novo;
                ed->evento_selecionado = novo;
                ed->comando_selecionado = 0;
            }
        }
        if (ui_botao(ui, (Retangulo){r.x + 134, r.y + r.altura - 44, 116, 24}, "- EVENTO", 0) &&
            excluir_evento_editor(a, ev))
            return;
        Evento *evento = &a->projeto.eventos[ev];
        int painel_x = r.x + 258, painel_largura = r.largura - 274;
        ui_campo(ui, (Retangulo){painel_x, r.y + 40, painel_largura, 25}, evento->nome,
                 sizeof(evento->nome));
        evento->condicao_flag =
            ui_numero(ui, (Retangulo){painel_x, r.y + 70, painel_largura / 2 - 3, 24}, "FLAG",
                      evento->condicao_flag, -1, MYTHARA_MAX_FLAGS - 1);
        evento->condicao_valor = ui_numero(
            ui,
            (Retangulo){painel_x + painel_largura / 2 + 3, r.y + 70, painel_largura / 2 - 3, 24},
            "VALOR", evento->condicao_valor, -999, 999);
        int inicio_y = r.y + 104, visiveis = limitar_int((r.altura - 154) / 25, 4, 12);
        Retangulo area_comandos = {r.x + 16, inicio_y, 234, visiveis * 25};
        if (ponto_em_retangulo(ui->entrada->mouse_x, ui->entrada->mouse_y, area_comandos) &&
            ui->entrada->roda)
            ed->rolagem_comandos_evento -= ui->entrada->roda;
        ed->rolagem_comandos_evento = limitar_int(
            ed->rolagem_comandos_evento, 0,
            evento->quantidade_comandos > visiveis ? evento->quantidade_comandos - visiveis : 0);
        for (int linha_comando = 0;
             linha_comando < visiveis &&
             ed->rolagem_comandos_evento + linha_comando < evento->quantidade_comandos;
             ++linha_comando) {
            int i = ed->rolagem_comandos_evento + linha_comando;
            char linha[96];
            snprintf(linha, sizeof(linha), "%02d %s", i + 1,
                     nomes_comandos[limitar_int(evento->comandos[i].tipo, 0, 16)]);
            if (ui_botao(ui, (Retangulo){r.x + 16, inicio_y + linha_comando * 25, 234, 22}, linha,
                         ed->comando_selecionado == i))
                ed->comando_selecionado = i;
        }
        if (ui_botao(ui, (Retangulo){r.x + 16, r.y + r.altura - 44, 112, 24}, "+ COMANDO", 0) &&
            evento->quantidade_comandos < MYTHARA_MAX_COMANDOS &&
            reservar_comandos(evento, evento->quantidade_comandos + 1)) {
            int i = evento->quantidade_comandos++;
            memset(&evento->comandos[i], 0, sizeof(ComandoEvento));
            evento->comandos[i].tipo = COMANDO_TEXTO;
            copiar_texto(evento->comandos[i].texto, sizeof(evento->comandos[i].texto),
                         "Novo dialogo");
            ed->comando_selecionado = i;
        }
        if (evento->quantidade_comandos > 0) {
            int ci = limitar_int(ed->comando_selecionado, 0, evento->quantidade_comandos - 1);
            ComandoEvento *c = &evento->comandos[ci];
            int cy = r.y + 104;
            c->tipo = ui_numero(ui, (Retangulo){painel_x, cy, painel_largura, 24}, "TIPO", c->tipo,
                                0, 16);
            cy += 28;
            ui_campo_multilinha(ui, (Retangulo){painel_x, cy, painel_largura, 46}, c->texto,
                                sizeof(c->texto));
            cy += 50;
            TipoReferencia tipo_referencia;
            const char *rotulo_referencia;
            if (tipo_referencia_comando(c->tipo, &tipo_referencia, &rotulo_referencia))
                c->a = ui_referencia(ui, (Retangulo){painel_x, cy, painel_largura, 24}, &a->projeto,
                                     rotulo_referencia, tipo_referencia, c->a, 0);
            else
                c->a = ui_numero(ui, (Retangulo){painel_x, cy, painel_largura, 24}, "PARAMETRO A",
                                 c->a, -1, 999);
            cy += 28;
            c->b = ui_numero(ui, (Retangulo){painel_x, cy, painel_largura, 24}, "PARAMETRO B", c->b,
                             -999, 999);
            cy += 28;
            c->c = ui_numero(ui, (Retangulo){painel_x, cy, painel_largura, 24}, "PARAMETRO C", c->c,
                             -999, 999);
            cy += 28;
            c->profundidade = ui_numero(ui, (Retangulo){painel_x, cy, painel_largura, 24}, "NIVEL",
                                        c->profundidade, 0, 8);
            if (ui_botao(ui, (Retangulo){painel_x, r.y + r.altura - 44, 130, 24}, "EXCLUIR", 0)) {
                memmove(c, c + 1, (size_t)(evento->quantidade_comandos - ci - 1) * sizeof(*c));
                evento->quantidade_comandos--;
                ed->comando_selecionado = limitar_int(
                    ci - 1, 0, evento->quantidade_comandos ? evento->quantidade_comandos - 1 : 0);
            }
        }
        if (ui_botao(ui, (Retangulo){r.x + r.largura - 108, r.y + r.altura - 44, 88, 24},
                     "CONCLUIR", 0)) {
            ed->alterado = 1;
            a->modal = MODAL_NENHUM;
        }
        return;
    }
    ui_campo(ui, (Retangulo){r.x + 16, r.y + 40, 200, 27}, ed->busca_eventos,
             sizeof(ed->busca_eventos));
    y = r.y + 76;
    int filtrados = 0;
    for (int i = 0; i < a->projeto.quantidade_eventos; ++i)
        if (a->projeto.eventos[i].ativo &&
            texto_contem_sem_caixa(a->projeto.eventos[i].nome, ed->busca_eventos))
            filtrados++;
    Retangulo area_eventos = {r.x + 16, y, 200, 15 * 27};
    if (ponto_em_retangulo(ui->entrada->mouse_x, ui->entrada->mouse_y, area_eventos) &&
        ui->entrada->roda)
        ed->rolagem_eventos -= ui->entrada->roda * 2;
    ed->rolagem_eventos = limitar_int(ed->rolagem_eventos, 0, filtrados > 15 ? filtrados - 15 : 0);
    int ordem_evento = 0, eventos_mostrados = 0;
    for (int i = 0; i < a->projeto.quantidade_eventos && eventos_mostrados < 15; ++i)
        if (a->projeto.eventos[i].ativo &&
            texto_contem_sem_caixa(a->projeto.eventos[i].nome, ed->busca_eventos)) {
            if (ordem_evento++ < ed->rolagem_eventos)
                continue;
            if (ui_botao(ui, (Retangulo){r.x + 16, y, 200, 25}, a->projeto.eventos[i].nome,
                         ed->evento_selecionado == i)) {
                ed->evento_selecionado = i;
                ed->comando_selecionado = 0;
                ed->rolagem_comandos_evento = 0;
            }
            y += 27;
            eventos_mostrados++;
        }
    if (ui_botao(ui, (Retangulo){r.x + 16, r.y + r.altura - 76, 94, 25}, "+ EVENTO", 0)) {
        int i = adicionar_evento(a, "Novo evento");
        if (i >= 0)
            ed->evento_selecionado = i;
    }
    if (ui_botao(ui, (Retangulo){r.x + 116, r.y + r.altura - 76, 100, 25}, "- EVENTO", 0))
        excluir_evento_editor(a, ed->evento_selecionado);
    if (a->projeto.quantidade_eventos <= 0) {
        desenhar_texto(ui->tela, r.x + 236, r.y + 48, "NENHUM EVENTO. USE + EVENTO PARA CRIAR.",
                       tema_ativo.texto_suave, 1);
        if (ui_botao(ui, (Retangulo){r.x + r.largura - 112, r.y + r.altura - 44, 90, 27},
                     "CONCLUIR", 0))
            a->modal = MODAL_NENHUM;
        return;
    }
    int ev = limitar_int(ed->evento_selecionado, 0, a->projeto.quantidade_eventos - 1);
    Evento *e = &a->projeto.eventos[ev];
    if (e->ativo) {
        ui_campo(ui, (Retangulo){r.x + 236, r.y + 42, 360, 28}, e->nome, sizeof(e->nome));
        e->condicao_flag = ui_numero(ui, (Retangulo){r.x + 610, r.y + 42, 190, 28}, "FLAG",
                                     e->condicao_flag, -1, MYTHARA_MAX_FLAGS - 1);
        e->condicao_valor = ui_numero(ui, (Retangulo){r.x + 808, r.y + 42, 190, 28}, "VALOR",
                                      e->condicao_valor, -999, 999);
        y = r.y + 86;
        Retangulo area_comandos = {r.x + 236, y, 370, 17 * 27};
        if (ponto_em_retangulo(ui->entrada->mouse_x, ui->entrada->mouse_y, area_comandos) &&
            ui->entrada->roda)
            ed->rolagem_comandos_evento -= ui->entrada->roda * 2;
        ed->rolagem_comandos_evento =
            limitar_int(ed->rolagem_comandos_evento, 0,
                        e->quantidade_comandos > 17 ? e->quantidade_comandos - 17 : 0);
        for (int linha_comando = 0;
             linha_comando < 17 &&
             ed->rolagem_comandos_evento + linha_comando < e->quantidade_comandos;
             ++linha_comando) {
            int i = ed->rolagem_comandos_evento + linha_comando;
            char linha[128], recuo[18];
            int profundidade = limitar_int(e->comandos[i].profundidade, 0, 8);
            memset(recuo, ' ', (size_t)profundidade * 2);
            recuo[profundidade * 2] = 0;
            snprintf(linha, sizeof(linha), "%02d %s%s", i + 1, recuo,
                     nomes_comandos[limitar_int(e->comandos[i].tipo, 0, 16)]);
            if (ui_botao(ui, (Retangulo){r.x + 236, y, 370, 25}, linha,
                         ed->comando_selecionado == i))
                ed->comando_selecionado = i;
            y += 27;
        }
        if (ui_botao(ui, (Retangulo){r.x + 236, r.y + r.altura - 76, 116, 25}, "+ COMANDO", 0) &&
            e->quantidade_comandos < MYTHARA_MAX_COMANDOS &&
            reservar_comandos(e, e->quantidade_comandos + 1)) {
            int i = e->quantidade_comandos++;
            memset(&e->comandos[i], 0, sizeof(ComandoEvento));
            e->comandos[i].tipo = COMANDO_TEXTO;
            copiar_texto(e->comandos[i].texto, sizeof(e->comandos[i].texto), "Novo dialogo");
            ed->comando_selecionado = i;
        }
        if (e->quantidade_comandos > 0) {
            int ci = limitar_int(ed->comando_selecionado, 0, e->quantidade_comandos - 1);
            ComandoEvento *c = &e->comandos[ci];
            int x = r.x + 630;
            y = r.y + 100;
            c->tipo = ui_numero(ui, (Retangulo){x, y, 370, 28}, "TIPO", c->tipo, 0, 16);
            y += 36;
            desenhar_texto(ui->tela, x, y, nomes_comandos[c->tipo], tema_ativo.destaque, 1);
            y += 24;
            desenhar_texto(ui->tela, x, y, "TEXTO / OPCOES SEPARADAS POR |", tema_ativo.texto_suave,
                           1);
            y += 16;
            ui_campo_multilinha(ui, (Retangulo){x, y, 370, 58}, c->texto, sizeof(c->texto));
            y += 68;
            TipoReferencia tipo_referencia = REFERENCIA_MAPA;
            const char *rotulo_referencia = NULL;
            if (c->tipo == COMANDO_ITEM) {
                tipo_referencia = REFERENCIA_ITEM;
                rotulo_referencia = "ITEM";
            } else if (c->tipo == COMANDO_TELEPORTE) {
                tipo_referencia = REFERENCIA_MAPA;
                rotulo_referencia = "MAPA";
            } else if (c->tipo == COMANDO_BATALHA) {
                tipo_referencia = REFERENCIA_INIMIGO;
                rotulo_referencia = "INIMIGO";
            } else if (c->tipo == COMANDO_AUDIO) {
                tipo_referencia = REFERENCIA_RECURSO;
                rotulo_referencia = "AUDIO";
            } else if (c->tipo == COMANDO_CHAMAR_EVENTO) {
                tipo_referencia = REFERENCIA_EVENTO;
                rotulo_referencia = "EVENTO";
            } else if (c->tipo == COMANDO_LOJA) {
                tipo_referencia = REFERENCIA_LOJA;
                rotulo_referencia = "LOJA";
            } else if (c->tipo == COMANDO_MISSAO) {
                tipo_referencia = REFERENCIA_MISSAO;
                rotulo_referencia = "MISSAO";
            }
            if (rotulo_referencia)
                c->a = ui_referencia(ui, (Retangulo){x, y, 370, 28}, &a->projeto, rotulo_referencia,
                                     tipo_referencia, c->a, 0);
            else
                c->a = ui_numero(ui, (Retangulo){x, y, 370, 28}, "PARAMETRO A", c->a, -1, 999);
            y += 36;
            c->b = ui_numero(ui, (Retangulo){x, y, 370, 28}, "PARAMETRO B", c->b, -999, 999);
            y += 36;
            c->c = ui_numero(ui, (Retangulo){x, y, 370, 28}, "PARAMETRO C", c->c, -999, 999);
            y += 36;
            c->profundidade =
                ui_numero(ui, (Retangulo){x, y, 370, 28}, "NIVEL DO BLOCO", c->profundidade, 0, 8);
            y += 38;
            desenhar_texto(ui->tela, x, y, "REFERENCIAS USAM NOMES; B/C SAO VALORES",
                           cor_rgb(145, 156, 171), 1);
            y += 30;
            if (ui_botao(ui, (Retangulo){x, y, 160, 27}, "EXCLUIR COMANDO", 0)) {
                memmove(c, c + 1, (size_t)(e->quantidade_comandos - ci - 1) * sizeof(*c));
                e->quantidade_comandos--;
                ed->comando_selecionado =
                    limitar_int(ci - 1, 0, e->quantidade_comandos ? e->quantidade_comandos - 1 : 0);
            }
        }
    }
    if (ui_botao(ui, (Retangulo){r.x + r.largura - 112, r.y + r.altura - 44, 90, 27}, "CONCLUIR",
                 0)) {
        ed->alterado = 1;
        a->modal = MODAL_NENHUM;
    }
}

/* Recursos e animacoes ----------------------------------------------------- */

static void modal_recursos(Aplicativo *a, Interface *ui) {
    Retangulo r = iniciar_modal(ui, "RECURSOS E ANIMACOES", 1040, 650);
    Editor *e = &a->editor;
    int y = r.y + 80, compacto = r.largura < 760;
    const char *extensoes[] = {"BMP", "WAV", "QOI", "TGA"};
    ui_campo(ui, (Retangulo){r.x + 18, r.y + 43, 270, 28}, e->busca_recursos,
             sizeof(e->busca_recursos));
    int filtrados = 0;
    for (int i = 0; i < a->projeto.quantidade_recursos; ++i)
        if (a->projeto.recursos[i].ativo &&
            texto_contem_sem_caixa(a->projeto.recursos[i].nome, e->busca_recursos))
            filtrados++;
    Retangulo area_lista = {r.x + 18, y, 270, 12 * 29};
    if (ponto_em_retangulo(ui->entrada->mouse_x, ui->entrada->mouse_y, area_lista) &&
        ui->entrada->roda)
        e->rolagem_recursos -= ui->entrada->roda * 2;
    e->rolagem_recursos = limitar_int(e->rolagem_recursos, 0, filtrados > 12 ? filtrados - 12 : 0);
    int ordem = 0, mostrados = 0;
    for (int i = 0; i < a->projeto.quantidade_recursos && mostrados < 12; ++i)
        if (a->projeto.recursos[i].ativo) {
            if (!texto_contem_sem_caixa(a->projeto.recursos[i].nome, e->busca_recursos))
                continue;
            if (ordem++ < e->rolagem_recursos)
                continue;
            char linha[96];
            snprintf(linha, sizeof(linha), "%s  %s",
                     extensoes[limitar_int(a->projeto.recursos[i].tipo, 0, 3)],
                     a->projeto.recursos[i].nome);
            if (ui_botao(ui, (Retangulo){r.x + 18, y, 270, 26}, linha,
                         e->recurso_selecionado == i)) {
                e->recurso_selecionado = i;
                e->clipe_recurso = 0;
            }
            y += 29;
            mostrados++;
        }
    desenhar_texto(ui->tela, r.x + 320, r.y + 45, "CAMINHO PARA IMPORTAR", tema_ativo.texto_suave,
                   1);
    ui_campo(ui, (Retangulo){r.x + 320, r.y + 64, compacto ? r.largura - 340 : 550, 30},
             a->campo_caminho, sizeof(a->campo_caminho));
    int tipos[] = {RECURSO_BMP, RECURSO_QOI, RECURSO_TGA, RECURSO_WAV};
    const char *rotulos[] = {"IMPORTAR BMP", "IMPORTAR QOI", "IMPORTAR TGA", "IMPORTAR WAV"};
    for (int botao = 0; botao < 4; ++botao)
        if (ui_botao(
                ui,
                (Retangulo){r.x + 320 + (botao % 2) * 145, r.y + 103 + (botao / 2) * 33, 136, 28},
                rotulos[botao], 0) &&
            a->projeto.quantidade_recursos < MYTHARA_MAX_RECURSOS &&
            reservar_memoria((void **)&a->projeto.recursos, &a->projeto.capacidade_recursos,
                             a->projeto.quantidade_recursos + 1, sizeof(Recurso),
                             MYTHARA_MAX_RECURSOS)) {
            int tipo = tipos[botao], valido = 1;
            Imagem teste = {0};
            char erro[128];
            if (tipo != RECURSO_WAV) {
                valido = carregar_imagem(tipo, a->campo_caminho, &teste, erro, sizeof(erro));
                liberar_imagem(&teste);
            } else {
                FILE *f = fopen(a->campo_caminho, "rb");
                char h[12];
                valido = f && fread(h, 12, 1, f) == 1 && !memcmp(h, "RIFF", 4) &&
                         !memcmp(h + 8, "WAVE", 4);
                if (f)
                    fclose(f);
                if (!valido)
                    copiar_texto(erro, sizeof(erro), "WAV PCM invalido.");
            }
            if (valido) {
                char relativo[MYTHARA_MAX_CAMINHO];
                if (!importar_arquivo_projeto(&a->projeto, a->campo_caminho, tipo == RECURSO_WAV,
                                              relativo, sizeof(relativo))) {
                    mensagem_aplicativo(a, "Falha ao copiar o recurso para a pasta do projeto.");
                    continue;
                }
                int i = a->projeto.quantidade_recursos++;
                Recurso *rec = &a->projeto.recursos[i];
                memset(rec, 0, sizeof(*rec));
                rec->id = novo_identificador(&a->projeto);
                rec->ativo = 1;
                rec->tipo = tipo;
                copiar_texto(rec->nome, sizeof(rec->nome), nome_base(relativo));
                copiar_texto(rec->caminho, sizeof(rec->caminho), relativo);
                rec->largura_quadro = rec->altura_quadro = a->projeto.tamanho_tile;
                rec->quantidade_quadros = 1;
                rec->duracao_quadro_ms = 160;
                rec->quantidade_clipes = 1;
                copiar_texto(rec->clipes[0].nome, sizeof(rec->clipes[0].nome), "Parado");
                rec->clipes[0].quantidade_quadros = 1;
                rec->clipes[0].duracao_ms = 160;
                rec->clipes[0].repetir = 1;
                e->recurso_selecionado = i;
                e->clipe_recurso = 0;
                e->alterado = 1;
                recarregar_recursos(a);
                mensagem_aplicativo(a, "Recurso copiado para o projeto.");
            } else
                mensagem_aplicativo(a, erro);
        }
    if (e->recurso_selecionado >= 0 && e->recurso_selecionado < a->projeto.quantidade_recursos) {
        Recurso *rec = &a->projeto.recursos[e->recurso_selecionado];
        int x = r.x + (compacto ? 320 : 620), y2 = r.y + 190;
        if (rec->tipo != RECURSO_WAV && a->imagens[e->recurso_selecionado].pixels) {
            Imagem *img = &a->imagens[e->recurso_selecionado];
            int total_quadros = rec->quantidade_quadros > 0 ? rec->quantidade_quadros : 1;
            int primeiro = 0, quantidade = total_quadros, duracao = rec->duracao_quadro_ms;
            if (rec->quantidade_clipes > 0) {
                e->clipe_recurso = limitar_int(e->clipe_recurso, 0, rec->quantidade_clipes - 1);
                ClipeAnimacao *visualizado = &rec->clipes[e->clipe_recurso];
                primeiro = limitar_int(visualizado->primeiro_quadro, 0, total_quadros - 1);
                quantidade =
                    limitar_int(visualizado->quantidade_quadros, 1, total_quadros - primeiro);
                duracao = visualizado->duracao_ms;
            }
            int quadro =
                quantidade > 0
                    ? primeiro + (int)(agora_segundos() * 1000.0 / limitar_int(duracao, 30, 5000)) %
                                     quantidade
                    : 0;
            int colunas = img->largura / limitar_int(rec->largura_quadro, 1, img->largura);
            if (colunas < 1)
                colunas = 1;
            Retangulo origem = {(quadro % colunas) * rec->largura_quadro,
                                (quadro / colunas) * rec->altura_quadro, rec->largura_quadro,
                                rec->altura_quadro};
            if (!compacto) {
                desenhar_imagem(ui->tela, img, (Retangulo){r.x + 320, r.y + 190, 260, 220}, origem);
                contornar_retangulo(ui->tela, (Retangulo){r.x + 320, r.y + 190, 260, 220},
                                    tema_ativo.borda);
            }
            int largura_controle = compacto ? r.largura - 340 : 240;
            rec->largura_quadro = ui_numero(ui, (Retangulo){x, y2, largura_controle, 27},
                                            "LARGURA QUADRO", rec->largura_quadro, 1, img->largura);
            y2 += 34;
            rec->altura_quadro = ui_numero(ui, (Retangulo){x, y2, largura_controle, 27},
                                           "ALTURA QUADRO", rec->altura_quadro, 1, img->altura);
            y2 += 34;
            rec->quantidade_quadros = ui_numero(ui, (Retangulo){x, y2, largura_controle, 27},
                                                "QUADROS", rec->quantidade_quadros, 1, 256);
            y2 += 34;
            rec->duracao_quadro_ms = ui_numero(ui, (Retangulo){x, y2, largura_controle, 27},
                                               "DURACAO MS", rec->duracao_quadro_ms, 30, 5000);
            y2 += 34;
            int quantidade_anterior = rec->quantidade_clipes;
            rec->quantidade_clipes = ui_numero(ui, (Retangulo){x, y2, largura_controle, 27},
                                               "CLIPES", rec->quantidade_clipes, 1, 16);
            for (int novo = quantidade_anterior; novo < rec->quantidade_clipes; ++novo) {
                memset(&rec->clipes[novo], 0, sizeof(rec->clipes[novo]));
                snprintf(rec->clipes[novo].nome, sizeof(rec->clipes[novo].nome), "Clipe %d",
                         novo + 1);
                rec->clipes[novo].quantidade_quadros = 1;
                rec->clipes[novo].duracao_ms = rec->duracao_quadro_ms;
                rec->clipes[novo].repetir = 1;
            }
            y2 += 34;
            e->clipe_recurso =
                ui_numero(ui, (Retangulo){x, y2, largura_controle, 27}, "CLIPE ATUAL",
                          e->clipe_recurso, 0, rec->quantidade_clipes - 1);
            ClipeAnimacao *clipe = &rec->clipes[e->clipe_recurso];
            y2 += 34;
            ui_campo(ui, (Retangulo){x, y2, largura_controle, 27}, clipe->nome,
                     sizeof(clipe->nome));
            y2 += 34;
            clipe->primeiro_quadro =
                ui_numero(ui, (Retangulo){x, y2, largura_controle, 27}, "PRIMEIRO QUADRO",
                          clipe->primeiro_quadro, 0, rec->quantidade_quadros - 1);
            y2 += 34;
            clipe->quantidade_quadros = ui_numero(ui, (Retangulo){x, y2, largura_controle, 27},
                                                  "QUADROS DO CLIPE", clipe->quantidade_quadros, 1,
                                                  rec->quantidade_quadros - clipe->primeiro_quadro);
            y2 += 34;
            clipe->duracao_ms = ui_numero(ui, (Retangulo){x, y2, largura_controle, 27},
                                          "DURACAO CLIPE", clipe->duracao_ms, 30, 5000);
            y2 += 34;
            clipe->repetir = ui_checkbox(ui, (Retangulo){x, y2, largura_controle, 22}, "REPETIR",
                                         clipe->repetir);
            y2 += 28;
            clipe->direcoes = ui_numero(ui, (Retangulo){x, y2, largura_controle, 27}, "DIRECOES",
                                        clipe->direcoes, 0, 8);
        } else if (rec->tipo == RECURSO_WAV &&
                   ui_botao(ui, (Retangulo){r.x + 320, r.y + 195, 130, 28}, "OUVIR AUDIO", 0)) {
            char caminho[MYTHARA_MAX_CAMINHO];
            caminho_do_recurso(&a->projeto, rec->caminho, caminho, sizeof(caminho));
            tocar_wav(caminho);
        }
    }
    if (ui_botao(ui, (Retangulo){r.x + 18, r.y + r.altura - 44, 120, 27}, "EXCLUIR", 0))
        excluir_recurso_editor(a, e->recurso_selecionado);
    if (ui_botao(ui, (Retangulo){r.x + r.largura - 112, r.y + r.altura - 44, 90, 27}, "CONCLUIR",
                 0))
        a->modal = MODAL_NENHUM;
}

static void modal_ajuda(Aplicativo *a, Interface *ui) {
    Retangulo r = iniciar_modal(ui, "MYTHARA - AJUDA", 720, 470);
    int x = r.x + 25, y = r.y + 50;
    const char *linhas[] = {"FLUXO RAPIDO",
                            "1. ESCOLHA UM TILE E PINTE O MAPA.",
                            "2. MARQUE PAREDES COM A FERRAMENTA COLISAO.",
                            "3. CRIE ENTIDADES E EDITE SEUS EVENTOS.",
                            "4. PRESSIONE F5 PARA TESTAR.",
                            "5. SALVE E USE EXPORTAR PARA DISTRIBUIR.",
                            "",
                            "ATALHOS DO EDITOR",
                            "CTRL+P: PALETA DE COMANDOS   F5: PLAYTEST",
                            "SETAS OU WASD: MOVER   ESPACO/E: INTERAGIR",
                            "M: GRUPO/EQUIPAMENTO   J: MISSOES",
                            "F1/F2: SALVAR/CARREGAR   ESC: EDITOR",
                            "",
                            "IMAGENS: BMP, QOI E TGA   AUDIO: WAV PCM",
                            "PROJETOS: .MYR   SAVES: .MYS   TEMAS: .MYT"};
    for (size_t i = 0; i < sizeof(linhas) / sizeof(linhas[0]); ++i) {
        desenhar_texto(ui->tela, x, y, linhas[i],
                       i == 0 || i == 7 ? cor_rgb(135, 198, 235) : cor_rgb(211, 220, 231), 1);
        y += 24;
    }
    if (ui_botao(ui, (Retangulo){r.x + r.largura - 112, r.y + r.altura - 44, 90, 27}, "FECHAR", 0))
        a->modal = MODAL_NENHUM;
}

static void editor_painel_inferior(Aplicativo *a, Interface *ui, Retangulo r) {
    Editor *e = &a->editor;
    Mapa *m = &a->projeto.mapas[e->mapa_atual];
    ui_painel(ui->tela, r, "TILES, AUTOTILE, CARIMBOS E MINIMAPA");
    int y = r.y + 35;
    for (int i = 0; i < 32; ++i) {
        Retangulo q = {r.x + 10 + (i % 16) * 30, y + (i / 16) * 30, 26, 26};
        desenhar_tile_recurso(a, ui->tela, q, i, 0);
        if (e->tile == i)
            contornar_retangulo(ui->tela, (Retangulo){q.x - 2, q.y - 2, 30, 30}, tema_ativo.aviso);
        if (ponto_em_retangulo(ui->entrada->mouse_x, ui->entrada->mouse_y, q) &&
            ui->entrada->mouse_pressionado)
            e->tile = i;
    }
    e->autotile =
        ui_checkbox(ui, (Retangulo){r.x + 10, y + 68, 130, 20}, "AUTOTILE 4 LADOS", e->autotile);
    if (ui_botao(ui, (Retangulo){r.x + 160, y + 64, 146, 25}, "CRIAR CARIMBO", 0))
        copiar_selecao_para_carimbo(a);
    char info[128];
    snprintf(info, sizeof(info), "TILE %d  |  %s  |  %dx%d", e->tile, m->camadas[e->camada].nome,
             m->largura, m->altura);
    desenhar_texto(ui->tela, r.x + 325, y + 72, info, tema_ativo.texto_suave, 1);
    int mw = 170, mh = r.altura - 38, mx = r.x + r.largura - mw - 12, my = r.y + 31;
    preencher_retangulo(ui->tela, (Retangulo){mx, my, mw, mh}, tema_ativo.fundo);
    int sx = mw / m->largura, sy = mh / m->altura, escala = sx < sy ? sx : sy;
    if (escala < 1)
        escala = 1;
    for (int yy = 0; yy < m->altura && yy * escala < mh; ++yy)
        for (int xx = 0; xx < m->largura && xx * escala < mw; ++xx) {
            int tile = m->camadas[0].tiles[yy * m->largura + xx];
            preencher_retangulo(ui->tela,
                                (Retangulo){mx + xx * escala, my + yy * escala, escala, escala},
                                paleta_tiles[tile & 15]);
        }
    contornar_retangulo(ui->tela, (Retangulo){mx, my, mw, mh}, tema_ativo.borda);
}

static Cor *cor_tema_por_indice(int indice) {
    Cor *cores[] = {&tema_ativo.fundo,    &tema_ativo.painel,         &tema_ativo.painel_elevado,
                    &tema_ativo.controle, &tema_ativo.controle_sobre, &tema_ativo.selecao,
                    &tema_ativo.borda,    &tema_ativo.texto,          &tema_ativo.texto_suave,
                    &tema_ativo.destaque, &tema_ativo.perigo,         &tema_ativo.aviso,
                    &tema_ativo.sucesso};
    return cores[limitar_int(indice, 0, 12)];
}

/* Tema e preferencias visuais ---------------------------------------------- */

static void modal_tema(Aplicativo *a, Interface *ui) {
    Retangulo r = iniciar_modal(ui, "EDITOR COMPLETO DE TEMA", 900, 590);
    const char *nomes[] = {"FUNDO",   "PAINEL", "PAINEL ELEVADO", "CONTROLE",    "CONTROLE SOBRE",
                           "SELECAO", "BORDA",  "TEXTO",          "TEXTO SUAVE", "DESTAQUE",
                           "PERIGO",  "AVISO",  "SUCESSO"};
    int y = r.y + 44, compacto = r.largura < 760;
    if (ui_botao(ui, (Retangulo){r.x + r.largura - 258, r.y + 38, 112, 26},
                 a->idioma_editor == IDIOMA_INGLES ? "EDITOR: EN" : "EDITOR: PT", 0)) {
        a->idioma_editor = a->idioma_editor == IDIOMA_INGLES ? IDIOMA_PORTUGUES : IDIOMA_INGLES;
        idioma_definir((IdiomaMythara)a->idioma_editor);
    }
    if (ui_botao(ui, (Retangulo){r.x + r.largura - 138, r.y + 38, 112, 26},
                 a->projeto.idioma_interface == IDIOMA_INGLES ? "JOGO: EN" : "JOGO: PT", 0)) {
        a->projeto.idioma_interface =
            a->projeto.idioma_interface == IDIOMA_INGLES ? IDIOMA_PORTUGUES : IDIOMA_INGLES;
        a->editor.alterado = 1;
    }
    for (int i = 0; i < 13; ++i) {
        Cor *cor = cor_tema_por_indice(i);
        int coluna = compacto ? i / 7 : 0, linha = compacto ? i % 7 : i,
            base_x = r.x + 20 + coluna * 135;
        Retangulo amostra = {base_x, y + linha * 32, 22, 24};
        preencher_retangulo(ui->tela, amostra, *cor);
        contornar_retangulo(ui->tela, amostra, tema_ativo.borda);
        if (ui_botao(ui, (Retangulo){base_x + 27, y + linha * 32, compacto ? 102 : 210, 24},
                     nomes[i], a->editor.cor_tema_selecionada == i))
            a->editor.cor_tema_selecionada = i;
    }
    Cor *cor = cor_tema_por_indice(a->editor.cor_tema_selecionada);
    int rr = (*cor >> 16) & 255, gg = (*cor >> 8) & 255, bb = *cor & 255;
    int x = r.x + 300;
    y = r.y + 58;
    desenhar_texto(ui->tela, x, y, "COR SELECIONADA", tema_ativo.destaque, 1);
    y += 24;
    rr = ui_numero(ui, (Retangulo){x, y, 300, 28}, "VERMELHO", rr, 0, 255);
    y += 38;
    gg = ui_numero(ui, (Retangulo){x, y, 300, 28}, "VERDE", gg, 0, 255);
    y += 38;
    bb = ui_numero(ui, (Retangulo){x, y, 300, 28}, "AZUL", bb, 0, 255);
    y += 42;
    *cor = cor_rgb((unsigned)rr, (unsigned)gg, (unsigned)bb);
    tema_ativo.escala_percentual = ui_numero(ui, (Retangulo){x, y, 300, 28}, "ESCALA %",
                                             tema_ativo.escala_percentual, 100, 200);
    y += 38;
    tema_ativo.espacamento =
        ui_numero(ui, (Retangulo){x, y, 300, 28}, "ESPACAMENTO", tema_ativo.espacamento, 2, 16);
    y += 38;
    tema_ativo.altura_controle =
        ui_numero(ui, (Retangulo){x, y, 300, 28}, "ALTURA", tema_ativo.altura_controle, 22, 44);
    y += 48;
    if (!compacto) {
        preencher_retangulo(ui->tela, (Retangulo){x, y, 520, 110}, tema_ativo.painel);
        desenhar_texto(ui->tela, x + 14, y + 14, "PRE-VISUALIZACAO AO VIVO", tema_ativo.texto, 1);
        ui_botao(ui, (Retangulo){x + 14, y + 42, 150, 30}, "BOTAO NORMAL", 0);
        ui_botao(ui, (Retangulo){x + 174, y + 42, 150, 30}, "SELECIONADO", 1);
        ui_campo(ui, (Retangulo){x + 14, y + 78, 310, 25}, tema_ativo.nome,
                 sizeof(tema_ativo.nome));
    }
    if (ui_botao(ui, (Retangulo){r.x + 20, r.y + r.altura - 45, 130, 27}, "RESTAURAR", 0))
        iniciar_tema_padrao();
    if (!a->campo_tema[0])
        copiar_texto(a->campo_tema, sizeof(a->campo_tema), "tema_mythara.myt");
    if (!compacto)
        ui_campo(ui, (Retangulo){r.x + 165, r.y + r.altura - 45, 285, 27}, a->campo_tema,
                 sizeof(a->campo_tema));
    if (!compacto &&
        ui_botao(ui, (Retangulo){r.x + 460, r.y + r.altura - 45, 105, 27}, "IMPORTAR", 0)) {
        mensagem_aplicativo(a, carregar_tema_arquivo(a->campo_tema)
                                   ? "Tema importado."
                                   : "Nao foi possivel importar o tema.");
    }
    if (!compacto &&
        ui_botao(ui, (Retangulo){r.x + 575, r.y + r.altura - 45, 105, 27}, "EXPORTAR", 0)) {
        mensagem_aplicativo(a, salvar_tema_arquivo(a->campo_tema)
                                   ? "Tema exportado."
                                   : "Nao foi possivel exportar o tema.");
    }
    if (ui_botao(ui, (Retangulo){r.x + r.largura - 112, r.y + r.altura - 45, 90, 27}, "CONCLUIR",
                 0))
        a->modal = MODAL_NENHUM;
}

/* Recuperacao, migracao e validacao ---------------------------------------- */

static void modal_recuperacao(Aplicativo *a, Interface *ui) {
    Retangulo r = iniciar_modal(ui, "RECUPERACAO DE SESSAO", 620, 230);
    desenhar_texto(ui->tela, r.x + 24, r.y + 55,
                   "FOI ENCONTRADO UM AUTOSAVE MAIS NOVO QUE O PROJETO.", tema_ativo.aviso, 1);
    desenhar_texto(ui->tela, r.x + 24, r.y + 82,
                   "Recupere a sessao ou continue com o ultimo salvamento manual.",
                   tema_ativo.texto_suave, 1);
    desenhar_texto(ui->tela, r.x + 24, r.y + 110, a->caminho_autosave, tema_ativo.texto_suave, 1);
    if (ui_botao(ui, (Retangulo){r.x + r.largura - 240, r.y + r.altura - 55, 100, 30}, "IGNORAR",
                 0)) {
        a->oferecer_recuperacao = 0;
        a->modal = MODAL_NENHUM;
    }
    if (ui_botao(ui, (Retangulo){r.x + r.largura - 130, r.y + r.altura - 55, 105, 30}, "RECUPERAR",
                 0)) {
        if (recuperar_autosave(a))
            a->modal = MODAL_NENHUM;
    }
}

static void modal_migracao(Aplicativo *a, Interface *ui) {
    Retangulo r = iniciar_modal(ui, "MIGRAR PROJETO V3 PARA V4", 760, 300);
    desenhar_texto(ui->tela, r.x + 24, r.y + 50, "O ARQUIVO ORIGINAL SERA PRESERVADO.",
                   tema_ativo.sucesso, 1);
    desenhar_texto(ui->tela, r.x + 24, r.y + 82, "ORIGEM V3", tema_ativo.texto_suave, 1);
    desenhar_texto(ui->tela, r.x + 24, r.y + 103, a->caminho_migracao_origem, tema_ativo.texto, 1);
    desenhar_texto(ui->tela, r.x + 24, r.y + 139, "NOVA COPIA V4", tema_ativo.texto_suave, 1);
    ui_campo(ui, (Retangulo){r.x + 24, r.y + 158, r.largura - 48, 30}, a->caminho_migracao_destino,
             sizeof(a->caminho_migracao_destino));
    desenhar_texto(ui->tela, r.x + 24, r.y + 204,
                   "Referencias por indice serao convertidas em IDs estaveis.",
                   tema_ativo.texto_suave, 1);
    if (ui_botao(ui, (Retangulo){r.x + r.largura - 245, r.y + r.altura - 52, 100, 30}, "CANCELAR",
                 0)) {
        a->modal = MODAL_NENHUM;
        a->caminho_migracao_origem[0] = 0;
        return;
    }
    if (ui_botao(ui, (Retangulo){r.x + r.largura - 133, r.y + r.altura - 52, 108, 30},
                 "CRIAR COPIA", 0)) {
        if (!strcmp(a->caminho_migracao_origem, a->caminho_migracao_destino)) {
            mensagem_aplicativo(a, "Escolha outro nome; a origem v3 nunca e sobrescrita.");
            return;
        }
        if (access(a->caminho_migracao_destino, F_OK) == 0) {
            mensagem_aplicativo(a, "O destino ja existe; escolha outro nome.");
            return;
        }
        Projeto migrado = {0};
        char erro[192];
        if (!carregar_projeto_v3_de(&migrado, a->caminho_migracao_origem, erro, sizeof(erro))) {
            mensagem_aplicativo(a, erro);
            return;
        }
        atribuir_ids_ausentes(&migrado);
        if (!validar_projeto(&migrado, erro, sizeof(erro)) ||
            !salvar_projeto_em(&migrado, a->caminho_migracao_destino, erro, sizeof(erro))) {
            liberar_projeto(&migrado);
            mensagem_aplicativo(a, erro);
            return;
        }
        char primeiro_aviso[160];
        int mapas = migrado.quantidade_mapas, eventos = migrado.quantidade_eventos,
            recursos = migrado.quantidade_recursos,
            avisos = avisos_projeto(&migrado, primeiro_aviso, sizeof(primeiro_aviso));
        liberar_projeto(&migrado);
        char destino[MYTHARA_MAX_CAMINHO];
        copiar_texto(destino, sizeof(destino), a->caminho_migracao_destino);
        a->caminho_migracao_origem[0] = 0;
        a->modal = MODAL_NENHUM;
        if (abrir_projeto_atual(a, destino)) {
            char relatorio[192];
            snprintf(relatorio, sizeof(relatorio),
                     "Migracao concluida: %d mapa(s), %d evento(s), %d recurso(s); %d aviso(s).",
                     mapas, eventos, recursos, avisos);
            mensagem_aplicativo(a, relatorio);
        }
    }
}

static void modal_validacao(Aplicativo *a, Interface *ui) {
    Retangulo r = iniciar_modal(ui, "VALIDADOR DO PROJETO", 720, 260);
    desenhar_texto(ui->tela, r.x + 24, r.y + 51, "ERRO QUE BLOQUEIA PLAYTEST E EXPORTACAO",
                   tema_ativo.perigo, 1);
    desenhar_texto(ui->tela, r.x + 24, r.y + 85, a->erro_validacao, tema_ativo.texto, 1);
    desenhar_texto(ui->tela, r.x + 24, r.y + 118,
                   "Use CORRIGIR para abrir o editor relacionado ao problema.",
                   tema_ativo.texto_suave, 1);
    if (ui_botao(ui, (Retangulo){r.x + r.largura - 235, r.y + r.altura - 52, 98, 30}, "FECHAR", 0))
        a->modal = MODAL_NENHUM;
    if (ui_botao(ui, (Retangulo){r.x + r.largura - 125, r.y + r.altura - 52, 100, 30}, "CORRIGIR",
                 0)) {
        if (strstr(a->erro_validacao, "Evento"))
            a->modal = MODAL_EVENTOS;
        else if (strstr(a->erro_validacao, "Recurso"))
            a->modal = MODAL_RECURSOS;
        else if (strstr(a->erro_validacao, "Heroi") || strstr(a->erro_validacao, "Classe") ||
                 strstr(a->erro_validacao, "Habilidade") || strstr(a->erro_validacao, "Item") ||
                 strstr(a->erro_validacao, "Inimigo") || strstr(a->erro_validacao, "Estado") ||
                 strstr(a->erro_validacao, "Loja") || strstr(a->erro_validacao, "Missao"))
            a->modal = MODAL_BANCO;
        else
            a->modal = MODAL_NENHUM;
    }
}

static void concluir_descarte(Aplicativo *a) {
    AcaoDescarte acao = a->acao_descarte;
    a->acao_descarte = DESCARTE_NENHUM;
    a->modal = MODAL_NENHUM;
    if (acao == DESCARTE_ABRIR) {
        char caminho[MYTHARA_MAX_CAMINHO];
        copiar_texto(caminho, sizeof(caminho), a->caminho_pendente);
        a->caminho_pendente[0] = 0;
        abrir_projeto_atual(a, caminho);
    } else if (acao == DESCARTE_SAIR)
        a->encerrar_confirmado = 1;
}

static void modal_descartar(Aplicativo *a, Interface *ui) {
    const char *titulo = a->acao_descarte == DESCARTE_SAIR ? "SAIR DA MYTHARA" : "ABRIR PROJETO";
    Retangulo r = iniciar_modal(ui, titulo, 670, 235);
    desenhar_texto(ui->tela, r.x + 24, r.y + 55, "HA ALTERACOES NAO SALVAS.", tema_ativo.aviso, 1);
    desenhar_texto(ui->tela, r.x + 24, r.y + 85,
                   "Salve o projeto atual, descarte as alteracoes ou cancele.",
                   tema_ativo.texto_suave, 1);
    if (ui_botao(ui, (Retangulo){r.x + 24, r.y + r.altura - 56, 145, 31}, "SALVAR E CONTINUAR",
                 0) &&
        salvar_projeto_atual(a))
        concluir_descarte(a);
    if (ui_botao(ui, (Retangulo){r.x + r.largura - 305, r.y + r.altura - 56, 125, 31}, "DESCARTAR",
                 0))
        concluir_descarte(a);
    if (ui_botao(ui, (Retangulo){r.x + r.largura - 168, r.y + r.altura - 56, 143, 31}, "CANCELAR",
                 0)) {
        a->acao_descarte = DESCARTE_NENHUM;
        a->caminho_pendente[0] = 0;
        a->modal = MODAL_NENHUM;
    }
}

/* Boas-vindas e paleta de comandos ---------------------------------------- */

static void modal_boas_vindas(Aplicativo *a, Interface *ui) {
    int largura = limitar_int(ui->tela->largura - 40, 420, 760),
        altura = limitar_int(ui->tela->altura - 40, 360, 500);
    Retangulo r = iniciar_modal(ui, "BEM-VINDO A MYTHARA 4", largura, altura);
    int x = r.x + 28, y = r.y + 50;
    desenhar_texto(ui->tela, x, y, "CRIE SUA LENDA, UMA CENA DE CADA VEZ.", tema_ativo.destaque, 1);
    y += 28;
    desenhar_texto(ui->tela, x, y,
                   "O projeto de exemplo ja inclui herois, NPCs, batalha, loja e missao.",
                   tema_ativo.texto_suave, 1);
    y += 42;
    if (a->caminho_recente[0] && r.largura >= 570) {
        int rx = r.x + 270, rw = r.largura - 298;
        desenhar_texto(ui->tela, rx, y, "PROJETO RECENTE", tema_ativo.texto_suave, 1);
        char recente[48];
        copiar_texto(recente, sizeof(recente), nome_base(a->caminho_recente));
        desenhar_texto(ui->tela, rx, y + 22, recente, tema_ativo.texto, 1);
        if (ui_botao(ui, (Retangulo){rx, y + 48, rw, 34}, "ABRIR ULTIMO", 0) &&
            abrir_projeto_atual(a, a->caminho_recente)) {
            a->modal = MODAL_NENHUM;
            a->mostrar_boas_vindas = 0;
        }
    }
    if (ui_botao(ui, (Retangulo){x, y, 210, 34}, "CONTINUAR NO EXEMPLO", 0)) {
        a->modal = MODAL_NENHUM;
        a->mostrar_boas_vindas = 0;
    }
    y += 44;
    if (ui_botao(ui, (Retangulo){x, y, 210, 34}, "CRIAR PROJETO LIMPO", 0)) {
        novo_projeto_limpo(a);
        a->modal = MODAL_NENHUM;
        a->mostrar_boas_vindas = 0;
    }
    y += 44;
    if (ui_botao(ui, (Retangulo){x, y, 210, 34}, "ABRIR PROJETO V4", 0)) {
        a->acao_caminho = ACAO_ABRIR;
        copiar_texto(a->campo_caminho, sizeof(a->campo_caminho), ".");
        a->modal = MODAL_CAMINHO;
        a->mostrar_boas_vindas = 0;
    }
    y += 54;
    a->dicas_ativas =
        ui_checkbox(ui, (Retangulo){x, y, 310, 22}, "MOSTRAR DICAS CONTEXTUAIS", a->dicas_ativas);
    desenhar_texto(ui->tela, r.x + 28, r.y + r.altura - 42,
                   "ATALHOS: CTRL+P COMANDOS  |  F5 PLAYTEST  |  CTRL+S SALVAR",
                   tema_ativo.texto_suave, 1);
}

static void executar_acao_paleta(Aplicativo *a, int acao) {
    if (acao == 0)
        salvar_projeto_atual(a);
    else if (acao == 1) {
        a->modal = MODAL_EVENTOS;
        return;
    } else if (acao == 2) {
        a->modal = MODAL_BANCO;
        return;
    } else if (acao == 3) {
        a->modal = MODAL_RECURSOS;
        return;
    } else if (acao == 4) {
        a->modal = MODAL_TEMA;
        return;
    } else if (acao == 5)
        desfazer_edicao(a);
    else if (acao == 6)
        refazer_edicao(a);
    else if (acao == 7)
        iniciar_playtest_editor(a);
    a->modal = MODAL_NENHUM;
}

static void modal_comandos(Aplicativo *a, Interface *ui) {
    Retangulo r = iniciar_modal(ui, "PALETA DE COMANDOS  CTRL+P", 620, 430);
    ui_campo(ui, (Retangulo){r.x + 20, r.y + 44, r.largura - 40, 30}, a->editor.busca_comandos,
             sizeof(a->editor.busca_comandos));
    const char *acoes[] = {"Salvar projeto",
                           "Abrir editor de eventos",
                           "Abrir banco de dados",
                           "Abrir recursos",
                           "Editar tema",
                           "Desfazer",
                           "Refazer",
                           "Iniciar playtest"};
    int y = r.y + 88;
    for (int i = 0; i < 8; ++i)
        if (texto_contem_sem_caixa(acoes[i], a->editor.busca_comandos)) {
            if (ui_botao(ui, (Retangulo){r.x + 20, y, r.largura - 40, 30}, acoes[i], 0))
                executar_acao_paleta(a, i);
            y += 34;
        }
}

/* Composicao final do workspace ------------------------------------------- */

static void desenhar_editor(Aplicativo *a, Interface *ui) {
    Editor *e = &a->editor;
    if (a->modal == MODAL_NENHUM && e->layout_ativo != LAYOUT_MAPA)
        e->layout_ativo = LAYOUT_MAPA;
    editor_barra_superior(a, ui);
    e->largura_arvore = limitar_int(e->largura_arvore, 190, 360);
    e->largura_inspetor = limitar_int(e->largura_inspetor, 250, 420);
    e->altura_inferior = limitar_int(e->altura_inferior, 95, 240);
    int base = ui->tela->altura - 28, compacto = ui->tela->largura < 900,
        centro_x = compacto ? 0 : e->largura_arvore + 4,
        centro_largura = compacto ? ui->tela->largura
                                  : ui->tela->largura - e->largura_arvore - e->largura_inspetor - 8;
    if (!compacto && centro_largura < 320) {
        compacto = 1;
        centro_x = 0;
        centro_largura = ui->tela->largura;
    }
    int divisor_direito = centro_x + centro_largura, divisor_baixo = base - e->altura_inferior;
    Retangulo div_esq = {e->largura_arvore, 70, 4, base - 70},
              div_dir = {divisor_direito, 70, 4, base - 70},
              div_baixo = {centro_x, divisor_baixo - 4, centro_largura, 4};
    Entrada *in = ui->entrada;
    if (in->mouse_pressionado) {
        if (!compacto && ponto_em_retangulo(in->mouse_x, in->mouse_y, div_esq))
            e->divisor_ativo = 1;
        else if (!compacto && ponto_em_retangulo(in->mouse_x, in->mouse_y, div_dir))
            e->divisor_ativo = 2;
        else if (ponto_em_retangulo(in->mouse_x, in->mouse_y, div_baixo))
            e->divisor_ativo = 3;
    }
    if (in->mouse_baixo) {
        if (e->divisor_ativo == 1)
            e->largura_arvore = in->mouse_x;
        if (e->divisor_ativo == 2)
            e->largura_inspetor = ui->tela->largura - in->mouse_x;
        if (e->divisor_ativo == 3)
            e->altura_inferior = base - in->mouse_y;
    }
    if (in->mouse_solto)
        e->divisor_ativo = 0;
    if (!compacto) {
        editor_painel_esquerdo(a, ui, (Retangulo){0, 70, e->largura_arvore, base - 70});
        editor_inspetor(a, ui,
                        (Retangulo){divisor_direito + 4, 70, e->largura_inspetor, base - 70});
    }
    preencher_retangulo(ui->tela, (Retangulo){centro_x, 70, centro_largura, 30},
                        tema_ativo.painel_elevado);
    int tx = centro_x + 5;
    if (compacto) {
        if (ui_botao(ui, (Retangulo){5, 73, 62, 24}, "ARVORE", e->gaveta_arvore))
            e->gaveta_arvore = !e->gaveta_arvore;
        if (ui_botao(ui, (Retangulo){71, 73, 68, 24}, "INSPETOR", e->gaveta_inspetor))
            e->gaveta_inspetor = !e->gaveta_inspetor;
        tx = 144;
    }
    for (int i = 0; i < a->projeto.quantidade_mapas && i < 8; ++i) {
        int w = limitar_int((int)strlen(a->projeto.mapas[i].nome) * 6 + 24, 70, 150);
        if (tx + w > divisor_direito)
            break;
        if (ui_botao(ui, (Retangulo){tx, 73, w, 24}, a->projeto.mapas[i].nome,
                     e->mapa_atual == i)) {
            e->mapa_atual = i;
            e->entidade_selecionada = -1;
            e->camada = 0;
        }
        tx += w + 3;
    }
    editor_canvas(a, ui, (Retangulo){centro_x, 100, centro_largura, divisor_baixo - 104});
    editor_painel_inferior(
        a, ui, (Retangulo){centro_x, divisor_baixo, centro_largura, e->altura_inferior});
    if (compacto) {
        if (e->gaveta_arvore)
            editor_painel_esquerdo(
                a, ui,
                (Retangulo){0, 100, limitar_int(e->largura_arvore, 190, ui->tela->largura - 40),
                            base - 100});
        if (e->gaveta_inspetor) {
            int w = limitar_int(e->largura_inspetor, 250, ui->tela->largura - 40);
            editor_inspetor(a, ui, (Retangulo){ui->tela->largura - w, 100, w, base - 100});
        }
    } else {
        preencher_retangulo(ui->tela, div_esq,
                            e->divisor_ativo == 1 ? tema_ativo.destaque : tema_ativo.borda);
        preencher_retangulo(ui->tela, div_dir,
                            e->divisor_ativo == 2 ? tema_ativo.destaque : tema_ativo.borda);
    }
    preencher_retangulo(ui->tela, div_baixo,
                        e->divisor_ativo == 3 ? tema_ativo.destaque : tema_ativo.borda);
    preencher_retangulo(ui->tela, (Retangulo){0, base, ui->tela->largura, 28},
                        tema_ativo.painel_elevado);
    char status[800];
    snprintf(
        status, sizeof(status), "%s%.360s   |   %.47s   |   %.47s   |   ZOOM %d%%   |   %.250s",
        e->alterado ? "* " : "", a->caminho_projeto, a->projeto.mapas[e->mapa_atual].nome,
        a->projeto.mapas[e->mapa_atual].camadas[e->camada].nome, e->zoom * 100 / 32, a->mensagem);
    desenhar_texto(ui->tela, 8, base + 10, status, tema_ativo.texto_suave, 1);
    if (a->modal == MODAL_NENHUM && ui->id_foco == 0) {
        Entrada *atalho = ui->entrada;
        if (atalho->controle && atalho->teclas_pressionadas['Z'])
            desfazer_edicao(a);
        if (atalho->controle && atalho->teclas_pressionadas['Y'])
            refazer_edicao(a);
        if (e->canvas_em_foco && atalho->controle && atalho->teclas_pressionadas['A']) {
            Mapa *m = &a->projeto.mapas[e->mapa_atual];
            e->selecao_x0 = e->selecao_y0 = 0;
            e->selecao_x1 = m->largura - 1;
            e->selecao_y1 = m->altura - 1;
            e->selecao_ativa = 1;
        }
        if (e->canvas_em_foco && atalho->controle && atalho->teclas_pressionadas['C'])
            copiar_area_transferencia(a, 0);
        if (e->canvas_em_foco && atalho->controle && atalho->teclas_pressionadas['X'])
            copiar_area_transferencia(a, 1);
        if (e->canvas_em_foco && atalho->controle && atalho->teclas_pressionadas['V'])
            colar_area_transferencia(a, e->cursor_tile_x, e->cursor_tile_y);
        if (e->canvas_em_foco && atalho->controle && atalho->teclas_pressionadas['D']) {
            copiar_area_transferencia(a, 0);
            colar_area_transferencia(a, e->selecao_x0 + 1, e->selecao_y0 + 1);
        }
        if (e->canvas_em_foco && atalho->teclas_pressionadas[127])
            apagar_selecao_editor(a);
    }
    if (a->modal == MODAL_CAMINHO)
        modal_caminho(a, ui);
    else if (a->modal == MODAL_NOVO)
        modal_novo(a, ui);
    else if (a->modal == MODAL_BANCO)
        modal_banco(a, ui);
    else if (a->modal == MODAL_EVENTOS)
        modal_eventos(a, ui);
    else if (a->modal == MODAL_RECURSOS)
        modal_recursos(a, ui);
    else if (a->modal == MODAL_AJUDA)
        modal_ajuda(a, ui);
    else if (a->modal == MODAL_TEMA)
        modal_tema(a, ui);
    else if (a->modal == MODAL_COMANDOS)
        modal_comandos(a, ui);
    else if (a->modal == MODAL_RECUPERACAO)
        modal_recuperacao(a, ui);
    else if (a->modal == MODAL_BOAS_VINDAS)
        modal_boas_vindas(a, ui);
    else if (a->modal == MODAL_MIGRACAO)
        modal_migracao(a, ui);
    else if (a->modal == MODAL_VALIDACAO)
        modal_validacao(a, ui);
    else if (a->modal == MODAL_DESCARTAR)
        modal_descartar(a, ui);
    if (a->dicas_ativas && ui->rotulo_quente && !ui->entrada->mouse_baixo) {
        int w = limitar_int((int)strlen(ui->rotulo_quente) * 6 + 16, 60, 260),
            x = limitar_int(ui->entrada->mouse_x + 14, 0, ui->tela->largura - w),
            y = limitar_int(ui->entrada->mouse_y + 18, 0, ui->tela->altura - 24);
        preencher_retangulo(ui->tela, (Retangulo){x, y, w, 22}, tema_ativo.painel_elevado);
        contornar_retangulo(ui->tela, (Retangulo){x, y, w, 22}, tema_ativo.destaque);
        desenhar_texto(ui->tela, x + 8, y + 8, ui->rotulo_quente, tema_ativo.texto, 1);
    }
    if (ui->entrada->teclas_pressionadas[27])
        a->modal = MODAL_NENHUM;
}
