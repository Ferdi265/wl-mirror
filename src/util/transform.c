#include <string.h>
#include <wayland-client-protocol.h>
#include <wlm/util/transform.h>

static const wlm_util_mat3_t mat_identity = { .data = {
    {  1,  0,  0 },
    {  0,  1,  0 },
    {  0,  0,  1 }
}};

static const wlm_util_mat3_t mat_rot_ccw_90 = { .data = {
    {  0,  1,  0 },
    { -1,  0,  1 },
    {  0,  0,  1 }
}};

static const wlm_util_mat3_t mat_rot_ccw_180 = { .data = {
    { -1,  0,  1 },
    {  0, -1,  1 },
    {  0,  0,  1 }
}};

static const wlm_util_mat3_t mat_rot_ccw_270 = { .data = {
    {  0, -1,  1 },
    {  1,  0,  0 },
    {  0,  0,  1 }
}};

static const wlm_util_mat3_t mat_flip_x = { .data = {
    { -1,  0,  1 },
    {  0,  1,  0 },
    {  0,  0,  1 }
}};

static const wlm_util_mat3_t mat_flip_y = { .data = {
    {  1,  0,  0 },
    {  0, -1,  1 },
    {  0,  0,  1 }
}};

void wlm_util_mat3_identity(wlm_util_mat3_t * mat) {
    *mat = mat_identity;
}

void wlm_util_mat3_transpose(wlm_util_mat3_t * mat) {
    for (size_t row = 0; row < 3; row++) {
        for (size_t col = row + 1; col < 3; col++) {
            float temp = mat->data[row][col];
            mat->data[row][col] = mat->data[col][row];
            mat->data[col][row] = temp;
        }
    }
}

void wlm_util_mat3_mul(const wlm_util_mat3_t * mul, wlm_util_mat3_t * dest) {
    wlm_util_mat3_t src = *dest;

    for (size_t row = 0; row < 3; row++) {
        for (size_t col = 0; col < 3; col++) {
            dest->data[row][col] = 0;
            for (size_t i = 0; i < 3; i++) {
                dest->data[row][col] += mul->data[row][i] * src.data[i][col];
            }
        }
    }
}

void wlm_util_mat3_apply_transform(wlm_util_mat3_t * mat, wlm_util_transform_t transform) {
    // apply inverse transformation matrix as we need to transform
    // from OpenGL space to dmabuf space

    switch (transform.rotation) {
        case ROT_NORMAL:
            break;
        case ROT_CW_90:
            wlm_util_mat3_mul(&mat_rot_ccw_90, mat);
            break;
        case ROT_CW_180:
            wlm_util_mat3_mul(&mat_rot_ccw_180, mat);
            break;
        case ROT_CW_270:
            wlm_util_mat3_mul(&mat_rot_ccw_270, mat);
            break;
    }

    if (transform.flip_x) wlm_util_mat3_mul(&mat_flip_x, mat);
    if (transform.flip_y) wlm_util_mat3_mul(&mat_flip_y, mat);
}

void wlm_util_mat3_apply_region_transform(wlm_util_mat3_t * mat, const wlm_util_region_t * region, const wlm_util_region_t * output) {
    wlm_util_mat3_t region_transform;
    wlm_util_mat3_identity(&region_transform);

    region_transform.data[0][2] = (float)region->x / output->width;
    region_transform.data[1][2] = (float)region->y / output->height;
    region_transform.data[0][0] = (float)region->width / output->width;
    region_transform.data[1][1] = (float)region->height / output->height;

    wlm_util_mat3_mul(&region_transform, mat);
}

void wlm_util_mat3_apply_output_transform(wlm_util_mat3_t * mat, enum wl_output_transform transform) {
    // wl_output transform is already inverted

    switch (transform) {
        case WL_OUTPUT_TRANSFORM_NORMAL:
            break;
        case WL_OUTPUT_TRANSFORM_90:
            wlm_util_mat3_mul(&mat_rot_ccw_90, mat);
            break;
        case WL_OUTPUT_TRANSFORM_180:
            wlm_util_mat3_mul(&mat_rot_ccw_180, mat);
            break;
        case WL_OUTPUT_TRANSFORM_270:
            wlm_util_mat3_mul(&mat_rot_ccw_270, mat);
            break;
        case WL_OUTPUT_TRANSFORM_FLIPPED:
            wlm_util_mat3_mul(&mat_flip_x, mat);
            break;
        case WL_OUTPUT_TRANSFORM_FLIPPED_90:
            wlm_util_mat3_mul(&mat_flip_x, mat);
            wlm_util_mat3_mul(&mat_rot_ccw_90, mat);
            break;
        case WL_OUTPUT_TRANSFORM_FLIPPED_180:
            wlm_util_mat3_mul(&mat_flip_x, mat);
            wlm_util_mat3_mul(&mat_rot_ccw_180, mat);
            break;
        case WL_OUTPUT_TRANSFORM_FLIPPED_270:
            wlm_util_mat3_mul(&mat_flip_x, mat);
            wlm_util_mat3_mul(&mat_rot_ccw_270, mat);
            break;
    }
}

void wlm_util_mat3_apply_invert_y(wlm_util_mat3_t * mat, bool invert_y) {
    if (invert_y) {
        wlm_util_mat3_mul(&mat_flip_y, mat);
    }
}

void wlm_util_viewport_apply_transform(uint32_t * width, uint32_t * height, wlm_util_transform_t transform) {
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

void wlm_util_viewport_apply_output_transform(uint32_t * width, uint32_t * height, enum wl_output_transform transform) {
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

bool wlm_util_region_contains(const wlm_util_region_t * region, const wlm_util_region_t * output) {
    if (region->x + region->width <= output->x) return false;
    if (region->x >= output->x + output->width) return false;
    if (region->y + region->height <= output->y) return false;
    if (region->y >= output->y + output->height) return false;
    return true;
}

void wlm_util_region_scale(wlm_util_region_t * region, double scale) {
    region->x *= scale;
    region->y *= scale;
    region->width *= scale;
    region->height *= scale;
}

void wlm_util_region_clamp(wlm_util_region_t * region, const wlm_util_region_t * output) {
    if (region->x < output->x) {
        region->width -= output->x - region->x;
        region->x = 0;
    } else {
        region->x -= output->x;
    }

    if (region->x + region->width > output->width) {
        region->width = output->width - region->x;
    }

    if (region->y < output->y) {
        region->height -= output->y - region->y;
        region->y = 0;
    } else {
        region->y -= output->y;
    }

    if (region->y + region->height > output->height) {
        region->height = output->height - region->y;
    }
}
