cmake_minimum_required(VERSION 3.21)

project(gsj_led_player VERSION 0.7.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# 실행 파일과 테스트를 한 폴더에 모아 DLL 공유
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")

option(LED_BUILD_TESTS "테스트 빌드" ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets Network SerialPort)
find_package(FFmpeg REQUIRED)

if(MSVC)
    # /utf-8: 소스의 한글 문자열을 UTF-8로 처리 (없으면 한글 깨짐)
    add_compile_options(/utf-8 /W3 /permissive- /Zc:__cplusplus /MP)
endif()
if(WIN32)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
endif()

set(SRC "${CMAKE_CURRENT_SOURCE_DIR}/src")

# ---------------------------------------------------------------- core
add_library(led_core STATIC
    src/core/Geometry.cpp
    src/core/Project.cpp
    src/core/ScheduleLogic.cpp
    src/core/TickerMotion.cpp
    src/core/Snap.cpp
    src/core/ControlCommand.cpp
    src/core/OscParser.cpp
    src/core/TimelineEdit.cpp
    src/core/LayerContent.cpp
    src/core/Log.cpp)
target_include_directories(led_core PUBLIC "${SRC}")

# ---------------------------------------------------------------- config
add_library(led_config STATIC
    src/config/ProjectSerializer.cpp)
target_link_libraries(led_config PUBLIC led_core Qt6::Core)

# ---------------------------------------------------------------- media
add_library(led_media STATIC
    src/media/Frame.cpp
    src/media/VideoSource.cpp
    src/media/ImageSource.cpp
    src/media/QImageFrame.cpp
    src/media/MediaFactory.cpp
    src/media/TickerStrip.cpp
    src/media/CaptureDevices.cpp
    src/media/MediaInfo.cpp
    src/media/AudioSource.cpp
    src/media/AudioOutput.cpp)
# miniaudio (외부 코드): 경고 끔
set_source_files_properties(src/media/AudioOutput.cpp PROPERTIES COMPILE_OPTIONS "$<IF:$<CXX_COMPILER_ID:MSVC>,/w,-w>")
target_link_libraries(led_media PUBLIC led_core Qt6::Gui PRIVATE FFmpeg::FFmpeg)
target_include_directories(led_media PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniaudio")
if(UNIX AND NOT APPLE)
    target_link_libraries(led_media PRIVATE dl pthread m)
endif()

# ---------------------------------------------------------------- render
add_library(led_render STATIC src/render/Scene.cpp)
if(WIN32)
    target_sources(led_render PRIVATE src/render/d3d11/D3D11Renderer.cpp)
    target_link_libraries(led_render PUBLIC d3d11 dxgi d3dcompiler)
endif()
target_link_libraries(led_render PUBLIC led_media)

# ---------------------------------------------------------------- playback
add_library(led_playback STATIC
    src/playback/PlaybackEngine.cpp
    src/playback/RegionView.cpp
    src/playback/TickerPlayer.cpp
    src/playback/RuntimeOverrides.cpp
    src/playback/TimelinePlayer.cpp)
target_link_libraries(led_playback PUBLIC led_render led_media)

# ---------------------------------------------------------------- display
add_library(led_display STATIC src/display/DisplayInfo.cpp)
if(WIN32)
    target_sources(led_display PRIVATE src/display/DisplayEnumerator_win.cpp)
    target_link_libraries(led_display PUBLIC led_core dxgi)
else()
    target_sources(led_display PRIVATE src/display/DisplayEnumerator_qt.cpp)
    target_link_libraries(led_display PUBLIC led_core Qt6::Gui)
endif()

# ---------------------------------------------------------------- testpattern
add_library(led_testpattern STATIC src/testpattern/TestPattern.cpp)
target_link_libraries(led_testpattern PUBLIC led_core Qt6::Gui)

# ---------------------------------------------------------------- output
add_library(led_output STATIC src/output/OutputManager.cpp)
if(WIN32)
    target_sources(led_output PRIVATE src/output/win32/Win32OutputWindow.cpp)
else()
    target_sources(led_output PRIVATE src/output/qt/QtOutputWindow.cpp)
endif()
target_link_libraries(led_output PUBLIC led_render led_display led_testpattern led_media Qt6::Gui)

# ---------------------------------------------------------------- schedule
add_library(led_schedule STATIC src/schedule/ScheduleRunner.cpp)
target_link_libraries(led_schedule PUBLIC led_core Qt6::Core)

# ---------------------------------------------------------------- interactive
add_library(led_interactive STATIC
    src/interactive/InteractiveHub.cpp
    src/interactive/MidiInput.cpp)
target_link_libraries(led_interactive PUBLIC led_core Qt6::Network Qt6::SerialPort)
if(WIN32)
    target_link_libraries(led_interactive PUBLIC winmm)
endif()

# ---------------------------------------------------------------- 실행 파일
add_executable(gsj_led_player
    src/app/main.cpp
    src/app/LogBridge.cpp
    src/ui/Theme.cpp
    src/ui/Icons.cpp
    src/ui/ProjectController.cpp
    src/ui/CanvasView.cpp
    src/ui/RegionPanel.cpp
    src/ui/OutputPanel.cpp
    src/ui/CanvasPanel.cpp
    src/ui/LogPanel.cpp
    src/ui/ColorButton.cpp
    src/ui/EmergencyHotkey.cpp
    src/ui/PlaylistEditor.cpp
    src/ui/LiveInputDialog.cpp
    src/ui/TickerEditor.cpp
    src/ui/ContentEditor.cpp
    src/ui/ScenePanel.cpp
    src/ui/SchedulePanel.cpp
    src/ui/MainWindow.cpp
    src/ui/MainWindowInteractive.cpp
    src/ui/MainWindowOperator.cpp
    src/ui/operator/ProgramMonitor.cpp
    src/ui/operator/CueProgressBar.cpp
    src/ui/operator/OperatorSceneGrid.cpp
    src/ui/operator/OperatorRegionControls.cpp
    src/ui/operator/OperatorPanel.cpp
    src/ui/InteractivePanel.cpp
    src/ui/timeline/TimelineView.cpp
    src/ui/timeline/TimelinePropertyPanel.cpp
    src/ui/timeline/TimelineEditor.cpp)
target_link_libraries(gsj_led_player PRIVATE
    led_config led_playback led_output led_display led_schedule led_interactive Qt6::Widgets)
target_compile_definitions(gsj_led_player PRIVATE LED_PLAYER_VERSION="${PROJECT_VERSION}")
set_target_properties(gsj_led_player PROPERTIES WIN32_EXECUTABLE ON)

# Windows: FFmpeg DLL을 실행 파일 옆으로 복사
if(WIN32 AND FFMPEG_BIN_DIR)
    file(GLOB FFMPEG_DLLS "${FFMPEG_BIN_DIR}/*.dll")
    if(FFMPEG_DLLS)
        add_custom_command(TARGET gsj_led_player POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${FFMPEG_DLLS} "$<TARGET_FILE_DIR:gsj_led_player>"
            COMMENT "FFmpeg DLL 복사")
    endif()
endif()

# ---------------------------------------------------------------- 테스트
if(LED_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
