#include "wlm/mirror/target.h"
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wlm/context.h>
#include <wlm/egl.h>
#include <wlm/transform.h>
#include <wlm/util.h>
#include <wlm/glsl/vertex_shader.h>
#include <wlm/glsl/fragment_shader.h>

// --- buffers ---

static const float vertex_array[] = {
    -1.0, -1.0, 0.0, 0.0,
     1.0, -1.0, 1.0, 0.0,
    -1.0,  1.0, 0.0, 1.0,
    -1.0,  1.0, 0.0, 1.0,
     1.0, -1.0, 1.0, 0.0,
     1.0,  1.0, 1.0, 1.0
};

// --- has_extension ---

static bool has_extension(const char * extension) {
    size_t ext_len = strlen(extension);

    // try to find extension in extension list
    const char * extensions = (const char *)glGetString(GL_EXTENSIONS);
    const char * match = strstr(extensions, extension);

    // verify match was not a substring of another extension
    bool found = (
        match != NULL &&
        (match == extensions || match[-1] == ' ') &&
        (match[ext_len] == '\0' || match[ext_len] == ' ')
    );

    return found;
}

// --- init_egl ---

void wlm_egl_init(ctx_t * ctx) {
    // initialize context structure
    ctx->egl.display = EGL_NO_DISPLAY;
    ctx->egl.context = EGL_NO_CONTEXT;
    ctx->egl.config = EGL_NO_CONFIG_KHR;
    ctx->egl.surface = EGL_NO_SURFACE;
    ctx->egl.window = EGL_NO_SURFACE;

    ctx->egl.glEGLImageTargetTexture2DOES = NULL;
    ctx->egl.eglQueryDmaBufFormatsEXT = NULL;
    ctx->egl.eglQueryDmaBufModifiersEXT = NULL;

    ctx->egl.dmabuf_formats.num_formats = 0;
    ctx->egl.dmabuf_formats.formats = NULL;

    ctx->egl.width = 1;
    ctx->egl.height = 1;
    ctx->egl.format = 0;

    ctx->egl.vbo = 0;
    ctx->egl.texture = 0;
    ctx->egl.freeze_texture = 0;
    ctx->egl.freeze_framebuffer = 0;
    ctx->egl.shader_program = 0;
    ctx->egl.texture_transform_uniform = 0;
    ctx->egl.invert_colors_uniform = 0;

    ctx->egl.texture_region_aware = false;
    ctx->egl.texture_initialized = false;
    ctx->egl.initialized = true;

    // create egl display
    ctx->egl.display = eglGetDisplay((EGLNativeDisplayType)ctx->wl.display);
    if (ctx->egl.display == EGL_NO_DISPLAY) {
        wlm_log_error("egl::init(): failed to create EGL display\n");
        wlm_exit_fail(ctx);
    }

    // initialize egl display and check egl version
    EGLint major, minor;
    if (eglInitialize(ctx->egl.display, &major, &minor) != EGL_TRUE) {
        wlm_log_error("egl::init(): failed to initialize EGL display\n");
        wlm_exit_fail(ctx);
    }
    wlm_log_debug(ctx, "egl::init(): initialized EGL %d.%d\n", major, minor);

    // find an egl config with
    // - window support
    // - OpenGL ES 2.0 support
    // - RGB888 texture support
    EGLint num_configs;
    EGLint config_attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_NONE
    };
    if (eglChooseConfig(ctx->egl.display, config_attribs, &ctx->egl.config, 1, &num_configs) != EGL_TRUE) {
        wlm_log_error("egl::init(): failed to get EGL config\n");
        wlm_exit_fail(ctx);
    }

    // default window size to 100x100 if not set
    if (ctx->wl.width == 0) ctx->wl.width = 100;
    if (ctx->wl.height == 0) ctx->wl.height = 100;

    // create egl window
    ctx->egl.window = wl_egl_window_create(ctx->wl.surface, ctx->wl.width, ctx->wl.height);
    if (ctx->egl.window == EGL_NO_SURFACE) {
        wlm_log_error("egl::init(): failed to create EGL window\n");
        wlm_exit_fail(ctx);
    }

    // create egl surface
    ctx->egl.surface = eglCreateWindowSurface(ctx->egl.display, ctx->egl.config, (EGLNativeWindowType)ctx->egl.window, NULL);

    // create egl context with support for OpenGL ES 2.0
    EGLint context_attribs[] = {
        EGL_CONTEXT_MAJOR_VERSION, 2,
        EGL_CONTEXT_MINOR_VERSION, 0,
        EGL_NONE
    };
    ctx->egl.context = eglCreateContext(ctx->egl.display, ctx->egl.config, EGL_NO_CONTEXT, context_attribs);
    if (ctx->egl.context == EGL_NO_CONTEXT) {
        wlm_log_error("egl::init(): failed to create EGL context\n");
        wlm_exit_fail(ctx);
    }

    // activate egl context
    if (eglMakeCurrent(ctx->egl.display, ctx->egl.surface, ctx->egl.surface, ctx->egl.context) != EGL_TRUE) {
        wlm_log_error("egl::init(): failed to activate EGL context\n");
        wlm_exit_fail(ctx);
    }

    // check for needed extensions
    // - GL_OES_EGL_image: for converting EGLImages to GL textures
    if (!has_extension("GL_OES_EGL_image")) {
        wlm_log_error("egl::init(): missing EGL extension GL_OES_EGL_image\n");
        wlm_exit_fail(ctx);
    }

    // get pointers to functions provided by extensions
    // - glEGLImageTargetTexture2DOES: for converting EGLImages to GL textures
    ctx->egl.glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
    if (ctx->egl.glEGLImageTargetTexture2DOES == NULL) {
        wlm_log_error("egl::init(): failed to get pointer to glEGLImageTargetTexture2DOES\n");
        wlm_exit_fail(ctx);
    }

    // query dmabuf formats
    if (!wlm_egl_query_dmabuf_formats(ctx)) {
        wlm_log_warn("egl::init(): can't list dmabuf modifiers, this might affect some dmabuf backends\n");
    }

    // create vertex buffer object
    glGenBuffers(1, &ctx->egl.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, ctx->egl.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof vertex_array, vertex_array, GL_STATIC_DRAW);

    // create texture and set scaling mode
    glGenTextures(1, &ctx->egl.texture);
    glBindTexture(GL_TEXTURE_2D, ctx->egl.texture);
    if (ctx->opt.scaling_filter == SCALE_FILTER_LINEAR) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    // create freeze texture and set scaling mode
    glGenTextures(1, &ctx->egl.freeze_texture);
    glBindTexture(GL_TEXTURE_2D, ctx->egl.freeze_texture);
    if (ctx->opt.scaling_filter == SCALE_FILTER_LINEAR) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    // create freeze framebuffer
    glGenFramebuffers(1, &ctx->egl.freeze_framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, ctx->egl.freeze_framebuffer);
    glBindTexture(GL_TEXTURE_2D, ctx->egl.texture);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ctx->egl.texture, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // error log for shader compilation error messages
    GLint success;
    const char * shader_source = NULL;
    char errorLog[1024] = { 0 };

    // compile vertex shader
    shader_source = wlm_glsl_vertex_shader;
    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &shader_source, NULL);
    glCompileShader(vertex_shader);
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
    if (success != GL_TRUE) {
        glGetShaderInfoLog(vertex_shader, sizeof errorLog, NULL, errorLog);
        errorLog[strcspn(errorLog, "\n")] = '\0';
        wlm_log_error("egl::init(): failed to compile vertex shader: %s\n", errorLog);
        glDeleteShader(vertex_shader);
        wlm_exit_fail(ctx);
    }

    // compile fragment shader
    shader_source = wlm_glsl_fragment_shader;
    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &shader_source, NULL);
    glCompileShader(fragment_shader);
    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
    if (success != GL_TRUE) {
        glGetShaderInfoLog(fragment_shader, sizeof errorLog, NULL, errorLog);
        errorLog[strcspn(errorLog, "\n")] = '\0';
        wlm_log_error("egl::init(): failed to compile fragment shader: %s\n", errorLog);
        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
        wlm_exit_fail(ctx);
    }

    // create shader program and get pointers to shader uniforms
    ctx->egl.shader_program = glCreateProgram();
    glAttachShader(ctx->egl.shader_program, vertex_shader);
    glAttachShader(ctx->egl.shader_program, fragment_shader);
    glLinkProgram(ctx->egl.shader_program);
    glGetProgramiv(ctx->egl.shader_program, GL_LINK_STATUS, &success);
    if (success != GL_TRUE) {
        wlm_log_error("egl::init(): failed to link shader program\n");
        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
        glDeleteProgram(ctx->egl.shader_program);
        wlm_exit_fail(ctx);
    }
    ctx->egl.texture_transform_uniform = glGetUniformLocation(ctx->egl.shader_program, "uTexTransform");
    ctx->egl.invert_colors_uniform = glGetUniformLocation(ctx->egl.shader_program, "uInvertColors");
    glUseProgram(ctx->egl.shader_program);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    // set initial texture transform matrix
    mat3_t texture_transform;
    wlm_util_mat3_identity(&texture_transform);
    glUniformMatrix3fv(ctx->egl.texture_transform_uniform, 1, false, (float *)texture_transform.data);

    // set invert colors uniform
    bool invert_colors = ctx->opt.invert_colors;
    glUniform1i(ctx->egl.invert_colors_uniform, invert_colors);

    // set GL clear color to back and set GL vertex layout
    glClearColor(0.0, 0.0, 0.0, 1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof (float), (void *)(0 * sizeof (float)));
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof (float), (void *)(2 * sizeof (float)));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
}

