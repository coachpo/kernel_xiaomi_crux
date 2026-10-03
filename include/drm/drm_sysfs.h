/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _DRM_SYSFS_H_
#define _DRM_SYSFS_H_

#include <linux/types.h>

struct drm_device;
struct drm_connector;
#ifdef CONFIG_MACH_XIAOMI_CRUX
int drm_crux_set_fod_dimlayer(struct drm_connector *connector, bool enabled);
int drm_crux_get_fod_dimlayer(struct drm_connector *connector);
int drm_crux_get_fod_ui_ready(struct drm_connector *connector);
int drm_crux_get_doze_backlight(struct drm_connector *connector);
int drm_crux_set_doze_backlight(struct drm_connector *connector, u32 value);
int drm_crux_set_disp_param(struct drm_connector *connector, u32 param);
#endif
struct device;

int drm_class_device_register(struct device *dev);
void drm_class_device_unregister(struct device *dev);

void drm_sysfs_hotplug_event(struct drm_device *dev);

#endif
