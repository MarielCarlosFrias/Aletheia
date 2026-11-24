#include "backgroundremover.h"
#include <QDebug>
#include <QFileInfo>
#include <QFile>
#include <QDateTime>
#include <QProcessEnvironment>
#include <QStringList>
#include <cmath>
#include <algorithm>
#include <array>
#include <stdexcept>
#include <thread>

// Constantes ImageNet
static constexpr float MEAN[3] = {0.485f, 0.456f, 0.406f};
static constexpr float STD[3]  = {0.229f, 0.224f, 0.225f};

// Seleciona o execution provider (GPU se disponivel, senao CPU).
// A variavel de ambiente ALETHEIA_PROVIDER controla a escolha:
//   auto (padrao) | cpu | cuda | dml | coreml
// No modo auto tenta CUDA -> DirectML -> CoreML -> CPU, conforme os
// providers disponiveis no build do ONNX Runtime. Devolve o nome do
// provider efetivamente usado (para o log).
static QString selectProvider(Ort::SessionOptions& opts)
{
    QString requested = QProcessEnvironment::systemEnvironment()
        .value(QStringLiteral("ALETHEIA_PROVIDER"))
        .toLower().trimmed();
    if (requested.isEmpty()) requested = QStringLiteral("auto");

    // Lista de providers disponiveis neste build do ONNX Runtime.
    QStringList available;
    for (const auto& p : Ort::GetAvailableProviders())
        available << QString::fromStdString(p);

    const int cpuThreads = std::max(
        1, static_cast<int>(std::thread::hardware_concurrency()));

    auto tryAppend = [&](const QString& name) -> bool {
        if (!available.contains(name, Qt::CaseInsensitive)) return false;
        try {
            if (name.compare(QStringLiteral("CUDA"), Qt::CaseInsensitive) == 0) {
                OrtCUDAProviderOptions cudaOpts{};
                cudaOpts.device_id = 0;
                opts.AppendExecutionProvider_CUDA(cudaOpts);
            } else {
                // DML, CoreML e outros usam o metodo generico por nome.
                opts.AppendExecutionProvider(name.toStdString());
            }
            return true;
        } catch (const Ort::Exception& e) {
            qWarning() << "[BackgroundRemover] Falha ao ativar provider"
                       << name << ":" << e.what();
            return false;
        } catch (...) {
            return false;
        }
    };

    if (requested == QLatin1String("cuda")) {
        if (tryAppend(QStringLiteral("CUDA"))) return QStringLiteral("CUDA");
    } else if (requested == QLatin1String("dml")) {
        if (tryAppend(QStringLiteral("DML"))) return QStringLiteral("DML");
    } else if (requested == QLatin1String("coreml")) {
        if (tryAppend(QStringLiteral("CoreML"))) return QStringLiteral("CoreML");
    } else if (requested == QLatin1String("cpu")) {
        // CPU apenas: e o provider padrao, basta configurar as threads.
    } else { // auto
        if (tryAppend(QStringLiteral("CUDA"))) return QStringLiteral("CUDA");
        if (tryAppend(QStringLiteral("DML")))  return QStringLiteral("DML");
        if (tryAppend(QStringLiteral("CoreML"))) return QStringLiteral("CoreML");
    }

    // CPU e sempre o ultimo recurso (e o unico garantido): e o
    // provider padrao do ONNX Runtime, nao precisa de Append explicito.
    opts.SetIntraOpNumThreads(cpuThreads);
    return QStringLiteral("CPU");
}

std::mutex BackgroundRemover::cacheMutex_;
std::unordered_map<QString, std::weak_ptr<BackgroundRemover>> BackgroundRemover::cache_;


