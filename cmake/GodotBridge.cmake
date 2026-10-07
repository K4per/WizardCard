# Optional: the standalone engine, CLI and tests do not discover Godot or godot-cpp.
set(WIZARD_GODOT_VERSION "4.7.2" CACHE STRING "Pinned editor/export-template version")
set(GODOTCPP_API_VERSION "4.7" CACHE STRING "Pinned extension API")
set(GODOTCPP_TARGET "template_debug" CACHE STRING "Godot extension target")
set(GODOTCPP_BUILD_PROFILE "${CMAKE_CURRENT_SOURCE_DIR}/godot/bridge_profile.json" CACHE FILEPATH "Minimal bridge API")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${GODOTCPP_BUILD_PROFILE}")
set(GODOTCPP_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
# JSON validation uses exceptions; keep the dependency and bridge on the same policy.
set(GODOTCPP_DISABLE_EXCEPTIONS OFF CACHE BOOL "" FORCE)
set(GODOTCPP_SYSTEM_HEADERS ON CACHE BOOL "" FORCE)
if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL" CACHE STRING "Consistent bridge CRT" FORCE)
endif()
if(NOT WIZARD_GODOT_VERSION STREQUAL "4.7.2" OR NOT GODOTCPP_API_VERSION STREQUAL "4.7")
  message(FATAL_ERROR "Update godot/toolchain-lock.json and validate before changing the Godot/API version")
endif()
FetchContent_Declare(godot_cpp
  GIT_REPOSITORY https://github.com/godotengine/godot-cpp.git
  GIT_TAG 507ed9d840c01a3c5b2a39af8bb4000bfac30bf5
  EXCLUDE_FROM_ALL)
FetchContent_MakeAvailable(godot_cpp)
foreach(target wizard_core wizard_content wizard_ai wizard_application wizard_interaction wizard_bridge_model)
  set_property(TARGET ${target} PROPERTY POSITION_INDEPENDENT_CODE ON)
  if(MSVC)
    set_property(TARGET ${target} PROPERTY MSVC_RUNTIME_LIBRARY "${CMAKE_MSVC_RUNTIME_LIBRARY}")
  endif()
endforeach()
add_library(wizard_godot_bridge SHARED src/bridge/godot_bridge.cpp)
target_link_libraries(wizard_godot_bridge PRIVATE wizard_bridge_model godot::cpp)
wizard_warnings(wizard_godot_bridge)
if(GODOTCPP_TARGET STREQUAL "template_release")
  set(bridge_flavor release)
else()
  set(bridge_flavor debug)
endif()
set_target_properties(wizard_godot_bridge PROPERTIES
  OUTPUT_NAME "wizard_godot_bridge.${bridge_flavor}"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/godot/bin"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/godot/bin")
foreach(config Debug Release RelWithDebInfo MinSizeRel)
  string(TOUPPER "${config}" upper_config)
  set_target_properties(wizard_godot_bridge PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_${upper_config} "${CMAKE_CURRENT_SOURCE_DIR}/godot/bin"
    LIBRARY_OUTPUT_DIRECTORY_${upper_config} "${CMAKE_CURRENT_SOURCE_DIR}/godot/bin")
endforeach()
set(GODOT_BIN "" CACHE FILEPATH "Godot executable (or set GODOT_BIN in the environment)")
if(NOT GODOT_BIN AND DEFINED ENV{GODOT_BIN})
  set(GODOT_BIN "$ENV{GODOT_BIN}")
endif()
if(BUILD_TESTING AND GODOT_BIN)
  execute_process(COMMAND "${GODOT_BIN}" --version OUTPUT_VARIABLE godot_version
    OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE godot_version_result TIMEOUT 10)
  if(NOT godot_version_result EQUAL 0 OR NOT godot_version STREQUAL "4.7.2.stable.official.ed1daf0bf")
    message(FATAL_ERROR "GODOT_BIN must match the tested Godot 4.7.2 editor in toolchain-lock.json")
  endif()
  # Isolated harness also loads the release DLL in an editor process without requiring a debug DLL.
  set(probe_dir "${CMAKE_CURRENT_BINARY_DIR}/godot-probe")
  file(MAKE_DIRECTORY "${probe_dir}/tests")
  file(WRITE "${probe_dir}/project.godot"
    "config_version=5\n[application]\nconfig/name=\"WizardCard Bridge Probe\"\n[rendering]\nrenderer/rendering_method=\"gl_compatibility\"\n")
  foreach(script bridge_smoke boundary_smoke local_ui_smoke local_ui_replay match_scene_smoke art_nonmodel_smoke)
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/godot/tests/${script}.gd" "${probe_dir}/tests/${script}.gd" COPYONLY)
  endforeach()
  # Exercise the actual client with the same extension artifact as the bridge tests.
  file(GLOB_RECURSE client_sources CONFIGURE_DEPENDS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/godot"
    "${CMAKE_CURRENT_SOURCE_DIR}/godot/scripts/*.gd" "${CMAKE_CURRENT_SOURCE_DIR}/godot/scenes/*.tscn"
    "${CMAKE_CURRENT_SOURCE_DIR}/godot/art/*.png" "${CMAKE_CURRENT_SOURCE_DIR}/godot/art/*.tres"
    "${CMAKE_CURRENT_SOURCE_DIR}/godot/art/*.tscn" "${CMAKE_CURRENT_SOURCE_DIR}/godot/art/*.gdshader"
    "${CMAKE_CURRENT_SOURCE_DIR}/godot/art/*.json")
  foreach(source IN LISTS client_sources)
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/godot/${source}" "${probe_dir}/${source}" COPYONLY)
  endforeach()
  file(GENERATE OUTPUT "${probe_dir}/wizard_bridge.gdextension" CONTENT
    "[configuration]\nentry_symbol=\"wizard_bridge_init\"\ncompatibility_minimum=\"4.7\"\n[libraries]\nwindows.debug.x86_64=\"$<TARGET_FILE:wizard_godot_bridge>\"\nwindows.release.x86_64=\"$<TARGET_FILE:wizard_godot_bridge>\"\nlinux.debug.x86_64=\"$<TARGET_FILE:wizard_godot_bridge>\"\nlinux.release.x86_64=\"$<TARGET_FILE:wizard_godot_bridge>\"\n")
  add_test(NAME godot_import COMMAND "${GODOT_BIN}" --headless --path ${probe_dir}
    --import --log-file ${CMAKE_CURRENT_BINARY_DIR}/godot_import.log)
  set_tests_properties(godot_import PROPERTIES FIXTURES_SETUP godot_import RUN_SERIAL TRUE TIMEOUT 120)
  add_test(NAME godot_bridge_boundary COMMAND "${GODOT_BIN}" --headless
    --path ${probe_dir} --log-file ${CMAKE_CURRENT_BINARY_DIR}/godot_boundary.log
    --script res://tests/boundary_smoke.gd -- --user-data ${CMAKE_CURRENT_BINARY_DIR}/boundary_data
    --assets ${CMAKE_CURRENT_SOURCE_DIR}/assets)
  set_tests_properties(godot_bridge_boundary PROPERTIES FIXTURES_REQUIRED godot_import TIMEOUT 30)
  add_test(NAME godot_local_ui COMMAND "${GODOT_BIN}" --headless --path ${probe_dir}
    --log-file ${CMAKE_CURRENT_BINARY_DIR}/godot_local_ui.log --script res://tests/local_ui_smoke.gd --
    --assets ${CMAKE_CURRENT_SOURCE_DIR}/assets --user-data ${CMAKE_CURRENT_BINARY_DIR}/local_ui_data)
  set_tests_properties(godot_local_ui PROPERTIES FIXTURES_REQUIRED godot_import TIMEOUT 60)
  add_test(NAME godot_match_scene COMMAND "${GODOT_BIN}" --headless --path ${probe_dir}
    --log-file ${CMAKE_CURRENT_BINARY_DIR}/godot_match_scene.log --script res://tests/match_scene_smoke.gd --
    --assets ${CMAKE_CURRENT_SOURCE_DIR}/assets --user-data ${CMAKE_CURRENT_BINARY_DIR}/match_scene_data)
  set_tests_properties(godot_match_scene PROPERTIES FIXTURES_REQUIRED godot_import TIMEOUT 60)
  add_test(NAME godot_art_nonmodel COMMAND "${GODOT_BIN}" --headless --path ${probe_dir}
    --log-file ${CMAKE_CURRENT_BINARY_DIR}/godot_art_nonmodel.log --script res://tests/art_nonmodel_smoke.gd)
  set_tests_properties(godot_art_nonmodel PROPERTIES FIXTURES_REQUIRED godot_import TIMEOUT 30)
  add_test(NAME godot_bridge_alpha_replay COMMAND "${GODOT_BIN}" --headless --path ${probe_dir}
    --log-file ${CMAKE_CURRENT_BINARY_DIR}/godot_alpha.log --script res://tests/bridge_smoke.gd --
    --assets ${CMAKE_CURRENT_SOURCE_DIR}/assets --user-data ${CMAKE_CURRENT_BINARY_DIR}/alpha_data
    --reference ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/alpha-v1-replay.json)
  set_tests_properties(godot_bridge_alpha_replay PROPERTIES FIXTURES_REQUIRED godot_import TIMEOUT 30)
  # CLI records and Godot submit independently; the script checks every saved digest.
  foreach(mode smoke ai-match tutorial-smoke)
    string(REPLACE "-" "_" test_name "${mode}")
    if(mode STREQUAL "smoke")
      set(session_mode hotseat)
    elseif(mode STREQUAL "ai-match")
      set(session_mode ai)
    else()
      set(session_mode tutorial)
    endif()
    add_test(NAME godot_reference_${test_name}
      COMMAND wizard_cli ${mode} ${CMAKE_CURRENT_SOURCE_DIR}/assets ${CMAKE_CURRENT_BINARY_DIR}/${test_name}.json)
    set_tests_properties(godot_reference_${test_name} PROPERTIES FIXTURES_SETUP godot_${test_name})
    add_test(NAME godot_bridge_${test_name}
      COMMAND "${GODOT_BIN}" --headless --path ${probe_dir}
      --log-file ${CMAKE_CURRENT_BINARY_DIR}/godot_${test_name}.log --script res://tests/bridge_smoke.gd --
      --assets ${CMAKE_CURRENT_SOURCE_DIR}/assets --user-data ${CMAKE_CURRENT_BINARY_DIR}/godot_${test_name}_data
      --reference ${CMAKE_CURRENT_BINARY_DIR}/${test_name}.json --report ${CMAKE_CURRENT_BINARY_DIR}/godot_${test_name}.json
      --mode ${session_mode})
    set_tests_properties(godot_bridge_${test_name} PROPERTIES FIXTURES_REQUIRED "godot_${test_name};godot_import" TIMEOUT 600)
    add_test(NAME godot_local_${test_name}
      COMMAND "${GODOT_BIN}" --headless --path ${probe_dir}
      --log-file ${CMAKE_CURRENT_BINARY_DIR}/godot_local_${test_name}.log --script res://tests/local_ui_replay.gd --
      --assets ${CMAKE_CURRENT_SOURCE_DIR}/assets --user-data ${CMAKE_CURRENT_BINARY_DIR}/local_${test_name}_data
      --reference ${CMAKE_CURRENT_BINARY_DIR}/${test_name}.json --report ${CMAKE_CURRENT_BINARY_DIR}/local_${test_name}.json
      --mode ${session_mode})
    set_tests_properties(godot_local_${test_name} PROPERTIES FIXTURES_REQUIRED "godot_${test_name};godot_import" TIMEOUT 180)
  endforeach()
  foreach(difficulty 0 2)
    add_test(NAME godot_reference_ai_${difficulty} COMMAND wizard_cli ai-match
      ${CMAKE_CURRENT_SOURCE_DIR}/assets ${CMAKE_CURRENT_BINARY_DIR}/ai_${difficulty}.json 42 ${difficulty})
    set_tests_properties(godot_reference_ai_${difficulty} PROPERTIES FIXTURES_SETUP godot_ai_${difficulty})
    add_test(NAME godot_local_ai_${difficulty} COMMAND "${GODOT_BIN}" --headless --path ${probe_dir}
      --log-file ${CMAKE_CURRENT_BINARY_DIR}/godot_local_ai_${difficulty}.log --script res://tests/local_ui_replay.gd --
      --assets ${CMAKE_CURRENT_SOURCE_DIR}/assets --user-data ${CMAKE_CURRENT_BINARY_DIR}/local_ai_${difficulty}_data
      --reference ${CMAKE_CURRENT_BINARY_DIR}/ai_${difficulty}.json --report ${CMAKE_CURRENT_BINARY_DIR}/local_ai_${difficulty}.json
      --mode ai --difficulty ${difficulty})
    set_tests_properties(godot_local_ai_${difficulty} PROPERTIES FIXTURES_REQUIRED "godot_ai_${difficulty};godot_import" TIMEOUT 180)
  endforeach()
  # Godot can exit successfully after a script/runtime error. Treat its diagnostic
  # output as a failure as well, instead of accepting only the process exit code.
  get_property(registered_tests DIRECTORY PROPERTY TESTS)
  foreach(test IN LISTS registered_tests)
    if(test MATCHES "^godot_")
      set_tests_properties(${test} PROPERTIES FAIL_REGULAR_EXPRESSION "SCRIPT ERROR;ERROR:")
    endif()
  endforeach()
endif()
