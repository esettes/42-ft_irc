#ifndef CONSOLE_HPP
# define CONSOLE_HPP

/**
 * @file Console.hpp
 * @brief PODRIA QUEDAR DEFINIDO EN EL PROPIO .hpp
 * @brief ¿POR QUÉ UTILIZAMOS const char[] EN LUGAR DE std::string PARA LOS CÓDIGOS DE FORMATEO DE CONSOLA?
 * @brief Declares console color and formatting constants used for server logging.
 * 
 * @param RESET ANSI escape code to reset console formatting.
 * @param SERVER ANSI escape code for server log messages (cyan).
 * @param CLIENT ANSI escape code for client log messages (green).
 * @param WARNING ANSI escape code for warning log messages (yellow).
 * @param ERROR ANSI escape code for error log messages (red).
 */
namespace Console
{
    extern const char RESET[];
    extern const char SERVER[];
    extern const char CLIENT[];
    extern const char WARNING[];
    extern const char ERROR[];
}

#endif
