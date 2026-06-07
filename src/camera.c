#include <cglm/cglm.h>

#include "camera.h"

void init_camera(t_camera *cam)
{
	cam->position[0] = 0.0f;
	cam->position[1] = 2.0f;
	cam->position[2] = 6.0f;
	glm_mat4_identity(cam->rotation);
	glm_vec3_copy((vec3){ 0.0f, 0.0f, -1.0f }, cam->direction);
}

void camera_rotate(t_camera *cam, float dx, float dy)
{
	mat4 rot_x, rot_y;

	dx *= SENSITIVITY;
	dy *= SENSITIVITY;

	vec3 up = { 0.0f, 1.0f, 0.0f };
	glm_rotate_make(rot_x, dx, up);
	glm_mat4_mul(rot_x, cam->rotation, cam->rotation);

	vec3 local_right = {
		cam->rotation[0][0],
		cam->rotation[1][0],
		cam->rotation[2][0]
	};
	glm_rotate_make(rot_y, dy, local_right);
    glm_mat4_mul(rot_y, cam->rotation, cam->rotation);
}

void get_mvp(t_camera *cam, float screen_width, float screen_height, mat4 mvp)
{
	mat4 model, view, proj;
	glm_mat4_identity(model);

	// forward vector
	cam->direction[0] = -cam->rotation[0][2];
	cam->direction[1] = -cam->rotation[1][2];
	cam->direction[2] = -cam->rotation[2][2];
	glm_vec3_normalize(cam->direction);

	vec3 up = { 0.0f, 1.0f, 0.0f };
	glm_look(cam->position, cam->direction, up, view);

	float aspect = screen_width / screen_height;
	glm_perspective(glm_rad(60.0f), aspect, 0.1f, 100.0f, proj);
	proj[1][1] *= -1;   /* opengl and vulkans y's are flipped */

	glm_mat4_mul(proj, view, mvp);
	glm_mat4_mul(mvp, model, mvp);
}
