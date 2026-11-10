#include "mainwindow.h"
#include "i18n.h"
#include "maskeditcanvas.h"
#include "previewinspector.h"
#include "inferenceworker.h"
#include "batchtabcontroller.h"
#include "helpwidget.h"
#include "collapsiblesection.h"
#include "imagepostprocessor.h"
#include "modelmanager.h"
#include "modelpathresolver.h"
#include <QCloseEvent>
#include <QFile>
#include <QFileDialog>
#include <QIcon>
#include <QImageReader>
#include <QImageWriter>
#include <QMessageBox>
#include <QScrollArea>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QProgressBar>
#include <QPlainTextEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFrame>
#include <QSplitter>
#include <QApplication>
#include <QByteArray>
#include <QPixmap>
#include <QSizePolicy>
#include <QDir>
#include <QFileInfo>
#include <QTimer>
#include <QColorDialog>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QCloseEvent>
#include <QKeySequence>
#include <QMimeData>
#include <QDrag>
#include <QUrl>
#include <QClipboard>
#include <QDesktopServices>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSettings>
#include <QShortcut>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QStackedWidget>
#include <QMenu>
#include <QMenuBar>
#include <QtConcurrent/QtConcurrent>
#include <QWheelEvent>
#include <algorithm>
#include <iostream>

#include "apptheme.h"
#include "savepath.h"

static constexpr float kAlphaThreshold = 30.0f;

QColor MainWindow::currentBackgroundColor() const {
    int idx = bgColorCombo_->currentIndex();
    if (idx == 0) return QColor();
    if (idx == 1) return Qt::white;
    if (idx == 2) return Qt::black;
    if (idx == 3) return customBgColor_;
    return QColor();
}

ProcessingOptions MainWindow::currentProcessingOptions() const
{
    ProcessingOptions options;
    options.modelPath = modelPathEdit_->text();
    options.cleanNoise = noiseCheckBox_->isChecked();
    options.alphaThreshold = noiseThresholdSpin_->value();
    options.refineEdges = refineCheckBox_->isChecked();
    options.edgeRefinementIntensity = 0.5f;
    options.backgroundColor = currentBackgroundColor();

    const int bgIdx = bgColorCombo_->currentIndex();
    if (bgIdx == 4) {
        options.backgroundStyle = ProcessingOptions::BgBlurredOriginal;
    } else if (bgIdx == 5 && !customBgImagePath_.isEmpty()) {
        options.backgroundStyle = ProcessingOptions::BgCustomImage;
        options.backgroundImagePath = customBgImagePath_;
    } else if (options.backgroundColor.isValid()) {
        options.backgroundStyle = ProcessingOptions::BgSolidColor;
    } else {
        options.backgroundStyle = ProcessingOptions::BgTransparent;
    }

    options.stickerOutline = stickerCheckBox_->isChecked();
    options.stickerWidth = 6;
    options.stickerColor = Qt::white;

    options.dropShadow = shadowCheckBox_->isChecked();
    options.shadowBlur = 10;
    options.shadowOffsetX = 4;
    options.shadowOffsetY = 6;
    options.shadowOpacity = 0.5f;

    return options;
}

