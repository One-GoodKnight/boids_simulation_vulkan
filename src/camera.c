#include <SDL3/SDL_scancode.h>
#include <cglm/cglm.h>
#include <cglm/vec3.h>
#include <stdint.h>

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

void camera_move(t_camera *cam, const bool *keyboard_state, float dt)
{
	vec3 input_vector = {0};

	input_vector[2] += keyboard_state[SDL_SCANCODE_W]; /* forward is -Z */
	input_vector[2] -= keyboard_state[SDL_SCANCODE_S];
	input_vector[0] += keyboard_state[SDL_SCANCODE_D];
	input_vector[0] -= keyboard_state[SDL_SCANCODE_A];

	input_vector[1] += keyboard_state[SDL_SCANCODE_SPACE];
	input_vector[1] -= keyboard_state[SDL_SCANCODE_LSHIFT];

	glm_vec3_normalize(input_vector);

	vec3 forward = {
		cam->direction[0],
		0,
		cam->direction[2]
	};
	glm_vec3_normalize(forward);

	vec3 up = { 0.0f, 1.0f, 0.0f };
	vec3 right;
	glm_vec3_cross(forward, up, right);
	glm_vec3_normalize(right);

	vec3 world_direction = {0};
	glm_vec3_muladds(right, input_vector[0], world_direction);
	glm_vec3_muladds(up, input_vector[1], world_direction);
	glm_vec3_muladds(forward, input_vector[2], world_direction);

	cam->position[0] += world_direction[0] * CAMERA_SPEED * dt;
	cam->position[1] += world_direction[1] * CAMERA_SPEED * dt;
	cam->position[2] += world_direction[2] * CAMERA_SPEED * dt;
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
	glm_perspective(glm_rad(60.0f), aspect, 0.1f, 1000000.0f, proj);
	proj[1][1] *= -1;   /* opengl and vulkans y's are flipped */

	glm_mat4_mul(proj, view, mvp);
	glm_mat4_mul(mvp, model, mvp);
}