BackgroundRemover::BackgroundRemover(const QString& modelPath)
    : env_(ORT_LOGGING_LEVEL_WARNING, "aletheia"),
      session_(nullptr),
      modelPath_(modelPath)
{
    const int cpuThreads = std::max(
        1, static_cast<int>(std::thread::hardware_concurrency()));

    // Primeira tentativa: provider escolhido (GPU se disponivel).
    Ort::SessionOptions gpu_opts;
    gpu_opts.SetIntraOpNumThreads(cpuThreads);
    gpu_opts.SetGraphOptimizationLevel(
        GraphOptimizationLevel::ORT_ENABLE_ALL);
    const QString provider = selectProvider(gpu_opts);

#ifdef _WIN32
    const std::wstring wpath = modelPath.toStdWString();
    const wchar_t* wpathPtr = wpath.c_str();
#else
    const QByteArray utf8Path = modelPath.toUtf8();
    const char* utf8Ptr = utf8Path.constData();
#endif

    try {
#ifdef _WIN32
        session_ = Ort::Session(env_, wpathPtr, gpu_opts);
#else
        session_ = Ort::Session(env_, utf8Ptr, gpu_opts);
#endif
        provider_ = provider;
    } catch (const Ort::Exception& e) {
        // Se o provider GPU falhou (ex.: driver ausente), faz fallback
        // para CPU puro e recria a sessao.
        if (provider != QLatin1String("CPU")) {
            qWarning() << "[BackgroundRemover] Sessao com provider"
                       << provider << "falhou (" << e.what()
                       << "); a usar CPU.";
            Ort::SessionOptions cpu_opts;
            cpu_opts.SetIntraOpNumThreads(cpuThreads);
            cpu_opts.SetGraphOptimizationLevel(
                GraphOptimizationLevel::ORT_ENABLE_ALL);
            // CPU e o provider padrao; nao precisa de Append explicito.
#ifdef _WIN32
            session_ = Ort::Session(env_, wpathPtr, cpu_opts);
#else
            session_ = Ort::Session(env_, utf8Ptr, cpu_opts);
#endif
            provider_ = QStringLiteral("CPU");
        } else {
            throw;
        }
    }

    Ort::AllocatorWithDefaultOptions allocator;

    Ort::AllocatedStringPtr in_ptr  = session_.GetInputNameAllocated(0, allocator);
    Ort::AllocatedStringPtr out_ptr = session_.GetOutputNameAllocated(0, allocator);
    input_name_  = std::string(in_ptr.get());
    output_name_ = std::string(out_ptr.get());

    input_size_ = detectInputSize(modelPath);

    const QString sizeStr = QString::number(input_size_)
        + QStringLiteral("x") + QString::number(input_size_);
    qInfo().noquote() << QStringLiteral(
        "[BackgroundRemover] Modelo carregado: %1\n  Provider: %2\n  Input: %3 (%4)\n  Output: %5")
        .arg(modelPath,
             provider_,
             QString::fromStdString(input_name_),
             sizeStr,
             QString::fromStdString(output_name_));

    // Metadados do ficheiro: o tamanho e a data de modificacao sao
    // guardados para detetar alteracoes sem re-ler o modelo inteiro
    // (176 MB+ por carga).
    const QFileInfo info(modelPath_);
    modelSize_ = info.size();
    modelModified_ = info.lastModified();
}

bool BackgroundRemover::modelFileUnchanged() const
{
    const QFileInfo info(modelPath_);
    if (!info.exists()) return false;
    if (info.size() != modelSize_) return false;
    return info.lastModified() == modelModified_;
}

std::shared_ptr<BackgroundRemover> BackgroundRemover::get(const QString& modelPath) {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    auto it = cache_.find(modelPath);
    if (it != cache_.end()) {
        if (auto sp = it->second.lock()) {
            // O ficheiro do modelo mudou? A comparacao usa tamanho e
            // data (stat): nao volta a ler o modelo inteiro para
            // re-hashear (176 MB por imagem no lote do CLI).
            if (sp->modelFileUnchanged())
                return sp;
            cache_.erase(it);
        } else {
            cache_.erase(it);
        }
    }
    auto sp = std::make_shared<BackgroundRemover>(modelPath);
    cache_[modelPath] = sp;
    return sp;
}

void BackgroundRemover::requestCancel() {
    cancelRequested_.store(true);
    // Cancela TODAS as inferencias em curso desta sessao: no lote CLI
    // varias threads podem estar a correr removeBackground() em
    // simultaneo sobre a mesma instancia partilhada.
    std::vector<std::shared_ptr<Ort::RunOptions>> opts;
    {
        std::lock_guard<std::mutex> lock(runMutex_);
        opts = activeRunOptions_;
    }
    for (auto& o : opts) {
        try {
            o->SetTerminate();
        } catch (...) {
            // ignorar falhas de cancelamento
        }
    }
}