MainWindow::MainWindow(QWidget* parent, const QString& modelPath)
    : QMainWindow(parent),
      workspaceStack_(new QStackedWidget),
      dropZoneWidget_(nullptr),
      previewContainer_(nullptr),
      imageStatusLabel_(new QLabel(TR("Nenhuma imagem selecionada",
                                      "No image selected"))),
      originalLabel_(new QLabel(TR("Nenhuma imagem", "No image"))),
      imageNameLabel_(new QLabel(TR("Nenhuma imagem selecionada",
                                    "No image selected"))),
      zoomLabel_(new QLabel("100%")),
      originalScroll_(new QScrollArea),
      resultScroll_(new QScrollArea),
      modelCombo_(new QComboBox),
      modelPathEdit_(new QLineEdit),
      btnOpen_(new QPushButton(TR("Abrir imagem", "Open image"))),
      btnThemeToggle_(new QPushButton),
      btnRemove_(new QPushButton(TR("Remover fundo", "Remove background"))),
      btnSave_(new QPushButton(TR("Salvar imagem", "Save image"))),
      btnCopy_(new QPushButton),
      btnCopyTransparent_(new QPushButton),
      actCopyTransparent_(new QAction(this)),
      btnLoadModel_(new QPushButton(TR("Buscar...", "Browse..."))),
      btnUndo_(new QPushButton(TR("Desfazer", "Undo"))),
      btnRedo_(new QPushButton(TR("Refazer", "Redo"))),
      btnAutoCrop_(new QPushButton(TR("Auto-Recorte", "Auto-Crop"))),
      btnDragOut_(new QPushButton(TR("Arrastar resultado", "Drag result"))),
      presetCombo_(new QComboBox),
      bgColorCombo_(new QComboBox),
      brushModeCombo_(new QComboBox),
      brushRadiusSpin_(new QSpinBox),
      noiseThresholdSpin_(new QSpinBox),
      refineCheckBox_(new QCheckBox(TR("Refinar bordas", "Refine edges"))),
      noiseCheckBox_(new QCheckBox(TR("Limpar ruído", "Clean noise"))),
      stickerCheckBox_(new QCheckBox(TR("Contorno (Sticker)", "Sticker outline"))),
      shadowCheckBox_(new QCheckBox(TR("Sombra suave", "Drop shadow"))),
      removalMethodCombo_(new QComboBox),
      aiRemovalWidget_(new QWidget),
      colorRemovalWidget_(new QWidget),
      btnKeyColorSample_(new QPushButton),
      btnAutoDetectKeyColor_(new QPushButton(TR("Detectar fundo", "Detect background"))),
      colorToleranceSpin_(new QSpinBox),
      colorContiguousCheckBox_(new QCheckBox(TR("Apenas conectado (Varinha)", "Connected only (Wand)"))),
      colorPixelArtCheckBox_(new QCheckBox(TR("Modo Pixel Art (bordas nítidas)", "Pixel Art mode (sharp edges)"))),
      btnRemoveByColor_(new QPushButton(TR("Remover cor de fundo", "Remove background color"))),
      keyColor_(QColor(255, 0, 255)),
      saveFormatCombo_(new QComboBox),
      updateBanner_(new QLabel),
      progressBar_(new QProgressBar),
      logEdit_(new QPlainTextEdit),
      maskEditCanvas_(new MaskEditCanvas(this)),
      zoomDebounce_(new QTimer(this)),
      customBgColor_(QColor()),
      // Ordem igual a da declaracao na classe (evita -Wreorder).
      preview_(new PreviewInspector(this)),
      batch_(new BatchTabController(this)),
      inference_(new InferenceWorker(this)),
      modelCache_(inference_->cache())
{
    originalLabel_->setAlignment(Qt::AlignCenter);

    originalScroll_->setWidget(originalLabel_);
    originalScroll_->setWidgetResizable(false);
    originalScroll_->setAlignment(Qt::AlignCenter);

    maskEditCanvas_->setMinimumSize(320, 240);
    maskEditCanvas_->setSizePolicy(
        QSizePolicy::Expanding, QSizePolicy::Expanding);
    resultScroll_->setWidget(maskEditCanvas_);
    resultScroll_->setWidgetResizable(false);
    resultScroll_->setAlignment(Qt::AlignCenter);

    // A lista de modelos inclui os instalados na aba "Modelos";
    // a selecao preferida (CLI ou QSettings) e aplicada no fim do
    // construtor, depois de restaurar as preferencias.
    populateModelCombo();
    modelPathEdit_->setMinimumWidth(0);

    presetCombo_->addItem(TR("Personalizado", "Custom"));
    presetCombo_->addItem(TR("Padrão (Transparente)", "Standard (Transparent)"));
    presetCombo_->addItem(TR("E-commerce (Branco + Sombra)", "E-commerce (White + Shadow)"));
    presetCombo_->addItem(TR("Sticker (Contorno Branco)", "Sticker (White Outline)"));
    presetCombo_->addItem(TR("Retrato (Fundo Desfocado)", "Portrait (Blurred Background)"));
    presetCombo_->setCurrentIndex(0);

    bgColorCombo_->addItem(TR("Transparente", "Transparent"), QVariant());
    bgColorCombo_->addItem(
        TR("Branco", "White"), QVariant::fromValue(QColor(Qt::white)));
    bgColorCombo_->addItem(
        TR("Preto", "Black"), QVariant::fromValue(QColor(Qt::black)));
    bgColorCombo_->addItem(TR("Personalizado...", "Custom..."), QVariant());
    bgColorCombo_->addItem(TR("Desfoque original (Retrato)", "Original blur (Portrait)"), QVariant());
    bgColorCombo_->addItem(TR("Imagem de fundo...", "Background image..."), QVariant());
    bgColorCombo_->setCurrentIndex(0);
    bgColorCombo_->setMaximumWidth(200);

    brushModeCombo_->addItem(TR("Restaurar", "Restore"));
    brushModeCombo_->addItem(TR("Apagar", "Erase"));
    brushModeCombo_->setCurrentIndex(0);

    brushRadiusSpin_->setRange(1, 200);
    brushRadiusSpin_->setValue(10);
    brushRadiusSpin_->setSuffix(" px");

    maskEditCanvas_->setMode(MaskEditCanvas::Restore);
    maskEditCanvas_->setBrushRadius(brushRadiusSpin_->value());

    btnUndo_->setEnabled(false);
    btnAutoCrop_->setEnabled(false);
    btnDragOut_->setEnabled(false);
    btnAutoCrop_->setToolTip(TR("Recorta o espaço transparente ao redor do objeto",
                                "Crops transparent empty space around the object"));
    btnDragOut_->setToolTip(TR("Clique e arraste para soltar o PNG no Photoshop, Telegram ou pasta",
                               "Click and drag to drop the PNG into Photoshop, Telegram or folder"));

    progressBar_->setRange(0, 100);
    progressBar_->setValue(0);
    progressBar_->setVisible(false);

    logEdit_->setReadOnly(true);
    logEdit_->setMaximumHeight(150);
    logEdit_->setVisible(false);
    logEdit_->setPlaceholderText(TR("Log do processamento...",
                                    "Processing log..."));

    // Nomes estaveis para acessibilidade e para o teste GUI (offscreen)
    // encontrar os controlos sem depender da ordem do layout.
    originalLabel_->setObjectName("originalPreview");
    modelCombo_->setObjectName("modelCombo");
    modelPathEdit_->setObjectName("modelPath");
    saveFormatCombo_->setObjectName("saveFormatCombo");
    presetCombo_->setObjectName("presetCombo");
    bgColorCombo_->setObjectName("bgColorCombo");
    btnAutoCrop_->setObjectName("autoCropButton");
    btnDragOut_->setObjectName("dragOutButton");
    stickerCheckBox_->setObjectName("stickerCheckBox");
    shadowCheckBox_->setObjectName("shadowCheckBox");
    refineCheckBox_->setObjectName("refineEdges");
    noiseCheckBox_->setObjectName("cleanNoise");
    noiseThresholdSpin_->setObjectName("noiseThreshold");
    brushModeCombo_->setObjectName("brushMode");
    brushRadiusSpin_->setObjectName("brushRadius");
    progressBar_->setObjectName("progressBar");
    logEdit_->setObjectName("logEdit");
    maskEditCanvas_->setObjectName("resultCanvas");
    // Acessibilidade: o canvas e os controlos do pincel precisam de
    // nome/descricao para leitores de ecra.
    maskEditCanvas_->setAccessibleName(
        TR("Resultado (máscara editável)",
           "Result (editable mask)"));
    maskEditCanvas_->setAccessibleDescription(
        TR("Clique para restaurar ou apagar areas da máscara.",
           "Click to restore or erase areas of the mask."));
    // O inspector recebe os widgets ja construidos.
    preview_->init(originalLabel_, zoomLabel_, originalScroll_, resultScroll_,
                   maskEditCanvas_, workspaceStack_, imageStatusLabel_);
    brushModeCombo_->setAccessibleName(TR("Modo do pincel", "Brush mode"));
    brushRadiusSpin_->setAccessibleName(TR("Raio do pincel", "Brush radius"));
    btnUndo_->setObjectName("undoButton");
    btnRedo_->setObjectName("redoButton");

    // Reescalar os previews em cada evento de resize e caro: acumula
    // 40 ms e recalcula uma unica vez quando o utilizador para de
    // arrastar a janela.
    zoomDebounce_->setSingleShot(true);
    zoomDebounce_->setInterval(40);
    connect(zoomDebounce_, &QTimer::timeout,
            this,          &MainWindow::updatePreviewZoom);

    auto* header = new QFrame;
    header->setObjectName("titleBar");
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(18, 0, 18, 0);
    headerLayout->setSpacing(10);
    auto* brandMark = new QLabel;
    brandMark->setObjectName("brandMark");
    brandMark->setFixedSize(24, 24);
    // Renderiza o icone em 2x e reduz para 24 px logicos: fica nitido
    // em ecras HiDPI e continua correto em ecras 1x.
    QPixmap brandPixmap =
        QIcon(":/assets/aletheia.svg").pixmap(QSize(48, 48));
    brandPixmap.setDevicePixelRatio(2.0);
    brandMark->setPixmap(brandPixmap);
    auto* title = new QLabel("Aletheia");
    title->setObjectName("appTitle");
    auto* subtitle = new QLabel(TR("Editor de fundo local",
                                   "Local background editor"));
    subtitle->setObjectName("appSubtitle");
    headerLayout->addWidget(brandMark);
    headerLayout->addWidget(title);
    headerLayout->addWidget(subtitle);
    headerLayout->addStretch();
    langCombo_ = new QComboBox;
    langCombo_->setObjectName("langCombo");
    // Nomes de idioma sempre na propria lingua (pratica padrao).
    langCombo_->addItem("\U0001F310 PT-BR", "pt");
    langCombo_->addItem("\U0001F310 English", "en");
    langCombo_->setToolTip(
        TR("Idioma (aplicado ao reiniciar o aplicativo)",
           "Language (applies after restarting the app)"));
    langCombo_->setCurrentIndex(
        I18n::lang() == I18n::Lang::En ? 1 : 0);
    headerLayout->addWidget(langCombo_);
    btnThemeToggle_->setObjectName("themeToggle");
    btnThemeToggle_->setToolTip(TR("Alternar tema claro/escuro",
                                   "Toggle light/dark theme"));
    btnThemeToggle_->setText(AppTheme::isDark()
        ? TR("\u2600 Tema claro", "\u2600 Light theme")
        : TR("\u263E Tema escuro", "\u263E Dark theme"));
    headerLayout->addWidget(btnThemeToggle_);
    btnOpen_->setObjectName("openButton");
    headerLayout->addWidget(btnOpen_);

    connect(langCombo_, QOverload<int>::of(
                &QComboBox::currentIndexChanged),
            this, [this](int idx) {
        QSettings settings("aletheia", "aletheia");
        settings.setValue("lang", idx == 1 ? "en" : "pt");
        QMessageBox::information(this, TR("Idioma", "Language"),
            TR("O idioma sera aplicado ao reiniciar o Aletheia.",
               "The language will be applied after restarting Aletheia."));
    });

    auto* originalPanel = new QFrame;
    originalPanel->setObjectName("previewPanel");
    auto* originalLayout = new QVBoxLayout(originalPanel);
    originalLayout->setContentsMargins(0, 0, 0, 0);
    auto* originalTitle = new QLabel("Original");
    originalTitle->setObjectName("previewTitle");
    originalLayout->addWidget(originalTitle);
    originalLayout->addWidget(originalScroll_);

    auto* resultPanel = new QFrame;
    resultPanel->setObjectName("previewPanel");
    auto* resultLayout = new QVBoxLayout(resultPanel);
    resultLayout->setContentsMargins(0, 0, 0, 0);
    auto* resultTitle = new QLabel(TR("Resultado", "Result"));
    resultTitle->setObjectName("previewTitle");
    resultLayout->addWidget(resultTitle);
    resultLayout->addWidget(resultScroll_);

    auto* previewSplitter = new QSplitter(Qt::Horizontal);
    previewSplitter->setChildrenCollapsible(false);
    previewSplitter->setHandleWidth(12);
    previewSplitter->addWidget(originalPanel);
    previewSplitter->addWidget(resultPanel);
    previewSplitter->setStretchFactor(0, 1);
    previewSplitter->setStretchFactor(1, 1);
    previewContainer_ = previewSplitter;

    dropZoneWidget_ = createEmptyStateWidget();

    workspaceStack_->setObjectName("workspaceStack");
    workspaceStack_->addWidget(dropZoneWidget_);
    workspaceStack_->addWidget(previewContainer_);

    auto* workspace = new QFrame;
    workspace->setObjectName("workspace");
    auto* workspaceLayout = new QVBoxLayout(workspace);
    workspaceLayout->setContentsMargins(0, 0, 0, 0);
    workspaceLayout->setSpacing(6);
    auto* workspaceToolbar = new QHBoxLayout;
    workspaceToolbar->setContentsMargins(0, 0, 0, 0);
    imageNameLabel_->setObjectName("imageName");
    workspaceToolbar->addWidget(imageNameLabel_);
    workspaceToolbar->addStretch();
    auto* btnCompare = new QPushButton(TR("Comparar", "Compare"));
    btnCompare->setObjectName("viewModeActive");
    btnCompare->setCheckable(true);
    btnCompare->setToolTip(
        TR("Arraste o divisor para ver o antes/depois (desativa o pincel)",
           "Drag the divider to see before/after (disables the brush)"));
    connect(btnCompare, &QPushButton::toggled, this,
            [this](bool on) {
        maskEditCanvas_->setCompareEnabled(on);
        maskEditCanvas_->setCompareImage(preview_->originalImage());
        logEdit_->appendPlainText(on
            ? "[INFO] " + TR("Comparacao antes/depois ativada.",
                             "Before/after comparison enabled.")
            : "[INFO] " + TR("Comparacao desativada.",
                             "Comparison disabled."));
    });
    workspaceToolbar->addWidget(btnCompare);
    auto* editHint = new QLabel(TR("Edite a máscara no resultado",
                                   "Edit the mask on the result"));
    editHint->setObjectName("workspaceHint");
    workspaceToolbar->addWidget(editHint);
    auto* zoomOutButton = new QPushButton("−");
    zoomOutButton->setObjectName("zoomButton");
    zoomOutButton->setToolTip(
        TR("Diminuir zoom do resultado (Ctrl + roda do mouse)",
           "Zoom out result (Ctrl + mouse wheel)"));
    auto* zoomInButton = new QPushButton("+");
    zoomInButton->setObjectName("zoomButton");
    zoomInButton->setToolTip(
        TR("Aumentar zoom do resultado (Ctrl + roda do mouse)",
           "Zoom in result (Ctrl + mouse wheel)"));
    auto* fitButton = new QPushButton(TR("Ajustar", "Fit"));
    fitButton->setObjectName("fitButton");
    fitButton->setToolTip(
        TR("Ajustar resultado à área de trabalho (Ctrl+0)",
           "Fit result to the workspace (Ctrl+0)"));
    zoomLabel_->setObjectName("zoomLabel");
    workspaceToolbar->addSpacing(4);
    workspaceToolbar->addWidget(zoomOutButton);
    workspaceToolbar->addWidget(zoomLabel_);
    workspaceToolbar->addWidget(zoomInButton);
    workspaceToolbar->addWidget(fitButton);
    workspaceLayout->addLayout(workspaceToolbar);
    workspaceLayout->addWidget(workspaceStack_, 1);

    auto* editorFooter = new QFrame;
    editorFooter->setObjectName("editorFooter");
    auto* editorFooterLayout = new QHBoxLayout(editorFooter);
    editorFooterLayout->setContentsMargins(14, 8, 14, 8);
    editorFooterLayout->setSpacing(10);
    imageStatusLabel_->setObjectName("imageStatusLabel");
    editorFooterLayout->addWidget(imageStatusLabel_);
    editorFooterLayout->addStretch(1);
    btnCopy_->setObjectName("copyButton");
    btnCopy_->setToolTip(
        TR("Copiar resultado para a área de transferência (Ctrl+C ou Ctrl+Shift+C)",
           "Copy result to clipboard (Ctrl+C or Ctrl+Shift+C)"));
    btnCopy_->setText(TR("Copiar", "Copy"));
    editorFooterLayout->addWidget(btnCopy_);
    btnCopyTransparent_->setObjectName("copyTransparentButton");
    btnCopyTransparent_->setToolTip(
        TR("Copiar PNG transparente para a área de transferência",
           "Copy transparent PNG to clipboard"));
    btnCopyTransparent_->setText(TR("Copiar PNG transparente", "Copy transparent PNG"));
    editorFooterLayout->addWidget(btnCopyTransparent_);
    actCopyTransparent_->setText(TR("Copiar PNG transparente", "Copy transparent PNG"));
    actCopyTransparent_->setToolTip(btnCopyTransparent_->toolTip());
    connect(actCopyTransparent_, &QAction::triggered, this, &MainWindow::copyTransparentResult);
    btnSave_->setObjectName("saveButton");
    editorFooterLayout->addWidget(btnSave_);
    workspaceLayout->addWidget(editorFooter);

    auto* inspector = new QFrame;
    inspector->setObjectName("inspector");
    auto* inspectorLayout = new QVBoxLayout(inspector);
    inspectorLayout->setContentsMargins(12, 12, 12, 12);
    inspectorLayout->setSpacing(4);

    // O inspector passa a ter seccoes colapsaveis: a principal (acao
    // de remover) comeca aberta; as restantes comecam fechadas para o
    // painel nao parecer uma lista infinita de opcoes.
    auto* secRemove = new CollapsibleSection(
        TR("Remover fundo", "Remove background"), true, this);
    auto* secRemoveLayout = secRemove->contentLayout();

    removalMethodCombo_->addItem(TR("🧠 IA (Rede neural)", "🧠 AI (Neural network)"));
    removalMethodCombo_->addItem(TR("🎨 Cor / Chroma Key (Pixel Art)", "🎨 Color / Chroma Key (Pixel Art)"));
    secRemoveLayout->addWidget(removalMethodCombo_);

    // Modo 1: IA
    auto* aiLayout = new QVBoxLayout(aiRemovalWidget_);
    aiLayout->setContentsMargins(0, 0, 0, 0);
    aiLayout->setSpacing(7);
    auto* modelForm = new QFormLayout;
    modelForm->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    modelForm->setRowWrapPolicy(QFormLayout::WrapAllRows);
    modelForm->addRow(TR("Modelo", "Model"), modelCombo_);
    auto* modelPathLayout = new QHBoxLayout;
    modelPathLayout->addWidget(modelPathEdit_);
    modelPathLayout->addWidget(btnLoadModel_);
    modelForm->addRow(TR("Arquivo", "File"), modelPathLayout);
    aiLayout->addLayout(modelForm);
    btnRemove_->setObjectName("primaryAction");
    btnRemove_->setMinimumHeight(38);
    aiLayout->addWidget(btnRemove_);
    secRemoveLayout->addWidget(aiRemovalWidget_);

    // Modo 2: Cor / Chroma Key (Pixel Art)
    auto* colorLayout = new QVBoxLayout(colorRemovalWidget_);
    colorLayout->setContentsMargins(0, 0, 0, 0);
    colorLayout->setSpacing(7);
    auto* colorForm = new QFormLayout;
    colorForm->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    colorForm->setRowWrapPolicy(QFormLayout::WrapAllRows);

    auto* keyColorRow = new QHBoxLayout;
    btnKeyColorSample_->setObjectName("colorPickerButton");
    btnKeyColorSample_->setCursor(Qt::PointingHandCursor);
    btnKeyColorSample_->setToolTip(TR("Clique para escolher a cor a ser removida",
                                      "Click to choose the color to be removed"));
    btnAutoDetectKeyColor_->setToolTip(TR("Detecta a cor predominante dos cantos da imagem",
                                          "Detects the dominant color in image corners"));
    keyColorRow->addWidget(btnKeyColorSample_, 1);
    keyColorRow->addWidget(btnAutoDetectKeyColor_);
    colorForm->addRow(TR("Cor alvo", "Target color"), keyColorRow);

    colorToleranceSpin_->setRange(0, 100);
    colorToleranceSpin_->setValue(0);
    colorToleranceSpin_->setSuffix(" %");
    colorToleranceSpin_->setToolTip(TR("0% para pixel art exato; aumente para fundos com ruído ou variações",
                                       "0% for exact pixel art; increase for noisy backgrounds"));
    colorForm->addRow(TR("Tolerância", "Tolerance"), colorToleranceSpin_);
    colorLayout->addLayout(colorForm);

    colorContiguousCheckBox_->setChecked(false);
    colorContiguousCheckBox_->setToolTip(TR("Desmarcado: remove a cor em toda a imagem (inclusive entre pernas/braços);\nMarcado: remove apenas o fundo contíguo às bordas.",
                                            "Unchecked: removes color in entire image (including holes between limbs);\nChecked: removes only background connected to the borders."));
    colorLayout->addWidget(colorContiguousCheckBox_);

    colorPixelArtCheckBox_->setChecked(true);
    colorPixelArtCheckBox_->setToolTip(TR("Mantém as bordas perfeitamente duras sem suavização (essencial para Pixel Art e Sprites).",
                                          "Keeps edges perfectly sharp without feathering (essential for Pixel Art & Sprites)."));
    colorLayout->addWidget(colorPixelArtCheckBox_);

    btnRemoveByColor_->setObjectName("primaryAction");
    btnRemoveByColor_->setMinimumHeight(38);
    colorLayout->addWidget(btnRemoveByColor_);
    colorRemovalWidget_->setVisible(false);
    secRemoveLayout->addWidget(colorRemovalWidget_);

    // Fim da seccao 1: fecha o colapsavel e adiciona a seccao 2.
    inspectorLayout->addWidget(secRemove);

    auto* secOutput = new CollapsibleSection(
        TR("Ajustes de saída", "Output adjustments"), false, this);
    auto* secOutputLayout = secOutput->contentLayout();
    auto* outputForm = new QFormLayout;
    outputForm->setRowWrapPolicy(QFormLayout::WrapAllRows);
    outputForm->addRow(TR("Predefinição", "Preset"), presetCombo_);
    outputForm->addRow(TR("Fundo ao salvar", "Background on save"),
                       bgColorCombo_);
    // Formato de saida: PNG e WebP guardam transparencia; JPEG
    // compoe sobre o fundo escolhido (branco por omissao).
    saveFormatCombo_->addItem(TR("PNG (transparente)",
                                 "PNG (transparent)"));
    saveFormatCombo_->addItem(TR("JPEG", "JPEG"));
    saveFormatCombo_->addItem(TR("WebP", "WebP"));
    outputForm->addRow(TR("Formato", "Format"),
                       saveFormatCombo_);
    secOutputLayout->addLayout(outputForm);

    // Ajustes agrupados numa superficie propria: leem-se como um
    // bloco coerente em vez de 5 linhas soltas.
    auto* settingsBlock = new QFrame;
    settingsBlock->setObjectName("settingsBlock");
    auto* settingsBlockLayout = new QVBoxLayout(settingsBlock);
    settingsBlockLayout->setContentsMargins(11, 10, 11, 10);
    settingsBlockLayout->setSpacing(6);
    settingsBlockLayout->addWidget(refineCheckBox_);
    settingsBlockLayout->addWidget(noiseCheckBox_);
    auto* noiseRow = new QHBoxLayout;
    noiseRow->setContentsMargins(22, 0, 0, 0);
    noiseThresholdSpin_->setRange(0, 255);
    noiseThresholdSpin_->setValue(kAlphaThreshold);
    noiseThresholdSpin_->setSuffix(" /255");
    noiseThresholdSpin_->setToolTip(
        TR("Alfa abaixo deste valor fica totalmente transparente",
           "Alpha below this value becomes fully transparent"));
    noiseRow->addWidget(new QLabel(TR("Limiar", "Threshold")));
    noiseRow->addWidget(noiseThresholdSpin_, 1);
    settingsBlockLayout->addLayout(noiseRow);
    settingsBlockLayout->addWidget(stickerCheckBox_);
    settingsBlockLayout->addWidget(shadowCheckBox_);
    secOutputLayout->addWidget(settingsBlock);

    // Fim da seccao 2: fecha e adiciona a seccao 3.
    inspectorLayout->addWidget(secOutput);

    auto* secManual = new CollapsibleSection(
        TR("Ações e Retoque", "Actions and Retouch"),
        false, this);
    auto* secManualLayout = secManual->contentLayout();
    auto* brushForm = new QFormLayout;
    brushForm->setRowWrapPolicy(QFormLayout::WrapAllRows);
    brushForm->addRow(TR("Pincel", "Brush"), brushModeCombo_);
    brushForm->addRow(TR("Tamanho", "Size"), brushRadiusSpin_);
    secManualLayout->addLayout(brushForm);
    auto* undoRow = new QHBoxLayout;
    undoRow->addWidget(btnUndo_);
    undoRow->addWidget(btnRedo_);
    secManualLayout->addLayout(undoRow);
    auto* actionRow = new QHBoxLayout;
    actionRow->addWidget(btnAutoCrop_);
    actionRow->addWidget(btnDragOut_);
    secManualLayout->addLayout(actionRow);
    auto* brushHint = new QLabel(TR(
        "Após remover o fundo, use o pincel para corrigir detalhes, faça auto-recorte ou arraste o resultado.",
        "After removing the background, use the brush to touch up details, auto-crop or drag the result."));
    brushHint->setObjectName("inspectorHint");
    brushHint->setWordWrap(true);
    secManualLayout->addWidget(brushHint);
    inspectorLayout->addWidget(secManual);
    inspectorLayout->addStretch();

    // O inspector vive dentro de uma area rolavel: com janelas baixas
    // ele roda em vez de cortar os campos dos formularios.
    auto* inspectorScroll = new QScrollArea;
    inspectorScroll->setObjectName("inspectorScroll");
    inspectorScroll->setWidget(inspector);
    inspectorScroll->setWidgetResizable(true);
    inspectorScroll->setFrameShape(QFrame::NoFrame);
    inspectorScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    inspectorScroll->setFixedWidth(280);

    auto* contentSplitter = new QSplitter(Qt::Horizontal);
    contentSplitter->setChildrenCollapsible(false);
    contentSplitter->addWidget(workspace);
    contentSplitter->addWidget(inspectorScroll);
    contentSplitter->setStretchFactor(0, 1);

    // Abas principais: Editor, Lote, Modelos e Ajuda.
    mainTabs_ = new QTabWidget;
    mainTabs_->setObjectName("mainTabs");
    mainTabs_->addTab(contentSplitter, TR("Editor", "Editor"));
    {
        // A aba do lote e controlada pelo BatchTabController; o log e
        // a barra de progresso sao partilhados com a janela principal.
        BatchTabController::Config batchConfig;
        batchConfig.log = logEdit_;
        batchConfig.progress = progressBar_;
        batchConfig.modelCache = modelCache_;
        batchConfig.optionsProvider = [this]() {
            return currentProcessingOptions();
        };
        batchConfig.outputExtensionProvider = [this]() {
            return saveFormatExtension();
        };
        batchConfig.outputFormatHasAlpha = [this]() {
            return saveFormatIsAlphaCapable();
        };
        batch_->init(batchConfig);
        connect(batch_, &BatchTabController::busyChanged,
                this, &MainWindow::setUiBusy);
        connect(batch_, &BatchTabController::finished,
                this, [this](int, int) {
            emit jobFinished();
            maybeCloseAfterJobs();
        });
    }
    mainTabs_->addTab(batch_->createTab(), TR("Lote", "Batch"));
    modelManager_ = new ModelManagerWidget(this);
    mainTabs_->addTab(modelManager_, TR("Modelos", "Models"));
    mainTabs_->addTab(new HelpWidget(this), TR("Ajuda", "Help"));
    connect(modelManager_, &ModelManagerWidget::modelsChanged,
            this, [this](const QString&) {
        populateModelCombo();
    });

    mainLayout_ = new QVBoxLayout();
    mainLayout_->setContentsMargins(16, 0, 16, 16);
    mainLayout_->setSpacing(10);
    mainLayout_->addWidget(header);
    mainLayout_->addWidget(progressBar_);
    mainLayout_->addWidget(mainTabs_, 1);
    connect(zoomOutButton, &QPushButton::clicked, this, [this]() {
        setPreviewZoom(preview_->previewZoomPercent() - 25);
    });
    connect(zoomInButton, &QPushButton::clicked, this, [this]() {
        setPreviewZoom(preview_->previewZoomPercent() + 25);
    });
    connect(fitButton, &QPushButton::clicked, this, [this]() {
        setPreviewZoom(100);
    });

    auto* central = new QWidget(this);
    central->setLayout(mainLayout_);
    central->setObjectName("studioRoot");
    setCentralWidget(central);

    // O tema (paleta Fusion + QSS global) e aplicado por
    // AppTheme::apply() em main.cpp; o botao de tema alterna
    // em tempo de execucao e persiste em QSettings.

    setWindowTitle("Aletheia " ALETHEIA_VERSION);
    setMinimumSize(980, 680);
    resize(1320, 860);

    connect(btnThemeToggle_,   &QPushButton::clicked,
            this,              [this]() {
        const bool dark = AppTheme::toggle();
        btnThemeToggle_->setText(dark
            ? TR("\u2600 Tema claro", "\u2600 Light theme")
            : TR("\u263E Tema escuro", "\u263E Dark theme"));
    });
    connect(btnOpen_,          &QPushButton::clicked,
            this,              &MainWindow::openImage);
    connect(btnRemove_,        &QPushButton::clicked,
            this,              &MainWindow::removeBackground);
    connect(btnSave_,          &QPushButton::clicked,
            this,              &MainWindow::saveResult);
    connect(btnCopy_,          &QPushButton::clicked,
            this,              &MainWindow::copyResult);
    connect(btnCopyTransparent_, &QPushButton::clicked,
            this,              &MainWindow::copyTransparentResult);
    connect(saveFormatCombo_, QOverload<int>::of(
                &QComboBox::currentIndexChanged),
            this,              &MainWindow::onSaveFormatChanged);
    connect(btnLoadModel_,     &QPushButton::clicked,
            this,              &MainWindow::loadModel);
    connect(btnUndo_,          &QPushButton::clicked,
            this,              [this]() {
        if (!isBusy_) maskEditCanvas_->undo();
    });
    connect(btnRedo_,          &QPushButton::clicked,
            this,              [this]() {
        if (!isBusy_) maskEditCanvas_->redo();
    });
    connect(maskEditCanvas_, &MaskEditCanvas::availabilityChanged,
            this, [this](bool canUndo, bool canRedo) {
        btnUndo_->setEnabled(!isBusy_ && canUndo);
        btnRedo_->setEnabled(!isBusy_ && canRedo);
    });
    btnUndo_->setEnabled(false);
    btnRedo_->setEnabled(false);

    connect(btnAutoCrop_,      &QPushButton::clicked,
            this,              &MainWindow::autoCropResult);
    connect(btnDragOut_,       &QPushButton::clicked,
            this,              &MainWindow::startDragOut);
    connect(presetCombo_, QOverload<int>::of(
                &QComboBox::currentIndexChanged),
            this,              &MainWindow::presetChanged);
    connect(stickerCheckBox_,  &QCheckBox::toggled, this, [this](bool) {
        if (presetCombo_->currentIndex() != 0) presetCombo_->setCurrentIndex(0);
    });
    connect(shadowCheckBox_,   &QCheckBox::toggled, this, [this](bool) {
        if (presetCombo_->currentIndex() != 0) presetCombo_->setCurrentIndex(0);
    });

    updateKeyColorButton();
    btnAutoDetectKeyColor_->setEnabled(false);
    btnRemoveByColor_->setEnabled(false);
    connect(removalMethodCombo_, QOverload<int>::of(
                &QComboBox::currentIndexChanged),
            this,              &MainWindow::removalMethodChanged);
    connect(btnKeyColorSample_, &QPushButton::clicked,
            this,              &MainWindow::pickKeyColor);
    connect(btnAutoDetectKeyColor_, &QPushButton::clicked,
            this,              &MainWindow::autoDetectKeyColor);
    connect(btnRemoveByColor_, &QPushButton::clicked,
            this,              &MainWindow::removeByColor);

    // A inferencia corre numa thread; o resultado volta pelo watcher.
    connect(inference_, &InferenceWorker::finished,
            this,       &MainWindow::onRemoveFinished);

    // Atalhos de teclado.
    auto* scOpen = new QShortcut(QKeySequence::Open, this);
    connect(scOpen, &QShortcut::activated, this, &MainWindow::openImage);
    auto* scSave = new QShortcut(QKeySequence::Save, this);
    connect(scSave, &QShortcut::activated, this, &MainWindow::saveResult);
    auto* scCopy = new QShortcut(QKeySequence("Ctrl+Shift+C"), this);
    connect(scCopy, &QShortcut::activated, this, &MainWindow::copyResult);
    auto* scPaste = new QShortcut(QKeySequence::Paste, this);
    connect(scPaste, &QShortcut::activated, this, &MainWindow::pasteImageFromClipboard);
    auto* scUndo = new QShortcut(QKeySequence::Undo, this);
    connect(scUndo, &QShortcut::activated, this, [this]() {
        if (!isBusy_) maskEditCanvas_->undo();
    });
    auto* scRedo = new QShortcut(QKeySequence::Redo, this);
    connect(scRedo, &QShortcut::activated, this, [this]() {
        if (!isBusy_) maskEditCanvas_->redo();
    });
    auto* scRedoY = new QShortcut(QKeySequence("Ctrl+Y"), this);
    connect(scRedoY, &QShortcut::activated, this, [this]() {
        if (!isBusy_) maskEditCanvas_->redo();
    });
    auto* scZoomIn = new QShortcut(QKeySequence("Ctrl++"), this);
    connect(scZoomIn, &QShortcut::activated, this,
            [this]() { setPreviewZoom(preview_->previewZoomPercent() + 25); });
    auto* scZoomInEq = new QShortcut(QKeySequence("Ctrl+="), this);
    connect(scZoomInEq, &QShortcut::activated, this,
            [this]() { setPreviewZoom(preview_->previewZoomPercent() + 25); });
    auto* scZoomOut = new QShortcut(QKeySequence("Ctrl+-"), this);
    connect(scZoomOut, &QShortcut::activated, this,
            [this]() { setPreviewZoom(preview_->previewZoomPercent() - 25); });
    auto* scFit = new QShortcut(QKeySequence("Ctrl+0"), this);
    connect(scFit, &QShortcut::activated, this,
            [this]() { setPreviewZoom(100); });
    auto* scQuit = new QShortcut(QKeySequence::Quit, this);
    connect(scQuit, &QShortcut::activated, this, &QWidget::close);

    // Abrir imagens e pastas arrastando para a janela.
    setAcceptDrops(true);

    // Ctrl + roda do rato sobre qualquer preview faz zoom do resultado.
    originalScroll_->viewport()->installEventFilter(this);
    resultScroll_->viewport()->installEventFilter(this);
    connect(modelCombo_, QOverload<int>::of(
                &QComboBox::currentIndexChanged),
            this,              &MainWindow::modelIndexChanged);
    connect(bgColorCombo_, QOverload<int>::of(
                &QComboBox::currentIndexChanged),
            this,              &MainWindow::bgColorChanged);
    connect(maskEditCanvas_, &MaskEditCanvas::editingDone,
            this,              &MainWindow::editingDone);
    connect(brushModeCombo_, QOverload<int>::of(
                &QComboBox::currentIndexChanged),
            this, [this](int idx) {
        maskEditCanvas_->setMode(
            idx == 0 ? MaskEditCanvas::Restore
                     : MaskEditCanvas::Erase);
    });
    connect(brushRadiusSpin_, QOverload<int>::of(
                &QSpinBox::valueChanged),
            this, [this](int value) {
        maskEditCanvas_->setBrushRadius(value);
    });

    restoreSettings();

    // Ação de toggle para visualização de máscara alfa-only
    actionShowMaskPreview_ = new QAction(TR("Mostrar máscara alfa", "Show alpha mask"), this);
    actionShowMaskPreview_->setCheckable(true);
    actionShowMaskPreview_->setShortcut(QKeySequence("Ctrl+M"));
    actionShowMaskPreview_->setToolTip(TR("Alternar visualização da máscara alfa (alpha-only)", "Toggle alpha mask visualization (alpha-only)"));
    connect(actionShowMaskPreview_, &QAction::toggled, this, [this](bool checked){
        showMaskPreview_ = checked;
        updateResultDisplay();
    });
    QMenu* viewMenu = menuBar()->addMenu(TR("Ver", "View"));
    viewMenu->addAction(actionShowMaskPreview_);

    // Modelo preferido: o argumento da linha de comandos tem
    // prioridade; sem argumento, usa o ultimo guardado em QSettings.
    QString preferred = modelPath;
    if (preferred.isEmpty()) {
        preferred = QSettings("aletheia", "aletheia")
                        .value("model/path").toString();
    }
    if (!preferred.isEmpty()) {
        const int idx = modelCombo_->findData(preferred);
        if (idx >= 0) {
            modelCombo_->setCurrentIndex(idx);
        } else {
            modelPathEdit_->setText(preferred);
            modelCombo_->setCurrentIndex(modelCombo_->count() - 1);
        }
    }

    updateWorkspaceStack();

    // Verificacao de nova versao (assincrona, silenciosa em falha).
    checkForUpdate();
}

