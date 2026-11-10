# Analise do projeto Aletheia (bg-remover)

Versao 1.2.2. Aplicacao desktop (Qt6) + CLI para remocao de fundo de imagens localmente, usando ONNX Runtime com modelos U2-Net/ISNet.

## Modulos principais

- `CMakeLists.txt` — Build C++17, Qt6 (Core/Gui/Widgets/Network/Concurrent). Baixa e extrai ONNX Runtime 1.19.0 se ausente. Opcoes: INSTALLED, FETCH_ONNX_RUNTIME, BUNDLE_MODELS, WARNINGS.
- `src/cli/main_cli.cpp` — Entrada CLI (QCommandLineParser): diretorio de entrada, saida, modelo, formato.
- `src/core/backgroundremover.h/.cpp` — Sessao ONNX, preprocessamento (resize, normalizacao ImageNet), inferencia, construcao da mascara alfa e composicao RGBA. Deteta tamanho de entrada (320 U2-Net, 1024 ISNet) pelo shape do modelo ou nome do ficheiro.
- `src/core/batchprocessor.h/.cpp` — Processa um diretorio inteiro: carrega imagens, remove fundo, salva saidas, reporta erros/fatais.
- `src/core/batchplan.h` — Logica compartilhada GUI+CLI: filtra imagens de entrada, gera nomes de saida sufixo `_rembg` sem sobrescrever, ignora saidas antigas quando entrada==saida.
- `src/core/processingoptions.h` — Struct de opcoes: modelo, limiar alfa, refinamento de bordas, cor/fundo desfocado/imagem, contorno adesivo, sombra, formato de saida.
- `src/core/imagepostprocessor.h` — Pos-processamento: limiar de alpha, refinamento de bordas, composicao de fundo, auto-recorte, contorno adesivo, sombra projetada, chroma key, carregamento com EXIF.
- `src/core/modelmanager.h` — Aba "Modelos" (GUI): catalogo, download, validacao SHA256 e remocao de modelos ONNX.
- `src/core/modelpathresolver.h` — Resolve caminhos de modelos instalados (diretorio canonico + diretorios legados).
- `src/core/modelvalidator.h` — Valida modelo instalado (tamanho + SHA256) sem carregar no ONNX.
- `src/core/savepath.h` — Garante extensao de saida coerente com o formato.
- `src/core/i18n.h` — I18n leve PT-BR/EN sem ferramentas externas (variavel de ambiente, QSettings, localidade do sistema).
- `src/gui/mainwindow.h/.cpp` — Janela principal: abas, drag&drop, processamento individual/lote, cache de modelo compartilhado (mutex), atualizacao, mascara manual, temas.
- `src/gui/maskeditcanvas.h/.cpp` — Edicao manual da mascara (pincel restaurar/apagar, desfazer/refazer com limite de bytes, comparacao antes/depois).
- `src/gui/apptheme.h/.cpp` — Tema escuro/claro (Fusion + QSS), persistido em QSettings.
- `src/gui/helpwidget.h`, `src/gui/collapsiblesection.h` — Widgets de ajuda e secoes colapsaveis.
- `tests/` — Testes C++ para cada modulo core.
- `assets/` — Icone, .desktop.

## Fluxo geral

GUI: utilizador carrega imagem (abre/arrasta/colcha) -> escolhe modelo e opcoes -> `removeBackground` executa em thread (QtConcurrent) com `BackgroundRemover` -> resultado compuesto (RGBA) mostrado em preview -> opcional edicao manual em `MaskEditCanvas` -> salva/copia. Lote: `MainWindow::processBatch` constroi plano com `BatchPlan::build` e processa sequencialmente. CLI: argumentos -> `BatchProcessor::processDirectory` -> um `BackgroundRemover` por diretorio -> saidas numeradas.

## Pontos fracos

1. `src/core/backgroundremover.cpp` — suporte apenas a provider CPU (sem CUDA/DirectML/TensorRT).
2. `src/gui/mainwindow.cpp` (2258 linhas) — classe muito grande e acoplada; pouco coberto por testes de GUI.
3. `src/core/i18n.h` — traducao manual (duas strings por chamada), dificil de estender para mais idiomat; nao usa .ts/.qm do Qt.
4. `CMakeLists.txt` — build falha sem rede quando ONNX Runtime nao esta presente; dependencia externa baixada em tempo de build.
5. `src/core/backgroundremover.cpp` — mascara de saida assumida com valores 0..1 (clamp direto); sem detecao de modelos com saida logit.
6. `src/gui/mainwindow.cpp` — cancelamento real de inferencia em curso fraco (sao desativados botoes, mas a inferencia ONNX nao e interrompida).
7. `src/core/backgroundremover.h` — modelo recarregado a cada nova instancia; cache na GUI (`ModelCache`) e valido apenas durante a sessao.

## Ideias de melhoria

1. Adicionar suporte a CUDA/DirectML via `OrtSessionOptions` em `src/core/backgroundremover.cpp`.
2. Quebrar `MainWindow` em classes menores (aba de lote, inspector de opcoes, worker de inferencia) e adicionar testes de GUI.
3. Substituir i18n manual por arquivos `.ts`/`.qm` do Qt Linguist.
4. Empacotar/vendar ONNX Runtime para build offline; permitir caminho custom via variavel de ambiente.
5. Implementar cancelamento de inferencia com flag atomic + `Ort::RunOptions`.
6. Adortar detecao de tipo de saida do modelo (sigmoid vs logit) e normalizacao adequada.
7. Manter `BackgroundRemover` instanciado e reutilizado entre trocas de modelo na GUI.
8. Suportar mais formatos de entrada (HEIC/AVIF) e exportacao em lote com paralelismo controlado.
