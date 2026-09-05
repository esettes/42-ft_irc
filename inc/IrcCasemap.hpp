// Copyright 2026 @esettes, @danielfdez17
#ifndef INC_IRCCASEMAP_HPP_
#define INC_IRCCASEMAP_HPP_

#include <string>

/**
 * @file IrcCasemap.hpp
 * @brief RFC 1459 casemapping helpers for IRC nicknames and channel names.
 *
 * Original casing is preserved for display; normalized forms are used as map keys
 * and for equality checks. Under rfc1459, {}| are the lowercase equivalents of []\.
 */
namespace IrcCasemap {
std::string normalize(const std::string &value);
bool equal(const std::string &left, const std::string &right);
}  // namespace IrcCasemap

#endif  // INC_IRCCASEMAP_HPP_
