# FFmpeg 찾기 (avformat, avcodec, avutil, swscale)
#
# Windows: FFMPEG_ROOT (CMake 변수 또는 환경 변수)에 FFmpeg "shared" 빌드 폴더 지정
#          (include/, lib/, bin/ 폴더가 있는 위치)
# Linux:   pkg-config 사용
#
# 결과: FFmpeg::FFmpeg 타깃, FFMPEG_BIN_DIR (Windows DLL 폴더)

if(NOT FFMPEG_ROOT AND DEFINED ENV{FFMPEG_ROOT})
    set(FFMPEG_ROOT "$ENV{FFMPEG_ROOT}")
endif()

if(NOT FFMPEG_ROOT)
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(PC_FFMPEG QUIET IMPORTED_TARGET libavformat libavcodec libavutil libswscale libavdevice)
        if(PC_FFMPEG_FOUND)
            if(NOT TARGET FFmpeg::FFmpeg)
                add_library(FFmpeg::FFmpeg INTERFACE IMPORTED)
                target_link_libraries(FFmpeg::FFmpeg INTERFACE PkgConfig::PC_FFMPEG)
            endif()
            set(FFmpeg_FOUND TRUE)
            set(FFMPEG_BIN_DIR "")
            return()
        endif()
    endif()
endif()

find_path(FFMPEG_INCLUDE_DIR libavcodec/avcodec.h
    HINTS "${FFMPEG_ROOT}"
    PATH_SUFFIXES include)

set(_ffmpeg_libs avdevice avformat avcodec avutil swscale)
set(_ffmpeg_lib_vars)
foreach(_lib IN LISTS _ffmpeg_libs)
    find_library(FFMPEG_${_lib}_LIBRARY NAMES ${_lib}
        HINTS "${FFMPEG_ROOT}"
        PATH_SUFFIXES lib lib64)
    list(APPEND _ffmpeg_lib_vars FFMPEG_${_lib}_LIBRARY)
endforeach()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(FFmpeg
    REQUIRED_VARS FFMPEG_INCLUDE_DIR ${_ffmpeg_lib_vars}
    FAIL_MESSAGE "FFmpeg를 찾을 수 없음. FFMPEG_ROOT에 FFmpeg shared 빌드 폴더를 지정하세요.")

if(FFmpeg_FOUND AND NOT TARGET FFmpeg::FFmpeg)
    add_library(FFmpeg::FFmpeg INTERFACE IMPORTED)
    set_target_properties(FFmpeg::FFmpeg PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${FFMPEG_INCLUDE_DIR}")
    foreach(_lib IN LISTS _ffmpeg_libs)
        target_link_libraries(FFmpeg::FFmpeg INTERFACE "${FFMPEG_${_lib}_LIBRARY}")
    endforeach()
endif()

if(FFMPEG_ROOT AND EXISTS "${FFMPEG_ROOT}/bin")
    set(FFMPEG_BIN_DIR "${FFMPEG_ROOT}/bin")
else()
    set(FFMPEG_BIN_DIR "")
endif()
