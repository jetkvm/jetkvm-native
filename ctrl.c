#include <stdio.h>
#include <string.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>
#include <fcntl.h>
#include "frozen.h"
#include "video.h"
#include "screen.h"
#include "edid.h"
#include "lvgl/lvgl.h"

typedef struct
{
    bool ready;
    const char *error;
    u_int16_t width;
    u_int16_t height;
    double frame_per_second;
} state_t;

state_t state;

struct sockaddr_un addr;

int ctrl_client_fd = 0;

int write_json(int fd, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    char *json_str = json_vasprintf(fmt, ap);
    va_end(ap);
    if (json_str == NULL)
    {
        return -1;
    }
    int written = write(fd, json_str, strlen(json_str));
    free(json_str);
    return written;
}

int write_json_error(int fd, int seq, const char *error)
{
    return write_json(fd, "{seq: %d, error: %Q, errno: %d}", seq, error, errno);
}

static int write_input_video_state(int fd)
{
    return write_json(fd, "{event: %Q, data: {ready: %B, error: %Q, width: %u, height:%u, frame_per_second: %f}}",
                      "video_input_state", state.ready, state.error, state.width, state.height, state.frame_per_second);
}

void report_video_format(bool ready, const char *error, u_int16_t width, u_int16_t height, double frame_per_second)
{
    state.ready = ready;
    state.error = error;
    state.width = width;
    state.height = height;
    state.frame_per_second = frame_per_second;
    if (ctrl_client_fd > 0)
    {
        if (write_input_video_state(ctrl_client_fd) < 0)
        {
            perror("error writing video state");
        }
    }
}

/**
 * @brief Convert a hexadecimal string to an array of uint8_t bytes
 *
 * @param hex_str The input hexadecimal string
 * @param bytes The output byte array (must be pre-allocated)
 * @param max_len The maximum number of bytes that can be stored in the output array
 * @return int The number of bytes converted, or -1 on error
 */
int hex_to_bytes(const char *hex_str, uint8_t *bytes, size_t max_len)
{
    size_t hex_len = strnlen(hex_str, 4096);
    if (hex_len % 2 != 0 || hex_len / 2 > max_len)
    {
        return -1; // Invalid input length or insufficient output buffer
    }

    for (size_t i = 0; i < hex_len; i += 2)
    {
        char byte_str[3] = {hex_str[i], hex_str[i + 1], '\0'};
        char *end_ptr;
        long value = strtol(byte_str, &end_ptr, 16);

        if (*end_ptr != '\0' || value < 0 || value > 255)
        {
            return -1; // Invalid hexadecimal value
        }

        bytes[i / 2] = (uint8_t)value;
    }

    return hex_len / 2;
}

/**
 * @brief Convert an array of uint8_t bytes to a hexadecimal string, user must free the returned string
 *
 * @param bytes The input byte array
 * @param len The number of bytes in the input array
 * @return char* The output hexadecimal string (dynamically allocated, must be freed by the caller), or NULL on error
 */
const char *bytes_to_hex(const uint8_t *bytes, size_t len)
{
    if (bytes == NULL || len == 0)
    {
        return NULL;
    }

    char *hex_str = malloc(2 * len + 1); // Each byte becomes 2 hex chars, plus null terminator
    if (hex_str == NULL)
    {
        return NULL; // Memory allocation failed
    }

    for (size_t i = 0; i < len; i++)
    {
        snprintf(hex_str + (2 * i), 3, "%02x", bytes[i]);
    }

    hex_str[2 * len] = '\0'; // Ensure null termination
    return hex_str;
}

