set(ENGINE_PCH_COMMON_HEADERS
    <vector> <string> <memory> <unordered_map> <functional> <algorithm> <cstdint>
)

function(engine_target_pch target)
    # ARGN = extra headers a specific target wants beyond the common set
    target_precompile_headers(${target} PRIVATE
        ${ENGINE_PCH_COMMON_HEADERS}
        ${ARGN}
    )
endfunction()

function(engine_add_module name)
    set(target engine_${name})
    
    if(ENGINE_SHARED)
        add_library(${target} SHARED ${ARGN})
    else()
        add_library(${target} STATIC ${ARGN})
    endif()

    add_library(engine::${name} ALIAS ${target})

    target_include_directories(${target}
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_compile_features(${target} PUBLIC cxx_std_20)
    engine_set_warnings(${target})
    engine_target_pch(${target})

    string(TOUPPER ${name} name_upper)

    include(GenerateExportHeader)
    generate_export_header(${target}
        BASE_NAME ${name}
        EXPORT_MACRO_NAME ENGINE_${name_upper}_API
        EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/include/engine/${name}/${name}_export.h
    )
    target_include_directories(${target}
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
    )

    set_target_properties(${target} PROPERTIES 
        FOLDER "modules"
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
        )
endfunction()
