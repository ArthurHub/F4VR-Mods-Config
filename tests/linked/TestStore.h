#pragma once

#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "model/Store.h"

// A store double for the panel's tests, which records the calls the panel makes of it.
namespace test_store
{
    using namespace mods_config;
    using Calls = std::vector<std::string>;

    /**
     * A store that records each lifecycle call it gets, with Save and Discard, in order: the mod, or the page's name
     * ("front" for a mod's front page, "none" for no page), or whether a row is pointed. Its changes are a count the
     * test sets, which Save and Discard clear. A setting's value by its id, which conditions ask, is the one the test
     * sets, and none by default. It writes a change at once or keeps it for a Save, and says why an action does not
     * run, as the test sets. A test's own store derives from it for the values of its settings.
     */
    class RecordingStore : public model::Store
    {
    public:
        Calls calls;
        std::size_t dirty = 0;
        model::StoredValue byId;
        bool atOnce = false;
        std::string notRunnableReason;

        bool writesChangesAtOnce() const override
        {
            return atOnce;
        }

        std::string notRunnable(const model::Action&) const override
        {
            return notRunnableReason;
        }

        void open(const model::Mod& mod) override
        {
            calls.push_back("open " + mod.name);
        }

        void close() override
        {
            calls.emplace_back("close");
        }

        void pageShown() override
        {
            calls.emplace_back("pageShown");
        }

        void readTexts(model::Mod& mod) override
        {
            calls.push_back("readTexts " + mod.name);
        }

        void showPage(const model::Page* page) override
        {
            calls.push_back("showPage " + (!page ? std::string("none") : page->name.empty() ? std::string("front") : page->name));
        }

        void showPointed(const model::Item* item) override
        {
            calls.push_back(item ? "showPointed row" : "showPointed none");
        }

        void startFrame() override
        {
            calls.emplace_back("startFrame");
        }

        model::StoredValue value(const model::Setting&) override
        {
            return {};
        }

        model::StoredValue valueById(std::string_view) override
        {
            return byId;
        }

        bool set(const model::Setting&, const model::Value&) override
        {
            return false;
        }

        std::optional<model::Value> defaultValue(const model::Setting&) override
        {
            return std::nullopt;
        }

        const std::vector<model::Option>& options(const model::Setting& setting) const override
        {
            return setting.valueOptions.options;
        }

        bool isDirty(const model::Setting&) const override
        {
            return false;
        }

        std::size_t dirtyCount() const override
        {
            return dirty;
        }

        std::size_t invalidCount() const override
        {
            return 0;
        }

        std::optional<model::Applies> applies(const model::Setting&) const override
        {
            return std::nullopt;
        }

        std::size_t dirtyRestartCount() const override
        {
            return 0;
        }

        std::optional<std::size_t> lastSave() const override
        {
            return std::nullopt;
        }

        std::size_t savedRestartCount(const model::Mod&) const override
        {
            return 0;
        }

        std::expected<void, std::string> save() override
        {
            calls.emplace_back("save");
            dirty = 0;
            return {};
        }

        std::string_view saveFailure() const override
        {
            return {};
        }

        void discard() override
        {
            calls.emplace_back("discard");
            dirty = 0;
        }

        void discard(const model::Setting&) override
        {}

        std::optional<model::Value> savedValue(const model::Setting&) override
        {
            return std::nullopt;
        }

        model::StoredValue hotkey(const model::Hotkey&) override
        {
            return {};
        }

        model::ActionResult run(const model::Action&) override
        {
            return {};
        }
    };
}
