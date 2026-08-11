# Migração de projetos v3 para v4

A Mythara 4 abre projetos v3 somente pelo assistente de migração. O processo lê o formato legado,
normaliza IDs, valida o conteúdo e grava um novo projeto no formato portátil v4.

## Pelo editor

1. Abra o arquivo `.myr` v3 normalmente.
2. Confira a origem e o destino sugerido, normalmente `nome-v4.myr`.
3. Escolha **Criar cópia**.
4. Revise o resumo de mapas, eventos e recursos importados.
5. Salve e execute um playtest antes de continuar o trabalho.

O arquivo de origem nunca é sobrescrito. O destino deve ser diferente e não pode existir. Se a
validação encontrar uma estrutura ou referência inválida, a cópia não é criada e o editor mostra o
motivo.

## O que é migrado

- metadados e ponto inicial do projeto;
- mapas, camadas, tiles, colisões e entidades;
- eventos e comandos;
- heróis, classes, habilidades, itens, inimigos, estados, lojas e missões;
- registros e clipes de recursos.

Referências antigas baseadas em posição são convertidas para IDs persistentes no novo arquivo.

## O que não é migrado

- saves `.mys` v3;
- temas e configurações locais v3;
- autosaves v3.

Mantenha uma cópia do diretório antigo até concluir os playtests. Recursos continuam sendo
referenciados por caminhos relativos ao projeto, portanto preserve a pasta `recursos/` junto da nova
cópia ou reimporte os arquivos ausentes.

## Linha de comando

`--jogar` e `--recuperar` recusam projetos v3. A criação da cópia precisa ser feita no editor para
que o destino e o resultado possam ser confirmados explicitamente.
