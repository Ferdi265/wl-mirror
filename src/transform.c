#include <string.h>
#include <wayland-client-protocol.h>
#include "transform.h"

static const mat3_t mat_identity = { .data = {
    {  1,  0,  0 },
    {  0,  1,  0 },
    {  0,  0,  1 }
}};

static const mat3_t mat_rot_cw_90 = { .data = {
    {  0, -1,  1 },
    {  1,  0,  0 },
    {  0,  0,  1 }
}};

static const mat3_t mat_rot_cw_180 = { .data = {
    { -1,  0,  1 },
    {  0, -1,  1 },
    {  0,  0,  1 }
}};

static const mat3_t mat_rot_cw_270 = { .data = {
    {  0,  1,  0 },
    { -1,  0,  1 },
    {  0,  0,  1 }
}};

static const mat3_t mat_flip_x = { .data = {
    { -1,  0,  1 },
    {  0,  1,  0 },
    {  0,  0,  1 }
}};

static const mat3_t mat_flip_y = { .data = {
    {  1,  0,  0 },
    {  0, -1,  1 },
    {  0,  0,  1 }
}};

void mat3_identity(mat3_t * mat) {
    *mat = mat_identity;
}

void mat3_transpose(mat3_t * mat) {
    for (size_t row = 0; row < 3; row++) {
        for (size_t col = row + 1; col < 3; col++) {
            float temp = mat->data[row][col];
            mat->data[row][col] = mat->data[col][row];
            mat->data[col][row] = temp;
        }
    }
}

void mat3_mul(const mat3_t * mul, mat3_t * dest) {
    mat3_t src = *dest;

    for (size_t row = 0; row < 3; row++) {
        for (size_t col = 0; col < 3; col++) {
            dest->data[row][col] = 0;
            for (size_t i = 0; i < 3; i++) {
                dest->data[row][col] += mul->data[row][i] * src.data[i][col];
            }
        }
    }
}

void mat3_apply_transform(mat3_t * mat, transform_t transform) {
    switch (transform.rotation) {
        case ROT_NORMAL:
            break;
        case ROT_CW_90:
            mat3_mul(&mat_rot_cw_90, mat);
            break;
        case ROT_CW_180:
            mat3_mul(&mat_rot_cw_180, mat);
            break;
        case ROT_CW_270:
            mat3_mul(&mat_rot_cw_270, mat);
            break;
    }

    if (transform.flip_x) mat3_mul(&mat_flip_x, mat);
    if (transform.flip_y) mat3_mul(&mat_flip_y, mat);
}

void mat3_apply_inverse_output_transform(mat3_t * mat, enum wl_output_transform transform) {
    switch (transform) {
        case WL_OUTPUT_TRANSFORM_NORMAL:
            break;
        case WL_OUTPUT_TRANSFORM_90:
            mat3_mul(&mat_rot_cw_90, mat);
            break;
        case WL_OUTPUT_TRANSFORM_180:
            mat3_mul(&mat_rot_cw_180, mat);
            break;
        case WL_OUTPUT_TRANSFORM_270:
            mat3_mul(&mat_rot_cw_270, mat);
            break;
        case WL_OUTPUT_TRANSFORM_FLIPPED:
            mat3_mul(&mat_flip_x, mat);
            break;
        case WL_OUTPUT_TRANSFORM_FLIPPED_90:
            mat3_mul(&mat_flip_x, mat);
            mat3_mul(&mat_rot_cw_90, mat);
            break;
        case WL_OUTPUT_TRANSFORM_FLIPPED_180:
            mat3_mul(&mat_flip_x, mat);
            mat3_mul(&mat_rot_cw_180, mat);
            break;
        case WL_OUTPUT_TRANSFORM_FLIPPED_270:
            mat3_mul(&mat_flip_x, mat);
            mat3_mul(&mat_rot_cw_270, mat);
            break;
    }
}

void mat3_apply_invert_y(mat3_t * mat, bool invert_y) {
    if (invert_y) {
        mat3_mul(&mat_flip_y, mat);
    }
}

void viewport_apply_transform(uint32_t * width, uint32_t * height, transform_t transform) {
    uint32_t w = *width;
    uint32_t h = *height;

    switch (transform.rotation) {
        case ROT_CCW_90:
        case ROT_CCW_270:
            *height = w;
            *width = h;
            break;
        default:
            break;
    }
}

void viewport_apply_inverse_output_transform(uint32_t * width, uint32_t * height, enum wl_output_transform transform) {
    uint32_t w = *width;
    uint32_t h = *height;

    switch (transform) {
        case WL_OUTPUT_TRANSFORM_90:
        case WL_OUTPUT_TRANSFORM_270:
        case WL_OUTPUT_TRANSFORM_FLIPPED_90:
        case WL_OUTPUT_TRANSFORM_FLIPPED_270:
            *height = w;
            *width = h;
            break;
        default:
            break;
    }
}
