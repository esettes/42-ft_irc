// Copyright 2026 @esettes, @danielfdez17
#include "Channel.hpp"
#include "Client.hpp"

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

static int g_failures = 0;

static void reportFailure(
    const std::string &message,
    const std::string &expected,
    const std::string &actual
)
{
    std::cerr << "FAIL: " << message << std::endl;
    std::cerr << "  expected: " << expected << std::endl;
    std::cerr << "  actual  : " << actual << std::endl;
    ++g_failures;
}

static std::string sizeToString(std::size_t value)
{
    std::ostringstream stream;

    stream << value;
    return stream.str();
}

static void expectTrue(
    bool condition,
    const std::string &message
)
{
    if (!condition)
        reportFailure(message, "true", "false");
}

static void expectFalse(
    bool condition,
    const std::string &message
)
{
    if (condition)
        reportFailure(message, "false", "true");
}

static void expectEqual(
    const std::string &actual,
    const std::string &expected,
    const std::string &message
)
{
    if (actual != expected)
        reportFailure(message, expected, actual);
}

static void expectSizeEqual(
    std::size_t actual,
    std::size_t expected,
    const std::string &message
)
{
    if (actual != expected)
    {
        reportFailure(
            message,
            sizeToString(expected),
            sizeToString(actual)
        );
    }
}

/**
 * @brief Verifies the complete initial state of a newly created channel.
 */
static void testInitialChannelState()
{
    Channel channel("#general");

    expectEqual(
        channel.getName(),
        "#general",
        "A channel should preserve the name supplied to its constructor"
    );

    expectEqual(
        channel.getTopic(),
        "",
        "A new channel should have an empty topic"
    );

    expectTrue(
        channel.isEmpty(),
        "A new channel should have no members"
    );

    expectSizeEqual(
        channel.getMemberCount(),
        0,
        "A new channel should report zero members"
    );

    expectTrue(
        channel.getMembers().empty(),
        "A new channel membership collection should be empty"
    );

    expectFalse(
        channel.isInviteOnly(),
        "A new channel should start with invite-only mode disabled"
    );

    expectFalse(
        channel.isTopicRestricted(),
        "A new channel should start with topic restriction disabled"
    );

    expectFalse(
        channel.isKeyEnabled(),
        "A new channel should start with key mode disabled"
    );

    expectEqual(
        channel.getKey(),
        "",
        "A new channel should have no stored key"
    );

    expectFalse(
        channel.isLimitEnabled(),
        "A new channel should start with user-limit mode disabled"
    );

    expectSizeEqual(
        channel.getUserLimit(),
        0,
        "A new channel should have a user limit of zero"
    );

    expectFalse(
        channel.hasMember(NULL),
        "A null client should never be a channel member"
    );

    expectFalse(
        channel.hasOperator(NULL),
        "A null client should never be a channel operator"
    );

    expectFalse(
        channel.hasInvitation(NULL),
        "A null client should never have an invitation"
    );

    expectTrue(
        channel.getMessageHistory().empty(),
        "A new channel should have no stored messages"
    );
}

/**
 * @brief Verifies topic assignment, replacement, and clearing.
 */
static void testTopicManagement()
{
    Channel channel("#general");

    channel.setTopic("Initial topic");

    expectEqual(
        channel.getTopic(),
        "Initial topic",
        "setTopic should store the supplied topic"
    );

    channel.setTopic("Updated topic");

    expectEqual(
        channel.getTopic(),
        "Updated topic",
        "setTopic should replace the previous topic"
    );

    channel.setTopic("");

    expectEqual(
        channel.getTopic(),
        "",
        "An empty topic should clear the current topic"
    );
}

/**
 * @brief Verifies membership insertion, uniqueness, lookup, removal, and
 * empty-channel detection.
 */