// --- query_dmabuf_formats ---

bool wlm_egl_query_dmabuf_formats(ctx_t * ctx) {
    //if (!has_extension("EGL_EXT_image_dma_buf_import_modifiers")) {
    //    wlm_log_error("egl::init(): missing EGL extension EGL_EXT_image_dma_buf_import_modifiers\n");
    //    return false;
    //}

    // get pointers to optional functions provided by extensions
    // - eglQueryDmaBufFormatsEXT: for getting dmabuf formats
    ctx->egl.eglQueryDmaBufFormatsEXT = (PFNEGLQUERYDMABUFFORMATSEXTPROC)eglGetProcAddress("eglQueryDmaBufFormatsEXT");
    if (ctx->egl.eglQueryDmaBufFormatsEXT == NULL) {
        wlm_log_error("egl::init(): failed to get pointer to eglQueryDmaBufFormatsEXT\n");
        return false;
    }

    // - eglQueryDmaBufModifiersEXT: for getting dmabuf format modifiers
    ctx->egl.eglQueryDmaBufModifiersEXT = (PFNEGLQUERYDMABUFMODIFIERSEXTPROC)eglGetProcAddress("eglQueryDmaBufModifiersEXT");
    if (ctx->egl.eglQueryDmaBufModifiersEXT == NULL) {
        wlm_log_error("egl::init(): failed to get pointer to eglQueryDmaBufModifiersEXT\n");
        return false;
    }

    EGLint num_formats = 0;
    if (!ctx->egl.eglQueryDmaBufFormatsEXT(ctx->egl.display, 0, NULL, &num_formats)) {
        wlm_log_error("egl::init(): failed to query number of egl dmabuf formats\n");
        return false;
    }

    EGLint * egl_drm_formats = calloc(num_formats, sizeof *egl_drm_formats);
    if (egl_drm_formats == NULL) {
        wlm_log_error("egl::init(): failed to allocate egl dmabuf format array\n");
        return false;
    }

    if (!ctx->egl.eglQueryDmaBufFormatsEXT(ctx->egl.display, num_formats, egl_drm_formats, &num_formats)) {
        free(egl_drm_formats);
        wlm_log_error("egl::init(): failed to query egl dmabuf formats\n");
        return false;
    }

    dmabuf_format_t * dmabuf_formats = calloc(num_formats, sizeof *dmabuf_formats);
    if (dmabuf_formats == NULL) {
        free(egl_drm_formats);
        wlm_log_error("egl::init(): failed to allocate dmabuf format array\n");
        return false;
    }

    bool success = true;
    for (size_t i = 0; i < (size_t)num_formats; i++) {
        EGLint num_modifiers = 0;
        if (!ctx->egl.eglQueryDmaBufModifiersEXT(ctx->egl.display, egl_drm_formats[i], 0, NULL, NULL, &num_modifiers)) {
            wlm_log_error("egl::init(): failed to allocate dmabuf format array\n");
            success = false;
            break;
        }

        EGLuint64KHR * egl_drm_modifiers = calloc(num_modifiers, sizeof *egl_drm_modifiers);
        if (egl_drm_modifiers == NULL) {
            wlm_log_error("egl::init(): failed to allocate egl dmabuf format modifier array for format %x\n", egl_drm_formats[i]);
            success = false;
            break;
        }

        if (!ctx->egl.eglQueryDmaBufModifiersEXT(ctx->egl.display, egl_drm_formats[i], 0, (EGLuint64KHR*)egl_drm_modifiers, NULL, &num_modifiers)) {
            free(egl_drm_modifiers);
            wlm_log_error("egl::init(): failed query egl dmabuf format modifiers for format %x\n", egl_drm_formats[i]);
            success = false;
            break;
        }

        uint64_t * modifiers = calloc(num_modifiers, sizeof *modifiers);
        if (modifiers == NULL) {
            free(egl_drm_modifiers);
            wlm_log_error("egl::init(): failed to allocate modifier array for format %x\n", egl_drm_formats[i]);
            success = false;
            break;
        }

        for (size_t j = 0; j < (size_t)num_modifiers; j++) {
            modifiers[j] = egl_drm_modifiers[j];
        }

        dmabuf_formats[i].drm_format = egl_drm_formats[i];
        dmabuf_formats[i].num_modifiers = num_modifiers;
        dmabuf_formats[i].modifiers = modifiers;

        free(egl_drm_modifiers);
    }

    free(egl_drm_formats);

    // cleanup
    if (!success) {
        for (size_t i = 0; i < (size_t)num_formats; i++) {
            if (dmabuf_formats[i].modifiers != NULL) {
                free(dmabuf_formats[i].modifiers);
            }
        }
        free(dmabuf_formats);
        return false;
    }

    ctx->egl.dmabuf_formats.num_formats = num_formats;
    ctx->egl.dmabuf_formats.formats = dmabuf_formats;
    return true;
}

