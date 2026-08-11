/*
 * Modulo: plataforma
 * Responsabilidade: janela, entrada, framebuffer e clipboard nativos.
 * Implementacoes: X11 no Linux e Win32 no Windows.
 */

/* Backend X11 --------------------------------------------------------------- */

#ifndef _WIN32
static Display *exibidor_clipboard_x11;
typedef struct {
    Display *exibidor;
    Window janela;
    GC gc;
    Atom apagar_janela;
    XImage *imagem;
    Tela tela;
    Entrada entrada;
    int executando, fechamento_solicitado;
    int largura, altura;
} Plataforma;

static int plataforma_redimensionar(Plataforma *p, int largura, int altura) {
    if (largura < 640)
        largura = 640;
    if (altura < 480)
        altura = 480;
    if (p->imagem) {
        p->imagem->data = (char *)p->tela.pixels;
        XDestroyImage(p->imagem);
        p->imagem = NULL;
        p->tela.pixels = NULL;
    }
    p->tela.pixels = calloc((size_t)largura * (size_t)altura, sizeof(uint32_t));
    if (!p->tela.pixels)
        return 0;
    p->imagem = XCreateImage(p->exibidor, DefaultVisual(p->exibidor, DefaultScreen(p->exibidor)),
                             24, ZPixmap, 0, (char *)p->tela.pixels, (unsigned)largura,
                             (unsigned)altura, 32, 0);
    if (!p->imagem) {
        free(p->tela.pixels);
        p->tela.pixels = NULL;
        return 0;
    }
    p->largura = p->tela.largura = largura;
    p->altura = p->tela.altura = altura;
    return 1;
}

