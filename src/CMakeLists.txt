add_executable(walltz)

ecm_add_qml_module(walltz
    URI org.walltz.walltz
)

target_sources(walltz PRIVATE
    main.cpp
    WallpaperProcessor.h
    WallpaperProcessor.cpp
    blur_presets.h
    blur_presets.cpp
    WallpaperAnalyzer.h
    WallpaperAnalyzer.cpp
    icons.qrc
)

ecm_target_qml_sources(walltz SOURCES
    Main.qml
    ThemedIcon.qml
    CollapsibleSection.qml
)

target_link_libraries(walltz PRIVATE
    Qt6::Core
    Qt6::Quick
    Qt6::Gui
    Qt6::QuickControls2
    Qt6::Widgets
    Qt6::Concurrent
    Qt6::Svg
    KF6::Kirigami
    KF6::I18n
    KF6::I18nQml
    KF6::CoreAddons
    KF6::IconThemes
)

# ── Performance flags (GCC/Clang) ───────────────────────────────────────
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # -march=native enables AVX2/FMA/BMI for pixel loops
    # -funroll-loops: unrolls tight pixel-processing loops
    # -fno-rtti: safe with Qt (qobject_cast ≠ C++ RTTI), smaller vtable, better inlining
    # -fno-unwind-tables / -fno-asynchronous-unwind-tables: no exceptions → no unwind info needed
    target_compile_options(walltz PRIVATE
        -march=native
        -funroll-loops
        -fno-rtti
        -fno-unwind-tables
        -fno-asynchronous-unwind-tables
        -ffunction-sections
        -fdata-sections
    )
    # Dead code elimination at link time (smaller binary → better I-cache)
    target_link_options(walltz PRIVATE -Wl,--gc-sections)
endif()

# ── Link-Time Optimization (cross-module inlining, devirtualization) ────
include(CheckIPOSupported)
check_ipo_supported(RESULT have_ipo OUTPUT ipo_error)
if(have_ipo)
    set_property(TARGET walltz PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    message(STATUS "LTO enabled for walltz")
else()
    message(STATUS "LTO not available: ${ipo_error}")
endif()

install(TARGETS walltz ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