// U2-Net/Silueta trabalham a 320x320; a geracao ISNet a 1024x1024.
// O shape declarado pelo modelo e a fonte de verdade; modelos com
// shape dinamico (-1) caem no fallback pelo nome do ficheiro.
int BackgroundRemover::detectInputSize(const QString& modelPath) const
{
    try {
        const std::vector<int64_t> shape =
            session_.GetInputTypeInfo(0)
                .GetTensorTypeAndShapeInfo()
                .GetShape();
        if (shape.size() == 4 && shape[2] > 0 && shape[3] > 0) {
            if (shape[2] == shape[3]) {
                return static_cast<int>(shape[2]);
            }
            qWarning() << "[BackgroundRemover] Entrada nao quadrada ("
                       << shape[2] << "x" << shape[3]
                       << "); a usar" << kDefaultInputSize;
        }
    } catch (const Ort::Exception& e) {
        qWarning() << "[BackgroundRemover] Nao foi possivel ler o shape "
                      "da entrada:" << e.what();
    }

    const QString name = QFileInfo(modelPath).fileName().toLower();
    if (name.contains(QStringLiteral("isnet"))) {
        return 1024;
    }
    return kDefaultInputSize;
}

QImage BackgroundRemover::removeBackground(const QImage& input) {
    if (input.isNull()) {
        throw std::invalid_argument("A imagem de entrada e invalida");
    }
    if (cancelRequested_.load()) {
        cancelRequested_.store(false);
        throw std::runtime_error("Inferência cancelada pelo usuário");
    }

    const int size = input_size_;
    std::vector<float> blob = buildInputBlob(input);

    if (cancelRequested_.load()) {
        cancelRequested_.store(false);
        throw std::runtime_error("Inferência cancelada pelo usuário");
    }

    std::vector<int64_t> input_shape = {1, 3, size, size};
    Ort::MemoryInfo mem_info = Ort::MemoryInfo::CreateCpu(
        OrtArenaAllocator, OrtMemTypeDefault);
    Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
        mem_info, blob.data(), blob.size(),
        input_shape.data(), input_shape.size());

    const char* in_name  = input_name_.c_str();
    const char* out_name = output_name_.c_str();

    // O RunOptions vive num shared_ptr registado no vetor de ativos:
    // requestCancel() pode estar a usa-lo quando a inferencia acaba
    // (com um ponteiro para um objeto da pilha isso era use-after-free).
    auto runOptions = std::make_shared<Ort::RunOptions>();
    {
        std::lock_guard<std::mutex> lock(runMutex_);
        activeRunOptions_.push_back(runOptions);
    }

    std::vector<Ort::Value> outputs;
    try {
        outputs = session_.Run(
            *runOptions,
            &in_name, &input_tensor, 1,
            &out_name, 1);
    } catch (...) {
        std::lock_guard<std::mutex> lock(runMutex_);
        // Remove este runOptions do vetor (procura por ponteiro).
        auto& v = activeRunOptions_;
        v.erase(std::remove(v.begin(), v.end(), runOptions), v.end());
        throw;
    }
    {
        std::lock_guard<std::mutex> lock(runMutex_);
        auto& v = activeRunOptions_;
        v.erase(std::remove(v.begin(), v.end(), runOptions), v.end());
    }

    if (cancelRequested_.load()) {
        throw std::runtime_error("Inferência cancelada pelo usuário");
    }

    if (outputs.empty() || !outputs[0].IsTensor()) {
        throw std::runtime_error("O modelo nao retornou uma mascara tensorial");
    }

    const auto tensorInfo = outputs[0].GetTensorTypeAndShapeInfo();
    if (tensorInfo.GetElementType()
            != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT) {
        throw std::runtime_error(
            "Mascara de saida incompativel: esperado tensor float, "
            "recebido tipo "
            + std::to_string(
                  static_cast<int>(tensorInfo.GetElementType())));
    }

    // As dimensoes da mascara vem do proprio tensor de saida: nem todos
    // os modelos devolvem uma mascara do tamanho da entrada.
    int mask_w = size;
    int mask_h = size;
    const std::vector<int64_t> out_shape = tensorInfo.GetShape();
    if (out_shape.size() == 4 && out_shape[2] > 0 && out_shape[3] > 0) {
        mask_h = static_cast<int>(out_shape[2]);
        mask_w = static_cast<int>(out_shape[3]);
    }

    const size_t expectedMaskSize =
        static_cast<size_t>(mask_w) * static_cast<size_t>(mask_h);
    if (tensorInfo.GetElementCount() != expectedMaskSize) {
        throw std::runtime_error(
            "Mascara de saida incompativel: esperado tensor float com "
            + std::to_string(expectedMaskSize) + " elementos, recebido "
            + std::to_string(tensorInfo.GetElementCount()));
    }

    const float* mask_ptr = outputs[0].GetTensorData<float>();

    QImage alpha = buildAlphaMask(
        mask_ptr, mask_w, mask_h, input.width(), input.height());

    return composite(input, alpha);
}

