#ifndef CAMERA_H
# define CAMERA_H

# include <SDL3/SDL_keycode.h>
# include "cglm/cglm.h"

# define SENSITIVITY 0.01f
# define CAMERA_SPEED 50
# define FOV 70.0f

typedef struct s_camera {
	vec3              position;
	vec3              direction;
	float             yaw;
	float             pitch;
} t_camera;

void init_camera(t_camera *cam);
void camera_rotate(t_camera *cam, float dx, float dy);
void camera_move(t_camera *cam, const bool *keyboard_state, float dt);
void get_mvp(t_camera *cam, float screen_width, float screen_height, mat4 mvp);

#endif
