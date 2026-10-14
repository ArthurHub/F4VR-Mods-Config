#include "devbench/MCDevBench.h"

#include <algorithm>
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

            // what the panel shows, each as its place in the mods and in the mod's pages: -1 for none
            int selectedMod = -1;
            int selectedPage = -1;
        };

        const char* stateDescription()
        {
            return "menu: open, minimized (the panel is hidden and only the offhand buttons show); "
                   "pointer: onPanel (a wand's ray is on the panel), hand (primary or offhand), "
                   "x and y (where, in the panel's pixels from its top left), down (it presses the panel), wanted (ImGui uses the pointer); "
                   "mods: count (the mods the menu lists; the mods action has each of them); "
                   "selection: mod and page (what the panel shows, each as its place from 0 in the mods action's answer; "
                   "a null mod is the menu's front page, and a null page the mod's front page); "
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
            const auto& selection = menu.selection();
            state.selectedMod = selection.mod ? static_cast<int>(*selection.mod) : -1;
            state.selectedPage = selection.page ? static_cast<int>(*selection.page) : -1;
        }

        /**
         * A place in a list, which is null for none.
         */
        json placeToJson(const int place)
        {
            return place < 0 ? json(nullptr) : json(place);
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
                { "selection", { { "mod", placeToJson(state.selectedMod) }, { "page", placeToJson(state.selectedPage) } } },
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
         * One step back in the open menu, as the Back button does: the answer has what the panel shows then,
         * and whether the menu is still open.
         */
        json runBack(const json&)
        {
            auto& menu = g_modsConfig.menu();
            if (!menu.isOpen()) {
                throw std::runtime_error("the menu is not open");
            }
            menu.back();
            const auto& selection = menu.selection();
            return { { "open", menu.isOpen() },
                { "mod", selection.mod ? json(*selection.mod) : json(nullptr) },
                { "page", selection.page ? json(*selection.page) : json(nullptr) } };
        }

        /**
         * The mods the menu lists, in its order: each with its names, the names of its pages, and why it is not
         * available, which is null for one that is.
         */
        json runMods(const json&)
        {
            auto mods = json::array();
            for (const auto& mod : g_modsConfig.menu().mods()) {
                auto pages = json::array();
                for (const auto& page : mod.pages) {
                    pages.push_back(page.name);
                }
                mods.push_back({ { "name", mod.name },
                    { "displayName", mod.displayName },
                    { "pages", std::move(pages) },
                    { "notAvailable", mod.notAvailable.empty() ? json(nullptr) : json(mod.notAvailable) } });
            }
            return { { "mods", std::move(mods) } };
        }

        /**
         * Shows a mod's front page or one of its pages, as a press on its row does: the mod by its name, and
         * the page by its name. With no mod it is the menu's front page. A mod that is not available is not
         * shown, as its row is not pressed.
         */
        json runSelect(const json& args)
        {
            auto& menu = g_modsConfig.menu();
            if (!menu.isOpen()) {
                throw std::runtime_error("the menu is not open");
            }

            ui::Selection selection;
            if (const auto modName = args.value("mod", std::string()); !modName.empty()) {
                const auto& mods = menu.mods();
                const auto mod = std::ranges::find(mods, modName, &model::Mod::name);
                if (mod == mods.end()) {
                    throw std::runtime_error("no mod is named '" + modName + "'");
                }
                if (!mod->notAvailable.empty()) {
                    throw std::runtime_error("the mod is not available: " + mod->notAvailable);
                }
                selection.mod = static_cast<std::size_t>(mod - mods.begin());

                if (const auto pageName = args.value("page", std::string()); !pageName.empty()) {
                    const auto page = std::ranges::find(mod->pages, pageName, &model::Page::name);
                    if (page == mod->pages.end()) {
                        throw std::runtime_error("the mod has no page named '" + pageName + "'");
                    }
                    selection.page = static_cast<std::size_t>(page - mod->pages.begin());
                }
            }
            menu.select(selection);
            return { { "mod", selection.mod ? json(*selection.mod) : json(nullptr) }, { "page", selection.page ? json(*selection.page) : json(nullptr) } };
        }
    }

    void setupDevBenchTool()
    {
        f4cf::devbench::setToolDescription(
            "Mods Config VR: the in-game configuration menu for mods - open and close the menu, read its state and the mods it lists, show a mod or one of its pages, go back");

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
            .name = "back",
            .description = "one step back in the open menu, as the Back button: a page to its mod's front page, that to the menu's front page, and from there the menu closes",
            .handler = &runBack,
        });
        f4cf::devbench::addAction({
            .name = "mods",
            .description = "the mods the menu lists, in its order: each with its name, its display name, its pages, and why it is not available (null for one that is)",
            .handler = &runMods,
        });
        f4cf::devbench::addAction({
            .name = "select",
            .description = "show a mod's front page or one of its pages in the open menu, or the menu's front page with no mod",
            .arguments = { { "mod", { { "type", "string" }, { "description", "select: the mod's name, as the mods action has it; none for the menu's front page" } } },
                { "page", { { "type", "string" }, { "description", "select: the name of a page of the mod, as the mods action has it; none for the mod's front page" } } } },
            .handler = &runSelect,
        });
    }
}
