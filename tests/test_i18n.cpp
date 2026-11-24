#include <QCoreApplication>
#include <QString>
#include <cstdlib>
#include <iostream>

#include "i18n.h"

// Teste da camada de traducao. Corre num processo proprio, por isso o
// idioma pode ser escolhido com ALETHEIA_LANG antes do primeiro TR().
// O CTest invoca este binario sem argumentos (EN por omissao em CI) e
// com "es" para validar o dicionario JSON.
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    const QString requested = argc > 1 ? QString::fromUtf8(argv[1]) : QString();
    if (!requested.isEmpty()) {
        qputenv("ALETHEIA_LANG", requested.toUtf8());
    }

    const I18n::Lang active = I18n::lang();
    const QString actualCode = I18n::code(active);
    // Sem argumento o idioma vem do ambiente/QSettings/localidade: nao ha
    // valor esperado, apenas um dos tres codigos validos.
    if (!requested.isEmpty()) {
        const QString expected = requested.toLower();
        if (!expected.startsWith(actualCode) && !actualCode.startsWith(expected)) {
            std::cerr << "FAIL: idioma detetado " << actualCode.toStdString()
                      << " != " << expected.toStdString() << std::endl;
            return 1;
        }
    }

    // Mapeamento de codigos/aliases.
    if (I18n::fromCode("pt_BR") != I18n::Lang::PtBr
        || I18n::fromCode("pt-br") != I18n::Lang::PtBr
        || I18n::fromCode("EN-us") != I18n::Lang::En
        || I18n::fromCode("es_ES") != I18n::Lang::Es
        || I18n::fromCode("xx") != I18n::Lang::En) {
        std::cerr << "FAIL: fromCode" << std::endl;
        return 1;
    }
    if (I18n::code(I18n::Lang::PtBr) != "pt" || I18n::code(I18n::Lang::En) != "en"
        || I18n::code(I18n::Lang::Es) != "es") {
        std::cerr << "FAIL: code" << std::endl;
        return 1;
    }

    const QString translated = TR("Guardar imagem", "Save image");
    if (translated.isEmpty()) {
        std::cerr << "FAIL: TR devolve vazio" << std::endl;
        return 1;
    }

    if (active == I18n::Lang::PtBr && translated != QStringLiteral("Guardar imagem")) {
        std::cerr << "FAIL: PT devia devolver o texto PT" << std::endl;
        return 1;
    }
    if (active == I18n::Lang::En && translated != QStringLiteral("Save image")) {
        std::cerr << "FAIL: EN devia devolver o texto EN" << std::endl;
        return 1;
    }
    if (active == I18n::Lang::Es) {
        if (!I18n::dictionaryAvailable(I18n::Lang::Es)) {
            std::cerr << "FAIL: dicionario ES em falta" << std::endl;
            return 1;
        }
        if (translated == QStringLiteral("Guardar imagem")
            || translated == QStringLiteral("Save image")) {
            std::cerr << "FAIL: ES sem traducao para 'Guardar imagem'" << std::endl;
            return 1;
        }
        // Chave inexistente cai para EN, nunca para a chave PT.
        if (I18n::tr("Chave que nao existe", "Missing key")
            != QStringLiteral("Missing key")) {
            std::cerr << "FAIL: fallback EN em falta" << std::endl;
            return 1;
        }
    }

    std::cout << "OK (" << I18n::code(active).toStdString() << ")" << std::endl;
    return 0;
}
