#include "mainwindow.h"
#include "i18n.h"
#include "maskeditcanvas.h"
#include "previewinspector.h"
#include "inferenceworker.h"
#include "batchtabcontroller.h"
#include "updatechecker.h"
#include "optionspanel.h"
#include "helpwidget.h"
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
#include <QDoubleSpinBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFrame>
#include <QSplitter>
#include <QApplication>
#include <QByteArray>
#include <QBuffer>
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
#include <QMenu>
#include <QAction>
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
    options.edgeRefinementIntensity =
        edgeRefinementSpin_->value() / 100.0f;
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
    options.stickerWidth = stickerWidthSpin_->value();
    options.stickerColor = stickerColor_;

    options.dropShadow = shadowCheckBox_->isChecked();
    options.shadowBlur = shadowBlurSpin_->value();
    options.shadowOffsetX = shadowOffsetXSpin_->value();
    options.shadowOffsetY = shadowOffsetYSpin_->value();
    options.shadowOpacity = shadowOpacitySpin_->value() / 100.0f;

    options.jpegQuality = exportQualitySpin_->value();
    options.webpQuality = exportQualitySpin_->value();
    options.pngCompression = pngCompressionSpin_->value();
    options.preserveMetadata = preserveMetadataCheckBox_->isChecked();
    options.resizeWidth = outputWidthSpin_->value();
    options.resizeHeight = outputHeightSpin_->value();
    options.rotationDegrees = rotationCombo_->currentData().toInt();

    options.chromaDespill = chromaDespillSpin_->value() / 100.0f;
    options.chromaFeather = chromaFeatherSpin_->value();

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
      edgeRefinementSpin_(new QDoubleSpinBox),
      noiseCheckBox_(new QCheckBox(TR("Limpar ruído", "Clean noise"))),
      stickerCheckBox_(new QCheckBox(TR("Contorno (Sticker)", "Sticker outline"))),
      stickerWidthSpin_(new QSpinBox),
      stickerColorButton_(new QPushButton),
      shadowCheckBox_(new QCheckBox(TR("Sombra suave", "Drop shadow"))),
      shadowBlurSpin_(new QSpinBox),
      shadowOffsetXSpin_(new QSpinBox),
      shadowOffsetYSpin_(new QSpinBox),
      shadowOpacitySpin_(new QDoubleSpinBox),
      removalMethodCombo_(new QComboBox),
      aiRemovalWidget_(new QWidget),
      colorRemovalWidget_(new QWidget),
      btnKeyColorSample_(new QPushButton),
      btnAutoDetectKeyColor_(new QPushButton(TR("Detectar fundo", "Detect background"))),
      colorToleranceSpin_(new QSpinBox),
      colorContiguousCheckBox_(new QCheckBox(TR("Apenas conectado (Varinha)", "Connected only (Wand)"))),
      colorPixelArtCheckBox_(new QCheckBox(TR("Modo Pixel Art (bordas nítidas)", "Pixel Art mode (sharp edges)"))),
      chromaDespillSpin_(new QDoubleSpinBox),
      chromaFeatherSpin_(new QSpinBox),
      btnRemoveByColor_(new QPushButton(TR("Remover cor de fundo", "Remove background color"))),
      keyColor_(QColor(255, 0, 255)),
      saveFormatCombo_(new QComboBox),
      exportQualitySpin_(new QSpinBox),
      pngCompressionSpin_(new QSpinBox),
      preserveMetadataCheckBox_(new QCheckBox(
          TR("Preservar metadados (EXIF)", "Preserve metadata (EXIF)"))),
      outputWidthSpin_(new QSpinBox),
      outputHeightSpin_(new QSpinBox),
      rotationCombo_(new QComboBox),
      progressBar_(new QProgressBar),
      logEdit_(new QPlainTextEdit),
      maskEditCanvas_(new MaskEditCanvas(this)),
      zoomDebounce_(new QTimer(this)),
      customBgColor_(QColor()),
      // Ordem igual a da declaracao na classe (evita -Wreorder).
      // Ordem igual a da declaracao na classe (evita -Wreorder).
      inference_(new InferenceWorker(this)),
      modelCache_(inference_->cache()),
      preview_(new PreviewInspector(this)),
      batch_(new BatchTabController(this)),
      updateChecker_(new UpdateChecker(this))
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

    edgeRefinementSpin_->setRange(0, 100);
    edgeRefinementSpin_->setSingleStep(5);
    edgeRefinementSpin_->setValue(50);
    edgeRefinementSpin_->setSuffix(" %");
    edgeRefinementSpin_->setToolTip(TR(
        "Intensidade do refinamento das bordas",
        "Edge refinement intensity"));

    stickerWidthSpin_->setRange(1, 50);
    stickerWidthSpin_->setValue(6);
    stickerWidthSpin_->setSuffix(" px");
    stickerColorButton_->setObjectName("stickerColorButton");
    stickerColorButton_->setCursor(Qt::PointingHandCursor);
    stickerColorButton_->setToolTip(TR(
        "Cor do contorno do sticker", "Sticker outline color"));

    shadowBlurSpin_->setRange(0, 80);
    shadowBlurSpin_->setValue(10);
    shadowBlurSpin_->setSuffix(" px");
    shadowOffsetXSpin_->setRange(-100, 100);
    shadowOffsetXSpin_->setValue(4);
    shadowOffsetXSpin_->setSuffix(" px");
    shadowOffsetYSpin_->setRange(-100, 100);
    shadowOffsetYSpin_->setValue(6);
    shadowOffsetYSpin_->setSuffix(" px");
    shadowOpacitySpin_->setRange(0, 100);
    shadowOpacitySpin_->setSingleStep(5);
    shadowOpacitySpin_->setValue(50);
    shadowOpacitySpin_->setSuffix(" %");

    exportQualitySpin_->setRange(1, 100);
    exportQualitySpin_->setValue(90);
    exportQualitySpin_->setSuffix(" %");
    exportQualitySpin_->setToolTip(TR(
        "Qualidade de JPEG e WebP", "JPEG and WebP quality"));
    pngCompressionSpin_->setRange(0, 9);
    pngCompressionSpin_->setValue(6);
    pngCompressionSpin_->setToolTip(TR(
        "Nível de compressão PNG (0 sem compressão, 9 máxima)",
        "PNG compression level (0 none, 9 maximum)"));
    preserveMetadataCheckBox_->setChecked(true);
    preserveMetadataCheckBox_->setToolTip(TR(
        "Copia EXIF, GPS e texto para JPEG e PNG",
        "Copy EXIF, GPS and text metadata to JPEG and PNG"));

    outputWidthSpin_->setRange(0, 20000);
    outputHeightSpin_->setRange(0, 20000);
    outputWidthSpin_->setSpecialValueText(TR("Original", "Original"));
    outputHeightSpin_->setSpecialValueText(TR("Original", "Original"));
    outputWidthSpin_->setSuffix(" px");
    outputHeightSpin_->setSuffix(" px");
    rotationCombo_->addItem(TR("0°", "0°"), 0);
    rotationCombo_->addItem(TR("90°", "90°"), 90);
    rotationCombo_->addItem(TR("180°", "180°"), 180);
    rotationCombo_->addItem(TR("270°", "270°"), 270);

    chromaDespillSpin_->setRange(0, 100);
    chromaDespillSpin_->setSingleStep(5);
    chromaDespillSpin_->setValue(0);
    chromaDespillSpin_->setSuffix(" %");
    chromaDespillSpin_->setToolTip(TR(
        "Remove a cor de fundo das bordas semi-transparentes",
        "Removes background color from semi-transparent edges"));
    chromaFeatherSpin_->setRange(0, 5);
    chromaFeatherSpin_->setValue(0);
    chromaFeatherSpin_->setSuffix(" px");

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
    stickerWidthSpin_->setObjectName("stickerWidth");
    shadowCheckBox_->setObjectName("shadowCheckBox");
    shadowBlurSpin_->setObjectName("shadowBlur");
    shadowOffsetXSpin_->setObjectName("shadowOffsetX");
    shadowOffsetYSpin_->setObjectName("shadowOffsetY");
    shadowOpacitySpin_->setObjectName("shadowOpacity");
    refineCheckBox_->setObjectName("refineEdges");
    edgeRefinementSpin_->setObjectName("edgeRefinementIntensity");
    noiseCheckBox_->setObjectName("cleanNoise");
    noiseThresholdSpin_->setObjectName("noiseThreshold");
    exportQualitySpin_->setObjectName("exportQuality");
    pngCompressionSpin_->setObjectName("pngCompression");
    preserveMetadataCheckBox_->setObjectName("preserveMetadata");
    outputWidthSpin_->setObjectName("outputWidth");
    outputHeightSpin_->setObjectName("outputHeight");
    rotationCombo_->setObjectName("rotation");
    chromaDespillSpin_->setObjectName("chromaDespill");
    chromaFeatherSpin_->setObjectName("chromaFeather");
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
    // Nomes de idioma sempre na propria lingua (pratica padrao). Um
    // idioma externo so aparece quando existe um dicionario embutido ou
    // instalado; PT/EN estao sempre disponiveis.
    langCombo_->addItem("\U0001F310 " + I18n::nativeName(I18n::Lang::PtBr), "pt");
    langCombo_->addItem("\U0001F310 " + I18n::nativeName(I18n::Lang::En), "en");
    if (I18n::dictionaryAvailable(I18n::Lang::Es)) {
        langCombo_->addItem(
            "\U0001F310 " + I18n::nativeName(I18n::Lang::Es), "es");
    }
    langCombo_->setToolTip(
        TR("Idioma (aplicado ao reiniciar o aplicativo)",
           "Language (applies after restarting the app)"));
    const int activeLanguage = langCombo_->findData(
        I18n::code(I18n::lang()));
    langCombo_->setCurrentIndex(activeLanguage >= 0 ? activeLanguage : 0);
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
            this, [this](int) {
        QSettings settings("aletheia", "aletheia");
        settings.setValue("lang", langCombo_->currentData().toString());
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
    previewSplitter->setObjectName("previewSplitter");
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

    // O painel de opcoes (inspector) e construido pelo OptionsPanel:
    // o MainWindow e dono dos widgets, mas o layout vive fora daqui.
    OptionWidgets ow;
    ow.removalMethodCombo = removalMethodCombo_;
    ow.modelCombo = modelCombo_;
    ow.modelPathEdit = modelPathEdit_;
    ow.btnLoadModel = btnLoadModel_;
    ow.btnRemove = btnRemove_;
    ow.aiRemovalWidget = aiRemovalWidget_;
    ow.colorRemovalWidget = colorRemovalWidget_;
    ow.btnKeyColorSample = btnKeyColorSample_;
    ow.btnAutoDetectKeyColor = btnAutoDetectKeyColor_;
    ow.colorToleranceSpin = colorToleranceSpin_;
    ow.colorContiguousCheckBox = colorContiguousCheckBox_;
    ow.colorPixelArtCheckBox = colorPixelArtCheckBox_;
    ow.chromaDespillSpin = chromaDespillSpin_;
    ow.chromaFeatherSpin = chromaFeatherSpin_;
    ow.btnRemoveByColor = btnRemoveByColor_;
    ow.presetCombo = presetCombo_;
    ow.bgColorCombo = bgColorCombo_;
    ow.saveFormatCombo = saveFormatCombo_;
    ow.exportQualitySpin = exportQualitySpin_;
    ow.pngCompressionSpin = pngCompressionSpin_;
    ow.preserveMetadataCheckBox = preserveMetadataCheckBox_;
    ow.outputWidthSpin = outputWidthSpin_;
    ow.outputHeightSpin = outputHeightSpin_;
    ow.rotationCombo = rotationCombo_;
    ow.refineCheckBox = refineCheckBox_;
    ow.edgeRefinementSpin = edgeRefinementSpin_;
    ow.noiseCheckBox = noiseCheckBox_;
    ow.noiseThresholdSpin = noiseThresholdSpin_;
    ow.stickerCheckBox = stickerCheckBox_;
    ow.stickerWidthSpin = stickerWidthSpin_;
    ow.stickerColorButton = stickerColorButton_;
    ow.shadowCheckBox = shadowCheckBox_;
    ow.shadowBlurSpin = shadowBlurSpin_;
    ow.shadowOffsetXSpin = shadowOffsetXSpin_;
    ow.shadowOffsetYSpin = shadowOffsetYSpin_;
    ow.shadowOpacitySpin = shadowOpacitySpin_;
    ow.brushModeCombo = brushModeCombo_;
    ow.brushRadiusSpin = brushRadiusSpin_;
    ow.btnUndo = btnUndo_;
    ow.btnRedo = btnRedo_;
    ow.btnAutoCrop = btnAutoCrop_;
    ow.btnDragOut = btnDragOut_;
    QWidget* inspectorScroll = OptionsPanel::buildInspector(
        ow, static_cast<int>(kAlphaThreshold), this);

    auto* contentSplitter = new QSplitter(Qt::Horizontal);
    contentSplitter->setObjectName("contentSplitter");
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
                this, [this](int ok, int errors) {
            emit jobFinished();
            maybeCloseAfterJobs();
            // Resumo visual no fim do lote (so se nao foi cancelado
            // durante o fecho da janela).
            if (!closeRequested_) {
                if (errors > 0) {
                    QMessageBox::warning(this,
                        TR("Lote concluído com erros", "Batch finished with errors"),
                        TR("Processadas: %1 OK, %2 com erro.\nVeja o log para detalhes.",
                           "Processed: %1 OK, %2 failed.\nSee the log for details.")
                            .arg(ok).arg(errors));
                } else if (ok > 0) {
                    QMessageBox::information(this,
                        TR("Lote concluído", "Batch completed"),
                        TR("%1 imagens processadas com sucesso.",
                           "%1 images processed successfully.").arg(ok));
                }
            }
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
    setMinimumSize(900, 600);
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
        scheduleEffectsReapply();
    });
    connect(shadowCheckBox_,   &QCheckBox::toggled, this, [this](bool) {
        if (presetCombo_->currentIndex() != 0) presetCombo_->setCurrentIndex(0);
        scheduleEffectsReapply();
    });
    connect(stickerWidthSpin_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int) { scheduleEffectsReapply(); });
    connect(shadowBlurSpin_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int) { scheduleEffectsReapply(); });
    connect(shadowOffsetXSpin_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int) { scheduleEffectsReapply(); });
    connect(shadowOffsetYSpin_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int) { scheduleEffectsReapply(); });
    connect(shadowOpacitySpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double) { scheduleEffectsReapply(); });
    connect(refineCheckBox_, &QCheckBox::toggled, this, [this](bool) {
        if (presetCombo_->currentIndex() != 0) presetCombo_->setCurrentIndex(0);
        scheduleEffectsReapply();
    });
    connect(edgeRefinementSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double) { scheduleEffectsReapply(); });
    connect(noiseCheckBox_, &QCheckBox::toggled, this, [this](bool) {
        if (presetCombo_->currentIndex() != 0) presetCombo_->setCurrentIndex(0);
        scheduleEffectsReapply();
    });
    connect(noiseThresholdSpin_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int) { scheduleEffectsReapply(); });
    connect(&effectsWatcher_, &QFutureWatcher<QImage>::finished,
            this, &MainWindow::onEffectsReapplied);
    connect(stickerColorButton_, &QPushButton::clicked,
            this, &MainWindow::pickStickerColor);

    updateKeyColorButton();
    updateStickerColorButton();
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
    connect(&colorWatcher_, &QFutureWatcher<QPair<QImage, quint64>>::finished,
            this,            &MainWindow::onColorRemovalFinished);
    connect(&saveWatcher_, &QFutureWatcher<QImage>::finished,
            this,            &MainWindow::onSaveProcessingFinished);
    connect(&postProcessWatcher_, &QFutureWatcher<QImage>::finished,
            this,            &MainWindow::onPostProcessFinished);
    connect(&autoCropWatcher_, &QFutureWatcher<QImage>::finished,
            this,            &MainWindow::onAutoCropFinished);

    // Atalhos de teclado.
    auto* scOpen = new QShortcut(QKeySequence::Open, this);
    connect(scOpen, &QShortcut::activated, this, &MainWindow::openImage);
    auto* scSave = new QShortcut(QKeySequence::Save, this);
    connect(scSave, &QShortcut::activated, this, &MainWindow::saveResult);
    auto* scCopy = new QShortcut(QKeySequence("Ctrl+Shift+C"), this);
    connect(scCopy, &QShortcut::activated, this, &MainWindow::copyResult);
    // Ctrl+C tambem copia o resultado (documentado no README).
    auto* scCopyPlain = new QShortcut(QKeySequence::Copy, this);
    connect(scCopyPlain, &QShortcut::activated, this, &MainWindow::copyResult);
    // Ctrl+D executa a remocao de fundo (atalho documentado no README).
    auto* scRemove = new QShortcut(QKeySequence("Ctrl+D"), this);
    connect(scRemove, &QShortcut::activated, this, &MainWindow::removeBackground);
    // F1 abre a aba de Ajuda (procura pelo objectName do HelpWidget,
    // nao depende da ordem das abas).
    auto* scHelp = new QShortcut(QKeySequence("F1"), this);
    connect(scHelp, &QShortcut::activated, this, [this]() {
        if (!mainTabs_) return;
        for (int i = 0; i < mainTabs_->count(); ++i) {
            if (mainTabs_->widget(i)
                    && mainTabs_->widget(i)->findChild<QWidget*>("helpView")) {
                mainTabs_->setCurrentIndex(i);
                return;
            }
        }
    });
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
    // restoreSettings pode ter restaurado a cor alvo: atualizar o botao.
    updateKeyColorButton();

    // Ação de toggle para visualização de máscara alfa-only
    actionShowMaskPreview_ = new QAction(TR("Mostrar máscara alfa", "Show alpha mask"), this);
    actionShowMaskPreview_->setCheckable(true);
    actionShowMaskPreview_->setShortcut(QKeySequence("Ctrl+M"));
    actionShowMaskPreview_->setToolTip(TR("Alternar visualização da máscara alfa (alpha-only)", "Toggle alpha mask visualization (alpha-only)"));
    connect(actionShowMaskPreview_, &QAction::toggled, this, [this](bool checked){
        showMaskPreview_ = checked;
        updateResultDisplay();
    });
    // Menu Ficheiro: abrir, recentes, sair.
    QMenu* fileMenu = menuBar()->addMenu(TR("Ficheiro", "File"));
    QAction* actOpen = fileMenu->addAction(TR("Abrir...", "Open..."));
    actOpen->setShortcut(QKeySequence::Open);
    connect(actOpen, &QAction::triggered, this, &MainWindow::openImage);
    recentMenu_ = fileMenu->addMenu(TR("Abrir recente", "Open recent"));
    fileMenu->addSeparator();
    QAction* actQuit = fileMenu->addAction(TR("Sair", "Quit"));
    actQuit->setShortcut(QKeySequence::Quit);
    connect(actQuit, &QAction::triggered, this, &QWidget::close);
    rebuildRecentMenu();

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
    connect(updateChecker_, &UpdateChecker::updateAvailable,
            this, &MainWindow::showUpdateBanner);
    updateChecker_->check();
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

