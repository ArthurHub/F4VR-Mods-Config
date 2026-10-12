# Unit tests for the mod's code that does not need the running game: the settings model and its loaders.
# The executable compiles the sources it covers, with the mod's PCH, and links the framework and CommonLibF4 as the
# mod does, so the code under test can use both. There is no game in the process: Setup.cpp sets REL::NO_GAME, so
# CommonLibF4 resolves every game address into a range that cannot be used, and a test that calls the game fails
# with an access violation.

find_package(Catch2 3 CONFIG REQUIRED)

add_executable(${PROJECT_NAME}_tests
  FullPanelRenderTests.cpp
  FullPanelTests.cpp
  McmLoaderTests.cpp
  Setup.cpp
  TestFiles.h
  TestPanel.h
  # the mod's sources under test
  ${SOURCE_DIR}/mcm/McmLoader.cpp
  ${SOURCE_DIR}/ui/Drawing.cpp
  ${SOURCE_DIR}/ui/FullPanel.cpp
  ${SOURCE_DIR}/ui/ModPage.cpp
  ${SOURCE_DIR}/ui/ModTree.cpp
)

# No link-time optimization: the tests don't need it, and it makes their link slow
set_target_properties(${PROJECT_NAME}_tests PROPERTIES INTERPROCEDURAL_OPTIMIZATION OFF)

target_compile_features(${PROJECT_NAME}_tests PRIVATE cxx_std_23)
target_compile_definitions(${PROJECT_NAME}_tests PRIVATE _UNICODE
  # where FullPanelRenderTests.cpp saves its images
  MODS_CONFIG_TEST_OUTPUT_DIR="${CMAKE_CURRENT_BINARY_DIR}/render"
)
target_include_directories(${PROJECT_NAME}_tests PRIVATE ${BUILD_DIR}/include ${SOURCE_DIR})
target_precompile_headers(${PROJECT_NAME}_tests PRIVATE ${SOURCE_DIR}/PCH.h)
target_compile_options(${PROJECT_NAME}_tests PRIVATE /utf-8 /permissive- /W4 /external:anglebrackets /external:W0)
target_link_libraries(${PROJECT_NAME}_tests PRIVATE F4VRCommon::framework Catch2::Catch2WithMain delayimp)

# openvr_api.dll is in the game's folder and not here, and the executable would not start without it. So it is
# loaded on its first call, which no test makes. Release drops the unused import whole, hence the ignored warning.
target_link_options(${PROJECT_NAME}_tests PRIVATE /DELAYLOAD:openvr_api.dll /IGNORE:4199)

include(Catch)
catch_discover_tests(${PROJECT_NAME}_tests)
