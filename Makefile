# ── Variables and utilities ────────────────────────────────────────────────────────────────
SHELL := /usr/bin/bash
.SHELLFLAGS := -ec

BLUE := $(shell printf '\033[0;34m')
GREEN := $(shell printf '\033[0;32m')
YELLOW := $(shell printf '\033[0;33m')
RESET := $(shell printf '\033[0m')
CYAN := $(shell printf '\033[0;36m')
ORANGE := $(shell printf '\033[0;31m')
RED := $(shell printf '\033[0;31m')
MAGENTA := $(shell printf '\033[0;35m')
SUCCESS := $(GREEN)✓
FAIL := $(RED)✗
INFO := $(CYAN)ℹ
WARN := $(YELLOW)⚠

# * Top row (╭━━━╮) - round corners, full-span
# * Bottom row (╰━━━╯) - round corners, full-span
# * Merge row (┣━━━┫) - full-span, left/right T junctions
# * Merge-bottom (╰━━━╯) - alias for kind 3
# * Column cross (┣━╋━┫) - cross junctions (columns above/below)
# * Column open (┣━┳━┫) - T-down (columns start below)
# * Column close (┣━┻━┫) - T-up (columns end above)

TOP_LEFT_CORNER := $(ORANGE)╭$(RESET)
TOP_RIGHT_CORNER := $(ORANGE)╮$(RESET)
BOTTOM_LEFT_CORNER := $(ORANGE)╰$(RESET)
BOTTOM_RIGHT_CORNER := $(ORANGE)╯$(RESET)
HORIZONTAL_LINE := $(ORANGE)━$(RESET)
VERTICAL_LINE := $(ORANGE)┃$(RESET)
LEFT_JUNCTION := $(ORANGE)┣$(RESET)
RIGHT_JUNCTION := $(ORANGE)┫$(RESET)
CROSS_JUNCTION := $(ORANGE)┣$(RESET)$(ORANGE)━$(RESET)$(ORANGE)┫$(RESET)
OPEN_JUNCTION := $(ORANGE)┣$(RESET)$(ORANGE)━$(RESET)$(ORANGE)┫$(RESET)
CLOSE_JUNCTION := $(ORANGE)┣$(RESET)$(ORANGE)━$(RESET)$(ORANGE)┫$(RESET)

