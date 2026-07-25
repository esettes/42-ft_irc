NAME =	ircserv

CXX =	c++

CXXFLAGS = -Wall -Wextra -Werror -std=c++98 -g3 -fsanitize=address

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

RM = /bin/rm -rf

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(INC) $(OBJS) -o $(NAME)

all: obj $(NAME)

$(OBJ_DIR)%.o: $(SRC_DIR)%.cpp
	@mkdir -p $(OBJ_DIR)
	@$(CXX) $(CXXFLAGS) $(INC) -c $< -o $@

$(OBJ_DIR)%.o: $(COMMANDS_DIR)%.cpp
	@$(CXX) $(CXXFLAGS) $(INC) -c $< -o $@

obj:
	@mkdir -p $(OBJ_DIR)

clean:
	$(RM) $(OBJS)

fclean: clean 
	$(RM) $(NAME)

re: fclean all

.PHONY: all clean fclean re