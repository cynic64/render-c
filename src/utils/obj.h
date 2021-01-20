#ifndef UTILS_OBJ_H
#define UTILS_OBJ_H

#include <stdio.h>

struct ObjVertex {
        float pos[3];
        float normal[3];
};

// Allocates its own memory
void obj_load(const char* path, int* vertex_ct, struct ObjVertex** vertices, int* index_ct, int** indices) {
        FILE *fp = fopen(path, "r");
        assert(fp != NULL);

	// Figure out counts
        *vertex_ct = 0;
        *index_ct = 0;
        int normal_ct = 0;

        size_t len = 100;
        char* line = malloc(len);
	while (getline(&line, &len, fp) != -1) {
        	if (line[0] == 'v' && line[1] == ' ') (*vertex_ct)++;
        	else if (line[0] == 'v' && line[1] == 'n') normal_ct++;
        	else if (line[0] == 'f') (*index_ct) += 3;
	}

	*vertices = malloc(*vertex_ct * sizeof(*vertices[0]));
	*indices = malloc(*index_ct * sizeof(*indices[0]));
	float (*normals)[3] = malloc(normal_ct * sizeof(normals[0]));

	// One pass for vertices and normals (we don't yet know which belongs to which)
	rewind(fp);

	int vertex_idx = 0;
	int normal_idx = 0;
	while (getline(&line, &len, fp) != -1) {
        	float x, y, z;
        	if (sscanf(line, "v %f %f %f\n", &x, &y, &z)) {
                	(*vertices)[vertex_idx].pos[0] = x;
                	(*vertices)[vertex_idx].pos[1] = y;
                	(*vertices)[vertex_idx].pos[2] = z;
                	vertex_idx++;
        	} else if (sscanf(line, "vn %f %f %f\n", &x, &y, &z)) {
                	normals[normal_idx][0] = x;
                	normals[normal_idx][1] = y;
                	normals[normal_idx][2] = z;
                	normal_idx++;
        	}
	}

	// Final pass to complete vertices and fill indices
	rewind(fp);
	int indices_idx = 0;
	while (getline(&line, &len, fp) != -1) {
        	int v1_idx, n1_idx, v2_idx, n2_idx, v3_idx, n3_idx;
        	int bytes_read = sscanf(line, "f %d//%d %d//%d %d//%d\n",
                                        &v1_idx, &n1_idx, &v2_idx, &n2_idx, &v3_idx, &n3_idx);
        	if (bytes_read > 0) {
                	// Assign normals to vertices
			(*vertices)[v1_idx-1].normal[0] = normals[n1_idx-1][0];
			(*vertices)[v1_idx-1].normal[1] = normals[n1_idx-1][1];
			(*vertices)[v1_idx-1].normal[2] = normals[n1_idx-1][2];
			(*vertices)[v2_idx-1].normal[0] = normals[n2_idx-1][0];
			(*vertices)[v2_idx-1].normal[1] = normals[n2_idx-1][1];
			(*vertices)[v2_idx-1].normal[2] = normals[n2_idx-1][2];
			(*vertices)[v3_idx-1].normal[0] = normals[n3_idx-1][0];
			(*vertices)[v3_idx-1].normal[1] = normals[n3_idx-1][1];
			(*vertices)[v3_idx-1].normal[2] = normals[n3_idx-1][2];

			// Assign to indices
			(*indices)[indices_idx++] = v1_idx-1;
			(*indices)[indices_idx++] = v2_idx-1;
			(*indices)[indices_idx++] = v3_idx-1;
        	}
	}

	free(normals);
	free(line);
	fclose(fp);
}

#endif // UTILS_OBJ_H

