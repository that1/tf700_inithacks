#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

//#include <cutils/klog.h>

// this is from system/core/libcutils/klog.c
static int klog_fd = -1;

void klog_init(void)
{
    static const char *name = "/dev/__kmsg__";
    if (mknod(name, S_IFCHR | 0600, (1 << 8) | 11) == 0) {
        klog_fd = open(name, O_WRONLY);
        fcntl(klog_fd, F_SETFD, FD_CLOEXEC);
        unlink(name);
    }
}

/*
#define KLOG_ERROR(tag,x...)   klog_write(3, "<3>" tag ": " x)
#define KLOG_WARNING(tag,x...) klog_write(4, "<4>" tag ": " x)
#define KLOG_NOTICE(tag,x...)  klog_write(5, "<5>" tag ": " x)
#define KLOG_INFO(tag,x...)    klog_write(6, "<6>" tag ": " x)
#define KLOG_DEBUG(tag,x...)   klog_write(7, "<7>" tag ": " x)

#define ERROR(x...)   KLOG_ERROR("init", x)
#define NOTICE(x...)  KLOG_NOTICE("init", x)
#define INFO(x...)    KLOG_INFO("init", x)
*/
#define KLOG(x) write(klog_fd, x, sizeof(x))

int main(int argc, char *argv[], char *envp[])
{
    int rc;
/*
    printf("hello world, this is that-init. my pid = %d\n", getpid());
    int fd = open("log.txt", O_WRONLY | O_CREAT, 0600);
    if (fd != -1)
    {
	write(fd, "hello\n", 6);
	close(fd);
    }
*/
    mount("sysfs", "/sys", "sysfs", 0, NULL);
    klog_init();

    struct stat s;
    rc = stat("/sys/block/mmcblk1/mmcblk1p2", &s);
//    printf("stat returned %d\n", rc);
//    sleep(5);
    if (rc == 0)
    {
	KLOG("<6>init: mmcblk1p2 detected, setting up for data2sd");
	umount("/data");
        rename("fstab.cardhu.data2sd", "fstab.cardhu");
        rename("init.cardhu.rc.data2sd", "init.cardhu.rc");
    }
    else
    {
	KLOG("<6>init: mmcblk1p2 not detected, setting up for internal storage");
//        printf("stat returned errno %d\n", errno);
//        sleep(5);
	unlink("fstab.cardhu.data2sd");
	unlink("init.cardhu.rc.data2sd");
    }

    // this rename is essential, or the symlink sbin/ueventd must be changed!
    rename("init-android", "init");
    rc = execve("/init", argv, envp);

    printf("argh! rc=%d errno=%d", rc, errno);
    sleep(5);
    return 0;
}