bool wlm_egl_check_errors(struct ctx * ctx, const char * msg) {
    GLenum error = glGetError();
    if (error == GL_NO_ERROR) return true;

    while (error != GL_NO_ERROR) {
        wlm_log_error("egl::check_errors(): %s: GL error 0x%x\n", msg, error);
        error = glGetError();
    }
    return false;

    (void)ctx;
}

// --- draw_frame ---

void wlm_egl_draw_frame(struct ctx * ctx) {
    // render frame, set swap interval to 0 to ensure nonblocking buffer swap
    wlm_egl_draw_texture(ctx);
    eglSwapInterval(ctx->egl.display, 0);
    if (eglSwapBuffers(ctx->egl.display, ctx->egl.surface) != EGL_TRUE) {
        wlm_log_error("egl::draw_frame(): failed to swap buffers\n");
        wlm_exit_fail(ctx);
    }
}

// --- draw_texture ---

void wlm_egl_draw_texture(ctx_t *ctx) {
    glBindTexture(GL_TEXTURE_2D, ctx->opt.freeze ? ctx->egl.freeze_texture : ctx->egl.texture);
    glClear(GL_COLOR_BUFFER_BIT);

    if (ctx->egl.texture_initialized) {
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }
}

// --- resize_viewport

void wlm_egl_resize_viewport(ctx_t * ctx) {
    wlm_log_debug(ctx, "egl::resize_viewport(): resizing viewport\n");

    uint32_t win_width = round(ctx->wl.width * ctx->wl.scale);
    uint32_t win_height = round(ctx->wl.height * ctx->wl.scale);
    uint32_t tex_width = ctx->egl.width;
    uint32_t tex_height = ctx->egl.height;
    uint32_t view_width = win_width;
    uint32_t view_height = win_height;

    // rotate texture dimensions by output transform
    if (ctx->egl.texture_initialized) {
        enum wl_output_transform transform = wlm_mirror_target_get_transform(ctx->mirror.current_target);
        wlm_util_viewport_apply_output_transform(&tex_width, &tex_height, transform);
    }

    // clamp texture dimensions to specified region
    region_t output_region;
    region_t clamp_region;
    if (ctx->egl.texture_initialized && ctx->opt.has_region && !ctx->egl.texture_region_aware) {
        // TODO: handle output regions differently
        // TODO: allow specifying regions relative to recording target

        output_region = (region_t){
            .x = 0, .y = 0,
            .width = tex_width, .height = tex_height
        };
        clamp_region = ctx->mirror.current_region;

        // HACK: calculate effective output fractional scale
        // wayland doesn't provide this information
        // TODO: figure out how to deal with this without hardcoding output targets
        double output_scale = 1;
        output_list_node_t * output_node = wlm_mirror_target_get_output_node(ctx->mirror.current_target);
        if (output_node != NULL) {
            output_scale = (double)tex_width / output_node->width;
        }
        wlm_util_region_scale(&clamp_region, output_scale);
        wlm_util_region_clamp(&clamp_region, &output_region);

        tex_width = clamp_region.width;
        tex_height = clamp_region.height;
    }

    // rotate texture dimensions by user transform
    wlm_util_viewport_apply_transform(&tex_width, &tex_height, ctx->opt.transform);

    // calculate aspect ratio
    double win_aspect = (double)win_width / win_height;
    double tex_aspect = (double)tex_width / tex_height;

    if (ctx->opt.scaling == SCALE_FIT) {
        // select biggest width or height that fits and preserves aspect ratio
        if (win_aspect > tex_aspect) {
            view_width = view_height * tex_aspect;
        } else if (win_aspect < tex_aspect) {
            view_height = view_width / tex_aspect;
        }
    } else if (ctx->opt.scaling == SCALE_COVER) {
        // select biggest width or height that covers and preserves aspect ratio
        if (win_aspect < tex_aspect) {
            view_width = view_height * tex_aspect;
        } else if (win_aspect > tex_aspect) {
            view_height = view_width / tex_aspect;
        }
    } else if (ctx->opt.scaling == SCALE_EXACT) {
        // select biggest fitting integer scale
        double width_scale = (double)win_width / tex_width;
        double height_scale = (double)win_height / tex_height;
        uint32_t upscale_factor = floorf(fminf(width_scale, height_scale));
        uint32_t downscale_factor = ceilf(fmaxf(1 / width_scale, 1 / height_scale));

        if (upscale_factor > 1) {
            wlm_log_debug(ctx, "egl::resize_viewport(): upscaling by factor = %d\n", upscale_factor);
            view_width = tex_width * upscale_factor;
            view_height = tex_height * upscale_factor;
        } else if (downscale_factor > 1) {
            wlm_log_debug(ctx, "egl::resize_viewport(): downscaling by factor = %d\n", downscale_factor);
            view_width = tex_width / downscale_factor;
            view_height = tex_height / downscale_factor;
        } else {
            view_width = tex_width;
            view_height = tex_height;
        }
    }

    wlm_log_debug(ctx, "egl::resize_viewport(): win_width = %d, win_height = %d\n", win_width, win_height);
    wlm_log_debug(ctx, "egl::resize_viewport(): view_width = %d, view_height = %d\n", view_width, view_height);

    // updating GL viewport
    wlm_log_debug(ctx, "egl::resize_viewport(): viewport %d, %d, %d, %d\n",
        (int32_t)(win_width - view_width) / 2, (int32_t)(win_height - view_height) / 2, view_width, view_height
    );
    glViewport((int32_t)(win_width - view_width) / 2, (int32_t)(win_height - view_height) / 2, view_width, view_height);

    // recalculate texture transform
    mat3_t texture_transform;
    wlm_util_mat3_identity(&texture_transform);
    if (ctx->egl.texture_initialized) {
        // apply transformations in reverse order as we need to transform
        // from OpenGL space to texture space

        wlm_util_mat3_apply_invert_y(&texture_transform, true);
        wlm_util_mat3_apply_transform(&texture_transform, ctx->opt.transform);

        if (ctx->opt.has_region && !ctx->egl.texture_region_aware) {
            wlm_util_mat3_apply_region_transform(&texture_transform, &clamp_region, &output_region);
        }

        enum wl_output_transform transform = wlm_mirror_target_get_transform(ctx->mirror.current_target);
        wlm_util_mat3_apply_output_transform(&texture_transform, transform);
        wlm_util_mat3_apply_invert_y(&texture_transform, ctx->mirror.invert_y);
    }

    // set texture transform matrix uniform
    // - GL matrices are stored in column-major order, so transpose the matrix
    wlm_util_mat3_transpose(&texture_transform);
    glUniformMatrix3fv(ctx->egl.texture_transform_uniform, 1, false, (float *)texture_transform.data);
}

