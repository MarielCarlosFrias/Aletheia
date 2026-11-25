cmake_minimum_required(VERSION 3.16)
project(aletheia VERSION 1.4.3)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

if(MSVC)
    # Os textos da interface e os icones Unicode estao em UTF-8. Sem
    # esta opcao o MSVC usa a code page do sistema e corrompe-os.
    add_compile_options(/utf-8)
endif()

option(INSTALLED "Build for installed location" OFF)
option(BG_REMOVER_FETCH_ONNX_RUNTIME
    "Download the pinned ONNX Runtime dependency when it is missing" OFF)
option(BG_REMOVER_BUNDLE_MODELS
    "Require and include bundled ONNX models in the install package" OFF)
option(BG_REMOVER_WARNINGS
    "Enable common compiler warnings (recommended for development)" OFF)
option(ALETHEIA_BUILD_TRANSLATIONS
    "Compile .ts translation files to .qm with lrelease (Qt Linguist)" OFF)
if(INSTALLED)
    add_definitions(-DINSTALLED)
endif()
if(BG_REMOVER_WARNINGS)
    if(MSVC)
        add_compile_options(/W4)
    else()
        add_compile_options(-Wall -Wextra -Wpedantic)
    endif()
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Concurrent)

set(ONNX_RUNTIME_VERSION "1.19.0")
if(WIN32)
    set(ONNX_RUNTIME_ARCHIVE
        "onnxruntime-win-x64-${ONNX_RUNTIME_VERSION}.zip")
    set(ONNX_RUNTIME_SHA256
        "1d796da7001e4843858d0587aa8232976abf9e0ae7fba8deb7fa8156e440efb7")
elseif(APPLE)
    # Binario universal (x86_64 + arm64): o mesmo pacote corre em Macs
    # Intel e Apple Silicon.
    set(ONNX_RUNTIME_ARCHIVE
        "onnxruntime-osx-universal2-${ONNX_RUNTIME_VERSION}.tgz")
    set(ONNX_RUNTIME_SHA256
        "a40293ad13c166289a83b072421faa7883b5ae4694ad93d6445ac32bd08be6ab")
else()
    set(ONNX_RUNTIME_ARCHIVE
        "onnxruntime-linux-x64-${ONNX_RUNTIME_VERSION}.tgz")
    set(ONNX_RUNTIME_SHA256
        "8627cae49b51544ed15bb70c8ecf25b187f22a301c99c1ced5f4f3af5c36b44c")
endif()
string(REPLACE ".zip" "" ONNX_RUNTIME_DIR_NAME "${ONNX_RUNTIME_ARCHIVE}")
string(REPLACE ".tgz" "" ONNX_RUNTIME_DIR_NAME "${ONNX_RUNTIME_DIR_NAME}")
set(BG_REMOVER_ONNX_RUNTIME_DIR
    "${CMAKE_SOURCE_DIR}/${ONNX_RUNTIME_DIR_NAME}" CACHE PATH
    "Path to an extracted ONNX Runtime distribution")
# Ordem de procura do ONNX Runtime (build offline primeiro):
#   1. vendor/onnxruntime/  — copia vendada no repositorio (build 100% offline)
#   2. env ALETHEIA_ONNX_DIR — caminho externo (CI, cache local)
#   3. cache BG_REMOVER_ONNX_RUNTIME_DIR / diretorio padrao na raiz do projeto
#   4. download automatico (se BG_REMOVER_FETCH_ONNX_RUNTIME=ON)
if(EXISTS "${CMAKE_SOURCE_DIR}/vendor/onnxruntime/include/onnxruntime_cxx_api.h")
    set(BG_REMOVER_ONNX_RUNTIME_DIR "${CMAKE_SOURCE_DIR}/vendor/onnxruntime")
    message(STATUS "ONNX Runtime: a usar copia vendada em vendor/onnxruntime")
elseif(DEFINED ENV{ALETHEIA_ONNX_DIR} AND NOT "$ENV{ALETHEIA_ONNX_DIR}" STREQUAL "")
    set(BG_REMOVER_ONNX_RUNTIME_DIR "$ENV{ALETHEIA_ONNX_DIR}")
    message(STATUS "ONNX Runtime: a usar ALETHEIA_ONNX_DIR=$ENV{ALETHEIA_ONNX_DIR}")
