// init.c - pre-init for Android

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/wait.h>
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

void mount_system()
{
	int fd, major, minor;
	char buf[16];

	fd = open("/sys/block/mmcblk0/mmcblk0p1/dev", O_RDONLY);
	if (fd == -1) {
		KLOG("<3>preinit: open /sys/block/mmcblk0/mmcblk0p1/dev failed");
		return;
	}
	if (read(fd, buf, sizeof(buf)) <= 0) {
		KLOG("<3>preinit: read /sys/block/mmcblk0/mmcblk0p1/dev failed");
		return;
	}
	close(fd);
	if (sscanf(buf, "%d:%d", &major, &minor) != 2) {
		KLOG("<3>preinit: parsing /sys/block/mmcblk0/mmcblk0p1/dev failed");
		return;
	}
	if (mknod(PREINIT_SYSTEM_DEVNAME, S_IFBLK | 0600, makedev(major, minor)) != 0) {
		KLOG("<3>preinit: mknod " PREINIT_SYSTEM_DEVNAME " failed");
		return;
	}
	if (mount(PREINIT_SYSTEM_DEVNAME, "/system", "ext4", MS_RDONLY, NULL) != 0) {
		KLOG("<3>preinit: mount " PREINIT_SYSTEM_DEVNAME " failed");
		return;
	}
}

void unmount_system()
{
	umount("/system");
	unlink(PREINIT_SYSTEM_DEVNAME);
}

void unbind_fbcon()
{
	int fd;
	fd = open("/sys/class/vtconsole/vtcon1/bind", O_WRONLY);
	if (fd != -1) {
		write(fd, "0", 1);
		close(fd);
	}
}


int main(int argc, char *argv[], char *envp[])
{
	int rc;
	struct stat s;
	char buf[80];

	if (strcmp(argv[0], "/init-chainload") == 0) {
		argv[0] = "/init";
		goto chainload;
	}
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
	
	KLOG("<2>preinit: ########## starting ##########");

	mount_system();

	// a fully custom init can take over if it exists
	if (stat("/system/boot/init", &s) == 0) {
		KLOG("<2>preinit: starting /system/boot/init");
		// if this works, it takes over from here and execve never returns.
		// if custom init wants to boot Android, it must unmount /system or modify init.rc scripts/fstab accordingly
		rc = execve("/system/boot/init", argv, envp);
		sprintf(buf, "<2>preinit: execve /system/boot/init failed: rc=%d errno=%d, resuming default boot.", rc, errno);
		KLOG(buf);
	}

	// no custom init, so we're still in charge - boot Android

	// this rename is essential, or the symlink sbin/ueventd must be changed!
	rename("init-android", "init");

	// run pre-init hook as child and wait
	if (stat("/system/boot/preinit", &s) == 0) {
		KLOG("<2>preinit: starting /system/boot/preinit");
		int pid = fork();
		if (pid == 0) {
			rc = execve("/system/boot/preinit", argv, envp);
			sprintf(buf, "<2>preinit: execve /system/boot/preinit failed: rc=%d errno=%d, resuming default boot.", rc, errno);
			KLOG(buf);
			return 42;
		} else if (pid == -1) {
			sprintf(buf, "<2>preinit: fork failed: errno=%d, resuming default boot.", errno);
			KLOG(buf);
		} else {
			wait(&rc);
			sprintf(buf, "<6>preinit: /system/boot/preinit exit status=%d", rc);
			KLOG(buf);
		}
	}

	// unbind fb console to avoid crash when the console is blanked while Android is running
	unbind_fbcon();

chainload:
	// but first clean up the mess we made, otherwise "mount_all" in real init fails
	unmount_system();

	rc = execve("/init", argv, envp);

	printf("argh! rc=%d errno=%d", rc, errno);
	sleep(5);
	return 0;
}
