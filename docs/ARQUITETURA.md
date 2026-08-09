# Arquitetura da Mythara

## Princípio central

A Mythara mantém todo o motor e editor em `src/mythara.c`. Isso reduz o número de dependências e
facilita copiar, auditar e compilar o motor. A modularidade é lógica: cada subsistema ocupa uma seção
claramente marcada no arquivo.

## Subsistemas

```text
Plataforma X11/Win32
        │
        ▼
Entrada ─────► Toolkit imediato ─────► Editor visual
        │                                  │
        │                                  ▼
        └────────────────────────────► Modelo do projeto
                                           │
                  ┌────────────────────────┼─────────────────────┐
                  ▼                        ▼                     ▼
             Persistência              Runtime              Exportação
                  │                        │                     │
                  ▼                        ▼                     ▼
           .myr/.mys/.myt          Eventos e combate      Jogo distribuível
```

### Configuração e tipos básicos

Define limites, cores, identificadores, caminhos e utilitários portáveis. IDs persistentes usam
`uint64_t` e são gerados pelo contador `Projeto.proximo_id`.

### Modelo persistente

Contém mapas, camadas, entidades, eventos, comandos, heróis, classes, habilidades, itens, inimigos,
estados, lojas, missões e recursos. Estruturas com quantidade variável usam capacidade e ponteiro
dinâmico; estruturas pequenas e limitadas permanecem embutidas.

### Renderizador e fonte

Renderização totalmente por software em um framebuffer ARGB de 32 bits. A plataforma apresenta o
buffer por XImage no Linux ou `StretchDIBits` no Windows. A fonte bitmap está incorporada no fonte.

### Toolkit imediato

Botões, campos, números, checkboxes, painéis e modais são reconstruídos a cada quadro. O estado
persistente mínimo inclui foco, cursor, seleção e rolagem textual.

### Plataforma

- Linux: X11, pthread e ALSA opcional.
- Windows: Win32, GDI, threads do sistema e WinMM opcional.

A camada recebe teclado, mouse, redimensionamento e texto UTF-8, além de apresentar o framebuffer.

### Persistência

Projetos são serializados em memória, recebem checksum FNV-1a e são escritos primeiro em um arquivo
temporário. O rename final reduz o risco de deixar um projeto parcialmente salvo. Consulte
`FORMATOS.md` para as restrições de compatibilidade.

### Recursos e exportação

Imagens BMP, QOI e TGA são decodificadas internamente. WAV PCM é reproduzido pela API do sistema.
Na importação, recursos são copiados para o projeto e armazenados por caminho relativo.

### Editor

O estado do editor inclui workspace atual, painéis, seleção, ferramenta, clipboard estruturado e
histórico. O histórico global armazena snapshots profundos limitados por quantidade e memória.

### Runtime

Executa movimentação, colisão, eventos, decisões, flags, variáveis, inventário, grupo, missões,
lojas e combate por rodadas. O mesmo executável entra no runtime por `--jogar` ou pelo nome `jogo`.

## Fluxo de um quadro

1. A plataforma limpa os eventos transitórios e coleta a entrada.
2. O editor converte a entrada para a escala lógica configurada.
3. A UI e o conteúdo são desenhados no framebuffer lógico.
4. Alterações no projeto são detectadas para o histórico global.
5. O autosave pode serializar um snapshot e delegar a escrita a uma thread.
6. O framebuffer é escalado e apresentado pela janela nativa.

## Memória e propriedade

- `Projeto` é dono de mapas, eventos, itens, inimigos e recursos dinâmicos.
- `Mapa` é dono de tiles, colisões e entidades.
- `Evento` é dono de sua lista de comandos.
- `Aplicativo` é dono do projeto, imagens carregadas, editor e runtime.
- Snapshots de histórico são clones profundos e precisam ser liberados individualmente.

Ao adicionar um ponteiro persistente, atualize em conjunto inicialização, clone, serialização,
desserialização e liberação.

## Restrições deliberadas

- Um único arquivo-fonte para o motor.
- Nenhum framework de UI ou renderização.
- Formato v3 estrito, sem conversão automática de versões anteriores.
- Limites explícitos para evitar crescimento de memória sem controle.
- Exportação nativa, sem runtime web ou móvel.
