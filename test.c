#include <sys/stat.h>
#include <stdio.h>

int main() {
	struct stat buf;
	stat("test.c", &buf);
	fprintf(stderr, "owned by %d:%d\n", buf.st_uid, buf.st_gid);
	chown("test.c", 0, 0);
	stat("test.c", &buf);
	fprintf(stderr, "owned by %d:%d\n", buf.st_uid, buf.st_gid);
	return 0;
}
