# Optional render-core test target (H1).
# Build: cmake -DBUILD_TESTING=ON .. && make walltz_render_test
# Run:   ./build/tests/walltz_render_test   (or ctest)

include(CTest)

if(BUILD_TESTING)
    add_executable(walltz_render_test render_test.cpp)

    target_sources(walltz_render_test PRIVATE
        ../src/WallpaperProcessor.cpp
        ../src/blur_presets.cpp
        ../src/WallpaperAnalyzer.cpp
    )

    target_include_directories(walltz_render_test PRIVATE ../src)

    target_link_libraries(walltz_render_test PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Svg
        Qt6::Concurrent
        KF6::I18n
        KF6::CoreAddons
    )

    # Match the app's performance flags so the test exercises the same codegen.
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(walltz_render_test PRIVATE
            -march=native
            -fno-rtti
            -ffunction-sections
            -fdata-sections
        )
    endif()

    add_test(NAME walltz_render_test COMMAND walltz_render_test)

    # S1/S2/S3/P1/P2 blur-core regression (range-clamp equivalence + box gate).
    add_executable(walltz_blur_regression blur_regression.cpp)
    target_sources(walltz_blur_regression PRIVATE
        ../src/WallpaperProcessor.cpp
        ../src/blur_presets.cpp
        ../src/WallpaperAnalyzer.cpp
    )
    target_include_directories(walltz_blur_regression PRIVATE ../src)
    target_link_libraries(walltz_blur_regression PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Svg
        Qt6::Concurrent
        KF6::I18n
        KF6::CoreAddons
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(walltz_blur_regression PRIVATE
            -march=native
            -fno-rtti
            -ffunction-sections
            -fdata-sections
        )
    endif()
    add_test(NAME walltz_blur_regression COMMAND walltz_blur_regression)

    # F2/F6 serialization round-trip (remember/restore, presets, clamping).
    add_executable(walltz_serialization_test verify_serialization.cpp)
    target_sources(walltz_serialization_test PRIVATE
        ../src/WallpaperProcessor.cpp
        ../src/blur_presets.cpp
        ../src/WallpaperAnalyzer.cpp
    )
    target_include_directories(walltz_serialization_test PRIVATE ../src)
    target_link_libraries(walltz_serialization_test PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Svg
        Qt6::Concurrent
        KF6::I18n
        KF6::CoreAddons
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(walltz_serialization_test PRIVATE
            -march=native
            -fno-rtti
            -ffunction-sections
            -fdata-sections
        )
    endif()
    add_test(NAME walltz_serialization_test COMMAND walltz_serialization_test)
endif()
