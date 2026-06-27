#pragma once
#include <string_view>

namespace LibV3
{
    enum class LogLevel { Debug, Info, Warning, Error };

    class Logger 
    {
    public:
        static void log  (std::string_view msg, LogLevel lvl = LogLevel::Info);
        static void warn (std::string_view msg);
        static void error(std::string_view msg);
        static void setLevel(LogLevel lvl);
    private:
        static LogLevel s_level;
    };

} // namespace LibV3

