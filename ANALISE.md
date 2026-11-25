# Analise do projeto Aletheia (bg-remover)

> **Nota**: documento histórico da auditoria feita na v1.2.3 (2026-11-10).
> O projeto já vai na v1.4.3 — para o histórico atual de versões ver
> [CHANGELOG.md](CHANGELOG.md).

Versao 1.2.3. Aplicacao desktop (Qt6) + CLI para remocao de fundo de imagens localmente, usando ONNX Runtime com modelos U2-Net/ISNet.

## Modulos principais

- `CMakeLists.txt` — Build C++17, Qt6 (Core/Gui/Widgets/Network/Concurrent). Baixa e extrai ONNX Runtime 1.19.0 se ausente. Opcoes: INSTALLED, FETCH_ONNX_RUNTIME, BUNDLE_MODELS, WARNINGS. Empacotamento DEB (CPack) e ZIP macOS.
- `src/cli/main_cli.cpp` — Entrada CLI (QCommandLineParser): diretorio de entrada, saida, modelo, formato (`-f`) e tarefas em paralelo (`-j`).
- `src/core/backgroundremover.h/.cpp` — Sessao ONNX, preprocessamento (resize, normalizacao ImageNet), inferencia, construcao da mascara alfa e composicao RGBA. Deteta tamanho de entrada (320 U2-Net, 1024 ISNet) pelo shape do modelo ou nome do ficheiro. Cache estatico por caminho; SHA256 calculado uma vez e alteracoes detetadas por tamanho/data.
- `src/core/batchprocessor.h/.cpp` — Processa um diretorio inteiro: pool fixo de threads (1 por omissao, `-j N` no CLI), carrega imagens, remove fundo, salva saidas, reporta erros/fatais.
- `src/core/batchplan.h` — Logica compartilhada GUI+CLI: filtra imagens de entrada, gera nomes de saida sufixo `_rembg` sem sobrescrever, ignora saidas antigas quando entrada==saida. A extensao de saida segue o formato pedido.
- `src/core/processingoptions.h` — Struct de opcoes: modelo, limiar alfa, refinamento de bordas, cor/fundo desfocado/imagem, contorno adesivo, sombra, formato de saida, tarefas em paralelo.
- `src/core/imagepostprocessor.h/.cpp` — Pos-processamento: limiar de alpha, refinamento de bordas, composicao de fundo, auto-recorte, contorno adesivo, sombra projetada, chroma key, carregamento com EXIF, visualizacao da mascara.
- `src/core/modelmanager.h/.cpp` — Aba "Modelos" (GUI): catalogo, download, validacao SHA256 e remocao de modelos ONNX.
- `src/core/modelpathresolver.h/.cpp` — Resolve caminhos de modelos instalados (diretorio canonico + diretorios legados).
- `src/core/modelvalidator.h/.cpp` — Valida modelo instalado (tamanho + SHA256) sem carregar no ONNX.
- `src/core/savepath.h` — Garante extensao de saida coerente com o formato.
- `src/core/i18n.h` — I18n leve PT-BR/EN sem ferramentas externas (variavel de ambiente, QSettings, localidade do sistema). O idioma e detetado uma vez (cache estatico).
- `src/gui/mainwindow.h/.cpp` — Janela principal: abas, drag&drop, painel de opcoes, atualizacao, mascara manual, temas. O resto foi extraido: `previewinspector` (preview e zoom), `inferenceworker` (inferencia em thread + cache do modelo) e `batchtabcontroller` (aba do lote). O modelo e carregado na thread de trabalho.
- `src/gui/maskeditcanvas.h/.cpp` — Edicao manual da mascara (pincel restaurar/apagar, desfazer/refazer com limite de bytes, comparacao antes/depois).
- `src/gui/apptheme.h/.cpp` — Tema escuro/claro (Fusion + QSS), persistido em QSettings.
- `src/gui/helpwidget.h/.cpp`, `src/gui/collapsiblesection.h/.cpp` — Widgets de ajuda e secoes colapsaveis.
- `tests/` — 11 alvos de teste: post-processing, plano do lote, caminhos/sha de modelos, formatos de imagem, GUI (PT/EN), lote (CLI), inferencia e regressao visual.
- `assets/` — Icone (SVG/PNG/ICNS), `.desktop`, AppStream, fixtures de regressao visual.

## Fluxo geral

