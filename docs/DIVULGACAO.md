# 📣 Kit de Divulgação — Aletheia v1.4.3

Textos prontos a copiar/colar para divulgar o Aletheia. Personaliza à vontade.
Versão atual: **v1.4.3** (2026-11-25). CI verde em Linux, macOS e Windows.

## 🟢 Modo simples — SEM tokens, SEM código (só cliques + copiar texto)

Não precisas de saber criar tokens nem programar. Faz isto:

### 1️⃣ Ligar a landing page (2 min)
1. Abre https://github.com/MarielCarlosFrias/Aletheia/settings/pages
2. Em **Source** escolhe **Deploy from a branch**
3. **Branch:** `gh-pages` · **Folder:** `/ (root)` → clica **Save**
4. Espera 1–2 minutos. A página fica disponível em:
   **https://marielcarlosfrias.github.io/Aletheia/**

### 2️⃣ Adicionar "topics" ao repositório (1 min)
1. Na página principal do repo, clica na ⚙️ ao lado de **About** (coluna da direita)
2. No campo **Topics** cola isto (separado por espaços):
   `qt6 qt onnxruntime background-removal image-processing cpp privacy offline-first rembg pixel-art desktop-app`
3. **Save changes**

### 3️⃣ Ativar Discussions e publicar o anúncio de lançamento (3 min)
1. Abre https://github.com/MarielCarlosFrias/Aletheia/settings → desce até **Features** → mete ✅ em **Discussions** → **Save**
2. Vai a https://github.com/MarielCarlosFrias/Aletheia/discussions → **New discussion** → categoria **Announcements**
3. **Título:** `🚀 Aletheia v1.4.3 — 100% offline AI background remover (Qt 6 + ONNX Runtime)`
4. **Corpo:** copia o texto da secção [📋 Anúncio pronto a colar](#-anúncio-pronto-a-colar-discussions) abaixo
5. **Start discussion**

### 4️⃣ Publicar nas redes sociais (copiar/colar, 1 post por comunidade)
Usa os textos prontos das secções mais abaixo deste ficheiro (Hacker News, Reddit, X, Mastodon, LinkedIn, Dev.to, Product Hunt). Em cada um, diz que és o autor do projeto ("I'm the developer…") — é regra e boa prática.

---

### 📋 Anúncio pronto a colar (Discussions)

```
🚀 Aletheia v1.4.3 is out!

Aletheia is a 100% offline, open-source (MIT) desktop app that removes image backgrounds
locally with AI — native C++17 + Qt 6 with ONNX Runtime (U²-Net, BRIA RMBG 1.4/2.0,
BiRefNet, ISNet, all SHA256-verified).

- 🔒 No cloud, no accounts, no telemetry — your photos never leave your device
- 🤖 One-click model catalog with SHA256 integrity verification
- 🎨 Chroma key / pixel-art mode (exact color, tolerance, contiguous flood-fill)
- ✂️ Sticker outline, drop shadow, auto-crop, edge refinement + live preview
- 🖌️ Manual mask brush with undo/redo
- 📁 Batch processing (GUI + aletheia_cli) with parallel workers
- 🪟🍎🐧 Pre-built binaries: Windows zip, macOS universal2, Linux AppImage/.deb

👉 Download: https://github.com/MarielCarlosFrias/Aletheia/releases/latest
👉 Landing page: https://marielcarlosfrias.github.io/Aletheia/

CI is green on Linux, macOS and Windows. Feedback, issues and PRs very welcome! ⭐
```

---

## 🔗 Links para usar em todo o lado

| O quê | URL |
|---|---|
| Repositório | https://github.com/MarielCarlosFrias/Aletheia |
| Última release (v1.4.3) | https://github.com/MarielCarlosFrias/Aletheia/releases/latest |
| Release v1.4.3 direta | https://github.com/MarielCarlosFrias/Aletheia/releases/tag/v1.4.3 |
| Windows (zip) | https://github.com/MarielCarlosFrias/Aletheia/releases/download/v1.4.3/aletheia_1.4.3_win64.zip |
| macOS (universal2) | https://github.com/MarielCarlosFrias/Aletheia/releases/download/v1.4.3/aletheia_1.4.3_macos_universal2.zip |
| Linux (AppImage) | https://github.com/MarielCarlosFrias/Aletheia/releases/download/v1.4.3/aletheia_1.4.3_amd64.AppImage |
| Debian/Ubuntu (.deb) | https://github.com/MarielCarlosFrias/Aletheia/releases/download/v1.4.3/aletheia_1.4.3_amd64.deb |
| Checksums | https://github.com/MarielCarlosFrias/Aletheia/releases/download/v1.4.3/SHA256SUMS |
| Landing page (ativar Pages primeiro — ver abaixo) | https://marielcarlosfrias.github.io/Aletheia/ |

**Assets para anexar:** `docs/images/aletheia-editor.png` (hero), `aletheia-models.png`,
`aletheia-batch.png`, `aletheia-help.png`; logo: `assets/aletheia.svg` / `assets/aletheia.png`.

**Ativar a landing page:** branch `gh-pages` já existe. No GitHub:
*Settings → Pages → Source: "Deploy from a branch" → branch `gh-pages` → `/ (root)` → Save*.
Depois fica disponível em https://marielcarlosfrias.github.io/Aletheia/

**Topics no repo** (1 clique): *Settings → General → Topics*: `qt6`, `onnxruntime`,
`background-removal`, `image-processing`, `cpp`, `desktop-app`, `privacy`, `onnx`.

---

## ⚡ One-liner / elevator pitch

**EN:** Aletheia is a free (MIT), 100% offline desktop app that removes image backgrounds
locally with AI — native C++17/Qt 6 + ONNX Runtime. No cloud, no account, no telemetry.
Windows, macOS (universal2) and Linux. Plus a full CLI for batch automation and a chroma-key
mode for pixel art / green screen.

**PT:** O Aletheia é uma aplicação desktop gratuita (MIT) e 100% offline que remove o fundo
de imagens localmente com IA — C++17/Qt 6 nativo + ONNX Runtime. Sem cloud, sem conta, sem
telemetria. Windows, macOS (universal2) e Linux. Inclui CLI completa para automação em lote e
modo chroma key para pixel art / ecrã verde.

---

## 🚀 Hacker News — "Show HN"

Título:
```
Show HN: Aletheia – 100% offline AI background remover in C++/Qt 6 (MIT)
```

Texto (comentar como autor, estilo HN: factual, sem marketing):
```
Hi HN — I built Aletheia, a desktop app that removes image backgrounds entirely on your
machine: native C++17 + Qt 6 GUI, ONNX Runtime inference (U²-Net, BRIA RMBG, BiRefNet, ISNet),
plus a chroma-key/pixel-art mode and a CLI (aletheia_cli) for batch jobs.

Why: every other tool I tried either uploads your photos or needs a GPU/account. Aletheia
runs on CPU, works offline, verifies model downloads with SHA256, and is MIT-licensed so you
can audit it. Releases ship AppImage/.deb/macOS universal2/Windows zip from CI.

Repo: https://github.com/MarielCarlosFrias/Aletheia
Downloads: https://github.com/MarielCarlosFrias/Aletheia/releases/latest

Happy to answer questions about the ONNX pipeline, the mask editing canvas, or the CI setup.
```

---

## 📱 Reddit

Regra de ouro: **diz sempre que és o autor** ("I'm the developer of...") e verifica as regras
de cada subreddit (muitos só permitem self-promo em threads semanais). Não faças spam —
um post por comunidade, espaçados.

### r/LocalLLaMA, r/opensource, r/linux, r/SideProject, r/gamedev, r/software

Título:
```
I made a 100% offline, open-source (MIT) background remover that runs locally on CPU — Aletheia v1.4.3 (Qt 6 + ONNX Runtime, Windows/macOS/Linux)
```

Corpo (EN):
```
Aletheia removes image backgrounds entirely on your device — no uploads, no API keys,
no telemetry. Native C++17/Qt 6 app with ONNX Runtime models (U²-Netp, U²-Net, BRIA RMBG
1.4/2.0, BiRefNet, ISNet) downloaded with SHA256 verification.

Highlights:
- GUI with split before/after slider, manual mask brush (restore/erase + undo), sticker
  outline, drop shadow, auto-crop, dark/light themes
- Chroma key / pixel-art mode (exact color, tolerance, contiguous flood-fill) for sprites
  and green screen
- Batch processing of whole folders, GUI + CLI (`aletheia_cli`), parallel jobs
- PT/EN/ES interface, GPU providers (CUDA/DML/CoreML) when available, CPU fallback
- Pre-built binaries: AppImage, .deb, macOS universal2, Windows zip (CI-built, SHA256SUMS)

Links: repo https://github.com/MarielCarlosFrias/Aletheia · downloads
https://github.com/MarielCarlosFrias/Aletheia/releases/latest

I'm the developer — feedback and issues very welcome!
```

### r/brasil (PT)

Título:
```
[Meu projeto] Aletheia v1.4.3 — removedor de fundo de imagens 100% offline e open source (Qt 6 + ONNX Runtime), com binários para Windows/macOS/Linux
```

Corpo (PT):
```
Criei o Aletheia: app desktop que remove o fundo de fotos localmente com IA, sem enviar
nada para a cloud — sem conta, sem telemetria. C++17/Qt 6 nativo, modelos ONNX (U²-Net,
BRIA RMBG, BiRefNet) com verificação SHA256, modo chroma key para pixel art, lote de pastas
via GUI ou CLI, interface em PT/EN/ES.

Licença MIT. Binários prontos (AppImage, .deb, macOS universal2, Windows zip):
https://github.com/MarielCarlosFrias/Aletheia/releases/latest

Sou o autor do projeto — feedback é muito bem-vindo!
```

---

## 🐦 X / Twitter — thread (EN)

1. `I built Aletheia — a 100% offline, open-source (MIT) background remover for images. Native C++17/Qt 6 + ONNX Runtime. Your photos never leave your device. 🧵`
2. `AI segmentation with U²-Net, BRIA RMBG, BiRefNet... all running locally on CPU (GPU providers optional). Models are downloaded with SHA256 verification. 🤖`
3. `Plus a chroma-key/pixel-art mode for sprites & green screen, manual mask brush with undo, sticker outlines, drop shadows, auto-crop. ✂️`
4. `Batch-process whole folders from the GUI or the CLI (aletheia_cli) — parallel workers, collision-free names. 📁`
5. `Free binaries for Windows, macOS (universal2) and Linux (AppImage/.deb): 👉 https://github.com/MarielCarlosFrias/Aletheia/releases/latest — star the repo if useful! ⭐`

**PT:** (versão curta, 1 post)
```
Criei o Aletheia v1.4.3: removedor de fundo de imagens 100% offline e open source (MIT),
com IA local (ONNX Runtime + Qt 6/C++17). Sem cloud, sem telemetria — as tuas fotos nunca
saem do teu PC. Binários para Windows/macOS/Linux + CLI para lote:
https://github.com/MarielCarlosFrias/Aletheia/releases/latest
```

Hashtags: `#opensource #privacy #onnxruntime #qt6 #cpp #selfhosted #rembg #pixelart`

---

## 🐘 Mastodon / Fediverse (EN, ≤500 chars)

```
New release: Aletheia v1.4.3 — a 100% offline, MIT-licensed background remover for images.
Native C++17/Qt 6 + ONNX Runtime (U²-Net, BRIA RMBG, BiRefNet), chroma-key/pixel-art mode,
batch CLI. No cloud, no accounts, no telemetry: your photos stay on your device.
Windows / macOS / Linux binaries 👉 https://github.com/MarielCarlosFrias/Aletheia/releases/latest
#OpenSource #Privacy #Qt6 #ONNXRuntime
```

(Anexa `docs/images/aletheia-editor.png`.)

---

## 💼 LinkedIn (EN)

```
I'm excited to share Aletheia v1.4.3 — an open-source (MIT) desktop application that removes
image backgrounds 100% locally using AI (ONNX Runtime + native C++17/Qt 6).

The problem I wanted to solve: most background-removal tools upload your photos to a cloud
service. Aletheia keeps everything on-device — no accounts, no API keys, no telemetry —
with SHA256-verified model downloads, a batch CLI for automation, and a chroma-key mode for
pixel art.

Available for Windows, macOS (Apple Silicon + Intel) and Linux.
🔗 https://github.com/MarielCarlosFrias/Aletheia
⬇️ https://github.com/MarielCarlosFrias/Aletheia/releases/latest

Feedback from the community would be greatly appreciated!
```

---

## 🛠️ Dev.to / Hashnode / Medium — artigo longo (EN)

Título: **"I built a 100% offline AI background remover in C++/Qt 6 — here's how (MIT)"**

Estrutura sugerida:
1. O problema (privacidade das ferramentas cloud)
2. Stack: C++17, Qt 6 Widgets, ONNX Runtime (providers CPU/CUDA/DML/CoreML), CMake, CPack
3. Pipeline: preprocessamento (resize + normalização ImageNet) → inferência → máscara alfa → pós-processamento (limiar, refinamento de bordas, chroma key, sticker, sombra)
4. Desafios técnicos: modelo na thread de trabalho (não bloquear a GUI), cache de SHA256 por modelo, pool fixo de threads no lote, escrita atómica, testes headless com offscreen
5. CI multiplataforma (Linux ASan, macOS universal2, Windows com aqtinstall) e releases automáticas com SHA256SUMS
6. Downloads e código fonte + call to action (estrela, issues, PRs)

Anexa as 4 screenshots de `docs/images/`.

---

## 🚀 Product Hunt (se quiseres lançar lá)

- **Tagline:** "100% offline AI background removal — your photos never leave your device"
- **Topics:** Artificial Intelligence, Open Source, Privacy, Design Tools, Developer Tools
- **Descrição:** Aletheia is a free, MIT-licensed desktop app (C++17/Qt 6 + ONNX Runtime) that
  removes image backgrounds locally with AI. No cloud, no accounts, no telemetry. Includes a
  batch CLI and a chroma-key mode for pixel art. Windows, macOS (universal2), Linux.
- **Galeria:** as 4 screenshots + logo
- **Comentário do maker (primeiro comentário):** contar a história em 2-3 frases e responder
  a todos os comentários nas primeiras horas.

---

## 📋 PR para awesome-lists (texto do PR)

Título: `Add Aletheia to the list`

Corpo:
```
Hi! I'd like to add my project to this list:

- **Aletheia** — https://github.com/MarielCarlosFrias/Aletheia
  100% offline, MIT-licensed background remover (C++17/Qt 6 + ONNX Runtime) with GUI, batch
  CLI and chroma-key/pixel-art mode. Pre-built binaries for Windows/macOS/Linux.

It fits the list because <razão específica da lista, ex.: "it's an open-source alternative to
cloud background-removal tools">.
```

Listas prováveis: `awesome-background-removal`, `awesome-onnx`, `awesome-qt`,
`awesome-open-source-apps`, `awesome-selfhosted` (secção desktop/tools).

---

## ✅ Checklist antes de publicar

- [ ] CI verde em `main` (badge do README mostra "passing")
- [ ] Release v1.4.3 com todos os binários + SHA256SUMS
- [ ] Landing page `gh-pages` ativa (Settings → Pages)
- [ ] Topics adicionados ao repo
- [ ] Screenshots atualizados em `docs/images/`
- [ ] Postar como autor revelado ("I'm the developer"), 1 vez por comunidade
- [ ] Guardar os links dos posts publicados para responder a comentários

## 🕐 Sugestão de ordem/horário (UTC)

1. **Hacker News** — 14:00–16:00 UTC (ter–qui tem melhor tração)
2. **Reddit r/LocalLLaMA / r/opensource** — 13:00–17:00 UTC
3. **X + Mastodon** — logo a seguir ao HN (aproveita o tráfego)
4. **Dev.to** — artigo no dia seguinte
5. **Product Hunt** — quando tiveres 2–3 dias para responder a comentários (lançar de manhã, hora PT)
6. **PRs para awesome-lists** — na semana seguinte