QWidget* MainWindow::createEmptyStateWidget()
{
    auto* container = new QWidget;
    auto* outerLayout = new QVBoxLayout(container);
    outerLayout->setContentsMargins(24, 24, 24, 24);
    outerLayout->setAlignment(Qt::AlignCenter);

    auto* dropCard = new QFrame;
    dropCard->setObjectName("dropZone");
    dropCard->setFrameShape(QFrame::StyledPanel);
    dropCard->setMinimumSize(440, 260);
    dropCard->setMaximumSize(640, 360);

    auto* cardLayout = new QVBoxLayout(dropCard);
    cardLayout->setContentsMargins(36, 32, 36, 32);
    cardLayout->setSpacing(14);
    cardLayout->setAlignment(Qt::AlignCenter);

    auto* iconLabel = new QLabel(QString::fromUtf8("\U0001F5BC\uFE0F"));
    iconLabel->setObjectName("dropZoneIcon");
    iconLabel->setAlignment(Qt::AlignCenter);
    QFont iconFont = iconLabel->font();
    iconFont.setPointSize(38);
    iconLabel->setFont(iconFont);

    auto* titleLabel = new QLabel(TR("Arraste e solte uma imagem aqui",
                                     "Drag and drop an image here"));
    titleLabel->setObjectName("dropZoneTitle");
    titleLabel->setAlignment(Qt::AlignCenter);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);

    auto* subLabel = new QLabel(TR("Formatos suportados: PNG, JPG, WebP, BMP\nou use os botões rápidos abaixo",
                                   "Supported formats: PNG, JPG, WebP, BMP\nor use the quick buttons below"));
    subLabel->setObjectName("dropZoneSubtitle");
    subLabel->setAlignment(Qt::AlignCenter);
    subLabel->setWordWrap(true);
    QFont subFont = subLabel->font();
    subFont.setPointSize(10);
    subLabel->setFont(subFont);

    auto* btnRow = new QHBoxLayout;
    btnRow->setSpacing(12);
    btnRow->setAlignment(Qt::AlignCenter);

    auto* btnPick = new QPushButton(TR("Abrir imagem (Ctrl+O)", "Open image (Ctrl+O)"));
    btnPick->setObjectName("dropZonePickBtn");
    btnPick->setCursor(Qt::PointingHandCursor);
    connect(btnPick, &QPushButton::clicked, this, &MainWindow::openImage);

    auto* btnPaste = new QPushButton(TR("Colar da área de transferência (Ctrl+V)", "Paste from clipboard (Ctrl+V)"));
    btnPaste->setObjectName("dropZonePasteBtn");
    btnPaste->setCursor(Qt::PointingHandCursor);
    connect(btnPaste, &QPushButton::clicked, this, &MainWindow::pasteImageFromClipboard);

    btnRow->addWidget(btnPick);
    btnRow->addWidget(btnPaste);

    cardLayout->addWidget(iconLabel);
    cardLayout->addWidget(titleLabel);
    cardLayout->addWidget(subLabel);
    cardLayout->addSpacing(6);
    cardLayout->addLayout(btnRow);

    outerLayout->addWidget(dropCard, 0, Qt::AlignCenter);
    return container;
}

