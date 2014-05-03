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

#define PREINIT_SYSTEM_DEVPATH "/dev/mmcblk0p1"

#define DETECTED_UNKNOWN 0
#define DETECTED_EXT4 1
#define DETECTED_F2FS 2

void mount_system()
{
/*
 * this stuff is not needed anymore with CONFIG_DEVTMPFS:
	int fd, major, minor;
	char buf[16];

	fd = open("/sys/block/mmcblk0/mmcblk0p1/dev", O_RDONLY);
	if (fd == -1) {
		KLOG("<3>preinit: open /sys/block/mmcblk0/mmcblk0p1/dev failed\n");
		return;
	}
	if (read(fd, buf, sizeof(buf)) <= 0) {
		KLOG("<3>preinit: read /sys/block/mmcblk0/mmcblk0p1/dev failed\n");
		return;
	}
	close(fd);
	if (sscanf(buf, "%d:%d", &major, &minor) != 2) {
		KLOG("<3>preinit: parsing /sys/block/mmcblk0/mmcblk0p1/dev failed\n");
		return;
	}
	if (mknod(PREINIT_SYSTEM_DEVPATH, S_IFBLK | 0600, makedev(major, minor)) != 0) {
		KLOG("<3>preinit: mknod " PREINIT_SYSTEM_DEVPATH " failed\n");
		return;
	}
*/
	if (mount(PREINIT_SYSTEM_DEVPATH, "/system", "ext4", MS_RDONLY, NULL) != 0) {
		KLOG("<3>preinit: mount " PREINIT_SYSTEM_DEVPATH " failed\n");
		return;
	}
}

void unmount(const char* target)
{
	if (umount(target) != 0)
	{
		char buf[160];
		sprintf(buf, "<2>preinit: unmount of %s failed, errno=%d\n", target, errno);
		KLOG(buf);
	}
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

void enable_verbose_printk()
{
	int fd;
	fd = open("/proc/sys/kernel/printk", O_WRONLY);
	if (fd != -1) {
		write(fd, "7", 1);
		close(fd);
		KLOG("<7>preinit: verbose printk re-enabled.\n");
	}
}

int detect_filesystem(const char *devpath)
{
	char buf[2048];
	int fd;

	fd = open(devpath, O_RDONLY);
	if (fd == -1)
		return -errno;

	memset(buf, 0, sizeof(buf));
	if (!read(fd, buf, sizeof(buf)))
	{
		close(fd);
		return -errno;
	}
	close(fd);

	if (!memcmp(buf+1024+0x38, "\x53\xef", 2))
		return DETECTED_EXT4;

	if (!memcmp(buf+1024, "\x10\x20\xf5\xf2", 4))
		return DETECTED_F2FS;

	return DETECTED_UNKNOWN;
}

void print_detect_filesystem(const char *devpath, const char *name, char *script, int buflen)
{
	int result;
	char buf[80], line[80];

	result = detect_filesystem(devpath);
	switch (result)
	{
		case DETECTED_EXT4:
			strcpy(buf, "ext4");
			break;

		case DETECTED_F2FS:
			strcpy(buf, "f2fs");
			break;

		case DETECTED_UNKNOWN:
			strcpy(buf, "unknown");
			break;

		case -ENOENT:
			strcpy(buf, "not present");
			break;

		default:
			sprintf(buf, "error %d", -result);
			break;
	}

	const char* devname = devpath + 5;	/* HACK: skip "/dev/" */
	if (result > 0)
		sprintf(line, "FS_%s=%s\n", devname, buf);
	else
		sprintf(line, "# error for %s: %s\n", devname, buf);
	strlcat(script, line, buflen);

	printf("%-16s %-50s -> %s\n", devname, name, buf);
}

void detect_filesystems()
{
	char script[1024];
	int buflen = sizeof(script);
	int fd;

	printf("\n\nDetecting filesystems...\n");
	print_detect_filesystem("/dev/mmcblk0p8", "/data on internal (UDA)", script, buflen);
	print_detect_filesystem("/dev/mmcblk1p2", "/data on microSD (for Data2SD/ROM2SD)", script, buflen);
	print_detect_filesystem("/dev/mmcblk1p3", "/system on microSD (for ROM2SD)", script, buflen);
	printf("\n\n");

	fd = open("/fs_detected.sh", O_CREAT | O_WRONLY, 0644);
	if (fd != -1)
	{
		write(fd, script, strlen(script));
		close(fd);
	}
	else
	{
		KLOG("<2>preinit: failed to write /fs_detected.sh\n");
	}
}

int main(int argc, char *argv[], char *envp[])
{
	int rc;
	struct stat s;
	char buf[160];

	if (strcmp(argv[0], "/init-chainload") == 0) {
		argv[0] = "/init";
		goto chainload;
	}

	mount("proc", "/proc", "proc", 0, NULL);
	mount("sysfs", "/sys", "sysfs", 0, NULL);
	mount("devtmpfs", "/dev", "devtmpfs", 0, NULL);

	klog_init();
	KLOG("<2>preinit: ########## starting ##########\n");

	mount_system();

	// a fully custom init can take over if it exists
	if (stat("/system/boot/init", &s) == 0) {
		KLOG("<2>preinit: starting /system/boot/init\n");
		// if this works, it takes over from here and execve never returns.
		// if custom init wants to boot Android, it must unmount /system and /dev or modify init.rc scripts/fstab accordingly
		rc = execve("/system/boot/init", argv, envp);
		sprintf(buf, "<2>preinit: execve /system/boot/init failed: rc=%d errno=%d, resuming default boot.\n", rc, errno);
		KLOG(buf);
	}

	// no custom init, so we're still in charge - boot Android

	// this rename is essential, or the symlink sbin/ueventd must be changed!
	rename("init-android", "init");

	detect_filesystems();

	// run pre-init hook as child and wait
	if (stat("/system/boot/preinit", &s) == 0) {
		KLOG("<2>preinit: starting /system/boot/preinit\n");
		int pid = fork();
		if (pid == 0) {
			rc = execve("/system/boot/preinit", argv, envp);
			sprintf(buf, "<2>preinit: execve /system/boot/preinit failed: rc=%d errno=%d, resuming default boot.\n", rc, errno);
			KLOG(buf);
			return 42;
		} else if (pid == -1) {
			sprintf(buf, "<2>preinit: fork failed: errno=%d, resuming default boot.\n", errno);
			KLOG(buf);
		} else {
			wait(&rc);
			sprintf(buf, "<6>preinit: /system/boot/preinit exit status=%d\n", rc);
			KLOG(buf);
		}
	}

chainload:
	// clean up the mess we made, otherwise "mount_all" in real init fails
	unmount("/system");
//	unlink(PREINIT_SYSTEM_DEVPATH);
	unmount("/dev");

	// re-enable verbose printk for ram_console (/proc/last_kmsg)
	enable_verbose_printk();

	// unbind fb console to avoid crash when the console is blanked while Android is running
	unbind_fbcon();

	rc = execve("/init", argv, envp);

	printf("argh! rc=%d errno=%d", rc, errno);
	sleep(5);
	return 0;
}
