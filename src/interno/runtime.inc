/* ========================================================================== */
/* Runtime de jogo, eventos e combate                                          */
/* ========================================================================== */

static void iniciar_evento_jogo(Aplicativo *a, int indice) {
    if (indice < 0 || indice >= a->projeto.quantidade_eventos || !a->projeto.eventos[indice].ativo)
        return;
    Evento *e = &a->projeto.eventos[indice];
    if (e->condicao_flag >= 0 && e->condicao_flag < MYTHARA_MAX_FLAGS &&
        a->jogo.estado.flags[e->condicao_flag] != e->condicao_valor)
        return;
    a->jogo.evento_ativo = indice;
    a->jogo.proximo_comando = 0;
    a->jogo.aguardando_texto = 0;
    a->jogo.aguardando_escolha = 0;
}

static void separar_escolhas(const char *texto, char *a, size_t ca, char *b, size_t cb) {
    const char *barra = strchr(texto, '|');
    if (!barra) {
        copiar_texto(a, ca, texto[0] ? texto : "Sim");
        copiar_texto(b, cb, "Nao");
        return;
    }
    size_t n = (size_t)(barra - texto);
    if (n >= ca)
        n = ca - 1;
    memcpy(a, texto, n);
    a[n] = 0;
    copiar_texto(b, cb, barra + 1);
}

static void preparar_batalha(Aplicativo *a, int inimigo, int quantidade) {
    Jogo *j = &a->jogo;
    if (!a->projeto.quantidade_inimigos)
        return;
    j->quantidade_inimigos_batalha = limitar_int(quantidade ? quantidade : 1, 1, 8);
    for (int i = 0; i < j->quantidade_inimigos_batalha; ++i) {
        int id = limitar_int(inimigo + i, 0, a->projeto.quantidade_inimigos - 1);
        j->inimigos_batalha[i] = id;
        j->vidas_inimigos[i] = a->projeto.inimigos[id].vida;
    }
    j->inimigo = j->inimigos_batalha[0];
    j->vida_inimigo = j->vidas_inimigos[0];
    j->membro_escolhendo = 0;
    memset(j->acoes_grupo, 0, sizeof(j->acoes_grupo));
    memset(j->defesas_grupo, 0, sizeof(j->defesas_grupo));
    a->modo = MODO_BATALHA;
}

static int procurar_limite_bloco(Evento *e, int inicio, int nivel, int aceitar_senao) {
    for (int i = inicio; i < e->quantidade_comandos; ++i) {
        ComandoEvento *c = &e->comandos[i];
        if (c->profundidade == nivel &&
            ((aceitar_senao && c->tipo == COMANDO_SENAO) || c->tipo == COMANDO_FIM_BLOCO))
            return i;
    }
    return e->quantidade_comandos;
}