void MainWindow::updateWorkspaceStack()
{
    preview_->updateWorkspaceStack();
}

void MainWindow::editingDone()
{
    // Durante uma inferencia/lote o canvas fica desativado; ainda
    // assim, ignorar edicoes evita substituir o resultado antigo por
    // engano caso algum evento chegue fora de tempo.
    if (isBusy_) return;
    preview_->setResult(maskEditCanvas_->getEditedImage());
    updateResultDisplay();
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    zoomDebounce_->start();
}

void MainWindow::setPreviewZoom(int percent)
{
    preview_->setPreviewZoom(percent);
}

void MainWindow::zoomAt(int percent, const QPoint& globalPos)
{
    preview_->zoomAt(percent, globalPos);
}

void MainWindow::updatePreviewZoom()
{
    preview_->updatePreviewZoom();
}

void MainWindow::openImage() {
    if (isBusy_) return;
    const QString path = QFileDialog::getOpenFileName(
        this, TR("Abrir imagem", "Open image"),
        !batch_->folderEdit()->text().isEmpty()
            ? batch_->folderEdit()->text() : QDir::homePath(),
        TR("Imagens (*.png *.jpg *.jpeg *.bmp *.webp *.tiff)",
           "Images (*.png *.jpg *.jpeg *.bmp *.webp *.tiff)"));
    if (path.isEmpty()) return;
    openPath(path);
}

