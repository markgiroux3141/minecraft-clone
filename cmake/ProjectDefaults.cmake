# Shared compile settings applied to first-party targets only (never to
# third-party code, which has its own warning baselines).
function(vox_set_target_defaults target)
    target_compile_features(${target} PUBLIC cxx_std_23)

    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4             # high warning level
            /permissive-    # strict standard conformance
            /Zc:__cplusplus # report the real C++ version
            /Zc:preprocessor
            /utf-8
        )
        target_compile_definitions(${target} PRIVATE
            NOMINMAX
            WIN32_LEAN_AND_MEAN
            _CRT_SECURE_NO_WARNINGS
        )
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    target_compile_definitions(${target} PUBLIC
        $<$<CONFIG:Debug>:VOX_DEBUG>
    )
endfunction()
