#pragma once
#include <string_view>

namespace LV3
{
    enum class LogLevel { Debug, Info, Warning, Error, Success };

    class Logger 
    {
    public:
        static void write(std::string_view msg, LogLevel lvl = LogLevel::Info);
        static void info(std::string_view msg);
        static void debug(std::string_view msg);
        static void success(std::string_view msg);
        static void warn (std::string_view msg);
        static void error(std::string_view msg);
        static void setLevel(LogLevel lvl);
    private:
        static LogLevel s_level;
    };

} // namespace LV3

