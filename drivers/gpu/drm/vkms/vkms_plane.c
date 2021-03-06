// SPDX-License-Identifier: GPL-2.0
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "vkms_drv.h"
#include <drm/drm_plane_helper.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>

static const struct drm_plane_funcs vkms_plane_funcs = {
	.update_plane		= drm_atomic_helper_update_plane,
	.disable_plane		= drm_atomic_helper_disable_plane,
	.destroy		= drm_plane_cleanup,
	.reset			= drm_atomic_helper_plane_reset,
	.atomic_duplicate_state = drm_atomic_helper_plane_duplicate_state,
	.atomic_destroy_state	= drm_atomic_helper_plane_destroy_state,
};

static void vkms_primary_plane_update(struct drm_plane *plane,
				      struct drm_plane_state *old_state)
{
}

static int vkms_plane_atomic_check(struct drm_plane *plane,
				   struct drm_plane_state *state)
{
	struct drm_crtc_state *crtc_state;
	int ret;

	if (!state->fb | !state->crtc)
		return 0;

	crtc_state = drm_atomic_get_crtc_state(state->state, state->crtc);
	if (IS_ERR(crtc_state))
		return PTR_ERR(crtc_state);

	ret = drm_atomic_helper_check_plane_state(state, crtc_state,
						  DRM_PLANE_HELPER_NO_SCALING,
						  DRM_PLANE_HELPER_NO_SCALING,
						  false, true);
	if (ret != 0)
		return ret;

	/* for now primary plane must be visible and full screen */
	if (!state->visible)
		return -EINVAL;

	return 0;
}

static int vkms_prepare_fb(struct drm_plane *plane,
			   struct drm_plane_state *state)
{
	struct drm_gem_object *gem_obj;
	struct vkms_gem_object *vkms_obj;
	int ret;

	if (!state->fb)
		return 0;

	gem_obj = drm_gem_fb_get_obj(state->fb, 0);
	vkms_obj = drm_gem_to_vkms_gem(gem_obj);
	ret = vkms_gem_vmap(gem_obj);
	if (ret)
		DRM_ERROR("vmap failed: %d\n", ret);

	return drm_gem_fb_prepare_fb(plane, state);
}

static void vkms_cleanup_fb(struct drm_plane *plane,
			    struct drm_plane_state *old_state)
{
	struct drm_gem_object *gem_obj;

	if (!old_state->fb)
		return;

	gem_obj = drm_gem_fb_get_obj(old_state->fb, 0);
	vkms_gem_vunmap(gem_obj);
}

static const struct drm_plane_helper_funcs vkms_primary_helper_funcs = {
	.atomic_update		= vkms_primary_plane_update,
	.atomic_check		= vkms_plane_atomic_check,
	.prepare_fb		= vkms_prepare_fb,
	.cleanup_fb		= vkms_cleanup_fb,
};

struct drm_plane *vkms_plane_init(struct vkms_device *vkmsdev)
{
	struct drm_device *dev = &vkmsdev->drm;
	struct drm_plane *plane;
	const u32 *formats;
	int ret, nformats;

	plane = kzalloc(sizeof(*plane), GFP_KERNEL);
	if (!plane)
		return ERR_PTR(-ENOMEM);

	formats = vkms_formats;
	nformats = ARRAY_SIZE(vkms_formats);

	ret = drm_universal_plane_init(dev, plane, 0,
				       &vkms_plane_funcs,
				       formats, nformats,
				       NULL, DRM_PLANE_TYPE_PRIMARY, NULL);
	if (ret) {
		kfree(plane);
		return ERR_PTR(ret);
	}

	drm_plane_helper_add(plane, &vkms_primary_helper_funcs);

	return plane;
}
