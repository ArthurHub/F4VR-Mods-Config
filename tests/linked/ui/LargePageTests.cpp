#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <format>
#include <map>
#include <optional>
#include <string>

#include "TestClients.h"
#include "TestPanel.h"
#include "spec/ConfigSpecLoader.h"
#include "spec/ConfigSpecStore.h"
#include "ui/FullPanel.h"
#include "ui/RowView.h"

using namespace mods_config;
using test_panel::POINTER_AWAY;

namespace
{
    constexpr auto WHEEL = R"({
        "minSchemaVersion": 1, "modName": "PalmLike", "displayName": "Wheel menu", "applies": "live",
        "pages": [ { "id": "items", "pageDisplayName": "Items", "contentFromClientCallback": true } ]
    })";

    /**
     * The rows a client gives: one section of `count` switchers, the first `on` of them on, with a `maxOn` of 8,
     * or none for 0.
     */
    std::string itemsJson(const int count, const int maxOn)
    {
        std::string rows;
        for (int i = 0; i < count; ++i) {
            rows += std::format(R"({}{{ "type": "switcher", "text": "Item {}", "default": false, "storage": {{ "clientState": "item.{}" }} }})", i ? "," : "", i, i);
        }
        const auto limit = maxOn > 0 ? std::format(R"("maxOn": {},)", maxOn) : std::string();
        return std::format(R"([ {{ "type": "section", "text": "Aid", {} "content": [ {} ] }} ])", limit, rows);
    }

    /**
     * The mean time of a frame of the panel on the page of `count` switchers, `on` of them on.
     */
    double frameMicroseconds(const int count, const int on, const int maxOn, const int frames)
    {
        const auto mod = spec::loadConfigSpec(WHEEL);
        REQUIRE(mod);
        test_clients::TestClient client{ .pages = { { "items", [count, maxOn] {
                                                         return itemsJson(count, maxOn);
                                                     } } } };
        auto calls = client.calls();
        calls.state = [on](const std::string_view name) -> std::optional<model::Value> {
            return std::stoi(std::string(name.substr(5))) < on;
        };
        spec::ConfigSpecStore store({}, {});
        store.clientCalls().addClient("PalmLike", calls);
        const test_panel::PanelContext context;
        ui::FullPanel panel;
        panel.setStore(model::Source::ConfigSpec, &store);
        panel.setMods({ *mod });
        panel.select({ .mod = 0, .page = { 0 } });
        test_panel::drawFrames(panel, 5);
        const auto start = std::chrono::steady_clock::now();
        test_panel::drawFrames(panel, frames);
        const auto elapsed = std::chrono::steady_clock::now() - start;
        return std::chrono::duration<double, std::micro>(elapsed).count() / frames;
    }
}

TEST_CASE("Large page: a section of 500 switchers with a maxOn draws about as fast as one with none")
{
    // the fastest of a few runs, which a busy machine slows the least; a section's limit grew with the square of
    // its size when every row counted its switchers, about 14 times the time of one with none at 500
    const auto fastest = [](const int maxOn) {
        double best = frameMicroseconds(500, 4, maxOn, 20);
        for (int run = 0; run < 2; ++run) {
            best = (std::min)(best, frameMicroseconds(500, 4, maxOn, 20));
        }
        return best;
    };
    const auto limited = fastest(8);
    const auto plain = fastest(0);
    INFO(std::format("{:.0f} us a frame with maxOn 8, {:.0f} us with none", limited, plain));
    CHECK(limited < plain * 2.0);
}

TEST_CASE("Large page: the time of a frame of a page of many switchers, with and without a maxOn", "[.bench]")
{
    for (const int count : { 20, 100, 250, 500 }) {
        const auto limited = frameMicroseconds(count, 4, 8, 60);
        const auto plain = frameMicroseconds(count, 4, 0, 60);
        WARN(std::format("{} switchers: {:.0f} us a frame with maxOn 8, {:.0f} us with none", count, limited, plain));
    }
}
