#include "pch.h"

#include "Logger.h"
#include <iostream>

namespace LV3
{
    LogLevel Logger::s_level = LogLevel::Info;
   
	// Définit le niveau de log.
    void Logger::setLevel(LogLevel lvl)
    {
        s_level = lvl;
    }

    // Logue un message si le niveau est supérieur ou égal au niveau courant.
    void Logger::write(std::string_view msg, LogLevel lvl)
    {
        if (lvl >= s_level)
        switch (lvl)
        {
        case LV3::LogLevel::Debug:
            std::cout << "\n\033[34m=[LibV3] " << msg << "\033[0m";   // Affichage en bleu
            break;
        case LV3::LogLevel::Info:
            std::cout << "\n\033[37m=[LibV3] " << msg << "\033[0m";   // Affichage en blanc
            break;        
        case LV3::LogLevel::Success:    
            std::cout << "\n\033[32m=[LibV3] " << msg << "\033[0m";   // Affichage en vert
            break;
        case LV3::LogLevel::Warning:
            std::cout << "\n\033[33m=[LibV3] " << msg << "\033[0m";   // Affichage en jaune
            break;
        case LV3::LogLevel::Error:
            std::cout << "\n\033[31m=[LibV3] " << msg << "\033[0m";   // Affichage en vert
            break;
        default:
            break;
        }

        //if (lvl >= s_level)
        //    std::cout << "[LibV3] " << msg << "\n";
    }

	// Logue un message info
    void Logger::info(std::string_view msg)
    {
        write(msg, LogLevel::Info);
    }
    // Logue un message info
    void Logger::success(std::string_view msg)
    {
        write(msg, LogLevel::Success);
    }
    // Logue un message info
    void Logger::debug(std::string_view msg)
    {
        write(msg, LogLevel::Debug);
    }
	// Logue un message d'avertissement.
    void Logger::warn (std::string_view msg)
    { 
        write(msg, LogLevel::Warning);
    }
    
	// Logue un message d'erreur.
    void Logger::error(std::string_view msg)
    {
        write(msg, LogLevel::Error);
    }

} // namespace LV3
