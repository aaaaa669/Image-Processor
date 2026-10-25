cmake_minimum_required(VERSION 3.16)
project(ImageProcessor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(IMAGE_PROCESSOR_SOURCES
    Image.cpp
    Filters.cpp
    BMPwork.cpp
)

add_executable(image_processor
    main.cpp
    ${IMAGE_PROCESSOR_SOURCES}
)

include(CTest)

if(BUILD_TESTING)
    find_package(Threads REQUIRED)

    add_executable(test_image_processor
        test.cpp
        ${IMAGE_PROCESSOR_SOURCES}
        third_party/catch/catch_main.cpp
    )
    target_include_directories(test_image_processor SYSTEM PRIVATE third_party/catch)
    target_link_libraries(test_image_processor PRIVATE Threads::Threads)

    add_test(NAME image_processor_tests COMMAND test_image_processor)

    add_custom_target(check_image_processor
        COMMAND $<TARGET_FILE:test_image_processor>
        DEPENDS test_image_processor
        COMMENT "Running image_processor unit tests"
        VERBATIM
    )
endif()
