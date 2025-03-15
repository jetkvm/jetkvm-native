#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <getopt.h>

#include "rk_mpi_sys.h"
#include "rk_common.h"
#include "ctrl.h"
#include "video.h"
#include "screen.h"

#include <sys/ioctl.h>
#include <linux/nbd.h>

volatile bool running = true;

void handle_signal(int signal)
{
    if (running == false)
    {
        return;
    }
    running = false;
    printf("Caught signal %d, stopping\n", signal);
    stop_ctrl_loop();
    printf("Stopped control loop\n");
    video_shutdown();
    printf("Video system shut down\n");
}

int main(int argc, char **argv)
{
    int opt;
    int log_status = 0;
    int ioctl = 0;
    struct option long_options[] = {
        {"log-status", no_argument, &log_status, 1},
        {"ioctl", no_argument, &ioctl, 1},
        {0, 0, 0, 0}};

    while ((opt = getopt_long(argc, argv, "", long_options, NULL)) != -1)
    {
        if (opt == '?')
        {
            fprintf(stderr, "Usage: %s [--log-status]\n", argv[0]);
            fprintf(stderr, "  --log-status    Log video status\n");
            exit(EXIT_FAILURE);
        }
    }

    if (log_status)
    {
        const char *status = videoc_log_status();
        if (status != NULL)
        {
            printf("Video status:\n%s\n", status);
            free((void *)status);
        }
        else
        {
            fprintf(stderr, "Failed to get video status\n");
        }
        return 0;
    }
    if (ioctl)
    {
        printf("NEGOTIATION_IOCTL_SET_SOCK: %d\n", NBD_SET_SOCK);
        printf("NEGOTIATION_IOCTL_SET_BLOCKSIZE: %d\n", NBD_SET_BLKSIZE);
        printf("NEGOTIATION_IOCTL_SET_SIZE_BLOCKS: %d\n", NBD_SET_SIZE_BLOCKS);
        printf("NEGOTIATION_IOCTL_DO_IT: %d\n", NBD_DO_IT);
        printf("NEGOTIATION_IOCTL_SET_TIMEOUT: %d\n", NBD_SET_TIMEOUT);
        printf("TRANSMISSION_IOCTL_DISCONNECT: %d\n", NBD_DISCONNECT);
        printf("TRANSMISSION_IOCTL_CLEAR_SOCK: %d\n", NBD_CLEAR_SOCK);
        printf("TRANSMISSION_IOCTL_CLEAR_QUE: %d\n", NBD_CLEAR_QUE);
        return 0;
    }

    init_lvgl();

    if (connect_ctrl_client("/var/run/jetkvm_ctrl.sock") != 0)
    {
        printf("can not connect to ctrl server\n");
        return -1;
    }
    start_ctrl_loop();

    printf("creating screen_thread\n");
    pthread_t screen_thread;
    pthread_create(&screen_thread, NULL, run_lvgl_loop, NULL);

    if (RK_MPI_SYS_Init() != RK_SUCCESS)
    {
        RK_LOGE("RK_MPI_SYS_Init fail!");
        exit(-1);
    }
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    if (video_init() != 0)
    {
        printf("Error initializing video\n");
        return -1;
    }
    printf("video system inited\n");

    video_start_streaming();
    while (running)
    {
        // TODO: use pthread primitives
        //  Sleep for a short interval to avoid busy-waiting
        usleep(1000000); // Sleep for 100ms
    }
    printf("RK_MPI_SYS_Exit()\n");
    RK_MPI_SYS_Exit();
    printf("video system exited\n");
    return 0;
}