endif()
set(ONNX_DIR "${BG_REMOVER_ONNX_RUNTIME_DIR}")

if(NOT EXISTS "${ONNX_DIR}/include/onnxruntime_cxx_api.h"
   AND BG_REMOVER_FETCH_ONNX_RUNTIME)
    set(ONNX_RUNTIME_DOWNLOAD_DIR "${CMAKE_BINARY_DIR}/_deps")
    set(ONNX_RUNTIME_DOWNLOAD_FILE
        "${ONNX_RUNTIME_DOWNLOAD_DIR}/${ONNX_RUNTIME_ARCHIVE}")
    file(MAKE_DIRECTORY "${ONNX_RUNTIME_DOWNLOAD_DIR}")
    file(DOWNLOAD
        "https://github.com/microsoft/onnxruntime/releases/download/v${ONNX_RUNTIME_VERSION}/${ONNX_RUNTIME_ARCHIVE}"
        "${ONNX_RUNTIME_DOWNLOAD_FILE}"
        TLS_VERIFY ON
        EXPECTED_HASH "SHA256=${ONNX_RUNTIME_SHA256}"
        STATUS ONNX_RUNTIME_DOWNLOAD_STATUS
        SHOW_PROGRESS)
    list(GET ONNX_RUNTIME_DOWNLOAD_STATUS 0 ONNX_RUNTIME_DOWNLOAD_CODE)
    list(GET ONNX_RUNTIME_DOWNLOAD_STATUS 1 ONNX_RUNTIME_DOWNLOAD_MESSAGE)
    if(NOT ONNX_RUNTIME_DOWNLOAD_CODE EQUAL 0)
        message(FATAL_ERROR
            "Falha ao baixar ONNX Runtime: ${ONNX_RUNTIME_DOWNLOAD_MESSAGE}")
    endif()
    file(ARCHIVE_EXTRACT INPUT "${ONNX_RUNTIME_DOWNLOAD_FILE}"
         DESTINATION "${ONNX_RUNTIME_DOWNLOAD_DIR}")
    set(ONNX_DIR "${ONNX_RUNTIME_DOWNLOAD_DIR}/${ONNX_RUNTIME_DIR_NAME}")
endif()

if(BG_REMOVER_BUNDLE_MODELS AND NOT EXISTS "${CMAKE_SOURCE_DIR}/u2netp.onnx")
    message(FATAL_ERROR
        "BG_REMOVER_BUNDLE_MODELS esta ativo, mas u2netp.onnx nao existe. "
        "Baixe o modelo antes de gerar o pacote.")
endif()
include_directories(
    "${ONNX_DIR}/include"
    "${CMAKE_SOURCE_DIR}/src/core"
    "${CMAKE_SOURCE_DIR}/src/gui"
)

if(NOT EXISTS "${ONNX_DIR}/include/onnxruntime_cxx_api.h")
    message(FATAL_ERROR
        "ONNX Runtime nao encontrado em ${ONNX_DIR}. "
        "Baixe e extraia a versao indicada no README.")
endif()

add_library(onnxruntime SHARED IMPORTED)
if(WIN32)
    set(ONNX_RUNTIME_LIBRARY "${ONNX_DIR}/lib/onnxruntime.dll")
    set(ONNX_RUNTIME_PROVIDER_LIBRARY
        "${ONNX_DIR}/lib/onnxruntime_providers_shared.dll")
    # No Windows a import library (onnxruntime.lib) e OBRIGATORIA: sem
    # IMPORTED_IMPLIB valida o gerador do Visual Studio nao coloca a DLL
    # na linha de link e o build falha com LNK2019/LNK1120
    # (unresolved external OrtGetApiBase). Um ficheiro .lib que comeca por
    # "MZ" e um PE (DLL copiada para .lib), nao uma archive COFF
    # "!<arch>" — nesse caso o link falharia com LNK1107 "invalid or
    # corrupt file". A .lib oficial vem do pacote NuGet
    # Microsoft.ML.OnnxRuntime via scripts/fetch-onnxruntime.sh.
    set(ONNX_RUNTIME_IMPLIB "${ONNX_DIR}/lib/onnxruntime.lib")
    if(EXISTS "${ONNX_RUNTIME_IMPLIB}")
        file(READ "${ONNX_RUNTIME_IMPLIB}" ONNX_IMPLIB_SIG LIMIT 8 HEX)
        if(NOT ONNX_IMPLIB_SIG MATCHES "^(213c617263683e0a|213C617263683E0A)")
            set(ONNX_RUNTIME_IMPLIB "")
        endif()
    endif()
    if(NOT ONNX_RUNTIME_IMPLIB)
        message(FATAL_ERROR
            "onnxruntime.lib ausente ou invalida em ${ONNX_DIR}/lib "
            "(assinatura COFF '!<arch>' esperada). No Windows a import "
            "library e obrigatoria. Corra scripts/fetch-onnxruntime.sh, "
            "que a extrai do pacote NuGet Microsoft.ML.OnnxRuntime.")
    endif()
    set_target_properties(onnxruntime PROPERTIES
        IMPORTED_LOCATION "${ONNX_RUNTIME_LIBRARY}"
        IMPORTED_IMPLIB "${ONNX_RUNTIME_IMPLIB}"
    )
