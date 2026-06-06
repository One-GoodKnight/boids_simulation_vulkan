NAME			:= vulkan

BUILD_DIR		:= .build

SRC_DIR			:= src
SRCS			:= 								\
	main.c										\
	read_file.c									\

INCLUDES		:= include

SRCS			:= $(SRCS:%=$(SRC_DIR)/%)
OBJS			:= $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
DEPS			:= $(OBJS:.o=.d)

CC				:= cc
CFLAGS			:= -Wall -Wextra -g3 -DDEBUG
CPPFLAGS		:= -I$(INCLUDES) -MMD -MP
LDFLAGS 		:= -lSDL3 -lvulkan

all: $(NAME)

$(NAME): $(OBJS)
	$(CC) $^ $(LDFLAGS) -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

-include $(DEPS)

clean:
	rm -rf $(OBJS) $(DEPS)

fclean: clean
	rm -f $(NAME)

re: fclean all

run: all
	./$(NAME)

.PHONY: all clean fclean re run