static int plataforma_iniciar(Plataforma *p) {
    memset(p, 0, sizeof(*p));
    p->exibidor = XOpenDisplay(NULL);
    if (!p->exibidor) {
        fprintf(stderr, "Mythara: nao foi possivel abrir o servidor X11.\n");
        return 0;
    }
    exibidor_clipboard_x11 = p->exibidor;
    int tela = DefaultScreen(p->exibidor);
    p->janela = XCreateSimpleWindow(p->exibidor, RootWindow(p->exibidor, tela), 0, 0,
                                    MYTHARA_LARGURA_INICIAL, MYTHARA_ALTURA_INICIAL, 0,
                                    BlackPixel(p->exibidor, tela), BlackPixel(p->exibidor, tela));
    XStoreName(p->exibidor, p->janela, "Mythara - Motor de JRPG");
    XSelectInput(p->exibidor, p->janela,
                 ExposureMask | KeyPressMask | KeyReleaseMask | ButtonPressMask |
                     ButtonReleaseMask | PointerMotionMask | StructureNotifyMask);
    p->apagar_janela = XInternAtom(p->exibidor, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(p->exibidor, p->janela, &p->apagar_janela, 1);
    p->gc = XCreateGC(p->exibidor, p->janela, 0, NULL);
    XMapWindow(p->exibidor, p->janela);
    if (!plataforma_redimensionar(p, MYTHARA_LARGURA_INICIAL, MYTHARA_ALTURA_INICIAL))
        return 0;
    p->executando = 1;
    return 1;
}

static int codigo_tecla(KeySym s) {
    if (s >= XK_a && s <= XK_z)
        return (int)(s - XK_a + 'A');
    if (s >= XK_A && s <= XK_Z)
        return (int)s;
    if (s >= XK_0 && s <= XK_9)
        return (int)s;
    if (s == XK_BackSpace)
        return 8;
    if (s == XK_Return)
        return 13;
    if (s == XK_Escape)
        return 27;
    if (s == XK_space)
        return 32;
    if (s == XK_Left)
        return 128;
    if (s == XK_Right)
        return 129;
    if (s == XK_Up)
        return 130;
    if (s == XK_Down)
        return 131;
    if (s == XK_F5)
        return 132;
    if (s == XK_F1)
        return 133;
    if (s == XK_F2)
        return 134;
    if (s == XK_Control_L || s == XK_Control_R)
        return 135;
    if (s == XK_Home)
        return 136;
    if (s == XK_End)
        return 137;
    if (s == XK_Delete)
        return 127;
    return 0;
}

static void plataforma_eventos(Plataforma *p) {
    Entrada *e = &p->entrada;
    memset(e->teclas_pressionadas, 0, sizeof(e->teclas_pressionadas));
    e->mouse_pressionado = e->mouse_solto = e->roda = e->tamanho_texto = 0;
    e->controle = e->teclas[135];
    e->shift = 0;
    while (XPending(p->exibidor)) {
        XEvent evento;
        XNextEvent(p->exibidor, &evento);
        if (evento.type == ClientMessage && (Atom)evento.xclient.data.l[0] == p->apagar_janela)
            p->fechamento_solicitado = 1;
        else if (evento.type == ConfigureNotify &&
                 (evento.xconfigure.width != p->largura || evento.xconfigure.height != p->altura))
            plataforma_redimensionar(p, evento.xconfigure.width, evento.xconfigure.height);
        else if (evento.type == MotionNotify) {
            e->mouse_x = evento.xmotion.x;
            e->mouse_y = evento.xmotion.y;
        } else if (evento.type == ButtonPress) {
            e->mouse_x = evento.xbutton.x;
            e->mouse_y = evento.xbutton.y;
            if (evento.xbutton.button == Button1)
                e->mouse_baixo = e->mouse_pressionado = 1;
            if (evento.xbutton.button == Button4)
                e->roda = 1;
            if (evento.xbutton.button == Button5)
                e->roda = -1;
        } else if (evento.type == ButtonRelease) {
            e->mouse_x = evento.xbutton.x;
            e->mouse_y = evento.xbutton.y;
            if (evento.xbutton.button == Button1) {
                e->mouse_baixo = 0;
                e->mouse_solto = 1;
            }
        } else if (evento.type == KeyPress || evento.type == KeyRelease) {
            KeySym simbolo = NoSymbol;
            char buffer[16];
            int n = XLookupString(&evento.xkey, buffer, sizeof(buffer), &simbolo, NULL);
            int c = codigo_tecla(simbolo);
            int baixo = evento.type == KeyPress;
            e->controle = (evento.xkey.state & ControlMask) != 0;
            e->shift = (evento.xkey.state & ShiftMask) != 0;
            if (c >= 0 && c < 256) {
                if (baixo && !e->teclas[c])
                    e->teclas_pressionadas[c] = 1;
                e->teclas[c] = baixo;
            }
            if (baixo && n > 0 && e->tamanho_texto + n < (int)sizeof(e->texto)) {
                memcpy(e->texto + e->tamanho_texto, buffer, (size_t)n);
                e->tamanho_texto += n;
            }
        }
    }
}

static void plataforma_apresentar(Plataforma *p) {
    XPutImage(p->exibidor, p->janela, p->gc, p->imagem, 0, 0, 0, 0, (unsigned)p->largura,
              (unsigned)p->altura);
    XFlush(p->exibidor);
}

static void plataforma_encerrar(Plataforma *p) {
    if (!p->exibidor)
        return;
    if (p->imagem) {
        p->imagem->data = (char *)p->tela.pixels;
        XDestroyImage(p->imagem);
    }
    if (p->gc)
        XFreeGC(p->exibidor, p->gc);
    if (p->janela)
        XDestroyWindow(p->exibidor, p->janela);
    XCloseDisplay(p->exibidor);
    exibidor_clipboard_x11 = NULL;
    memset(p, 0, sizeof(*p));
}
#else

/* Backend Win32 ------------------------------------------------------------- */
typedef struct {
    HWND janela;
    BITMAPINFO bitmap;
    Tela tela;
    Entrada entrada;
    int executando, fechamento_solicitado;
    int largura, altura;
} Plataforma;

static Plataforma *plataforma_win32_ativa;

static int plataforma_redimensionar(Plataforma *p, int largura, int altura) {
    if (largura < 640)
        largura = 640;
    if (altura < 480)
        altura = 480;
    uint32_t *pixels = calloc((size_t)largura * (size_t)altura, sizeof(uint32_t));
    if (!pixels)
        return 0;
    free(p->tela.pixels);
    p->tela.pixels = pixels;
    p->largura = p->tela.largura = largura;
    p->altura = p->tela.altura = altura;
    memset(&p->bitmap, 0, sizeof(p->bitmap));
    p->bitmap.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    p->bitmap.bmiHeader.biWidth = largura;
    p->bitmap.bmiHeader.biHeight = -altura;
    p->bitmap.bmiHeader.biPlanes = 1;
    p->bitmap.bmiHeader.biBitCount = 32;
    p->bitmap.bmiHeader.biCompression = BI_RGB;
    return 1;
}

static int codigo_tecla_win32(WPARAM tecla) {
    if (tecla >= 'A' && tecla <= 'Z')
        return (int)tecla;
    if (tecla >= '0' && tecla <= '9')
        return (int)tecla;
    if (tecla == VK_BACK)
        return 8;
    if (tecla == VK_RETURN)
        return 13;
    if (tecla == VK_ESCAPE)
        return 27;
    if (tecla == VK_SPACE)
        return 32;
    if (tecla == VK_DELETE)
        return 127;
    if (tecla == VK_LEFT)
        return 128;
    if (tecla == VK_RIGHT)
        return 129;
    if (tecla == VK_UP)
        return 130;
    if (tecla == VK_DOWN)
        return 131;
    if (tecla == VK_F5)
        return 132;
    if (tecla == VK_F1)
        return 133;
    if (tecla == VK_F2)
        return 134;
    if (tecla == VK_CONTROL)
        return 135;
    if (tecla == VK_HOME)
        return 136;
    if (tecla == VK_END)
        return 137;
    return 0;
}

static LRESULT CALLBACK janela_mythara_proc(HWND janela, UINT mensagem, WPARAM wparam,
                                            LPARAM lparam) {
    Plataforma *p = plataforma_win32_ativa;
    Entrada *e = p ? &p->entrada : NULL;
    switch (mensagem) {
    case WM_CLOSE:
        if (p)
            p->fechamento_solicitado = 1;
        return 0;
    case WM_DESTROY:
        if (p)
            p->executando = 0;
        PostQuitMessage(0);
        return 0;
    case WM_SIZE:
        if (p && wparam != SIZE_MINIMIZED) {
            int w = LOWORD(lparam), h = HIWORD(lparam);
            if (w != p->largura || h != p->altura)
                plataforma_redimensionar(p, w, h);
        }
        return 0;
    case WM_MOUSEMOVE:
        if (e) {
            e->mouse_x = (short)LOWORD(lparam);
            e->mouse_y = (short)HIWORD(lparam);
        }
        return 0;
    case WM_LBUTTONDOWN:
        if (e) {
            SetCapture(janela);
            e->mouse_x = (short)LOWORD(lparam);
            e->mouse_y = (short)HIWORD(lparam);
            e->mouse_baixo = e->mouse_pressionado = 1;
        }
        return 0;
    case WM_LBUTTONUP:
        if (e) {
            ReleaseCapture();
            e->mouse_x = (short)LOWORD(lparam);
            e->mouse_y = (short)HIWORD(lparam);
            e->mouse_baixo = 0;
            e->mouse_solto = 1;
        }
        return 0;
    case WM_MOUSEWHEEL:
        if (e)
            e->roda = GET_WHEEL_DELTA_WPARAM(wparam) > 0 ? 1 : -1;
        return 0;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (e) {
            int c = codigo_tecla_win32(wparam);
            if (c && c < 256) {
                if (!e->teclas[c])
                    e->teclas_pressionadas[c] = 1;
                e->teclas[c] = 1;
            }
        }
        return 0;
    case WM_KEYUP:
    case WM_SYSKEYUP:
        if (e) {
            int c = codigo_tecla_win32(wparam);
            if (c && c < 256)
                e->teclas[c] = 0;
        }
        return 0;
    case WM_CHAR:
        if (e && wparam >= 32 && wparam != 127) {
            wchar_t wc = (wchar_t)wparam;
            char utf8[8];
            int n = WideCharToMultiByte(CP_UTF8, 0, &wc, 1, utf8, sizeof(utf8), NULL, NULL);
            if (n > 0 && e->tamanho_texto + n < (int)sizeof(e->texto)) {
                memcpy(e->texto + e->tamanho_texto, utf8, (size_t)n);
                e->tamanho_texto += n;
            }
        }
        return 0;
    default:
        return DefWindowProcW(janela, mensagem, wparam, lparam);
    }
}

static int plataforma_iniciar(Plataforma *p) {
    memset(p, 0, sizeof(*p));
    plataforma_win32_ativa = p;
    HINSTANCE instancia = GetModuleHandleW(NULL);
    WNDCLASSW classe = {0};
    classe.lpfnWndProc = janela_mythara_proc;
    classe.hInstance = instancia;
    classe.hCursor = LoadCursor(NULL, IDC_ARROW);
    classe.lpszClassName = L"MytharaJanelaV4";
    RegisterClassW(&classe);
    RECT r = {0, 0, MYTHARA_LARGURA_INICIAL, MYTHARA_ALTURA_INICIAL};
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    p->janela = CreateWindowW(classe.lpszClassName, L"Mythara 4 - Motor de JRPG",
                              WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
                              r.right - r.left, r.bottom - r.top, NULL, NULL, instancia, NULL);
    if (!p->janela)
        return 0;
    if (!plataforma_redimensionar(p, MYTHARA_LARGURA_INICIAL, MYTHARA_ALTURA_INICIAL))
        return 0;
    p->executando = 1;
    return 1;
}

static void plataforma_eventos(Plataforma *p) {
    Entrada *e = &p->entrada;
    memset(e->teclas_pressionadas, 0, sizeof(e->teclas_pressionadas));
    e->mouse_pressionado = e->mouse_solto = e->roda = e->tamanho_texto = 0;
    e->controle = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    e->shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    MSG m;
    while (PeekMessageW(&m, NULL, 0, 0, PM_REMOVE)) {
        if (m.message == WM_QUIT)
            p->executando = 0;
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
}

static void plataforma_apresentar(Plataforma *p) {
    HDC dc = GetDC(p->janela);
    StretchDIBits(dc, 0, 0, p->largura, p->altura, 0, 0, p->largura, p->altura, p->tela.pixels,
                  &p->bitmap, DIB_RGB_COLORS, SRCCOPY);
    ReleaseDC(p->janela, dc);
}
static void plataforma_encerrar(Plataforma *p) {
    if (p->janela && IsWindow(p->janela))
        DestroyWindow(p->janela);
    free(p->tela.pixels);
    memset(p, 0, sizeof(*p));
    plataforma_win32_ativa = NULL;
}
#endif

/* Clipboard UTF-8 ----------------------------------------------------------- */

static int plataforma_clipboard_copiar(const char *texto) {
#ifdef _WIN32
    int n = MultiByteToWideChar(CP_UTF8, 0, texto, -1, NULL, 0);
    if (n <= 0 || !OpenClipboard(NULL))
        return 0;
    EmptyClipboard();
    HGLOBAL bloco = GlobalAlloc(GMEM_MOVEABLE, (size_t)n * sizeof(wchar_t));
    if (!bloco) {
        CloseClipboard();
        return 0;
    }
    wchar_t *p = GlobalLock(bloco);
    MultiByteToWideChar(CP_UTF8, 0, texto, -1, p, n);
    GlobalUnlock(bloco);
    if (!SetClipboardData(CF_UNICODETEXT, bloco)) {
        GlobalFree(bloco);
        CloseClipboard();
        return 0;
    }
    CloseClipboard();
    return 1;
#else
    if (!exibidor_clipboard_x11)
        return 0;
    XStoreBuffer(exibidor_clipboard_x11, texto, (int)strlen(texto), 0);
    XFlush(exibidor_clipboard_x11);
    return 1;
#endif
}

static int plataforma_clipboard_colar(char *destino, size_t capacidade) {
#ifdef _WIN32
    if (!OpenClipboard(NULL))
        return 0;
    HANDLE bloco = GetClipboardData(CF_UNICODETEXT);
    if (!bloco) {
        CloseClipboard();
        return 0;
    }
    const wchar_t *p = GlobalLock(bloco);
    int n = p ? WideCharToMultiByte(CP_UTF8, 0, p, -1, destino, (int)capacidade, NULL, NULL) : 0;
    if (p)
        GlobalUnlock(bloco);
    CloseClipboard();
    if (n <= 0) {
        if (capacidade)
            destino[0] = 0;
        return 0;
    }
    destino[capacidade - 1] = 0;
    return 1;
#else
    if (!exibidor_clipboard_x11)
        return 0;
    int n = 0;
    char *texto = XFetchBuffer(exibidor_clipboard_x11, &n, 0);
    if (!texto || n <= 0) {
        if (texto)
            XFree(texto);
        return 0;
    }
    size_t tam = (size_t)n;
    if (tam >= capacidade)
        tam = capacidade - 1;
    memcpy(destino, texto, tam);
    destino[tam] = 0;
    XFree(texto);
    return 1;
#endif
}