GUI: utilizador carrega imagem (abre/arrasta/cola) -> escolhe modelo e opcoes -> `removeBackground` arranca a thread de trabalho, que carrega o modelo ONNX e devolve o resultado composto (RGBA) -> preview -> opcional edicao manual em `MaskEditCanvas` -> salva/copia. Lote: `MainWindow::processBatch` constroi o plano com `BatchPlan::build` e processa imagem a imagem. CLI: argumentos -> `BatchProcessor::processDirectory` -> uma sessao ONNX por lote -> saidas com a extensao do formato pedido.

## Resolvido nesta serie (1.2.3)

1. **Crash ao mostrar a mascara alfa** — `visualizeMask` escrevia 4 bytes num buffer RGB888 de 3 (heap-buffer-overflow, SIGSEGV). Passou a escrever por bytes.
2. **A GUI abortava (SIGABRT) quando o modelo nao existia** — a excecao do ONNX escapava do slot. `obtainRemover` devolve agora `nullptr` com o motivo e o erro aparece no log/dialogo.
3. **CLI gravava JPEG dentro de ficheiros `.png`** (`-f jpg`) — o plano do lote nao recebia a extensao. Corrigido; o JPEG passa a ser composto sobre branco e o formato e validado antes de processar.
4. **Congelamento da janela ao carregar o modelo** — o modelo era carregado na thread da GUI (2,2 s com u2net.onnx). Passa a ser carregado na thread de trabalho.
5. **Use-after-free no cancelamento** — o `Ort::RunOptions` vivia na pilha e `requestCancel` usava-o fora do lock. Passa a ser um `shared_ptr`.
6. **Modelo re-hasheado por imagem** (176 MB por imagem no lote) — o SHA e calculado uma vez; o cache fica em 0 ms.
7. **Pre-carregamento do modelo inutil** — aquecer a cache estatica e destrui-la no fim. Passa a aquecer a cache da GUI.
8. **Uma thread por imagem no lote** — pool fixo de threads com `--jobs/-j` no CLI.
9. **Teste de regressao visual criava a baseline quando faltava** (escondia regressoes) e tinha um caminho absoluto fixo. Agora falha sem baseline (`VISUAL_REGRESSION_UPDATE=1` regenera) e os caminhos sao relativos ao source.
10. **Botao primario desativado mantinha a cor de destaque** (regra QSS por id ganha a `:disabled`).
11. **Codigo morto** — `batchtabcontroller`, `inferenceworker` e `previewinspector` (nunca usados) foram removidos.
12. **Lote: uma thread por imagem** — pool fixo de threads com `--jobs/-j` no CLI.
13. **Cada alvo de teste recompilava as fontes** — passou a existir a biblioteca partilhada `aletheia_core`/`aletheia_gui`.
14. **DEB sem `postinst`** — a lib do ONNX nao era registada no cache do linker (`ldconfig`).
15. **7 dos 9 modelos do catalogo sem SHA256** — todos tem agora hash publicado (baixados e verificados um a um).
16. **CI**: actions fixadas por SHA, cache do Qt, `contents: write` so no job de publicacao, versao do CMake comparada com a tag e `linuxdeploy` verificado pelo `.zsync`.
17. **Ordem de inicializacao dos membros** desalinhada (aviso `-Wreorder`) e membros/includes obsoletos no `MainWindow`.
18. **Preview do original reescalado a cada passo de zoom** (12 MP => 42 ms e 48 MB por evento): agora existe cache do preview escalado (9 ms).
19. **Remocao por cor (chroma key) na thread da GUI**: a inundacao de regiao demorava 1,8 s numa foto de 6 MP e congelava a janela. Passou a correr na thread de trabalho e a pilha passou a guardar indices (4 bytes) em vez de `QPoint` (8 bytes): 1841 ms -> 1261 ms.

## Pontos fracos

