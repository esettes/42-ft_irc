// Copyright 2026 @esettes, @danielfdez17
#include <iostream>
#include <string>

#include "IrcCasemap.hpp"

static int g_failures = 0;

static void expectTrue(bool condition, const std::string &message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << std::endl;
        ++g_failures;
    }
}

static void expectEqual(
    const std::string &actual,
    const std::string &expected,
    const std::string &message
) {
    if (actual != expected) {
        std::cerr << "FAIL: " << message << std::endl;
        std::cerr << "  expected: " << expected << std::endl;
        std::cerr << "  actual  : " << actual << std::endl;
        ++g_failures;
    }
}

static void testAsciiCaseFolding() {
    expectEqual(IrcCasemap::normalize("Roxana"), "roxana",
        "normalize should lowercase ASCII letters");
    expectEqual(IrcCasemap::normalize("ROXANA"), "roxana",
        "normalize should lowercase uppercase nicknames");
    expectTrue(IrcCasemap::equal("Roxana", "roxana"),
        "equal should treat mixed-case nicknames as the same");
    expectTrue(IrcCasemap::equal("ROXANA", "roxana"),
        "equal should treat fully upper and lower nicknames as the same");
}

static void testRfc1459SpecialEquivalence() {
    expectEqual(IrcCasemap::normalize("Nick[A]"), "nick{a}",
        "normalize should map [] to {} under rfc1459");
    expectEqual(IrcCasemap::normalize("Path\\X"), "path|x",
        "normalize should map backslash to pipe under rfc1459");
    expectTrue(IrcCasemap::equal("Nick[A]", "nick{a}"),
        "equal should treat [] and {} as equivalent");
    expectTrue(IrcCasemap::equal("User\\One", "user|one"),
        "equal should treat \\ and | as equivalent");
}

static void testChannelNames() {
    expectTrue(IrcCasemap::equal("#General", "#general"),
        "equal should ignore case in channel names");
    expectTrue(IrcCasemap::equal("#Chan[Test]", "#chan{test}"),
        "equal should apply rfc1459 mapping to channel names");
    expectEqual(IrcCasemap::normalize("#General"), "#general",
        "normalize should preserve channel prefix while lowercasing");
}

static void testNonEquivalence() {
    expectTrue(!IrcCasemap::equal("alice", "bob"),
        "equal should reject distinct nicknames");
    expectTrue(!IrcCasemap::equal("#chat", "#lounge"),
        "equal should reject distinct channel names");
}

int main() {
    testAsciiCaseFolding();
    testRfc1459SpecialEquivalence();
    testChannelNames();
    testNonEquivalence();

    if (g_failures != 0) {
        std::cerr << g_failures << " test(s) failed" << std::endl;
        return 1;
    }

    std::cout << "All IrcCasemap tests passed" << std::endl;
    return 0;
}
