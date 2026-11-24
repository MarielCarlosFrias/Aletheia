# Troubleshooting — Aletheia

## Build

### ONNX Runtime não encontrado
```
ONNX Runtime nao encontrado em ...
```
Corra antes do cmake:
```bash
bash scripts/fetch-onnxruntime.sh
```
No Linux, a biblioteca `.so` já está em `vendor/onnxruntime/` (build offline automático).

### Qt6 não encontrado
```bash
# Debian/Ubuntu
sudo apt install qt6-base-dev qt6-tools-dev qt6-l10n-tools qt6-image-formats-plugins
# Fedora
sudo dnf install qt6-qtbase-devel qt6-qttools-devel
```

### WebP/HEIC/AVIF não funcionam
Instale o pacote de plugins:
```bash
sudo apt install qt6-image-formats-plugins   # Debian/Ubuntu
```
O programa avisa quais os formatos em falta.

## Runtime

### Modelo não encontrado
Coloque `u2netp.onnx` em:
- **Linux**: `~/.local/share/aletheia/models/`
- **macOS**: `~/Library/Application Support/aletheia/models/`
- **Windows**: `%LOCALAPPDATA%\aletheia\models\`

Ou indique o caminho: GUI → aba Modelos → descarregar, ou CLI `-m caminho.onnx`.

### GPU não ativa
Verifique se o ONNX Runtime tem o provider:
```bash
ALETHEIA_PROVIDER=cuda ./build-release/aletheia_cli --help  # vê no log o provider usado
```
Providers: `auto` (CUDA → DML → CoreML → CPU), `cpu`, `cuda`, `dml`, `coreml`.

### Imagem muito grande (>50 MP)
O programa avisa. Para imagens grandes, redimensione primeiro ou use o CLI com `-j 1`.

### Erro "Modelo corrompido"
Na aba Modelos, apague e volte a descarregar — o SHA256 é verificado automaticamente.

## Variáveis de ambiente

| Variável | Valores | Descrição |
|---|---|---|
| `ALETHEIA_LANG` | `pt`, `en`, `es` | Idioma da interface |
| `ALETHEIA_PROVIDER` | `auto`, `cpu`, `cuda`, `dml`, `coreml` | Execution provider ONNX |
| `ALETHEIA_THEME` | `dark`, `light` | Tema da GUI |
| `ALETHEIA_NO_UPDATE_CHECK` | (qualquer) | Desativa verificação de updates |
| `ALETHEIA_ONNX_DIR` | caminho | Diretório do ONNX Runtime (build) |
| `BG_REMOVER_LANG` / `BG_REMOVER_THEME` | (legacy) | Aliases antigos, ainda funcionam |
