# Unit tests for the mod's code that does not need the running game: the settings model and its loaders.
# The executable compiles the sources it covers, with the mod's PCH, and links the framework and CommonLibF4 as the
# mod does, so the code under test can use both. There is no game in the process: Setup.cpp sets REL::NO_GAME, so
# CommonLibF4 resolves every game address into a range that cannot be used, and a test that calls the game fails
# with an access violation.

find_package(Catch2 3 CONFIG REQUIRED)

add_executable(${PROJECT_NAME}_tests
  ConditionsTests.cpp
  FallUITranslationsTests.cpp
  FullPanelRenderTests.cpp
  FullPanelTests.cpp
  IniFileTests.cpp
  LiveCopiesTests.cpp
  McmLoaderTests.cpp
  McmStoreTests.cpp
  MenuControlsTests.cpp
  ModFileTests.cpp
  NativeClientsTests.cpp
  NumbersTests.cpp
  ConfigSpecLoaderTests.cpp
  ConfigSpecStoreTests.cpp
  SwfImagesTests.cpp
  TextMarkupTests.cpp
  TomlFileTests.cpp
  TranslationsTests.cpp
  Setup.cpp
  TestActions.cpp
  TestActions.h
  TestEvents.cpp
  TestEvents.h
  TestFiles.h
  TestGlobals.cpp
  TestGlobals.h
  TestProperties.cpp
  TestProperties.h
  TestSwf.h
  TestPanel.h
  # the mod's sources under test. Not GameActions.cpp, GameGlobals.cpp, GameProperties.cpp and
  # mcm/McmEvents.cpp, which call the game: TestActions.cpp, TestEvents.cpp, TestGlobals.cpp and
  # TestProperties.cpp have the tests' own functions in their place
  ${SOURCE_DIR}/api/NativeClients.cpp
  ${SOURCE_DIR}/mcm/FallUITranslations.cpp
  ${SOURCE_DIR}/mcm/McmConfigImages.cpp
  ${SOURCE_DIR}/mcm/McmKeybinds.cpp
  ${SOURCE_DIR}/mcm/McmLoader.cpp
  ${SOURCE_DIR}/mcm/McmModSettings.cpp
  ${SOURCE_DIR}/mcm/McmStore.cpp
  ${SOURCE_DIR}/mcm/SwfImages.cpp
  ${SOURCE_DIR}/model/ChoiceOptions.cpp
  ${SOURCE_DIR}/model/ClientFunctions.cpp
  ${SOURCE_DIR}/model/Conditions.cpp
  ${SOURCE_DIR}/model/TextMarkup.cpp
  ${SOURCE_DIR}/spec/IniFile.cpp
  ${SOURCE_DIR}/spec/LiveCopies.cpp
  ${SOURCE_DIR}/spec/ModFile.cpp
  ${SOURCE_DIR}/spec/ConfigSpecLoader.cpp
  ${SOURCE_DIR}/spec/ConfigSpecStore.cpp
  ${SOURCE_DIR}/spec/TomlFile.cpp
  ${SOURCE_DIR}/model/Translations.cpp
  ${SOURCE_DIR}/ui/Buttons.cpp
  ${SOURCE_DIR}/ui/Choices.cpp
  ${SOURCE_DIR}/ui/ConfirmDialog.cpp
  ${SOURCE_DIR}/ui/Developer.cpp
  ${SOURCE_DIR}/ui/Drawing.cpp
  ${SOURCE_DIR}/ui/FrontPage.cpp
  ${SOURCE_DIR}/ui/FullPanel.cpp
  ${SOURCE_DIR}/ui/HelpLine.cpp
  ${SOURCE_DIR}/ui/HelpPage.cpp
  ${SOURCE_DIR}/ui/MenuControls.cpp
  ${SOURCE_DIR}/ui/ModPage.cpp
  ${SOURCE_DIR}/ui/Numbers.cpp
  ${SOURCE_DIR}/ui/ConfigSpecReport.cpp
  ${SOURCE_DIR}/ui/PageImages.cpp
  ${SOURCE_DIR}/ui/PageRows.cpp
  ${SOURCE_DIR}/ui/Sections.cpp
  ${SOURCE_DIR}/ui/SettingRow.cpp
  ${SOURCE_DIR}/ui/Sliders.cpp
  ${SOURCE_DIR}/ui/TextInputs.cpp
  ${SOURCE_DIR}/ui/TitleBar.cpp
  ${SOURCE_DIR}/ui/ModTree.cpp
  ${SOURCE_DIR}/ui/UnsavedDialog.cpp
)

# No link-time optimization: the tests don't need it, and it makes their link slow
set_target_properties(${PROJECT_NAME}_tests PROPERTIES INTERPROCEDURAL_OPTIMIZATION OFF)

target_compile_features(${PROJECT_NAME}_tests PRIVATE cxx_std_23)
target_compile_definitions(${PROJECT_NAME}_tests PRIVATE _UNICODE
  # where FullPanelRenderTests.cpp saves its images
  MODS_CONFIG_TEST_OUTPUT_DIR="${CMAKE_CURRENT_BINARY_DIR}/render"
  # the MCM showcase, which is an optional file of the mod
  MODS_CONFIG_MCM_SHOWCASE_DIR="${ROOT_DIR}/data/showcase/MCM/Config"
  # the mod's own config spec, and the config file it edits as the mod ships it
  MODS_CONFIG_SPECS_DIR="${ROOT_DIR}/data/mod/ModsConfigVR/Config"
  MODS_CONFIG_SHIPPED_CONFIG="${ROOT_DIR}/data/config/ModsConfigVR.toml"
  # the config spec showcase, which is an optional file of the mod
  MODS_CONFIG_SPEC_SHOWCASE_DIR="${ROOT_DIR}/data/showcase/ModsConfigVR/Config"
)
target_include_directories(${PROJECT_NAME}_tests PRIVATE ${BUILD_DIR}/include ${SOURCE_DIR})
target_precompile_headers(${PROJECT_NAME}_tests PRIVATE ${SOURCE_DIR}/PCH.h)
target_compile_options(${PROJECT_NAME}_tests PRIVATE /utf-8 /permissive- /W4 /external:anglebrackets /external:W0)
target_link_libraries(${PROJECT_NAME}_tests PRIVATE F4VRCommon::framework Catch2::Catch2WithMain ZLIB::ZLIB delayimp)

# openvr_api.dll is in the game's folder and not here, and the executable would not start without it. So it is
# loaded on its first call, which no test makes. Release drops the unused import whole, hence the ignored warning.
target_link_options(${PROJECT_NAME}_tests PRIVATE /DELAYLOAD:openvr_api.dll /IGNORE:4199)

include(Catch)
catch_discover_tests(${PROJECT_NAME}_tests)
