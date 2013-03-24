// startinit.c - unmount /system after preinit and run real init

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
*/
#define KLOG(x) write(klog_fd, x, strlen(x))

#define PREINIT_SYSTEM_DEVNAME "/dev/mmcblk0p1"

int main(int argc, char *argv[], char *envp[])
{
	int rc;
//	char buf[80];
	
	KLOG("<4>init-chainload: unmounting /system");
	// but first clean up the mess we made :)
	umount("/system");
	unlink(PREINIT_SYSTEM_DEVNAME);

	KLOG("<4>init-chainload: running real init");
	rc = execve("/init", argv, envp);

	printf("argh! rc=%d errno=%d", rc, errno);
	sleep(5);
	return 0;
}
