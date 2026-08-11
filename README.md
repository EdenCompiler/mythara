# Mythara

Mythara é um motor e editor visual de JRPG 2D escrito em C11. Ele reúne mapas em camadas, eventos
visuais, banco de dados de RPG, recursos gráficos e de áudio, playtest, combate, grupo, lojas,
missões, autosave e exportação nativa em um único executável.

A versão 4 prioriza quem está criando seu primeiro jogo: listas pesquisáveis e roláveis,
referências mostradas por nome, validação antes de salvar/testar/exportar, proteção contra perda de
alterações e interface em português ou inglês.

## Estado do projeto

- Versão do motor e dos formatos: 4.0.0
- Linux: X11, com ALSA opcional
- Windows: Win32, GDI e WinMM
- Linguagem: C11, sem dependências incorporadas
- Persistência portátil little-endian com CRC32 e IDs estáveis
- 33 autotestes executáveis pelo próprio programa
- Licença: MIT

## Início rápido no Linux

No Debian ou Ubuntu:

```bash
sudo apt install build-essential cmake libx11-dev libasound2-dev
cmake --preset desenvolvimento
cmake --build --preset desenvolvimento
ctest --preset desenvolvimento
./build/desenvolvimento/mythara
```

Também é possível usar Make:

```bash
make RELEASE=1
make RELEASE=1 test
./build/make/mythara
```

Para compilar sem ALSA, use o preset `sem-audio` ou `make AUDIO=0 RELEASE=1`.

## Build no Windows

Em um terminal com CMake e Visual Studio ou MinGW disponíveis:

```powershell
./scripts/compilar-windows.ps1
```

Consulte [docs/COMPILACAO.md](docs/COMPILACAO.md) para comandos diretos, sanitizadores e
empacotamento.

## Uso

```text
./mythara [projeto.myr]
./mythara --jogar projeto.myr
./mythara --novo projeto.myr
./mythara --recuperar projeto.myr
./mythara --resetar-layout
./mythara --autoteste
./mythara --ajuda
```

A escala da interface pode ser forçada entre 100% e 200%:

```bash
MYTHARA_ESCALA=200 ./mythara
```

## Primeiro projeto

1. Abra a Mythara e escolha **Criar projeto limpo**.
2. Edite o mapa inicial e use o inspetor para posicionar o herói.
3. Abra **Banco de dados** para ajustar heróis, itens e inimigos.
4. Crie diálogos e lógica em **Eventos** e ligue-os às entidades pelo nome.
5. Use **Playtest**; o validador indica a área a corrigir se houver referência inválida.
6. Salve e use **Exportar jogo** para gerar a distribuição nativa.

O editor pede confirmação antes de descartar alterações. Projetos v3 são abertos por um assistente
que cria uma cópia v4; o arquivo original nunca é sobrescrito. Saves v3 não são migrados.

## Arquivos

| Extensão | Conteúdo |
|---|---|
| `.myr` | Projeto Mythara v4 |
| `.mys` | Save v4 vinculado ao ID do projeto |
| `.myt` | Tema portátil v4 |
| `.myc` | Configuração local v4 |

Um projeto novo usa esta estrutura:

```text
meu_projeto/
├── projeto.myr
├── recursos/
│   ├── audio/
│   └── imagens/
├── exportacoes/
└── .mythara/backups/
    ├── autosave_0.myr
    └── ...
```

Veja [docs/FORMATOS.md](docs/FORMATOS.md) e [docs/MIGRACAO_V4.md](docs/MIGRACAO_V4.md) antes de
alterar a persistência.

## Estrutura do repositório

```text
src/mythara.c             Unidade de composição do executável
src/interno/base.inc      Utilitários e tipos fundamentais
src/interno/modelo.inc    Ciclo de vida do modelo persistente
src/interno/renderizador.inc  Framebuffer e fonte bitmap
src/interno/interface.inc UI imediata e edição de texto
src/interno/plataforma.inc    X11, Win32, áudio e entrada
src/interno/persistencia.inc  Projetos, saves, temas e configuração
src/interno/recursos_exportacao.inc  Imagens, áudio e distribuição
src/interno/aplicativo.inc    Estado e operações do editor
src/interno/editor.inc    Workspace e painéis do editor
src/interno/modais.inc    Janelas e fluxos guiados
src/interno/runtime.inc   Eventos, exploração e combate
src/interno/programa.inc  Autotestes, CLI e `main`
src/modelo.h              Modelo persistente compartilhado
src/formato_binario.*     Codec little-endian e CRC32
src/idioma.*              Localização português/inglês
```

Antes de enviar uma alteração, execute:

```bash
./scripts/verificar.sh
```

As regras de contribuição estão em [CONTRIBUTING.md](CONTRIBUTING.md). A Mythara é distribuída sob
a [licença MIT](LICENSE).