static void executar_evento(Aplicativo *a) {
    Jogo *j = &a->jogo;
    if (j->esperar_evento_ate > 0.0) {
        if (agora_segundos() < j->esperar_evento_ate)
            return;
        j->esperar_evento_ate = 0.0;
    }
    while (j->evento_ativo >= 0 && !j->aguardando_texto && !j->aguardando_escolha &&
           a->modo == MODO_JOGO) {
        Evento *e = &a->projeto.eventos[j->evento_ativo];
        if (j->proximo_comando >= e->quantidade_comandos) {
            if (j->topo_pilha > 0) {
                j->topo_pilha--;
                j->evento_ativo = j->pilha_eventos[j->topo_pilha];
                j->proximo_comando = j->pilha_comandos[j->topo_pilha];
                continue;
            }
            j->evento_ativo = -1;
            break;
        }
        ComandoEvento *c = &e->comandos[j->proximo_comando++];
        switch (c->tipo) {
        case COMANDO_TEXTO:
            copiar_texto(j->texto, sizeof(j->texto), c->texto);
            j->aguardando_texto = 1;
            break;
        case COMANDO_ESCOLHA:
            separar_escolhas(c->texto, j->escolha_a, sizeof(j->escolha_a), j->escolha_b,
                             sizeof(j->escolha_b));
            j->aguardando_escolha = 1;
            break;
        case COMANDO_FLAG:
            if (c->a >= 0 && c->a < MYTHARA_MAX_FLAGS)
                j->estado.flags[c->a] = c->b;
            break;
        case COMANDO_VARIAVEL:
            if (c->a >= 0 && c->a < MYTHARA_MAX_VARIAVEIS)
                j->estado.variaveis[c->a] += c->b;
            break;
        case COMANDO_ITEM:
            if (c->a >= 0 && c->a < a->projeto.quantidade_itens)
                j->estado.inventario[c->a] = limitar_int(j->estado.inventario[c->a] + c->b, 0, 99);
            break;
        case COMANDO_TELEPORTE:
            if (c->a >= 0 && c->a < a->projeto.quantidade_mapas) {
                Mapa *m = &a->projeto.mapas[c->a];
                j->estado.mapa = c->a;
                j->estado.x = limitar_int(c->b, 0, m->largura - 1);
                j->estado.y = limitar_int(c->c, 0, m->altura - 1);
            }
            break;
        case COMANDO_BATALHA:
            preparar_batalha(a, c->a, c->b);
            break;
        case COMANDO_CURAR:
            j->estado.vida = limitar_int(j->estado.vida + (c->a ? c->a : j->estado.vida_maxima), 0,
                                         j->estado.vida_maxima);
            break;
        case COMANDO_AUDIO:
            if (c->a >= 0 && c->a < a->projeto.quantidade_recursos &&
                a->projeto.recursos[c->a].tipo == RECURSO_WAV) {
                char caminho[MYTHARA_MAX_CAMINHO];
                caminho_do_recurso(&a->projeto, a->projeto.recursos[c->a].caminho, caminho,
                                   sizeof(caminho));
                tocar_wav(caminho);
            }
            break;
        case COMANDO_ESPERAR:
            j->esperar_evento_ate = agora_segundos() + (double)limitar_int(c->a, 0, 10000) / 1000.0;
            return;
        case COMANDO_SE:
            if (c->a < 0 || c->a >= MYTHARA_MAX_FLAGS || j->estado.flags[c->a] != c->b)
                j->proximo_comando =
                    procurar_limite_bloco(e, j->proximo_comando, c->profundidade, 1) + 1;
            break;
        case COMANDO_SENAO:
            j->proximo_comando =
                procurar_limite_bloco(e, j->proximo_comando, c->profundidade, 0) + 1;
            break;
        case COMANDO_FIM_BLOCO:
            if (j->topo_repeticao > 0 &&
                j->pilha_repeticao_nivel[j->topo_repeticao - 1] == c->profundidade) {
                int topo = j->topo_repeticao - 1;
                if (--j->pilha_repeticao_restante[topo] > 0)
                    j->proximo_comando = j->pilha_repeticao_pc[topo];
                else
                    j->topo_repeticao--;
            }
            break;
        case COMANDO_REPETIR:
            if (j->topo_repeticao < 8) {
                int topo = j->topo_repeticao++;
                j->pilha_repeticao_pc[topo] = j->proximo_comando;
                j->pilha_repeticao_restante[topo] = limitar_int(c->a, 1, 100);
                j->pilha_repeticao_nivel[topo] = c->profundidade;
            }
            break;
        case COMANDO_CHAMAR_EVENTO:
            if (c->a >= 0 && c->a < a->projeto.quantidade_eventos && j->topo_pilha < 8) {
                j->pilha_eventos[j->topo_pilha] = j->evento_ativo;
                j->pilha_comandos[j->topo_pilha] = j->proximo_comando;
                j->topo_pilha++;
                j->evento_ativo = c->a;
                j->proximo_comando = 0;
            }
            break;
        case COMANDO_LOJA:
            if (c->a >= 0 && c->a < a->projeto.quantidade_lojas) {
                j->loja_ativa = c->a;
                j->tela_sobreposta = 3;
                return;
            }
            break;
        case COMANDO_MISSAO:
            if (c->a >= 0 && c->a < a->projeto.quantidade_missoes) {
                j->estado.estado_missoes[c->a] = limitar_int(c->b, 0, 2);
                j->estado.progresso_missoes[c->a] = c->c;
            }
            break;
        default:
            break;
        }
    }
}

static int posicao_bloqueada(Aplicativo *a, int x, int y) {
    Mapa *m = &a->projeto.mapas[a->jogo.estado.mapa];
    if (x < 0 || y < 0 || x >= m->largura || y >= m->altura)
        return 1;
    if (m->colisoes[y * m->largura + x])
        return 1;
    for (int i = 0; i < m->quantidade_entidades; ++i)
        if (m->entidades[i].ativo && m->entidades[i].x == x && m->entidades[i].y == y)
            return 1;
    return 0;
}

