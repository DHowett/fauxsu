#include <dlfcn.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <stdio.h>

#include <map>
using namespace std;

// {,l,f}stat, access, chmod, fchmod, chown, fchown

#define dlog(args...) fprintf(stderr, args);

struct ownership_entry {
	uid_t owner;
	gid_t group;
};

struct mode_entry {
	mode_t mode;
};

typedef map<ino_t, struct ownership_entry *> ownmap;
typedef map<ino_t, mode_t> modemap;
ownmap olist;
modemap mlist;

static __attribute((unused)) void hexdump(void *mem, unsigned int c) {
	unsigned int num;
	for(num = 0; num < c; ++num) {
		//if(num % 16 == 0) dlog("LIB   %p: ", (unsigned char *)mem + num);
		dlog("%02x ", *(((unsigned char *)mem) + num));
		if(num % 16 == 15) dlog("\n");
	}
	if(num % 16 != 0) dlog("\n");
}

static void ifakemode(dev_t dev, ino_t ino, mode_t mode) {
	fprintf(stderr, "Setting faking mode to %o for %d.\n", mode, ino);
	mlist[ino] = mode;
}

static void ifakeown(dev_t dev, ino_t ino, uid_t owner, gid_t group) {
	fprintf(stderr, "Setting fake ownership to %d:%d for %d.\n", owner, group, ino);
	struct ownership_entry *x = new ownership_entry();
	x->owner = owner;
	x->group = group;
	olist[ino] = x;
}

static void fakemode(const char *path, mode_t mode) {
	struct stat buf;
	stat(path, &buf);
	ifakemode(buf.st_dev, buf.st_ino, mode);
}

static void ffakemode(int fildes, mode_t mode) {
	struct stat buf;
	fstat(fildes, &buf);
	ifakemode(buf.st_dev, buf.st_ino, mode);
}

static void fakeown(const char *path, uid_t owner, gid_t group) {
	struct stat buf;
	stat(path, &buf);
	owner = (owner == -1 ? buf.st_uid : owner); group = (group == -1 ? buf.st_gid : group);
	ifakeown(buf.st_dev, buf.st_ino, owner, group);
}

static void ffakeown(int fildes, uid_t owner, gid_t group) {
	struct stat buf;
	fstat(fildes, &buf);
	owner = (owner == -1 ? buf.st_uid : owner); group = (group == -1 ? buf.st_gid : group);
	ifakeown(buf.st_dev, buf.st_ino, owner, group);
}

static void lfakeown(const char *path, uid_t owner, gid_t group) {
	struct stat buf;
	lstat(path, &buf);
	owner = (owner == -1 ? buf.st_uid : owner); group = (group == -1 ? buf.st_gid : group);
	ifakeown(buf.st_dev, buf.st_ino, owner, group);
}

static void dofake(struct stat *buf) {
	buf->st_uid = 0;
	buf->st_gid = 0;

	if(olist.find(buf->st_ino) != olist.end()) {
		struct ownership_entry *x = olist[buf->st_ino];
		buf->st_uid = x->owner;
		buf->st_gid = x->group;
		dlog("Returning fake ownership %d:%d for %d.\n", buf->st_uid, buf->st_gid, buf->st_ino);
	}

	if(mlist.find(buf->st_ino) != mlist.end()) {
		buf->st_mode = mlist[buf->st_ino];
		dlog("Returning fake mode %d for %d.\n", buf->st_mode, buf->st_ino);
	}

	return;
}

extern "C" int my_chmod(const char *path, mode_t mode) {
	int ret = chmod(path, mode);
	if(ret != 0 && errno == EPERM) {
		fakemode(path, mode);
		ret = 0; errno = 0;
	}
	return ret;
}

extern "C" int my_fchmod(int fildes, mode_t mode) {
	int ret = fchmod(fildes, mode);
	if(ret != 0 && errno == EPERM) {
		ffakemode(fildes, mode);
		ret = 0; errno = 0;
	}
	return ret;
}

extern "C" int my_chown(const char *path, uid_t owner, gid_t group) {
	int ret = chown(path, owner, group);
	if(ret != 0 && errno == EPERM) {
		fakeown(path, owner, group);
		ret = 0; errno = 0;
	}
	return ret;
}

extern "C" int my_fchown(int fildes, uid_t owner, gid_t group) {
	int ret = fchown(fildes, owner, group);
	if(ret != 0 && errno == EPERM) {
		ffakeown(fildes, owner, group);
		ret = 0; errno = 0;
	}
	return ret;
}

