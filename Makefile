GREEN = \033[0;32m
YELLOW = \033[0;33m
RED = \033[0;31m
RESET = \033[0m

NAME =	ircserv

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
					IrcMessage.cpp)
SRCS 			= $(SRC_SRCS)

COMMANDS_DIR	= src/commands/
COMMANDS_SRC	= $(addprefix $(COMMANDS_DIR), \
					RegistrationCommands.cpp \
					CommandDispatcher.cpp)

SRCS			+= $(COMMANDS_SRC)

OBJS = $(addprefix $(OBJ_DIR), $(notdir $(SRCS:.cpp=.o)))
# .d files to avoid recompiling the whole app when a header file is modified
DEPS = $(addprefix $(OBJ_DIR)/,$(SRCS:.cpp=.d))
-include $(DEPS)

RM = /bin/rm -rf

$(NAME): $(OBJS)
	@$(CXX) $(CXXFLAGS) $(INC) $(OBJS) -o $(NAME)

all: obj $(NAME)

$(OBJ_DIR)%.o: $(SRC_DIR)%.cpp
	@mkdir -p $(OBJ_DIR)
	@echo "$(YELLOW)$<$(RESET)"
	@$(CXX) $(CXXFLAGS) $(INC) -c $< -o $@

$(OBJ_DIR)%.o: $(COMMANDS_DIR)%.cpp
	@echo "$(YELLOW)$<$(RESET)"
	@$(CXX) $(CXXFLAGS) $(INC) -c $< -o $@

obj:
	@mkdir -p $(OBJ_DIR)

clean:
	@$(RM) $(OBJS) $(OBJ_DIR)

fclean: clean 
	@$(RM) $(NAME)

re: fclean all
	@echo "$(GREEN)$(NAME) [OK]$(RESET)"

.PHONY: all clean fclean re
