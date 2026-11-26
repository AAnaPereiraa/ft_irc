NAME = ircserv
CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98

SRC_DIR = src
OBJ_DIR = obj

SRCS = $(SRC_DIR)/main.cpp \
       $(SRC_DIR)/Server.cpp \
       $(SRC_DIR)/Client.cpp \
       $(SRC_DIR)/Channel.cpp \
       $(SRC_DIR)/Command.cpp \
	   $(SRC_DIR)/MessageHandler.cpp
OBJS = $(SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)
DEPS = $(OBJS:.o=.d)

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -MMD -MP -Iinclude -c $< -o $@

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -f $(NAME)

re: fclean all

# Integration tests (tests/test_server.py), also run by GitHub Actions.
test: $(NAME)
	python3 tests/test_server.py

test-valgrind: $(NAME)
	VALGRIND=1 python3 tests/test_server.py

-include $(DEPS)

.PHONY: all clean fclean re test test-valgrind