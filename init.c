// init.c - detect storage and run real init

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

int find_preinit()
{
	int fd, major, minor;
	char buf[16];
	struct stat s;

	fd = open("/sys/block/mmcblk0/mmcblk0p1/dev", O_RDONLY);
	if (fd) {
		if (read(fd, buf, sizeof(buf)) <= 0) {
			KLOG("<4>preinit: can't read /sys/block/mmcblk0/mmcblk0p1/dev");
			return;
		}
		close(fd);
		if (sscanf(buf, "%d:%d", &major, &minor) != 2) {
			KLOG("<4>preinit: failed to parse /sys/block/mmcblk0/mmcblk0p1/dev");
			return;
		}
		if (mknod(PREINIT_SYSTEM_DEVNAME, S_IFBLK | 0600, makedev(major, minor)) != 0) {
			KLOG("<4>preinit: mknod " PREINIT_SYSTEM_DEVNAME " failed");
			return;
		}
		if (mount(PREINIT_SYSTEM_DEVNAME, "/system", "ext4", MS_RDONLY, NULL) != 0) {
			KLOG("<4>preinit: mount " PREINIT_SYSTEM_DEVNAME " failed");
			return;
		}
		if (stat("/system/boot/preinit", &s) == 0) {
			KLOG("<6>preinit: found /system/boot/preinit");
			return 1;
		}
	}
	return 0;
}

void detect_data2sd()
{
	int rc;
	struct stat s;

	rc = stat("/sys/block/mmcblk1/mmcblk1p2", &s);
	if (rc == 0) {
		KLOG("<6>preinit: mmcblk1p2 detected, setting up for data2sd");
//		umount("/data");
		rename("fstab.cardhu.data2sd", "fstab.cardhu");
		rename("init.cardhu.rc.data2sd", "init.cardhu.rc");
	} else {
		KLOG("<6>preinit: mmcblk1p2 not detected, setting up for internal storage");
//		printf("stat returned errno %d\n", errno);
//		sleep(5);
		unlink("fstab.cardhu.data2sd");
		unlink("init.cardhu.rc.data2sd");
	}
}

int main(int argc, char *argv[], char *envp[])
{
	int rc;
	char buf[80];
/*
	printf("hello world, this is that-init. my pid = %d\n", getpid());
	int fd = open("log.txt", O_WRONLY | O_CREAT, 0600);
	if (fd != -1) {
		write(fd, "hello\n", 6);
		close(fd);
	}
*/
	mount("sysfs", "/sys", "sysfs", 0, NULL);
	klog_init();
	
	// this rename is essential, or the symlink sbin/ueventd must be changed!
	rename("init-android", "init");

	if (find_preinit()) {
		// if this works, it takes over from here and execve never returns.
		// preinit must modify the root fs as it likes, unmount /system and unlink PREINIT_SYSTEM_DEVNAME, finally execve /init
		rc = execve("/system/boot/preinit", argv, envp);
		// otherwise, we continue with standard boot:
		sprintf(buf, "<4>preinit: execve /system/boot/preinit failed: rc=%d errno=%d ", rc, errno);
		KLOG(buf);
	}
	
	// but first clean up the mess we made :)
	umount("/system");
	unlink(PREINIT_SYSTEM_DEVNAME);

	// TODO: move this stuff to preinit so users can opt out from auto-detection
	detect_data2sd();

	rc = execve("/init", argv, envp);

	printf("argh! rc=%d errno=%d", rc, errno);
	sleep(5);
	return 0;
}

