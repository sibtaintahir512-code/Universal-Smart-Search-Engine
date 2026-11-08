# cmake/CompilerWarnings.cmake
# Compiler warning configurations for Universal Smart Search Engine

function(set_project_warnings target_name)
    option(WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

    set(MSVC_WARNINGS
        /W4          # Baseline reasonable high-level warnings
        /permissive- # Standards conformance mode
        /utf-8       # Set Source and Executable character sets to UTF-8
        /w14242      # 'identifier': conversion from 'type1' to 'type1', possible loss of data
        /w14254      # 'operator': conversion from 'type1:field_bits' to 'type2:field_bits'
        /w14263      # 'function': member function does not override any base class virtual member function
        /w14265      # 'class': class has virtual functions, but destructor is not virtual
        /w14287      # 'operator': unsigned/negative constant mismatch
        /we4289      # nonstandard extension used: 'variable': loop control variable declared in the for-loop is used outside
        /w14296      # 'operator': expression is always 'boolean_value'
        /w14311      # 'variable': pointer truncation from 'type1' to 'type2'
        /w14545      # expression before comma evaluates to a function which is missing an argument list
        /w14546      # function call before comma missing argument list
        /w14547      # 'operator': operator before comma has no effect; expected operator with side-effect
        /w14549      # 'operator': operator before comma has no effect; did you intend 'operator'?
        /w14555      # expression has no effect; expected expression with side-effect
        /w14619      # pragma warning: there is no warning number 'number'
        /w14640      # Enable warning on thread un-safe static member initialization
        /w14826      # Conversion from 'type1' to 'type_2' is sign-extended. This may cause unexpected runtime behavior.
        /w14905      # wide string literal cast to 'LPSTR'
        /w14906      # string literal cast to 'LPWSTR'
        /w14928      # illegal copy-initialization; more than one user-defined conversion has been implicitly applied
    )

    set(CLANG_GCC_WARNINGS
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wcast-align
        -Wunused
        -Woverloaded-virtual
        -Wconversion
        -Wsign-conversion
        -Wnull-dereference
        -Wformat=2
    )

    if(WARNINGS_AS_ERRORS)
        list(APPEND MSVC_WARNINGS /WX)
        list(APPEND CLANG_GCC_WARNINGS -Werror)
    endif()

    if(MSVC)
        target_compile_options(${target_name} PRIVATE ${MSVC_WARNINGS})
    else()
        target_compile_options(${target_name} PRIVATE ${CLANG_GCC_WARNINGS})
    endif()
endfunction()
