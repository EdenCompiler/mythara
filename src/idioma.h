#ifndef MYTHARA_IDIOMA_H
#define MYTHARA_IDIOMA_H

typedef enum { IDIOMA_PORTUGUES, IDIOMA_INGLES } IdiomaMythara;

void idioma_definir(IdiomaMythara idioma);
IdiomaMythara idioma_obter(void);
const char *idioma_traduzir(const char *portugues);

#endif
