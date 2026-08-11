# Arquitetura da Mythara 4

## Direção

A Mythara continua sendo um único executável C11, sem framework de UI, renderização ou
serialização. Todo subsistema possui um arquivo próprio. Os módulos fortemente acoplados são
compostos por `src/mythara.c` em uma unidade C; codec binário e localização são unidades de
compilação independentes. Assim, detalhes internos continuam com ligação privada e não viram uma
API pública artificial.

```text
X11 / Win32 ──► entrada e framebuffer ──► UI imediata ──► editor
                                                        │
                         modelo.h ◄─────────────────────┤
                            │                           │
             ┌──────────────┼───────────────┐           │
             ▼              ▼               ▼           ▼
       persistência      runtime        validador   exportação
             │
       formato_binario.c
```

## Módulos

### `src/modelo.h`

Declara limites, identificadores estáveis e estruturas persistentes: mapas, camadas, entidades,
eventos, comandos, recursos e bancos de RPG. Também contém o estado do jogo necessário ao formato
de save. Ele não expõe uma API pública.

### `src/formato_binario.c`

Fornece leitores e escritores limitados para inteiros little-endian, bytes, textos UTF-8 e CRC32.
Uma leitura truncada ou acima dos limites marca o fluxo como inválido; os chamadores nunca usam o
layout de uma `struct` C como layout de disco no formato v4.

### `src/idioma.c`

Mantém o idioma ativo e o catálogo interno português/inglês. Português é o padrão. O idioma do
editor fica na configuração local e o idioma do jogo faz parte do projeto.

### Unidade de composição

`src/mythara.c` contém somente os includes dos módulos, em ordem de dependência. Não contém
implementação de funcionalidades.

| Módulo interno | Responsabilidade |
|---|---|
| `preludio.inc` | includes portáveis, opções de plataforma e dependências internas |
| `base.inc` | tipos fundamentais, caminhos, arquivos e utilitários |
| `modelo.inc` | alocação, IDs, inicialização e liberação do projeto |
| `renderizador.inc` | framebuffer por software e fonte bitmap |
| `interface.inc` | toolkit imediato, campos UTF-8 e controles |
| `plataforma.inc` | janela, entrada, clipboard, tempo e apresentação X11/Win32 |
| `persistencia.inc` | validação e formatos de projeto, save, tema e configuração |
| `recursos_exportacao.inc` | BMP, QOI, TGA, WAV, importação e exportação |
| `aplicativo.inc` | estado do aplicativo, histórico e operações seguras |
| `editor.inc` | barras, árvore, mapa, inspetor e painéis |
| `modais.inc` | banco, eventos, recursos, migração e ciclo do documento |
| `runtime.inc` | exploração, eventos, grupo, lojas, missões e combate |
| `programa.inc` | autotestes, linha de comando e ponto de entrada |

Arquivos `.inc` são código C interno, formatado e acompanhado pelo CMake e Make. Eles não devem ser
compilados isoladamente: dependências seguem a ordem explícita da unidade de composição.
O guia curto de navegação e convenções fica em [`src/interno/README.md`](../src/interno/README.md).

## Modelo e propriedade

- `Projeto` é dono de mapas, eventos, itens, inimigos e recursos dinâmicos.
- `Mapa` é dono de tiles, colisões e entidades.
- `Evento` é dono de seus comandos.
- `Aplicativo` é dono do projeto, imagens, editor e runtime.
- O histórico usa clones profundos limitados por quantidade e memória.

Ao adicionar um campo persistente, atualize inicialização, clone, validação, serialização v4,
desserialização v4, migração v3 quando aplicável e liberação.

## Persistência e referências

Cada objeto persistente recebe um `Identificador` de 64 bits. A memória pode usar índices para
acesso rápido, mas arquivos v4 gravam IDs nas referências. O carregador reconstrói e valida os
índices somente depois de ler todos os objetos.

Projetos são gravados em chunks com versão, flags, comprimento de 64 bits e CRC32. A escrita usa um
arquivo temporário sincronizado antes do rename. Chunks opcionais desconhecidos são ignorados;
chunks obrigatórios desconhecidos impedem a abertura.

## Validação

Salvar, iniciar playtest e exportar passam pelo mesmo validador estrutural. Ele verifica limites,
UTF-8, IDs únicos, dimensões e memória de mapas, caminhos relativos seguros, referências e
aninhamento de comandos de controle. A UI leva o criador para a área relacionada ao primeiro erro.

## Fluxo de um quadro

1. A plataforma coleta entrada, texto UTF-8, redimensionamento e pedido de fechamento.
2. O editor transforma coordenadas para a escala lógica.
3. UI e conteúdo são desenhados no framebuffer ARGB.
4. Alterações alimentam o histórico e o estado de documento modificado.
5. O autosave pode serializar um snapshot v4 e delegar a escrita a uma thread.
6. O framebuffer é escalado e apresentado pela janela nativa.

## Restrições deliberadas

- Um único executável e nenhuma API pública ou sistema de plugins.
- Nenhuma dependência incorporada; apenas APIs nativas e bibliotecas de sistema.
- Limites explícitos para arquivos e alocações.
- Exportação nativa, sem runtime web ou móvel.
- Projetos v3 só entram pelo fluxo explícito de migração para uma cópia v4.
