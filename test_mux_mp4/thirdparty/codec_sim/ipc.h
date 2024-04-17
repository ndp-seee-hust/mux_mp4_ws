

#ifndef IPC_H
#define IPC_H

#include <stdint.h>

typedef enum
{
    AUDIO_AAC,
    AUDIO_G711,
} AUDIO_CODEC;

typedef enum
{
    H264,
    H265,
} VIDEO_CODEC;
enum
{
    EVENT_CAPTURE_PICTURE_SUCCESS,
    EVENT_CAPTURE_PICTURE_FAIL,
    EVENT_MOTION_DETECTION,
    EVENT_MOTION_DETECTION_DISAPEER,
};

enum
{
    FRAME_TYPE_AUDIO,
    FRAME_TYPE_VIDEO
};

typedef struct
{
    uint8_t frame_type;
    uint8_t *frame;
    uint32_t len;
    int64_t timestamp;
} frame_info_t;

typedef struct
{
    int audio_codec;
    int video_codec;
    int video_fps;
    int audio_sample;

    char *video_file;
    char *pic_file;
    char *audio_file;
    int (*video_cb)(uint8_t *frame, int len, int iskey, int64_t timestamp);
    int (*audio_cb)(uint8_t *frame, int len, int64_t timestamp);
    int (*event_cb)(int event, void *data);
} ipc_param_t;

typedef struct ipc_dev_t
{
    int (*init)(struct ipc_dev_t *ipc, ipc_param_t *param);
    void (*run)(struct ipc_dev_t *ipc);
    int (*capture_picture)(struct ipc_dev_t *ipc, char *file);
    void (*deinit)(struct ipc_dev_t *ipc);
    void *priv;
} ipc_dev_t;

extern int ipc_init(ipc_param_t *param);
extern void ipc_run();
extern int ipc_dev_register(ipc_dev_t *dev);
extern int ipc_capture_picture(char *file);

extern ipc_dev_t sim_ipc;

#endif /*IPC_H*/
