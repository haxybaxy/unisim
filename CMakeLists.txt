cmake_minimum_required(VERSION 3.10)
project(unisim)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable CTest
enable_testing()

# Generate compile_commands.json for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find GTK4
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 REQUIRED gtk4)

# Find epoxy (OpenGL loader)
pkg_check_modules(EPOXY REQUIRED epoxy)

find_package(CUDAToolkit QUIET)
set(USE_CUDA FALSE)
if(CUDAToolkit_FOUND)
    enable_language(CUDA)
    set(USE_CUDA TRUE)
endif()

# Get full linker flags from pkg-config
execute_process(
    COMMAND pkg-config --libs gtk4
    OUTPUT_VARIABLE GTK4_LINK_FLAGS
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Convert space-separated string to CMake list
string(REPLACE " " ";" GTK4_LINK_FLAGS_LIST "${GTK4_LINK_FLAGS}")

# Source files
set(SOURCES
    src/main.cpp
    src/simulation/vector3d.hpp
    src/simulation/body.hpp
    src/simulation/universe.hpp
    src/simulation/integrators/integrator.hpp
    src/simulation/integrators/euler.cpp
    src/simulation/integrators/verlet.cpp
    src/simulation/integrators/leapfrog.cpp
    src/simulation/force_computers/force_computer.hpp
    src/simulation/force_computers/brute_force.cpp
    src/simulation/force_computers/barnes_hut.cpp
    src/simulation/force_computers/fast_multipole.cpp
    src/rendering/renderer.hpp
    src/rendering/renderer_2d.cpp
    src/rendering/renderer_3d.cpp
    src/rendering/renderer_3d_opengl.cpp
    src/initial_conditions/initializer.hpp
    src/initial_conditions/random.cpp
    src/initial_conditions/spiral_galaxy.cpp
    src/initial_conditions/elliptical_galaxy.cpp
    src/initial_conditions/galaxy_collision.cpp
    src/initial_conditions/solar_system.cpp
    src/initial_conditions/binary_star.cpp
    src/compute/compute_backend.hpp
    src/compute/cpu_backend.cpp
    src/simulation/integrators/runge_kutta.cpp
    src/ui/viewport.cpp
    src/ui/control_panel.cpp
    src/ui/main_window.cpp
    src/ui/metrics_monitor.cpp
    src/ui/scenario_generator.cpp
)

if(APPLE)
    list(APPEND SOURCES src/compute/metal_backend.mm)
else()
    list(APPEND SOURCES src/compute/metal_backend_stub.cpp)
endif()

if(USE_CUDA)
    list(APPEND SOURCES src/compute/cuda_backend.cu)
else()
    list(APPEND SOURCES src/compute/cuda_backend_stub.cpp)
endif()

# Add executable
add_executable(unisim ${SOURCES})

# Set include directories and compile flags
target_include_directories(unisim PRIVATE 
    ${CMAKE_SOURCE_DIR}/src
    ${GTK4_INCLUDE_DIRS}
    ${EPOXY_INCLUDE_DIRS}
)

target_compile_options(unisim PRIVATE ${GTK4_CFLAGS_OTHER} ${EPOXY_CFLAGS_OTHER})

# Use full linker flags directly
target_link_options(unisim PRIVATE ${GTK4_LINK_FLAGS_LIST} ${EPOXY_LDFLAGS})

if(USE_CUDA)
    target_compile_definitions(unisim PRIVATE UNISIM_USE_CUDA)
    target_link_libraries(unisim PRIVATE CUDA::cudart)
endif()

# Link Metal and Foundation frameworks on macOS for GPU backend
if(APPLE)
    find_library(METAL_LIB Metal)
    find_library(FOUNDATION_LIB Foundation)
    if(METAL_LIB AND FOUNDATION_LIB)
        target_link_libraries(unisim PRIVATE ${METAL_LIB} ${FOUNDATION_LIB})
        # Set Objective-C++ language for .mm files
        set_source_files_properties(src/compute/metal_backend.mm PROPERTIES
            COMPILE_FLAGS "-x objective-c++ -std=c++17"
        )
    endif()
endif()

add_executable(physics_regression
    tests/physics_regression.cpp
    src/simulation/force_computers/brute_force.cpp
    src/simulation/force_computers/fast_multipole.cpp
    src/simulation/force_computers/barnes_hut.cpp
    src/simulation/integrators/euler.cpp
    src/simulation/integrators/verlet.cpp
    src/simulation/integrators/leapfrog.cpp
    src/simulation/integrators/runge_kutta.cpp
)

target_include_directories(physics_regression PRIVATE ${CMAKE_SOURCE_DIR}/src)

if(USE_CUDA)
    target_sources(physics_regression PRIVATE src/compute/cuda_backend.cu)
    target_compile_definitions(physics_regression PRIVATE UNISIM_USE_CUDA)
    target_link_libraries(physics_regression PRIVATE CUDA::cudart)
else()
    target_sources(physics_regression PRIVATE src/compute/cuda_backend_stub.cpp)
endif()

if(APPLE)
    target_sources(physics_regression PRIVATE src/compute/metal_backend.mm)
    if(METAL_LIB AND FOUNDATION_LIB)
        target_link_libraries(physics_regression PRIVATE ${METAL_LIB} ${FOUNDATION_LIB})
    endif()
endif()

add_test(NAME physics_regression COMMAND physics_regression)
