#ifndef VIDEO_DAEMON_CTRL_H
#define VIDEO_DAEMON_CTRL_H

#include <stdbool.h>
#include <sys/types.h>

int connect_ctrl_client(const char *path);

void start_ctrl_loop();

void stop_ctrl_loop();

void report_video_format(bool ready, const char *error, u_int16_t width, u_int16_t height, double frame_per_second);

#endif //VIDEO_DAEMON_CTRL_H
