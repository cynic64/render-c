#ifndef LL_GLFW_ERROR_H
#define LL_GLFW_ERROR_H

#include <stdio.h>

void glfw_error_callback(int code, const char* description) {
        fprintf(stderr, "GLFW error [%d]: %s\n", code, description);
}

#endif // LL_GLFW_ERROR_H

