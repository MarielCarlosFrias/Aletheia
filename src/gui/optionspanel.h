#ifndef OPTIONSPANEL_H
#define OPTIONSPANEL_H

#include <QWidget>

class QComboBox;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QCheckBox;

// Agrupa os widgets do painel de opcoes (inspector) para que a sua
// construcao possa viver fora do MainWindow. O MainWindow e dono dos
// widgets (inicializados na lista de inicializacao) e preenche esta
// struct; OptionsPanel::buildInspector monta o layout e devolve o
// QScrollArea pronto a adicionar a um splitter.
struct OptionWidgets {
    // Seccao "Remover fundo"
    QComboBox* removalMethodCombo = nullptr;
    QComboBox* modelCombo = nullptr;
    QLineEdit* modelPathEdit = nullptr;
    QPushButton* btnLoadModel = nullptr;
    QPushButton* btnRemove = nullptr;
    QWidget* aiRemovalWidget = nullptr;
    QWidget* colorRemovalWidget = nullptr;
    QPushButton* btnKeyColorSample = nullptr;
    QPushButton* btnAutoDetectKeyColor = nullptr;
    QSpinBox* colorToleranceSpin = nullptr;
    QCheckBox* colorContiguousCheckBox = nullptr;
    QCheckBox* colorPixelArtCheckBox = nullptr;
    QPushButton* btnRemoveByColor = nullptr;
    // Seccao "Ajustes de saida"
    QComboBox* presetCombo = nullptr;
    QComboBox* bgColorCombo = nullptr;
    QComboBox* saveFormatCombo = nullptr;
    QCheckBox* refineCheckBox = nullptr;
    QCheckBox* noiseCheckBox = nullptr;
    QSpinBox* noiseThresholdSpin = nullptr;
    QCheckBox* stickerCheckBox = nullptr;
    QCheckBox* shadowCheckBox = nullptr;
    // Seccao "Acoes e Retoque"
    QComboBox* brushModeCombo = nullptr;
    QSpinBox* brushRadiusSpin = nullptr;
    QPushButton* btnUndo = nullptr;
    QPushButton* btnRedo = nullptr;
    QPushButton* btnAutoCrop = nullptr;
    QPushButton* btnDragOut = nullptr;
};

// Constroi o painel inspector completo (3 seccoes colapsaveis dentro
// de um QScrollArea de largura fixa) e devolve o widget raiz.
// noiseThreshold e o valor inicial do spin de limiar de alfa.
class OptionsPanel {
public:
    static QWidget* buildInspector(const OptionWidgets& w,
                                   int noiseThreshold,
                                   QWidget* parent);
};

#endif // OPTIONSPANEL_H