void MainWindow::openPath(const QString& path) {
    // setAutoTransform aplica a orientacao EXIF (fotos de telemovel).
    const QImage img = ImagePostProcessor::loadImageExif(path);
    if (img.isNull()) {
        QMessageBox::warning(this, TR("Erro", "Error"),
            TR("Nao foi possivel carregar:\n", "Could not load:\n")
                + path);
        return;
    }
    ++imageGeneration_;
    preview_->clearResult();
    btnAutoCrop_->setEnabled(false);
    btnDragOut_->setEnabled(false);
    preview_->setOriginal(img);
    currentImagePath_ = path;
    imageNameLabel_->setText(QFileInfo(path).fileName());
    // O canvas do resultado e limpo por completo: sem isto, Salvar e
    // Copiar continuavam a exportar a imagem anterior.
    maskEditCanvas_->clearImage();
    // Se o modo Comparar estiver ativo, o lado "antes" passa a ser a
    // imagem recem-aberta.
    maskEditCanvas_->setCompareImage(img);
    keyColor_ = ImagePostProcessor::detectCornerColor(img);
    updateKeyColorButton();
    btnAutoDetectKeyColor_->setEnabled(true);
    btnRemoveByColor_->setEnabled(true);
    logEdit_->appendPlainText("[INFO] " + TR("Carregada: ", "Loaded: ") + path);
}

void MainWindow::loadModel() {
    if (isBusy_) return;
    QString path = QFileDialog::getOpenFileName(
        this, TR("Carregar modelo ONNX", "Load ONNX model"),
        QDir::homePath(), "ONNX (*.onnx)");
    if (path.isEmpty()) return;
    // Seleciona "Caminho customizado" sem deixar o slot limpar o
    // caminho escolhido; o indice fixo (2) apontava para outro modelo
    // quando havia modelos descarregados.
    {
        const QSignalBlocker blocker(modelCombo_);
        const int customIdx = modelCombo_->findData(QString());
        modelCombo_->setCurrentIndex(
            customIdx >= 0 ? customIdx : modelCombo_->count() - 1);
    }
    modelPathEdit_->setText(path);
    resetModel();
    logEdit_->appendPlainText(
        "[INFO] " + TR("Modelo definido: ", "Model set: ") + path);
}

void MainWindow::modelIndexChanged(int index) {
    if (index < 0 || index >= modelCombo_->count()) return;
    QString path = modelCombo_->itemData(index).toString();
    modelPathEdit_->setText(path);
    resetModel();

    // Pré-carregar modelo em background para evitar bloqueio na primeira inferência
    if (!path.isEmpty() && QFile::exists(path)) {
        logEdit_->appendPlainText("[INFO] " + TR("Pré-carregando modelo...", "Preloading model..."));
        (void)QtConcurrent::run([cache = modelCache_, path]() {
            try {
                // Aquece a MESMA cache usada pela inferencia. Antes o
                // pré-carregamento usava outra cache (a estatica de
                // BackgroundRemover::get) e destruia a sessao no fim,
                // obrigando a ler o modelo outra vez na inferencia.
                (void)InferenceWorker::obtainRemover(cache, path, nullptr);
            } catch (const std::exception&) {
                // Log silencioso; o erro sera mostrado na inferencia.
            }
        });
    }
}

void MainWindow::bgColorChanged(int index) {
    if (presetCombo_->currentIndex() != 0) {
        const QSignalBlocker blocker(presetCombo_);
        presetCombo_->setCurrentIndex(0);
    }
    if (index == 3) {
        QColor color = QColorDialog::getColor(
            customBgColor_.isValid() ? customBgColor_ : Qt::white,
            this, TR("Escolher cor de fundo", "Choose background color"));
        if (color.isValid()) {
            customBgColor_ = color;
            bgColorCombo_->setItemData(3,
                QVariant::fromValue(color));
            bgColorCombo_->setItemText(3, color.name());
            bgColorCombo_->setCurrentIndex(3);
        } else {
            bgColorCombo_->setCurrentIndex(0);
        }
    } else if (index == 5) {
        const QString path = QFileDialog::getOpenFileName(
            this, TR("Escolher imagem de fundo", "Choose background image"),
            QDir::homePath(),
            TR("Imagens (*.png *.jpg *.jpeg *.bmp *.webp *.tiff)",
               "Images (*.png *.jpg *.jpeg *.bmp *.webp *.tiff)"));
        if (!path.isEmpty()) {
            customBgImagePath_ = path;
            bgColorCombo_->setItemText(5, QFileInfo(path).fileName());
        } else if (customBgImagePath_.isEmpty()) {
            bgColorCombo_->setCurrentIndex(0);
        }
    }
    if (!preview_->resultImage().isNull()) {
        maskEditCanvas_->setBackgroundColor(currentBackgroundColor());
    }
}

void MainWindow::removeBackground() {
    if (preview_->originalImage().isNull()) {
        QMessageBox::warning(this, TR("Aviso", "Warning"),
            TR("Abra uma imagem primeiro.", "Open an image first."));
        return;
    }
    if (inference_->isRunning()) {
        // O modelo e carregado na thread de trabalho: enquanto nao
        // houver sessao na cache nao ha o que cancelar.
        std::shared_ptr<BackgroundRemover> remover;
        {
            QMutexLocker lock(&modelCache_->mutex);
            if (modelCache_->remover
                && modelCache_->loadedPath == modelPathEdit_->text())
                remover = modelCache_->remover;
        }
        if (remover) {
            inference_->cancel();
            logEdit_->appendPlainText("[INFO] " + TR("Cancelamento solicitado.", "Cancellation requested."));
        } else {
            logEdit_->appendPlainText("[INFO] " + TR("A carregar o modelo...", "Loading the model..."));
        }
        return;
    }
    if (batch_->isRunning()) return;

    // A inferencia corre numa thread de trabalho: a janela continua
    // reativa e o modelo e carregado la dentro (pode demorar segundos).
    setUiBusy(true);
    progressBar_->setRange(0, 0);
    progressBar_->setValue(0);
    progressBar_->setVisible(true);
    logEdit_->appendPlainText(
        "[INFO] " + TR("A remover fundo...", "Removing background..."));

    ProcessingOptions options = currentProcessingOptions();
    // A cor de fundo e apenas uma pre-visualizacao na imagem unica;
    // manter a mascara transparente permite continuar editando-a.
    options.backgroundColor = QColor();

    // Se outra imagem for aberta durante a inferencia, este numero
    // muda e o resultado antigo e descartado ao chegar.
    const quint64 generation = imageGeneration_;

    // O modelo e carregado DENTRO da thread de trabalho: a janela nao
    // congela enquanto o ONNX le o ficheiro (176 MB no U2-Net). Se o
    // modelo nao existir ou for invalido, o worker devolve o erro e
    // este chega pelo mesmo caminho (log + dialogo).
    inference_->start(preview_->originalImage(), options,
                      options.modelPath, generation);
}

