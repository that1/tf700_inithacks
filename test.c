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

int detect_filesystem(const char *devpath)
{
	return DETECTED_EXT4;
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
        print_detect_filesystem("/dev/mmcblk0p1", "/system on internal (UDA)", script, buflen);
	print_detect_filesystem("/dev/mmcblk0p8", "/data on internal (UDA)", script, buflen);
	print_detect_filesystem("/dev/mmcblk1p2", "/data on microSD (for Data2SD/ROM2SD)", script, buflen);
        print_detect_filesystem("/dev/mmcblk0p2", "/cache on internal (UDA)", script, buflen);
	print_detect_filesystem("/dev/mmcblk1p3", "/system on microSD (for ROM2SD)", script, buflen);
	printf("\n\n");

	fd = open("/fs_detected", O_CREAT | O_WRONLY, 0644);
	write(fd, script, strlen(script));
	close(fd);
}

int main(int argc, char *argv[], char *envp[])
{
	detect_filesystems();
}
