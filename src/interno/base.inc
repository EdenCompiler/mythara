/*
 * Modulo: base
 * Responsabilidade: tipos geometricos, tempo, caminhos e sistema de arquivos.
 * Depende de: preludio.
 * Consumido por: todos os demais modulos internos.
 */

/* Tipos e operacoes primitivas --------------------------------------------- */

#define MYTHARA_LARGURA_INICIAL 1280
#define MYTHARA_ALTURA_INICIAL 720
#define MYTHARA_TAMANHO_TILE 32
#define MYTHARA_MAX_EDICOES 10000
#define MYTHARA_MAX_HISTORICO_BYTES (256u * 1024u * 1024u)

typedef uint32_t Cor;
typedef int32_t Inteiro;

typedef struct {
    int x, y, largura, altura;
} Retangulo;
typedef struct {
    uint32_t *pixels;
    int largura, altura;
} Tela;

static Cor cor_rgb(unsigned r, unsigned g, unsigned b) {
    return 0xff000000u | ((r & 255u) << 16) | ((g & 255u) << 8) | (b & 255u);
}

static int limitar_int(int valor, int minimo, int maximo) {
    return valor < minimo ? minimo : (valor > maximo ? maximo : valor);
}

static int ponto_em_retangulo(int x, int y, Retangulo r) {
    return x >= r.x && y >= r.y && x < r.x + r.largura && y < r.y + r.altura;
}

static void copiar_texto(char *destino, size_t capacidade, const char *origem) {
    if (!capacidade)
        return;
    if (!origem)
        origem = "";
    size_t n = strlen(origem);
    if (n >= capacidade)
        n = capacidade - 1;
    memmove(destino, origem, n);
    destino[n] = 0;
}

/* Tempo e executavel -------------------------------------------------------- */

static const char *nome_base(const char *caminho) {
    const char *barra = strrchr(caminho, '/');
#ifdef _WIN32
    const char *barra_windows = strrchr(caminho, '\\');
    if (!barra || barra_windows > barra)
        barra = barra_windows;
#endif
    return barra ? barra + 1 : caminho;
}

static double agora_segundos(void) {
#ifdef _WIN32
    LARGE_INTEGER frequencia, contador;
    QueryPerformanceFrequency(&frequencia);
    QueryPerformanceCounter(&contador);
    return (double)contador.QuadPart / (double)frequencia.QuadPart;
#else
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec / 1000000000.0;
#endif
}

static void pausar_milissegundos(int ms) {
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    struct timespec pausa = {ms / 1000, (long)(ms % 1000) * 1000000L};
    nanosleep(&pausa, NULL);
#endif
}

static int caminho_do_executavel(char *destino, size_t capacidade) {
#ifdef _WIN32
    DWORD n = GetModuleFileNameA(NULL, destino, (DWORD)capacidade);
    return n > 0 && n < capacidade;
#else
    ssize_t n = readlink("/proc/self/exe", destino, capacidade - 1);
    if (n <= 0)
        return 0;
    destino[n] = 0;
    return 1;
#endif
}

#ifdef _WIN32
static FILE *abrir_arquivo_utf8(const char *caminho, const char *modo) {
    wchar_t wcaminho[MYTHARA_MAX_CAMINHO], wmodo[16];
    if (!MultiByteToWideChar(CP_UTF8, 0, caminho, -1, wcaminho,
                             (int)(sizeof(wcaminho) / sizeof(wcaminho[0]))))
        return NULL;
    if (!MultiByteToWideChar(CP_UTF8, 0, modo, -1, wmodo, (int)(sizeof(wmodo) / sizeof(wmodo[0]))))
        return NULL;
    return _wfopen(wcaminho, wmodo);
}
#define fopen abrir_arquivo_utf8
#endif

/* Caminhos e escrita atomica ------------------------------------------------ */