void MainWindow::onRemoveFinished(const InferenceResult& res)
{
    progressBar_->setVisible(false);
    progressBar_->setRange(0, 100);
    progressBar_->setValue(0);
    setUiBusy(false);

    // A imagem pode ter sido trocada enquanto a inferencia corria.
    // Nunca aplicar o resultado antigo sobre a imagem nova.
    if (res.generation != imageGeneration_) {
        logEdit_->appendPlainText("[INFO] " + TR(
            "Resultado de uma imagem anterior ignorado.",
            "Result from a previous image ignored."));
        emit jobFinished();
        maybeCloseAfterJobs();
        return;
    }
    if (!res.error.isEmpty()) {
        logEdit_->appendPlainText("[ERROR] " + res.error);
        // Sinal antes do dialogo: consumidores automaticos (ex.: a
        // ferramenta de screenshots em modo offscreen) nao ficam
        // bloqueados por uma caixa modal.
        emit jobFinished();
        // Durante o fecho nao bloquear a janela com uma caixa modal.
        if (!closeRequested_) {
            QMessageBox::critical(this, TR("Erro", "Error"),
                TR("Falha ao remover o fundo:\n",
                   "Failed to remove the background:\n") + res.error);
        }
        maybeCloseAfterJobs();
        return;
    }
    preview_->setResult(res.image);
    updateResultDisplay();
    btnAutoCrop_->setEnabled(true);
    btnDragOut_->setEnabled(true);
    logEdit_->appendPlainText("[OK] " + TR("Fundo removido.",
                                           "Background removed."));
    emit jobFinished();
    maybeCloseAfterJobs();
}

void MainWindow::saveResult() {
    if (isBusy_) return;
    if (preview_->resultImage().isNull()) {
        QMessageBox::warning(this, TR("Aviso", "Warning"),
            TR("Remova o fundo primeiro.", "Remove the background first."));
        return;
    }

    const QString extension = saveFormatExtension();
    QSettings settings("aletheia", "aletheia");
    QString startDir = settings.value("output/lastSaveDir").toString();
    if (startDir.isEmpty() || !QDir(startDir).exists()) {
        startDir = currentImagePath_.isEmpty()
            ? QDir::homePath()
            : QFileInfo(currentImagePath_).absolutePath();
    }
    QString path = QFileDialog::getSaveFileName(
        this, TR("Salvar imagem", "Save image"), startDir,
        saveFormatFilter());
    if (path.isEmpty()) return;
    const QString typedPath = path;
    const QString typedSuffix = QFileInfo(path).suffix();
    path = savePathWithExtension(
        path, extension,
        extension == "jpg" ? QStringList() << "jpeg"
                            : QStringList());
    if (path != typedPath && !typedSuffix.isEmpty()) {
        logEdit_->appendPlainText(
            "[INFO] " + TR(
                "Extensão ajustada para o formato escolhido: ",
                "Extension adjusted to the chosen format: ") + path);
    }

    const QByteArray fmt = extension.toUpper().toUtf8();
    if (!QImageWriter::supportedImageFormats().contains(
            extension.toLatin1())) {
        QMessageBox::warning(this, TR("Aviso", "Warning"),
            TR("O formato %1 não é suportado nesta instalação do Qt "
               "(no Linux instale o pacote "
               "'qt6-image-formats-plugins'). Use PNG.",
               "Format %1 is not supported by this Qt installation "
               "(on Linux install the 'qt6-image-formats-plugins' "
               "package). Use PNG instead.")
                .arg(extension.toUpper()));
        return;
    }

    ProcessingOptions saveOptions = currentProcessingOptions();
    // Formatos sem canal alfa precisam de fundo solido, senao o Qt
    // compoe sobre preto e o resultado surpreende.
    const bool needSolidBg = currentBackgroundColor().isValid()
        || !saveFormatIsAlphaCapable();
    if (needSolidBg && saveOptions.backgroundStyle == ProcessingOptions::BgTransparent) {
        saveOptions.backgroundStyle = ProcessingOptions::BgSolidColor;
        saveOptions.backgroundColor = Qt::white;
    }

    const QImage toSave = ImagePostProcessor::apply(
        preview_->resultImage(), saveOptions, preview_->originalImage());

    QImageWriter writer(path, fmt);
    if (extension == "png" && saveOptions.pngCompression >= 0) {
        writer.setQuality(-1); // PNG usa compressão, não quality
        writer.setCompression(saveOptions.pngCompression);
    } else if (extension == "jpg") {
        writer.setQuality(saveOptions.jpegQuality);
    } else if (extension == "webp") {
        writer.setQuality(saveOptions.webpQuality);
    }
    if (!writer.write(toSave)) {
        QMessageBox::critical(this, TR("Erro", "Error"),
            TR("Não foi possível salvar:\n", "Could not save:\n") + path + "\n" + writer.errorString());
    } else {
        settings.setValue("output/lastSaveDir",
                          QFileInfo(path).absolutePath());
        QMessageBox::information(this, TR("OK", "OK"),
            TR("Salvo:\n", "Saved:\n") + path);
        logEdit_->appendPlainText(
            "[OK] " + TR("Imagem salva: ", "Image saved: ") + path);
    }
}

QString MainWindow::saveFormatFilter() const
{
    switch (saveFormatCombo_->currentIndex()) {
    case 1: return QStringLiteral("JPEG (*.jpg *.jpeg)");
    case 2: return QStringLiteral("WebP (*.webp)");
    default: return QStringLiteral("PNG (*.png)");
    }
}

QString MainWindow::saveFormatExtension() const
{
    switch (saveFormatCombo_->currentIndex()) {
    case 1: return QStringLiteral("jpg");
    case 2: return QStringLiteral("webp");
    default: return QStringLiteral("png");
    }
}

bool MainWindow::saveFormatIsAlphaCapable() const
{
    // PNG e WebP suportam canal alfa; JPEG nao.
    const int index = saveFormatCombo_->currentIndex();
    return index == 0 || index == 2;
}

void MainWindow::copyResult()
{
    if (isBusy_) return;
    if (preview_->resultImage().isNull()) {
        QMessageBox::warning(this, TR("Aviso", "Warning"),
            TR("Remova o fundo primeiro.", "Remove the background first."));
        return;
    }
    ProcessingOptions copyOptions = currentProcessingOptions();
    const QImage toCopy = ImagePostProcessor::apply(
        preview_->resultImage(), copyOptions, preview_->originalImage());

    QApplication::clipboard()->setImage(toCopy);
    logEdit_->appendPlainText(
        "[OK] " + TR("Resultado copiado para a area de transferencia.",
                     "Result copied to the clipboard."));
}

void MainWindow::copyTransparentResult()
{
    if (isBusy_) return;
    if (preview_->resultImage().isNull()) {
        QMessageBox::warning(this, TR("Aviso", "Warning"),
            TR("Remova o fundo primeiro.", "Remove the background first."));
        return;
    }
    // Use resultImage directly without applying background/blurring.
    QImage img = preview_->resultImage().convertToFormat(QImage::Format_RGBA8888);

    // Encode as PNG via QBuffer to guarantee PNG format.
    QByteArray pngData;
    QBuffer buffer(&pngData);
    buffer.open(QIODevice::WriteOnly);
    QImageWriter writer(&buffer, "PNG");
    if (!writer.write(img)) {
        QMessageBox::warning(this, TR("Aviso", "Warning"),
            TR("Falha ao codificar PNG para a área de transferência.",
               "Failed to encode PNG for clipboard."));
        return;
    }

    QMimeData *mime = new QMimeData;
    mime->setData("image/png", pngData);
    mime->setImageData(img);
    QApplication::clipboard()->setMimeData(mime);

    logEdit_->appendPlainText(
        "[OK] " + TR("PNG transparente copiado para a área de transferência.",
                     "Transparent PNG copied to clipboard."));
}

void MainWindow::onSaveFormatChanged(int index)
{
    // PNG e WebP guardam transparencia; JPEG funde sobre o fundo
    // escolhido, por isso o rotulo da combo muda de contexto.
    Q_UNUSED(index)
}

void MainWindow::checkForUpdate()
{
    // Uma vez por sessao, assincrono e silencioso: falha de rede nao
    // deve incomodar quem trabalha offline. A verificacao pode ser
    // desativada por variavel de ambiente ou QSettings; nao envia
    // imagens, apenas consulta a API publica de releases.
    if (qEnvironmentVariableIsSet("ALETHEIA_NO_UPDATE_CHECK") ||
        qEnvironmentVariableIsSet("BG_REMOVER_NO_UPDATE_CHECK")) return;
    if (!QSettings("aletheia", "aletheia")
             .value("update/check", true).toBool()) {
        return;
    }
    if (updateNam_) return;
    updateNam_ = new QNetworkAccessManager(this);

    QNetworkRequest request(QUrl(QStringLiteral(
        "https://api.github.com/repos/MarielCarlosFrias/aletheia"
        "/releases/latest")));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("User-Agent",
                         QByteArray("aletheia/") + ALETHEIA_VERSION);
    request.setTransferTimeout(5000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = updateNam_->get(request);
    connect(reply, &QNetworkReply::finished, this,
            &MainWindow::onUpdateCheckFinished);
}

void MainWindow::onUpdateCheckFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        // Offline ou rate limit: sigiloso.
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    if (doc.isObject()) {
        latestRemoteVersion_ =
            doc.object().value("tag_name").toString();
    }
    if (latestRemoteVersion_.isEmpty()) return;

    // Comparacao semantica simples (major.minor.patch), ignorando o "v".
    const QString remote = latestRemoteVersion_.startsWith('v')
        ? latestRemoteVersion_.mid(1) : latestRemoteVersion_;
    const QString local = QStringLiteral(ALETHEIA_VERSION);
    auto parse = [](const QString& s) {
        const QStringList parts = s.split('.');
        QList<int> nums;
        for (const QString& p : parts) nums << p.toInt();
        while (nums.size() < 3) nums << 0;
        return nums;
    };
    const QList<int> r = parse(remote);
    const QList<int> l = parse(local);

    if (r.at(0) > l.at(0)
        || (r.at(0) == l.at(0) && r.at(1) > l.at(1))
        || (r.at(0) == l.at(0) && r.at(1) == l.at(1) && r.at(2) > l.at(2))) {
        showUpdateBanner(latestRemoteVersion_);
    }
}

void MainWindow::showUpdateBanner(const QString& version)
{
    if (version.isEmpty()) return;

    // Banner clicavel no topo do workspace. O texto tem de ser
    // rich text com <a href>; sem isso linkActivated nunca dispara.
    const QUrl releasesUrl(QStringLiteral(
        "https://github.com/MarielCarlosFrias/aletheia/releases"));
    updateBanner_->setTextFormat(Qt::RichText);
    updateBanner_->setOpenExternalLinks(false);
    updateBanner_->setTextInteractionFlags(Qt::LinksAccessibleByMouse);
    updateBanner_->setText(
        TR("⬆ <a href=\"%1\" style=\"color:#102217;\">"
           "Nova versão %2 disponível — clique para abrir as releases"
           "</a>",
           "⬆ <a href=\"%1\" style=\"color:#102217;\">"
           "New version %2 available — click to open releases"
           "</a>")
            .arg(releasesUrl.toString(), version));
    updateBanner_->setCursor(Qt::PointingHandCursor);
    updateBanner_->setObjectName("updateBanner");
    updateBanner_->setWordWrap(true);

    // Inserido antes do divisor de preview (posicao 0 do workspace).
    if (auto* wl = workspaceLayout()) {
        if (wl->indexOf(updateBanner_) < 0)
            wl->insertWidget(0, updateBanner_);
    }

    // Evita ligacoes duplicadas se o banner for mostrado mais de uma vez.
    QObject::disconnect(updateBanner_, nullptr, this, nullptr);
    connect(updateBanner_, &QLabel::linkActivated, this,
            [releasesUrl](const QString&) {
        QDesktopServices::openUrl(releasesUrl);
    });
}