void MainWindow::updateWindowTitle()
{
    const QString base = QStringLiteral("Aletheia %1").arg(ALETHEIA_VERSION);
    if (currentImagePath_.isEmpty()) {
        setWindowTitle(base);
    } else {
        setWindowTitle(base + " — " + QFileInfo(currentImagePath_).fileName());
    }
}

void MainWindow::addToRecentFiles(const QString& path)
{
    // Remove duplicados e coloca no topo; limita a 10.
    recentFiles_.removeAll(path);
    recentFiles_.prepend(path);
    while (recentFiles_.size() > 10)
        recentFiles_.removeLast();
    rebuildRecentMenu();
    // Persiste para a proxima sessao.
    QSettings settings("aletheia", "aletheia");
    settings.setValue("recent/files", recentFiles_);
}

void MainWindow::rebuildRecentMenu()
{
    if (!recentMenu_) return;
    recentMenu_->clear();
    if (recentFiles_.isEmpty()) {
        QAction* none = recentMenu_->addAction(
            TR("(vazio)", "(empty)"));
        none->setEnabled(false);
        return;
    }
    for (const QString& f : recentFiles_) {
        if (!QFile::exists(f)) continue; // ficheiro ja nao existe
        QAction* act = recentMenu_->addAction(QFileInfo(f).fileName());
        act->setToolTip(f);
        act->setData(f);
        connect(act, &QAction::triggered, this, &MainWindow::openRecentFile);
    }
    recentMenu_->addSeparator();
    QAction* clear = recentMenu_->addAction(TR("Limpar lista", "Clear list"));
    connect(clear, &QAction::triggered, this, [this]() {
        recentFiles_.clear();
        QSettings("aletheia", "aletheia").setValue("recent/files", recentFiles_);
        rebuildRecentMenu();
    });
}

