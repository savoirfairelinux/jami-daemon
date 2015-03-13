#include <glib.h>

#ifndef _VIDEO_CAPABILITIES_H
#define _VIDEO_CAPABILITIES_H

typedef GNode VideoCapabilities;

VideoCapabilities *video_capabilities_new(const gchar *name);

void video_capabilities_free(VideoCapabilities *cap);

gchar **video_capabilities_get_channels(VideoCapabilities *cap);
gchar **video_capabilities_get_sizes(VideoCapabilities *cap, const gchar *channel);
gchar **video_capabilities_get_rates(VideoCapabilities *cap, const gchar *channel, const gchar *size);

#endif /* _VIDEO_CAPABILITIES_H */
