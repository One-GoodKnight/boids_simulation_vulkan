#include <cglm/cglm.h>

#include "camera.h"

void init_camera(t_camera *cam)
{
	cam->position[0] = 0.0f;
	cam->position[1] = 2.0f;
	cam->position[2] = 6.0f;
	cam->yaw = -90.0f;
	cam->pitch = 0.0f;
}

void camera_rotate(t_camera *cam, float dx, float dy)
{
	dx *= SENSITIVITY;
	dy *= SENSITIVITY;

	cam->yaw += dx;
	cam->pitch -= dy;

	if (cam->pitch < -89.9f)
		cam->pitch = -89.9f;

	if (cam->pitch > 89.9f)
		cam->pitch = 89.9f;
}

void get_mvp(t_camera *cam, float screen_width, float screen_height, mat4 mvp)
{
	mat4 model, view, proj;
	glm_mat4_identity(model);

	// forward vector
	float yaw   = glm_rad(cam->yaw);
    float pitch = glm_rad(cam->pitch);
    cam->direction[0] = cos(pitch) * cos(yaw);
    cam->direction[1] = sin(pitch);
    cam->direction[2] = cos(pitch) * sin(yaw);
	glm_vec3_normalize(cam->direction);

	vec3 up = { 0.0f, 1.0f, 0.0f };
	glm_look(cam->position, cam->direction, up, view);

	float aspect = screen_width / screen_height;
	glm_perspective(glm_rad(60.0f), aspect, 0.1f, 100.0f, proj);
	proj[1][1] *= -1;   /* opengl and vulkans y's are flipped */

	glm_mat4_mul(proj, view, mvp);
	glm_mat4_mul(mvp, model, mvp);
}
