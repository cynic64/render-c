#ifndef OBJ_H
#define OBJ_H

/*
 * fast_obj.h must already be included by the user
 */

#include "hashmap.h"

#include <stdlib.h>

struct ObjMesh {
        uint32_t index_offset;
        uint32_t index_ct;
};

struct ObjVertex {
        vec3 pos;
        vec3 norm;
        vec2 tex_c;
};

// To be able to use ObjVertex with hashmap.h
uint32_t obj_vertex_hash(uint32_t bucket_ct, const void* key) {
        const uint8_t* bytes = key;

	uint32_t hash = 0;
	for (int i = 0; i < sizeof(struct ObjVertex); i++) {
        	hash ^= hash << 5;
        	hash += bytes[i];
	}

	return hash % bucket_ct;
}

int obj_vertex_compare(const void* a, const void* b) {
        return memcmp(a, b, sizeof(struct ObjVertex));
}

// Returns one ObjMesh for each material in the OBJ file. Call once with vertices/indices/meshes
// NULL to determine how much space to allocate, then call again to fill the allocated space. The real vertex
// count is not necessarily what was returned the first time (since some are duplicates), so the vertex_ct
// returned the second time is the real count.
void obj_convert_model(fastObjMesh* model,
                       uint32_t* vertex_ct, struct ObjVertex* vertices,
                       uint32_t* index_ct, uint32_t* indices,
                       uint32_t* mesh_ct, struct ObjMesh* meshes)
{
        // Establish counts, maybe return early
        *index_ct = 0;
        for (int i = 0; i < model->face_count; i++) {
                int num_tris = model->face_vertices[i] - 2;
                *index_ct += 3 * num_tris;
        }
        *vertex_ct = *index_ct;
        *mesh_ct = model->material_count;

        if (vertices == NULL || indices == NULL || meshes == NULL) return;

        // Fill in index counts for each material
        for (int i = 0; i < *mesh_ct; i++) meshes[i].index_ct = 0;

        for (int i = 0; i < model->face_count; i++) {
                int mat_idx = model->face_materials[i];
                int num_tris = model->face_vertices[i] - 2;
                meshes[mat_idx].index_ct += 3 * num_tris;
        }

        // Fill in index offsets for each material
        uint32_t mesh_idx_offset = 0;
        for (int i = 0; i < *mesh_ct; i++) {
                meshes[i].index_offset = mesh_idx_offset;
                mesh_idx_offset += meshes[i].index_ct;
        }

	// Read vertices and fill in indices
        uint32_t vertex_idx_out = 0;
        uint32_t index_idx_in = 0;

        // Where we output the index to depends on what the current face's material is,
        // so we need to know what the next index index in each mesh is
        uint32_t* mesh_idx_outs = malloc(*mesh_ct * sizeof(mesh_idx_outs[0]));
        for (int i = 0; i < *mesh_ct; i++) mesh_idx_outs[i] = meshes[i].index_offset;

        struct Hashmap vertices_seen;
        hashmap_create(&vertices_seen);

        for (int i = 0; i < model->face_count; i++) {
                int face_verts = model->face_vertices[i];
                int mat_idx = model->face_materials[i];
                for (int offset = 1; offset < face_verts - 1; offset++) {
                        uint32_t tri_idx_idxs[3] = {index_idx_in, index_idx_in + offset, index_idx_in + offset + 1};
                        for (int j = 0; j < 3; ++j) {
                                fastObjIndex idx = model->indices[tri_idx_idxs[j]];
                                uint32_t index_idx_out = mesh_idx_outs[mat_idx];

                                struct ObjVertex vertex;
                                vertex.pos[0] = model->positions[3*idx.p+0];
                                vertex.pos[1] = model->positions[3*idx.p+1];
                                vertex.pos[2] = model->positions[3*idx.p+2];
                                vertex.norm[0] = model->normals[3*idx.n+0];
                                vertex.norm[1] = model->normals[3*idx.n+1];
                                vertex.norm[2] = model->normals[3*idx.n+2];
                                vertex.tex_c[0] = model->texcoords[2*idx.t+0];
                                vertex.tex_c[1] = model->texcoords[2*idx.t+1];

        			uint32_t prev_idx;
                                int found = hashmap_get(&vertices_seen, obj_vertex_hash, obj_vertex_compare,
                                                        &vertex, &prev_idx);
                                if (found) {
                                        indices[index_idx_out] = prev_idx;
                                } else {
                                        vertices[vertex_idx_out] = vertex;
                                        indices[index_idx_out] = vertex_idx_out;
                                        hashmap_insert(&vertices_seen, obj_vertex_hash,
                                                       &vertices[vertex_idx_out], vertex_idx_out);
                                        vertex_idx_out++;
                                }

                                mesh_idx_outs[mat_idx]++;
                        }
                }
                index_idx_in += face_verts;
        }

        *vertex_ct = vertex_idx_out;

        hashmap_destroy(&vertices_seen);
        free(mesh_idx_outs);
}

#endif // OBJ_H

