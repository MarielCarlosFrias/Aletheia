#include "helpwidget.h"
#include "i18n.h"
#include <QApplication>
#include <QPalette>

namespace {

// Mistura a cor a em direcao a b no fator t (0..1).
QString blend(const QColor& a, const QColor& b, float t)
{
    const QColor c(
        int(a.red()   * (1 - t) + b.red()   * t),
        int(a.green() * (1 - t) + b.green() * t),
        int(a.blue()  * (1 - t) + b.blue()  * t));
    return c.name();
}

} // namespace

HelpWidget::HelpWidget(QWidget* parent)
    : QTextBrowser(parent)
{
    setOpenExternalLinks(true);
    setObjectName("helpView");
    setFrameShape(QFrame::NoFrame);

    // Acompanha o tema escuro/claro em tempo real.
    rebuildHtml();
}

bool HelpWidget::event(QEvent* event)
{
    if (event->type() == QEvent::ApplicationPaletteChange) {
        rebuildHtml();
    }
    return QTextBrowser::event(event);
}

void HelpWidget::rebuildHtml()
{
#ifdef ALETHEIA_VERSION
    const QString version = ALETHEIA_VERSION;
#else
    const QString version = QStringLiteral("1.0.0");
#endif

    const QString repo =
        QStringLiteral("https://github.com/MarielCarlosFrias/aletheia");
    const QString issues = repo + QStringLiteral("/issues");
    const QString releases = repo + QStringLiteral("/releases");
    const QString profile =
        QStringLiteral("https://github.com/MarielCarlosFrias");

    // Cores derivadas da paleta ativa (tema escuro ou claro).
    const QPalette pal = palette();
    const QColor base = pal.color(QPalette::Base);
    const QColor text = pal.color(QPalette::WindowText);
    const bool dark = base.lightness() < 128;
    const QString card = dark
        ? blend(base, QColor(255, 255, 255), 0.07f)
        : blend(base, QColor(0, 0, 0), 0.045f);
    const QString muted = blend(base, text, 0.60f);
    const QString chip = card;
    const QString accent = dark ? "#5da9e9" : "#1a6fb5";
    const QString green = "#36c66a";
    const QString greenDark = "#2f9e52";
    const QString onGreen = "#102217";

    auto cardCell = [&](const QString& emoji, const QString& num,
                        const QString& title, const QString& desc) {
        return QString(
                   "<td width=\"50%\" bgcolor=\"%1\" style=\"border-radius:6px; padding:12px;\">"
                   "<span style=\"font-size:22px\">%2</span> "
                   "<b>%3 %4</b><br>"
                   "<span style=\"color:%5\">%6</span></td>")
            .arg(card, emoji, num, title, muted, desc);
    };

    auto shortcutRow = [&](const QString& k1, const QString& a1,
                           const QString& k2, const QString& a2) {
        return QString(
                   "<tr>"
                   "<td bgcolor=\"%1\" style=\"padding:5px 10px; border-radius:4px;\"><b><font color=\"%2\">%3</font></b></td>"
                   "<td style=\"padding:5px 10px;\"><span style=\"color:%4\">%5</span></td>"
                   "<td bgcolor=\"%1\" style=\"padding:5px 10px; border-radius:4px;\"><b><font color=\"%2\">%6</font></b></td>"
                   "<td style=\"padding:5px 10px;\"><span style=\"color:%4\">%7</span></td>"
                   "</tr>")
            .arg(chip, green, k1, muted, a1, k2, a2);
    };

    auto faqItem = [&](const QString& q, const QString& a) {
        return QString(
                   "<p style=\"margin:6px 0;\"><b style=\"color:%1\">Q: %2</b><br>"
                   "<span style=\"color:%3\">%4</span></p>")
            .arg(text.name(), q, muted, a);
    };

    QString html;

    // ===== Cabecalho + privacidade =====
    html += QString(
        "<h1>Aletheia %1</h1>"
        "<table width=\"100%\" cellpadding=\"12\" cellspacing=\"0\" style=\"border-radius:6px;\">"
        "<tr><td bgcolor=\"%2\" style=\"border-radius:6px;\"><b><span style=\"font-size:14px; color:%3\">"
        "\U0001F512 %4"
        "</span></b></td></tr></table><br>")
        .arg(version, green, onGreen,
             TR("100% Local e Privado — suas imagens são processadas no seu computador e nunca vão para a nuvem. "
                "Ideal para fotos pessoais, documentos e sprites.",
                "100% Local and Private — your images are processed on your computer and never uploaded. "
                "Ideal for personal photos, documents and sprites."));

    // ===== 4 passos =====
    html += QString("<h3>\U0001F680 %1</h3>").arg(TR("Fluxo de Trabalho em 4 Passos", "Quick 4-Step Workflow"));
    html += QString(
        "<table width=\"100%\" cellpadding=\"10\" cellspacing=\"8\">"
        "<tr>%1%2</tr>"
        "<tr>%3%4</tr>"
        "</table><br>")
        .arg(
            cardCell("\U0001F5BC", "1.",
                     TR("Abrir ou Colar", "Open or Paste"),
                     TR("Arraste uma foto, use <b>Ctrl+O</b> ou cole com <b>Ctrl+V</b>.",
                        "Drag an image, press <b>Ctrl+O</b> or paste with <b>Ctrl+V</b>.")),
            cardCell("\u2699", "2.",
                     TR("Escolha o Modo", "Choose the Mode"),
                     TR("<b>IA</b> para fotos; <b>Cor (Chroma Key)</b> para Pixel Art. Pressione <b>Ctrl+D</b> para executar.",
                        "<b>AI</b> for photos; <b>Color (Chroma Key)</b> for Pixel Art. Press <b>Ctrl+D</b> to run.")),
            cardCell("\u2702", "3.",
                     TR("Ajuste e Efeitos", "Touch Up & Effects"),
                     TR("Pincel (Restaurar/Apagar), Auto-Recorte, Contorno, Sombra. <b>Ctrl+Z</b> desfaz.",
                        "Brush (Restore/Erase), Auto-Crop, Sticker, Shadow. <b>Ctrl+Z</b> undoes.")),
            cardCell("\U0001F4BE", "4.",
                     TR("Salvar ou Arrastar", "Save or Drag-Out"),
                     TR("<b>Ctrl+S</b> salva, <b>Ctrl+C</b> copia, ou arraste o resultado para outro app.",
                        "<b>Ctrl+S</b> saves, <b>Ctrl+C</b> copies, or drag the result to another app.")));

    // ===== IA vs Chroma Key =====
    html += QString("<h3>\U0001F4A1 %1</h3>").arg(TR("Qual Modo de Remoção Escolher?", "Which Removal Mode to Choose?"));
    html += QString(
        "<table width=\"100%\" cellpadding=\"12\" cellspacing=\"8\">"
        "<tr>"
        "<td width=\"50%\" bgcolor=\"%1\" style=\"border-radius:6px;\">"
        "<b>\U0001F9E0 %2</b><br>"
        "<span style=\"color:%3\">%4</span>"
        "</td>"
        "<td width=\"50%\" bgcolor=\"%1\" style=\"border-radius:6px;\">"
        "<b>\U0001F3A8 %5</b><br>"
        "<span style=\"color:%3\">%6</span>"
        "</td>"
        "</tr>"
        "</table><br>")
        .arg(card)
        .arg(TR("Modo IA (Rede Neural)", "AI Mode (Neural Network)"))
        .arg(muted)
        .arg(TR("Para fotos reais, produtos, pessoas e animais. Analisa formas complexas e cabelos. Modelos extras (BRIA RMBG, BiRefNet, ISNet) na aba <b>Modelos</b>. GPU suportada via CUDA/DirectML quando disponível.",
                "For real photos, products, people and pets. Analyzes complex shapes and hair. Extra models (BRIA RMBG, BiRefNet, ISNet) in the <b>Models</b> tab. GPU via CUDA/DirectML when available."))
        .arg(TR("Modo Cor / Chroma Key (Pixel Art)", "Color Mode / Chroma Key (Pixel Art)"))
        .arg(TR("Para <b>Pixel Art</b>, sprites (fundo magenta #FF00FF), logos com fundo liso. Detecta a cor dos cantos (<b>Detectar fundo</b>), bordas 100% nítidas, execução instantânea.",
                "For <b>Pixel Art</b>, sprites (magenta #FF00FF background), flat-color logos. Auto-detects corner color (<b>Detect background</b>), 100% crisp edges, instant execution."));

    // ===== Recursos especiais =====
    html += QString("<h3>\u2728 %1</h3>").arg(TR("Recursos e Ferramentas", "Tools & Features"));
    html += QString(
        "<table width=\"100%\" cellpadding=\"12\" cellspacing=\"8\">"
        "<tr>"
        "<td width=\"33%\" bgcolor=\"%1\" style=\"border-radius:6px;\">"
        "<b>\u2702 %2</b><br><span style=\"color:%3\">%4</span></td>"
        "<td width=\"33%\" bgcolor=\"%1\" style=\"border-radius:6px;\">"
        "<b>\U0001F4CF %5</b><br><span style=\"color:%3\">%6</span></td>"
        "<td width=\"33%\" bgcolor=\"%1\" style=\"border-radius:6px;\">"
        "<b>\U0001F504 %7</b><br><span style=\"color:%3\">%8</span></td>"
        "</tr>"
        "<tr>"
        "<td width=\"33%\" bgcolor=\"%1\" style=\"border-radius:6px;\">"
        "<b>\U0001F3A8 %9</b><br><span style=\"color:%3\">%10</span></td>"
        "<td width=\"33%\" bgcolor=\"%1\" style=\"border-radius:6px;\">"
        "<b>\U0001F31B %11</b><br><span style=\"color:%3\">%12</span></td>"
        "<td width=\"33%\" bgcolor=\"%1\" style=\"border-radius:6px;\">"
        "<b>\U0001F9ED %13</b><br><span style=\"color:%3\">%14</span></td>"
        "</tr>"
        "</table><br>")
        .arg(card)
        .arg(TR("Auto-Recorte", "Auto-Crop"))
        .arg(muted)
        .arg(TR("Remove margens transparentes vazias ao redor do objeto.", "Crops empty transparent margins around the subject."))
        .arg(TR("Arrastar Resultado", "Drag Result"))
        .arg(TR("Arraste o PNG direto para Photoshop, Discord ou pastas.", "Drag the PNG straight into Photoshop, Discord or folders."))
        .arg(TR("Modo Comparar", "Compare Mode"))
        .arg(TR("Divisor interativo antes/depois em tempo real.", "Interactive before/after slider in real time."))
        .arg(TR("Sticker / Sombra", "Sticker / Shadow"))
        .arg(TR("Contorno de adesivo e sombra projetada ajustáveis.", "Adjustable sticker outline and drop shadow."))
        .arg(TR("Tema Claro/Escuro", "Light/Dark Theme"))
        .arg(TR("Alterne no cabecalho; a preferencia fica guardada.", "Toggle in the header; preference is saved."))
        .arg(TR("Visualizar Máscara", "View Mask"))
        .arg(TR("<b>Ctrl+M</b> mostra so o canal alfa da máscara.", "<b>Ctrl+M</b> shows only the alpha mask channel."));

    // ===== Atalhos completos =====
    html += QString("<h3>\u2328 %1</h3>").arg(TR("Atalhos de Teclado (completos)", "Keyboard Shortcuts (complete)"));
    html += QString("<table cellpadding=\"5\" cellspacing=\"4\">%1%2%3%4%5%6%7</table><br>")
        .arg(
            shortcutRow("Ctrl+O", TR("abrir imagem", "open image"),
                        "Ctrl+V", TR("colar do clipboard", "paste from clipboard")),
            shortcutRow("Ctrl+D", TR("remover fundo (IA ou Cor)", "remove background (AI or Color)"),
                        "Ctrl+S", TR("salvar resultado", "save result")),
            shortcutRow("Ctrl+C", TR("copiar resultado", "copy result"),
                        "Ctrl+Shift+C", TR("copiar (alternativo)", "copy (alternate)")),
            shortcutRow("Ctrl+Z / Ctrl+Y", TR("desfazer / refazer pincel", "undo / redo brush"),
                        "Ctrl+M", TR("mostrar/esconder máscara alfa", "toggle alpha mask view")),
            shortcutRow("Ctrl + roda", TR("zoom no cursor", "zoom at cursor"),
                        "Ctrl++ / Ctrl+-", TR("aumentar / diminuir zoom", "zoom in / out")),
            shortcutRow("Ctrl+0", TR("zoom 100% (ajustar)", "zoom 100% (fit)"),
                        "F1", TR("abrir esta aba de Ajuda", "open this Help tab")),
            shortcutRow("Ctrl+Q", TR("sair", "quit"), "", ""));

    // ===== Formatos suportados =====
    html += QString("<h3>\U0001F4C4 %1</h3>").arg(TR("Formatos de Imagem", "Image Formats"));
    html += QString(
        "<table width=\"100%\" cellpadding=\"10\" cellspacing=\"0\">"
        "<tr><td bgcolor=\"%1\" style=\"border-radius:6px;\">"
        "<span style=\"color:%2\">"
        "<b>%3:</b> PNG, JPEG, BMP, TIFF, WebP<br>"
        "<b>%4:</b> PNG (transparente), JPEG, WebP<br>"
        "<b>%5:</b> HEIC, AVIF, JP2, JXL — requer o pacote <code>qt6-image-formats-plugins</code> "
        "(no Linux: <code>sudo apt install qt6-image-formats-plugins</code>)."
        "</span></td></tr></table><br>")
        .arg(card, muted,
             TR("Entrada", "Input"),
             TR("Saída", "Output"),
             TR("Extra (com plugin)", "Extra (with plugin)"));

    // ===== CLI =====
    html += QString("<h3>\U0001F4E6 %1</h3>").arg(TR("Processamento em Lote & CLI", "Batch Processing & CLI"));
    html += QString(
        "<table width=\"100%\" cellpadding=\"12\" cellspacing=\"8\">"
        "<tr><td width=\"100%\" bgcolor=\"%1\" style=\"border-radius:6px;\">"
        "<span style=\"color:%2\">%3<br><br>"
        "<code>aletheia_cli ~/imagens -o ~/saida</code> — lote básico<br>"
        "<code>aletheia_cli ~/imagens -o ~/saida -f webp -j 4</code> — WebP, 4 em paralelo<br>"
        "<code>aletheia_cli ~/imagens -m u2net.onnx</code> — modelo específico<br>"
        "<code>aletheia_cli --help</code> · <code>aletheia_cli --version</code><br>"
        "<i>%4</i></span></td></tr></table><br>")
        .arg(card, muted,
             TR("A aba <b>Lote</b> converte pastas inteiras pela GUI. O CLI oferece o mesmo motor para scripts e servidores. "
                "Ctrl+C interrompe o lote de forma limpa (as imagens já processadas ficam guardadas).",
                "The <b>Batch</b> tab converts whole folders in the GUI. The CLI provides the same engine for scripts and servers. "
                "Ctrl+C stops the batch cleanly (already processed images are kept)."),
             TR("Dica: use -j N para processar N imagens em paralelo.", "Tip: use -j N to process N images in parallel."));

    // ===== FAQ =====
    html += QString("<h3>\u2753 %1</h3>").arg(TR("Perguntas Frequentes (FAQ)", "FAQ"));
    html += QString("<table width=\"100%\" cellpadding=\"10\" cellspacing=\"0\">"
                    "<tr><td bgcolor=\"%1\" style=\"border-radius:6px;\">%2</td></tr></table><br>")
        .arg(card,
             faqItem(TR("O resultado ficou desfocado nas bordas?", "Are the edges blurry?"),
                     TR("Tente um modelo de alta precisão na aba Modelos (BRIA RMBG 2.0 ou BiRefNet). "
                        "Para Pixel Art, use sempre o modo Cor/Chroma Key com tolerância 0%.",
                        "Try a high-precision model in the Models tab (BRIA RMBG 2.0 or BiRefNet). "
                        "For Pixel Art, always use Color/Chroma Key mode with 0% tolerance."))
             + faqItem(TR("Posso usar GPU?", "Can I use a GPU?"),
                       TR("Sim — se o ONNX Runtime tiver CUDA/DirectML, a app usa automaticamente. "
                          "Force com a variável de ambiente <code>ALETHEIA_PROVIDER=cuda</code> (ou dml/coreml/cpu).",
                          "Yes — if ONNX Runtime has CUDA/DirectML, the app uses it automatically. "
                          "Force with <code>ALETHEIA_PROVIDER=cuda</code> (or dml/coreml/cpu)."))
             + faqItem(TR("Onde ficam os modelos descarregados?", "Where are downloaded models stored?"),
                       TR("Em <code>~/.local/share/aletheia/models</code> (Linux), "
                          "<code>%APPDATA%\\aletheia\\models</code> (Windows) ou Application Support (macOS). "
                          "São detetados automaticamente pela GUI e pelo CLI.",
                          "In <code>~/.local/share/aletheia/models</code> (Linux), "
                          "<code>%APPDATA%\\aletheia\\models</code> (Windows) or Application Support (macOS). "
                          "Auto-detected by both GUI and CLI."))
             + faqItem(TR("Como processar uma pasta inteira?", "How to batch-process a folder?"),
                       TR("Abra a aba <b>Lote</b>, escolha a pasta de entrada e saída, e clique em processar. "
                          "Ou use o CLI: <code>aletheia_cli pasta_entrada -o pasta_saida -j 4</code>.",
                          "Open the <b>Batch</b> tab, pick input/output folders, and process. "
                          "Or use the CLI: <code>aletheia_cli input_folder -o output_folder -j 4</code>."))
             + faqItem(TR("A imagem tem fundo transparente e não consigo detetar a cor?", "Transparent background — can't detect color?"),
                       TR("O botão <b>Detectar fundo</b> ignora pixels transparentes e procura a cor opaca dominante. "
                          "Se a imagem for 100% transparente, o padrão é magenta (#FF00FF).",
                          "The <b>Detect background</b> button ignores transparent pixels and finds the dominant opaque color. "
                          "If the image is 100% transparent, the default is magenta (#FF00FF).")));

    // ===== Rodape =====
    html += QString(
        "<table cellpadding=\"10\" cellspacing=\"8\"><tr>"
        "<td bgcolor=\"%1\" style=\"border-radius:6px;\"><a href=\"%2\" style=\"color:%3; text-decoration:none;\">"
        "<b>\u2b50 %4</b></a></td>"
        "<td bgcolor=\"%5\" style=\"border-radius:6px;\"><a href=\"%6\" style=\"color:%3; text-decoration:none;\">"
        "<b>\U0001F41E %7</b></a></td>"
        "<td bgcolor=\"%5\" style=\"border-radius:6px;\"><a href=\"%8\" style=\"color:%3; text-decoration:none;\">"
        "<b>\U0001F4E6 %9</b></a></td>"
        "<td><span style=\"color:%10\">%11 <a href=\"%12\" style=\"color:%10; font-weight:bold;\">Mariel Carlos Frias</a></span></td>"
        "</tr></table>"
        "<p><span style=\"color:%10; font-size:small;\">"
        "%13 <a href=\"https://github.com/danielgatis/rembg\" style=\"color:%10;\">rembg</a> · "
        "ONNX Runtime · Qt 6 · Licença MIT</span></p>")
        .arg(green, repo, onGreen)
        .arg(TR("Repositório no GitHub", "GitHub Repository"))
        .arg(greenDark, issues)
        .arg(TR("Reportar Problema", "Report an Issue"))
        .arg(releases)
        .arg(TR("Releases / Novidades", "Releases / What's New"))
        .arg(muted)
        .arg(TR("por", "by"))
        .arg(profile)
        .arg(TR("Modelos U²-Net / IS-Net / BiRefNet · Arquivos ONNX por ",
                "U²-Net / IS-Net / BiRefNet models · ONNX files by "));

    setHtml(html);
}
