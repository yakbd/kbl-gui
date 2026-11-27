find_package(Qt6 REQUIRED COMPONENTS Test QuickTest)

# The released CLI the integration tests run: -DYAKBD_CLI=/path/to/yakbd, or fetched once into the build tree
# from the release pinned in tools/cli-pins.cmake.
set(YAKBD_CLI "" CACHE FILEPATH "yakbd binary for the integration tests (empty: fetch the pinned release)")
if(NOT YAKBD_CLI AND CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
  set(cli_dir ${CMAKE_BINARY_DIR}/cli)
  if(NOT EXISTS ${cli_dir}/yakbd)
    execute_process(COMMAND ${CMAKE_COMMAND} -DASSET=linux-amd64 -DDEST=${cli_dir}
                    -P ${CMAKE_SOURCE_DIR}/tools/fetch-cli.cmake RESULT_VARIABLE rc)
    if(NOT rc EQUAL 0)
      message(WARNING "could not fetch the yakbd CLI; the integration tests will be skipped")
    endif()
  endif()
  if(EXISTS ${cli_dir}/yakbd)
    set(YAKBD_CLI ${cli_dir}/yakbd)
  endif()
endif()

set(test_env "QT_QPA_PLATFORM=offscreen;YAKBD_TESTDATA=${CMAKE_CURRENT_SOURCE_DIR}/data;YAKBD_CLI=${YAKBD_CLI}")

function(yakbd_test name)
  qt_add_executable(${name} ${name}.cpp)
  target_link_libraries(${name} PRIVATE kblcore Qt6::Test)
  add_test(NAME ${name} COMMAND ${name})
  set_tests_properties(${name} PROPERTIES ENVIRONMENT "${test_env}")
endfunction()

yakbd_test(tst_jsontree)
yakbd_test(tst_document)
yakbd_test(tst_cli)

# QML views, offscreen, with the app's QML module.
qt_add_executable(tst_qml tst_qml.cpp)
target_link_libraries(tst_qml PRIVATE kblcore Qt6::QuickTest Qt6::Quick Qt6::QuickControls2 yakbduiplugin)
target_compile_definitions(tst_qml PRIVATE QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml")
add_test(NAME tst_qml COMMAND tst_qml)
set_tests_properties(tst_qml PROPERTIES ENVIRONMENT "${test_env};QT_QUICK_CONTROLS_STYLE=Fusion")

# Screenshots in light and dark under Xvfb with the GTK platform theme (Linux, when xvfb-run exists).
find_program(XVFB_RUN xvfb-run)
if(XVFB_RUN AND UNIX AND NOT APPLE)
  add_test(NAME screenshots
           COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/screenshots.sh $<TARGET_FILE:yakbd-gui> ${CMAKE_BINARY_DIR}/screenshots
                   ${CMAKE_CURRENT_SOURCE_DIR}/data)
  set_tests_properties(screenshots PROPERTIES ENVIRONMENT "YAKBD_CLI=${YAKBD_CLI}" TIMEOUT 300)
  add_executable(pngstat pngstat.cpp)
  target_link_libraries(pngstat PRIVATE Qt6::Gui)
endif()

# The main window with the platform's standard shortcuts (⌘ on macOS) and the live appearance switch.
qt_add_executable(tst_window tst_window.cpp)
target_link_libraries(tst_window PRIVATE kblcore Qt6::Test Qt6::Quick Qt6::QuickControls2 yakbduiplugin)
# The results also go to a file: Windows does not hand a GUI program's output to ctest.
add_test(NAME tst_window COMMAND tst_window -o ${CMAKE_CURRENT_BINARY_DIR}/tst_window.txt,txt -o -,txt)
if(APPLE OR WIN32)
  # The native platform (cocoa, windows): only there do the standard keys become the platform's shortcuts (⌘ on
  # macOS) and does the colour scheme switch.
  if(APPLE)
    set(native cocoa)
  else()
    set(native windows)
  endif()
  set_tests_properties(tst_window PROPERTIES ENVIRONMENT
    "YAKBD_TESTDATA=${CMAKE_CURRENT_SOURCE_DIR}/data;YAKBD_CLI=${YAKBD_CLI};QT_QPA_PLATFORM=${native}")
else()
  set_tests_properties(tst_window PROPERTIES ENVIRONMENT "${test_env}")
endif()
