#pragma once

#include <span>
#include <string>
#include <vector>

#include "Drawing.h"
#include "PageImages.h"
#include "model/ConfigDefinition.h"
#include "model/Store.h"
#include "vrcf/VRControllersManager.h"

namespace mods_config::ui
{
    bool onRightController(vrcf::Hand hand, bool leftHanded);
    std::vector<std::string> bindingIcons(const vrcf::InputBinding& binding, bool leftHanded);
    bool samePress(const vrcf::InputBinding& binding, const vrcf::InputBinding& other, bool leftHanded);
    std::string bindingValueName(const model::Value& value, bool leftHanded);
    std::string samePressHelp(const model::Setting& setting, std::span<const model::Setting* const> bindings, model::Store& store, bool leftHanded);
    float drawBindingIcons(const vrcf::InputBinding& binding, float left, float middle, float scale, ImU32 color, bool leftHanded, const ImageDrawer& drawer);
    float drawBinding(float right, float top, const std::string* value, ValueLook look, bool open, bool leftHanded, const ImageDrawer& drawer);
}