static int substituir_arquivo(const char *origem, const char *destino) {
#ifdef _WIN32
    wchar_t worigem[MYTHARA_MAX_CAMINHO + 16], wdestino[MYTHARA_MAX_CAMINHO + 16];
    if (!MultiByteToWideChar(CP_UTF8, 0, origem, -1, worigem,
                             (int)(sizeof(worigem) / sizeof(worigem[0]))) ||
        !MultiByteToWideChar(CP_UTF8, 0, destino, -1, wdestino,
                             (int)(sizeof(wdestino) / sizeof(wdestino[0]))))
        return 0;
    return MoveFileExW(worigem, wdestino, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    return rename(origem, destino) == 0;
#endif
}

static void normalizar_caminho(char *caminho) {
    for (char *p = caminho; *p; ++p)
        if (*p == '\\')
            *p = '/';
}

static int caminho_absoluto(const char *caminho) {
#ifdef _WIN32
    return (isalpha((unsigned char)caminho[0]) && caminho[1] == ':') || caminho[0] == '/' ||
           caminho[0] == '\\';
#else
    return caminho[0] == '/';
#endif
}

static int caminho_relativo_seguro(const char *caminho) {
    if (!caminho || !caminho[0] || caminho[0] == '/' || caminho[0] == '\\' ||
        (isalpha((unsigned char)caminho[0]) && caminho[1] == ':'))
        return 0;
    char copia[MYTHARA_MAX_CAMINHO];
    copiar_texto(copia, sizeof(copia), caminho);
    normalizar_caminho(copia);
    char *segmento = copia;
    while (segmento && *segmento) {
        char *barra = strchr(segmento, '/');
        if (barra)
            *barra = 0;
        if (!strcmp(segmento, ".."))
            return 0;
        segmento = barra ? barra + 1 : NULL;
    }
    return 1;
}

static void pasta_do_arquivo(const char *caminho, char *destino, size_t capacidade) {
    copiar_texto(destino, capacidade, caminho);
    normalizar_caminho(destino);
    char *barra = strrchr(destino, '/');
    if (barra)
        *barra = 0;
    else
        copiar_texto(destino, capacidade, ".");
}

static int juntar_caminho(char *destino, size_t capacidade, const char *pasta, const char *nome) {
    int n = snprintf(destino, capacidade, "%s%s%s", pasta,
                     pasta[0] && pasta[strlen(pasta) - 1] != '/' ? "/" : "", nome);
    if (n < 0 || (size_t)n >= capacidade)
        return 0;
    normalizar_caminho(destino);
    return 1;
}

static int criar_diretorios(const char *caminho) {
    char copia[MYTHARA_MAX_CAMINHO];
    copiar_texto(copia, sizeof(copia), caminho);
    normalizar_caminho(copia);
    size_t inicio = 1;
#ifdef _WIN32
    if (isalpha((unsigned char)copia[0]) && copia[1] == ':')
        inicio = 3;
#endif
    for (size_t i = inicio; copia[i]; ++i)
        if (copia[i] == '/') {
            char salvo = copia[i];
            copia[i] = 0;
            if (copia[0] && mkdir(copia, 0755) != 0 && errno != EEXIST)
                return 0;
            copia[i] = salvo;
        }
    return !copia[0] || mkdir(copia, 0755) == 0 || errno == EEXIST;
}

/* Enumeracao de diretorios -------------------------------------------------- */

typedef struct {
    char nome[256];
    int diretorio;
} ItemDiretorio;

static int comparar_itens_diretorio(const void *a, const void *b) {
    const ItemDiretorio *x = a, *y = b;
    if (x->diretorio != y->diretorio)
        return y->diretorio - x->diretorio;
    return strcasecmp(x->nome, y->nome);
}

static int listar_diretorio(const char *pasta, ItemDiretorio *itens, int maximo) {
    int quantidade = 0;
#ifdef _WIN32
    char padrao[MYTHARA_MAX_CAMINHO];
    juntar_caminho(padrao, sizeof(padrao), pasta, "*");
    wchar_t wpadrao[MYTHARA_MAX_CAMINHO];
    if (!MultiByteToWideChar(CP_UTF8, 0, padrao, -1, wpadrao, MYTHARA_MAX_CAMINHO))
        return 0;
    WIN32_FIND_DATAW d;
    HANDLE busca = FindFirstFileW(wpadrao, &d);
    if (busca == INVALID_HANDLE_VALUE)
        return 0;
    do {
        char nome[256];
        if (!WideCharToMultiByte(CP_UTF8, 0, d.cFileName, -1, nome, sizeof(nome), NULL, NULL))
            continue;
        if (!strcmp(nome, ".") || !strcmp(nome, ".."))
            continue;
        copiar_texto(itens[quantidade].nome, sizeof(itens[quantidade].nome), nome);
        itens[quantidade].diretorio = (d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        quantidade++;
    } while (quantidade < maximo && FindNextFileW(busca, &d));
    FindClose(busca);
#else
    DIR *dir = opendir(pasta);
    if (!dir)
        return 0;
    struct dirent *d;
    while (quantidade < maximo && (d = readdir(dir))) {
        if (!strcmp(d->d_name, ".") || !strcmp(d->d_name, ".."))
            continue;
        copiar_texto(itens[quantidade].nome, sizeof(itens[quantidade].nome), d->d_name);
        char completo[MYTHARA_MAX_CAMINHO];
        struct stat st;
        juntar_caminho(completo, sizeof(completo), pasta, d->d_name);
        itens[quantidade].diretorio = stat(completo, &st) == 0 && S_ISDIR(st.st_mode);
        quantidade++;
    }
    closedir(dir);
#endif
    qsort(itens, (size_t)quantidade, sizeof(*itens), comparar_itens_diretorio);
    return quantidade;
}

static void subir_diretorio(char *pasta) {
    normalizar_caminho(pasta);
    size_t n = strlen(pasta);
    while (n > 1 && pasta[n - 1] == '/')
        pasta[--n] = 0;
    char *barra = strrchr(pasta, '/');
    if (barra && barra != pasta)
        *barra = 0;
    else if (barra)
        pasta[1] = 0;
}