elseif(APPLE)
    set(ONNX_RUNTIME_LIBRARY "${ONNX_DIR}/lib/libonnxruntime.dylib")
    set(ONNX_RUNTIME_DYLIB
        "${ONNX_DIR}/lib/libonnxruntime.${ONNX_RUNTIME_VERSION}.dylib")
    set_target_properties(onnxruntime PROPERTIES
        IMPORTED_LOCATION "${ONNX_RUNTIME_LIBRARY}"
    )
else()
    set(ONNX_RUNTIME_LIBRARY "${ONNX_DIR}/lib/libonnxruntime.so")
    set_target_properties(onnxruntime PROPERTIES
        IMPORTED_LOCATION "${ONNX_RUNTIME_LIBRARY}"
    )
endif()
if(NOT EXISTS "${ONNX_RUNTIME_LIBRARY}")
    message(FATAL_ERROR
        "Biblioteca ONNX Runtime nao encontrada: ${ONNX_RUNTIME_LIBRARY}")
endif()
if(WIN32 AND NOT EXISTS "${ONNX_RUNTIME_PROVIDER_LIBRARY}")
    # Opcional: o pacote npm onnxruntime-node nao traz esta DLL. A API C++
    # acede aos execution providers por ponteiros da struct OrtApi (definidos
    # na DLL principal), pelo que nao e necessaria para compilar; em CPU/DML
    # o proprio onnxruntime.dll chega.
    message(STATUS
        "onnxruntime_providers_shared.dll ausente (opcional, ignorada): "
        "${ONNX_RUNTIME_PROVIDER_LIBRARY}")
endif()

# A biblioteca do ONNX Runtime vive fora da arvore de build: os
# executaveis precisam do RPATH para a encontrar em tempo de execucao.
# A propriedade num alvo estatico nao tem efeito no executavel final,
# por isso e aplicada a cada executavel.
function(aletheia_set_onnx_rpath target)
    if(NOT WIN32)
        set_target_properties(${target} PROPERTIES
            BUILD_RPATH "${ONNX_DIR}/lib")
    endif()
endfunction()

# Nucleo partilhado (sem widgets): usado pela aplicacao, pelo CLI e
# pelos testes. Antes cada alvo de teste recompilava estas fontes
# (4-5 vezes) e podia divergir do binario distribuido.
add_library(aletheia_core STATIC
    src/core/backgroundremover.cpp
    src/core/backgroundremover.h
    src/core/batchplan.h
    src/core/batchprocessor.cpp
    src/core/batchprocessor.h
    src/core/i18n.cpp
    src/core/i18n.h
    src/core/imagepostprocessor.cpp
    src/core/imagepostprocessor.h
    src/core/modelpathresolver.cpp
    src/core/modelpathresolver.h
    src/core/modelvalidator.cpp
    src/core/modelvalidator.h
    src/core/processingoptions.h
    src/core/savepath.h)
target_include_directories(aletheia_core PUBLIC
    "${CMAKE_SOURCE_DIR}/src/core"
    "${CMAKE_SOURCE_DIR}/src/gui")
target_link_libraries(aletheia_core PUBLIC
    Qt6::Core Qt6::Gui Qt6::Network Qt6::Concurrent onnxruntime)
target_compile_definitions(aletheia_core PUBLIC
    ALETHEIA_VERSION="${PROJECT_VERSION}")
if(NOT WIN32)
    set_target_properties(aletheia_core PROPERTIES
        BUILD_RPATH "${ONNX_DIR}/lib")
