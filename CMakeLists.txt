add_executable(image_processor
    main.cpp
    Image.cpp
    Filters.cpp
    BMPwork.cpp
)

add_catch(test_image_processor
    test.cpp
    Image.cpp
    Filters.cpp
    BMPwork.cpp
)

add_custom_target(check_image_processor
    COMMAND $<TARGET_FILE:test_image_processor>
    DEPENDS test_image_processor
    COMMENT "Running image_processor unit tests"
    VERBATIM
)
