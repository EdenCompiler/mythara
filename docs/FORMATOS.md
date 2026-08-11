# Formatos da Mythara

Esta é a referência de manutenção dos projetos e saves v4 e dos documentos locais de interface v5.
Todos os inteiros são gravados em ordem little-endian e com largura explícita. Textos são UTF-8
prefixados por tamanho; nenhuma estrutura C é copiada diretamente para o arquivo.

## Projeto `.myr`

O cabeçalho possui:

| Campo | Tipo | Valor |
|---|---|---|
| assinatura | 8 bytes | `MYTHRV4\0` |
| versão | `u32` | `4` |
| quantidade de chunks | `u32` | entre 1 e 64 |

Cada chunk possui:

| Campo | Tipo | Descrição |
|---|---|---|
| identificador | 4 bytes | por exemplo `DADO` |
| flags | `u32` | bit 0 indica chunk obrigatório |
| versão do chunk | `u32` | versão própria do conteúdo |
| tamanho | `u64` | bytes do payload |
| CRC | `u32` | CRC32 do payload |
| payload | bytes | conteúdo do chunk |

`DADO` versão 1 é único e obrigatório. Ele contém metadados, bancos de dados, eventos, recursos,
mapas, tiles, colisões e entidades em uma ordem canônica definida pelo serializador. Referências são
IDs persistentes de 64 bits, não posições de arrays. Um chunk opcional desconhecido pode ser
ignorado; um chunk obrigatório desconhecido é rejeitado.

O carregador limita o arquivo a 512 MiB, valida comprimentos antes de avançar, confere CRC, UTF-8,
quantidades, dimensões, IDs e referências. Dados excedentes e estruturas incompletas são rejeitados.

## Save `.mys`

| Campo | Tipo | Descrição |
|---|---|---|
| assinatura | 8 bytes | `MYTSAV4\0` |
| versão | `u32` | `4` |
| ID do projeto | `u64` | impede usar o save no projeto errado |
| tamanho | `u64` | tamanho do payload |
| CRC | `u32` | CRC32 do payload |
| payload | bytes | estado do runtime |

Mapa, itens, equipamentos, missões, heróis e estados são identificados por IDs persistentes. A
Mythara 4 não converte saves v3.

## Tema `.myt` e configuração `.myc` v5

Assinaturas: `MYTTEM5\0` e `MYTCFG5\0`. Ambos usam o envelope:

| Campo | Tipo |
|---|---|
| assinatura | 8 bytes |
| versão | `u32` |
| tamanho do payload | `u64` |
| CRC32 | `u32` |
| payload | bytes |

O tema guarda as cores semânticas, métricas, preset e preferência de movimento da interface. A
configuração guarda o tema, dimensões de painéis, workspace, estado da barra lateral, preferências,
idioma do editor e projeto recente. Uma configuração antiga ou inválida é ignorada com retorno
seguro aos padrões.

Temas v4 são rejeitados com uma mensagem explícita e devem ser recriados a partir dos presets v5.
Configurações v4 não são migradas; o editor inicia com o preset Arcano Noturno e grava uma nova
configuração v5 ao encerrar.

Local padrão da configuração:

- Linux: `$XDG_CONFIG_HOME/mythara/config.myc` ou `~/.config/mythara/config.myc`.
- Windows: `%APPDATA%/mythara/config.myc`.

## Escrita atômica e autosave

Arquivos v4 são escritos em `<destino>.tmp`, recebem `fflush`/sincronização e só então substituem o
destino por rename. Autosaves usam exatamente o formato `.myr` v4 em `.mythara/backups`, com até dez
snapshots rotativos de `autosave_0.myr` a `autosave_9.myr`.

## Compatibilidade

- O editor reconhece projetos v3 apenas para o assistente de migração.
- A migração sempre cria outro arquivo v4 e preserva a origem.
- O runtime e a recuperação de autosave exigem projetos v4.
- O leitor v4 não tenta adivinhar versões ou layouts.
- Projetos e saves permanecem em v4; a versão v5 é exclusiva a temas e configuração do editor.
- Campos incompatíveis exigem nova versão do chunk ou do documento.

Consulte [MIGRACAO_V4.md](MIGRACAO_V4.md) para o fluxo voltado a usuários.
