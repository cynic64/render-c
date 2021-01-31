#ifndef CAMERA_H
#define CAMERA_H

#include <cglm/cglm.h>

#include <math.h>

const float CAMERA_FOV = 1.5F;
const float CAMERA_NEAR = 0.1F;
const float CAMERA_FAR = 10000.0F;
const float CAMERA_EXTREMES_MARGIN = 0.01F;

struct CameraFly {
        float pitch;
        float yaw;

	vec3 eye;
	vec3 forward;

	mat4 view;
};

// Movement is relative to the camera. +X means right, +Y means up, +Z means forward.
void camera_fly_update(struct CameraFly* cam, float d_yaw, float d_pitch,
                       vec3 movement, float delta)
{
        // Adjust angles
        cam->pitch += d_pitch;
        cam->yaw += d_yaw;

	const float min_pitch = -M_PI_2 + CAMERA_EXTREMES_MARGIN;
	const float max_pitch = M_PI_2 - CAMERA_EXTREMES_MARGIN;
	cam->pitch = cam->pitch > max_pitch ? max_pitch : cam->pitch;
	cam->pitch = cam->pitch < min_pitch ? min_pitch : cam->pitch;

	cam->forward[0] = sinf(cam->yaw) * cosf(cam->pitch);
	cam->forward[1] = -sinf(cam->pitch);
	cam->forward[2] = cosf(cam->yaw) * cosf(cam->pitch);

	// Adjust position
	vec3 scaled_movement;
	glm_vec3_scale(movement, delta, scaled_movement);

	vec3 up = {0.0F, -1.0F, 0.0F};
        vec3 right;
        glm_vec3_crossn(cam->forward, up, right);

        glm_vec3_muladds(right, scaled_movement[0], cam->eye);
        glm_vec3_muladds(up, scaled_movement[1], cam->eye);
        glm_vec3_muladds(cam->forward, scaled_movement[2], cam->eye);

	// Calculate view matrix
	vec3 looking_at;
	glm_vec3_add(cam->eye, cam->forward, looking_at);

	glm_lookat(cam->eye, looking_at, up, cam->view);
}

#endif // CAMERA_H