static void atualizar_jogo(Aplicativo *a, Entrada *in) {
    Jogo *j = &a->jogo;
    char save[32];
    if (in->teclas_pressionadas[27]) {
        if (j->tela_sobreposta) {
            j->tela_sobreposta = 0;
            executar_evento(a);
            return;
        }
        a->modo = MODO_EDITOR;
        mensagem_aplicativo(a, "Teste encerrado.");
        return;
    }
    if (in->teclas_pressionadas['M'])
        j->tela_sobreposta = j->tela_sobreposta == 1 ? 0 : 1;
    if (in->teclas_pressionadas['J'])
        j->tela_sobreposta = j->tela_sobreposta == 2 ? 0 : 2;
    if (j->tela_sobreposta)
        return;
    if (j->slot < 1 || j->slot > 3)
        j->slot = 1;
    if (!j->aguardando_escolha) {
        if (in->teclas_pressionadas['1'])
            j->slot = 1;
        if (in->teclas_pressionadas['2'])
            j->slot = 2;
        if (in->teclas_pressionadas['3'])
            j->slot = 3;
    }
    snprintf(save, sizeof(save), "save%d.mys", j->slot);
    if (in->teclas_pressionadas['K'] || in->teclas_pressionadas[133]) {
        if (salvar_estado(&a->projeto, &j->estado, save))
            snprintf(j->aviso, sizeof(j->aviso),
                     idioma_obter() == IDIOMA_INGLES ? "Game saved in slot %d."
                                                     : "Jogo salvo no slot %d.",
                     j->slot);
    }
    if (in->teclas_pressionadas['L'] || in->teclas_pressionadas[134]) {
        if (carregar_estado(&a->projeto, &j->estado, save))
            snprintf(j->aviso, sizeof(j->aviso),
                     idioma_obter() == IDIOMA_INGLES ? "Slot %d loaded." : "Slot %d carregado.",
                     j->slot);
        else
            copiar_texto(j->aviso, sizeof(j->aviso), "Nenhum save valido.");
    }
    if (j->aguardando_texto) {
        if (in->teclas_pressionadas[32] || in->teclas_pressionadas[13] ||
            in->teclas_pressionadas['E']) {
            j->aguardando_texto = 0;
            executar_evento(a);
        }
        return;
    }
    if (j->aguardando_escolha) {
        if (in->teclas_pressionadas['1'] || in->teclas_pressionadas[128]) {
            Evento *e = &a->projeto.eventos[j->evento_ativo];
            ComandoEvento *c = &e->comandos[j->proximo_comando - 1];
            if (c->a >= 0 && c->a < MYTHARA_MAX_VARIAVEIS)
                j->estado.variaveis[c->a] = 0;
            j->aguardando_escolha = 0;
            executar_evento(a);
        }
        if (in->teclas_pressionadas['2'] || in->teclas_pressionadas[129]) {
            Evento *e = &a->projeto.eventos[j->evento_ativo];
            ComandoEvento *c = &e->comandos[j->proximo_comando - 1];
            if (c->a >= 0 && c->a < MYTHARA_MAX_VARIAVEIS)
                j->estado.variaveis[c->a] = 1;
            j->aguardando_escolha = 0;
            executar_evento(a);
        }
        return;
    }
    if (j->evento_ativo >= 0) {
        executar_evento(a);
        return;
    }
    double agora = agora_segundos();
    if (agora < j->proximo_movimento)
        return;
    int dx = 0, dy = 0;
    if (in->teclas[128] || in->teclas['A'] || in->teclas_pressionadas[128] ||
        in->teclas_pressionadas['A'])
        dx = -1;
    else if (in->teclas[129] || in->teclas['D'] || in->teclas_pressionadas[129] ||
             in->teclas_pressionadas['D'])
        dx = 1;
    else if (in->teclas[130] || in->teclas['W'] || in->teclas_pressionadas[130] ||
             in->teclas_pressionadas['W'])
        dy = -1;
    else if (in->teclas[131] || in->teclas['S'] || in->teclas_pressionadas[131] ||
             in->teclas_pressionadas['S'])
        dy = 1;
    if (dx || dy) {
        int nx = j->estado.x + dx, ny = j->estado.y + dy;
        if (!posicao_bloqueada(a, nx, ny)) {
            j->estado.x = nx;
            j->estado.y = ny;
            j->passos_encontro++;
            Mapa *m = &a->projeto.mapas[j->estado.mapa];
            if (m->regiao_encontro >= 0 && j->passos_encontro >= 12 && rand() % 10 == 0) {
                j->passos_encontro = 0;
                preparar_batalha(a, m->regiao_encontro, 1);
                return;
            }
        }
        j->proximo_movimento = agora + 0.12;
    }
    if (in->teclas_pressionadas[32] || in->teclas_pressionadas['E']) {
        Mapa *m = &a->projeto.mapas[j->estado.mapa];
        for (int i = 0; i < m->quantidade_entidades; ++i) {
            Entidade *n = &m->entidades[i];
            int distancia = abs(n->x - j->estado.x) + abs(n->y - j->estado.y);
            if (n->ativo && distancia <= 1) {
                iniciar_evento_jogo(a, n->evento);
                executar_evento(a);
                break;
            }
        }
    }
}

