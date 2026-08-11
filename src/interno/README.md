# Módulos internos

Estes arquivos formam uma única unidade C através de `src/mythara.c`. A ordem dos includes é a
ordem de dependência: um módulo pode usar tipos e funções definidos pelos módulos anteriores, mas
não pelos posteriores.

## Como navegar

1. `preludio.inc`: APIs do sistema e cabeçalhos compartilhados.
2. `base.inc`: tipos primitivos, caminhos, arquivos e tempo.
3. `modelo.inc`: memória, IDs e ciclo de vida do projeto.
4. `renderizador.inc` e `interface.inc`: apresentação e controles.
5. `plataforma.inc`: backends X11 e Win32.
6. `persistencia.inc` e `recursos_exportacao.inc`: limites de entrada e saída.
7. `aplicativo.inc`: estado compartilhado e operações do documento.
8. `editor.inc` e `modais.inc`: experiência de autoria.
9. `runtime.inc`: execução do jogo.
10. `programa.inc`: testes, CLI e `main`.

## Convenções

- Funções e tipos internos usam nomes em português.
- Divisores com `---` marcam grupos funcionais dentro de um módulo.
- Comentários explicam responsabilidade, invariantes ou decisões; não repetem a implementação.
- Funções que leem dados externos validam limites antes de alocar ou avançar buffers.
- Estado persistente usa IDs no disco e índices somente em memória.
- Novas funcionalidades devem entrar no módulo que possui o estado alterado.