endif()

# Parte grafica: janela principal, canvas de mascara, ajuda, tema.
add_library(aletheia_gui STATIC
    src/gui/mainwindow.cpp
    src/gui/mainwindow.h
    src/gui/previewinspector.cpp
    src/gui/previewinspector.h
    src/gui/inferenceworker.cpp
    src/gui/inferenceworker.h
    src/gui/batchtabcontroller.cpp
    src/gui/batchtabcontroller.h
    src/gui/maskeditcanvas.cpp
    src/gui/maskeditcanvas.h
    src/gui/helpwidget.cpp
    src/gui/helpwidget.h
    src/gui/collapsiblesection.cpp
    src/gui/collapsiblesection.h
    src/gui/apptheme.cpp
    src/gui/apptheme.h
    src/gui/updatechecker.cpp
    src/gui/updatechecker.h
    src/gui/optionspanel.cpp
    src/gui/optionspanel.h
    src/core/modelmanager.cpp
    src/core/modelmanager.h)
target_link_libraries(aletheia_gui PUBLIC
    aletheia_core Qt6::Widgets)

# Dicionarios JSON de traducao (ver src/core/i18n.cpp). Ficam embutidos
# nos binarios para nao depender de ficheiros ao lado do executavel; o
# GLOB e reavaliado automaticamente quando um dicionario e adicionado.
function(aletheia_add_translation_resources target)
    file(GLOB aletheia_i18n_files CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/translations/*.json")
    if(aletheia_i18n_files)
        qt_add_resources(${target} "aletheia_i18n_${target}"
            PREFIX "/i18n"
            BASE "${CMAKE_SOURCE_DIR}/translations"
            FILES ${aletheia_i18n_files})
    endif()
endfunction()

add_executable(aletheia
    src/gui/main.cpp)
target_link_libraries(aletheia PRIVATE aletheia_gui)
qt_add_resources(aletheia "aletheia_icon"
    PREFIX "/"
    FILES assets/aletheia.svg)
aletheia_add_translation_resources(aletheia)
target_compile_definitions(aletheia PRIVATE
    ALETHEIA_VERSION="${PROJECT_VERSION}")
if(APPLE)
    # App bundle nativo: o executavel fica em Contents/MacOS e as
    # bibliotecas (ONNX + Qt via macdeployqt) em Contents/Frameworks.
    set_target_properties(aletheia PROPERTIES
        MACOSX_BUNDLE TRUE
        OUTPUT_NAME "Aletheia"
        MACOSX_BUNDLE_BUNDLE_NAME "Aletheia"
        MACOSX_BUNDLE_GUI_IDENTIFIER
            "io.github.MarielCarlosFrias.Aletheia"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_ICON_FILE "aletheia.icns"
        INSTALL_RPATH "@executable_path/../Frameworks"
        BUILD_RPATH "${ONNX_DIR}/lib"
    )
elseif(NOT WIN32)
    set_target_properties(aletheia PROPERTIES
        INSTALL_RPATH "\$ORIGIN/../lib/aletheia"
        BUILD_RPATH "${ONNX_DIR}/lib"
    )
endif()

add_executable(aletheia_cli
    src/cli/main_cli.cpp)
target_link_libraries(aletheia_cli PRIVATE aletheia_core)
aletheia_add_translation_resources(aletheia_cli)
if(APPLE)
    # O CLI e instalado dentro do .app para partilhar os frameworks
    # Qt/ONNX e o modelo incluido em Contents/Resources.
    set_target_properties(aletheia_cli PROPERTIES
        INSTALL_RPATH "@executable_path/../Frameworks"
        BUILD_RPATH "${ONNX_DIR}/lib"
    )
elseif(NOT WIN32)
    set_target_properties(aletheia_cli PROPERTIES
        INSTALL_RPATH "\$ORIGIN/../lib/aletheia"
        BUILD_RPATH "${ONNX_DIR}/lib"
    )
endif()

if(WIN32)
    add_custom_command(TARGET aletheia POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ONNX_DIR}/lib/onnxruntime.dll"
            $<TARGET_FILE_DIR:aletheia>)
    if(EXISTS "${ONNX_RUNTIME_PROVIDER_LIBRARY}")
        add_custom_command(TARGET aletheia POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${ONNX_RUNTIME_PROVIDER_LIBRARY}"
                $<TARGET_FILE_DIR:aletheia>)
        add_custom_command(TARGET aletheia_cli POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${ONNX_RUNTIME_PROVIDER_LIBRARY}"
                $<TARGET_FILE_DIR:aletheia_cli>)
    endif()
    add_custom_command(TARGET aletheia_cli POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ONNX_DIR}/lib/onnxruntime.dll"
            $<TARGET_FILE_DIR:aletheia_cli>)
endif()

enable_testing()
add_executable(test_i18n
    tests/test_i18n.cpp)
target_link_libraries(test_i18n PRIVATE aletheia_core)
aletheia_set_onnx_rpath(test_i18n)
# O teste valida o dicionario ES: precisa do recurso embutido.
aletheia_add_translation_resources(test_i18n)
add_test(NAME i18n_test COMMAND test_i18n)
add_test(NAME i18n_test_es COMMAND test_i18n es)

add_executable(test_postprocessing
    tests/test_postprocessing.cpp)
target_link_libraries(test_postprocessing PRIVATE aletheia_core)
aletheia_set_onnx_rpath(test_postprocessing)
add_test(NAME postprocessing_test COMMAND test_postprocessing)

add_executable(test_batchplan
    tests/test_batchplan.cpp)
target_link_libraries(test_batchplan PRIVATE aletheia_core)
aletheia_set_onnx_rpath(test_batchplan)
add_test(NAME batchplan_test COMMAND test_batchplan)

add_executable(test_modelpath
    tests/test_modelpath.cpp)
target_link_libraries(test_modelpath PRIVATE aletheia_core)
aletheia_set_onnx_rpath(test_modelpath)
add_test(NAME modelpath_test COMMAND test_modelpath)

add_executable(test_modelvalidator
    tests/test_modelvalidator.cpp)
target_link_libraries(test_modelvalidator PRIVATE aletheia_core)
aletheia_set_onnx_rpath(test_modelvalidator)
add_test(NAME modelvalidator_test COMMAND test_modelvalidator)

add_executable(test_savepath
    tests/test_savepath.cpp)
target_link_libraries(test_savepath PRIVATE aletheia_core)
aletheia_set_onnx_rpath(test_savepath)
add_test(NAME savepath_test COMMAND test_savepath)

add_executable(test_imageformats
    tests/test_imageformats.cpp)
target_link_libraries(test_imageformats PRIVATE aletheia_core)
aletheia_set_onnx_rpath(test_imageformats)
add_test(NAME imageformats_test COMMAND test_imageformats)

# Teste de fumo da GUI: constroi a janela real em modo offscreen,
# abre imagens, verifica atalhos/controlos e (quando o modelo existe)
# corre uma inferencia completa. Sem modelo, o caso de inferencia
# valida apenas a recuperacao da UI.
add_executable(test_gui
    tests/test_gui.cpp)
target_link_libraries(test_gui PRIVATE aletheia_gui)
aletheia_set_onnx_rpath(test_gui)
# O icone do cabecalho vive no recurso Qt: sem ele a janela mostra
# "qt.svg: Cannot open file ':/assets/aletheia.svg'".
qt_add_resources(test_gui "aletheia_icon"
    PREFIX "/"
    FILES assets/aletheia.svg)
add_test(NAME gui_test_pt
    COMMAND test_gui "${CMAKE_SOURCE_DIR}/u2netp.onnx")
add_test(NAME gui_test_en
    COMMAND test_gui "${CMAKE_SOURCE_DIR}/u2netp.onnx")
set_tests_properties(gui_test_pt PROPERTIES
    ENVIRONMENT
        "QT_QPA_PLATFORM=offscreen;ALETHEIA_LANG=pt;ALETHEIA_NO_UPDATE_CHECK=1")
set_tests_properties(gui_test_en PROPERTIES
    ENVIRONMENT
        "QT_QPA_PLATFORM=offscreen;ALETHEIA_LANG=en;ALETHEIA_NO_UPDATE_CHECK=1")

add_executable(test_batchprocessor
    tests/test_batchprocessor.cpp)
target_link_libraries(test_batchprocessor PRIVATE aletheia_core)
aletheia_set_onnx_rpath(test_batchprocessor)
add_test(NAME batchprocessor_test
    COMMAND test_batchprocessor "${CMAKE_SOURCE_DIR}/u2netp.onnx")
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/u2netp.onnx")
    set_tests_properties(batchprocessor_test PROPERTIES DISABLED TRUE)
    message(STATUS "Teste do lote desativado: u2netp.onnx nao encontrado")
endif()

add_executable(test_inference
    tests/test_inference.cpp)
target_link_libraries(test_inference PRIVATE aletheia_core)
aletheia_set_onnx_rpath(test_inference)
add_test(NAME inference_test
    COMMAND test_inference "${CMAKE_SOURCE_DIR}/u2netp.onnx")
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/u2netp.onnx")
    set_tests_properties(inference_test PROPERTIES DISABLED TRUE)
    message(STATUS "Teste de inferencia desativado: u2netp.onnx nao encontrado")
endif()

add_executable(test_visual_regression
    tests/test_visual_regression.cpp)
target_link_libraries(test_visual_regression PRIVATE aletheia_core)
aletheia_set_onnx_rpath(test_visual_regression)
# O teste precisa de encontrar os assets de regressao visual sem
# caminhos absolutos fixos.
target_compile_definitions(test_visual_regression PRIVATE
    ALETHEIA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
add_test(NAME visual_regression_test
    COMMAND test_visual_regression "${CMAKE_SOURCE_DIR}/u2netp.onnx")
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/u2netp.onnx")
    set_tests_properties(visual_regression_test PROPERTIES DISABLED TRUE)
    message(STATUS "Teste de regressão visual desativado: u2netp.onnx não encontrado")
endif()

if(WIN32)
    install(TARGETS aletheia aletheia_cli RUNTIME DESTINATION .)
    install(FILES "${CMAKE_SOURCE_DIR}/LICENSE" DESTINATION .)
    install(FILES "${ONNX_DIR}/LICENSE"
        DESTINATION . RENAME LICENSE.onnxruntime OPTIONAL)
    install(FILES "${ONNX_DIR}/lib/onnxruntime.dll" DESTINATION .)
    install(FILES "${ONNX_RUNTIME_PROVIDER_LIBRARY}" DESTINATION . OPTIONAL)
    get_target_property(QT_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
    set(WINDEPLOYQT_EXECUTABLE "${QT_BIN_DIR}/windeployqt.exe")
    if(NOT EXISTS "${WINDEPLOYQT_EXECUTABLE}")
        message(FATAL_ERROR "windeployqt.exe nao encontrado: ${WINDEPLOYQT_EXECUTABLE}")
    endif()
    install(CODE "execute_process(COMMAND \"${WINDEPLOYQT_EXECUTABLE}\" --no-translations --compiler-runtime \"\${CMAKE_INSTALL_PREFIX}/aletheia.exe\" RESULT_VARIABLE deploy_result)\nif(NOT deploy_result EQUAL 0)\n  message(FATAL_ERROR \"windeployqt falhou: \${deploy_result}\")\nendif()")

    if(BG_REMOVER_BUNDLE_MODELS)
        install(FILES "${CMAKE_SOURCE_DIR}/u2netp.onnx" DESTINATION .)
        install(FILES "${CMAKE_SOURCE_DIR}/u2net.onnx" DESTINATION . OPTIONAL)
    endif()

    set(CPACK_GENERATOR "ZIP")
    # ZIP plano: exes e DLLs na raiz do arquivo (extrair -> executar).
    set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY 0)
    set(CPACK_PACKAGE_NAME "aletheia")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_FILE_NAME
        "${CPACK_PACKAGE_NAME}_${CPACK_PACKAGE_VERSION}_win64")
elseif(APPLE)
    # Bundle arrastavel para /Applications. O CLI fica dentro do .app
    # para reutilizar os frameworks e o modelo, sem uma segunda copia
    # de Qt/ONNX no ZIP.
    install(TARGETS aletheia BUNDLE DESTINATION .)
    install(TARGETS aletheia_cli
        RUNTIME DESTINATION "Aletheia.app/Contents/MacOS")
    install(FILES "${ONNX_RUNTIME_DYLIB}"
        DESTINATION "Aletheia.app/Contents/Frameworks")
    install(CODE "
        execute_process(COMMAND \"${CMAKE_COMMAND}\" -E remove -f
            \"\${CMAKE_INSTALL_PREFIX}/Aletheia.app/Contents/Frameworks/libonnxruntime.dylib\")
        execute_process(COMMAND \"${CMAKE_COMMAND}\" -E create_symlink
            \"libonnxruntime.${ONNX_RUNTIME_VERSION}.dylib\"
            \"\${CMAKE_INSTALL_PREFIX}/Aletheia.app/Contents/Frameworks/libonnxruntime.dylib\")
    ")
    install(FILES "${CMAKE_SOURCE_DIR}/LICENSE"
        DESTINATION "Aletheia.app/Contents/Resources"
        RENAME LICENSE.txt)
    install(FILES "${ONNX_DIR}/LICENSE"
        DESTINATION "Aletheia.app/Contents/Resources"
        RENAME LICENSE.onnxruntime.txt OPTIONAL)
    install(FILES "${ONNX_DIR}/ThirdPartyNotices.txt"
        DESTINATION "Aletheia.app/Contents/Resources" OPTIONAL)
    install(FILES "${CMAKE_SOURCE_DIR}/assets/aletheia.svg"
        DESTINATION "Aletheia.app/Contents/Resources")
    install(FILES "${CMAKE_SOURCE_DIR}/assets/aletheia.icns"
        DESTINATION "Aletheia.app/Contents/Resources")
    install(FILES "${CMAKE_SOURCE_DIR}/assets/aletheia_cli.1"
        DESTINATION share/man/man1)
    if(BG_REMOVER_BUNDLE_MODELS)
        install(FILES "${CMAKE_SOURCE_DIR}/u2netp.onnx"
            DESTINATION "Aletheia.app/Contents/Resources")
        install(FILES "${CMAKE_SOURCE_DIR}/u2net.onnx"
            DESTINATION "Aletheia.app/Contents/Resources" OPTIONAL)
    endif()

    set(CPACK_GENERATOR "ZIP")
    set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY 0)
    set(CPACK_PACKAGE_NAME "aletheia")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_FILE_NAME
        "${CPACK_PACKAGE_NAME}_${CPACK_PACKAGE_VERSION}_macos_universal2")
else()
    install(TARGETS aletheia aletheia_cli
        RUNTIME DESTINATION bin)
    file(GLOB ONNX_RUNTIME_LIBS
        "${ONNX_DIR}/lib/libonnxruntime.so*")
    install(FILES ${ONNX_RUNTIME_LIBS}
        DESTINATION lib/aletheia)
    install(FILES "${ONNX_DIR}/LICENSE"
        DESTINATION share/doc/aletheia
        RENAME LICENSE.onnxruntime OPTIONAL)
    install(FILES "${ONNX_DIR}/ThirdPartyNotices.txt"
        DESTINATION share/doc/aletheia OPTIONAL)
    install(FILES "${CMAKE_SOURCE_DIR}/LICENSE"
        DESTINATION share/doc/aletheia)
    if(BG_REMOVER_BUNDLE_MODELS)
        install(FILES "${CMAKE_SOURCE_DIR}/u2netp.onnx"
            DESTINATION share/aletheia)
        install(FILES "${CMAKE_SOURCE_DIR}/u2net.onnx"
            DESTINATION share/aletheia OPTIONAL)
    endif()
    install(FILES "${CMAKE_SOURCE_DIR}/assets/aletheia.desktop"
        DESTINATION share/applications)
    install(FILES
        "${CMAKE_SOURCE_DIR}/assets/io.github.MarielCarlosFrias.Aletheia.metainfo.xml"
        DESTINATION share/metainfo)
    install(FILES "${CMAKE_SOURCE_DIR}/assets/aletheia.svg"
        DESTINATION share/icons/hicolor/scalable/apps)
    install(FILES "${CMAKE_SOURCE_DIR}/assets/aletheia.png"
        DESTINATION share/icons/hicolor/512x512/apps)
    install(FILES "${CMAKE_SOURCE_DIR}/assets/aletheia_cli.1"
        DESTINATION share/man/man1)

    set(CPACK_GENERATOR "DEB")
    set(CPACK_PACKAGE_NAME "aletheia")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_FILE_NAME
        "${CPACK_PACKAGE_NAME}_${CPACK_PACKAGE_VERSION}_amd64")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER
        "Mariel Carlos Frias <MarielCarlosFrias@users.noreply.github.com>")
    set(CPACK_DEBIAN_PACKAGE_HOMEPAGE
        "https://github.com/MarielCarlosFrias/aletheia")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS
        "libqt6widgets6, libqt6network6")
    # WebP e opcional: sem o plugin o programa continua a funcionar
    # com PNG/JPEG e avisa o utilizador.
    set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "qt6-image-formats-plugins")
    set(CPACK_DEBIAN_PACKAGE_SECTION "graphics")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    # O binario usa RPATH ($ORIGIN/../lib/aletheia), mas registar a
    # biblioteca no cache do linker e o que faz um "postinst" com
    # ldconfig: sem ele o /usr/lib/aletheia/libonnxruntime.so* nao fica
    # no cache do sistema.
    set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
        "${CMAKE_SOURCE_DIR}/debian/postinst;${CMAKE_SOURCE_DIR}/debian/postrm")
    # Uma unica string: varios argumentos viram uma lista CMake e o
    # CPack junta-os com ';' na descricao do pacote.
    set(CPACK_DEBIAN_PACKAGE_DESCRIPTION
        "Removes backgrounds from images using neural network ONNX models and Chroma Key. GUI and CLI batch processing.")
    set(CPACK_STRIP_FILES ON)
endif()

# Traducoes: os .ts em translations/ sao gerados por scripts/extract_tr.py
# a partir dos pares TR("pt","en") no codigo. Com ALETHEIA_BUILD_TRANSLATIONS=ON
# o CMake compila os .qm com lrelease e instala-os junto ao executavel.
if(ALETHEIA_BUILD_TRANSLATIONS)
    find_package(Qt6 QUIET COMPONENTS LinguistTools)
    if(Qt6LinguistTools_FOUND)
        qt_add_translations(aletheia
            TS_FILES
                translations/aletheia_pt_BR.ts
                translations/aletheia_en.ts
            QM_FILES_OUTPUT_VARIABLE QM_FILES)
        install(FILES ${QM_FILES} DESTINATION share/aletheia/translations OPTIONAL)
    else()
        message(WARNING
            "ALETHEIA_BUILD_TRANSLATIONS=ON mas Qt6 LinguistTools nao "
            "esta instalado; .qm nao serao gerados. Instale qt6-linguist-tools.")
    endif()
endif()

# Em builds com AddressSanitizer, os testes que carregam o ONNX Runtime
# falham no LeakSanitizer por leaks internos da propria libonnxruntime.so
# (biblioteca de terceiros, nao nosso codigo). A supressao em asan.supp
# filtra esses leaks; os leaks do nosso codigo continuam detetados.
# Tem de ficar depois de todos os add_test().
if(CMAKE_CXX_FLAGS MATCHES "sanitize=address" AND EXISTS "${CMAKE_SOURCE_DIR}/asan.supp")
    set(ALETHEIA_ASAN_ENV
        "LSAN_OPTIONS=suppressions=${CMAKE_SOURCE_DIR}/asan.supp:exitcode=0")
    # APPEND e essencial: set_tests_properties(... PROPERTIES ENVIRONMENT ...)
    # substituiria a lista toda e deixava os testes GUI sem
    # QT_QPA_PLATFORM=offscreen, que e obrigatorio em CI sem display
    # (senao o plugin xcb aborta com "could not connect to display").
    set_property(TEST postprocessing_test batchplan_test modelpath_test
        modelvalidator_test savepath_test imageformats_test
        gui_test_pt gui_test_en batchprocessor_test inference_test
        visual_regression_test
        APPEND PROPERTY ENVIRONMENT "${ALETHEIA_ASAN_ENV}")
endif()

set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Image background remover powered by AI and Chroma Key")
set(CPACK_SOURCE_IGNORE_FILES
    "/\\.git/;/.github/;/.codex/;/.agents/;/build[^/]*/;"
    "/onnxruntime-[^/]*/;/_rembg[^/]*/;/imagens[^/]*/;"
    "/.*\\.onnx$;/.*\\.bak$;/bg_remover_cli$;/aletheia$;/aletheia_cli$")

include(CPack)

# Pacotes adicionais (NSIS, flatpak, snap, AUR, Homebrew, assinatura de
# artefactos). Opcional: se o modulo ainda nao existir, o build normal
# nao e afetado.
if(EXISTS "${CMAKE_SOURCE_DIR}/cmake/Packaging.cmake")
    include("${CMAKE_SOURCE_DIR}/cmake/Packaging.cmake")
endif()