static void desenhar_mapa_jogo(Aplicativo *a, Tela *t, Retangulo area) {
    EstadoJogo *j = &a->jogo.estado;
    Mapa *m = &a->projeto.mapas[j->mapa];
    int tamanho = a->projeto.tamanho_tile ? a->projeto.tamanho_tile : MYTHARA_TAMANHO_TILE;
    int ox = area.x + area.largura / 2 - j->x * tamanho - tamanho / 2,
        oy = area.y + area.altura / 2 - j->y * tamanho - tamanho / 2;
    preencher_retangulo(t, area, cor_rgb(12, 16, 21));
    for (int y = 0; y < m->altura; ++y)
        for (int x = 0; x < m->largura; ++x) {
            int idx = y * m->largura + x;
            Retangulo q = {ox + x * tamanho, oy + y * tamanho, tamanho, tamanho};
            if (q.x + tamanho < area.x || q.y + tamanho < area.y || q.x >= area.x + area.largura ||
                q.y >= area.y + area.altura)
                continue;
            desenhar_tile_recurso(a, t, q, m->camadas[0].tiles[idx], 0);
            for (int c = 1; c < m->quantidade_camadas; ++c)
                if (m->camadas[c].visivel && m->camadas[c].tiles[idx])
                    contornar_retangulo(t,
                                        (Retangulo){q.x + 3 + c, q.y + 3 + c, tamanho - 6 - c * 2,
                                                    tamanho - 6 - c * 2},
                                        paleta_tiles[m->camadas[c].tiles[idx] & 15]);
        }
    for (int i = 0; i < m->quantidade_entidades; ++i)
        if (m->entidades[i].ativo) {
            Entidade *n = &m->entidades[i];
            Retangulo q = {ox + n->x * tamanho + 5, oy + n->y * tamanho + 3, 22, 27};
            preencher_retangulo(t, q, paleta_tiles[n->cor & 15]);
            contornar_retangulo(t, q, cor_rgb(237, 240, 245));
        }
    Retangulo heroi = {ox + j->x * tamanho + 5, oy + j->y * tamanho + 3, 22, 27};
    preencher_retangulo(t, heroi, cor_rgb(245, 204, 78));
    contornar_retangulo(t, heroi, cor_rgb(255, 244, 190));
    desenhar_texto(t, heroi.x + 8, heroi.y + 9, "H", cor_rgb(45, 39, 28), 1);
}

static void desenhar_menu_grupo(Aplicativo *a, Interface *ui) {
    Jogo *j = &a->jogo;
    Tela *t = ui->tela;
    Retangulo r = {70, 55, t->largura - 370, t->altura - 110};
    ui_painel(t, r, "GRUPO, ATRIBUTOS E EQUIPAMENTOS  [M/ESC FECHA]");
    int y = r.y + 42;
    for (int i = 0; i < j->estado.quantidade_grupo; ++i) {
        MembroGrupo *g = &j->estado.grupo[i];
        Heroi *h = &a->projeto.herois[g->heroi];
        char linha[160];
        snprintf(linha, sizeof(linha), "%s  NIVEL %d  ATQ %d  DEF %d  VEL %d", h->nome, g->nivel,
                 g->ataque, g->defesa, g->velocidade);
        desenhar_texto(t, r.x + 22, y, linha, tema_ativo.texto, 1);
        y += 17;
        ui_barra_progresso(t, (Retangulo){r.x + 22, y, 250, 12}, g->vida, g->vida_maxima,
                           tema_ativo.perigo);
        snprintf(linha, sizeof(linha), "PV %d/%d", g->vida, g->vida_maxima);
        desenhar_texto(t, r.x + 282, y + 2, linha, tema_ativo.texto_suave, 1);
        y += 17;
        ui_barra_progresso(t, (Retangulo){r.x + 22, y, 250, 10}, g->magia, g->magia_maxima,
                           tema_ativo.destaque);
        y += 26;
    }
    y = r.y + r.altura - 120;
    desenhar_texto(t, r.x + 22, y, "EQUIPAR PRIMEIRO ITEM DISPONIVEL", tema_ativo.texto_suave, 1);
    y += 18;
    if (ui_botao(ui, (Retangulo){r.x + 22, y, 220, 28}, "EQUIPAR NO HEROI 1", 0)) {
        for (int it = 0; it < a->projeto.quantidade_itens; ++it)
            if (j->estado.inventario[it] > 0 && a->projeto.itens[it].tipo_equipamento > 0) {
                int slot = limitar_int(a->projeto.itens[it].tipo_equipamento - 1, 0, 3);
                MembroGrupo *g = &j->estado.grupo[0];
                if (g->equipamentos[slot] >= 0)
                    j->estado.inventario[g->equipamentos[slot]]++;
                g->equipamentos[slot] = it;
                j->estado.inventario[it]--;
                g->ataque = a->projeto.herois[g->heroi].ataque + a->projeto.itens[it].ataque;
                g->defesa = a->projeto.herois[g->heroi].defesa + a->projeto.itens[it].defesa;
                copiar_texto(j->aviso, sizeof(j->aviso), "Equipamento atualizado.");
                break;
            }
    }
}

static void desenhar_diario_missoes(Aplicativo *a, Interface *ui) {
    Jogo *j = &a->jogo;
    Tela *t = ui->tela;
    Retangulo r = {100, 70, t->largura - 430, t->altura - 140};
    ui_painel(t, r, "DIARIO DE MISSOES  [J/ESC FECHA]");
    int y = r.y + 45;
    for (int i = 0; i < a->projeto.quantidade_missoes; ++i) {
        Missao *m = &a->projeto.missoes[i];
        const char *estado = j->estado.estado_missoes[i] == 2
                                 ? "CONCLUIDA"
                                 : (j->estado.estado_missoes[i] == 1 ? "ATIVA" : "DISPONIVEL");
        char linha[180];
        snprintf(linha, sizeof(linha), "%s  [%s]", m->nome, estado);
        desenhar_texto(t, r.x + 24, y, linha,
                       j->estado.estado_missoes[i] == 2 ? tema_ativo.sucesso : tema_ativo.destaque,
                       1);
        y += 18;
        desenhar_texto(t, r.x + 36, y, m->descricao, tema_ativo.texto_suave, 1);
        y += 30;
        if (m->quantidade_etapas) {
            snprintf(linha, sizeof(linha), "- %s  %d/%d", m->etapas[0].descricao,
                     j->estado.progresso_missoes[i], m->etapas[0].quantidade);
            desenhar_texto(t, r.x + 36, y, linha, tema_ativo.texto, 1);
            y += 28;
        }
    }
}

