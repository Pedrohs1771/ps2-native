if(NOT PS2X_FAST_ITERATION AND PS2X_ENABLE_RELEASE_IPO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT IPO_SUPPORTED OUTPUT IPO_ERROR)
endif()

function(EnableFastReleaseMode TargetName)
    if(PS2X_FAST_ITERATION)
        message(STATUS "Fast development profile without LTO: ${TargetName}")
        set_target_properties(${TargetName} PROPERTIES
            INTERPROCEDURAL_OPTIMIZATION FALSE
            INTERPROCEDURAL_OPTIMIZATION_RELEASE FALSE
            INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO FALSE)
        if(MSVC)
            target_compile_options(${TargetName} PRIVATE /O1 /GL-)
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(${TargetName} PRIVATE -O1 -fno-lto)
            target_link_options(${TargetName} PRIVATE -fno-lto)
        endif()
        return()
    endif()
    message("> Enabling optimization for: ${TargetName}")
    if(MSVC)
        target_compile_options(${TargetName} PRIVATE
            $<$<CONFIG:Release>:
                /O2 # speed
                /Ob2 # inline aggressively
                /Oi # intrinsics
                /Gy # function-level linking
                /Gw # global data in COMDAT
                /GF # string pooling
                /Zc:inline # remove unreferenced inline
                /fp:fast # fast math (graphics friendly)
                /DNDEBUG
                /arch:AVX2 # Advanced Vector Extensions 2
                /GS- # Disable Buffer Security Check (faster)
                /Qspectre- # Disable Spectre mitigations (faster)
            >
        )

        if(TARGET ${TargetName})
            target_link_options(${TargetName} PRIVATE
                $<$<CONFIG:Release>:
                    /OPT:REF # remove unreferenced
                    /OPT:ICF # fold identical COMDATs
                >
            )
        endif()
        if(PS2X_ENABLE_RELEASE_IPO AND IPO_SUPPORTED)
            target_compile_options(${TargetName} PRIVATE $<$<CONFIG:Release>:/GL>)
            target_link_options(${TargetName} PRIVATE $<$<CONFIG:Release>:/LTCG>)
        endif()
    endif()

    if(PS2X_ENABLE_RELEASE_IPO AND IPO_SUPPORTED)
        set_property(TARGET ${TargetName} PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    elseif(PS2X_ENABLE_RELEASE_IPO)
        message(WARNING "Interprocedural optimization not supported: ${IPO_ERROR}")
    endif()
endfunction()
