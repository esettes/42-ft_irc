#include "MessageParser.hpp"

#include <cctype>
#include <stdexcept>

namespace
{
    std::string toUpperAscii(const std::string &value)
    {
        std::string normalized = value;

        for (std::size_t i = 0; i < normalized.size(); ++i)
        {
            normalized[i] = static_cast<char>(
                std::toupper(static_cast<unsigned char>(normalized[i]))
            );
        }

        return normalized;
    }

    bool isIrcLineSafe(const std::string &line)
    {
        for (std::size_t i = 0; i < line.size(); ++i)
        {
            if (line[i] == '\0' || line[i] == '\r' || line[i] == '\n')
                return false;
        }

        return true;
    }
}

IrcMessage MessageParser::parse(const std::string &line)
{
    if (!isIrcLineSafe(line))
        throw std::invalid_argument("invalid IRC line characters");

    const std::string::size_type lineLength = line.size();
    std::string::size_type cursor = 0;

    while (cursor < lineLength && line[cursor] == ' ')
        ++cursor;

    if (cursor == lineLength)
        throw std::invalid_argument("empty IRC line");

    std::string prefix;
    if (line[cursor] == ':')
    {
        ++cursor;

        const std::string::size_type prefixEnd = line.find(' ', cursor);
        if (prefixEnd == std::string::npos)
            throw std::invalid_argument("missing command after prefix");

        prefix = line.substr(cursor, prefixEnd - cursor);
        cursor = prefixEnd;

        while (cursor < lineLength && line[cursor] == ' ')
            ++cursor;

        if (cursor == lineLength)
            throw std::invalid_argument("missing command after prefix");
    }

    const std::string::size_type commandStart = cursor;
    while (cursor < lineLength && line[cursor] != ' ')
        ++cursor;

    if (cursor == commandStart)
        throw std::invalid_argument("missing IRC command");

    const std::string command = toUpperAscii(
        line.substr(commandStart, cursor - commandStart)
    );

    std::vector<std::string> params;
    while (cursor < lineLength)
    {
        while (cursor < lineLength && line[cursor] == ' ')
            ++cursor;

        if (cursor == lineLength)
            break;

        if (line[cursor] == ':')
        {
            params.push_back(line.substr(cursor + 1));
            break;
        }

        const std::string::size_type paramStart = cursor;
        while (cursor < lineLength && line[cursor] != ' ')
            ++cursor;

        params.push_back(line.substr(paramStart, cursor - paramStart));
    }

    return IrcMessage(command, params, prefix);
}
