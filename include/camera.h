#ifndef CAMERA_H
# define CAMERA_H

# include "cglm/cglm.h"

# define SENSITIVITY 25

typedef struct s_camera {
	vec3              position;
	vec3              direction;
	float             yaw;
	float             pitch;
} t_camera;

void init_camera(t_camera *cam);
void camera_rotate(t_camera *cam, float dx, float dy);
void get_mvp(t_camera *cam, float screen_width, float screen_height, mat4 mvp);

#endif
