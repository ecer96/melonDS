# Frontend SDL2 minimo para RG DS Plus
# Parte del fork de melonDS.

cmake_minimum_required(VERSION 3.16)

# Buscar SDL2
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)

# Ejecutable
add_executable(melonDS_sdl
    main.cpp
    platform.cpp
    video.cpp
    input.cpp
    audio.cpp
)

# Linkear contra el core de melonDS
target_link_libraries(melonDS_sdl PRIVATE core)

# Incluir headers del core
target_include_directories(melonDS_sdl PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/../..
)

# SDL2
target_include_directories(melonDS_sdl PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(melonDS_sdl PRIVATE ${SDL2_LIBRARIES})
target_link_directories(melonDS_sdl PRIVATE ${SDL2_LIBRARY_DIRS})

# Threading
find_package(Threads REQUIRED)
target_link_libraries(melonDS_sdl PRIVATE Threads::Threads)

# dlopen para DynamicLibrary
target_link_libraries(melonDS_sdl PRIVATE ${CMAKE_DL_LIBS})

# Flags de compilacion
target_compile_options(melonDS_sdl PRIVATE
    -O3
    -fomit-frame-pointer
    -fno-stack-protector
    -ffast-math
)

# Definiciones
target_compile_definitions(melonDS_sdl PRIVATE
    JIT_ENABLED
)

# Copiar el ejecutable a la raiz del build para encontrarlo facil
set_target_properties(melonDS_sdl PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
