# Godot dependencies are discovered only when explicitly enabled.
set(WIZARD_GODOT_VERSION "4.7.2" CACHE STRING "Pinned editor/export-template version")
set(GODOTCPP_API_VERSION "4.7" CACHE STRING "Pinned extension API")
set(GODOTCPP_TARGET "template_debug" CACHE STRING "Godot extension target")
set(GODOTCPP_BUILD_PROFILE "${CMAKE_CURRENT_SOURCE_DIR}/godot/bridge_profile.json" CACHE FILEPATH "Minimal bridge API")
set(GODOTCPP_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(GODOTCPP_DISABLE_EXCEPTIONS OFF CACHE BOOL "" FORCE)
set(GODOTCPP_SYSTEM_HEADERS ON CACHE BOOL "" FORCE)
if(NOT WIZARD_GODOT_VERSION STREQUAL "4.7.2" OR NOT GODOTCPP_API_VERSION STREQUAL "4.7")
  message(FATAL_ERROR "Update godot/toolchain-lock.json and validate before changing the Godot/API version")
endif()
if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL" CACHE STRING "Bridge CRT" FORCE)
endif()
FetchContent_Declare(godot_cpp
  GIT_REPOSITORY https://github.com/godotengine/godot-cpp.git
  GIT_TAG 507ed9d840c01a3c5b2a39af8bb4000bfac30bf5 EXCLUDE_FROM_ALL)
FetchContent_MakeAvailable(godot_cpp)
foreach(target wizard_core wizard_content wizard_codec wizard_replay wizard_ai wizard_application wizard_interaction wizard_bridge_model)
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
set_target_properties(wizard_godot_bridge PROPERTIES OUTPUT_NAME "wizard_godot_bridge.${bridge_flavor}")
foreach(config Debug Release RelWithDebInfo MinSizeRel)
  string(TOUPPER "${config}" upper_config)
  set_target_properties(wizard_godot_bridge PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_${upper_config} "${CMAKE_CURRENT_SOURCE_DIR}/godot/bin"
    LIBRARY_OUTPUT_DIRECTORY_${upper_config} "${CMAKE_CURRENT_SOURCE_DIR}/godot/bin")
endforeach()
set_target_properties(wizard_godot_bridge PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/godot/bin"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/godot/bin")
set(GODOT_BIN "" CACHE FILEPATH "Pinned Godot executable")
if(BUILD_TESTING AND GODOT_BIN)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  execute_process(COMMAND "${GODOT_BIN}" --version OUTPUT_VARIABLE godot_version
    OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE godot_version_result TIMEOUT 10)
  if(NOT godot_version_result EQUAL 0 OR NOT godot_version STREQUAL "4.7.2.stable.official.ed1daf0bf")
    message(FATAL_ERROR "GODOT_BIN must match godot/toolchain-lock.json")
  endif()
  set(probe_dir "${CMAKE_CURRENT_BINARY_DIR}/godot-probe")
  file(MAKE_DIRECTORY "${probe_dir}/tests")
  file(WRITE "${probe_dir}/project.godot"
    "config_version=5\n[application]\nconfig/name=\"WizardCard Bridge Probe\"\n[rendering]\nrenderer/rendering_method=\"gl_compatibility\"\n")
  foreach(script bridge_smoke boundary_smoke)
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/godot/tests/${script}.gd" "${probe_dir}/tests/${script}.gd" COPYONLY)
  endforeach()
  file(GENERATE OUTPUT "${probe_dir}/wizard_bridge.gdextension" CONTENT
    "[configuration]\nentry_symbol=\"wizard_bridge_init\"\ncompatibility_minimum=\"4.7\"\n[libraries]\nwindows.debug.x86_64=\"$<TARGET_FILE:wizard_godot_bridge>\"\nwindows.release.x86_64=\"$<TARGET_FILE:wizard_godot_bridge>\"\nlinux.debug.x86_64=\"$<TARGET_FILE:wizard_godot_bridge>\"\nlinux.release.x86_64=\"$<TARGET_FILE:wizard_godot_bridge>\"\n")
  # Cold GDExtension docs can race immediate editor shutdown (Godot #111048).
  # Keep --import's completion check; allow deferred docs callbacks to finish.
  add_test(NAME godot_import COMMAND "${GODOT_BIN}" --headless --path ${probe_dir} --import --frame-delay 1000)
  set_tests_properties(godot_import PROPERTIES FIXTURES_SETUP godot_import RUN_SERIAL TRUE TIMEOUT 120)
  add_test(NAME godot_bridge_boundary COMMAND "${GODOT_BIN}" --headless --path ${probe_dir}
    --script res://tests/boundary_smoke.gd -- --assets ${CMAKE_CURRENT_SOURCE_DIR}/assets
    --user-data ${CMAKE_CURRENT_BINARY_DIR}/boundary-data)
  set_tests_properties(godot_bridge_boundary PROPERTIES FIXTURES_REQUIRED godot_import TIMEOUT 30)
  add_test(NAME godot_reference COMMAND wizard_cli smoke ${CMAKE_CURRENT_SOURCE_DIR}/assets ${CMAKE_CURRENT_BINARY_DIR}/godot-reference.json 42)
  set_tests_properties(godot_reference PROPERTIES FIXTURES_SETUP godot_reference)
  add_test(NAME godot_cold_import COMMAND "${Python3_EXECUTABLE}"
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/godot_cold_import.py --godot "${GODOT_BIN}"
    --probe ${probe_dir} --output ${CMAKE_CURRENT_BINARY_DIR}/cold-import)
  set_tests_properties(godot_cold_import PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE
    FAIL_REGULAR_EXPRESSION "SCRIPT ERROR;ERROR:")
  add_test(NAME godot_bridge_replay COMMAND "${GODOT_BIN}" --headless --path ${probe_dir}
    --script res://tests/bridge_smoke.gd -- --assets ${CMAKE_CURRENT_SOURCE_DIR}/assets
    --user-data ${CMAKE_CURRENT_BINARY_DIR}/replay-data --reference ${CMAKE_CURRENT_BINARY_DIR}/godot-reference.json
    --report ${CMAKE_CURRENT_BINARY_DIR}/godot-replay-report.json)
  set_tests_properties(godot_bridge_replay PROPERTIES FIXTURES_REQUIRED "godot_import;godot_reference" TIMEOUT 120)
  foreach(test godot_import godot_bridge_boundary godot_bridge_replay)
    set_tests_properties(${test} PROPERTIES FAIL_REGULAR_EXPRESSION "SCRIPT ERROR;ERROR:")
  endforeach()
endif()
