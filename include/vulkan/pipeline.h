#ifndef PIPELINE_H
# define PIPELINE_H

# include "app.h"

void create_compute_spatial_hash_pipelines(t_app *a, const char *boid_cell_path);
void create_compute_pipeline(t_app *a, const char *path);
void create_graphics_pipeline(t_app *a);
void create_outline_pipeline(t_app *a);

#endif
