# cmake/Sanitizers.cmake
# Memory and undefined behavior sanitizers configuration

function(enable_sanitizers target_name)
    option(ENABLE_SANITIZER_ADDRESS "Enable Address Sanitizer" OFF)
    option(ENABLE_SANITIZER_UNDEFINED "Enable Undefined Behavior Sanitizer" OFF)

    if(ENABLE_SANITIZER_ADDRESS)
        if(MSVC)
            target_compile_options(${target_name} PRIVATE /fsanitize=address)
        else()
            target_compile_options(${target_name} PRIVATE -fsanitize=address)
            target_link_options(${target_name} PRIVATE -fsanitize=address)
        endif()
    endif()

    if(ENABLE_SANITIZER_UNDEFINED)
        if(NOT MSVC)
            target_compile_options(${target_name} PRIVATE -fsanitize=undefined)
            target_link_options(${target_name} PRIVATE -fsanitize=undefined)
        endif()
    endif()
endfunction()
