#ifndef NUMERIC_REPLIES_HPP
# define NUMERIC_REPLIES_HPP

#include <string>
#include <stdexcept>

/**
 * @file NumericReplies.hpp
 * @brief Centralizes IRC numeric reply codes, three-digit formatting and
 *        documented trailing messages used by the server.
 */
namespace NumericReply
{
    /* Welcome / server info */
    const int RPL_WELCOME = 1;
    const int RPL_YOURHOST = 2;
    const int RPL_CREATED = 3;
    const int RPL_MYINFO = 4;
    const int RPL_ISUPPORT = 5;

    /* Channel replies */
    const int RPL_CHANNELMODEIS = 324;
    const int RPL_NOTOPIC = 331;
    const int RPL_TOPIC = 332;
    const int RPL_INVITING = 341;
    const int RPL_NAMREPLY = 353;
    const int RPL_ENDOFNAMES = 366;

    /* Errors */
    const int ERR_NOSUCHNICK = 401;
    const int ERR_NOSUCHCHANNEL = 403;
    const int ERR_CANNOTSENDTOCHAN = 404;
    const int ERR_NOORIGIN = 409;
    const int ERR_NORECIPIENT = 411;
    const int ERR_NOTEXTTOSEND = 412;
    const int ERR_UNKNOWNCOMMAND = 421;
    const int ERR_NONICKNAMEGIVEN = 431;
    const int ERR_ERRONEUSNICKNAME = 432;
    const int ERR_NICKNAMEINUSE = 433;
    const int ERR_USERNOTINCHANNEL = 441;
    const int ERR_NOTONCHANNEL = 442;
    const int ERR_USERONCHANNEL = 443;
    const int ERR_NOTREGISTERED = 451;
    const int ERR_NEEDMOREPARAMS = 461;
    const int ERR_ALREADYREGISTRED = 462;
    const int ERR_ALREADYREGISTERED = ERR_ALREADYREGISTRED;
    const int ERR_PASSWDMISMATCH = 464;
    const int ERR_CHANNELISFULL = 471;
    const int ERR_UNKNOWNMODE = 472;
    const int ERR_INVITEONLYCHAN = 473;
    const int ERR_BADCHANNELKEY = 475;
    const int ERR_CHANOPRIVSNEEDED = 482;

    /* Documented trailing messages */
    const char *const MSG_CREATED = "This server was created today";
    const char *const MSG_ISUPPORT = "are supported by this server";
    const char *const MSG_NOTOPIC = "No topic is set";
    const char *const MSG_ENDOFNAMES = "End of /NAMES list";
    const char *const MSG_NOSUCHNICK = "No such nick";
    const char *const MSG_NOSUCHCHANNEL = "No such channel";
    const char *const MSG_CANNOTSENDTOCHAN = "Cannot send to channel";
    const char *const MSG_NOORIGIN = "No origin specified";
    const char *const MSG_NOTEXTTOSEND = "No text to send";
    const char *const MSG_UNKNOWNCOMMAND = "Unknown command";
    const char *const MSG_NONICKNAMEGIVEN = "No nickname given";
    const char *const MSG_ERRONEUSNICKNAME = "Erroneous nickname";
    const char *const MSG_NICKNAMEINUSE = "Nickname is already in use";
    const char *const MSG_USERNOTINCHANNEL = "They aren't on that channel";
    const char *const MSG_NOTONCHANNEL = "You're not on that channel";
    const char *const MSG_USERONCHANNEL = "is already on channel";
    const char *const MSG_NOTREGISTERED = "You have not registered";
    const char *const MSG_NEEDMOREPARAMS = "Not enough parameters";
    const char *const MSG_ALREADYREGISTRED = "You may not reregister";
    const char *const MSG_PASSWDMISMATCH = "Password incorrect";
    const char *const MSG_CHANNELISFULL = "Cannot join channel (+l)";
    const char *const MSG_UNKNOWNMODE = "is unknown mode char to me";
    const char *const MSG_INVITEONLYCHAN = "Cannot join channel (+i)";
    const char *const MSG_BADCHANNELKEY = "Cannot join channel (+k)";
    const char *const MSG_CHANOPRIVSNEEDED = "You're not channel operator";

    /* ISUPPORT tokens matching the modes this server will expose */
    const char *const ISUPPORT_CHANTYPES = "CHANTYPES=#";
    const char *const ISUPPORT_PREFIX = "PREFIX=(o)@";
    const char *const ISUPPORT_CHANMODES = "CHANMODES=,k,l,it";
    const char *const ISUPPORT_CASEMAPPING = "CASEMAPPING=rfc1459";

    /* Version / mode tokens for RPL_MYINFO */
    const char *const SERVER_VERSION = "1.0";
    const char *const AVAILABLE_USER_MODES = "o";
    const char *const AVAILABLE_CHANNEL_MODES = "itk";

    inline std::string formatCode(int numericCode)
    {
        if (numericCode < 0 || numericCode > 999)
            throw std::runtime_error("Invalid numeric reply code");

        std::string code(3, '0');
        code[0] = static_cast<char>('0' + (numericCode / 100));
        code[1] = static_cast<char>('0' + ((numericCode / 10) % 10));
        code[2] = static_cast<char>('0' + (numericCode % 10));
        return code;
    }

    inline std::string noRecipientMessage(const std::string &command)
    {
        return "No recipient given (" + command + ")";
    }

    inline std::string yourHostMessage(const std::string &serverName)
    {
        return "Your host is " + serverName;
    }

    inline std::string welcomeMessage(const std::string &clientIdentity)
    {
        return "Welcome to the IRC Network " + clientIdentity;
    }
}

#endif
