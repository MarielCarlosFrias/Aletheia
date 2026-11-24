# Arquitetura — Aletheia

```
┌─────────────┐     ┌──────────────┐     ┌─────────────────────────┐
│   GUI       │────▶│  aletheia_gui │────▶│  InferenceWorker        │
│  (Qt6)      │     │  (static lib) │     │  (QtConcurrent thread)  │
└─────────────┘     └──────────────┘     └──────────┬──────────────┘
       │                                            │
       │              ┌──────────────┐              ▼
       └─────────────▶│  aletheia_core│◀────────▶┌──────────────────┐
                      │  (static lib) │           │ BackgroundRemover │
┌─────────────┐      └──────┬───────┘           │ (ONNX Runtime)    │
│   CLI       │─────────────┘                   └──────────────────┘
│ (aletheia_  │────▶ BatchProcessor ──▶ ImagePostProcessor
│   cli)      │      (thread pool)      (efeitos, geometria, save)
└─────────────┘
```

## Fluxo de processamento

1. **Load**: `ImagePostProcessor::loadImageExif()` — lê com correção EXIF orientation
2. **Inference**: `BackgroundRemover::removeBackground()` — U²-Net/ISNet via ONNX Runtime
   - A máscara alfa crua é devolvida sem efeitos
3. **Mask effects** (preview e export): `ImagePostProcessor::applyMaskEffects()`
   - limiar alfa, refinamento de bordas, sticker, sombra, fundo
4. **Geometry** (só no export): `ImagePostProcessor::applyGeometry()`
   - crop, rotação, redimensionamento
5. **Save**: `ImagePostProcessor::saveImage()` — escrita atómica (QSaveFile),
   qualidade/compressão, metadados EXIF/ICC

## Separação preview vs export

| Fase | Preview | Export |
|---|---|---|
| Efeitos de máscara (sticker/sombra/limiar) | ✅ sim | ✅ sim |
| Geometria (crop/rotate/resize) | ❌ não | ✅ sim |

Isto evita aplicar a geometria duas vezes e permite o live preview:
mudar as opções de efeitos reaplica `applyMaskEffects()` sobre a máscara
base guardada, sem re-executar a inferência.

## Threads

- **GUI**: toda a UI na thread principal; inferência, flood-fill, save,
  auto-crop e lote correm em `QtConcurrent` com `QFutureWatcher`
- **CLI lote**: pool fixo de threads (`-j N`, limitado a `2×hardware_concurrency`)
- **Modelo**: cache partilhada (`BackgroundRemover::get()`) protegida por mutex;
  o cancelamento usa um vetor de `RunOptions` ativos

## i18n

- PT/EN: inline via `TR("pt", "en")` no código
- Outros idiomas (ES): dicionário JSON embutido (`translations/aletheia_<lang>.json`)
- Selección: `ALETHEIA_LANG` env → QSettings → locale do sistema
