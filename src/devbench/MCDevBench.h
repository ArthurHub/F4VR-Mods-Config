#pragma once

namespace mods_config::devbench
{
    /**
     * Set up the mod's devbench tool on the framework's f4cf::devbench: its description, the state behind the state
     * action, and the mod's actions (open, close, mods). Call once while the mod loads, before onGameLoaded returns, so
     * the tool is registered with all of it.
     */
    void setupDevBenchTool();
}
