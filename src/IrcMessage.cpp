// Copyright 2026 @esettes, @danielfdez17

#include <stdexcept>

#include "IrcMessage.hpp"

namespace {
void validateCommand(const std::string &cmd) {
    if (cmd.empty()
        || cmd.find(' ') != std::string::npos
        || cmd.find('\t') != std::string::npos
        || cmd.find('\r') != std::string::npos
        || cmd.find('\n') != std::string::npos
        || cmd.find('\0') != std::string::npos)
        throw std::runtime_error("Invalid command");
}

void validateField(const std::string &field, const std::string &fieldName) {
    if (field.find('\r') != std::string::npos
        || field.find('\n') != std::string::npos
        || field.find('\0') != std::string::npos)
        throw std::runtime_error("Invalid " + fieldName);
}

void validatePrefix(const std::string &prefix) {
    validateField(prefix, "prefix");

    if (prefix.find(' ') != std::string::npos || prefix[0] == ':') {
        throw std::runtime_error("Invalid prefix");
    }
}

void validateMiddleParameter(const std::string &parameter) {
    validateField(parameter, "middle parameter");

    if (parameter.empty()
        || parameter.find(' ') != std::string::npos
        || parameter[0] == ':') {
        throw std::runtime_error("Invalid middle parameter");
    }
}
}  // namespace

IrcMessage::IrcMessage(
        const std::string &msgCmd,
        const std::vector<std::string> &msgParams,
        const std::string &msgPrefix,
        bool messageHasTrailingParameter)
        :   prefix(msgPrefix),
            cmd(msgCmd),
            params(msgParams),
            hasTrailingParameter(messageHasTrailingParameter) {
}

IrcMessage::IrcMessage() : prefix(""),
    cmd(""),
    params(std::vector<std::string>()),
    hasTrailingParameter(false) {}

std::string IrcMessage::serialize() const {
    std::string result;

    validateCommand(cmd);

    if (hasTrailingParameter && params.empty()) {
        throw std::runtime_error(
            "Trailing parameter declared without parameters");
    }

    if (!prefix.empty()) {
        validatePrefix(prefix);
        result += ':';
        result += prefix;
        result += ' ';
    }

    result += cmd;

    for (std::size_t i = 0; i < params.size(); ++i) {
        const std::string &param = params[i];

        validateField(param, "parameter");

        const bool isLastParameter = i + 1 == params.size();
        const bool startsWithColon =
            !param.empty() && param[0] == ':';
        const bool requiresTrailingMarker =
            param.empty()
            || startsWithColon
            || param.find(' ') != std::string::npos;
        const bool serializeAsTrailing =
            isLastParameter
            && (hasTrailingParameter || requiresTrailingMarker);

        if (serializeAsTrailing) {
            validateField(param, "trailing parameter");
            result += " :";
            result += param;
        } else {
            validateMiddleParameter(param);
            result += ' ';
            result += param;
        }
    }

    result += "\r\n";

    if (result.size() > IRC_MAX_MESSAGE_LENGTH)
        throw std::runtime_error("IRC message exceeds 512 bytes");

    return result;
}

const std::string &IrcMessage::getCommand() const {
    return cmd;
}
