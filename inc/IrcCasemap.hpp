#ifndef IRC_CASEMAP_HPP
# define IRC_CASEMAP_HPP

#include <string>

/**
 * @file IrcCasemap.hpp
 * @brief RFC 1459 casemapping helpers for IRC nicknames and channel names.
 *
 * Original casing is preserved for display; normalized forms are used as map keys
 * and for equality checks. Under rfc1459, {}| are the lowercase equivalents of []\.
 */
namespace IrcCasemap
{
    std::string normalize(const std::string &value);
    bool equal(const std::string &left, const std::string &right);
}

#endif
