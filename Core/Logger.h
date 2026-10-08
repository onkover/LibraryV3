#pragma once
#include <string_view>

namespace LV3
{
    enum class LogLevel { Debug, Info, Warning, Error, Success };

    class Logger 
    {
    public:
        static void write(std::string_view msg, LogLevel lvl = LogLevel::Info);
        static void write();
        static void info(std::string_view msg);
        static void debug(std::string_view msg);
        static void success(std::string_view msg);
        static void warn (std::string_view msg);
        static void error(std::string_view msg);
        static void setLevel(LogLevel lvl);
        static void newline();

        // TNR : avertissements emis depuis le lancement, AFFICHES OU NON.
        // Un silence ne se constate pas a l'oeil : il se compte.
        [[nodiscard]] static std::uint32_t warnCount() noexcept { return s_warnCount; }
    private:
        static LogLevel s_level;
        static std::uint32_t s_warnCount;
    };

} // namespace LV3

