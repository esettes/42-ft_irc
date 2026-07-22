#include "CommandDispatcher.hpp"
#include "Server.hpp"
#include "Client.hpp"
#include "IrcMessage.hpp"

CommandDispatcher::CommandDefinition::CommandDefinition(
    CommandHandler handler,
    std::size_t minParams,
    bool requiresRegistration
) : handler(handler), minParams(minParams), requiresRegistration(requiresRegistration)
{
}

CommandDispatcher::CommandDispatcher(Server &server) : server(server)
{
    registerCommands();
}

void CommandDispatcher::registerCommands()
{
    cmmds.insert(
        std::make_pair(
            "PASS",
            CommandDefinition(&CommandDispatcher::handlePass, 1, false))
    );
    cmmds.insert(
        std::make_pair(
            "NICK",
            CommandDefinition(&CommandDispatcher::handleNick, 1, false))
    );
    cmmds.insert(
        std::make_pair(
            "USER",
            CommandDefinition(&CommandDispatcher::handleUser, 4, false))
    );
    cmmds.insert(
        std::make_pair(
            "JOIN",
            CommandDefinition(&CommandDispatcher::handleJoin, 1, true))
    );
    cmmds.insert(
        std::make_pair(
            "PRIVMSG",
            CommandDefinition(&CommandDispatcher::handlePrivateMessage, 2, true))
    );
}

void CommandDispatcher::execute(Client &client, const IrcMessage &message)
{
    CommandMap::iterator it;

    it = cmmds.find(message.getCommand());
    if (it != cmmds.end())
    {
        //server.dhandleUnknownCommand(client, message);
        return ;
    }

    const CommandDefinition &definition = it->second;
    if (message.params.size() < definition.minParams)
    {
        // server.handleNotEnoughParams(client, message);
        return ;
    }

    if (definition.requiresRegistration && !client.isRegistered())
    {
        // server.handleNotRegistered(client, message);
        return ;
    }

    (this->*(definition.handler))(client, message);
}