extern "C" int my_lchown(const char *path, uid_t owner, gid_t group) {
	int ret = lchown(path, owner, group);
	if(ret != 0 && errno == EPERM) {
		lfakeown(path, owner, group);
		ret = 0; errno = 0;
	}
	return ret;
}

extern "C" int my_stat(const char *path, struct stat *buf) {
	int ret = stat(path, buf);
	dofake(buf);
	return ret;
}

extern "C" int my_lstat(const char *path, struct stat *buf) {
	int ret = lstat(path, buf);
	dofake(buf);
	return ret;
}

extern "C" int my_fstat(int fildes, struct stat *buf) {
	int ret = fstat(fildes, buf);
	dofake(buf);
	return ret;
}

extern "C" uid_t my_getuid() { return 0; }
extern "C" uid_t my_geteuid() { return 0; }
extern "C" gid_t my_getgid() { return 0; }
extern "C" gid_t my_getegid() { return 0; }

#define DEFAULT_PERSIST_FILENAME ".fauxsu"
static char _persist_filename[PATH_MAX] = "";

char *persist_file_name() {
	if(_persist_filename[0] == '\0') {
		char *env = getenv("_FAUXSU_PERSIST_FILENAME");
		if(env && strlen(env) > 0) {
			strncpy(_persist_filename, env, PATH_MAX);
		} else {
			snprintf(_persist_filename, PATH_MAX, DEFAULT_PERSIST_FILENAME);
		}
		fprintf(stderr, "persist filename is %s\n", _persist_filename);
	}
	return _persist_filename;
}

static __attribute__((destructor)) void fini() {
	int nmodes = mlist.size();
	int nowns = olist.size();
	if(nmodes == 0 && nowns == 0) return;
	FILE *o = fopen(persist_file_name(), "wb+");
	fwrite(&nmodes, 1, sizeof(int), o);
	fwrite(&nowns, 1, sizeof(int), o);
	for(modemap::const_iterator it = mlist.begin(); it != mlist.end(); ++it) {
		ino_t ino = it->first;
		mode_t mode = it->second;
		fwrite(&ino, 1, sizeof(ino_t), o);
		fwrite(&mode, 1, sizeof(mode_t), o);
	}
	for(ownmap::const_iterator it = olist.begin(); it != olist.end(); ++it) {
		ino_t ino = it->first;
		struct ownership_entry *x = it->second;
		fwrite(&ino, 1, sizeof(ino_t), o);
		fwrite(&x->owner, 1, sizeof(uid_t), o);
		fwrite(&x->group, 1, sizeof(gid_t), o);
	}
	fclose(o);
}

static __attribute__((constructor)) void init() {
	int nmodes, nowns;
	FILE *o = fopen(persist_file_name(), "rb+");
	if(!o) return;
	fread(&nmodes, 1, sizeof(int), o);
	fread(&nowns, 1, sizeof(int), o);
	for(int i = 0; i < nmodes; ++i) {
		ino_t ino;
		mode_t mode;
		fread(&ino, 1, sizeof(ino_t), o);
		fread(&mode, 1, sizeof(mode_t), o);
		mlist[ino] = mode;
	}
	for(int i = 0; i < nowns; ++i) {
		ino_t ino;
		uid_t owner;
		gid_t group;
		fread(&ino, 1, sizeof(ino_t), o);
		fread(&owner, 1, sizeof(uid_t), o);
		fread(&group, 1, sizeof(gid_t), o);
		struct ownership_entry *x = new ownership_entry();
		x->owner = owner;
		x->group = group;
		olist[ino] = x;
	}
}

extern "C" const struct {void *n; void *o;} interposers[] __attribute((section("__DATA, __interpose"))) = {
	{ (void *)my_chmod, (void *)chmod },
	{ (void *)my_fchmod, (void *)fchmod },
	{ (void *)my_chown, (void *)chown },
	{ (void *)my_fchown, (void *)fchown },
	{ (void *)my_lchown, (void *)lchown },
	{ (void *)my_stat, (void *)stat },
	{ (void *)my_lstat, (void *)lstat },
	{ (void *)my_fstat, (void *)fstat },
	{ (void*)my_getuid, (void *)getuid },
	{ (void*)my_geteuid, (void *)geteuid },
	{ (void*)my_getgid, (void *)getgid },
	{ (void*)my_getegid, (void *)getegid },
};
