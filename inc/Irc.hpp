// Copyright 2026 @esettes, @danielfdez17
#ifndef INC_IRC_HPP_
#define INC_IRC_HPP_

#include <stdint.h>
#include <string>

namespace Constants {

const uint16_t MAX_PORT = 65535;
const uint16_t MIN_PORT = 1;

const std::string INVALID_PORT_MSG = "Invalid port";
const std::string INVALID_PORT_RANGE_MSG = "Port must be between 1 and 65535";
const std::string INVALID_PORT_CONTENT_MSG = "Port must contain only digits";
const std::string INVALID_PORT_EMPTY_MSG = "Port cannot be empty";

const std::string PASS_CMD = "PASS";
const std::string NICK_CMD = "NICK";
const std::string USER_CMD = "USER";
const std::string PING_CMD = "PING";
const std::string PONG_CMD = "PONG";
const std::string QUIT_CMD = "QUIT";
const std::string CAP_CMD = "CAP";
const std::string JOIN_CMD = "JOIN";
const std::string PART_CMD = "PART";
const std::string PRIVMSG_CMD = "PRIVMSG";
const std::string NOTICE_CMD = "NOTICE";
const std::string TOPIC_CMD = "TOPIC";
const std::string INVITE_CMD = "INVITE";
const std::string KICK_CMD = "KICK";
const std::string MODE_CMD = "MODE";
const std::string END_SUB_CMD = "END";
const std::string LS_SUB_CMD = "LS";
const std::string REQ_SUB_CMD = "REQ";
const std::string NAK_SUB_CMD = "NAK";
const std::string ADD_FLAG = "+";
const std::string REMOVE_FLAG = "-";

const char INVITE_MODE = 'i';
const char TOPIC_MODE = 't';
const char KEY_MODE = 'k';
const char OPERATOR_MODE = 'o';
const char LIMIT_MODE = 'l';
const char ADD_FLAG_MODE = '+';
const char REMOVE_FLAG_MODE = '-';

const int INVALID_FD = -1;
const int POLL_TIMEOUT_MS = 1000;
const std::size_t RECEIVE_BUFFER_SIZE = 4096;
const std::size_t MAX_SEND_SIZE = 16384;
const std::size_t MAX_CHANNEL_NAME_LENGTH = 50;

const std::string DEFAULT_SERVER_NAME = "irc.42.local";
const std::string INVALID_SERVER_PORT_MSG = "invalid server port";
const std::string EMPTY_PASSWORD_MSG = "password cannot be empty";

const std::string BOT_VERSION = "ft_irc bot 1.0";
const std::string BOT_VERSION_CXX98 = "ft_irc bot 1.0 (C++98)";
const std::string BOT_HELP = "Commands: help, ping, time, date, info, uptime, "
"version, users, whoami, echo <text>, dice. "
"Use /msg marvin <cmd> or talk in #bot with !cmd";
const std::string BOT_HOME_GREETING = "Hello! Type !help to see my commands.";
const std::string BOT_ECHO_USAGE = "Usage: echo <text>";

const std::string MSG_CREATED = "This server was created today";
const std::string MSG_ISUPPORT = "are supported by this server";
const std::string MSG_NOTOPIC = "No topic is set";
const std::string MSG_ENDOFNAMES = "End of /NAMES list";
const std::string MSG_NOSUCHNICK = "No such nick";
const std::string MSG_NOSUCHCHANNEL = "No such channel";
const std::string MSG_CANNOTSENDTOCHAN = "Cannot send to channel";
const std::string MSG_NOORIGIN = "No origin specified";
const std::string MSG_NOTEXTTOSEND = "No text to send";
const std::string MSG_UNKNOWNCOMMAND = "Unknown command";
const std::string MSG_NONICKNAMEGIVEN = "No nickname given";
const std::string MSG_ERRONEUSNICKNAME = "Erroneous nickname";
const std::string MSG_NICKNAMEINUSE = "Nickname is already in use";
const std::string MSG_USERNOTINCHANNEL = "They aren't on that channel";
const std::string MSG_NOTONCHANNEL = "You're not on that channel";
const std::string MSG_USERONCHANNEL = "is already on channel";
const std::string MSG_NOTREGISTERED = "You have not registered";
const std::string MSG_NEEDMOREPARAMS = "Not enough parameters";
const std::string MSG_ALREADYREGISTRED = "You may not reregister";
const std::string MSG_PASSWDMISMATCH = "Password incorrect";
const std::string MSG_CHANNELISFULL = "Cannot join channel (+l)";
const std::string MSG_UNKNOWNMODE = "is unknown mode char to me";
const std::string MSG_INVITEONLYCHAN = "Cannot join channel (+i)";
const std::string MSG_BADCHANNELKEY = "Cannot join channel (+k)";
const std::string MSG_CHANOPRIVSNEEDED = "You're not channel operator";

const std::string ISUPPORT_CHANTYPES = "CHANTYPES=#";
const std::string ISUPPORT_PREFIX = "PREFIX=(o)@";
const std::string ISUPPORT_CHANMODES = "CHANMODES=,k,l,it";
const std::string ISUPPORT_CASEMAPPING = "CASEMAPPING=rfc1459";
const std::string SERVER_VERSION = "1.0";
const std::string AVAILABLE_USER_MODES = "o";
const std::string AVAILABLE_CHANNEL_MODES = "itkol";

}  // namespace Constants

namespace BotConstants {

const std::string PING = "ping";
const std::string PONG = "pong";
const std::string TIME = "time";
const std::string VERSION = "version";
const std::string HELP = "help";
const std::string DATE = "date";
const std::string INFO = "info";
const std::string UPTIME = "uptime";
const std::string USERS = "users";
const std::string WHOAMI = "whoami";
const std::string ECHO = "echo";
const std::string DICE = "dice";
const std::string ROLL = "roll";
const std::string UNKNOWN = "unknown";

}  // namespace BotConstants

#endif  // INC_IRC_HPP_
