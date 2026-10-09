#include <spdlog/sinks/stdout_sinks.h>

// What the tests have in place of the game and of the mod's plugin load.

namespace REL
{
    // No game in the process: CommonLibF4 resolves every game address into a range that cannot be used, so the
    // framework's addresses resolve when the executable starts, and a test that uses one fails on it.
    const bool NO_GAME = true;
}

namespace
{
    /**
     * The framework's logger, which the mod sets up when its plugin loads. Here it writes to the output, which
     * ctest shows for a test that failed.
     */
    const struct Logger
    {
        Logger()
        {
            const auto sink = std::make_shared<spdlog::sinks::stdout_sink_mt>();
            logger::internal::_logger = std::make_shared<spdlog::logger>("GLOBAL", sink);
            logger::internal::_rawLogger = std::make_shared<spdlog::logger>(std::string(logger::internal::RAW_LOGGER_NAME), sink);
        }
    } LOGGER;
}
