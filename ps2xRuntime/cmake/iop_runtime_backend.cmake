# Keep the backend choice out of the runtime's shared Make flags file.
# All runtime objects depend on that file, including per-source flag comments.
function(ps2x_add_iop_runtime_backend runtime backend source native)
    add_library(${backend} OBJECT "${source}")
    target_compile_features(${backend} PRIVATE cxx_std_20)
    target_include_directories(${backend} PRIVATE
        "$<TARGET_PROPERTY:${runtime},INCLUDE_DIRECTORIES>")
    target_compile_options(${backend} PRIVATE
        "$<TARGET_PROPERTY:${runtime},COMPILE_OPTIONS>")
    target_compile_definitions(${backend} PRIVATE PS2X_RUNTIME_NATIVE_IOP=$<BOOL:${native}>)
    target_link_libraries(${backend} PRIVATE ps2_iop)
    set_property(TARGET ${backend} PROPERTY INTERPROCEDURAL_OPTIMIZATION FALSE)
    target_sources(${runtime} PRIVATE $<TARGET_OBJECTS:${backend}>)
endfunction()
