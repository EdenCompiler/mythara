# Histórico de mudanças

Este projeto segue o formato do [Keep a Changelog](https://keepachangelog.com/pt-BR/1.1.0/) e usa
versionamento semântico para as versões do motor.

## [Não publicado]

### Alterado

- O monólito restante foi separado em módulos internos de base, modelo, renderização, interface,
  plataforma, persistência, recursos, aplicativo, editor, modais, runtime e programa.

### Planejado

- Mais cobertura automatizada para interação visual.
- Migração explícita entre futuras versões do formato.

## [4.0.0] - 2026-08-11

### Adicionado

- Formatos portáteis v4 para projetos, saves, temas e configurações, com little-endian e CRC32.
- Assistente que migra projetos v3 para uma cópia v4 sem alterar a origem.
- Validação estrutural antes de salvar, executar playtest e exportar.
- Busca e rolagem nas listas principais do editor e referências exibidas por nome.
- Proteção contra descarte acidental ao abrir, criar ou fechar projetos.
- Interface em português ou inglês, com idiomas independentes para editor e jogo.
- Módulos internos para modelo, codec binário e localização.
- Testes de corrupção, compatibilidade v3, chunks, saves vinculados, ordem de bytes e framebuffer.
- Licença MIT.

### Alterado

- Exclusões do banco de dados agora recusam registros ainda referenciados.
- Exportação rejeita caminhos de recurso absolutos ou que escapem da pasta do projeto.
- A integração contínua testa Linux e Windows nativo e cobre todos os módulos.

### Removido

- Escrita de projetos, saves, temas e configurações no formato v3.

## [3.0.0] - 2026-08-09

### Adicionado

- Editor visual de mapas, eventos, banco de dados e recursos.
- Backends X11 e Win32 com renderização por software.
- Combate, grupo, habilidades, estados, lojas e missões.
- Histórico global com desfazer e refazer.
- Clipboard estruturado para tiles, colisões e entidades.
- Edição de texto UTF-8 com seleção e clipboard nativo.
- Interface responsiva com escala de 100% a 200%.
- Autosave assíncrono com dez snapshots rotativos.
- Exportação de executável e projeto para distribuição.
- Decodificadores BMP, QOI e TGA e reprodução de WAV.
- IDs persistentes para os objetos do projeto.
- Vinte e seis autotestes internos.

### Alterado

- Projeto renomeado integralmente para Mythara.
- Novas extensões `.myr`, `.mys`, `.myt` e `.myc`.
- Persistência restrita ao formato Mythara v3.
