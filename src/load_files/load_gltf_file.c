#include "geometry.h"

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

Mesh load_mesh_from_gltf_file(const char *path)
{
    cgltf_options options = {0};
    cgltf_data   *data    = NULL;

    if (cgltf_parse_file(&options, path, &data) != cgltf_result_success) {
        fprintf(stderr, "Failed to load gltf: %s\n", path);
        exit(1);
    }
    cgltf_load_buffers(&options, data, path);

    cgltf_primitive *prim = &data->meshes[0].primitives[0];

    cgltf_accessor *pos_acc = NULL;
    cgltf_accessor *uv_acc  = NULL;
    for (size_t i = 0; i < prim->attributes_count; i++) {
        if (prim->attributes[i].type == cgltf_attribute_type_position)
            pos_acc = prim->attributes[i].data;
        if (prim->attributes[i].type == cgltf_attribute_type_texcoord)
            uv_acc  = prim->attributes[i].data;
    }
    if (!pos_acc) { fputs("No positions in mesh\n", stderr); exit(1); }

    Mesh mesh = {0};
    mesh.vertex_count = (uint32_t)pos_acc->count;
    mesh.vertices     = malloc(sizeof(t_vertex) * mesh.vertex_count);

    for (uint32_t i = 0; i < mesh.vertex_count; i++) {
        cgltf_accessor_read_float(pos_acc, i, mesh.vertices[i].position, 3);
        if (uv_acc)
            cgltf_accessor_read_float(uv_acc, i, mesh.vertices[i].uv, 2);
    }

    /* indices */
    if (prim->indices) {
        mesh.index_count = (uint32_t)prim->indices->count;
        mesh.indices     = malloc(sizeof(uint32_t) * mesh.index_count);
        for (uint32_t i = 0; i < mesh.index_count; i++)
            mesh.indices[i] = (uint32_t)cgltf_accessor_read_index(prim->indices, i);
    }

    cgltf_free(data);
    printf("Loaded %u vertices, %u indices from %s\n",
           mesh.vertex_count, mesh.index_count, path);
	// for (uint32_t i = 0; i < mesh.vertex_count; i++)
	// {
	// 	printf(
	// 			"%.2f, %.2f, %.2f\n",
	// 			mesh.vertices[i].position[0],
	// 			mesh.vertices[i].position[1],
	// 			mesh.vertices[i].position[2]
	// 	);
	// }
    return mesh;
}

void free_mesh(Mesh *m)
{
    free(m->vertices);
    free(m->indices);
    m->vertices    = NULL;
    m->indices     = NULL;
    m->vertex_count = 0;
    m->index_count  = 0;
}
