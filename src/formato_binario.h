#ifndef MYTHARA_FORMATO_BINARIO_H
#define MYTHARA_FORMATO_BINARIO_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t *dados;
    size_t tamanho;
    size_t capacidade;
    int erro;
} EscritorBinario;

typedef struct {
    const uint8_t *dados;
    size_t tamanho;
    size_t posicao;
    int erro;
} LeitorBinario;

void escritor_binario_liberar(EscritorBinario *escritor);
int escritor_binario_bytes(EscritorBinario *escritor, const void *dados, size_t tamanho);
int escritor_binario_u8(EscritorBinario *escritor, uint8_t valor);
int escritor_binario_u16(EscritorBinario *escritor, uint16_t valor);
int escritor_binario_u32(EscritorBinario *escritor, uint32_t valor);
int escritor_binario_i32(EscritorBinario *escritor, int32_t valor);
int escritor_binario_u64(EscritorBinario *escritor, uint64_t valor);
int escritor_binario_texto(EscritorBinario *escritor, const char *texto);

int leitor_binario_bytes(LeitorBinario *leitor, void *destino, size_t tamanho);
uint8_t leitor_binario_u8(LeitorBinario *leitor);
uint16_t leitor_binario_u16(LeitorBinario *leitor);
uint32_t leitor_binario_u32(LeitorBinario *leitor);
int32_t leitor_binario_i32(LeitorBinario *leitor);
uint64_t leitor_binario_u64(LeitorBinario *leitor);
int leitor_binario_texto(LeitorBinario *leitor, char *destino, size_t capacidade);
int leitor_binario_fim(const LeitorBinario *leitor);

uint32_t crc32_binario(const void *dados, size_t tamanho);
int texto_utf8_valido(const char *texto);

#endif