# Reusable text banner: $(call print_banner,Your message)
define print_banner
box_width=50; \
inner_width=$$((box_width - 2)); \
message="$(1)"; \
message_length=$${#message}; \
padding=$$((inner_width - message_length)); \
left_padding=$$((padding / 2)); \
right_padding=$$((padding - left_padding)); \
top_border="$(TOP_LEFT_CORNER)"; \
bottom_border="$(BOTTOM_LEFT_CORNER)"; \
horizontal_line="$(HORIZONTAL_LINE)"; \
vertical_line="$(VERTICAL_LINE)"; \
for ((i=0; i<box_width-2; i++)); do top_border="$$top_border$$horizontal_line"; bottom_border="$$bottom_border$$horizontal_line"; done; \
top_border="$$top_border$(TOP_RIGHT_CORNER)"; \
bottom_border="$$bottom_border$(BOTTOM_RIGHT_CORNER)"; \
printf "%s\n" "$$top_border"; \
printf "%s%*s%s%*s%s\n" "$$vertical_line" "$$left_padding" '' "$$message" "$$right_padding" '' "$$vertical_line"; \
printf "%s\n" "$$bottom_border"
endef

define print_error
echo -e "$(FAIL) $(1)$(RESET)"
endef

define print_success
echo -e "$(SUCCESS) $(1)$(RESET)"
endef

define print_info
echo -e "$(INFO) $(1)$(RESET)"
endef

define print_warning
echo -e "$(WARN) $(1)$(RESET)"
endef

# * Timer helper
define RUN_AND_LOG
	@start_ms=$$(date +%s%3N); \
	$(1); status=$$?; \
	end_ms=$$(date +%s%3N); \
	elapsed_ms=$$((end_ms - start_ms)); \
	if [ $$status -eq 0 ]; then \
		printf "%b [%sms]\n" "$(2)" "$$elapsed_ms"; \
	fi; \
	exit $$status
endef

NAME =	ircserv
IRC := $(MAGENTA)[$(NAME)]$(RESET)

.DEFAULT_GOAL := $(NAME)

MAKEFLAGS += --no-print-directory

CXX =	c++

CXXFLAGS = -Wall -Wextra -Werror -std=c++98 -g3 -fsanitize=address -MMD -MP

INC = -I ./inc/

OBJ_DIR = ./src/obj/

SRC_DIR			= src/
SRC_SRCS		= $(addprefix $(SRC_DIR), \
					main.cpp \
					SignalHandler.cpp \
					Server.cpp \
					Client.cpp \
					IrcMessage.cpp \
					IrcCasemap.cpp \
					MessageParser.cpp \
					Console.cpp \
					Channel.cpp)

TEST_DIR = ./src/tests/
MSG_PARSER_TEST = message_parser.test
IRC_MSG_TEST = $(TEST_DIR)irc_message_tests.test
CASEMAP_TEST = $(TEST_DIR)irc_casemap_tests.test
TEST_SRC = $(TEST_DIR)IrcMessageTests.cpp
CASEMAP_TEST_SRC = $(TEST_DIR)IrcCasemapTests.cpp
PROTOCOL_TEST = $(TEST_DIR)protocol_tests.test
PROTOCOL_TEST_SRC = $(TEST_DIR)ProtocolTests.cpp

SRCS 			= $(SRC_SRCS)

COMMANDS_DIR	= src/commands/
COMMANDS_SRC	= $(addprefix $(COMMANDS_DIR), \
					RegistrationCommands.cpp \
					CommandDispatcher.cpp)

SRCS			+= $(COMMANDS_SRC)

TESTS_DIR		= src/tests/
TESTS_SRCS		= $(addprefix $(TESTS_DIR), \
					MessageParserTest.cpp)

OBJS = $(addprefix $(OBJ_DIR), $(notdir $(SRCS:.cpp=.o)))
# .d files to avoid recompiling the whole app when a header file is modified
DEPS = $(addprefix $(OBJ_DIR), $(notdir $(SRCS:.cpp=.d)))
-include $(DEPS)

RM = /bin/rm -rf

SILENT = --silent
NOPRINT += --no-print-directory

$(NAME): $(OBJS)
	@$(CXX) $(CXXFLAGS) $(INC) $(OBJS) -o $(NAME)

# all: obj $(NAME) ## 🔨 Compiles the whole program
all: $(NAME) ## 🔨 Compiles the whole program
	@build_plan="$$($(MAKE) -s -n $(NAME) 2>&1)"; status=$$?; \
	if [ $$status -ne 0 ]; then \
		printf "%s\n" "$$build_plan"; \
		exit $$status; \
	elif [ -n "$$build_plan" ]; then \
		$(MAKE) -s $(NAME); \
	else \
		printf "%b\n" "$(IRC) $(CYAN)Everything is up to date$(RESET)"; \
	fi

$(OBJ_DIR)%.o: $(SRC_DIR)%.cpp
	@mkdir -p $(OBJ_DIR)
	@echo " $(YELLOW)$<$(RESET)"
	@$(CXX) $(CXXFLAGS) $(INC) -c $< -o $@

$(OBJ_DIR)%.o: $(COMMANDS_DIR)%.cpp
	@echo " $(YELLOW)$<$(RESET)"
	@$(CXX) $(CXXFLAGS) $(INC) -c $< -o $@

obj: ## 🗂️  Creates the object directory
	@mkdir -p $(OBJ_DIR)

clean: ## 🧹 Removes the object files
# 	@$(RM) $(OBJS) $(OBJ_DIR)
	$(call RUN_AND_LOG,$(RM) $(OBJ_DIR),$(IRC) $(RED)Object files removed $(RESET))
fclean: ## 🗑️  Removes both object and executable files
# 	@$(RM) $(NAME)
	$(call RUN_AND_LOG,$(MAKE) clean $(NOPRINT); $(RM) $(NAME) $(MSG_PARSER_TEST) $(IRC_MSG_TEST) $(CASEMAP_TEST) $(PROTOCOL_TEST),$(IRC) $(RED)Removed $(RESET))

re: ## 🔁 Rebuilds the library
# 	@echo "$(GREEN)$(NAME) [OK]$(RESET)"
	$(call RUN_AND_LOG,$(MAKE) $(SILENT) fclean $(NOPRINT); $(MAKE) all $(NOPRINT),$(IRC) $(YELLOW)Rebuilt $(RESET))

help: ## ❓ Show available targets
	@$(call print_banner,Available Makefile Targets)
	@echo ""
	@grep -hE '^[a-zA-Z_-]+:.*## .*$$' Makefile | \
		awk 'BEGIN {FS = ":.*## "}; {printf "  $(CYAN)%-25s$(RESET) %s\n", $$1, $$2}'
	@echo ""

test: ## 🧪 Runs all tests
	@$(call print_banner,Parser tests)
	@$(MAKE) test-parser $(NOPRINT)
	@$(call print_banner,Message tests)
	@$(MAKE) test-message $(NOPRINT)
	@$(call print_banner,Protocol checklist tests)
	@$(MAKE) test-protocol $(NOPRINT)

test-parser: ## 🧪 Runs message parser test case
	@$(CXX) $(CXXFLAGS) $(INC) \
		$(TESTS_SRCS) \
		$(SRC_DIR)IrcMessage.cpp \
		$(SRC_DIR)MessageParser.cpp \
		-o $(MSG_PARSER_TEST)
	@./$(MSG_PARSER_TEST)

test-message: ## 🧪 Runs the IRC message serialization and casemap tests
	@mkdir -p $(TEST_DIR)
	@echo " $(YELLOW)$(TEST_SRC)$(RESET)"
	@$(CXX) $(CXXFLAGS) $(INC) $(TEST_SRC) $(SRC_DIR)IrcMessage.cpp -o $(IRC_MSG_TEST)
	@$(IRC_MSG_TEST)
	@echo " $(YELLOW)$(CASEMAP_TEST_SRC)$(RESET)"
	@$(CXX) $(CXXFLAGS) $(INC) $(CASEMAP_TEST_SRC) $(SRC_DIR)IrcCasemap.cpp -o $(CASEMAP_TEST)
	@$(CASEMAP_TEST)

test-protocol: $(NAME) ## 🧪 Runs point 1 protocol checklist tests
	@mkdir -p $(TEST_DIR)
	@echo " $(YELLOW)$(PROTOCOL_TEST_SRC)$(RESET)"
	@$(CXX) $(CXXFLAGS) $(INC) \
		$(PROTOCOL_TEST_SRC) \
		$(SRC_DIR)Client.cpp \
		$(SRC_DIR)IrcMessage.cpp \
		$(SRC_DIR)IrcCasemap.cpp \
		$(SRC_DIR)Console.cpp \
		-o $(PROTOCOL_TEST)
	@$(PROTOCOL_TEST)

.PHONY: all obj clean fclean re help test test-parser test-message test-protocol