static void desenhar_loja_jogo(Aplicativo *a, Interface *ui) {
    Jogo *j = &a->jogo;
    Tela *t = ui->tela;
    Loja *loja = &a->projeto.lojas[limitar_int(j->loja_ativa, 0, a->projeto.quantidade_lojas - 1)];
    Retangulo r = {120, 70, t->largura - 470, t->altura - 140};
    ui_painel(t, r, loja->nome);
    char linha[128];
    snprintf(linha, sizeof(linha), "OURO: %d", j->estado.ouro);
    desenhar_texto(t, r.x + 25, r.y + 43, linha, tema_ativo.aviso, 1);
    int y = r.y + 75;
    for (int i = 0; i < loja->quantidade_itens; ++i) {
        int id = loja->itens[i];
        if (id < 0 || id >= a->projeto.quantidade_itens)
            continue;
        int preco = loja->precos[i] ? loja->precos[i] : a->projeto.itens[id].valor;
        snprintf(linha, sizeof(linha), "%s  -  %d OURO", a->projeto.itens[id].nome, preco);
        if (ui_botao(ui, (Retangulo){r.x + 24, y, 420, 30}, linha, 0)) {
            if (j->estado.ouro >= preco) {
                j->estado.ouro -= preco;
                j->estado.inventario[id] = limitar_int(j->estado.inventario[id] + 1, 0, 99);
                copiar_texto(j->aviso, sizeof(j->aviso), "Compra realizada.");
            } else
                copiar_texto(j->aviso, sizeof(j->aviso), "Ouro insuficiente.");
        }
        if (j->estado.inventario[id] > 0 &&
            ui_botao(ui, (Retangulo){r.x + 455, y, 150, 30}, "VENDER METADE", 0)) {
            j->estado.inventario[id]--;
            j->estado.ouro += preco / 2;
        }
        y += 38;
    }
    if (ui_botao(ui, (Retangulo){r.x + r.largura - 130, r.y + r.altura - 48, 105, 28},
                 "SAIR DA LOJA", 0)) {
        j->tela_sobreposta = 0;
        executar_evento(a);
    }
}

