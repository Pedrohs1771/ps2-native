execute_process(
  COMMAND "${TEST_HELPER}" --write-fixture "${FIXTURE}"
  RESULT_VARIABLE fixture_result
  ERROR_VARIABLE fixture_error
)
if(NOT fixture_result EQUAL 0)
  message(FATAL_ERROR "Could not generate ISO fixture: ${fixture_error}")
endif()

execute_process(
  COMMAND "${INSPECTOR}" --json "${FIXTURE}"
  RESULT_VARIABLE inspect_result
  OUTPUT_VARIABLE json_output
  ERROR_VARIABLE inspect_error
)
if(NOT inspect_result EQUAL 0)
  file(REMOVE "${FIXTURE}")
  message(FATAL_ERROR "JSON inspection failed: ${inspect_error}")
endif()

foreach(required_token IN ITEMS
    "\"schema_version\": 1"
    "\"image\": {"
    "\"sha256\":"
    "\"entries\": ["
    "\"system_cnf\": {"
    "\"boot2\":"
    "\"version\":"
    "\"video_mode\":"
    "\"elf\": {"
    "\"byte_order\": \"little-endian\""
    "\"machine\": \"MIPS R5900\""
    "\"entrypoint\": \"0x00100000\""
    "\"load_segments\": ["
    "\"permissions\": \"R-X\""
)
  string(FIND "${json_output}" "${required_token}" token_position)
  if(token_position EQUAL -1)
    message(FATAL_ERROR "JSON output is missing schema token: ${required_token}")
  endif()
endforeach()

if(NOT json_output MATCHES "^[ \t\r\n]*\\{[\r\n]")
  message(FATAL_ERROR "JSON output does not begin with an object")
endif()
if(NOT json_output MATCHES "[\r\n]\\}[ \t\r\n]*$")
  message(FATAL_ERROR "JSON output does not end with an object")
endif()

set(EXTRACTED "${FIXTURE}.contents")
execute_process(
  COMMAND "${INSPECTOR}" extract "${FIXTURE}" "${EXTRACTED}"
  RESULT_VARIABLE extract_result
  OUTPUT_VARIABLE extract_output
  ERROR_VARIABLE extract_error
)
if(NOT extract_result EQUAL 0)
  file(REMOVE_RECURSE "${EXTRACTED}")
  file(REMOVE "${FIXTURE}")
  message(FATAL_ERROR "Synthetic ISO extraction failed: ${extract_error}")
endif()
if(NOT EXISTS "${EXTRACTED}/SYSTEM.CNF" OR NOT EXISTS "${EXTRACTED}/SLPM_551.08" OR
   NOT EXISTS "${EXTRACTED}/DATA/README.TXT")
  file(REMOVE_RECURSE "${EXTRACTED}")
  file(REMOVE "${FIXTURE}")
  message(FATAL_ERROR "Extracted tree is missing normalized paths")
endif()
file(SHA256 "${EXTRACTED}/SLPM_551.08" extracted_elf_sha256)
if(NOT extracted_elf_sha256 STREQUAL "e6f753cba55fc548a59c3fb43a177f34180c3e3565cab927b4ee32b1c302c3c3")
  file(REMOVE_RECURSE "${EXTRACTED}")
  file(REMOVE "${FIXTURE}")
  message(FATAL_ERROR "Extracted boot ELF does not match its ISO bytes")
endif()
file(REMOVE_RECURSE "${EXTRACTED}")
file(REMOVE "${FIXTURE}")