void handle_lvgl_call(const int seq, const char *method, const char *json, size_t json_len)
{
    lv_obj_t *obj = NULL;
    char *obj_name = NULL;

    if (strcmp(method, "lv_disp_set_rotation") == 0)
    {
        char *rotation = NULL;
        if (json_scanf(json, json_len, "{params: {rotation: %Q}}", &rotation) > 0)
        {
            lv_disp_rot_t rot;
            if (strcmp(rotation, "90") == 0)
            {
                rot = LV_DISP_ROT_90;
            }
            else if (strcmp(rotation, "270") == 0)
            {
                rot = LV_DISP_ROT_270;
            }
            else
            {
                rot = -1;
            }
            free(rotation);
            if (rot != -1)
            {
                lv_disp_set_rotation(NULL, rot);
                write_json(ctrl_client_fd, "{seq: %d}", seq);
            }
            else
            {
                write_json_error(ctrl_client_fd, seq, "illegal rotation value");
            }
        }
        else
        {
            write_json_error(ctrl_client_fd, seq, "missing rotation parameter");
        }
    }
    else if (json_scanf(json, json_len, "{params: {obj: %Q}}", &obj_name) > 0)
    {
        obj = ui_get_obj(obj_name);
        free(obj_name);

        if (obj == NULL)
        {
            write_json_error(ctrl_client_fd, seq, "object not found");
            return;
        }

        if (strcmp(method, "lv_scr_load") == 0)
        {
            if (lv_scr_act() != obj)
            {
                lv_scr_load(obj);
            }
            write_json(ctrl_client_fd, "{seq: %d}", seq);
        }
        else if (strcmp(method, "lv_label_set_text") == 0)
        {
            char *text = NULL;
            if (json_scanf(json, json_len, "{params: {text: %Q}}", &text) > 0)
            {
                lv_label_set_text(obj, text);
                free(text);
                write_json(ctrl_client_fd, "{seq: %d}", seq);
            }
            else
            {
                write_json_error(ctrl_client_fd, seq, "missing text parameter");
            }
        }
        else if (strcmp(method, "lv_img_set_src") == 0)
        {
            char *src = NULL;
            if (json_scanf(json, json_len, "{params: {src: %Q}}", &src) > 0)
            {
                lv_img_dsc_t *img = ui_get_image(src);
                free(src);
                if (img != NULL)
                {
                    lv_img_set_src(obj, img);
                    write_json(ctrl_client_fd, "{seq: %d}", seq);
                }
                else
                {
                    write_json_error(ctrl_client_fd, seq, "image source not found");
                }
            }
            else
            {
                write_json_error(ctrl_client_fd, seq, "missing src parameter");
            }
        }
        else if (strcmp(method, "lv_obj_set_state") == 0)
        {
            char *state = NULL;
            if (json_scanf(json, json_len, "{params: {state: %Q}}", &state) > 0)
            {
                lv_obj_add_state(obj, LV_STATE_USER_1);
                lv_state_t state_val = LV_STATE_DEFAULT;
                if (strcmp(state, "LV_STATE_USER_1") == 0)
                {
                    state_val = LV_STATE_USER_1;
                }
                else if (strcmp(state, "LV_STATE_USER_2") == 0)
                {
                    state_val = LV_STATE_USER_2;
                }
                free(state);
                lv_obj_clear_state(obj, LV_STATE_USER_1 | LV_STATE_USER_2);
                lv_obj_add_state(obj, state_val);
                write_json(ctrl_client_fd, "{seq: %d}", seq);
            }
            else
            {
                write_json_error(ctrl_client_fd, seq, "missing state parameter");
            }
        }
    }
    else
    {
        printf("handle_lvgl_call: No object name provided for method %s\n", method);
    }
}

