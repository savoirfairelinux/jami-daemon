#include <glib.h>

#ifndef _VIDEO_DEVICE_H
#define _VIDEO_DEVICE_H

struct video_device;
typedef struct video_device VideoDevice;

VideoDevice *video_device_new(const gchar *name);

void video_device_free(VideoDevice *dev);

gchar **video_device_get_channels(VideoDevice *dev);
gchar **video_device_get_sizes(VideoDevice *dev, const gchar *channel);
gchar **video_device_get_rates(VideoDevice *dev, const gchar *channel, const gchar *size);

gchar *video_device_get_prefered_channel(VideoDevice *dev);
gchar *video_device_get_prefered_size(VideoDevice *dev);
gchar *video_device_get_prefered_rate(VideoDevice *dev);

void video_device_set_prefered_channel(VideoDevice *dev, const gchar *channel);
void video_device_set_prefered_size(VideoDevice *dev, const gchar *size);
void video_device_set_prefered_rate(VideoDevice *dev, const gchar *rate);

void video_device_save_preferences(VideoDevice *dev);

#endif /* _VIDEO_DEVICE_H */