static void testMemberManagement()
{
    Channel channel("#general");
    Client firstClient(10, "localhost");
    Client secondClient(11, "localhost");
    Client unrelatedClient(12, "localhost");

    channel.addMember(NULL);

    expectSizeEqual(
        channel.getMemberCount(),
        0,
        "Adding a null client should not change member count"
    );

    channel.addMember(&firstClient);

    expectTrue(
        channel.hasMember(&firstClient),
        "addMember should add a valid client"
    );

    expectFalse(
        channel.isEmpty(),
        "A channel with one member should not be empty"
    );

    expectSizeEqual(
        channel.getMemberCount(),
        1,
        "Adding the first client should produce one member"
    );

    expectTrue(
        channel.getMembers().find(&firstClient)
            != channel.getMembers().end(),
        "getMembers should contain an added client"
    );

    expectFalse(
        channel.hasOperator(&firstClient),
        "Adding a member should not automatically grant operator privileges"
    );

    channel.addMember(&firstClient);

    expectSizeEqual(
        channel.getMemberCount(),
        1,
        "Adding the same client twice should not create duplicates"
    );

    channel.addMember(&secondClient);

    expectSizeEqual(
        channel.getMemberCount(),
        2,
        "Adding a different client should increase member count"
    );

    channel.removeMember(&unrelatedClient);

    expectSizeEqual(
        channel.getMemberCount(),
        2,
        "Removing an unrelated client should not change member count"
    );

    channel.removeMember(NULL);

    expectSizeEqual(
        channel.getMemberCount(),
        2,
        "Removing a null client should not change member count"
    );

    channel.removeMember(&secondClient);

    expectFalse(
        channel.hasMember(&secondClient),
        "removeMember should remove an existing member"
    );

    expectSizeEqual(
        channel.getMemberCount(),
        1,
        "Removing one of two members should leave one member"
    );

    channel.removeMember(&firstClient);

    expectTrue(
        channel.isEmpty(),
        "Removing the final member should leave the channel empty"
    );

    expectSizeEqual(
        channel.getMemberCount(),
        0,
        "Removing every member should produce a count of zero"
    );
}

/**
 * @brief Verifies that operator privileges can only be granted to members and
 * can be revoked without removing channel membership.
 */
static void testOperatorManagement()
{
    Channel channel("#general");
    Client memberClient(20, "localhost");
    Client externalClient(21, "localhost");

    channel.addOperator(NULL);

    expectFalse(
        channel.hasOperator(NULL),
        "A null client should not become an operator"
    );

    channel.addOperator(&externalClient);

    expectFalse(
        channel.hasOperator(&externalClient),
        "A client outside the channel should not become an operator"
    );

    channel.addMember(&memberClient);
    channel.addOperator(&memberClient);

    expectTrue(
        channel.hasOperator(&memberClient),
        "An existing member should be granted operator privileges"
    );

    channel.addOperator(&memberClient);
    channel.removeOperator(&memberClient);

    expectFalse(
        channel.hasOperator(&memberClient),
        "One removal should revoke privileges after duplicate grants"
    );

    expectTrue(
        channel.hasMember(&memberClient),
        "Removing operator privileges should preserve channel membership"
    );

    channel.addOperator(&memberClient);
    channel.removeOperator(NULL);

    expectTrue(
        channel.hasOperator(&memberClient),
        "Removing a null operator should not affect existing operators"
    );

    channel.removeOperator(&memberClient);
    channel.removeOperator(&memberClient);

    expectFalse(
        channel.hasOperator(&memberClient),
        "Removing absent operator privileges should be harmless"
    );
}

/**
 * @brief Verifies invitation insertion, uniqueness, removal, and rejection of
 * clients that already belong to the channel.
 */
static void testInvitationManagement()
{
    Channel channel("#private");
    Client invitedClient(30, "localhost");
    Client memberClient(31, "localhost");

    channel.inviteClient(NULL);

    expectFalse(
        channel.hasInvitation(NULL),
        "A null client should not receive an invitation"
    );

    channel.inviteClient(&invitedClient);

    expectTrue(
        channel.hasInvitation(&invitedClient),
        "inviteClient should store a valid invitation"
    );

    channel.inviteClient(&invitedClient);
    channel.removeInvitation(&invitedClient);

    expectFalse(
        channel.hasInvitation(&invitedClient),
        "One removal should consume a duplicated invitation attempt"
    );

    channel.inviteClient(&invitedClient);
    channel.removeInvitation(NULL);

    expectTrue(
        channel.hasInvitation(&invitedClient),
        "Removing a null invitation should preserve valid invitations"
    );

    channel.removeInvitation(&invitedClient);
    channel.removeInvitation(&invitedClient);

    expectFalse(
        channel.hasInvitation(&invitedClient),
        "Removing an absent invitation should be harmless"
    );

    channel.addMember(&memberClient);
    channel.inviteClient(&memberClient);

    expectFalse(
        channel.hasInvitation(&memberClient),
        "A current channel member should not receive an invitation"
    );
}

/**
 * @brief Verifies the activation and deactivation of invite-only and
 * topic-restricted modes.
 */
static void testBooleanChannelModes()
{
    Channel channel("#general");

    channel.setInviteOnly(true);

    expectTrue(
        channel.isInviteOnly(),
        "setInviteOnly(true) should enable invite-only mode"
    );

    channel.setInviteOnly(false);

    expectFalse(
        channel.isInviteOnly(),
        "setInviteOnly(false) should disable invite-only mode"
    );

    channel.setTopic("Persistent topic");
    channel.setTopicRestricted(true);

    expectTrue(
        channel.isTopicRestricted(),
        "setTopicRestricted(true) should enable topic restriction"
    );

    expectEqual(
        channel.getTopic(),
        "Persistent topic",
        "Enabling topic restriction should not modify the current topic"
    );

    channel.setTopicRestricted(false);

    expectFalse(
        channel.isTopicRestricted(),
        "setTopicRestricted(false) should disable topic restriction"
    );

    expectEqual(
        channel.getTopic(),
        "Persistent topic",
        "Disabling topic restriction should not modify the current topic"
    );
}

