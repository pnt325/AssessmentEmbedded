cmake_minimum_required(VERSION 3.25)
project(AssessmentEmbedded VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(gateway STATIC
    src/canbuf.c
    src/gwproto.c
    src/gateway.c
)
target_include_directories(gateway PUBLIC include)
target_compile_options(gateway PRIVATE -Wall -Wextra -Wpedantic)

add_executable(tests test/main.c)
target_link_libraries(tests PRIVATE gateway)
target_compile_options(tests PRIVATE -Wall -Wextra -Wpedantic)

# Build and run the test executable: cmake --build <dir> --target run_tests
add_custom_target(run_tests
    COMMAND tests
    DEPENDS tests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    USES_TERMINAL
)
