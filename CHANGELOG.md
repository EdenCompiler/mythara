# Histórico de mudanças

Este projeto segue o formato do [Keep a Changelog](https://keepachangelog.com/pt-BR/1.1.0/) e usa
versionamento semântico para as versões do motor.

## [Não publicado]

### Planejado

- Escolha e publicação de uma licença.
- Mais cobertura automatizada para interação visual.
- Migração explícita entre futuras versões do formato.

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
