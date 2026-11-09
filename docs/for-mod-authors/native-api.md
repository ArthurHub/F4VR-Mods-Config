# The Native API

For a mod with a DLL: what your DLL adds to its config spec through Mods Config FO4 VR's native API, and how. The config spec comes first: the API adds to it what only your code knows.

Where to start: [For Mod Authors](README.md). The API in full: [Native API](../design/architecture.md#native-api) in the design, and the client header [src/api/ModsConfigFO4Api.h](../../src/api/ModsConfigFO4Api.h).

## Contents

- [What It Adds](#what-it-adds)
- [CommonFramework Mods](#commonframework-mods)
- [Connecting Another DLL](#connecting-another-dll)
- [Functions for Buttons](#functions-for-buttons)
- [States](#states)
- [Options of a Choice](#options-of-a-choice)
- [Quick Launch](#quick-launch)
- [Live Preview and Saves](#live-preview-and-saves)
- [The Menu's Events](#the-menus-events)
- [Your Own UI](#your-own-ui)
- [Rules](#rules)
- [Versions](#versions)

## What It Adds

| Your DLL gives | So that |
| --- | --- |
| Functions | A `CallNativeFunction` button runs your code, and the help line shows its message |
| States | A condition, a text or a setting reads a value only your code has, as whether a calibration runs |
| Options | A choice lists what only your code knows, as your presets |
| Quick launch entries | The menu's front page opens your own UI |
| Handlers of changes and Saves | A `live` setting shows in the game as the player changes it, and your mod applies what a Save wrote |
| Handlers of the menu's events | Your mod shows in the world what a page sets while the player is on it |
| Calls of the menu | Your own UI moves the menu aside and back, hands its edits to the menu, and opens it at a page |

The menu matches your DLL to the config spec whose `modName` is the name it connects under. Without Mods Config FO4 VR installed nothing is called, and your mod works as it does.

## CommonFramework Mods

A mod built on the [F4VR Common Framework](https://github.com/ArthurHub/F4VR-CommonFramework) is connected by the framework, under its `Settings::name`, with nothing to write. It has, with no code of its own:

- **Live preview** of every `live` setting of its own config file: the framework applies the menu's changes as overrides of the file and reloads the config, as it does for a change of the file.
- **`configFile`,** a state with the path of the config file the mod loaded, for the file's `pathFromClientState`.
- **`modVersion`,** a state with the mod's version, for a text as `Version {clientState:modVersion}`.

What it adds beyond that, it adds to the same client, `ModsConfigFO4Client::get()`, at any time. The framework's [Mods Config client](https://github.com/ArthurHub/F4VR-CommonFramework/blob/main/src/modsconfig/README.md) has the details, and `Settings::modsConfigClient = false` turns the connection off.

## Connecting Another DLL

Copy `ModsConfigFO4Api.h` into your mod as it is, and connect its client once, from F4SE's `kPostLoad` on, under the `modName` of your config spec:

```cpp
#include "ModsConfigFO4Api.h"

using namespace mods_config::api;

// in your F4SE message handler, at kPostLoad
auto& modsConfig = ModsConfigFO4Client::get();
if (modsConfig.connect("MyMod") != 0) {
    // Mods Config FO4 VR is not installed: the mod works without it
}
```

What you add before `connect` is given to the menu as it connects, and what you add after it at once.

## Functions for Buttons

A config spec's button runs a function of your DLL by its name, with its named `params` in their JSON types:

```json
{ "type": "button", "text": "Calibrate", "buttonText": "Start", "action": { "type": "CallNativeFunction", "function": "calibrate", "params": { "samples": 30 } } }
```

```cpp
modsConfig.addFunction("calibrate", [](const Params& params) {
    const float height = measureHeight(params.getInt("samples", 10));
    return FunctionResult{ .done = true, .message = std::format("Done: {:.2f} m.", height) };
});
```

A function returns at once, and the help line shows its message, in red when `done` is false. Work that takes longer goes on after it returns, and a state tells the rows that it runs. A button whose function your DLL doesn't have is disabled, with the reason.

## States

A state is a value your code answers by its name, asked once a frame while your mod is shown. A config spec uses it in three places:

- **A condition:** `"enabledWhen": { "clientState": "calibrating", "equals": false }`.
- **A text:** `"Status: {clientState:status}"`, in a text row, or in the text or the help of a setting, a button or a section.
- **A setting kept in it:** `"storage": { "clientState": "pipboyType" }`, a value in your DLL's memory, which the setter applies as the menu changes it.

```cpp
modsConfig.addState("calibrating", [] { return std::optional<Value>(isCalibrating()); });

modsConfig.addState("pipboyType", [] { return std::optional<Value>(pipboyType()); },
    [](const Value& value) { setPipboyType(std::get<int>(value)); });
```

A state answers at once and changes nothing, and an empty answer is "not available". A setter applies a value, and doesn't keep it: a mod that keeps the value after a restart writes it from its handler of the Saves. A state with no setter is a `readOnly` setting's.

## Options of a Choice

A choice with `"listFromClientCallback": true` takes its options from your DLL, asked by the setting's id as the mod is picked:

```cpp
modsConfig.addOptions("preset", [] {
    return std::vector<Option>{ { .value = std::string("default"), .text = "Default" }, { .value = std::string("sniper"), .text = "Sniper" } };
});
```

## Quick Launch

An entry of the quick launch section, on the menu's front page, runs a function of your DLL, which opens your own UI or does a quick action. It needs no config spec:

```cpp
modsConfig.addQuickLaunch("recalibrate", { .text = "Recalibrate", .help = "Measures your height again.", .buttonText = "Run" },
    [](const Params&) { return FunctionResult{ .done = true, .message = "Calibrated." }; });
```

A button of the config spec can run the same function by its name.

## Live Preview and Saves

A `live` setting's change reaches your DLL as the player makes it: each valid pending value, and a Discard back to where the session started. A value that is dragged is told at most every 50 ms, and always the value it ends on; a Discard and a Save are told at once. After a Save, your DLL is told once what it wrote, every setting of any `applies`.

```cpp
modsConfig.subscribeToChanges([](const Change& change) {
    // change.id, where it is kept (change.filePath and change.path, a form, or a state), change.value, change.discard
    applyPreview(change.id, change.value);
});

modsConfig.subscribeToSaves([](const Saved& saved) {
    // saved.settings, each with its value before the Save and the value written, and saved.files
    reloadMyConfig();
});
```

The file or the game has a saved value by the time you're told, so a preview ends with the Save. A CommonFramework mod's own config file is previewed by the framework: these handlers are for a setting of another file or of the game.

A plugin named in the config spec's `f4sePluginRequirements` also gets an F4SE message after each Save that wrote something, `MODS_CONFIG_FO4_SAVED_MESSAGE`, from the sender `ModsConfigFO4VR`, with the `modName` as its data. It doesn't say what changed: `subscribeToSaves` does.

## The Menu's Events

Your DLL is told when the menu opens and closes, when your mod is shown and left, and when a page of your config spec that has an `id` is shown and left:

```cpp
modsConfig.subscribeToMenuEvents([](const MenuEventType event, std::string_view pageId) {
    if (event == MenuEventType::PageShown && pageId == "spheres") {
        showActivationSpheres();
    }
});
const bool spheresShown = modsConfig.shownPage() == "spheres";
```

Immersive Flashlight VR draws its activation spheres in the world while the page of its spheres is shown.

## Your Own UI

A function that opens your own UI moves the menu aside as it opens, and back as it closes:

```cpp
modsConfig.addQuickLaunch("openPipboyConfig", { .text = "FRIK: Pip-Boy", .help = "Moves the Pip-Boy on your wrist.", .buttonText = "Place" },
    [&modsConfig](const Params&) {
        if (!isSkeletonReady()) {
            return FunctionResult{ .done = false, .message = "The body is not ready yet." };
        }
        openPipboyConfig([&modsConfig] { modsConfig.restoreMenu(); });
        modsConfig.minimizeMenu();
        return FunctionResult{ .done = true };
    });
```

- **`minimizeMenu`** hides the menu, and keeps its changes pending. **`restoreMenu`** shows it again, only while it is minimized, and reads your mod's values again.
- **`setPendingValue`** hands an edit of your UI to the menu as the setting's pending value, by its id, so the menu's Save writes it with the rest, and its Discard drops it. Don't save the settings your UI edits yourself:

  ```cpp
  modsConfig.setPendingValue("fScale:Main", 1.5f);
  ```

  A setting with no `id` of its own is named by its key and its section. A value that is no value of its setting, as one outside its range, is refused, and the call returns false.

  Apply the value in your mod before you hand it over. A `live` setting's change comes back to your change handlers during the call, marked `handedOver`, so a handler that previews changes skips it.
- **`reloadValues`** reads your mod's values again, for a change your mod saved itself while its page is shown.
- **`openMenu`** opens the menu at your mod, or at a page of it by its `id`: a wrist button or a holotape opens your settings this way.

## Rules

- **The game's main thread:** every call, both ways, is on it.
- **Return at once:** a function, a state and a handler answer and return. Longer work goes on after.
- **Copy what you keep:** the texts the menu passes are valid during the call.
- **The first name stays:** a second function, state or option list of the same name is refused, and so is a second client of a mod's name.
- **A client that fails is cut off:** a call that throws or faults cuts your DLL off until the game restarts. Your mod is then a mod with no DLL in the menu, and every other mod keeps working.

## Versions

- Until version 1.0 of Mods Config FO4 VR, the API changes as the work needs it, so build your DLL with the header of the menu it runs with.
- From 1.0 it only grows: a newer menu takes a client of an older header. A client of a newer header checks `modsConfig.apiVersion()` before it uses what was added, or passes the version it needs to `connect`, which an older menu then refuses.
