# Analise do projeto Aletheia (bg-remover)

Versao 1.2.2. Aplicacao desktop (Qt6) + CLI para remocao de fundo de imagens localmente, usando ONNX Runtime com modelos U2-Net/ISNet.

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
- `src/gui/mainwindow.h/.cpp` — Janela principal: abas, drag&drop, processamento individual/lote, cache de modelo compartilhado (mutex), atualizacao, mascara manual, temas. O modelo e carregado na thread de trabalho.
- `src/gui/maskeditcanvas.h/.cpp` — Edicao manual da mascara (pincel restaurar/apagar, desfazer/refazer com limite de bytes, comparacao antes/depois).
- `src/gui/apptheme.h/.cpp` — Tema escuro/claro (Fusion + QSS), persistido em QSettings.
- `src/gui/helpwidget.h/.cpp`, `src/gui/collapsiblesection.h/.cpp` — Widgets de ajuda e secoes colapsaveis.
- `tests/` — 11 alvos de teste: post-processing, plano do lote, caminhos/sha de modelos, formatos de imagem, GUI (PT/EN), lote (CLI), inferencia e regressao visual.
- `assets/` — Icone (SVG/PNG/ICNS), `.desktop`, AppStream, fixtures de regressao visual.

## Fluxo geral

GUI: utilizador carrega imagem (abre/arrasta/cola) -> escolhe modelo e opcoes -> `removeBackground` arranca a thread de trabalho, que carrega o modelo ONNX e devolve o resultado composto (RGBA) -> preview -> opcional edicao manual em `MaskEditCanvas` -> salva/copia. Lote: `MainWindow::processBatch` constroi o plano com `BatchPlan::build` e processa imagem a imagem. CLI: argumentos -> `BatchProcessor::processDirectory` -> uma sessao ONNX por lote -> saidas com a extensao do formato pedido.

## Resolvido nesta serie (1.2.2 -> HEAD)

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

## Pontos fracos

1. `src/core/backgroundremover.cpp` — suporte apenas a provider CPU (sem CUDA/DirectML/TensorRT).
2. `src/gui/mainwindow.cpp` (2450 linhas) — classe muito grande e acoplada; o refactor em controllers foi abandonada.
3. `src/core/i18n.h` — traducao manual (duas strings por chamada), dificil de estender para mais idiomat; nao usa .ts/.qm do Qt.
4. `CMakeLists.txt` — build falha sem rede quando ONNX Runtime nao esta presente; dependencia externa baixada em tempo de build.
5. `src/core/backgroundremover.cpp` — deteccao de logit vs probabilidade e por amostragem (nao ha metadados no modelo).
6. `src/gui/mainwindow.cpp` — o cancelamento so tem efeito depois de o modelo estar carregado.
7. `src/core/modelmanager.cpp` — os modelos sao descarregados de um repositorio de terceiros (o projeto nao publica releases proprias).
8. `.github/workflows/release.yml` — os `SHA256SUMS` nao sao assinados (nem minisign/cosign) e o `linuxdeploy` so tem build "continuous" (verificado pelo `.zsync`, sem assinatura).
9. `src/gui/mainwindow.cpp` — o refactor em controllers continua por fazer (2450 linhas).

## Ideias de melhoria

1. Adicionar suporte a CUDA/DirectML via `OrtSessionOptions` em `src/core/backgroundremover.cpp`.
2. Quebrar `MainWindow` em classes menores (aba de lote, inspector de opcoes, worker de inferencia) e adicionar testes de GUI.
3. Substituir i18n manual por arquivos `.ts`/`.qm` do Qt Linguist.
4. Empacotar/vendar ONNX Runtime para build offline; permitir caminho custom via variavel de ambiente.
5. Criar a biblioteca `aletheia_core` partilhada para os alvos de teste.
6. Fixar as actions do CI por SHA de commit e verificar o checksum do `linuxdeploy`.
7. Publicar os modelos nas releases do proprio projeto, com SHA256 para todos.
8. Suportar mais formatos de entrada (HEIC/AVIF).