QVBoxLayout* MainWindow::workspaceLayout()
{
    // O workspace e o frame pai da barra de ferramentas de zoom.
    QWidget* w = originalScroll_;
    while (w && w->objectName() != QLatin1String("workspace"))
        w = w->parentWidget();
    return w ? qobject_cast<QVBoxLayout*>(w->layout()) : nullptr;
}

void MainWindow::setUiBusy(bool busy)
{
    isBusy_ = busy;
    btnOpen_->setEnabled(!busy);
    if (busy) {
        btnRemove_->setText(TR("Cancelar", "Cancel"));
    } else {
        btnRemove_->setText(TR("Remover fundo", "Remove background"));
    }
    btnRemove_->setEnabled(true);
    // Manter navegação e seleção de modelo ativa durante inferência para responsividade
    btnLoadModel_->setEnabled(true);
    modelCombo_->setEnabled(true);
    modelPathEdit_->setEnabled(true);
    btnThemeToggle_->setEnabled(true);
    btnSave_->setEnabled(!busy);
    btnCopy_->setEnabled(!busy);
    btnCopyTransparent_->setEnabled(!busy);
    batch_->setBusy(busy);
    saveFormatCombo_->setEnabled(!busy);
    presetCombo_->setEnabled(!busy);
    bgColorCombo_->setEnabled(!busy);
    refineCheckBox_->setEnabled(!busy);
    noiseCheckBox_->setEnabled(!busy);
    noiseThresholdSpin_->setEnabled(!busy);
    stickerCheckBox_->setEnabled(!busy);
    shadowCheckBox_->setEnabled(!busy);
    removalMethodCombo_->setEnabled(!busy);
    btnKeyColorSample_->setEnabled(!busy);
    btnAutoDetectKeyColor_->setEnabled(!busy && !preview_->originalImage().isNull());
    colorToleranceSpin_->setEnabled(!busy);
    colorContiguousCheckBox_->setEnabled(!busy);
    colorPixelArtCheckBox_->setEnabled(!busy);
    btnRemoveByColor_->setEnabled(!busy && !preview_->originalImage().isNull());
    brushModeCombo_->setEnabled(!busy);
    brushRadiusSpin_->setEnabled(!busy);
    btnAutoCrop_->setEnabled(!busy && !preview_->resultImage().isNull());
    btnDragOut_->setEnabled(!busy && !preview_->resultImage().isNull());
    maskEditCanvas_->setEnabled(!busy);
    btnUndo_->setEnabled(!busy && maskEditCanvas_->canUndo());
    btnRedo_->setEnabled(!busy && maskEditCanvas_->canRedo());
}

void MainWindow::resetModel()
{
    // O modelo pode estar em uso numa thread de trabalho; o mutex
    // protege e a copia shared_ptr do worker mantem a sessao viva
    // ate o passo terminar.
    inference_->resetModel();
}

void MainWindow::populateModelCombo()
{
    const QString current = modelPathEdit_->text();
    const QSignalBlocker blocker(modelCombo_);
    modelCombo_->clear();

    // So lista modelos que existem no disco; antes os dois primeiros
    // apareciam sempre e falhavam na inferencia quando em falta.
    const auto addInstalled = [this](const QString& fileName,
                                     const QString& label) {
        const QString path = ModelPathResolver::resolve(fileName);
        if (QFile::exists(path))
            modelCombo_->addItem(label, path);
    };
    addInstalled("u2netp.onnx",
                 TR("U\u00b2-Netp \u2014 r\u00e1pido (~5 MB)",
                    "U\u00b2-Netp \u2014 fast (~5 MB)"));
    addInstalled("u2net.onnx",
                 TR("U\u00b2-Net  \u2014 qualidade (~176 MB)",
                    "U\u00b2-Net  \u2014 quality (~176 MB)"));

    // Modelos descarregados na aba "Modelos". Inclui os caminhos
    // antigos (GUI/CLI anteriores) para nao "desaparecerem" modelos
    // ja instalados; o download novo usa o caminho canonico.
    const auto addModelsFromDir = [this](const QDir& dir) {
        const QStringList onnx = dir.entryList(
            QStringList() << "*.onnx", QDir::Files, QDir::Name);
        for (const QString& f : onnx) {
            bool alreadyListed = false;
            for (int i = 0; i < modelCombo_->count(); ++i) {
                const QString listed =
                    QFileInfo(modelCombo_->itemData(i).toString())
                        .fileName();
                if (listed.compare(f, Qt::CaseInsensitive) == 0) {
                    alreadyListed = true;
                    break;
                }
            }
            if (!alreadyListed)
                modelCombo_->addItem(f, dir.absoluteFilePath(f));
        }
    };
    addModelsFromDir(QDir(ModelPathResolver::userModelsDir()));
    for (const QString& legacy : ModelPathResolver::legacyUserModelsDirs())
        addModelsFromDir(QDir(legacy));

    modelCombo_->addItem(TR("Caminho customizado", "Custom path"), "");
    int idx = modelCombo_->findData(current);
    if (idx < 0 && !current.isEmpty()) {
        modelPathEdit_->setText(current);
        idx = modelCombo_->count() - 1;
    }
    modelCombo_->setCurrentIndex(qMax(0, idx));
}

void MainWindow::restoreSettings()
{
    QSettings settings("aletheia", "aletheia");
    const QByteArray geometry =
        settings.value("ui/geometry").toByteArray();
    if (!geometry.isEmpty()) restoreGeometry(geometry);

    batch_->folderEdit()->setText(settings.value("batch/inputDir").toString());
    {
        // Um valor guardado vazio nao pode apagar a pasta por omissao.
        const QString savedOutput =
            settings.value("batch/outputDir").toString();
        batch_->outputDirEdit()->setText(
            savedOutput.isEmpty() ? QStringLiteral("./_rembg")
                                   : savedOutput);
    }
    brushRadiusSpin_->setValue(
        settings.value("brush/radius", 10).toInt());
    refineCheckBox_->setChecked(
        settings.value("options/refineEdges", false).toBool());
    noiseCheckBox_->setChecked(
        settings.value("options/cleanNoise", false).toBool());
    noiseThresholdSpin_->setValue(
        settings.value("options/noiseThreshold", kAlphaThreshold).toInt());
    {
        const QSignalBlocker blocker(saveFormatCombo_);
        const int fmt = settings.value("output/saveFormat", 0).toInt();
        if (fmt >= 0 && fmt < saveFormatCombo_->count())
            saveFormatCombo_->setCurrentIndex(fmt);
    }

    const QVariant colorVar = settings.value("output/customColor");
    if (colorVar.isValid()) {
        const QColor color = colorVar.value<QColor>();
        if (color.isValid()) {
            customBgColor_ = color;
            bgColorCombo_->setItemData(3, QVariant::fromValue(color));
            bgColorCombo_->setItemText(3, color.name());
        }
    }
    {
        // Sem o QSignalBlocker, o indice 3 dispararia o dialogo de cor.
        const QSignalBlocker blocker(bgColorCombo_);
        const int idx =
            settings.value("output/bgIndex", 0).toInt();
        if (idx >= 0 && idx < bgColorCombo_->count())
            bgColorCombo_->setCurrentIndex(idx);
    }
}

void MainWindow::saveSettings()
{
    QSettings settings("aletheia", "aletheia");
    settings.setValue("ui/geometry", saveGeometry());
    settings.setValue("batch/inputDir", batch_->folderEdit()->text());
    settings.setValue("batch/outputDir", batch_->outputDirEdit()->text());
    settings.setValue("brush/radius", brushRadiusSpin_->value());
    settings.setValue("options/refineEdges",
                      refineCheckBox_->isChecked());
    settings.setValue("options/cleanNoise",
                      noiseCheckBox_->isChecked());
    settings.setValue("options/noiseThreshold",
                      noiseThresholdSpin_->value());
    settings.setValue("output/bgIndex", bgColorCombo_->currentIndex());
    settings.setValue("output/customColor", customBgColor_);
    settings.setValue("output/saveFormat",
                     saveFormatCombo_->currentIndex());
    if (!modelPathEdit_->text().isEmpty())
        settings.setValue("model/path", modelPathEdit_->text());
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    saveSettings();

    // Cancelar o lote, mas deixar o passo em curso terminar: o
    // watcher ainda esta a usar o modelo e a imagem.
    if (batch_->isRunning())
        batch_->requestCancel();

    if (inference_->isRunning() || batch_->isRunning()) {
        if (!closeRequested_) {
            closeRequested_ = true;
            logEdit_->appendPlainText("[INFO] " + TR(
                "A terminar o processamento antes de fechar...",
                "Finishing processing before closing..."));
        }
        event->ignore();
        return;
    }

    closeRequested_ = false;
    QMainWindow::closeEvent(event);
}

void MainWindow::maybeCloseAfterJobs()
{
    if (!closeRequested_) return;
    if (inference_->isRunning() || batch_->isRunning()) return;

    // O close() volta a entrar em closeEvent(); sem jobs pendentes
    // a janela fecha normalmente.
    closeRequested_ = false;
    QTimer::singleShot(0, this, [this]() { close(); });
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    // Aceitar se qualquer um dos URLs for pasta ou imagem; antes so
    // o primeiro era considerado.
    for (const QUrl& url : event->mimeData()->urls()) {
        const QFileInfo fi(url.toLocalFile());
        if (fi.isDir()
            || BatchPlan::imageExts().contains(fi.suffix().toLower())) {
            event->acceptProposedAction();
            return;
        }
    }
}