void MainWindow::openRecentFile()
{
    auto* act = qobject_cast<QAction*>(sender());
    if (!act) return;
    const QString path = act->data().toString();
    if (!path.isEmpty() && QFile::exists(path))
        openPath(path);
    else
        QMessageBox::warning(this, TR("Erro", "Error"),
            TR("O ficheiro já não existe:\n", "The file no longer exists:\n") + path);
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
        QString hint;
        ImagePostProcessor::isFormatSupported(path, &hint);
        QString msg = TR("Nao foi possivel carregar:\n", "Could not load:\n")
                + path;
        if (!hint.isEmpty()) {
            const QString suffix = QFileInfo(path).suffix().toLower();
            if (ImagePostProcessor::pluginDependentExtensions().contains(suffix)) {
                msg += TR("\n\nO formato .%1 precisa do pacote "
                          "'qt6-image-formats-plugins' (HEIC/AVIF/WebP).",
                          "\n\nThe .%1 format requires the "
                          "'qt6-image-formats-plugins' package (HEIC/AVIF/WebP).")
                          .arg(suffix.toUpper());
            } else {
                msg += "\n\n" + hint;
            }
        }
        QMessageBox::warning(this, TR("Erro", "Error"), msg);
        return;
    }
    // Aviso para imagens muito grandes: o processamento usa memoria
    // proporcional a w*h (máscara, flood-fill, undo stack).
    const qint64 megapixels =
        static_cast<qint64>(img.width()) * img.height() / 1000000;
    if (megapixels > 50) {
        QMessageBox::warning(this, TR("Imagem grande", "Large image"),
            TR("Esta imagem tem %1 MP. O processamento pode demorar "
               "e usar muita memória RAM. Considere redimensionar antes.",
               "This image is %1 MP. Processing may be slow and use "
               "a lot of RAM. Consider resizing it first.")
                .arg(megapixels));
    }

    ++imageGeneration_;
    preview_->clearResult();
    maskBaseImage_ = QImage();
    btnAutoCrop_->setEnabled(false);
    btnDragOut_->setEnabled(false);
    preview_->setOriginal(img);
    currentImagePath_ = path;
    // Nome + tamanho do ficheiro na barra de ferramentas.
    {
        const qint64 sizeBytes = QFileInfo(path).size();
        QString sizeStr;
        if (sizeBytes >= 1024 * 1024)
            sizeStr = QStringLiteral("%1 MB").arg(sizeBytes / 1048576.0, 0, 'f', 1);
        else if (sizeBytes >= 1024)
            sizeStr = QStringLiteral("%1 KB").arg(sizeBytes / 1024.0, 0, 'f', 1);
        else
            sizeStr = QStringLiteral("%1 B").arg(sizeBytes);
        imageNameLabel_->setText(
            QStringLiteral("%1  (%2)").arg(QFileInfo(path).fileName(), sizeStr));
    }
    updateWindowTitle();
    addToRecentFiles(path);
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
        // Cancela incondicionalmente: o worker trata do caso de o modelo
        // ainda estar a carregar (flag cancelRequested).
        inference_->cancel();
        logEdit_->appendPlainText("[INFO] " + TR("Cancelamento solicitado.", "Cancellation requested."));
        return;
    }
    // Bloquear se outro job estiver ativo (lote, cor, save, copy, crop).
    if (isBusy_ || batch_->isRunning() || colorWatcher_.isRunning()
        || saveWatcher_.isRunning() || postProcessWatcher_.isRunning()
        || autoCropWatcher_.isRunning()) {
        if (batch_->isRunning()) {
            batch_->requestCancel();
            logEdit_->appendPlainText("[INFO] " + TR("Cancelamento do lote solicitado.", "Batch cancellation requested."));
        }
        return;
    }

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
    // Guarda a mascara crua como base para o live preview; aplica os
    // efeitos de mascara (sticker/sombra/limiar/refinamento) no preview.
    maskBaseImage_ = res.image;
    {
        ProcessingOptions fx = currentProcessingOptions();
        fx.backgroundStyle = ProcessingOptions::BgTransparent;
        fx.backgroundColor = QColor();
        preview_->setResult(
            ImagePostProcessor::applyMaskEffects(res.image, fx,
                                                 preview_->originalImage()));
    }
    updateResultDisplay();
    btnAutoCrop_->setEnabled(true);
    btnDragOut_->setEnabled(true);
    logEdit_->appendPlainText("[OK] " + TR("Fundo removido.", "Background removed."));
    // Mostra o provider e a resolucao de entrada do modelo usado (GPU vs
    // CPU fica visivel no log; o BackgroundRemover guarda-os em cache).
    {
        QMutexLocker lock(&modelCache_->mutex);
        if (modelCache_->remover) {
            logEdit_->appendPlainText(
                QString("[INFO] %1: %2 · %3×%4")
                    .arg(TR("Modelo", "Model"),
                         modelCache_->remover->provider(),
                         QString::number(modelCache_->remover->inputSize()),
                         QString::number(modelCache_->remover->inputSize())));
        }
    }
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
    saveOptions.outputFormat = extension;
    // Formatos sem canal alfa precisam de fundo solido, senao o Qt
    // compoe sobre preto e o resultado surpreende.
    const bool needSolidBg = currentBackgroundColor().isValid()
        || !saveFormatIsAlphaCapable();
    if (needSolidBg && saveOptions.backgroundStyle == ProcessingOptions::BgTransparent) {
        saveOptions.backgroundStyle = ProcessingOptions::BgSolidColor;
        saveOptions.backgroundColor = Qt::white;
    }

    // O pós-processamento e a escrita (sticker, sombra, geometria,
    // qualidade e metadados) correm na thread de trabalho para não
    // congelar a interface com imagens grandes.
    pendingSavePath_ = path;
    const QString sourcePath = currentImagePath_;
    setUiBusy(true);
    progressBar_->setRange(0, 0);
    progressBar_->setValue(0);
    progressBar_->setVisible(true);
    btnSave_->setEnabled(false);
    logEdit_->appendPlainText(
        "[INFO] " + TR("A processar imagem para salvar...",
                       "Processing image for save..."));
    saveWatcher_.setFuture(QtConcurrent::run(
        [img = preview_->resultImage(), opts = saveOptions,
         outPath = path,
         source = sourcePath]() -> SaveOutcome {
            // O resultado do preview ja tem os efeitos de mascara
            // (sticker/sombra/limiar) aplicados pelo worker; no export
            // aplicamos apenas a geometria (crop/rotacao/resize) para
            // nao duplicar os efeitos.
            const QImage processed =
                ImagePostProcessor::applyGeometry(img, opts);
            QString error;
            const bool success = ImagePostProcessor::saveImage(
                processed, outPath, opts, source, &error);
            return { success, error };
        }));
}

