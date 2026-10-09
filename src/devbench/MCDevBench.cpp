#include "devbench/MCDevBench.h"

#include <stdexcept>

#include <nlohmann/json.hpp>

#include "ModsConfigVR.h"
#include "devbench/DevBench.h"
#include "imgui/ImGuiPointer.h"

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

            // the pointer, as the frame before saw it
            bool pointerOnPanel = false;
            float pointerX = 0.0f;
            float pointerY = 0.0f;
            bool pointerPrimaryHand = true;
            bool pointerDown = false;
            bool pointerWanted = false;

            // the mods the menu lists
            std::size_t modCount = 0;
        };

        const char* stateDescription()
        {
            return "menu: open, minimized (the panel is hidden and only the offhand buttons show); "
                   "pointer: onPanel (a wand's ray is on the panel), hand (primary or offhand), "
                   "x and y (where, in the panel's pixels from its top left), down (it presses the panel), wanted (ImGui uses the pointer); "
                   "mods: count (the mods the menu lists; the mods action has each of them); "
                   "frik: gestureTaken (FRIK's open-config binding is turned off)";
        }

        void captureState(MCDevState& state)
        {
            const auto& menu = g_modsConfig.menu();
            state.menuOpen = menu.isOpen();
            state.menuMinimized = menu.isMinimized();
            state.frikGestureTaken = g_modsConfig.isFrikGestureTaken();

            const auto& pointer = imgui::pointer().state();
            state.pointerOnPanel = pointer.canvas != nullptr;
            state.pointerX = pointer.x;
            state.pointerY = pointer.y;
            state.pointerPrimaryHand = pointer.primaryHand;
            state.pointerDown = pointer.down;
            state.pointerWanted = pointer.wantsPointer;

            state.modCount = menu.mods().size();
        }

        json stateToJson(const MCDevState& state)
        {
            return {
                { "menu", { { "open", state.menuOpen }, { "minimized", state.menuMinimized } } },
                { "pointer",
                    { { "onPanel", state.pointerOnPanel },
                        { "hand", state.pointerPrimaryHand ? "primary" : "offhand" },
                        { "x", state.pointerX },
                        { "y", state.pointerY },
                        { "down", state.pointerDown },
                        { "wanted", state.pointerWanted } } },
                { "mods", { { "count", state.modCount } } },
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

        /**
         * The mods the menu lists, in its order: each with its names and the names of its pages.
         */
        json runMods(const json&)
        {
            auto mods = json::array();
            for (const auto& mod : g_modsConfig.menu().mods()) {
                auto pages = json::array();
                for (const auto& page : mod.pages) {
                    pages.push_back(page.name);
                }
                mods.push_back({ { "name", mod.name }, { "displayName", mod.displayName }, { "pages", std::move(pages) } });
            }
            return { { "mods", std::move(mods) } };
        }
    }

    void setupDevBenchTool()
    {
        f4cf::devbench::setToolDescription("Mods Config VR: the in-game configuration menu for mods - open and close the menu, read its state and the mods it lists");

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
        f4cf::devbench::addAction({
            .name = "mods",
            .description = "the mods the menu lists, in its order: each with its name, its display name and its pages",
            .handler = &runMods,
        });
    }
}