/**
 * @brief Documents the privilege model used by TOPIC: with +t disabled any
 * member may change the topic; with +t enabled only channel operators may.
 */
static void testTopicRestrictionPrivilegeModel()
{
    Channel channel("#general");
    Client channelOperator(70, "localhost");
    Client regularMember(71, "localhost");

    channel.addMember(&channelOperator);
    channel.addOperator(&channelOperator);
    channel.addMember(&regularMember);

    expectFalse(
        channel.isTopicRestricted(),
        "A new channel should allow any member to change the topic"
    );

    expectTrue(
        channel.hasOperator(&channelOperator),
        "The first granted operator should keep operator privileges"
    );

    expectFalse(
        channel.hasOperator(&regularMember),
        "A later member should not become an operator automatically"
    );

    channel.setTopicRestricted(true);

    expectTrue(
        channel.isTopicRestricted()
            && channel.hasOperator(&channelOperator),
        "With +t enabled, a channel operator is allowed to change the topic"
    );

    expectTrue(
        channel.isTopicRestricted()
            && !channel.hasOperator(&regularMember),
        "With +t enabled, a regular member is not allowed to change the topic"
    );

    channel.setTopicRestricted(false);

    expectFalse(
        channel.isTopicRestricted(),
        "Disabling +t should restore unrestricted topic changes"
    );
}

/**
 * @brief Verifies key-mode activation, replacement, invalid empty keys,
 * removal, and state consistency.
 */
static void testKeyMode()
{
    Channel channel("#protected");

    channel.setKey("");

    expectFalse(
        channel.isKeyEnabled(),
        "An empty key should not enable key mode"
    );

    expectEqual(
        channel.getKey(),
        "",
        "An ignored empty key should leave the stored key empty"
    );

    channel.setKey("first-key");

    expectTrue(
        channel.isKeyEnabled(),
        "A non-empty key should enable key mode"
    );

    expectEqual(
        channel.getKey(),
        "first-key",
        "setKey should store the supplied key"
    );

    channel.setKey("replacement-key");

    expectEqual(
        channel.getKey(),
        "replacement-key",
        "setKey should replace an existing key"
    );

    channel.setKey("");

    expectTrue(
        channel.isKeyEnabled(),
        "An empty replacement should not disable an existing key"
    );

    expectEqual(
        channel.getKey(),
        "replacement-key",
        "An empty replacement should preserve the existing key"
    );

    channel.removeKey();

    expectFalse(
        channel.isKeyEnabled(),
        "removeKey should disable key mode"
    );

    expectEqual(
        channel.getKey(),
        "",
        "removeKey should clear the stored key"
    );

    channel.removeKey();

    expectFalse(
        channel.isKeyEnabled(),
        "Removing an absent key should be harmless"
    );

    expectEqual(
        channel.getKey(),
        "",
        "Repeated key removal should preserve an empty key"
    );
}

/**
 * @brief Verifies user-limit activation, replacement, rejection of zero,
 * removal, and preservation of existing members.
 */
static void testUserLimitMode()
{
    Channel channel("#limited");
    Client firstClient(40, "localhost");
    Client secondClient(41, "localhost");

    channel.setUserLimit(0);

    expectFalse(
        channel.isLimitEnabled(),
        "A zero limit should not enable user-limit mode"
    );

    expectSizeEqual(
        channel.getUserLimit(),
        0,
        "An ignored zero limit should leave the limit at zero"
    );

    channel.setUserLimit(10);

    expectTrue(
        channel.isLimitEnabled(),
        "A positive limit should enable user-limit mode"
    );

    expectSizeEqual(
        channel.getUserLimit(),
        10,
        "setUserLimit should store the supplied limit"
    );

    channel.setUserLimit(3);

    expectSizeEqual(
        channel.getUserLimit(),
        3,
        "setUserLimit should replace an existing limit"
    );

    channel.setUserLimit(0);

    expectTrue(
        channel.isLimitEnabled(),
        "A zero replacement should not disable an existing limit"
    );

    expectSizeEqual(
        channel.getUserLimit(),
        3,
        "A zero replacement should preserve the existing limit"
    );

    channel.addMember(&firstClient);
    channel.addMember(&secondClient);
    channel.setUserLimit(1);

    expectSizeEqual(
        channel.getMemberCount(),
        2,
        "Reducing the limit should not remove existing members"
    );

    expectSizeEqual(
        channel.getUserLimit(),
        1,
        "A limit lower than current membership should still be stored"
    );

    channel.removeUserLimit();

    expectFalse(
        channel.isLimitEnabled(),
        "removeUserLimit should disable user-limit mode"
    );

    expectSizeEqual(
        channel.getUserLimit(),
        0,
        "removeUserLimit should reset the stored limit to zero"
    );

    expectSizeEqual(
        channel.getMemberCount(),
        2,
        "Removing the limit should not modify channel membership"
    );

    channel.removeUserLimit();

    expectFalse(
        channel.isLimitEnabled(),
        "Removing an absent user limit should be harmless"
    );
}

