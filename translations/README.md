# Traduções (i18n)

O programa usa hoje o macro `TR("pt", "en")` inline (em `src/core/i18n.h`),
que cobre PT-BR e EN sem ferramentas externas.

Os ficheiros `.ts` nesta pasta sao **gerados** a partir desses pares e servem
como ponte para o Qt Linguist:

```bash
# Extrair os pares TR() do codigo para .ts
python3 scripts/extract_tr.py src

# Editar no Qt Linguist (opcional)
linguist translations/aletheia_pt_BR.ts translations/aletheia_en.ts

# Compilar para .qm durante o build
cmake -S . -B build -DALETHEIA_BUILD_TRANSLATIONS=ON
cmake --build build
```

## Adicionar um novo idioma

1. Corra `python3 scripts/extract_tr.py src`.
2. Copie `aletheia_en.ts` para `aletheia_<lang>.ts` e traduza os `<translation>`.
3. Adicione o ficheiro a `TS_FILES` no `CMakeLists.txt`.
4. Estenda o enum `Lang` em `src/core/i18n.h` e o seletor na GUI.
