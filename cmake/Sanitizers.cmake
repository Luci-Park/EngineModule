function(engine_apply_sanitizers target)
    if(NOT ENGINE_SANITIZE)
        return()
    endif()

    if(MSVC)
        target_compile_options(${target} PRIVATE /fsanitize=address /Zi)
        # disable stl container overflow annotations to match vcpkg libraries
        # mismatch (LNK2038 annotate_vector/string/optional -> LNK1319). 
        target_compile_definitions(${target} PRIVATE _DISABLE_STL_ANNOTATION=1)
    else()
        target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endif()
endfunction()
