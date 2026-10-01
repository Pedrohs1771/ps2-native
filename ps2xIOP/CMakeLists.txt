cmake_minimum_required(VERSION 3.21)

project(ps2xIOP LANGUAGES CXX)

option(PS2X_IOP_BUILD_TESTS "Build ps2xIOP emulator smoke tests" OFF)
option(PS2X_IOP_DEBUG_SYMBOLS "Emit debug types for IOP scheduling diagnostics" OFF)
option(PS2X_IOP_ENABLE_INTERPRETER "Include the diagnostic IOP instruction interpreter" ON)
option(PS2X_IOP_BUILD_LAB "Build offline IOP loader/probe tools" OFF)
set(NEXO_IOP_BANK_CPP "" CACHE FILEPATH "Offline generated single IOP bank for the native probe")
set(NEXO_IOP_BANK_MANIFEST "" CACHE FILEPATH "Offline generated multi-module IOP catalog JSON")

if(PS2X_IOP_DEBUG_SYMBOLS AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set_source_files_properties(src/emulator/iop_emulator.cpp src/emulator/core/iop_kernel.cpp
        PROPERTIES COMPILE_OPTIONS "-g")
endif()

add_library(ps2_iop STATIC
    src/ps2_path.cpp
    src/iop_module_manager.cpp
    src/iop_subsystem.cpp
    src/emulator/iop_emulator.cpp
    src/emulator/core/iop_cpu.cpp
    src/emulator/core/iop_native.cpp
    src/emulator/core/iop_kernel.cpp
    src/emulator/core/iop_memory.cpp
    src/emulator/services/iop_module_loader.cpp
    src/emulator/services/iop_rpc.cpp
    src/emulator/imports/iop_cdvd.cpp
    src/emulator/imports/iop_heaplib.cpp
    src/emulator/imports/iop_imports.cpp
    src/emulator/imports/iop_intrman.cpp
    src/emulator/imports/iop_ioman.cpp
    src/emulator/imports/iop_loadcore.cpp
    src/emulator/imports/iop_stdio.cpp
    src/emulator/imports/iop_sysclib.cpp
    src/emulator/imports/iop_sysmem.cpp
    src/emulator/imports/iop_timrman.cpp
    src/emulator/imports/iop_vblank.cpp
    src/modules/dbcman.cpp
    src/modules/cdvdfsv.cpp
    src/modules/loadfile.cpp
    src/modules/libsd.cpp
    src/modules/mcserv.cpp
)

if(PS2X_IOP_ENABLE_INTERPRETER)
    target_sources(ps2_iop PRIVATE src/emulator/core/iop_cpu_interpreter.cpp)
endif()
target_compile_definitions(ps2_iop PRIVATE PS2X_IOP_ENABLE_INTERPRETER=$<BOOL:${PS2X_IOP_ENABLE_INTERPRETER}>)

target_compile_features(ps2_iop PUBLIC cxx_std_20)

target_include_directories(ps2_iop
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_library(ps2x::iop ALIAS ps2_iop)

if(NEXO_IOP_BANK_MANIFEST)
    if(NEXO_IOP_BANK_CPP)
        message(FATAL_ERROR "Select either a single IOP bank or a catalog manifest")
    endif()
    include(cmake/iop_native_catalog.cmake)
    iop_catalog_sources("${NEXO_IOP_BANK_MANIFEST}" iop_catalog_sources_list)
    add_library(ps2_iop_compiled_catalog STATIC ${iop_catalog_sources_list})
    target_include_directories(ps2_iop_compiled_catalog PRIVATE src)
    target_link_libraries(ps2_iop_compiled_catalog PUBLIC ps2_iop)
endif()

if(PS2X_IOP_BUILD_LAB)
    add_executable(nexo_iop_inspect ../lab/src/iop_inspect_main.cpp)
    target_include_directories(nexo_iop_inspect PRIVATE src ../lab/include)
    target_link_libraries(nexo_iop_inspect PRIVATE ps2_iop)
    if(NEXO_IOP_BANK_CPP OR TARGET ps2_iop_compiled_catalog)
        add_executable(nexo_iop_native_probe ../lab/src/iop_native_probe_main.cpp ${NEXO_IOP_BANK_CPP})
        target_include_directories(nexo_iop_native_probe PRIVATE src ../lab/include)
        target_compile_definitions(nexo_iop_native_probe PRIVATE NEXO_IOP_NATIVE=1)
        target_link_libraries(nexo_iop_native_probe PRIVATE ps2_iop)
        if(TARGET ps2_iop_compiled_catalog)
            target_link_libraries(nexo_iop_native_probe PRIVATE ps2_iop_compiled_catalog)
        endif()
    endif()
    if(PS2X_IOP_ENABLE_INTERPRETER)
        add_executable(nexo_iop_baseline_probe ../lab/src/iop_native_probe_main.cpp)
        target_include_directories(nexo_iop_baseline_probe PRIVATE src ../lab/include)
        target_compile_definitions(nexo_iop_baseline_probe PRIVATE NEXO_IOP_NATIVE=0)
        target_link_libraries(nexo_iop_baseline_probe PRIVATE ps2_iop)
    endif()
endif()

if(PS2X_IOP_BUILD_TESTS)
    enable_testing()
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(iop_native_generated ${CMAKE_CURRENT_BINARY_DIR}/native-generated)
    add_custom_command(
        OUTPUT ${iop_native_generated}/iop_native_bank.cpp ${iop_native_generated}/iop_native_semantics.h
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../lab/generate_iop_bank.py
            --words-json ${CMAKE_CURRENT_SOURCE_DIR}/tests/iop_native_words.json
            --root ${CMAKE_CURRENT_SOURCE_DIR}/.. --output ${iop_native_generated} --symbol iopFixtureBank
        DEPENDS ../lab/generate_iop_bank.py tests/iop_native_words.json src/emulator/core/iop_cpu_interpreter.cpp
        VERBATIM
    )
    add_executable(ps2_iop_native_tests tests/iop_native_tests.cpp tests/iop_native_family_tests.cpp ${iop_native_generated}/iop_native_bank.cpp)
    set(iop_rpc_generated ${CMAKE_CURRENT_BINARY_DIR}/rpc-native-generated)
    add_custom_command(
        OUTPUT ${iop_rpc_generated}/iop_native_bank.cpp ${iop_rpc_generated}/iop_native_semantics.h
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../lab/generate_iop_bank.py
            --words-json ${CMAKE_CURRENT_SOURCE_DIR}/tests/iop_native_rpc_words.json
            --root ${CMAKE_CURRENT_SOURCE_DIR}/.. --output ${iop_rpc_generated} --symbol iopRpcFixtureBank
        DEPENDS ../lab/generate_iop_bank.py tests/iop_native_rpc_words.json src/emulator/core/iop_cpu_interpreter.cpp
        VERBATIM
    )
    target_sources(ps2_iop_native_tests PRIVATE ${iop_rpc_generated}/iop_native_bank.cpp)
    target_include_directories(ps2_iop_native_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${iop_native_generated})
    target_compile_definitions(ps2_iop_native_tests PRIVATE PS2X_IOP_ENABLE_INTERPRETER=$<BOOL:${PS2X_IOP_ENABLE_INTERPRETER}>)
    target_link_libraries(ps2_iop_native_tests PRIVATE ps2_iop)
    add_test(NAME ps2_iop_native_tests COMMAND ps2_iop_native_tests)
    add_test(NAME ps2_iop_codegen_tests COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/../lab/tests/test_iop_bank_codegen.py)
    add_test(NAME ps2_iop_catalog_codegen_tests COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/../lab/tests/test_iop_catalog_codegen.py)
    if(PS2X_IOP_BUILD_LAB)
        add_test(NAME nexo_iop_inspect_cli_tests COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/../lab/tests/test_iop_inspect_cli.py $<TARGET_FILE:nexo_iop_inspect>)
    endif()

    if(PS2X_IOP_ENABLE_INTERPRETER)
        add_executable(ps2_iop_emulator_tests tests/iop_emulator_tests.cpp)
        target_link_libraries(ps2_iop_emulator_tests PRIVATE ps2_iop)
        add_test(NAME ps2_iop_emulator_tests COMMAND ps2_iop_emulator_tests)

        add_executable(ps2_iop_import_tests tests/iop_import_tests.cpp)
        target_link_libraries(ps2_iop_import_tests PRIVATE ps2_iop)
        target_include_directories(ps2_iop_import_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        add_test(NAME ps2_iop_import_tests COMMAND ps2_iop_import_tests)

        add_executable(ps2_iop_compatibility_tests tests/iop_compatibility_tests.cpp)
        target_link_libraries(ps2_iop_compatibility_tests PRIVATE ps2_iop)
        add_test(NAME ps2_iop_compatibility_tests COMMAND ps2_iop_compatibility_tests)

        add_executable(ps2_iop_import_version_tests tests/iop_import_version_tests.cpp)
        target_link_libraries(ps2_iop_import_version_tests PRIVATE ps2_iop)
        target_include_directories(ps2_iop_import_version_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        add_test(NAME ps2_iop_import_version_tests COMMAND ps2_iop_import_version_tests)
    endif()
endif()

if(PS2X_FAST_ITERATION)
    foreach(iop_target ps2_iop ps2_iop_compiled_catalog)
        if(TARGET ${iop_target})
            set_property(TARGET ${iop_target} PROPERTY INTERPROCEDURAL_OPTIMIZATION FALSE)
            if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
                target_compile_options(${iop_target} PRIVATE -O1 -fno-lto)
            endif()
        endif()
    endforeach()
endif()

install(TARGETS ps2_iop
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/
    DESTINATION include
)
