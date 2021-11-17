#ifndef WL_MIRROR_TRANSFORM_H_
#define WL_MIRROR_TRANSFORM_H_

#include <stdbool.h>
#include <wayland-client-protocol.h>
#include "linux-dmabuf-unstable-v1.h"

typedef enum {
    ROT_CW_0,
    ROT_CW_90,
    ROT_CW_180,
    ROT_CW_270,

    ROT_NORMAL = ROT_CW_0,
    ROT_CCW_0 = ROT_CW_0,
    ROT_CCW_90 = ROT_CW_270,
    ROT_CCW_180 = ROT_CW_180,
    ROT_CCW_270 = ROT_CW_90
} rotation_t;

typedef struct {
    rotation_t rotation;
    bool flip_x;
    bool flip_y;
} transform_t;

typedef struct {
    float data[3][3];
} mat3_t;

void mat3_identity(mat3_t * mat);
void mat3_transpose(mat3_t * mat);
void mat3_mul(const mat3_t * mul, mat3_t * dest);

void mat3_apply_transform(mat3_t * mat, transform_t transform);
void mat3_apply_output_transform(mat3_t * mat, enum wl_output_transform transform);
void mat3_apply_invert_y(mat3_t * mat, bool invert_y);

void viewport_apply_transform(uint32_t * width, uint32_t * height, transform_t transform);
void viewport_apply_output_transform(uint32_t * width, uint32_t * height, enum wl_output_transform transform);

#endif
