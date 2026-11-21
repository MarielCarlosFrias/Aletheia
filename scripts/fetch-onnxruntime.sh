#!/usr/bin/env bash
# Prepara o ONNX Runtime para build, sem depender dos releases do GitHub
# (que foram removidos). Estrategia:
#   - Headers (C++ API, platform-independent): sempre de vendor/onnxruntime/include
#   - Biblioteca nativa: se vendor/onnxruntime/lib ja tem a da plataforma,
#     usa essa; senao descarrega o pacote npm onnxruntime-node@<ver> e extrai
#     o binario da plataforma para vendor/onnxruntime/lib/.
#
# Uso: scripts/fetch-onnxruntime.sh [versao]   (default: 1.19.0)
# O CMakeLists.txt encontra vendor/onnxruntime/ automaticamente.
set -euo pipefail

VERSION="${1:-1.19.0}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VENDOR="$ROOT/vendor/onnxruntime"

if [ ! -f "$VENDOR/include/onnxruntime_cxx_api.h" ]; then
    echo "ERRO: headers nao encontrados em $VENDOR/include" >&2
    echo "O vendor/onnxruntime/include deve existir no repositorio." >&2
    exit 1
fi

# Detecao da plataforma
OS="$(uname -s)"
ARCH="$(uname -m)"
case "$OS" in
    Linux)  NPM_OS="linux" ;;
    Darwin) NPM_OS="darwin" ;;
    MINGW*|MSYS*|CYGWIN*) NPM_OS="win32" ;;
    *) echo "Plataforma nao suportada: $OS" >&2; exit 1 ;;
esac
case "$ARCH" in
    x86_64|amd64) NPM_ARCH="x64" ;;
    aarch64|arm64) NPM_ARCH="arm64" ;;
    *) echo "Arquitetura nao suportada: $ARCH" >&2; exit 1 ;;
esac

# Ja existe biblioteca nativa para esta plataforma?
case "$NPM_OS" in
    linux)  LIB_PATTERN="libonnxruntime.so*" ;;
    darwin) LIB_PATTERN="libonnxruntime*.dylib" ;;
    win32)  LIB_PATTERN="onnxruntime.dll" ;;
esac
if compgen -G "$VENDOR/lib/$LIB_PATTERN" > /dev/null; then
    echo "ONNX Runtime $VERSION ja presente em $VENDOR/lib ($NPM_OS/$NPM_ARCH)"
    exit 0
fi

# Descarrega o pacote npm e extrai o binario nativo
echo "A descarregar onnxruntime-node@$VERSION do npm (GitHub releases removidos)..."
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
TARBALL="$TMP/ort.tgz"
curl -fsSL --retry 5 --retry-delay 3 --retry-all-errors \
    -o "$TARBALL" \
    "https://registry.npmjs.org/onnxruntime-node/-/onnxruntime-node-$VERSION.tgz"
tar -xzf "$TARBALL" -C "$TMP"

NAPI_DIR="$TMP/package/bin/napi-v3/$NPM_OS/$NPM_ARCH"
if [ ! -d "$NAPI_DIR" ]; then
    echo "ERRO: binario nativo nao encontrado em $NAPI_DIR" >&2
    ls "$TMP/package/bin/" >&2 || true
    exit 1
fi

mkdir -p "$VENDOR/lib"
case "$NPM_OS" in
    linux)
        cp "$NAPI_DIR"/libonnxruntime.so* "$VENDOR/lib/"
        ;;
    darwin)
        # O CI compila universal2 (x86_64;arm64): o pacote npm traz as
        # duas fatias em separado (darwin/x64 e darwin/arm64); unem-se
        # com lipo num dylib universal. Copiar so a fatia do runner faz o
        # linker falhar com "found architecture 'arm64', required
        # architecture 'x86_64'" e simbolos indefinidos (_OrtGetApiBase).
        X64_DYLIB="$TMP/package/bin/napi-v3/darwin/x64/libonnxruntime.$VERSION.dylib"
        ARM_DYLIB="$TMP/package/bin/napi-v3/darwin/arm64/libonnxruntime.$VERSION.dylib"
        if [ -f "$X64_DYLIB" ] && [ -f "$ARM_DYLIB" ]; then
            rm -f "$VENDOR/lib/libonnxruntime.$VERSION.dylib"
            lipo -create "$X64_DYLIB" "$ARM_DYLIB" \
                -output "$VENDOR/lib/libonnxruntime.$VERSION.dylib"
        else
            # Fallback: copia a fatia da arquitetura do runner.
            cp "$NAPI_DIR"/libonnxruntime.*.dylib "$VENDOR/lib/"
        fi
        # Cria o symlink canonico
        cd "$VENDOR/lib"
        ln -sf "libonnxruntime.$VERSION.dylib" libonnxruntime.dylib 2>/dev/null || true
        ;;
    win32)
        cp "$NAPI_DIR/onnxruntime.dll" "$VENDOR/lib/"
        cp "$NAPI_DIR/DirectML.dll" "$VENDOR/lib/" 2>/dev/null || true
        # Opcional: so existe nos releases oficiais; o pacote npm nao a
        # traz (os providers CPU/DML vao dentro do proprio onnxruntime.dll).
        cp "$NAPI_DIR/onnxruntime_providers_shared.dll" "$VENDOR/lib/" 2>/dev/null || true
        # O CMake procura onnxruntime.lib (import library) no Windows; o npm
        # so traz a DLL. Gera a import lib com lib.exe (disponivel no runner
        # Windows com Visual Studio) a partir dos exports da DLL.
        if command -v lib.exe > /dev/null 2>&1; then
            # Extrai os simbolos exportados e gera o .def
            if command -v dumpbin.exe > /dev/null 2>&1; then
                dumpbin.exe /exports "$VENDOR/lib/onnxruntime.dll" \
                    | awk '/ordinal hint .* \[.*\]/{print "    "$NF" = "$NF}' \
                    > "$TMP/onnxruntime.def" 2>/dev/null || true
                if [ -s "$TMP/onnxruntime.def" ]; then
                    { echo "LIBRARY onnxruntime.dll"; cat "$TMP/onnxruntime.def"; } \
                        > "$TMP/onnxruntime_full.def"
                    lib.exe /def:"$TMP/onnxruntime_full.def" \
                        /out:"$VENDOR/lib/onnxruntime.lib" /machine:x64 2>/dev/null || true
                fi
            fi
        fi
        # Se a import lib nao foi gerada, cria um stub .lib copiando a DLL
        # (o CMake usa IMPORTED_IMPLIB; sem ele, apontamos para a propria DLL).
        if [ ! -f "$VENDOR/lib/onnxruntime.lib" ]; then
            cp "$VENDOR/lib/onnxruntime.dll" "$VENDOR/lib/onnxruntime.lib" 2>/dev/null || true
        fi
        ;;
esac

echo "ONNX Runtime $VERSION ($NPM_OS/$NPM_ARCH) instalado em $VENDOR/lib"