void MainWindow::onSaveProcessingFinished()
{
    progressBar_->setVisible(false);
    progressBar_->setRange(0, 100);
    progressBar_->setValue(0);
    setUiBusy(false);
    btnSave_->setEnabled(true);

    const SaveOutcome outcome = saveWatcher_.result();
    const QString path = pendingSavePath_;

    QSettings settings("aletheia", "aletheia");
    if (!outcome.success) {
        QMessageBox::critical(this, TR("Erro", "Error"),
            TR("Não foi possível salvar:\n", "Could not save:\n") + path + "\n" + outcome.error);
    } else {
        settings.setValue("output/lastSaveDir",
                          QFileInfo(path).absolutePath());
        QMessageBox::information(this, TR("OK", "OK"),
            TR("Salvo:\n", "Saved:\n") + path);
        logEdit_->appendPlainText(
            "[OK] " + TR("Imagem salva: ", "Image saved: ") + path);
    }
    emit jobFinished();
    maybeCloseAfterJobs();
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
    // O pós-processamento (sticker/sombra) corre na thread de trabalho
    // para nao congelar a UI; a copia acontece em onPostProcessFinished().
    pendingAction_ = PendingAction::CopyToClipboard;
    pendingPostOptions_ = currentProcessingOptions();
    setUiBusy(true);
    progressBar_->setRange(0, 0);
    progressBar_->setVisible(true);
    postProcessWatcher_.setFuture(QtConcurrent::run(
        [img = preview_->resultImage(), opts = pendingPostOptions_]() {
            // Resultado do preview ja tem efeitos de mascara; so a
            // geometria (crop/rotacao/resize) falta aplicar.
            return ImagePostProcessor::applyGeometry(img, opts);
        }));
}

