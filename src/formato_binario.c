#include "formato_binario.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

static int escritor_binario_reservar(EscritorBinario *escritor, size_t adicional) {
    if (escritor->erro)
        return 0;
    if (adicional > SIZE_MAX - escritor->tamanho) {
        escritor->erro = 1;
        return 0;
    }
    size_t necessario = escritor->tamanho + adicional;
    if (necessario <= escritor->capacidade)
        return 1;
    size_t nova = escritor->capacidade ? escritor->capacidade : 4096;
    while (nova < necessario) {
        if (nova > SIZE_MAX / 2) {
            nova = necessario;
            break;
        }
        nova *= 2;
    }
    void *dados = realloc(escritor->dados, nova);
    if (!dados) {
        escritor->erro = 1;
        return 0;
    }
    escritor->dados = dados;
    escritor->capacidade = nova;
    return 1;
}

void escritor_binario_liberar(EscritorBinario *escritor) {
    free(escritor->dados);
    memset(escritor, 0, sizeof(*escritor));
}

int escritor_binario_bytes(EscritorBinario *escritor, const void *dados, size_t tamanho) {
    if (!escritor_binario_reservar(escritor, tamanho))
        return 0;
    if (tamanho)
        memcpy(escritor->dados + escritor->tamanho, dados, tamanho);
    escritor->tamanho += tamanho;
    return 1;
}

int escritor_binario_u8(EscritorBinario *escritor, uint8_t valor) {
    return escritor_binario_bytes(escritor, &valor, 1);
}

int escritor_binario_u16(EscritorBinario *escritor, uint16_t valor) {
    uint8_t bytes[2] = {(uint8_t)valor, (uint8_t)(valor >> 8)};
    return escritor_binario_bytes(escritor, bytes, sizeof(bytes));
}

int escritor_binario_u32(EscritorBinario *escritor, uint32_t valor) {
    uint8_t bytes[4] = {(uint8_t)valor, (uint8_t)(valor >> 8), (uint8_t)(valor >> 16),
                        (uint8_t)(valor >> 24)};
    return escritor_binario_bytes(escritor, bytes, sizeof(bytes));
}

int escritor_binario_i32(EscritorBinario *escritor, int32_t valor) {
    return escritor_binario_u32(escritor, (uint32_t)valor);
}

int escritor_binario_u64(EscritorBinario *escritor, uint64_t valor) {
    uint8_t bytes[8];
    for (unsigned i = 0; i < 8; ++i)
        bytes[i] = (uint8_t)(valor >> (i * 8));
    return escritor_binario_bytes(escritor, bytes, sizeof(bytes));
}

int escritor_binario_texto(EscritorBinario *escritor, const char *texto) {
    size_t tamanho = texto ? strlen(texto) : 0;
    if (tamanho > UINT32_MAX)
        return 0;
    return escritor_binario_u32(escritor, (uint32_t)tamanho) &&
           escritor_binario_bytes(escritor, texto, tamanho);
}

int leitor_binario_bytes(LeitorBinario *leitor, void *destino, size_t tamanho) {
    if (leitor->erro || leitor->posicao > leitor->tamanho ||
        tamanho > leitor->tamanho - leitor->posicao) {
        leitor->erro = 1;
        return 0;
    }
    if (tamanho && destino)
        memcpy(destino, leitor->dados + leitor->posicao, tamanho);
    leitor->posicao += tamanho;
    return 1;
}

uint8_t leitor_binario_u8(LeitorBinario *leitor) {
    uint8_t valor = 0;
    leitor_binario_bytes(leitor, &valor, sizeof(valor));
    return valor;
}

uint16_t leitor_binario_u16(LeitorBinario *leitor) {
    uint8_t bytes[2] = {0};
    leitor_binario_bytes(leitor, bytes, sizeof(bytes));
    return (uint16_t)(bytes[0] | ((uint16_t)bytes[1] << 8));
}

uint32_t leitor_binario_u32(LeitorBinario *leitor) {
    uint8_t bytes[4] = {0};
    leitor_binario_bytes(leitor, bytes, sizeof(bytes));
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16) |
           ((uint32_t)bytes[3] << 24);
}

int32_t leitor_binario_i32(LeitorBinario *leitor) {
    return (int32_t)leitor_binario_u32(leitor);
}

uint64_t leitor_binario_u64(LeitorBinario *leitor) {
    uint8_t bytes[8] = {0};
    leitor_binario_bytes(leitor, bytes, sizeof(bytes));
    uint64_t valor = 0;
    for (unsigned i = 0; i < 8; ++i)
        valor |= (uint64_t)bytes[i] << (i * 8);
    return valor;
}

int leitor_binario_texto(LeitorBinario *leitor, char *destino, size_t capacidade) {
    uint32_t tamanho = leitor_binario_u32(leitor);
    if (leitor->erro || !capacidade || tamanho >= capacidade ||
        tamanho > leitor->tamanho - leitor->posicao) {
        leitor->erro = 1;
        if (capacidade)
            destino[0] = 0;
        return 0;
    }
    leitor_binario_bytes(leitor, destino, tamanho);
    destino[tamanho] = 0;
    if (!texto_utf8_valido(destino)) {
        leitor->erro = 1;
        destino[0] = 0;
        return 0;
    }
    return 1;
}

int leitor_binario_fim(const LeitorBinario *leitor) {
    return !leitor->erro && leitor->posicao == leitor->tamanho;
}

uint32_t crc32_binario(const void *dados, size_t tamanho) {
    const uint8_t *bytes = dados;
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0; i < tamanho; ++i) {
        crc ^= bytes[i];
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xedb88320u & (uint32_t)-(int32_t)(crc & 1u));
    }
    return ~crc;
}

int texto_utf8_valido(const char *texto) {
    const unsigned char *p = (const unsigned char *)texto;
    while (*p) {
        if (*p < 0x80) {
            p++;
            continue;
        }
        unsigned restantes;
        uint32_t codigo;
        if ((*p & 0xe0) == 0xc0) {
            restantes = 1;
            codigo = *p & 0x1f;
            if (codigo < 2)
                return 0;
        } else if ((*p & 0xf0) == 0xe0) {
            restantes = 2;
            codigo = *p & 0x0f;
        } else if ((*p & 0xf8) == 0xf0) {
            restantes = 3;
            codigo = *p & 0x07;
        } else
            return 0;
        p++;
        for (unsigned i = 0; i < restantes; ++i) {
            if ((p[i] & 0xc0) != 0x80)
                return 0;
            codigo = (codigo << 6) | (p[i] & 0x3f);
        }
        if ((restantes == 2 && codigo < 0x800) || (restantes == 3 && codigo < 0x10000) ||
            codigo > 0x10ffff || (codigo >= 0xd800 && codigo <= 0xdfff))
            return 0;
        p += restantes;
    }
    return 1;
}
