NAME			:= vulkan

BUILD_DIR		:= .build

SRC_DIR			:= src
SRCS			:= 								\
	vulkan/BDA/create_buffer.c					\
	vulkan/BDA/upload.c							\
	vulkan/bindless.c							\
	vulkan/depth.c								\
	vulkan/device.c								\
	vulkan/frame.c								\
	vulkan/instance.c							\
	vulkan/pipeline.c							\
	vulkan/swapchain.c							\
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
SHADERS_VERT	:=								\
	boids_graphics								\

SHADERS_FRAG	:=								\
	boids_graphics								\

SHADERS_COMP	:=								\
	boids_compute								\

SPVS_VERT		:= $(SHADERS_VERT:%=$(SHADER_DIR)/%.vert.spv)
SPVS_FRAG		:= $(SHADERS_FRAG:%=$(SHADER_DIR)/%.frag.spv)
SPVS_COMP		:= $(SHADERS_COMP:%=$(SHADER_DIR)/%.comp.spv)
SPVS			:= $(SPVS_FRAG) $(SPVS_VERT) $(SPVS_COMP)

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

$(SHADER_DIR)/%.comp.spv: $(SHADER_DIR)/%.slang
	$(SLANGC) $< -target spirv -entry compute_main -o $@

clean:
	rm -rf $(OBJS) $(DEPS)

fclean: clean
	rm -f $(NAME) $(SPVS)

re: fclean all

run: all
	./$(NAME)

.PHONY: all clean fclean re run
