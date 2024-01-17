#ifndef WL_MIRROR_TRANSFORM_H_
#define WL_MIRROR_TRANSFORM_H_

#include <stdbool.h>
#include <wayland-client-protocol.h>

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
} wlm_util_rotation_t;

typedef struct {
    wlm_util_rotation_t rotation;
    bool flip_x;
    bool flip_y;
} wlm_util_transform_t;

typedef struct {
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
} wlm_util_region_t;

typedef struct {
    float data[3][3];
} wlm_util_mat3_t;

void wlm_util_mat3_identity(wlm_util_mat3_t * mat);
void wlm_util_mat3_transpose(wlm_util_mat3_t * mat);
void wlm_util_mat3_mul(const wlm_util_mat3_t * mul, wlm_util_mat3_t * dest);

void wlm_util_mat3_apply_transform(wlm_util_mat3_t * mat, wlm_util_transform_t transform);
void wlm_util_mat3_apply_region_transform(wlm_util_mat3_t * mat, const wlm_util_region_t * region, const wlm_util_region_t * output);
void wlm_util_mat3_apply_output_transform(wlm_util_mat3_t * mat, enum wl_output_transform transform);
void wlm_util_mat3_apply_invert_y(wlm_util_mat3_t * mat, bool invert_y);

void wlm_util_viewport_apply_transform(uint32_t * width, uint32_t * height, wlm_util_transform_t transform);
void wlm_util_viewport_apply_output_transform(uint32_t * width, uint32_t * height, enum wl_output_transform transform);

bool wlm_util_region_contains(const wlm_util_region_t * region, const wlm_util_region_t * output);
void wlm_util_region_scale(wlm_util_region_t * region, double scale);
void wlm_util_region_clamp(wlm_util_region_t * region, const wlm_util_region_t * output);

#endif
