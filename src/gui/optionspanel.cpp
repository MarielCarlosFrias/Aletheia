#include "optionspanel.h"
#include "collapsiblesection.h"
#include "i18n.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>

QWidget* OptionsPanel::buildInspector(const OptionWidgets& w,
                                      int noiseThreshold,
                                      QWidget* parent)
{
    auto* inspector = new QFrame(parent);
    inspector->setObjectName("inspector");
    auto* inspectorLayout = new QVBoxLayout(inspector);
    inspectorLayout->setContentsMargins(12, 12, 12, 12);
    inspectorLayout->setSpacing(4);

    // O inspector passa a ter seccoes colapsaveis: a principal (acao
    // de remover) comeca aberta; as restantes comecam fechadas para o
    // painel nao parecer uma lista infinita de opcoes.
    auto* secRemove = new CollapsibleSection(
        TR("Remover fundo", "Remove background"), true, parent);
    auto* secRemoveLayout = secRemove->contentLayout();

    w.removalMethodCombo->addItem(TR("🧠 IA (Rede neural)", "🧠 AI (Neural network)"));
    w.removalMethodCombo->addItem(TR("🎨 Cor / Chroma Key (Pixel Art)", "🎨 Color / Chroma Key (Pixel Art)"));
    secRemoveLayout->addWidget(w.removalMethodCombo);

    // Modo 1: IA
    auto* aiLayout = new QVBoxLayout(w.aiRemovalWidget);
    aiLayout->setContentsMargins(0, 0, 0, 0);
    aiLayout->setSpacing(7);
    auto* modelForm = new QFormLayout;
    modelForm->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    modelForm->setRowWrapPolicy(QFormLayout::WrapAllRows);
    modelForm->addRow(TR("Modelo", "Model"), w.modelCombo);
    auto* modelPathLayout = new QHBoxLayout;
    modelPathLayout->addWidget(w.modelPathEdit);
    modelPathLayout->addWidget(w.btnLoadModel);
    modelForm->addRow(TR("Arquivo", "File"), modelPathLayout);
    aiLayout->addLayout(modelForm);
    w.btnRemove->setObjectName("primaryAction");
    w.btnRemove->setMinimumHeight(38);
    aiLayout->addWidget(w.btnRemove);
    secRemoveLayout->addWidget(w.aiRemovalWidget);

    // Modo 2: Cor / Chroma Key (Pixel Art)
    auto* colorLayout = new QVBoxLayout(w.colorRemovalWidget);
    colorLayout->setContentsMargins(0, 0, 0, 0);
    colorLayout->setSpacing(7);
    auto* colorForm = new QFormLayout;
    colorForm->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    colorForm->setRowWrapPolicy(QFormLayout::WrapAllRows);

    auto* keyColorRow = new QHBoxLayout;
    w.btnKeyColorSample->setObjectName("colorPickerButton");
    w.btnKeyColorSample->setCursor(Qt::PointingHandCursor);
    w.btnKeyColorSample->setToolTip(TR("Clique para escolher a cor a ser removida",
                                       "Click to choose the color to be removed"));
    w.btnAutoDetectKeyColor->setToolTip(TR("Detecta a cor predominante dos cantos da imagem",
                                           "Detects the dominant color in image corners"));
    keyColorRow->addWidget(w.btnKeyColorSample, 1);
    keyColorRow->addWidget(w.btnAutoDetectKeyColor);
    colorForm->addRow(TR("Cor alvo", "Target color"), keyColorRow);

    w.colorToleranceSpin->setRange(0, 100);
    w.colorToleranceSpin->setValue(0);
    w.colorToleranceSpin->setSuffix(" %");
    w.colorToleranceSpin->setToolTip(TR("0% para pixel art exato; aumente para fundos com ruído ou variações",
                                        "0% for exact pixel art; increase for noisy backgrounds"));
    colorForm->addRow(TR("Tolerância", "Tolerance"), w.colorToleranceSpin);
    colorForm->addRow(TR("Descontaminação", "Despill"),
                      w.chromaDespillSpin);
    colorForm->addRow(TR("Suavização", "Feather"), w.chromaFeatherSpin);
    colorLayout->addLayout(colorForm);

    w.colorContiguousCheckBox->setChecked(false);
    w.colorContiguousCheckBox->setToolTip(TR("Desmarcado: remove a cor em toda a imagem (inclusive entre pernas/braços);\nMarcado: remove apenas o fundo contíguo às bordas.",
                                             "Unchecked: removes color in entire image (including holes between limbs);\nChecked: removes only background connected to the borders."));
    colorLayout->addWidget(w.colorContiguousCheckBox);

    w.colorPixelArtCheckBox->setChecked(true);
    w.colorPixelArtCheckBox->setToolTip(TR("Mantém as bordas perfeitamente duras sem suavização (essencial para Pixel Art e Sprites).",
                                           "Keeps edges perfectly sharp without feathering (essential for Pixel Art & Sprites)."));
    colorLayout->addWidget(w.colorPixelArtCheckBox);

    w.btnRemoveByColor->setObjectName("primaryAction");
    w.btnRemoveByColor->setMinimumHeight(38);
    colorLayout->addWidget(w.btnRemoveByColor);
    w.colorRemovalWidget->setVisible(false);
    secRemoveLayout->addWidget(w.colorRemovalWidget);

    inspectorLayout->addWidget(secRemove);

    // Seccao 2: Ajustes de saida
    auto* secOutput = new CollapsibleSection(
        TR("Ajustes de saída", "Output adjustments"), false, parent);
    auto* secOutputLayout = secOutput->contentLayout();
    auto* outputForm = new QFormLayout;
    outputForm->setRowWrapPolicy(QFormLayout::WrapAllRows);
    outputForm->addRow(TR("Predefinição", "Preset"), w.presetCombo);
    outputForm->addRow(TR("Fundo ao salvar", "Background on save"),
                       w.bgColorCombo);
    // Formato de saida: PNG e WebP guardam transparencia; JPEG
    // compoe sobre o fundo escolhido (branco por omissao).
    w.saveFormatCombo->addItem(TR("PNG (transparente)",
                                  "PNG (transparent)"));
    w.saveFormatCombo->addItem(TR("JPEG", "JPEG"));
    w.saveFormatCombo->addItem(TR("WebP", "WebP"));
    outputForm->addRow(TR("Formato", "Format"),
                       w.saveFormatCombo);
    outputForm->addRow(TR("Qualidade JPEG/WebP", "JPEG/WebP quality"),
                       w.exportQualitySpin);
    outputForm->addRow(TR("Compressão PNG", "PNG compression"),
                       w.pngCompressionSpin);
    outputForm->addRow(w.preserveMetadataCheckBox);

    auto* sizeRow = new QHBoxLayout;
    sizeRow->setContentsMargins(0, 0, 0, 0);
    sizeRow->addWidget(w.outputWidthSpin);
    sizeRow->addWidget(new QLabel(TR("×", "×")));
    sizeRow->addWidget(w.outputHeightSpin);
    outputForm->addRow(TR("Tamanho final", "Final size"), sizeRow);
    outputForm->addRow(TR("Rotação", "Rotation"), w.rotationCombo);
    auto* qualitySpin = w.exportQualitySpin;
    auto* compressionSpin = w.pngCompressionSpin;
    const auto updateQualityControls = [qualitySpin, compressionSpin](int index) {
        qualitySpin->setEnabled(index != 0);
        compressionSpin->setEnabled(index == 0);
    };
    QObject::connect(w.saveFormatCombo, QOverload<int>::of(
                         &QComboBox::currentIndexChanged),
                     w.saveFormatCombo, updateQualityControls);
    updateQualityControls(w.saveFormatCombo->currentIndex());
    secOutputLayout->addLayout(outputForm);

    // Ajustes agrupados numa superficie propria: leem-se como um
    // bloco coerente em vez de 5 linhas soltas.
    auto* settingsBlock = new QFrame;
    settingsBlock->setObjectName("settingsBlock");
    auto* settingsBlockLayout = new QVBoxLayout(settingsBlock);
    settingsBlockLayout->setContentsMargins(11, 10, 11, 10);
    settingsBlockLayout->setSpacing(6);
    settingsBlockLayout->addWidget(w.refineCheckBox);
    auto* edgeRow = new QHBoxLayout;
    edgeRow->setContentsMargins(22, 0, 0, 0);
    edgeRow->addWidget(new QLabel(TR("Intensidade", "Intensity")));
    edgeRow->addWidget(w.edgeRefinementSpin, 1);
    settingsBlockLayout->addLayout(edgeRow);

    settingsBlockLayout->addWidget(w.noiseCheckBox);
    auto* noiseRow = new QHBoxLayout;
    noiseRow->setContentsMargins(22, 0, 0, 0);
    w.noiseThresholdSpin->setRange(0, 255);
    w.noiseThresholdSpin->setValue(noiseThreshold);
    w.noiseThresholdSpin->setSuffix(" /255");
    w.noiseThresholdSpin->setToolTip(
        TR("Alfa abaixo deste valor fica totalmente transparente",
           "Alpha below this value becomes fully transparent"));
    noiseRow->addWidget(new QLabel(TR("Limiar", "Threshold")));
    noiseRow->addWidget(w.noiseThresholdSpin, 1);
    settingsBlockLayout->addLayout(noiseRow);

    settingsBlockLayout->addWidget(w.stickerCheckBox);
    auto* stickerWidthRow = new QHBoxLayout;
    stickerWidthRow->setContentsMargins(22, 0, 0, 0);
    stickerWidthRow->addWidget(new QLabel(TR("Espessura", "Width")));
    stickerWidthRow->addWidget(w.stickerWidthSpin, 1);
    settingsBlockLayout->addLayout(stickerWidthRow);
    auto* stickerColorRow = new QHBoxLayout;
    stickerColorRow->setContentsMargins(22, 0, 0, 0);
    stickerColorRow->addWidget(new QLabel(TR("Cor", "Color")));
    stickerColorRow->addWidget(w.stickerColorButton, 1);
    settingsBlockLayout->addLayout(stickerColorRow);

    settingsBlockLayout->addWidget(w.shadowCheckBox);
    auto* shadowBlurRow = new QHBoxLayout;
    shadowBlurRow->setContentsMargins(22, 0, 0, 0);
    shadowBlurRow->addWidget(new QLabel(TR("Desfoque", "Blur")));
    shadowBlurRow->addWidget(w.shadowBlurSpin, 1);
    settingsBlockLayout->addLayout(shadowBlurRow);
    auto* shadowOffsetRow = new QHBoxLayout;
    shadowOffsetRow->setContentsMargins(22, 0, 0, 0);
    shadowOffsetRow->addWidget(new QLabel(TR("Deslocamento", "Offset")));
    shadowOffsetRow->addWidget(w.shadowOffsetXSpin);
    shadowOffsetRow->addWidget(new QLabel(TR("×", "×")));
    shadowOffsetRow->addWidget(w.shadowOffsetYSpin);
    settingsBlockLayout->addLayout(shadowOffsetRow);
    auto* shadowOpacityRow = new QHBoxLayout;
    shadowOpacityRow->setContentsMargins(22, 0, 0, 0);
    shadowOpacityRow->addWidget(new QLabel(TR("Opacidade", "Opacity")));
    shadowOpacityRow->addWidget(w.shadowOpacitySpin, 1);
    settingsBlockLayout->addLayout(shadowOpacityRow);
    secOutputLayout->addWidget(settingsBlock);

    inspectorLayout->addWidget(secOutput);

    // Seccao 3: Acoes e Retoque
    auto* secManual = new CollapsibleSection(
        TR("Ações e Retoque", "Actions and Retouch"),
        false, parent);
    auto* secManualLayout = secManual->contentLayout();
    auto* brushForm = new QFormLayout;
    brushForm->setRowWrapPolicy(QFormLayout::WrapAllRows);
    brushForm->addRow(TR("Pincel", "Brush"), w.brushModeCombo);
    brushForm->addRow(TR("Tamanho", "Size"), w.brushRadiusSpin);
    secManualLayout->addLayout(brushForm);
    auto* undoRow = new QHBoxLayout;
    undoRow->addWidget(w.btnUndo);
    undoRow->addWidget(w.btnRedo);
    secManualLayout->addLayout(undoRow);
    auto* actionRow = new QHBoxLayout;
    actionRow->addWidget(w.btnAutoCrop);
    actionRow->addWidget(w.btnDragOut);
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
    // ele roda em vez de cortar os campos dos formularios. Largura
    // mínima de 320px garante que os formularios (label + spinbox)
    // nao ficam cortados; o splitter pode esticar ate 360px.
    auto* inspectorScroll = new QScrollArea(parent);
    inspectorScroll->setObjectName("inspectorScroll");
    inspectorScroll->setWidget(inspector);
    inspectorScroll->setWidgetResizable(true);
    inspectorScroll->setFrameShape(QFrame::NoFrame);
    inspectorScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    inspectorScroll->setMinimumWidth(320);
    inspectorScroll->setMaximumWidth(360);
    return inspectorScroll;
}
