#ifndef NUMERIC_REPLIES_HPP
# define NUMERIC_REPLIES_HPP

#include <string>
#include <stdexcept>

/**
 * @file NumericReplies.hpp
 * @brief Centralizes IRC numeric reply codes and three-digit formatting.
 */
namespace NumericReply
{
    const int RPL_WELCOME = 1;
    const int RPL_YOURHOST = 2;
    const int RPL_CREATED = 3;
    const int RPL_MYINFO = 4;
    const int RPL_ISUPPORT = 5;
    const int RPL_CHANNELMODEIS = 324;
    const int RPL_NOTOPIC = 331;
    const int RPL_TOPIC = 332;
    const int RPL_INVITING = 341;
    const int RPL_NAMREPLY = 353;
    const int RPL_ENDOFNAMES = 366;
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
    const int ERR_PASSWDMISMATCH = 464;
    const int ERR_CHANNELISFULL = 471;
    const int ERR_UNKNOWNMODE = 472;
    const int ERR_INVITEONLYCHAN = 473;
    const int ERR_BADCHANNELKEY = 475;
    const int ERR_CHANOPRIVSNEEDED = 482;

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
}

#endif