static void desenhar_jogo(Aplicativo *a, Interface *ui) {
    Jogo *j = &a->jogo;
    Tela *t = ui->tela;
    Retangulo mundo = {0, 0, t->largura - 230, t->altura};
    desenhar_mapa_jogo(a, t, mundo);
    ui_painel(t, (Retangulo){t->largura - 230, 0, 230, t->altura}, "MYTHARA - JOGO");
    char linha[100];
    int y = 42;
    snprintf(linha, sizeof(linha), "GRUPO: %s", a->projeto.herois[0].nome);
    desenhar_texto(t, t->largura - 216, y, linha, tema_ativo.texto, 1);
    y += 24;
    snprintf(linha, sizeof(linha), "VIDA: %d / %d", j->estado.vida, j->estado.vida_maxima);
    desenhar_texto(t, t->largura - 216, y, linha, cor_rgb(231, 117, 117), 1);
    y += 22;
    snprintf(linha, sizeof(linha), "NIVEL %d   XP %d", j->estado.nivel, j->estado.experiencia);
    desenhar_texto(t, t->largura - 216, y, linha, cor_rgb(181, 196, 216), 1);
    y += 22;
    snprintf(linha, sizeof(linha), "OURO: %d", j->estado.ouro);
    desenhar_texto(t, t->largura - 216, y, linha, cor_rgb(235, 205, 99), 1);
    y += 32;
    desenhar_texto(t, t->largura - 216, y, "INVENTARIO", cor_rgb(132, 194, 231), 1);
    y += 20;
    for (int i = 0; i < a->projeto.quantidade_itens; ++i)
        if (j->estado.inventario[i]) {
            snprintf(linha, sizeof(linha), "%s x%d", a->projeto.itens[i].nome,
                     j->estado.inventario[i]);
            desenhar_texto(t, t->largura - 216, y, linha, cor_rgb(205, 214, 224), 1);
            y += 18;
        }
    y = t->altura - 196;
    desenhar_texto(t, t->largura - 216, y, "SLOT DE SAVE", cor_rgb(132, 194, 231), 1);
    y += 18;
    for (int s = 1; s <= 3; ++s) {
        char slot[24];
        snprintf(slot, sizeof(slot), "SLOT %d", s);
        if (ui_botao(ui, (Retangulo){t->largura - 218 + (s - 1) * 69, y, 64, 25}, slot,
                     j->slot == s))
            j->slot = s;
    }
    y += 34;
    desenhar_texto(t, t->largura - 216, y, "WASD/SETAS: MOVER", cor_rgb(157, 169, 185), 1);
    desenhar_texto(t, t->largura - 216, y + 17, "E/ESPACO: INTERAGIR", cor_rgb(157, 169, 185), 1);
    desenhar_texto(t, t->largura - 216, y + 34, "F1/K SALVA  F2/L CARREGA", cor_rgb(157, 169, 185),
                   1);
    desenhar_texto(t, t->largura - 216, y + 51, "ESC: VOLTAR AO EDITOR", cor_rgb(157, 169, 185), 1);
    desenhar_texto(t, t->largura - 216, y + 78, j->aviso, cor_rgb(235, 191, 93), 1);
    if (j->aguardando_texto) {
        Retangulo caixa = {32, t->altura - 164, t->largura - 294, 132};
        preencher_retangulo(t, caixa, cor_rgb(24, 29, 39));
        contornar_retangulo(t, caixa, cor_rgb(187, 204, 225));
        desenhar_texto(t, caixa.x + 18, caixa.y + 18, j->texto, cor_rgb(235, 239, 244), 1);
        desenhar_texto(t, caixa.x + 18, caixa.y + 100, "ESPACO PARA CONTINUAR",
                       cor_rgb(137, 190, 225), 1);
    }
    if (j->aguardando_escolha) {
        Retangulo caixa = {t->largura / 2 - 220, t->altura / 2 - 85, 440, 170};
        preencher_retangulo(t, caixa, cor_rgb(24, 29, 39));
        contornar_retangulo(t, caixa, cor_rgb(187, 204, 225));
        desenhar_texto(t, caixa.x + 20, caixa.y + 20, "ESCOLHA UMA OPCAO", cor_rgb(225, 231, 239),
                       1);
        if (ui_botao(ui, (Retangulo){caixa.x + 20, caixa.y + 58, 400, 32}, j->escolha_a, 0)) {
            Evento *e = &a->projeto.eventos[j->evento_ativo];
            ComandoEvento *c = &e->comandos[j->proximo_comando - 1];
            if (c->a >= 0 && c->a < MYTHARA_MAX_VARIAVEIS)
                j->estado.variaveis[c->a] = 0;
            j->aguardando_escolha = 0;
            executar_evento(a);
        }
        if (ui_botao(ui, (Retangulo){caixa.x + 20, caixa.y + 102, 400, 32}, j->escolha_b, 0)) {
            Evento *e = &a->projeto.eventos[j->evento_ativo];
            ComandoEvento *c = &e->comandos[j->proximo_comando - 1];
            if (c->a >= 0 && c->a < MYTHARA_MAX_VARIAVEIS)
                j->estado.variaveis[c->a] = 1;
            j->aguardando_escolha = 0;
            executar_evento(a);
        }
    }
    if (j->tela_sobreposta == 1)
        desenhar_menu_grupo(a, ui);
    else if (j->tela_sobreposta == 2)
        desenhar_diario_missoes(a, ui);
    else if (j->tela_sobreposta == 3)
        desenhar_loja_jogo(a, ui);
}

static int primeiro_inimigo_vivo(const Jogo *j) {
    for (int i = 0; i < j->quantidade_inimigos_batalha; ++i)
        if (j->vidas_inimigos[i] > 0)
            return i;
    return -1;
}
static int primeiro_heroi_vivo(const EstadoJogo *e) {
    for (int i = 0; i < e->quantidade_grupo; ++i)
        if (e->grupo[i].vida > 0)
            return i;
    return -1;
}

static void perder_batalha(Aplicativo *a) {
    Jogo *j = &a->jogo;
    for (int i = 0; i < j->estado.quantidade_grupo; ++i)
        j->estado.grupo[i].vida = j->estado.grupo[i].vida_maxima;
    j->estado.vida = j->estado.vida_maxima;
    j->estado.mapa = a->projeto.mapa_inicial;
    j->estado.x = a->projeto.inicio_x;
    j->estado.y = a->projeto.inicio_y;
    j->evento_ativo = -1;
    a->modo = MODO_JOGO;
    copiar_texto(j->aviso, sizeof(j->aviso), "Derrota! O grupo retornou ao inicio.");
}