void MainWindow::dropEvent(QDropEvent* event)
{
    if (isBusy_) return;
    const QList<QUrl> urls = event->mimeData()->urls();
    if (urls.isEmpty()) return;

    QString folder;
    QString image;
    for (const QUrl& url : urls) {
        const QFileInfo fi(url.toLocalFile());
        if (folder.isEmpty() && fi.isDir())
            folder = fi.absoluteFilePath();
        else if (image.isEmpty()
                 && BatchPlan::imageExts().contains(
                        fi.suffix().toLower()))
            image = fi.absoluteFilePath();
    }

    if (!folder.isEmpty()) {
        batch_->folderEdit()->setText(folder);
        mainTabs_->setCurrentIndex(1);
        logEdit_->appendPlainText(
            "[INFO] " + TR("Pasta de lote: ", "Batch folder: ") + folder);
        event->acceptProposedAction();
        return;
    }
    if (!image.isEmpty()) {
        openPath(image);
        if (urls.size() > 1) {
            logEdit_->appendPlainText(
                "[INFO] " + TR("Vários itens arrastados; a abrir o primeiro.",
                               "Multiple items dropped; opening the first."));
        }
        event->acceptProposedAction();
    }
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::Wheel) {
        auto* wheel = static_cast<QWheelEvent*>(event);
        const bool overPreview =
            watched == originalScroll_->viewport()
            || watched == resultScroll_->viewport();
        if (overPreview
            && wheel->modifiers().testFlag(Qt::ControlModifier)) {
            // Zoom ancorado no ponto sob o cursor.
            zoomAt(preview_->previewZoomPercent()
                       + (wheel->angleDelta().y() > 0 ? 10 : -10),
                   wheel->globalPosition().toPoint());
            return true;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::setOriginal(const QImage& img) {
    preview_->setOriginal(img);
}

void MainWindow::setResult(const QImage& img) {
    preview_->setResult(img);
}

void MainWindow::updateResultDisplay() {
    if (preview_->resultImage().isNull()) {
        maskEditCanvas_->setImage(QImage(), QColor());
        return;
    }
    if (showMaskPreview_) {
        QImage maskImg = ImagePostProcessor::visualizeMask(preview_->resultImage(), true);
        maskEditCanvas_->setImage(maskImg, QColor());
        maskEditCanvas_->setEnabled(false);
    } else {
        maskEditCanvas_->setEnabled(true);
        maskEditCanvas_->setImage(preview_->resultImage(), currentBackgroundColor());
    }
    updatePreviewZoom();
}

void MainWindow::pasteImageFromClipboard()
{
    if (isBusy_) return;
    const QClipboard* clipboard = QApplication::clipboard();
    const QImage img = clipboard->image();
    if (!img.isNull()) {
        ++imageGeneration_;
        preview_->clearResult();
        btnAutoCrop_->setEnabled(false);
        btnDragOut_->setEnabled(false);
        preview_->setOriginal(img);
        currentImagePath_.clear();
        imageNameLabel_->setText(TR("Colada da área de transferência",
                                    "Pasted from clipboard"));
        maskEditCanvas_->clearImage();
        maskEditCanvas_->setCompareImage(img);
        setOriginal(img);
        keyColor_ = ImagePostProcessor::detectCornerColor(img);
        updateKeyColorButton();
        btnAutoDetectKeyColor_->setEnabled(true);
        btnRemoveByColor_->setEnabled(true);
        logEdit_->appendPlainText("[INFO] " + TR(
            "Imagem colada da área de transferência.",
            "Image pasted from clipboard."));
        return;
    }

    const QMimeData* mime = clipboard->mimeData();
    if (mime && mime->hasUrls()) {
        for (const QUrl& url : mime->urls()) {
            const QString localFile = url.toLocalFile();
            const QFileInfo fi(localFile);
            if (BatchPlan::imageExts().contains(fi.suffix().toLower())) {
                openPath(localFile);
                return;
            }
        }
    }

    QMessageBox::information(this, TR("Área de transferência", "Clipboard"),
        TR("Nenhuma imagem encontrada na área de transferência.",
           "No image found in the clipboard."));
}

void MainWindow::autoCropResult()
{
    if (isBusy_ || preview_->resultImage().isNull()) return;

    const QImage cropped = ImagePostProcessor::autoCrop(preview_->resultImage(), 2);
    if (cropped.size() == preview_->resultImage().size()) {
        logEdit_->appendPlainText("[INFO] " + TR("Nenhuma borda vazia para recortar.",
                                                "No empty border to crop."));
        return;
    }

    preview_->setResult(cropped);
    updateResultDisplay();
    logEdit_->appendPlainText(QString("[OK] %1: %2x%3")
        .arg(TR("Auto-recorte aplicado", "Auto-crop applied"))
        .arg(preview_->resultImage().width())
        .arg(preview_->resultImage().height()));
}

void MainWindow::presetChanged(int index)
{
    const QSignalBlocker bBg(bgColorCombo_);
    const QSignalBlocker bSticker(stickerCheckBox_);
    const QSignalBlocker bShadow(shadowCheckBox_);
    const QSignalBlocker bRefine(refineCheckBox_);
    const QSignalBlocker bNoise(noiseCheckBox_);
    const QSignalBlocker bNoiseThresh(noiseThresholdSpin_);

    switch (index) {
    case 1: // Padrão (Transparente)
        bgColorCombo_->setCurrentIndex(0);
        stickerCheckBox_->setChecked(false);
        shadowCheckBox_->setChecked(false);
        refineCheckBox_->setChecked(true);
        noiseCheckBox_->setChecked(false);
        noiseThresholdSpin_->setValue(30);
        break;
    case 2: // E-commerce (Branco + Sombra)
        bgColorCombo_->setCurrentIndex(1); // Branco
        stickerCheckBox_->setChecked(false);
        shadowCheckBox_->setChecked(true);
        refineCheckBox_->setChecked(true);
        noiseCheckBox_->setChecked(true);
        noiseThresholdSpin_->setValue(25);
        break;
    case 3: // Sticker (Contorno Branco)
        bgColorCombo_->setCurrentIndex(0); // Transparente
        stickerCheckBox_->setChecked(true);
        shadowCheckBox_->setChecked(false);
        refineCheckBox_->setChecked(true);
        noiseCheckBox_->setChecked(false);
        noiseThresholdSpin_->setValue(30);
        break;
    case 4: // Retrato (Fundo Desfocado)
        bgColorCombo_->setCurrentIndex(4); // Desfoque original
        stickerCheckBox_->setChecked(false);
        shadowCheckBox_->setChecked(false);
        refineCheckBox_->setChecked(true);
        noiseCheckBox_->setChecked(true);
        noiseThresholdSpin_->setValue(20);
        break;
    case 5: // Documento (Recorte justo)
        bgColorCombo_->setCurrentIndex(1); // Branco
        stickerCheckBox_->setChecked(false);
        shadowCheckBox_->setChecked(false);
        refineCheckBox_->setChecked(false);
        noiseCheckBox_->setChecked(false);
        noiseThresholdSpin_->setValue(15);
        break;
    case 6: // Arte Pixel (Chroma key tolerância baixa)
        bgColorCombo_->setCurrentIndex(0); // Transparente
        stickerCheckBox_->setChecked(false);
        shadowCheckBox_->setChecked(false);
        refineCheckBox_->setChecked(false);
        noiseCheckBox_->setChecked(false);
        noiseThresholdSpin_->setValue(10);
        break;
    default:
        break;
    }

    if (!preview_->resultImage().isNull()) {
        maskEditCanvas_->setBackgroundColor(currentBackgroundColor());
    }
}

void MainWindow::startDragOut()
{
    if (preview_->resultImage().isNull()) return;

    ProcessingOptions dragOptions = currentProcessingOptions();
    if (!saveFormatIsAlphaCapable()
        && dragOptions.backgroundStyle == ProcessingOptions::BgTransparent) {
        dragOptions.backgroundStyle = ProcessingOptions::BgSolidColor;
        dragOptions.backgroundColor = Qt::white;
    }
    const QImage finalImage = ImagePostProcessor::apply(
        preview_->resultImage(), dragOptions, preview_->originalImage());

    const QString tempDir = QDir::tempPath();
    const QString baseName = currentImagePath_.isEmpty()
        ? QStringLiteral("bg_removed")
        : QFileInfo(currentImagePath_).completeBaseName() + QStringLiteral("_rembg");
    const QString tempPath = QDir(tempDir).filePath(baseName + QStringLiteral(".png"));
    finalImage.save(tempPath, "PNG");

    auto* drag = new QDrag(this);
    auto* mimeData = new QMimeData;
    mimeData->setImageData(finalImage);
    mimeData->setUrls({QUrl::fromLocalFile(tempPath)});
    drag->setMimeData(mimeData);

    const QPixmap preview = QPixmap::fromImage(
        finalImage.scaled(128, 128, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    drag->setPixmap(preview);
    drag->setHotSpot(QPoint(preview.width() / 2, preview.height() / 2));

    drag->exec(Qt::CopyAction);
}

void MainWindow::updateKeyColorButton()
{
    const QString hex = keyColor_.name(QColor::HexRgb).toUpper();
    const double luminance = (0.299 * keyColor_.red() + 0.587 * keyColor_.green() + 0.114 * keyColor_.blue()) / 255.0;
    const QString textColor = luminance > 0.5 ? "#000000" : "#FFFFFF";

    btnKeyColorSample_->setText(hex);
    btnKeyColorSample_->setStyleSheet(QString(
        "QPushButton { background-color: %1; color: %2; font-weight: bold; "
        "border: 1px solid #555555; border-radius: 4px; padding: 4px; min-height: 24px; }")
        .arg(hex, textColor));
}

void MainWindow::removalMethodChanged(int index)
{
    aiRemovalWidget_->setVisible(index == 0);
    colorRemovalWidget_->setVisible(index == 1);
}

void MainWindow::pickKeyColor()
{
    const QColor c = QColorDialog::getColor(
        keyColor_, this,
        TR("Escolher cor de fundo a remover", "Choose background color to remove"));
    if (c.isValid()) {
        keyColor_ = c;
        updateKeyColorButton();
    }
}

void MainWindow::autoDetectKeyColor()
{
    if (preview_->originalImage().isNull()) return;
    keyColor_ = ImagePostProcessor::detectCornerColor(preview_->originalImage());
    updateKeyColorButton();
    logEdit_->appendPlainText("[INFO] " + TR("Cor detectada dos cantos: ",
                                            "Corner color detected: ") + keyColor_.name().toUpper());
}

void MainWindow::removeByColor()
{
    if (preview_->originalImage().isNull()) {
        QMessageBox::warning(this, TR("Aviso", "Warning"),
            TR("Abra uma imagem primeiro.", "Open an image first."));
        return;
    }

    const int tol = colorToleranceSpin_->value();
    const bool contiguous = colorContiguousCheckBox_->isChecked();
    const bool pixelArt = colorPixelArtCheckBox_->isChecked();

    QImage removed = ImagePostProcessor::removeByColor(
        preview_->originalImage(), keyColor_, tol, contiguous, pixelArt);

    preview_->setResult(removed);
    updateResultDisplay();
    btnAutoCrop_->setEnabled(true);
    btnDragOut_->setEnabled(true);

    logEdit_->appendPlainText(QString("[OK] %1 (%2: %3, %4: %5%)")
        .arg(TR("Remoção por cor concluída", "Color removal completed"))
        .arg(TR("Cor", "Color"))
        .arg(keyColor_.name().toUpper())
        .arg(TR("Tolerância", "Tolerance"))
        .arg(tol));
}