void *handle_client(void *arg)
{
    const int BUF_SIZE = 1024;
    char buf_in[BUF_SIZE];

    // TODO: fix memory leak caused by continue
    while (true)
    {
        ssize_t n = read(ctrl_client_fd, buf_in, BUF_SIZE);
        if (n > 0)
        {
            buf_in[n] = '\0';
            char *method;
            int32_t seq = -1;
            if (json_scanf(buf_in, n, "{action: %Q, seq: %d}", &method, &seq) > 0)
            {
                if (strcmp("start_video", method) == 0)
                {
                    video_start_streaming();
                    write_json(ctrl_client_fd, "{seq: %d}", seq);
                }
                else if (strcmp("stop_video", method) == 0)
                {
                    video_stop_streaming();
                    write_json(ctrl_client_fd, "{seq: %d}", seq);
                }
                else if (strcmp("set_video_quality_factor", method) == 0)
                {
                    float quality_factor;
                    if (json_scanf(buf_in, n, "{params: {quality_factor: %f}}", &quality_factor) > 0)
                    {
                        if (quality_factor < 0 || quality_factor > 1)
                        {
                            write_json_error(ctrl_client_fd, seq, "invalid quality factor");
                        }
                        else
                        {
                            video_set_quality_factor(quality_factor);
                            write_json(ctrl_client_fd, "{seq: %d}", seq);
                        }
                    }
                    else
                    {
                        write_json_error(ctrl_client_fd, seq, "invalid quality factor");
                    }
                }
                // if method starts with lv_, call the corresponding lvgl function
                else if (strncmp("lv_", method, 3) == 0)
                {
                    handle_lvgl_call(seq, method, buf_in, n);
                }
                else if (strcmp("ui_set_text", method) == 0)
                {
                    char *name;
                    char *text;
                    if (json_scanf(buf_in, n, "{name: %Q, text: %Q}", &name, &text) > 0)
                    {
                        ui_set_text(name, text);
                        free(name);
                        free(text);
                    }
                    if (seq > 0)
                    {
                        write_json(ctrl_client_fd, "{seq: %d}", seq);
                    }
                }
                else if (strcmp("set_edid", method) == 0)
                {
                    uint8_t edid[256];
                    char *edid_hex;
                    if (json_scanf(buf_in, n, "{params: {edid: %Q} }", &edid_hex) > 0)
                    {
                        int edid_len = hex_to_bytes(edid_hex, edid, 256);
                        free(edid_hex);

                        if (edid_len < 0)
                        {
                            write_json_error(ctrl_client_fd, seq, "invalid edid");
                            continue;
                        }
                        if (set_edid(edid, edid_len) < 0)
                        {
                            write_json_error(ctrl_client_fd, seq, "failed to set EDID");
                        }
                        write_json(ctrl_client_fd, "{seq: %d}", seq);
                    }
                    else
                    {
                        write_json_error(ctrl_client_fd, seq, "invalid edid");
                    }
                }
                else if (strcmp("get_edid", method) == 0)
                {
                    uint8_t edid[256];
                    int edid_len = get_edid(edid, 256);
                    if (edid_len < 0)
                    {
                        write_json_error(ctrl_client_fd, seq, "failed to get EDID");
                        continue;
                    }

                    char *edid_hex = bytes_to_hex(edid, edid_len);
                    if (edid_hex == NULL)
                    {
                        write_json_error(ctrl_client_fd, seq, "failed to convert EDID to hex");
                        free(edid_hex);
                        continue;
                    }

                    write_json(ctrl_client_fd, "{seq: %d, result: {edid: %Q}}", seq, edid_hex);
                    free(edid_hex);
                }
                free(method);
            }
        }
        else if (n == 0)
        {
            // EOF
            break;
        }
        else
        {
            perror("error read from ctrl client");
            break;
        }
    }

    close(ctrl_client_fd);
    return NULL;
}

int connect_ctrl_client(const char *path)
{
    int client_fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (client_fd == -1)
    {
        perror("can not create socket");
        return -1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

    if (connect(client_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        perror("can not connect to ctrl server");
        close(client_fd);
        return -1;
    }
    ctrl_client_fd = client_fd;
    return 0;
}

pthread_t ctrl_thread;

void start_ctrl_loop()
{
    if (pthread_create(&ctrl_thread, NULL, (void *(*)(void *))handle_client, NULL) != 0)
    {
        perror("Failed to create ctrl_thread");
    }
}

void stop_ctrl_loop()
{
    if (ctrl_client_fd != 0)
    {
        if (shutdown(ctrl_client_fd, SHUT_RDWR) == -1)
        {
            perror("Error shutting down ctrl client socket");
        }
        if (close(ctrl_client_fd) == -1)
        {
            perror("Error closing client socket");
        }
        ctrl_client_fd = 0;
    }
}
