#include "devbench/MCDevBench.h"

#include <stdexcept>

#include <nlohmann/json.hpp>

#include "ModsConfigVR.h"
#include "devbench/DevBench.h"

namespace mods_config::devbench
{
    namespace
    {
        using json = nlohmann::json;

        /**
         * One frame of the mod's state. Plain values only: a snapshot outlives its frame.
         */
        struct MCDevState
        {
            bool menuOpen = false;
            bool menuMinimized = false;
            bool frikGestureTaken = false;
        };

        const char* stateDescription()
        {
            return "menu: open, minimized (the panel is hidden and only the offhand buttons show); frik: gestureTaken (FRIK's open-config binding is turned off)";
        }

        void captureState(MCDevState& state)
        {
            const auto& menu = g_modsConfig.menu();
            state.menuOpen = menu.isOpen();
            state.menuMinimized = menu.isMinimized();
            state.frikGestureTaken = g_modsConfig.isFrikGestureTaken();
        }

        json stateToJson(const MCDevState& state)
        {
            return {
                { "menu", { { "open", state.menuOpen }, { "minimized", state.menuMinimized } } },
                { "frik", { { "gestureTaken", state.frikGestureTaken } } },
            };
        }

        json runOpen(const json&)
        {
            if (!ModsConfigVR::isPlayerLoaded()) {
                throw std::runtime_error("no save is loaded");
            }
            auto& menu = g_modsConfig.menu();
            menu.open();
            return { { "open", menu.isOpen() } };
        }

        json runClose(const json&)
        {
            auto& menu = g_modsConfig.menu();
            menu.close();
            return { { "open", menu.isOpen() } };
        }
    }

    void setupDevBenchTool()
    {
        f4cf::devbench::setToolDescription("Mods Config VR: the in-game configuration menu for mods - open and close the menu, read its state");

        f4cf::devbench::setStateProvider<MCDevState>(stateDescription(), &captureState, &stateToJson);

        f4cf::devbench::addAction({
            .name = "open",
            .description = "open the config menu",
            .handler = &runOpen,
        });
        f4cf::devbench::addAction({
            .name = "close",
            .description = "close the config menu",
            .handler = &runClose,
        });
    }
}