void MainWindow::onPostProcessFinished()
{
    progressBar_->setVisible(false);
    progressBar_->setRange(0, 100);
    setUiBusy(false);

    const QImage result = postProcessWatcher_.result();
    const PendingAction action = pendingAction_;
    pendingAction_ = PendingAction::None;

    if (action == PendingAction::CopyToClipboard) {
        QApplication::clipboard()->setImage(result);
        logEdit_->appendPlainText(
            "[OK] " + TR("Resultado copiado para a area de transferencia.",
                         "Result copied to the clipboard."));
    } else if (action == PendingAction::CopyTransparent) {
        const QImage image = result.convertToFormat(QImage::Format_RGBA8888);
        QByteArray pngData;
        QBuffer buffer(&pngData);
        buffer.open(QIODevice::WriteOnly);
        QImageWriter writer(&buffer, "PNG");
        if (!writer.write(image)) {
            QMessageBox::warning(this, TR("Aviso", "Warning"),
                TR("Falha ao codificar PNG para a área de transferência.",
                   "Failed to encode PNG for clipboard."));
        } else {
            QMimeData* mime = new QMimeData;
            mime->setData("image/png", pngData);
            mime->setImageData(image);
            QApplication::clipboard()->setMimeData(mime);
            logEdit_->appendPlainText(
                "[OK] " + TR("PNG transparente copiado para a área de transferência.",
                             "Transparent PNG copied to clipboard."));
        }
    } else if (action == PendingAction::DragOut) {
        const QString tempDir = QDir::tempPath();
        const QString baseName = currentImagePath_.isEmpty()
            ? QStringLiteral("bg_removed")
            : QFileInfo(currentImagePath_).completeBaseName()
                  + QStringLiteral("_rembg");
        const QString tempPath = QDir(tempDir).filePath(
            baseName + QStringLiteral(".png"));
        result.save(tempPath, "PNG");

        auto* drag = new QDrag(this);
        auto* mimeData = new QMimeData;
        mimeData->setImageData(result);
        mimeData->setUrls({QUrl::fromLocalFile(tempPath)});
        drag->setMimeData(mimeData);
        const QPixmap preview = QPixmap::fromImage(
            result.scaled(128, 128, Qt::KeepAspectRatio,
                          Qt::SmoothTransformation));
        drag->setPixmap(preview);
        drag->setHotSpot(QPoint(preview.width() / 2, preview.height() / 2));
        drag->exec(Qt::CopyAction);
    }
    emit jobFinished();
    maybeCloseAfterJobs();
}

void MainWindow::copyTransparentResult()
{
    if (isBusy_) return;
    if (preview_->resultImage().isNull()) {
        QMessageBox::warning(this, TR("Aviso", "Warning"),
            TR("Remova o fundo primeiro.", "Remove the background first."));
        return;
    }

    // O mesmo pipeline de exportacao e usado, mas sem fundo: sticker,
    // sombra, limpeza e geometria sao aplicados uma unica vez.
    ProcessingOptions options = currentProcessingOptions();
    options.backgroundStyle = ProcessingOptions::BgTransparent;
    options.backgroundColor = QColor();
    pendingAction_ = PendingAction::CopyTransparent;
    pendingPostOptions_ = options;
    setUiBusy(true);
    progressBar_->setRange(0, 0);
    progressBar_->setValue(0);
    progressBar_->setVisible(true);
    postProcessWatcher_.setFuture(QtConcurrent::run(
        [img = preview_->resultImage(), opts = pendingPostOptions_]() {
            return ImagePostProcessor::applyGeometry(img, opts);
        }));
}

