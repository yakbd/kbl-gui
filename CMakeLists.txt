cmake_minimum_required(VERSION 3.25)
project(yakbd-gui VERSION 0.2.0 LANGUAGES CXX)

# The full version, with a pre-release part CMake's VERSION cannot hold (0.2.0-preview.1). The release workflow sets it;
# local builds are "-dev". About, --version, package file names and release notes use it.
set(YAKBD_GUI_VERSION_FULL "${PROJECT_VERSION}-dev" CACHE STRING "Full version of the editor")
if(NOT YAKBD_GUI_VERSION_FULL MATCHES "^${PROJECT_VERSION}(-[0-9A-Za-z.]+)?$")
  message(FATAL_ERROR "YAKBD_GUI_VERSION_FULL ${YAKBD_GUI_VERSION_FULL} does not start with ${PROJECT_VERSION}")
endif()

if(WIN32)
  # Windows packages are one flat folder: the exe, Qt DLLs, plugins/ and qml/ beside it (and yakbd.exe). Set
  # before Qt's project setup, which records the install directories for its deployment script.
  set(CMAKE_INSTALL_BINDIR "." CACHE PATH "Executables (Windows: the package root)")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt is pinned to one release (tools/qt-pins.cmake); the build accepts that minor series only.
find_package(Qt6 6.12 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)
qt_standard_project_setup(REQUIRES 6.12)
set(QT_QML_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/qml)

option(YAKBD_GUI_TESTS "Build the tests (needs Qt Test and Qt Quick Test)" ON)

# The document model, CLI bridge and report: no UI, shared by the app and the tests.
add_library(kblcore STATIC
  src/jsontree.cpp src/jsontree.h
  src/kbldocument.cpp src/kbldocument.h src/kbllayout.cpp
  src/reportmodel.cpp src/reportmodel.h
  src/cli.cpp src/cli.h
  src/schemamodel.cpp src/schemamodel.h
  src/appsettings.cpp src/appsettings.h
)
target_include_directories(kblcore PUBLIC src)
target_link_libraries(kblcore PUBLIC Qt6::Core Qt6::Gui Qt6::Qml Qt6::QuickControls2)
target_compile_definitions(kblcore PUBLIC YAKBD_GUI_VERSION="${YAKBD_GUI_VERSION_FULL}")

add_subdirectory(qml)

qt_add_executable(yakbd-gui WIN32 MACOSX_BUNDLE src/main.cpp)
set_target_properties(yakbd-gui PROPERTIES
  OUTPUT_NAME yakbd-gui
  MACOSX_BUNDLE_BUNDLE_NAME "YAKBD Editor"  # the application menu: "About YAKBD Editor", "Quit YAKBD Editor"
  MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.yakbd.gui"
  MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
  MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
  MACOSX_BUNDLE_INFO_STRING "YAKBD Editor ${YAKBD_GUI_VERSION_FULL}"  # the short version must be numbers only
  MACOSX_BUNDLE_ICON_FILE "yakbd.icns"
)
if(APPLE)
  # Not "YAKBD": macOS file systems ignore case, and the bundle holds the CLI "yakbd" in the same folder.
  set_target_properties(yakbd-gui PROPERTIES OUTPUT_NAME "YAKBD Editor")
  set(icns ${CMAKE_SOURCE_DIR}/packaging/macos/yakbd.icns)
  if(EXISTS ${icns})
    target_sources(yakbd-gui PRIVATE ${icns})
    set_source_files_properties(${icns} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
  endif()
endif()
if(WIN32)
  configure_file(packaging/windows/yakbd-gui.rc.in ${CMAKE_CURRENT_BINARY_DIR}/yakbd-gui.rc @ONLY)
  target_sources(yakbd-gui PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/yakbd-gui.rc)
  # Packages keep Qt in Editor\qt\ as a private side-by-side assembly (package.ps1); an exe built with this option
  # starts only with that folder beside it, so plain development builds leave it off.
  option(YAKBD_QT_SUBFOLDER "Windows: find Qt in the qt\\ subfolder (private assembly; for packages)" OFF)
  if(YAKBD_QT_SUBFOLDER)
    target_sources(yakbd-gui PRIVATE packaging/windows/yakbd-gui.manifest)
  endif()
endif()
target_link_libraries(yakbd-gui PRIVATE kblcore Qt6::Quick Qt6::QuickControls2 yakbduiplugin)

include(GNUInstallDirs)
install(TARGETS yakbd-gui
  BUNDLE DESTINATION .
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
qt_generate_deploy_qml_app_script(
  TARGET yakbd-gui
  OUTPUT_SCRIPT deploy_script
  NO_TRANSLATIONS
  NO_COMPILER_RUNTIME  # Windows: package.ps1 copies the Visual C++ runtime DLLs itself (no vc_redist installer)
)
install(SCRIPT ${deploy_script})
if(UNIX AND NOT APPLE)
  install(FILES packaging/linux/io.github.yakbd.gui.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
  install(FILES qml/icons/yakbd.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
    RENAME io.github.yakbd.gui.svg)
endif()
if(UNIX AND NOT APPLE)
  install(FILES LICENSE NOTICE DESTINATION ${CMAKE_INSTALL_DOCDIR})
  install(DIRECTORY third_party/licenses DESTINATION ${CMAKE_INSTALL_DOCDIR})
endif()

if(YAKBD_GUI_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
