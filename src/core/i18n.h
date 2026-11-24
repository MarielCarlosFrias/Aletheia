#ifndef I18N_H
#define I18N_H

#include <QLocale>
#include <QString>
#include <QtGlobal>

// Internacionalizacao leve, sem ferramentas externas no build: cada
// chamada traz o texto em PT-BR e em EN. Para PT/EN a traducao e a
// propria string (custo zero, sem ficheiros). Para os restantes idiomas
// (ex.: ES) e carregado, uma unica vez, um dicionario JSON em
//   :/i18n/aletheia_<codigo>.json            (recurso embutido) ou
//   <app>/translations/aletheia_<codigo>.json
// com pares {"texto em PT": "texto traduzido"}. Sem dicionario (ou com
// uma chave em falta) o texto cai para o EN: a interface nunca aparece
// com chaves cruas.
//
// O idioma ativo vem, nesta ordem, de
//   1. variavel de ambiente ALETHEIA_LANG=pt|en|es (BG_REMOVER_LANG
//      ainda funciona por compatibilidade)
//   2. QSettings (seletor de idioma na interface)
//   3. localidade do sistema (portugues -> PT-BR, espanhol -> ES,
//      senao EN)
namespace I18n {

enum class Lang { PtBr, En, Es };

// Codigo estavel usado em QSettings/ambiente ("pt", "en", "es").
QString code(Lang lang);

// Converte "pt", "pt_BR", "en-US", "es_ES", ... no Lang correspondente.
// Idiomas desconhecidos caem para o fallback indicado.
Lang fromCode(const QString& code, Lang fallback = Lang::En);

Lang detectLang();

// O idioma e detetado uma unica vez (mudar de idioma exige reiniciar a
// interface, que avisa o utilizador), por isso nao ha leitura de
// QSettings em cada TR().
Lang lang();

// Nome do idioma na propria lingua (para o seletor): Portugues, English,
// Espanol. Nao passa pelo tradutor de proposito.
QString nativeName(Lang lang);

// Verdadeiro se o idioma pode ser apresentado. PT/EN estao sempre
// disponiveis porque vivem no proprio codigo; os demais dependem da
// existencia de um dicionario embutido ou instalado.
bool dictionaryAvailable(Lang lang);

QString tr(const char* ptBr, const char* en);

} // namespace I18n

// Atalho para os pontos de chamada.
#define TR(ptBr, en) I18n::tr(ptBr, en)

#endif