void MainWindow::onSaveFormatChanged(int index)
{
    // PNG e WebP guardam transparencia; JPEG funde sobre o fundo
    // escolhido. A qualidade aplica-se a JPEG/WebP e a compressao a PNG.
    exportQualitySpin_->setEnabled(!isBusy_ && index != 0);
    pngCompressionSpin_->setEnabled(!isBusy_ && index == 0);
}

void MainWindow::showUpdateBanner(const QString& version)
{
    if (version.isEmpty()) return;

    // Banner clicavel no topo do workspace (widget do UpdateChecker).
    QLabel* banner = updateChecker_->banner();
    const QUrl releasesUrl(QStringLiteral(
        "https://github.com/MarielCarlosFrias/aletheia/releases"));
    banner->setText(
        TR("⬆ <a href=\"%1\" style=\"color:#102217;\">"
           "Nova versão %2 disponível — clique para abrir as releases"
           "</a>",
           "⬆ <a href=\"%1\" style=\"color:#102217;\">"
           "New version %2 available — click to open releases"
           "</a>")
            .arg(releasesUrl.toString(), version));
    banner->setVisible(true);

    // Inserido antes do divisor de preview (posicao 0 do workspace).
    if (auto* wl = workspaceLayout()) {
        if (wl->indexOf(banner) < 0)
            wl->insertWidget(0, banner);
    }

    // Evita ligacoes duplicadas se o banner for mostrado mais de uma vez.
    QObject::disconnect(banner, nullptr, this, nullptr);
    connect(banner, &QLabel::linkActivated, this,
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
    edgeRefinementSpin_->setEnabled(!busy);
    noiseCheckBox_->setEnabled(!busy);
    noiseThresholdSpin_->setEnabled(!busy);
    stickerCheckBox_->setEnabled(!busy);
    stickerWidthSpin_->setEnabled(!busy);
    stickerColorButton_->setEnabled(!busy);
    shadowCheckBox_->setEnabled(!busy);
    shadowBlurSpin_->setEnabled(!busy);
    shadowOffsetXSpin_->setEnabled(!busy);
    shadowOffsetYSpin_->setEnabled(!busy);
    shadowOpacitySpin_->setEnabled(!busy);
    exportQualitySpin_->setEnabled(!busy && saveFormatCombo_->currentIndex() != 0);
    pngCompressionSpin_->setEnabled(!busy && saveFormatCombo_->currentIndex() == 0);
    outputWidthSpin_->setEnabled(!busy);
    outputHeightSpin_->setEnabled(!busy);
    rotationCombo_->setEnabled(!busy);
    removalMethodCombo_->setEnabled(!busy);
    btnKeyColorSample_->setEnabled(!busy);
    btnAutoDetectKeyColor_->setEnabled(!busy && !preview_->originalImage().isNull());
    colorToleranceSpin_->setEnabled(!busy);
    colorContiguousCheckBox_->setEnabled(!busy);
    colorPixelArtCheckBox_->setEnabled(!busy);
    chromaDespillSpin_->setEnabled(!busy);
    chromaFeatherSpin_->setEnabled(!busy);
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
    // Restaura os divisores (preview original/resultado e
    // workspace/inspector) para o utilizador nao ter de os redimensionar
    // em todas as sessoes.
    if (auto* ps = findChild<QSplitter*>("previewSplitter")) {
        const QByteArray st = settings.value("ui/previewSplitter").toByteArray();
        if (!st.isEmpty()) ps->restoreState(st);
    }
    if (auto* cs = findChild<QSplitter*>("contentSplitter")) {
        const QByteArray st = settings.value("ui/contentSplitter").toByteArray();
        if (!st.isEmpty()) cs->restoreState(st);
    }

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
    {
        const QSignalBlocker blocker(brushModeCombo_);
        const int bm = settings.value("brush/mode", 0).toInt();
        if (bm >= 0 && bm < brushModeCombo_->count())
            brushModeCombo_->setCurrentIndex(bm);
    }
    refineCheckBox_->setChecked(
        settings.value("options/refineEdges", false).toBool());
    noiseCheckBox_->setChecked(
        settings.value("options/cleanNoise", false).toBool());
    noiseThresholdSpin_->setValue(
        settings.value("options/noiseThreshold", kAlphaThreshold).toInt());
    edgeRefinementSpin_->setValue(
        settings.value("options/edgeRefinementIntensity", 50).toInt());
    stickerCheckBox_->setChecked(
        settings.value("options/sticker", false).toBool());
    stickerWidthSpin_->setValue(
        settings.value("options/stickerWidth", 6).toInt());
    {
        const QVariant stickerColor = settings.value("options/stickerColor");
        if (stickerColor.isValid()) {
            const QColor color = stickerColor.value<QColor>();
            if (color.isValid()) stickerColor_ = color;
        }
    }
    shadowCheckBox_->setChecked(
        settings.value("options/shadow", false).toBool());
    shadowBlurSpin_->setValue(
        settings.value("options/shadowBlur", 10).toInt());
    shadowOffsetXSpin_->setValue(
        settings.value("options/shadowOffsetX", 4).toInt());
    shadowOffsetYSpin_->setValue(
        settings.value("options/shadowOffsetY", 6).toInt());
    shadowOpacitySpin_->setValue(
        settings.value("options/shadowOpacity", 50).toInt());
    {
        const QSignalBlocker blocker(removalMethodCombo_);
        const int rm = settings.value("removal/method", 0).toInt();
        if (rm >= 0 && rm < removalMethodCombo_->count())
            removalMethodCombo_->setCurrentIndex(rm);
    }
    {
        const QSignalBlocker blocker(presetCombo_);
        const int ps = settings.value("output/preset", 0).toInt();
        if (ps >= 0 && ps < presetCombo_->count())
            presetCombo_->setCurrentIndex(ps);
    }
    colorToleranceSpin_->setValue(
        settings.value("color/tolerance", 0).toInt());
    colorContiguousCheckBox_->setChecked(
        settings.value("color/contiguous", false).toBool());
    colorPixelArtCheckBox_->setChecked(
        settings.value("color/pixelArt", true).toBool());
    chromaDespillSpin_->setValue(
        settings.value("color/despill", 0).toInt());
    chromaFeatherSpin_->setValue(
        settings.value("color/feather", 0).toInt());
    {
        const QVariant keyVar = settings.value("color/keyColor");
        if (keyVar.isValid()) {
            const QColor c = keyVar.value<QColor>();
            if (c.isValid()) keyColor_ = c;
        }
    }
    {
        const QSignalBlocker blocker(saveFormatCombo_);
        const int fmt = settings.value("output/saveFormat", 0).toInt();
        if (fmt >= 0 && fmt < saveFormatCombo_->count())
            saveFormatCombo_->setCurrentIndex(fmt);
    }
    exportQualitySpin_->setValue(
        settings.value("output/quality", 90).toInt());
    pngCompressionSpin_->setValue(
        settings.value("output/pngCompression", 6).toInt());
    preserveMetadataCheckBox_->setChecked(
        settings.value("output/preserveMetadata", true).toBool());
    outputWidthSpin_->setValue(
        settings.value("output/width", 0).toInt());
    outputHeightSpin_->setValue(
        settings.value("output/height", 0).toInt());
    {
        const QSignalBlocker blocker(rotationCombo_);
        const int rotation = settings.value("output/rotation", 0).toInt();
        const int index = rotationCombo_->findData(rotation);
        if (index >= 0) rotationCombo_->setCurrentIndex(index);
    }

    // Lista de ficheiros recentes (menu Ficheiro > Abrir recente).
    recentFiles_ = settings.value("recent/files").toStringList();
    rebuildRecentMenu();

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
    if (auto* ps = findChild<QSplitter*>("previewSplitter"))
        settings.setValue("ui/previewSplitter", ps->saveState());
    if (auto* cs = findChild<QSplitter*>("contentSplitter"))
        settings.setValue("ui/contentSplitter", cs->saveState());
    settings.setValue("batch/inputDir", batch_->folderEdit()->text());
    settings.setValue("batch/outputDir", batch_->outputDirEdit()->text());
    settings.setValue("brush/radius", brushRadiusSpin_->value());
    settings.setValue("brush/mode", brushModeCombo_->currentIndex());
    settings.setValue("options/refineEdges",
                      refineCheckBox_->isChecked());
    settings.setValue("options/cleanNoise",
                      noiseCheckBox_->isChecked());
    settings.setValue("options/noiseThreshold",
                      noiseThresholdSpin_->value());
    settings.setValue("options/edgeRefinementIntensity",
                      edgeRefinementSpin_->value());
    settings.setValue("options/sticker", stickerCheckBox_->isChecked());
    settings.setValue("options/stickerWidth", stickerWidthSpin_->value());
    settings.setValue("options/stickerColor", stickerColor_);
    settings.setValue("options/shadow", shadowCheckBox_->isChecked());
    settings.setValue("options/shadowBlur", shadowBlurSpin_->value());
    settings.setValue("options/shadowOffsetX", shadowOffsetXSpin_->value());
    settings.setValue("options/shadowOffsetY", shadowOffsetYSpin_->value());
    settings.setValue("options/shadowOpacity", shadowOpacitySpin_->value());
    settings.setValue("removal/method", removalMethodCombo_->currentIndex());
    settings.setValue("output/preset", presetCombo_->currentIndex());
    settings.setValue("color/tolerance", colorToleranceSpin_->value());
    settings.setValue("color/contiguous", colorContiguousCheckBox_->isChecked());
    settings.setValue("color/pixelArt", colorPixelArtCheckBox_->isChecked());
    settings.setValue("color/despill", chromaDespillSpin_->value());
    settings.setValue("color/feather", chromaFeatherSpin_->value());
    settings.setValue("color/keyColor", keyColor_);
    settings.setValue("output/bgIndex", bgColorCombo_->currentIndex());
    settings.setValue("output/customColor", customBgColor_);
    settings.setValue("output/saveFormat",
                     saveFormatCombo_->currentIndex());
    settings.setValue("output/quality", exportQualitySpin_->value());
    settings.setValue("output/pngCompression", pngCompressionSpin_->value());
    settings.setValue("output/width", outputWidthSpin_->value());
    settings.setValue("output/height", outputHeightSpin_->value());
    settings.setValue("output/rotation", rotationCombo_->currentData().toInt());
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

    if (inference_->isRunning() || batch_->isRunning()
        || colorWatcher_.isRunning() || saveWatcher_.isRunning()
        || postProcessWatcher_.isRunning() || autoCropWatcher_.isRunning()) {
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
    if (inference_->isRunning() || batch_->isRunning()
        || colorWatcher_.isRunning() || saveWatcher_.isRunning()
        || postProcessWatcher_.isRunning() || autoCropWatcher_.isRunning()) return;

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
        updateWindowTitle();
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
    if (autoCropWatcher_.isRunning()) return;

    // contentBounds percorre todos os pixels: em fotos grandes isto
    // congelava a UI; agora corre na thread de trabalho.
    setUiBusy(true);
    progressBar_->setRange(0, 0);
    progressBar_->setVisible(true);
    autoCropWatcher_.setFuture(QtConcurrent::run(
        [img = preview_->resultImage()]() {
            return ImagePostProcessor::autoCrop(img, 2);
        }));
}

void MainWindow::onAutoCropFinished()
{
    progressBar_->setVisible(false);
    progressBar_->setRange(0, 100);
    setUiBusy(false);

    const QImage cropped = autoCropWatcher_.result();
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
    emit jobFinished();
    maybeCloseAfterJobs();
}

void MainWindow::presetChanged(int index)
{
    const QSignalBlocker bBg(bgColorCombo_);
    const QSignalBlocker bSticker(stickerCheckBox_);
    const QSignalBlocker bShadow(shadowCheckBox_);
    const QSignalBlocker bRefine(refineCheckBox_);
    const QSignalBlocker bNoise(noiseCheckBox_);
    const QSignalBlocker bNoiseThresh(noiseThresholdSpin_);
    const QSignalBlocker bEdge(edgeRefinementSpin_);
    const QSignalBlocker bStickerWidth(stickerWidthSpin_);
    const QSignalBlocker bShadowBlur(shadowBlurSpin_);
    const QSignalBlocker bShadowX(shadowOffsetXSpin_);
    const QSignalBlocker bShadowY(shadowOffsetYSpin_);
    const QSignalBlocker bShadowOpacity(shadowOpacitySpin_);

    // Valores base partilhados pelos presets. As opcoes especificas de
    // cada preset sao ajustadas no switch abaixo.
    edgeRefinementSpin_->setValue(50);
    stickerWidthSpin_->setValue(6);
    stickerColor_ = Qt::white;
    updateStickerColorButton();
    shadowBlurSpin_->setValue(10);
    shadowOffsetXSpin_->setValue(4);
    shadowOffsetYSpin_->setValue(6);
    shadowOpacitySpin_->setValue(50);

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
    if (isBusy_) return;
    if (preview_->resultImage().isNull()) return;

    ProcessingOptions dragOptions = currentProcessingOptions();
    if (!saveFormatIsAlphaCapable()
        && dragOptions.backgroundStyle == ProcessingOptions::BgTransparent) {
        dragOptions.backgroundStyle = ProcessingOptions::BgSolidColor;
        dragOptions.backgroundColor = Qt::white;
    }
    // Pós-processamento na thread de trabalho; o drag inicia em
    // onPostProcessFinished().
    pendingAction_ = PendingAction::DragOut;
    pendingPostOptions_ = dragOptions;
    setUiBusy(true);
    progressBar_->setRange(0, 0);
    progressBar_->setVisible(true);
    postProcessWatcher_.setFuture(QtConcurrent::run(
        [img = preview_->resultImage(), opts = dragOptions]() {
            return ImagePostProcessor::applyGeometry(img, opts);
        }));
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

void MainWindow::updateStickerColorButton()
{
    const QString hex = stickerColor_.name(QColor::HexRgb).toUpper();
    const double luminance =
        (0.299 * stickerColor_.red()
         + 0.587 * stickerColor_.green()
         + 0.114 * stickerColor_.blue()) / 255.0;
    const QString textColor = luminance > 0.5 ? "#000000" : "#FFFFFF";

    stickerColorButton_->setText(hex);
    stickerColorButton_->setStyleSheet(QString(
        "QPushButton { background-color: %1; color: %2; font-weight: bold; "
        "border: 1px solid #555555; border-radius: 4px; padding: 4px; "
        "min-height: 24px; }")
        .arg(hex, textColor));
}

void MainWindow::pickStickerColor()
{
    const QColor color = QColorDialog::getColor(
        stickerColor_, this,
        TR("Escolher cor do contorno", "Choose outline color"));
    if (color.isValid()) {
        stickerColor_ = color;
        updateStickerColorButton();
    }
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
    if (isBusy_ || colorWatcher_.isRunning() || inference_->isRunning()
        || saveWatcher_.isRunning() || postProcessWatcher_.isRunning()
        || autoCropWatcher_.isRunning()) return;

    const int tol = colorToleranceSpin_->value();
    const bool contiguous = colorContiguousCheckBox_->isChecked();
    const bool pixelArt = colorPixelArtCheckBox_->isChecked();
    const float despill = chromaDespillSpin_->value() / 100.0f;
    const int feather = chromaFeatherSpin_->value();

    // A inundacao de regiao percorre a imagem toda: numa foto de 6 MP
    // demora ~1,8 s, o que congelava a janela. Agora corre na thread
    // de trabalho, como a inferencia.
    setUiBusy(true);
    progressBar_->setRange(0, 0);
    progressBar_->setValue(0);
    progressBar_->setVisible(true);
    const quint64 generation = imageGeneration_;
    colorWatcher_.setFuture(QtConcurrent::run(
        [img = preview_->originalImage(), color = keyColor_,
         tol, contiguous, pixelArt, despill, feather, generation]() {
            ProcessingOptions opts;
            opts.cleanNoise = false;
            QImage mask = ImagePostProcessor::removeByColor(
                img, color, tol, contiguous, pixelArt, despill, feather);
            // Aplica os mesmos efeitos de mascara do modo IA para o
            // preview ser consistente com o que o export produz.
            return std::make_pair(
                ImagePostProcessor::applyMaskEffects(mask, opts), generation);
        }));
}

void MainWindow::onColorRemovalFinished()
{
    progressBar_->setVisible(false);
    progressBar_->setRange(0, 100);
    progressBar_->setValue(0);
    setUiBusy(false);

    const auto result = colorWatcher_.result();
    // Descartar resultado de uma imagem anterior (trocada durante o job).
    if (result.second != imageGeneration_) {
        logEdit_->appendPlainText("[INFO] " + TR(
            "Resultado de uma imagem anterior ignorado.",
            "Result from a previous image ignored."));
        emit jobFinished();
        maybeCloseAfterJobs();
        return;
    }
    const QImage removed = result.first;
    if (!removed.isNull()) {
        // Guarda a mascara crua (antes dos efeitos) como base para o
        // live preview; o preview mostra com os efeitos aplicados.
        maskBaseImage_ = ImagePostProcessor::removeByColor(
            preview_->originalImage(), keyColor_,
            colorToleranceSpin_->value(),
            colorContiguousCheckBox_->isChecked(),
            colorPixelArtCheckBox_->isChecked(),
            chromaDespillSpin_->value() / 100.0f,
            chromaFeatherSpin_->value());
        preview_->setResult(removed);
        updateResultDisplay();
        btnAutoCrop_->setEnabled(true);
        btnDragOut_->setEnabled(true);
        logEdit_->appendPlainText(QString("[OK] %1 (%2: %3, %4: %5%)")
            .arg(TR("Remoção por cor concluída", "Color removal completed"))
            .arg(TR("Cor", "Color"))
            .arg(keyColor_.name().toUpper())
            .arg(TR("Tolerância", "Tolerance"))
            .arg(colorToleranceSpin_->value()));
    }
    emit jobFinished();
    maybeCloseAfterJobs();
}

// ---- Live preview: reaplicar efeitos de mascara quando as opcoes mudam ----

void MainWindow::scheduleEffectsReapply()
{
    if (isBusy_ || maskBaseImage_.isNull()) return;
    if (effectsWatcher_.isRunning()) {
        // Um pedido ja esta a correr; marcar para repetir no fim.
        effectsReapplyPending_ = true;
        return;
    }
    effectsReapplyPending_ = false;
    effectsGeneration_ = imageGeneration_;
    ProcessingOptions opts = currentProcessingOptions();
    opts.backgroundStyle = ProcessingOptions::BgTransparent;
    opts.backgroundColor = QColor();
    const QImage base = maskBaseImage_;
    const QImage orig = preview_->originalImage();
    effectsWatcher_.setFuture(QtConcurrent::run(
        [base, opts, orig]() {
            return ImagePostProcessor::applyMaskEffects(base, opts, orig);
        }));
}

void MainWindow::onEffectsReapplied()
{
    if (effectsWatcher_.result().isNull()) return;
    // So aplica se a imagem ainda e a mesma (generation unchanged).
    if (effectsGeneration_ == imageGeneration_) {
        preview_->setResult(effectsWatcher_.result());
        updateResultDisplay();
    }
    if (effectsReapplyPending_) {
        effectsReapplyPending_ = false;
        scheduleEffectsReapply();
    }
}
