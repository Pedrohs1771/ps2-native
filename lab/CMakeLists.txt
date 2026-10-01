# The opt-in runtime capture hooks need the codec too. Compile it into the
# runtime archive instead of creating a runtime <-> codec link dependency.
target_sources(ps2_runtime PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/vu_snapshot.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/device_snapshot.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/gs_snapshot.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/vif_capture.cpp")
target_include_directories(ps2_runtime PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_definitions(ps2_runtime PRIVATE PS2X_NEXO_LAB=1)
target_compile_features(ps2_runtime PRIVATE cxx_std_20)

add_library(nexo_vu_snapshot INTERFACE)
target_include_directories(nexo_vu_snapshot INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(nexo_vu_snapshot INTERFACE ps2_runtime)
target_compile_features(nexo_vu_snapshot INTERFACE cxx_std_20)

add_library(nexo_device_snapshot INTERFACE)
target_link_libraries(nexo_device_snapshot INTERFACE nexo_vu_snapshot)
add_executable(nexo_device_snapshot_tests tests/device_snapshot_tests.cpp tests/empty_ee_table.cpp)
target_include_directories(nexo_device_snapshot_tests PRIVATE "${PROJECT_SOURCE_DIR}/ps2xTest/include")
target_link_libraries(nexo_device_snapshot_tests PRIVATE nexo_device_snapshot)

add_library(nexo_vif_replay_support STATIC src/vif_replay.cpp)
target_link_libraries(nexo_vif_replay_support PUBLIC nexo_vu_runtime_binding)
add_executable(nexo_vif_capture_tests tests/vif_capture_tests.cpp tests/empty_ee_table.cpp)
target_include_directories(nexo_vif_capture_tests PRIVATE "${PROJECT_SOURCE_DIR}/ps2xTest/include")
target_link_libraries(nexo_vif_capture_tests PRIVATE nexo_vif_replay_support nexo_vu_native_frontend)
add_executable(nexo_vif_replay src/vif_replay_main.cpp tests/empty_ee_table.cpp)
target_link_libraries(nexo_vif_replay PRIVATE nexo_vif_replay_support)
add_executable(nexo_vif_issue_trace src/vif_issue_trace_main.cpp tests/empty_ee_table.cpp)
target_link_libraries(nexo_vif_issue_trace PRIVATE nexo_vif_replay_support)

add_library(nexo_vu_replay_support STATIC src/vu_replay.cpp)
target_link_libraries(nexo_vu_replay_support PUBLIC nexo_vu_snapshot)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(NEXO_PCSX2_VU_SOURCE_DIR "" CACHE PATH "Cached pinned PCSX2 sources for an optional laboratory reference")
if(NEXO_PCSX2_VU_SOURCE_DIR)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR NOT CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
        message(FATAL_ERROR "The isolated PCSX2 reference FP adapter currently requires Linux x86-64")
    endif()
    set(NEXO_PCSX2_STAGE "${CMAKE_CURRENT_BINARY_DIR}/pcsx2-reference")
    execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/prepare_pcsx2_reference.py"
        --source "${NEXO_PCSX2_VU_SOURCE_DIR}" --output "${NEXO_PCSX2_STAGE}"
        RESULT_VARIABLE NEXO_PCSX2_PREPARE_RESULT OUTPUT_QUIET ERROR_VARIABLE NEXO_PCSX2_PREPARE_ERROR)
    if(NOT NEXO_PCSX2_PREPARE_RESULT EQUAL 0)
        message(FATAL_ERROR "Cannot verify independent VU reference: ${NEXO_PCSX2_PREPARE_ERROR}")
    endif()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS prepare_pcsx2_reference.py)
    file(GLOB NEXO_PCSX2_STAGED_FILES CONFIGURE_DEPENDS "${NEXO_PCSX2_STAGE}/*")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${NEXO_PCSX2_STAGED_FILES}
        "${NEXO_PCSX2_VU_SOURCE_DIR}/pcsx2/VUops.cpp" "${NEXO_PCSX2_VU_SOURCE_DIR}/pcsx2/VUflags.cpp"
        "${NEXO_PCSX2_VU_SOURCE_DIR}/pcsx2/VU1microInterp.cpp" "${NEXO_PCSX2_VU_SOURCE_DIR}/pcsx2/VU.h"
        "${NEXO_PCSX2_VU_SOURCE_DIR}/pcsx2/VUops.h" "${NEXO_PCSX2_VU_SOURCE_DIR}/pcsx2/VUflags.h"
        "${NEXO_PCSX2_VU_SOURCE_DIR}/pcsx2/VUmicro.h" "${NEXO_PCSX2_VU_SOURCE_DIR}/pcsx2/Gif_Unit.h"
        "${NEXO_PCSX2_VU_SOURCE_DIR}/COPYING.GPLv3")
    add_library(nexo_pcsx2_vu_reference STATIC src/pcsx2_vu_reference.cpp
        "${NEXO_PCSX2_STAGE}/VUops.cpp" "${NEXO_PCSX2_STAGE}/VUflags.cpp" "${NEXO_PCSX2_STAGE}/VU1microInterp.cpp")
    target_include_directories(nexo_pcsx2_vu_reference PRIVATE reference/pcsx2 "${NEXO_PCSX2_STAGE}")
    target_link_libraries(nexo_pcsx2_vu_reference PUBLIC nexo_vu_snapshot)
    target_compile_options(nexo_pcsx2_vu_reference PRIVATE -O1 -fno-lto -fno-strict-aliasing -ffp-contract=off)
    set_property(TARGET nexo_pcsx2_vu_reference PROPERTY INTERPROCEDURAL_OPTIMIZATION FALSE)
    add_executable(nexo_pcsx2_vu_reference_tests tests/pcsx2_vu_reference_tests.cpp tests/empty_ee_table.cpp)
    target_include_directories(nexo_pcsx2_vu_reference_tests PRIVATE "${PROJECT_SOURCE_DIR}/ps2xTest/include")
    target_link_libraries(nexo_pcsx2_vu_reference_tests PRIVATE nexo_pcsx2_vu_reference)
    add_executable(nexo_vif_pcsx2_reference src/vif_pcsx2_reference_main.cpp tests/empty_ee_table.cpp)
    target_link_libraries(nexo_vif_pcsx2_reference PRIVATE nexo_pcsx2_vu_reference nexo_vif_replay_support)
    set_property(TARGET nexo_vif_pcsx2_reference PROPERTY INTERPROCEDURAL_OPTIMIZATION FALSE)
    target_compile_options(nexo_vif_pcsx2_reference PRIVATE -O1 -fno-lto)
    if(BUILD_TESTING)
        add_test(NAME nexo_pcsx2_vu_reference_tests COMMAND nexo_pcsx2_vu_reference_tests)
    endif()