// --- resize_window ---

void wlm_egl_resize_window(ctx_t * ctx) {
    uint32_t width = round(ctx->wl.width * ctx->wl.scale);
    uint32_t height = round(ctx->wl.height * ctx->wl.scale);
    wlm_log_debug(ctx, "egl::resize_window(): resizing EGL window to %dx%d\n", width, height);

    // resize window, then trigger viewport recalculation
    wl_egl_window_resize(ctx->egl.window, width, height, 0, 0);
    wlm_egl_resize_viewport(ctx);
}

// --- update_uniforms ---

void wlm_egl_update_uniforms(ctx_t * ctx) {
    // trigger viewport recalculation
    wlm_egl_resize_viewport(ctx);

    // set invert colors uniform
    bool invert_colors = ctx->opt.invert_colors;
    glUniform1i(ctx->egl.invert_colors_uniform, invert_colors);

    // set texture scaling mode
    glBindTexture(GL_TEXTURE_2D, ctx->egl.texture);
    if (ctx->opt.scaling_filter == SCALE_FILTER_LINEAR) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    glBindTexture(GL_TEXTURE_2D, ctx->egl.freeze_texture);
    if (ctx->opt.scaling_filter == SCALE_FILTER_LINEAR) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
}

// --- freeze_framebuffer ---

