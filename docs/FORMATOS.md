# Formatos da Mythara

Este documento descreve o formato v3 atual. Ele serve como referência de manutenção, não como uma
especificação estável para implementações externas.

## Projeto `.myr`

O arquivo começa com:

| Campo | Tipo | Valor |
|---|---|---|
| `magia` | 8 bytes | `MYTHRV3\0` |
| `versao` | `uint32_t` | `3` |
| `quantidade_blocos` | `uint32_t` | `1` |

O bloco principal contém:

| Campo | Descrição |
|---|---|
| `id` | Quatro bytes: `DADO` |
| `tamanho` | Tamanho do conteúdo em bytes |
| `soma` | FNV-1a de 32 bits sobre o conteúdo |
| `dados` | Projeto serializado |

O conteúdo de `DADO` segue esta ordem:

1. Dados fixos do projeto e bancos de tamanho limitado.
2. Quantidades de mapas, eventos, itens, inimigos e recursos.
3. Arrays de itens, inimigos e recursos.
4. Cada mapa, suas camadas, tiles, colisões e entidades.
5. Cada evento e seus comandos.

IDs persistentes são salvos junto aos objetos. Referências de entidades para eventos são resolvidas
por ID após o carregamento.

### Escrita atômica

O projeto é serializado por completo, escrito em `<arquivo>.tmp`, sincronizado e renomeado para o
destino. Uma falha antes do rename preserva o último arquivo completo.

### Limitações de portabilidade

O v3 ainda serializa algumas estruturas C diretamente. Portanto, pressupõe:

- inteiros com os tamanhos usados pelos compiladores suportados;
- mesma ordem de bytes;
- layout de estruturas compatível;
- ausência de corrupção ou manipulação externa.

Uma futura versão verdadeiramente interoperável deve definir campos byte a byte, ordem little-endian
e tamanhos independentes do compilador.

## Save `.mys`

O estado do runtime usa:

| Campo | Conteúdo |
|---|---|
| `magia` | `MYTSAVE\0` |
| `soma` | FNV-1a do estado |
| `estado` | Estrutura `EstadoJogo` |

Saves dependem da organização do banco de dados do projeto que os criou.

## Tema `.myt`

O tema usa a assinatura `MYTTEMA`, checksum FNV-1a e a estrutura `TemaInterface`. A escala é
normalizada para o intervalo de 100% a 200% ao carregar.

## Configuração `.myc`

A configuração local usa a assinatura `MYTCONF`, versão 3, tema, dimensões dos painéis, workspace,
preferências da tela inicial e caminho do projeto recente.

Local padrão:

- Linux: `$XDG_CONFIG_HOME/mythara/config.myc` ou `~/.config/mythara/config.myc`.
- Windows: `%APPDATA%/mythara/config.myc`.

## Autosave

Snapshots usam o mesmo formato `.myr` e ficam em `.mythara/backups`. Existem até dez arquivos,
numerados de `autosave_0.myr` a `autosave_9.myr`, sendo zero o mais recente.

## Política de versão

- A Mythara 3 abre somente projetos v3 com assinatura Mythara.
- Mudança incompatível exige incrementar `MYTHARA_VERSAO` e alterar a assinatura.
- Um conversor deve ser uma ferramenta explícita; o carregador principal não deve adivinhar layout.
- Novos campos opcionais exigem um mecanismo versionado antes de serem anexados ao formato atual.