static void vencer_batalha(Aplicativo *a) {
    Jogo *j = &a->jogo;
    int xp = 0, ouro = 0;
    for (int i = 0; i < j->quantidade_inimigos_batalha; ++i) {
        Inimigo *n = &a->projeto.inimigos[j->inimigos_batalha[i]];
        xp += n->experiencia;
        ouro += n->ouro;
        for (int q = 0; q < a->projeto.quantidade_missoes; ++q)
            if (j->estado.estado_missoes[q] == 1 && a->projeto.missoes[q].quantidade_etapas &&
                a->projeto.missoes[q].etapas[0].tipo == 1 &&
                a->projeto.missoes[q].etapas[0].alvo == j->inimigos_batalha[i]) {
                j->estado.progresso_missoes[q]++;
                if (j->estado.progresso_missoes[q] >= a->projeto.missoes[q].etapas[0].quantidade) {
                    j->estado.estado_missoes[q] = 2;
                    ouro += a->projeto.missoes[q].recompensa_ouro;
                    xp += a->projeto.missoes[q].recompensa_experiencia;
                }
            }
    }
    j->estado.experiencia += xp;
    j->estado.ouro += ouro;
    if (j->estado.experiencia >= j->estado.nivel * 20) {
        j->estado.nivel++;
        for (int i = 0; i < j->estado.quantidade_grupo; ++i) {
            j->estado.grupo[i].nivel++;
            j->estado.grupo[i].vida_maxima += 5;
            j->estado.grupo[i].vida = j->estado.grupo[i].vida_maxima;
            j->estado.grupo[i].ataque += 2;
        }
    }
    j->estado.vida = j->estado.grupo[0].vida;
    j->estado.vida_maxima = j->estado.grupo[0].vida_maxima;
    snprintf(j->aviso, sizeof(j->aviso),
             idioma_obter() == IDIOMA_INGLES ? "Victory! +%d XP, +%d gold."
                                             : "Vitoria! +%d XP, +%d ouro.",
             xp, ouro);
    a->modo = MODO_JOGO;
    executar_evento(a);
}

typedef struct {
    int lado, indice, velocidade;
} ParticipanteTurno;
static void resolver_rodada(Aplicativo *a) {
    Jogo *j = &a->jogo;
    ParticipanteTurno ordem[12];
    int total = 0;
    memset(j->defesas_grupo, 0, sizeof(j->defesas_grupo));
    for (int i = 0; i < j->estado.quantidade_grupo; ++i)
        if (j->estado.grupo[i].vida > 0)
            ordem[total++] = (ParticipanteTurno){0, i, j->estado.grupo[i].velocidade};
    for (int i = 0; i < j->quantidade_inimigos_batalha; ++i)
        if (j->vidas_inimigos[i] > 0)
            ordem[total++] =
                (ParticipanteTurno){1, i, a->projeto.inimigos[j->inimigos_batalha[i]].velocidade};
    for (int i = 0; i < total; ++i)
        for (int k = i + 1; k < total; ++k)
            if (ordem[k].velocidade > ordem[i].velocidade) {
                ParticipanteTurno t = ordem[i];
                ordem[i] = ordem[k];
                ordem[k] = t;
            }
    for (int o = 0; o < total; ++o) {
        if (primeiro_inimigo_vivo(j) < 0 || primeiro_heroi_vivo(&j->estado) < 0)
            break;
        if (!ordem[o].lado) {
            MembroGrupo *g = &j->estado.grupo[ordem[o].indice];
            if (g->vida <= 0)
                continue;
            int alvo = primeiro_inimigo_vivo(j);
            Inimigo *n = &a->projeto.inimigos[j->inimigos_batalha[alvo]];
            int acao = j->acoes_grupo[ordem[o].indice];
            if (acao == 3) {
                j->defesas_grupo[ordem[o].indice] = 1;
                continue;
            }
            if (acao == 2) {
                for (int it = 0; it < a->projeto.quantidade_itens; ++it)
                    if (j->estado.inventario[it] > 0 && a->projeto.itens[it].cura > 0) {
                        j->estado.inventario[it]--;
                        g->vida =
                            limitar_int(g->vida + a->projeto.itens[it].cura, 0, g->vida_maxima);
                        break;
                    }
                continue;
            }
            int dano;
            if (acao == 1 && a->projeto.quantidade_habilidades &&
                g->magia >= a->projeto.habilidades[0].custo_mp) {
                Habilidade *h = &a->projeto.habilidades[0];
                g->magia -= h->custo_mp;
                dano = limitar_int(g->poder_magico + h->poder - n->resistencia / 2, 1, 99999);
            } else
                dano = limitar_int(g->ataque - n->defesa / 2, 1, 99999);
            j->vidas_inimigos[alvo] -= dano;
        } else {
            int id = ordem[o].indice;
            if (j->vidas_inimigos[id] <= 0)
                continue;
            int alvo = primeiro_heroi_vivo(&j->estado);
            MembroGrupo *g = &j->estado.grupo[alvo];
            Inimigo *n = &a->projeto.inimigos[j->inimigos_batalha[id]];
            int dano = limitar_int(n->ataque - g->defesa / 2, 1, 99999);
            if (j->defesas_grupo[alvo])
                dano = (dano + 1) / 2;
            g->vida -= dano;
        }
    }
    if (primeiro_inimigo_vivo(j) < 0) {
        vencer_batalha(a);
        return;
    }
    if (primeiro_heroi_vivo(&j->estado) < 0) {
        perder_batalha(a);
        return;
    }
    j->membro_escolhendo = 0;
    while (j->membro_escolhendo < j->estado.quantidade_grupo &&
           j->estado.grupo[j->membro_escolhendo].vida <= 0)
        j->membro_escolhendo++;
    j->estado.vida = j->estado.grupo[0].vida;
    j->estado.magia = j->estado.grupo[0].magia;
    copiar_texto(j->aviso, sizeof(j->aviso), "Nova rodada: escolha as acoes por velocidade.");
}