void wlm_egl_freeze_framebuffer(struct ctx * ctx) {
    glBindFramebuffer(GL_FRAMEBUFFER, ctx->egl.freeze_framebuffer);
    glBindTexture(GL_TEXTURE_2D, ctx->egl.freeze_texture);

    glCopyTexImage2D(GL_TEXTURE_2D, 0, ctx->egl.format, 0, 0, ctx->egl.width, ctx->egl.height, 0);
    wlm_egl_check_errors(ctx, "failed to copy frame to freeze texture");

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// --- cleanup_egl ---

void wlm_egl_cleanup(ctx_t *ctx) {
    if (!ctx->egl.initialized) return;

    wlm_log_debug(ctx, "egl::cleanup(): destroying EGL objects\n");

    if (ctx->egl.dmabuf_formats.formats != NULL) {
        for (size_t i = 0; i < ctx->egl.dmabuf_formats.num_formats; i++) {
            if (ctx->egl.dmabuf_formats.formats[i].modifiers != NULL) {
                free(ctx->egl.dmabuf_formats.formats[i].modifiers);
            }
        }
        free(ctx->egl.dmabuf_formats.formats);
    }

    if (ctx->egl.shader_program != 0) glDeleteProgram(ctx->egl.shader_program);
    if (ctx->egl.freeze_framebuffer != 0) glDeleteFramebuffers(1, &ctx->egl.freeze_framebuffer);
    if (ctx->egl.freeze_texture != 0) glDeleteTextures(1, &ctx->egl.freeze_texture);
    if (ctx->egl.texture != 0) glDeleteTextures(1, &ctx->egl.texture);
    if (ctx->egl.vbo != 0) glDeleteBuffers(1, &ctx->egl.vbo);
    if (ctx->egl.context != EGL_NO_CONTEXT) eglDestroyContext(ctx->egl.display, ctx->egl.context);
    if (ctx->egl.surface != EGL_NO_SURFACE) eglDestroySurface(ctx->egl.display, ctx->egl.surface);
    if (ctx->egl.window != EGL_NO_SURFACE) wl_egl_window_destroy(ctx->egl.window);
    if (ctx->egl.display != EGL_NO_DISPLAY) eglTerminate(ctx->egl.display);

    ctx->egl.initialized = false;
}
