add_executable(test_geometry test_geometry.cpp)
target_link_libraries(test_geometry PRIVATE led_render)
add_test(NAME geometry COMMAND test_geometry)

add_executable(test_schedule test_schedule.cpp)
target_link_libraries(test_schedule PRIVATE led_core)
add_test(NAME schedule COMMAND test_schedule)

add_executable(test_playback test_playback.cpp)
target_link_libraries(test_playback PRIVATE led_playback Qt6::Gui)

add_executable(test_interactive test_interactive.cpp)
target_link_libraries(test_interactive PRIVATE led_interactive led_playback)
add_test(NAME interactive COMMAND test_interactive)
set_tests_properties(interactive PROPERTIES TIMEOUT 60)

add_executable(test_serializer test_serializer.cpp)
target_link_libraries(test_serializer PRIVATE led_config)
add_test(NAME serializer COMMAND test_serializer)

if(WIN32)
    add_executable(test_d3d11_smoke test_d3d11_smoke.cpp)
    target_link_libraries(test_d3d11_smoke PRIVATE led_render led_display)
    add_test(NAME d3d11_smoke COMMAND test_d3d11_smoke)
endif()

add_executable(test_video test_video.cpp)
target_link_libraries(test_video PRIVATE led_media)

# ffmpeg 명령이 있으면 테스트 영상을 만들어 디코딩 검사
find_program(FFMPEG_EXECUTABLE ffmpeg HINTS "${FFMPEG_BIN_DIR}")
if(FFMPEG_EXECUTABLE)
    set(SAMPLE_VIDEO "${CMAKE_CURRENT_BINARY_DIR}/sample_320x240_30fps_1s.mp4")
    add_test(NAME make_sample_video
        COMMAND "${FFMPEG_EXECUTABLE}" -y -loglevel error -f lavfi -i testsrc2=size=320x240:rate=30
                -t 1 -pix_fmt yuv420p "${SAMPLE_VIDEO}")
    set_tests_properties(make_sample_video PROPERTIES FIXTURES_SETUP sample_video)
    add_test(NAME video COMMAND test_video "${SAMPLE_VIDEO}")
    set_tests_properties(video PROPERTIES FIXTURES_REQUIRED sample_video TIMEOUT 60)
    add_test(NAME playback COMMAND test_playback "${SAMPLE_VIDEO}")
    set_tests_properties(playback PROPERTIES FIXTURES_REQUIRED sample_video TIMEOUT 120)
else()
    message(STATUS "ffmpeg 실행 파일 없음: 영상 디코딩 테스트 생략")
    add_test(NAME playback COMMAND test_playback)
    set_tests_properties(playback PROPERTIES TIMEOUT 120)
endif()
