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
    std::vector<std::string> bindingIcons(const vrcf::InputBinding& binding, bool leftHanded);
    bool samePress(const vrcf::InputBinding& binding, const vrcf::InputBinding& other, bool leftHanded);
    std::string bindingValueName(const model::Value& value, bool leftHanded);
    std::string samePressHelp(const model::Setting& setting, std::span<const model::Setting* const> bindings, model::Store& store, bool leftHanded);
    float drawBinding(float right, float top, const std::string* value, ValueLook look, bool leftHanded, const ImageDrawer& drawer);
}
