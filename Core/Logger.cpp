#include "pch.h"

#include "Logger.h"
#include <iostream>

namespace LibV3
{
    LogLevel Logger::s_level = LogLevel::Info;

   
	// Définit le niveau de log.
    void Logger::setLevel(LogLevel lvl)
    {
        s_level = lvl;
    }

	// Logue un message si le niveau est supérieur ou égal au niveau courant.
    void Logger::log(std::string_view msg, LogLevel lvl)
    {
        if (lvl >= s_level)
            std::cout << "[LibV2] " << msg << "\n";
    }

	// Logue un message d'avertissement.
    void Logger::warn (std::string_view msg)
    { 
        log(msg, LogLevel::Warning);
    }
    
	// Logue un message d'erreur.
    void Logger::error(std::string_view msg)
    {
        log(msg, LogLevel::Error);
    }

} // namespace LibV3
