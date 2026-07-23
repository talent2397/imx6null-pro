/**
 * @file    v4l2_capture.c
 * @brief   用 V4L2 API 从 USB 摄像头抓一帧 (USERPTR 模式)
 *
 * 编译:
 *   arm-linux-gnueabihf-gcc -std=gnu99 -O2 -Wall -o v4l2_capture v4l2_capture.c -static
 * 用法:
 *   ./v4l2_capture /dev/video1 640 480 capture.jpg
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>

#define NB_BUF  4

static void die(const char *msg)
{
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[])
{
    if (argc < 5) {
        fprintf(stderr, "用法: %s <设备> <宽> <高> <输出文件>\n", argv[0]);
        return 1;
    }
    const char *dev   = argv[1];
    int  w            = atoi(argv[2]);
    int  h            = atoi(argv[3]);
    const char *out   = argv[4];
    int  buf_size     = w * h * 2;  /* MJPG 压缩帧不会超过这个 */

    /* 1. 打开设备 */
    int fd = open(dev, O_RDWR);
    if (fd < 0) die("open");

    /* 2. 查询能力 */
    struct v4l2_capability cap;
    memset(&cap, 0, sizeof(cap));
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) die("VIDIOC_QUERYCAP");
    printf("设备: %s | 驱动: %s\n", cap.card, cap.driver);

    /* 3. 设置 MJPG 格式 */
    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width       = w;
    fmt.fmt.pix.height      = h;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    fmt.fmt.pix.field       = V4L2_FIELD_ANY;
    if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) die("VIDIOC_S_FMT");
    printf("格式: %ux%u MJPEG\n", fmt.fmt.pix.width, fmt.fmt.pix.height);

    /* 4. 申请 USERPTR 缓冲区 */
    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    req.count  = NB_BUF;
    req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_USERPTR;
    if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0) die("VIDIOC_REQBUFS(USERPTR)");
    printf("USERPTR 缓冲区: %u 个\n", req.count);

    /* 5. 分配用户态内存并入队 */
    void *bufs[NB_BUF];
    for (unsigned int i = 0; i < req.count; i++) {
        bufs[i] = malloc(buf_size);
        if (!bufs[i]) die("malloc");

        struct v4l2_buffer vbuf;
        memset(&vbuf, 0, sizeof(vbuf));
        vbuf.type      = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        vbuf.memory    = V4L2_MEMORY_USERPTR;
        vbuf.index     = i;
        vbuf.length    = buf_size;
        vbuf.m.userptr = (unsigned long)bufs[i];

        if (ioctl(fd, VIDIOC_QBUF, &vbuf) < 0) die("VIDIOC_QBUF");
    }

    /* 6. 开始视频流 */
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_STREAMON, &type) < 0) die("VIDIOC_STREAMON");
    printf("流已启动, 等待帧...\n");

    /* 7. 抓 3 帧, 只保留最后一帧 (前 2 帧用于 AE/AWB 稳定) */
    struct v4l2_buffer vbuf;
    int saved_bytes = 0;
    for (int i = 0; i < 3; i++) {
        memset(&vbuf, 0, sizeof(vbuf));
        vbuf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        vbuf.memory = V4L2_MEMORY_USERPTR;

        if (ioctl(fd, VIDIOC_DQBUF, &vbuf) < 0) die("VIDIOC_DQBUF");
        printf("  帧%d: %u 字节\n", i + 1, vbuf.bytesused);

        if (i == 2) {
            FILE *fp = fopen(out, "wb");
            if (!fp) die("fopen");
            saved_bytes = (int)fwrite((void *)(unsigned long)vbuf.m.userptr,
                                      1, vbuf.bytesused, fp);
            fclose(fp);
        }

        /* USERPTR 模式: 数据写入了我们 malloc 的内存,
         * 重新入队时 userptr 不变, 但需要通知内核 */
        if (ioctl(fd, VIDIOC_QBUF, &vbuf) < 0) die("VIDIOC_QBUF(requeue)");
    }

    /* 8. 停止视频流 */
    if (ioctl(fd, VIDIOC_STREAMOFF, &type) < 0) die("VIDIOC_STREAMOFF");
    printf("已保存 %s (%d 字节)\n", out, saved_bytes);

    /* 9. 清理 */
    for (unsigned int i = 0; i < req.count; i++) free(bufs[i]);
    close(fd);
    return 0;
}
