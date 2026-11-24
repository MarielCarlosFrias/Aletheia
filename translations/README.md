# Traduções (i18n)

O programa usa o macro `TR("pt", "en")` inline (em `src/core/i18n.h`),
que cobre PT-BR e EN sem ferramentas externas. Para os restantes idiomas
(ex.: Espanhol) é carregado um dicionário JSON embutido nos binários:

- `translations/aletheia_es.json` — dicionário espanhol (carregado automaticamente quando selecionado).

Os ficheiros `.ts` nesta pasta são **gerados** a partir dos pares `TR()` e
servem como ponte para o Qt Linguist:

```bash
# Extrair os pares TR() do código para .ts
python3 scripts/extract_tr.py src

# Editar no Qt Linguist (opcional)
linguist translations/aletheia_pt_BR.ts translations/aletheia_en.ts

# Compilar para .qm durante o build
cmake -S . -B build -DALETHEIA_BUILD_TRANSLATIONS=ON
cmake --build build
```

O dicionário JSON é embutido automaticamente nos binários via Qt Resource
(`qt_add_resources` em `CMakeLists.txt`), pelo que não é necessário instalar
ficheiros de tradução ao lado do executável.

## Seleção de idioma

O idioma ativo vem, nesta ordem, de:

1. variável de ambiente `ALETHEIA_LANG=pt|en|es` (`BG_REMOVER_LANG` ainda
   funciona por compatibilidade)
2. `QSettings` (seletor de idioma na interface)
3. localidade do sistema (português → PT-BR, espanhol → ES, senão EN)

A mudança só é aplicada após reiniciar a aplicação.

## Adicionar um novo idioma

1. Corra `python3 scripts/extract_tr.py src`.
2. Crie `translations/aletheia_<lang>.json` com os pares
   `{"texto em PT": "texto traduzido"}`. As chaves são as strings em PT-BR
   usadas nas chamadas `TR()`.
3. O seletor de idioma na GUI mostra automaticamente o novo idioma quando
   `I18n::dictionaryAvailable()` devolve `true` para esse código.
4. Não é necessário editar `CMakeLists.txt` nem `src/core/i18n.h`: o dicionário
   JSON é apanhado pelo `GLOB` de `translations/*.json` no build e o enum
   `Lang` já inclui `Es` para o caminho genérico.