static void escolher_acao_batalha(Aplicativo *a, int acao) {
    Jogo *j = &a->jogo;
    if (j->membro_escolhendo >= j->estado.quantidade_grupo)
        return;
    j->acoes_grupo[j->membro_escolhendo] = acao;
    do {
        j->membro_escolhendo++;
    } while (j->membro_escolhendo < j->estado.quantidade_grupo &&
             j->estado.grupo[j->membro_escolhendo].vida <= 0);
    if (j->membro_escolhendo >= j->estado.quantidade_grupo)
        resolver_rodada(a);
}

static void desenhar_batalha(Aplicativo *a, Interface *ui) {
    Tela *t = ui->tela;
    Jogo *j = &a->jogo;
    limpar_tela(t, cor_rgb(22, 20, 32));
    desenhar_texto(t, 35, 28,
                   a->projeto.batalha_lateral ? "BATALHA - VISAO LATERAL"
                                              : "BATALHA - VISAO FRONTAL",
                   tema_ativo.aviso, 2);
    char linha[128];
    for (int i = 0; i < j->estado.quantidade_grupo; ++i) {
        MembroGrupo *g = &j->estado.grupo[i];
        int x = a->projeto.batalha_lateral ? 80 + i * 120 : 60,
            y = a->projeto.batalha_lateral ? 170 + i * 65 : t->altura - 260 + i * 42;
        Retangulo corpo = {x, y, 58, 58};
        preencher_retangulo(t, corpo, paleta_tiles[(10 + i) & 15]);
        contornar_retangulo(t, corpo,
                            i == j->membro_escolhendo ? tema_ativo.aviso : tema_ativo.borda);
        snprintf(linha, sizeof(linha), "%s %d/%d", a->projeto.herois[g->heroi].nome, g->vida,
                 g->vida_maxima);
        desenhar_texto(t, x, y + 66, linha, tema_ativo.texto, 1);
        ui_barra_progresso(t, (Retangulo){x, y + 80, 100, 9}, g->vida, g->vida_maxima,
                           tema_ativo.perigo);
    }
    for (int i = 0; i < j->quantidade_inimigos_batalha; ++i) {
        Inimigo *n = &a->projeto.inimigos[j->inimigos_batalha[i]];
        int x = a->projeto.batalha_lateral
                    ? t->largura - 180 - i * 120
                    : t->largura / 2 - ((j->quantidade_inimigos_batalha * 110) / 2) + i * 110,
            y = a->projeto.batalha_lateral ? 155 + i * 58 : 145;
        Retangulo corpo = {x, y, 72, 72};
        preencher_retangulo(t, corpo, paleta_tiles[(2 + i) & 15]);
        contornar_retangulo(t, corpo, tema_ativo.perigo);
        snprintf(linha, sizeof(linha), "%s %d", n->nome,
                 j->vidas_inimigos[i] > 0 ? j->vidas_inimigos[i] : 0);
        desenhar_texto(t, x, y + 80, linha, tema_ativo.texto, 1);
        ui_barra_progresso(t, (Retangulo){x, y + 94, 95, 9}, j->vidas_inimigos[i], n->vida,
                           tema_ativo.perigo);
    }
    if (j->membro_escolhendo < j->estado.quantidade_grupo) {
        MembroGrupo *g = &j->estado.grupo[j->membro_escolhendo];
        snprintf(linha, sizeof(linha), "ACAO DE %s", a->projeto.herois[g->heroi].nome);
        desenhar_texto(t, 45, t->altura - 145, linha, tema_ativo.destaque, 1);
    }
    int y = t->altura - 110, x = 40;
    if (ui_botao(ui, (Retangulo){x, y, 130, 38}, "ATACAR", 0))
        escolher_acao_batalha(a, 0);
    if (ui_botao(ui, (Retangulo){x + 140, y, 130, 38}, "HABILIDADE", 0))
        escolher_acao_batalha(a, 1);
    if (ui_botao(ui, (Retangulo){x + 280, y, 130, 38}, "ITEM", 0))
        escolher_acao_batalha(a, 2);
    if (ui_botao(ui, (Retangulo){x + 420, y, 130, 38}, "DEFENDER", 0))
        escolher_acao_batalha(a, 3);
    if (ui_botao(ui, (Retangulo){x + 560, y, 130, 38}, "FUGIR", 0)) {
        if (rand() % 2) {
            a->modo = MODO_JOGO;
            j->evento_ativo = -1;
            copiar_texto(j->aviso, sizeof(j->aviso), "O grupo fugiu.");
        } else
            resolver_rodada(a);
    }
    desenhar_texto(t, 45, t->altura - 45, j->aviso, tema_ativo.aviso, 1);
}