std::vector<float> BackgroundRemover::buildInputBlob(
    const QImage& img) const
{
    const int size = input_size_;
    QImage rgb = img.convertToFormat(QImage::Format_RGB32);
    QImage resized = rgb.scaled(
        size, size,
        Qt::IgnoreAspectRatio,
        Qt::SmoothTransformation);

    const int plane = size * size;
    std::vector<float> blob(static_cast<size_t>(3) * plane);

    // Acesso por scanline (Format_RGB32 garante QRgb alinhado) evita
    // a chamada QImage::pixel() por pixel, que domina o custo.
    for (int y = 0; y < size; ++y) {
        const QRgb* row = reinterpret_cast<const QRgb*>(
            resized.constScanLine(y));
        for (int x = 0; x < size; ++x) {
            const QRgb px = row[x];
            const float r = qRed(px) / 255.0f;
            const float g = qGreen(px) / 255.0f;
            const float b = qBlue(px) / 255.0f;

            const int idx = y * size + x;
            blob[idx]             = (r - MEAN[0]) / STD[0];
            blob[plane + idx]     = (g - MEAN[1]) / STD[1];
            blob[2 * plane + idx] = (b - MEAN[2]) / STD[2];
        }
    }
    return blob;
}

QImage BackgroundRemover::buildAlphaMask(
    const float* mask, int mask_w, int mask_h,
    int orig_w, int orig_h) const
{
    // Detecção automática de logit vs probabilidade.
    // Amostra espaçada (strided) por toda a máscara: amostrar so os
    // primeiros N pixels podia nao ser representativo (ex.: canto
    // uniforme numa imagem com objeto no centro).
    const int totalPixels = mask_w * mask_h;
    const int sample = std::min(totalPixels, 10000);
    const int stride = std::max(1, totalPixels / sample);
    bool needsSigmoid = false;
    for (int i = 0; i < totalPixels; i += stride) {
        const float v = mask[i];
        if (v < -0.1f || v > 1.1f) {
            needsSigmoid = true;
            break;
        }
    }
    // U2-Net/ISNet ja devolvem probabilidades 0..1 (sigmoid na ultima
    // camada). O clamp direto mantem as bordas fieis; a normalizacao
    // min-max anterior esticava o contraste e saturava o fundo quando
    // o objeto ocupava pouco da imagem.
    QImage mask_img(mask_w, mask_h, QImage::Format_Grayscale8);
    for (int y = 0; y < mask_h; ++y) {
        quint8* row = mask_img.scanLine(y);
        const float* src = mask + static_cast<size_t>(y) * mask_w;
        for (int x = 0; x < mask_w; ++x) {
            float v = src[x];
            if (needsSigmoid) {
                // Sigmoid estável para evitar overflow
                if (v >  20.0f) v = 1.0f;
                else if (v < -20.0f) v = 0.0f;
                else v = 1.0f / (1.0f + std::exp(-v));
            } else {
                v = std::clamp(v, 0.0f, 1.0f);
            }
            row[x] = static_cast<quint8>(v * 255.0f + 0.5f);
        }
    }
    return mask_img.scaled(
        orig_w, orig_h,
        Qt::IgnoreAspectRatio,
        Qt::SmoothTransformation);
}

QImage BackgroundRemover::composite(
    const QImage& img, const QImage& alpha) const
{
    QImage result = img.convertToFormat(QImage::Format_RGBA8888);
    const int width = result.width();
    const int height = result.height();
    for (int y = 0; y < height; ++y) {
        quint8* out_row = result.scanLine(y);
        const quint8* alpha_row = alpha.constScanLine(y);
        for (int x = 0; x < width; ++x) {
            out_row[x * 4 + 3] = alpha_row[x];
        }
    }
    return result;
}
