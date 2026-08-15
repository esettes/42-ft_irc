#include "IrcCasemap.hpp"

namespace
{
    char toRfc1459Lower(char character)
    {
        const unsigned char value = static_cast<unsigned char>(character);

        if (value >= 'A' && value <= 'Z')
            return static_cast<char>(value - 'A' + 'a');
        if (value == '[')
            return '{';
        if (value == ']')
            return '}';
        if (value == '\\')
            return '|';
        if (value == '~')
            return '^';
        if (value == '^')
            return '^';
        return static_cast<char>(value);
    }
}

std::string IrcCasemap::normalize(const std::string &value)
{
    std::string normalized = value;

    for (std::size_t i = 0; i < normalized.size(); ++i)
        normalized[i] = toRfc1459Lower(normalized[i]);

    return normalized;
}

bool IrcCasemap::equal(const std::string &left, const std::string &right)
{
    return normalize(left) == normalize(right);
}