1. ~~`src/core/backgroundremover.cpp` — suporte apenas a provider CPU~~ **Resolvido**: CUDA/DirectML/CoreML com fallback CPU (env `ALETHEIA_PROVIDER`).
2. ~~`src/gui/mainwindow.cpp` grande~~ **Melhorado**: de 2060 para ~1915 linhas. Extraidos: `PreviewInspector`, `InferenceWorker`, `BatchTabController`, `UpdateChecker` e `OptionsPanel` (inspector com 3 seccoes). Restam os metodos de negocio (open/save/copy/lote) que estao acoplados ao estado da janela.
3. ~~`src/core/i18n.h` — traducao manual sem .ts/.qm~~ **Melhorado**: 280 pares TR() extraidos para `translations/*.ts` via `scripts/extract_tr.py`; CMake compila .qm com `ALETHEIA_BUILD_TRANSLATIONS=ON` (requer qt6-linguist-tools). TR() inline continua como fonte primaria.
4. ~~`CMakeLists.txt` — build falha sem rede~~ **Resolvido**: ordem de procura `vendor/onnxruntime/` → `ALETHEIA_ONNX_DIR` → cache/padrao → download automatico.
5. ~~`src/core/backgroundremover.cpp` — deteccao logit/probabilidade por amostragem linear~~ **Melhorado**: amostragem espaçada (strided) por toda a máscara em vez dos primeiros N pixels.
6. ~~Cancelamento durante carregamento do modelo~~ **Resolvido**: flag `cancelRequested` no `ModelCache`.
7. `src/core/modelmanager.cpp` — modelos de terceiros (aceite pelo utilizador: a aba de download chega).
8. `.github/workflows/release.yml` — `SHA256SUMS` nao assinados.
9. ~~Pós-processamento na thread da UI~~ **Resolvido**: save/copy/drag-out/auto-crop assincronos (`saveWatcher_`, `postProcessWatcher_`, `autoCropWatcher_`); `detectCornerColor` robusta.
10. ~~Formatos HEIC/AVIF sem aviso~~ **Resolvido**: `isFormatSupported()` + mensagem util na GUI e CLI; `batchplan.h` aceita as extensoes; testes em `test_imageformats.cpp`.
11. ~~Warnings de compilacao~~ **Resolvido**: 0 warnings com `-Wall -Wextra -Wpedantic`.
12. ~~ASAN/LSAN nos testes~~ **Resolvido**: leaks internos da `libonnxruntime.so` suprimidos via `asan.supp` + `LSAN_OPTIONS` no CTest; 11/11 passam em build ASAN. Nosso código está limpo (testes sem ONNX não têm leaks).
13. ~~Preferências não persistidas~~ **Resolvido**: `restoreSettings`/`saveSettings` agora guardam método de remoção (IA/Cor), preset, sticker, sombra, modo/tamanho do pincel, tolerância/contíguo/pixel-art da cor e cor alvo.
14. ~~Splitters não persistiam tamanho~~ **Resolvido**: `previewSplitter` e `contentSplitter` com `saveState`/`restoreState` via QSettings.
15. ~~CLI sem tratamento de Ctrl+C~~ **Resolvido**: handler SIGINT/SIGTERM com flag atómica; `BatchProcessor::setCancelFlag()` para entre imagens; exit code 130; imagens já processadas ficam guardadas.
16. ~~Atalhos documentados mas não implementados~~ **Resolvido**: `Ctrl+D` (remover fundo), `F1` (aba Ajuda), `Ctrl+C` (copiar) adicionados ao código.
17. ~~Aba Ajuda incompleta~~ **Resolvido**: tabela de atalhos completa (7 linhas), FAQ com 5 perguntas, secção de formatos suportados, 4 exemplos CLI, rodapé com link Releases.
18. ~~Sem menu Ficheiro / ficheiros recentes~~ **Resolvido**: menu Ficheiro (Abrir, Abrir recente max 10, Sair); título da janela mostra o ficheiro atual; aviso para imagens >50 MP; nome da imagem mostra tamanho do ficheiro.
19. ~~Provider do modelo invisível na UI~~ **Resolvido**: log mostra "Modelo: CPU · 320×320" apos a primeira inferencia.
20. ~~Lote sem resumo visual~~ **Resolvido**: diálogo de resumo no fim do lote (OK/erros).
21. ~~Atalhos F1/Ctrl+D/Ctrl+C documentados mas não implementados~~ **Resolvido** + testados em test_gui.cpp.

## Ideias de melhoria

1. ~~CUDA/DirectML~~ **Feito**.
2. ~~Extrair inspector de opcoes~~ **Feito** (`OptionsPanel`); `UpdateChecker` tambem extraido. MainWindow: 2060 → ~1915 linhas.
3. ~~i18n .ts/.qm~~ **Infraestrutura feita** (`translations/` + `scripts/extract_tr.py` + CMake `ALETHEIA_BUILD_TRANSLATIONS`); migracao completa dos TR() para `tr()` fica pendente.
4. ~~ONNX offline~~ **Feito** (`vendor/onnxruntime/` + `ALETHEIA_ONNX_DIR`).
5. ~~`aletheia_core` partilhada~~ **Feito**.
6. Fixar as actions do CI por SHA de commit e verificar o checksum do `linuxdeploy`.
7. ~~Publicar modelos nas releases proprias~~ **Descartado** pelo utilizador.
8. ~~HEIC/AVIF~~ **Feito** (detecao + mensagem util; leitura real depende do plugin Qt; `batchplan.h` aceita as extensoes).
