# Mythara

Mythara é um motor e editor visual de JRPG 2D escrito em C11. O motor inteiro permanece em um
único arquivo-fonte, enquanto o repositório fornece build, testes, documentação e automação para
Linux e Windows.

O editor inclui mapas em camadas, eventos visuais, banco de dados de RPG, recursos gráficos e de
áudio, playtest integrado, combate, grupo, lojas, missões, autosave e exportação nativa.

## Estado do projeto

- Versão do formato: 3
- Plataforma Linux: X11, com ALSA opcional
- Plataforma Windows: Win32, GDI e WinMM
- Linguagem: C11
- Dependências incorporadas: nenhuma
- Código do motor: [`src/mythara.c`](src/mythara.c)
- Testes: 26 autotestes executáveis pelo próprio programa

## Início rápido no Linux

Instale as dependências de desenvolvimento no Debian ou Ubuntu:

```bash
sudo apt install build-essential libx11-dev libasound2-dev
```

Compile e teste:

```bash
make RELEASE=1
make RELEASE=1 test
./build/make/mythara
```

Para compilar sem ALSA:

```bash
make AUDIO=0 RELEASE=1
```

## Build com CMake

```bash
cmake --preset desenvolvimento
cmake --build --preset desenvolvimento
ctest --preset desenvolvimento
```

Presets disponíveis:

- `desenvolvimento`: debug, áudio e avisos tratados como erros.
- `release`: otimização e áudio.
- `sem-audio`: otimização sem ALSA.

Também é possível configurar manualmente:

```bash
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release -DMYTHARA_AUDIO=ON
cmake --build build/release
ctest --test-dir build/release --output-on-failure
```

## Build no Windows

Com MinGW:

```powershell
gcc -std=c11 -O2 -Wall -Wextra -Wpedantic src/mythara.c `
    -o mythara.exe -lgdi32 -luser32 -lshell32 -lwinmm -lm
```

Ou use o script do repositório:

```powershell
./scripts/compilar-windows.ps1
```

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

A escala da interface pode ser forçada para testes e acessibilidade:

```bash
MYTHARA_ESCALA=200 ./mythara
```

## Estrutura de um projeto Mythara

Ao criar um projeto, o editor prepara esta estrutura:

```text
meu_projeto/
├── projeto.myr
├── recursos/
│   ├── audio/
│   └── imagens/
├── exportacoes/
└── .mythara/
    └── backups/
        ├── autosave_0.myr
        └── ...
```

Extensões usadas:

| Extensão | Conteúdo |
|---|---|
| `.myr` | Projeto Mythara v3 |
| `.mys` | Estado salvo do jogo |
| `.myt` | Tema da interface |
| `.myc` | Configuração local do editor |

Projetos anteriores ao formato v3 são rejeitados deliberadamente. Consulte
[`docs/FORMATOS.md`](docs/FORMATOS.md) antes de alterar a persistência.

## Estrutura do repositório

```text
.
├── .github/workflows/   Integração contínua
├── docs/                Arquitetura, formatos e compilação
├── scripts/             Build, testes e empacotamento
├── src/mythara.c        Motor e editor completos
├── CMakeLists.txt       Build CMake
├── CMakePresets.json    Configurações reproduzíveis
└── Makefile             Build direto
```

## Desenvolvimento

Antes de enviar uma alteração:

```bash
./scripts/verificar.sh
```

O script verifica a formatação, compila com avisos como erros e executa os autotestes.
As regras de contribuição estão em [`CONTRIBUTING.md`](CONTRIBUTING.md), e a visão arquitetural em
[`docs/ARQUITETURA.md`](docs/ARQUITETURA.md).

## Licença

Uma licença ainda não foi definida. Até que o responsável pelo projeto escolha e adicione uma
licença, o código não deve ser presumido como software de código aberto.
