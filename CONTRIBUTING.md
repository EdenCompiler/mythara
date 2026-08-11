# Contribuindo com a Mythara

Obrigado por ajudar a desenvolver a Mythara. O objetivo central é manter um motor de JRPG legível,
portátil, pequeno e organizado em módulos internos claros.

## Preparação

1. Instale um compilador C11, CMake 3.16 ou superior, X11 e opcionalmente ALSA.
2. Configure o preset de desenvolvimento com `cmake --preset desenvolvimento`.
3. Compile com `cmake --build --preset desenvolvimento`.
4. Execute `ctest --preset desenvolvimento`.

## Regras do código

- Coloque cada funcionalidade no módulo correspondente em `src/interno/`; `src/mythara.c` deve
  permanecer apenas como unidade de composição.
- Módulos `.inc` não são compilados isoladamente e devem respeitar a ordem documentada em
  `docs/ARQUITETURA.md`.
- Nomes internos e comentários próprios do projeto devem permanecer em português.
- APIs externas conservam os nomes definidos por suas bibliotecas.
- Não adicione uma dependência sem justificar portabilidade, tamanho e manutenção.
- Recursos alocados devem ter um caminho de liberação testável.
- Alterações persistentes exigem atualização de `docs/FORMATOS.md` e do número de versão.
- Interface nova precisa funcionar em 1280×720 com escala de 100% e 200%.
- Use `clang-format` com a configuração do repositório.

## Validação obrigatória

```bash
./scripts/verificar.sh
```

Para alterações de plataforma, valide também:

```bash
make AUDIO=0 RELEASE=1 test
x86_64-w64-mingw32-gcc -std=c11 -O2 -Wall -Wextra -Wpedantic \
    src/mythara.c src/formato_binario.c src/idioma.c \
    -o /tmp/mythara.exe -lgdi32 -luser32 -lshell32 -lwinmm -lm
```

## Commits e propostas

- Faça commits pequenos, com uma responsabilidade clara.
- Explique o comportamento anterior, a mudança e como ela foi testada.
- Atualize `CHANGELOG.md` quando a mudança for visível para usuários.
- Não inclua binários, builds locais, projetos pessoais ou arquivos de autosave.

## Bugs de segurança

Não publique detalhes exploráveis em uma issue aberta. Siga as orientações de `SECURITY.md`.
