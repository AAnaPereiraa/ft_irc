NAME = ircserv
CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98

SRC_DIR = src
OBJ_DIR = obj

SRCS = $(SRC_DIR)/main.cpp \
       $(SRC_DIR)/Server.cpp \
       $(SRC_DIR)/Client.cpp \
       $(SRC_DIR)/Channel.cpp \
       $(SRC_DIR)/Command.cpp
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
# Runs every test twice: normally, then with the server under valgrind.
# Fails on a failed test, a memory leak/error or a socket left open.
test: $(NAME)
	@command -v valgrind >/dev/null 2>&1 || { echo "make test: valgrind is not installed"; exit 1; }
	@echo "=== Integration tests ==="
	python3 tests/test_server.py
	@echo "=== Integration tests under valgrind ==="
	VALGRIND=1 python3 tests/test_server.py

-include $(DEPS)

.PHONY: all clean fclean re test