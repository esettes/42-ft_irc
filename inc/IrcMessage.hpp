// Copyright 2026 @esettes, @danielfdez17
#ifndef INC_IRCMESSAGE_HPP_
#define INC_IRCMESSAGE_HPP_

#include <string>
#include <vector>
#include <cstddef>

/**
 * IRC messages are limited to 512 bytes including the terminating CR-LF.
 * @see RFC 1459 / RFC 2812
 */
const std::size_t IRC_MAX_MESSAGE_LENGTH = 512;

/**
 * Maximum pending input bytes per client (incomplete fragments + burst).
 * Prevents unbounded growth from abusive senders.
 */
const std::size_t IRC_MAX_INPUT_BUFFER_SIZE = 8192;

/**
 * Maximum pending output bytes per client.
 * Disconnects slow clients that do not drain their socket.
 */
const std::size_t IRC_MAX_OUTPUT_BUFFER_SIZE = 65536;

/**
 * @file IrcMessage.hpp
 * @brief Declares the parsed IRC message representation shared across the server.
 * 
 * @param prefix Message author (:nickname!username@host)
 * @param cmd The command of the message, which can be a standard IRC command or a numeric reply.
 * @param params The parameters of the message, which can include channel names, usernames, and message text.
 */
struct IrcMessage {
    std::string prefix;
    std::string cmd;
    std::vector<std::string> params;
    bool hasTrailingParameter;

    IrcMessage();

    IrcMessage(
        const std::string &msgCmd,
        const std::vector<std::string> &msgParams,
        const std::string &msgPrefix = "",
        bool messageHasTrailingParameter = false);

    const std::string &getCommand() const;
    std::string serialize() const;
};

#endif  // INC_IRCMESSAGE_HPP_
