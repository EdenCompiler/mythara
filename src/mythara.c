/*
 * Unidade de composicao da Mythara.
 *
 * Cada subsistema vive em src/interno. A ordem abaixo documenta as
 * dependencias internas e permite que o compilador mantenha os simbolos
 * privados entre os modulos, sem criar uma API publica artificial.
 */

#include "interno/preludio.inc"
#include "interno/base.inc"
#include "interno/modelo.inc"
#include "interno/renderizador.inc"
#include "interno/interface.inc"
#include "interno/plataforma.inc"
#include "interno/persistencia.inc"
#include "interno/recursos_exportacao.inc"
#include "interno/aplicativo.inc"
#include "interno/editor.inc"
#include "interno/modais.inc"
#include "interno/runtime.inc"
#include "interno/programa.inc"
