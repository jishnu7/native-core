/* @license
 * This file is part of the Game Closure SDK.
 *
 * The Game Closure SDK is free software: you can redistribute it and/or modify
 * it under the terms of the Mozilla Public License v. 2.0 as published by Mozilla.

 * The Game Closure SDK is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * Mozilla Public License v. 2.0 for more details.

 * You should have received a copy of the Mozilla Public License v. 2.0
 * along with the Game Closure SDK.  If not, see <http://mozilla.org/MPL/2.0/>.
 */

/**
 * @file tealeaf_canvas.c
 * @brief
 */
#include "core/tealeaf_canvas.h"
#include "core/texture_2d.h"
#include "core/texture_manager.h"
#include "core/tealeaf_context.h"
#include "core/draw_textures.h"
#include "core/config.h"
#include "core/log.h"
#include "core/geometry.h"

static tealeaf_canvas canvas;

/**
 * @name tealeaf_canvas_get
 * @brief
 * @retval tealeaf_canvas* -
 */
tealeaf_canvas *tealeaf_canvas_get() {
    return &canvas;
}

void tealeaf_canvas_set_defaults() {
    LOG("{canvas} Initializing Canvas with Defaults");
    int width = config_get_screen_width();
    int height = config_get_screen_height();

    canvas.offscreen_framebuffer = 0;
    canvas.view_framebuffer = 0;

    canvas.onscreen_ctx = context_2d_init(&canvas, "onscreen", -1, true);
    canvas.onscreen_ctx->width = width;
    canvas.onscreen_ctx->height = height;
    canvas.onscreen_ctx->backing_width = width;
    canvas.onscreen_ctx->backing_height = height;
    canvas.active_ctx = 0;
}

/**
 * @name   tealeaf_canvas_init
 * @brief  initilizes the tealeaf canvas object
 * @param  framebuffer_name - (int) gl id of the onscreen framebuffer
 * @retval NONE
 */
void tealeaf_canvas_init(int framebuffer_name, int tex_name) {
    LOG("{canvas} Initializing Canvas");

    int width = config_get_screen_width();
    int height = config_get_screen_height();

    GLuint offscreen_buffer_name;
    GLTRACE(glGenFramebuffers(1, &offscreen_buffer_name));

    if (canvas.onscreen_ctx != NULL) {
        // The onscreen_ctx is now created initially in
        // tealeaf_canvas_set_defaults. We might be able to just reuse the
        // struct. For now, keep it simple and repeat the initialization
        // procedure.
        free(canvas.onscreen_ctx);
    }

    if (tex_name) {
        canvas.offscreen_framebuffer = offscreen_buffer_name;
        canvas.view_framebuffer = offscreen_buffer_name;
        canvas.onscreen_ctx = context_2d_init(&canvas, "onscreen", tex_name, true);
        canvas.onscreen_ctx->width = width;
        canvas.onscreen_ctx->height = height;
        canvas.onscreen_ctx->backing_width = width;
        canvas.onscreen_ctx->backing_height = height;
        canvas.active_ctx = 0;
    } else {
        canvas.offscreen_framebuffer = offscreen_buffer_name;
        canvas.view_framebuffer = framebuffer_name;
        canvas.onscreen_ctx = context_2d_init(&canvas, "onscreen", -1, true);
        canvas.onscreen_ctx->width = width;
        canvas.onscreen_ctx->height = height;
        canvas.active_ctx = 0;

        tealeaf_canvas_context_2d_bind(canvas.onscreen_ctx);
    }
}

/**
 * @name   tealeaf_canvas_bind_texture_buffer
 * @brief  binds the given context's texture backing to gl to draw to
 * @param  ctx - (context_2d *) pointer to the context to bind
 * @retval NONE
 */
void tealeaf_canvas_bind_texture_buffer(context_2d *ctx) {
    LOGFN("tealeaf_canvas_bind_texture_buffer");
   /* texture_2d *tex = texture_manager_get_texture(texture_manager_get(), ctx->url);

    if (!tex) {
        return;
    }
    */

    GLTRACE(glBindTexture(GL_TEXTURE_2D, ctx->destTex));
    GLTRACE(glBindFramebuffer(GL_FRAMEBUFFER, canvas.offscreen_framebuffer));
    GLTRACE(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ctx->destTex, 0));
    canvas.framebuffer_width = ctx->width;//tex->originalWidth;
    canvas.framebuffer_height = ctx->height;//tex->originalHeight;
    canvas.framebuffer_offset_bottom = 0;//tex->height - tex->originalHeight;

}

/**
 * @name   tealeaf_canvas_bind_render_buffer
 * @brief  bind's the render buffer and set's it's height / width to the given context's props
 * @param  ctx - (context_2d *) pointer to the context to use the width / height from
 * @retval NONE
 */
void tealeaf_canvas_bind_render_buffer(context_2d *ctx) {
    LOGFN("tealeaf_canvas_bind_render_buffer");
    GLTRACE(glBindFramebuffer(GL_FRAMEBUFFER, canvas.view_framebuffer));
    canvas.framebuffer_width = ctx->width;
    canvas.framebuffer_height = ctx->height;
    canvas.framebuffer_offset_bottom = 0;
}

/**
 * @name   tealeaf_canvas_context_2d_bind
 * @brief  uses the given texture to bind to either the render buffer or a fbo
 * @param  ctx - (context_2d *) pointer to the context to use for binding
 * @retval  bool - whether the bind failed or succeeded
 */
bool tealeaf_canvas_context_2d_bind(context_2d *ctx) {
    if (canvas.active_ctx != ctx) {
        draw_textures_flush();

        // Update active context after flushing
        canvas.active_ctx = ctx;

        if (ctx->destTex == -1) {
            tealeaf_canvas_bind_render_buffer(ctx);
        } else {
            tealeaf_canvas_bind_texture_buffer(ctx);
            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
                LOG("{canvas} WARNING: Failed to make complete framebuffer %i", glCheckFramebufferStatus(GL_FRAMEBUFFER));
            }
        }

        tealeaf_context_update_viewport(ctx, true);

        return true;
    } else {
        return false;
    }
}

/**
 * @name   tealeaf_canvas_resize
 * @brief  resize's the onscreen canvas
 * @param  w - (int) width to resize to
 * @param  h - (int) height to resize to
 * @retval NONE
 */
void tealeaf_canvas_resize(int w, int h) {
    LOG("{canvas} Resizing screen to (%d, %d)", w, h);

    context_2d *ctx = canvas.onscreen_ctx;
    context_2d_resize(ctx, w, h);

    if (canvas.active_ctx == canvas.onscreen_ctx) {
        tealeaf_canvas_bind_texture_buffer(ctx);
        tealeaf_context_update_viewport(ctx, true);
        context_2d_clear(ctx);
    }

    GLTRACE(glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA));
    GLTRACE(glEnable(GL_BLEND));
    config_set_screen_width(w);
    config_set_screen_height(h);
    canvas.should_resize = true;
}

