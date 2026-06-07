#include "app.h"

void init_camera(t_app *app)
{
	glm_mat4_identity(app->cam_rot);
	glm_vec3_copy((vec3){ 0.0f, 0.0f, -1.0f }, app->cam_dir);
}


