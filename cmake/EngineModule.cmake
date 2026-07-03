function(engine_add_module name)
    set(target engine_${name})
    add_library(${target} STATIC ${ARGN})
    add_library(engine::${name} ALIAS ${target})

    target_include_directories(${target}
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_compile_features(${target} PUBLIC cxx_std_20)
    engine_set_warnings(${target})

    set_target_properties(${target} PROPERTIES FOLDER "modules")
endfunction()