/**
 * @brief Verifies that removing a member also clears operator privileges and
 * any pending invitation while preserving unrelated members.
 */
static void testMemberRemovalCleansRelatedState()
{
    Channel channel("#general");
    Client removedClient(50, "localhost");
    Client remainingClient(51, "localhost");

    channel.inviteClient(&removedClient);
    channel.addMember(&removedClient);
    channel.addOperator(&removedClient);
    channel.addMember(&remainingClient);

    expectTrue(
        channel.hasInvitation(&removedClient),
        "The test client should have an invitation before removal"
    );

    expectTrue(
        channel.hasOperator(&removedClient),
        "The test client should be an operator before removal"
    );

    channel.removeMember(&removedClient);

    expectFalse(
        channel.hasMember(&removedClient),
        "removeMember should remove channel membership"
    );

    expectFalse(
        channel.hasOperator(&removedClient),
        "removeMember should clear operator privileges"
    );

    expectFalse(
        channel.hasInvitation(&removedClient),
        "removeMember should clear pending invitations"
    );

    expectTrue(
        channel.hasMember(&remainingClient),
        "Removing one client should preserve unrelated members"
    );

    expectFalse(
        channel.isEmpty(),
        "A channel with an unrelated remaining member should not be empty"
    );

    channel.removeMember(&remainingClient);

    expectTrue(
        channel.isEmpty(),
        "Removing the final remaining member should empty the channel"
    );
}

/**
 * @brief Verifies that Channel stores non-owning Client pointers and does not
 * destroy referenced Client objects.
 */
static void testChannelDoesNotOwnClients()
{
    Client survivingClient(60, "localhost");

    survivingClient.setNickname("survivor");

    {
        Channel channel("#temporary");

        channel.addMember(&survivingClient);

        expectTrue(
            channel.hasMember(&survivingClient),
            "The temporary channel should contain the client"
        );
    }

    expectEqual(
        survivingClient.getNickname(),
        "survivor",
        "Destroying a channel should not destroy its referenced clients"
    );

    expectEqual(
        survivingClient.getHost(),
        "localhost",
        "The client should remain usable after channel destruction"
    );
}

/**
 * @brief Verifies that channel PRIVMSG history ignores empty lines and
 * preserves insertion order for later JOIN replay.
 */
static void testMessageHistory()
{
    Channel channel("#general");

    expectTrue(
        channel.getMessageHistory().empty(),
        "A new channel should start with an empty message history"
    );

    channel.addHistoryMessage("");

    expectTrue(
        channel.getMessageHistory().empty(),
        "Empty messages should not be stored in channel history"
    );

    channel.addHistoryMessage(
        ":alice!alice@localhost PRIVMSG #general :hola\r\n"
    );
    channel.addHistoryMessage(
        ":alice!alice@localhost PRIVMSG #general :adios\r\n"
    );

    expectSizeEqual(
        channel.getMessageHistory().size(),
        2,
        "Channel history should keep every non-empty stored message"
    );

    expectEqual(
        channel.getMessageHistory()[0],
        ":alice!alice@localhost PRIVMSG #general :hola\r\n",
        "Channel history should preserve the first stored message"
    );

    expectEqual(
        channel.getMessageHistory()[1],
        ":alice!alice@localhost PRIVMSG #general :adios\r\n",
        "Channel history should preserve later stored messages in order"
    );
}

int main()
{
    testInitialChannelState();
    testTopicManagement();
    testMemberManagement();
    testOperatorManagement();
    testInvitationManagement();
    testBooleanChannelModes();
    testTopicRestrictionPrivilegeModel();
    testKeyMode();
    testUserLimitMode();
    testMemberRemovalCleansRelatedState();
    testChannelDoesNotOwnClients();
    testMessageHistory();

    if (g_failures != 0)
    {
        std::cerr
            << g_failures
            << " Channel model test(s) failed"
            << std::endl;

        return EXIT_FAILURE;
    }

    std::cout
        << "All Channel model tests passed"
        << std::endl;

    return EXIT_SUCCESS;
}