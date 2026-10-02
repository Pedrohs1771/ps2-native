# The pinned upstream fast handler uses the total chunk size after consuming a
# tag. Split GIF deliveries can then read an unsent vertex or stall when fewer
# than NREG qwords remain. Keep the external checkout unchanged and compile a
# byte-identified compatibility copy with both bounds corrected.
set(_ps2native_gs_original "${PS2X_PARALLEL_GS_SOURCE_DIR}/gs/gs_interface.cpp")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_ps2native_gs_original}")
file(SHA256 "${_ps2native_gs_original}" _ps2native_gs_original_sha)
if(NOT _ps2native_gs_original_sha STREQUAL "93246b693e3507f5498d2fa33929486bfd28623e0457d48df608d8b754606b69")
    message(FATAL_ERROR "The paraLLEl-GS GIF compatibility patch requires the reviewed gs_interface.cpp at commit 3a66c1976170cbc2cb53a3593fabbc7c4b2ccfbd")
endif()
file(READ "${_ps2native_gs_original}" _ps2native_gs_code)
string(REPLACE "if (path.reg == 0 && optimized_draw_handler[path_index])"
    "if (path.reg == 0 && optimized_draw_handler[path_index] && size - i >= nreg)"
    _ps2native_gs_code "${_ps2native_gs_code}")
string(REPLACE "std::min<uint32_t>(size / nreg, path.tag.NLOOP - path.loop)"
    "std::min<uint32_t>((size - i) / nreg, path.tag.NLOOP - path.loop)"
    _ps2native_gs_code "${_ps2native_gs_code}")
set(_ps2native_gs_patched "${CMAKE_BINARY_DIR}/gs-parallel-compat/gs_interface.cpp")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/gs-parallel-compat")
set(_ps2native_gs_existing "")
if(EXISTS "${_ps2native_gs_patched}")
    file(READ "${_ps2native_gs_patched}" _ps2native_gs_existing)
endif()
if(NOT _ps2native_gs_existing STREQUAL _ps2native_gs_code)
    file(WRITE "${_ps2native_gs_patched}" "${_ps2native_gs_code}")
endif()
get_target_property(_ps2native_gs_sources parallel-gs SOURCES)
get_target_property(_ps2native_gs_source_dir parallel-gs SOURCE_DIR)
set(_ps2native_gs_replaced_sources "")
foreach(_ps2native_gs_source IN LISTS _ps2native_gs_sources)
    if(_ps2native_gs_source STREQUAL "gs_interface.cpp" OR
       _ps2native_gs_source STREQUAL _ps2native_gs_original)
        list(APPEND _ps2native_gs_replaced_sources "${_ps2native_gs_patched}")
    elseif(IS_ABSOLUTE "${_ps2native_gs_source}")
        list(APPEND _ps2native_gs_replaced_sources "${_ps2native_gs_source}")
    else()
        list(APPEND _ps2native_gs_replaced_sources "${_ps2native_gs_source_dir}/${_ps2native_gs_source}")
    endif()
endforeach()
set_property(TARGET parallel-gs PROPERTY SOURCES "${_ps2native_gs_replaced_sources}")
