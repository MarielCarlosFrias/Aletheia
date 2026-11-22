# Changelog

Todas as alterações relevantes são documentadas aqui. O formato segue
[Keep a Changelog](https://keepachangelog.com/pt-BR/1.1.0/) e o projeto usa
[Versionamento Semântico](https://semver.org/lang/pt-BR/).

## [1.3.1] - 2026-11-22

### Corrigido

- **CI no Windows (job `windows`)**: falhava no link com `LNK1107` /
  `LNK2019` (`unresolved external OrtGetApiBase`) porque
  `scripts/fetch-onnxruntime.sh` copiava a `onnxruntime.dll` como
  `onnxruntime.lib` e a síntesis da import lib com `dumpbin.exe`/`lib.exe`
  nao e fiavel no Git Bash do runner (o MSYS converte `/exports` num
  caminho Windows). A DLL e a import library passam agora a vir do pacote
  NuGet oficial `Microsoft.ML.OnnxRuntime`, com validacao da assinatura
  COFF da `.lib` no script e no CMake.
- **CI**: o passo "Prepare ONNX Runtime" ja nao aborta com `pipefail`
  quando as ferramentas MSVC falham; os binarios Windows gerados no CI
  nao sao commitados (`.gitignore`).
- **CI**: ONNX Runtime por plataforma (vendor/npm/NuGet) e testes GUI
  headless (`QT_QPA_PLATFORM=offscreen`) — jobs linux, macOS e Windows
  todos verdes.

## [1.3.0] - 2026-11-21

### Adicionado

- **Suporte a GPU**: CUDA, DirectML e CoreML com fallback automático para
  CPU. Controlável pela variável de ambiente `ALETHEIA_PROVIDER=auto|cpu|cuda|dml|coreml`.
  O provider ativo aparece no log (`Modelo: CPU · 320×320`).
- **Menu Ficheiro** com Abrir, **Abrir recente** (últimos 10 ficheiros,
  persistidos em QSettings) e Sair.
- **Atalhos que o README documentava mas não existiam**: `Ctrl+D` (remover
  fundo), `F1` (aba Ajuda), `Ctrl+C` (copiar resultado).
- **Aba Ajuda renovada**: tabela de atalhos completa, FAQ com 5 perguntas,
  secção de formatos suportados (incl. HEIC/AVIF com plugin), 4 exemplos de
  CLI e link para Releases.
- **Avisos de UX**: imagem >50 MP gera aviso de memória; formatos sem plugin
  (HEIC/AVIF/WebP) muestran mensagem com o pacote a instalar.
- **Build offline**: o CMake procura `vendor/onnxruntime/` → `ALETHEIA_ONNX_DIR`
  → cache → download automático.
- **CLI com Ctrl+C limpo**: handler SIGINT/SIGTERM interrompe o lote entre
  imagens, guarda o que já foi processado e sai com código 130.
- **Persistência ampliada**: método de remoção (IA/Cor), preset, sticker,
  sombra, pincel, cor alvo, tolerâncias e divisores da janela.
- **Supressões ASAN** (`asan.supp`) para leaks internos da libonnxruntime;
  11/11 testes passam em build ASAN/UBSan.
- **Infraestrutura i18n**: 312 pares `TR()` extraídos para `translations/*.ts`
  via `scripts/extract_tr.py`; `.qm` compiláveis com `-DALETHEIA_BUILD_TRANSLATIONS=ON`.

### Melhorado

- **UI 100% assíncrona**: salvar, copiar, arrastar resultado e auto-recorte
  já não congelam a janela em imagens grandes (watchers dedicados).
- **Cancelamento durante o carregamento do modelo**: flag atómica no
  `ModelCache`, verificada pelo worker antes da inferência.
- **`detectCornerColor` robusta**: ignora pixels transparentes, amostra a
  borda inteira com quantização arredondada e tem fallback para grade global.
- **Detecção logit/probabilidade** com amostragem espaçada (strided) por toda
  a máscara.
- **MainWindow enxuto**: 2060 → ~2100 linhas com 5 controladores extraídos
  (`PreviewInspector`, `InferenceWorker`, `BatchTabController`,
  `UpdateChecker`, `OptionsPanel`).
- **Título da janela** mostra o ficheiro atual; barra mostra nome + tamanho.
- **Resumo visual no fim do lote** (diálogo OK/erros).
- **Lote aceita HEIC/AVIF/HEIF/JP2/JXL** (com mensagem útil se faltar plugin).

### Corrigido

- `-Wreorder` no `MainWindow` e todos os warnings de compilação eliminados
  (0 warnings com `-Wall -Wextra -Wpedantic`).

## [1.2.3] - 2026-11-10

### Corrigido

- A visualização da máscara alfa (`Ctrl+M`) escrevia 4 bytes num buffer de
  3 e corrompia a memória: a aplicação podia terminar com `SIGSEGV`.
- A janela deixava de funcionar quando o modelo ONNX não existia
  (utilizador novo, ou modelo apagado): a exceção do ONNX terminava o
  processo com `SIGABRT`. O erro passa a ser apresentado na interface.
- O CLI gravava JPEG dentro de ficheiros `.png` com `-f jpg`: a extensão
  dos ficheiros segue agora o formato pedido e o JPEG é composto sobre
  branco em vez de manter a cor original do fundo.

### Melhorado

- O modelo deixa de ser carregado na thread da interface: a janela já
  não congela durante os ~2,2 s de leitura dos 176 MB do U²-Net.
- O cancelamento da inferência já não pode usar memória libertada
  (`use-after-free` no `Ort::RunOptions`).
- O modelo deixou de ser re-calculado (SHA256) em cada imagem do lote.
- O lote usa um pool fixo de threads (antes criava uma thread por
  imagem) e ganhou a opção `-j/--jobs`.
- Os testes usam uma biblioteca partilhada com a aplicação, em vez de
  recompilar as fontes quatro vezes.
- O pacote DEB passou a ter `postinst` (a biblioteca do ONNX Runtime
  entra no cache do linker) e o teste de regressão visual deixou de
  passar quando a baseline falta.
- Todos os modelos do catálogo têm SHA256 publicado e a descarga é
  verificada criptograficamente.
- O CI passou a fixar as actions por SHA, a guardar cache do Qt e a
  verificar a versão do projeto contra a tag.

### Interface

- Novo ícone da Aletheia (SVG, PNG e `.icns` do bundle macOS) e o
  cabeçalho deixou de mostrar a letra "A" para mostrar o ícone.

### Refatoração

- A janela principal foi dividida: `PreviewInspector` (preview e zoom),
  `InferenceWorker` (inferência e cache do modelo) e
  `BatchTabController` (aba do lote).

## [1.2.2] - 2026-11-09

### Corrigido

- Workflows de CI e release passam a usar o runner `macos-15`, depois de o
  GitHub ter retirado as imagens `macos-14` em 2 de novembro de 2026.
- A release v1.2.2 repõe o pacote `aletheia_1.2.2_macos_universal2.zip`, que
  ficou em falta na v1.2.1.

## [1.2.1] - 2026-11-09

### Adicionado

- Novo nome e identidade **Aletheia**, com os binários `aletheia` e
  `aletheia_cli`, novos caminhos de instalação e compatibilidade com as
  configurações e modelos do BG Remover.
- Interface redesenhada: estado vazio com arrastar e soltar, inspector em
  accordion, aba **Lote** e rodapé com as informações da imagem.
- README em inglês e português, capturas atualizadas, man page, entrada
  desktop, metadados AppStream e ícone próprios.
- Estrutura do repositório reorganizada em `src/core`, `src/gui`, `src/cli`,
  `tests` e `assets`.
- Ícone de janela em PNG, ícone `.icns` do bundle macOS e ícone PNG nos
  pacotes Linux.

### Alterado

- CI e release atualizados para os novos nomes e para verificar o bundle
  `Aletheia.app` no macOS.
- Opções CMake e variáveis de ambiente antigas mantidas por compatibilidade.
- Capturas de ecrã regeneradas com o novo design e caminhos genéricos.

### Corrigido

- Nome do bundle e do executável macOS (`Aletheia.app` e
  `Contents/MacOS/Aletheia`), alinhado com o instalador e o CI.
- Caminhos pessoais removidos dos testes e das capturas de ecrã.

## [1.2.0] - 2026-11-03

### Adicionado

- Suporte a macOS com bundle universal2 (Intel + Apple Silicon), Qt e ONNX
  Runtime incluídos, CLI dentro do `.app` e ZIP de release.
- Testes de fumo da GUI em português e inglês, executados em modo offscreen.
- Opções `-o/--output` e `-m/--model` no CLI, mantendo os formatos posicionais
  anteriores.
- Templates de issue/PR, Dependabot, `CONTRIBUTING.md`, `SECURITY.md` e este
  changelog.
- Notas de release geradas automaticamente no GitHub.

### Alterado

- Empacotamento Linux/Windows/macOS e checksums de release num único
  `SHA256SUMS`.
- README, man page e metadados AppStream atualizados.

### Corrigido

- Vários problemas da auditoria 1.1.0: estado ocupado da GUI, resultados de
  imagens antigas, colisões de nomes no lote, validação de modelos, timeout da
  verificação de atualizações, undo/redo limitado por memória e extensão de
  salvamento coerente com o formato.

## [1.1.0] - 2026-10-31

### Adicionado

- Comparação antes/depois com divisor arrastável.
- Copiar resultado para a área de transferência (Ctrl+Shift+C).
- Exportação em PNG, JPEG e WebP; JPEG composto sobre o fundo escolhido.
- Escolha do formato no processamento em lote.
- Desfazer/refazer multi-nível com limite de memória.
- Aba **Modelos** com catálogo, download, verificação SHA256 e remoção.
- Verificação de novas versões com opt-out.
- Arrastar e soltar imagens ou pastas.
- Tema claro/escuro e preferências persistentes.
- Zoom de 50% a 400% centrado no cursor.

## [1.0.5] - 2026-10-31

### Corrigido

- Nome do pacote DEB/ZIP alinhado com a versão do CMake.
- Verificação do ZIP Windows independente do nome da versão.

## [1.0.4] - 2026-10-31

### Alterado

- AppImage gerado no `ubuntu-latest` (Qt 6.2 do Ubuntu 22.04 não configurava).

## [1.0.3] - 2026-10-31

### Corrigido

- `Categories` do `.desktop` com valor reconhecido (`2DGraphics`).

## [1.0.2] - 2026-10-31

### Adicionado

- Deteção do tamanho de entrada do modelo (ISNet 1024).
- Publicação da release mesmo quando um job isolado falha.
- Downloads do CI com novas tentativas.

### Corrigido

- Orientação EXIF ao abrir imagens.
- Lista de modelos e bloqueio da sessão ONNX durante o processamento.
- Validação de tamanho das descargas e confirmação de remoção de modelos.
- Filtros alfa e composição de imagem mais rápidos.

## [1.0.1] - 2026-10-14

### Adicionado

- Interface em português e inglês, com seletor de idioma.
- Screenshots reais no README.
- AppImage na release e RPATH portátil (`$ORIGIN`).

## [1.0.0] - 2026-10-14

### Adicionado

- Primeira versão pública: GUI Qt, CLI, remoção de fundo com U²-Netp/U²-Net,
  processamento em lote e pacotes Linux/Windows.
