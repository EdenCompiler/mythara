cmake_minimum_required(VERSION 3.16)

project(Mythara VERSION 4.0.0 LANGUAGES C)

include(GNUInstallDirs)
include(CTest)

option(MYTHARA_AUDIO "Habilita reproducao de audio" ON)
option(MYTHARA_AVISOS_COMO_ERROS "Trata avisos do compilador como erros" OFF)

set(MYTHARA_MODULOS_INTERNOS
    src/interno/preludio.inc
    src/interno/base.inc
    src/interno/modelo.inc
    src/interno/renderizador.inc
    src/interno/interface.inc
    src/interno/plataforma.inc
    src/interno/persistencia.inc
    src/interno/recursos_exportacao.inc
    src/interno/aplicativo.inc
    src/interno/editor.inc
    src/interno/modais.inc
    src/interno/runtime.inc
    src/interno/programa.inc)

add_executable(mythara src/mythara.c src/formato_binario.c src/idioma.c)
target_sources(mythara PRIVATE ${MYTHARA_MODULOS_INTERNOS})
set_source_files_properties(${MYTHARA_MODULOS_INTERNOS} PROPERTIES HEADER_FILE_ONLY TRUE)
target_compile_features(mythara PRIVATE c_std_11)

if(MSVC)
    target_compile_definitions(mythara PRIVATE _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS)
    target_compile_options(mythara PRIVATE /W4)
    if(MYTHARA_AVISOS_COMO_ERROS)
        target_compile_options(mythara PRIVATE /WX)
    endif()
else()
    target_compile_options(mythara PRIVATE -Wall -Wextra -Wpedantic)
    if(MYTHARA_AVISOS_COMO_ERROS)
        target_compile_options(mythara PRIVATE -Werror)
    endif()
endif()

if(WIN32)
    target_link_libraries(mythara PRIVATE gdi32 user32 shell32)
    if(MYTHARA_AUDIO)
        target_link_libraries(mythara PRIVATE winmm)
    else()
        target_compile_definitions(mythara PRIVATE MYTHARA_SEM_AUDIO)
    endif()
    if(MINGW)
        target_link_libraries(mythara PRIVATE m)
    endif()
else()
    find_package(X11 REQUIRED)
    find_package(Threads REQUIRED)
    target_include_directories(mythara PRIVATE ${X11_INCLUDE_DIR})
    target_link_libraries(mythara PRIVATE ${X11_LIBRARIES} Threads::Threads m)

    if(MYTHARA_AUDIO)
        find_package(ALSA REQUIRED)
        target_include_directories(mythara PRIVATE ${ALSA_INCLUDE_DIRS})
        target_link_libraries(mythara PRIVATE ${ALSA_LIBRARIES})
    else()
        target_compile_definitions(mythara PRIVATE MYTHARA_SEM_AUDIO)
    endif()
endif()

if(BUILD_TESTING)
    add_test(NAME mythara_autotestes COMMAND mythara --autoteste)
    set_tests_properties(mythara_autotestes PROPERTIES TIMEOUT 60)
endif()

install(TARGETS mythara RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES README.md CHANGELOG.md LICENSE DESTINATION ${CMAKE_INSTALL_DOCDIR})