endif()
set(NEXO_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(NEXO_VU_SEMANTICS "${NEXO_GENERATED_DIR}/nexo/vu_native_semantics.h")
set(NEXO_VU_MACHINE "${NEXO_GENERATED_DIR}/nexo/vu_native_machine.inc")
add_custom_command(OUTPUT "${NEXO_VU_SEMANTICS}" "${NEXO_VU_MACHINE}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/generate_vu_semantics.py"
        --root "${PROJECT_SOURCE_DIR}" --semantics "${NEXO_VU_SEMANTICS}" --machine "${NEXO_VU_MACHINE}"
    DEPENDS generate_vu_semantics.py
        "${PROJECT_SOURCE_DIR}/ps2xRuntime/src/lib/vu/ps2_vu1_core.cpp"
        "${PROJECT_SOURCE_DIR}/ps2xRuntime/src/lib/vu/ps2_vu1_upper.cpp"
        "${PROJECT_SOURCE_DIR}/ps2xRuntime/src/lib/vu/ps2_vu1_lower.cpp"
        "${PROJECT_SOURCE_DIR}/ps2xRuntime/src/lib/vu/ps2_vu1_detail.h"
    VERBATIM)
add_library(nexo_vu_native STATIC src/vu_native.cpp "${NEXO_VU_SEMANTICS}" "${NEXO_VU_MACHINE}")
target_link_libraries(nexo_vu_native PUBLIC nexo_vu_snapshot)
target_link_libraries(nexo_vu_replay_support PUBLIC nexo_vu_native)
target_include_directories(nexo_vu_native PUBLIC "${NEXO_GENERATED_DIR}")
add_library(nexo_vu_native_frontend STATIC src/vu_native_inspect.cpp)
target_link_libraries(nexo_vu_native_frontend PUBLIC nexo_vu_snapshot)
add_library(nexo_vu_runtime_binding STATIC src/vu_runtime_binding.cpp)
target_link_libraries(nexo_vu_runtime_binding PUBLIC nexo_vu_native)
add_library(nexo_vu_runtime_replay_support STATIC src/vu_runtime_replay.cpp)
target_link_libraries(nexo_vu_runtime_replay_support PUBLIC nexo_vu_runtime_binding nexo_vu_replay_support)
add_executable(nexo_vu_inspect src/vu_native_inspect_main.cpp tests/empty_ee_table.cpp)
target_link_libraries(nexo_vu_inspect PRIVATE nexo_vu_native_frontend)
add_executable(nexo_vu_native_tests tests/vu_native_tests.cpp tests/empty_ee_table.cpp)
target_include_directories(nexo_vu_native_tests PRIVATE "${PROJECT_SOURCE_DIR}/ps2xTest/include")
target_link_libraries(nexo_vu_native_tests PRIVATE nexo_vu_native nexo_vu_native_frontend nexo_vu_runtime_replay_support)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(NEXO_INTERPRETER_SYMBOLS
        _ZN14VU1Interpreter9execUpperEj
        _ZN14VU1Interpreter9execLowerEjPhjR2GSP9PS2Memoryj
        _ZN14VU1Interpreter6resumeEPhjS0_jR2GSP9PS2Memoryjjj
        _ZN14VU1Interpreter7executeEPhjS0_jR2GSP9PS2Memoryjjjj
        _ZN14VU1Interpreter3runEPhjS0_jR2GSP9PS2Memoryj
        _ZN14VU1Interpreter13applyFmacDestEPfS0_h
        _ZN14VU1Interpreter16applyFmacDestAccEPfh
        _ZN14VU1Interpreter19normalizeFmacResultEPfhPh)
    foreach(nexo_interpreter_symbol IN LISTS NEXO_INTERPRETER_SYMBOLS)
        target_link_options(nexo_vu_native_tests PRIVATE "-Wl,--wrap=${nexo_interpreter_symbol}")
    endforeach()
else()
    message(FATAL_ERROR "Native VU laboratory link traps currently require the Linux GNU/LLVM ABI")
endif()

set(NEXO_VU_BANK_CPP "" CACHE FILEPATH "Pre-generated finite VU bank source for native-only replay")
if(NEXO_VU_BANK_CPP)
    if(NOT EXISTS "${NEXO_VU_BANK_CPP}")
        message(FATAL_ERROR "Native VU bank source does not exist: ${NEXO_VU_BANK_CPP}")
    endif()
    # Compile the bank once; both executables use the same native objects.
    add_library(nexo_vu_compiled_bank STATIC "${NEXO_VU_BANK_CPP}")
    target_link_libraries(nexo_vu_compiled_bank PUBLIC nexo_vu_native)
    set_property(TARGET nexo_vu_compiled_bank PROPERTY INTERPROCEDURAL_OPTIMIZATION FALSE)
    target_compile_options(nexo_vu_compiled_bank PRIVATE -O1 -fno-lto)
    add_executable(nexo_vu_native_replay src/vu_replay_main.cpp src/native_interpreter_traps.cpp tests/empty_ee_table.cpp)
    target_link_libraries(nexo_vu_native_replay PRIVATE nexo_vu_replay_support nexo_vu_compiled_bank)
    add_executable(nexo_vu_runtime_replay src/vu_replay_main.cpp src/native_interpreter_traps.cpp tests/empty_ee_table.cpp)
    target_link_libraries(nexo_vu_runtime_replay PRIVATE nexo_vu_runtime_replay_support nexo_vu_compiled_bank)
    target_compile_definitions(nexo_vu_runtime_replay PRIVATE NEXO_VU_RUNTIME_CALLBACK_REPLAY=1)
    foreach(nexo_replay_target nexo_vu_native_replay nexo_vu_runtime_replay)
        target_compile_definitions(${nexo_replay_target} PRIVATE NEXO_COMPILED_VU_BANK=1)
        set_property(TARGET ${nexo_replay_target} PROPERTY INTERPROCEDURAL_OPTIMIZATION FALSE)
        target_compile_options(${nexo_replay_target} PRIVATE -O1 -fno-lto)
        foreach(nexo_interpreter_symbol IN LISTS NEXO_INTERPRETER_SYMBOLS)
            target_link_options(${nexo_replay_target} PRIVATE "-Wl,--wrap=${nexo_interpreter_symbol}")
        endforeach()
    endforeach()
endif()

set(NEXO_VIF_BANK_SOURCES "" CACHE STRING "Pre-generated finite VU banks and registry for original VIF replay")
if(NEXO_VIF_BANK_SOURCES)
    foreach(nexo_bank_source IN LISTS NEXO_VIF_BANK_SOURCES)
        if(NOT EXISTS "${nexo_bank_source}")
            message(FATAL_ERROR "VIF native bank source does not exist: ${nexo_bank_source}")
        endif()
    endforeach()
    add_library(nexo_vif_compiled_banks STATIC ${NEXO_VIF_BANK_SOURCES})
    target_link_libraries(nexo_vif_compiled_banks PUBLIC nexo_vu_native)
    if(NEXO_VU_BANK_CPP)
        target_link_libraries(nexo_vif_compiled_banks PUBLIC nexo_vu_compiled_bank)
    endif()
    set_property(TARGET nexo_vif_compiled_banks PROPERTY INTERPROCEDURAL_OPTIMIZATION FALSE)
    target_compile_options(nexo_vif_compiled_banks PRIVATE -O1 -fno-lto)
    set(NEXO_VIF_COMPILED_PROVIDER nexo_vif_compiled_banks)
    set(NEXO_VIF_COMPILED_DEFINE NEXO_VIF_COMPILED_BANKS=1)
elseif(NEXO_VU_BANK_CPP)
    # Reuse an existing object archive when its full code identity matches the
    # observed case. Unknown uploaded identities are rejected by the runtime.
    set(NEXO_VIF_COMPILED_PROVIDER nexo_vu_compiled_bank)
    set(NEXO_VIF_COMPILED_DEFINE NEXO_VIF_SINGLE_BANK=1)
endif()
if(NEXO_VIF_COMPILED_PROVIDER)
    add_executable(nexo_vif_native_replay src/vif_replay_main.cpp src/native_interpreter_traps.cpp tests/empty_ee_table.cpp)
    target_link_libraries(nexo_vif_native_replay PRIVATE nexo_vif_replay_support ${NEXO_VIF_COMPILED_PROVIDER})
    target_compile_definitions(nexo_vif_native_replay PRIVATE ${NEXO_VIF_COMPILED_DEFINE})
    set_property(TARGET nexo_vif_native_replay PROPERTY INTERPROCEDURAL_OPTIMIZATION FALSE)
    target_compile_options(nexo_vif_native_replay PRIVATE -O1 -fno-lto)
    foreach(nexo_interpreter_symbol IN LISTS NEXO_INTERPRETER_SYMBOLS)
        target_link_options(nexo_vif_native_replay PRIVATE "-Wl,--wrap=${nexo_interpreter_symbol}")
    endforeach()
endif()

add_executable(nexo_vu_replay src/vu_replay_main.cpp tests/empty_ee_table.cpp)
target_link_libraries(nexo_vu_replay PRIVATE nexo_vu_replay_support)

add_executable(nexo_vu_snapshot_tests tests/vu_snapshot_tests.cpp tests/empty_ee_table.cpp)
target_include_directories(nexo_vu_snapshot_tests PRIVATE "${PROJECT_SOURCE_DIR}/ps2xTest/include")
target_link_libraries(nexo_vu_snapshot_tests PRIVATE nexo_vu_replay_support)

foreach(nexo_target nexo_vu_replay_support nexo_vu_replay nexo_vu_snapshot_tests
        nexo_vu_native nexo_vu_native_frontend nexo_vu_runtime_binding nexo_vu_runtime_replay_support
        nexo_vu_native_tests nexo_vu_inspect nexo_device_snapshot_tests nexo_vif_replay_support nexo_vif_capture_tests nexo_vif_replay nexo_vif_issue_trace)
    set_property(TARGET ${nexo_target} PROPERTY INTERPROCEDURAL_OPTIMIZATION FALSE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(${nexo_target} PRIVATE -O1 -fno-lto)
    endif()
endforeach()

if(BUILD_TESTING)
    add_test(NAME nexo_pcsx2_reference_source_tests COMMAND "${Python3_EXECUTABLE}"
        -m unittest discover -s "${CMAKE_CURRENT_SOURCE_DIR}/tests" -p test_pcsx2_reference_source.py)
    add_test(NAME nexo_vu_timing_map_tests COMMAND "${Python3_EXECUTABLE}"
        -m unittest discover -s "${CMAKE_CURRENT_SOURCE_DIR}/tests" -p test_vu_timing_map.py)
    add_test(NAME nexo_vu_snapshot_tests COMMAND nexo_vu_snapshot_tests)
    add_test(NAME nexo_device_snapshot_tests COMMAND nexo_device_snapshot_tests)
    add_test(NAME nexo_vif_capture_tests COMMAND nexo_vif_capture_tests)
    add_test(NAME nexo_vu_native_tests COMMAND nexo_vu_native_tests)
    add_test(NAME nexo_vu_semantics_codegen_tests COMMAND "${Python3_EXECUTABLE}"
        -m unittest discover -s "${CMAKE_CURRENT_SOURCE_DIR}/tests" -p test_vu_semantics_codegen.py)
    add_test(NAME nexo_vu_bank_codegen_tests COMMAND "${Python3_EXECUTABLE}"
        -m unittest discover -s "${CMAKE_CURRENT_SOURCE_DIR}/tests" -p test_vu_bank_codegen.py)
    add_test(NAME nexo_vif_bank_codegen_tests COMMAND "${Python3_EXECUTABLE}"
        -m unittest discover -s "${CMAKE_CURRENT_SOURCE_DIR}/tests" -p test_vif_bank_codegen.py)
    add_test(NAME nexo_headless_isolation_tests COMMAND "${Python3_EXECUTABLE}"
        -m unittest discover -s "${PROJECT_SOURCE_DIR}/tools/ps2native/tests" -p test_headless_native_test.py)
endif()
