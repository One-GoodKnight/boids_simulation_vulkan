NAME			:= vulkan

BUILD_DIR		:= .build

SRC_DIR			:= src
SRCS			:= 								\
	vulkan/BDA/create_buffer.c					\
	vulkan/BDA/upload.c							\
	main.c										\
	camera.c									\
	load_files/load_gltf_file.c					\
	load_files/load_spirv_file.c				\

INCLUDES		:= include

SRCS			:= $(SRCS:%=$(SRC_DIR)/%)
OBJS			:= $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
DEPS			:= $(OBJS:.o=.d)

CC				:= cc
CFLAGS			:= -Wall -Wextra -g3 -DDEBUG -DWIREFRAMEO
CPPFLAGS		:= -I$(INCLUDES) -MMD -MP
LDFLAGS 		:= -lSDL3 -lvulkan -lcglm -lm

SHADER_DIR		:= assets/shaders
SHADERS_FRAG	:=								\
	triangle									\

SHADERS_VERT	:=								\
	triangle									\

SPVS_FRAG		:= $(SHADERS_FRAG:%=$(SHADER_DIR)/%.frag.spv)
SPVS_VERT		:= $(SHADERS_VERT:%=$(SHADER_DIR)/%.vert.spv)
SPVS			:= $(SPVS_FRAG) $(SPVS_VERT)

SLANGC      	:= slangc

all: $(NAME) $(SPVS)

$(NAME): $(OBJS)
	$(CC) $^ $(LDFLAGS) -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c Makefile
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

-include $(DEPS)

$(SHADER_DIR)/%.vert.spv: $(SHADER_DIR)/%.slang
	$(SLANGC) $< -target spirv -entry vertex_main -o $@

$(SHADER_DIR)/%.frag.spv: $(SHADER_DIR)/%.slang
	$(SLANGC) $< -target spirv -entry fragment_main -o $@

clean:
	rm -rf $(OBJS) $(DEPS)

fclean: clean
	rm -f $(NAME) $(SPVS)

re: fclean all

run: all
	./$(NAME)

.PHONY: all clean fclean re run
