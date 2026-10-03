/* Symbols the linux-gnu Rust std and bundled SQLite reference, which
   devkitA64 newlib does not provide. */
#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <time.h>
#include <dirent.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

struct iovec {
	void *iov_base;
	size_t iov_len;
};

#include <switch.h>

void switch_trace(const char *msg);

/* newlib has no glibc *64 names. Drop a large-file macro if a header added one. */
#ifdef open64
#undef open64
#endif
#ifdef openat64
#undef openat64
#endif
#ifdef stat64
#undef stat64
#endif
#ifdef fstat64
#undef fstat64
#endif
#ifdef lstat64
#undef lstat64
#endif
#ifdef lseek64
#undef lseek64
#endif
#ifdef readdir64
#undef readdir64
#endif
#ifdef mmap64
#undef mmap64
#endif

int fchown(int fd, uid_t owner, gid_t group)
{
	(void)fd;
	(void)owner;
	(void)group;
	errno = ENOSYS;
	return -1;
}

uid_t geteuid(void)
{
	return 0;
}

/* compiler-builtins and sha2 use this to detect LSE and crypto extensions.
   Cortex-A57 has neither, so reporting no hardware capabilities is correct. */
unsigned long getauxval(unsigned long type)
{
	(void)type;
	return 0;
}

int posix_memalign(void **memptr, size_t alignment, size_t size)
{
	void *p;

	if (alignment < sizeof(void *) || (alignment & (alignment - 1)) != 0)
		return EINVAL;
	p = memalign(alignment, size);
	if (p == NULL)
		return ENOMEM;
	*memptr = p;
	return 0;
}

/* glibc names the thread-local errno cell __errno_location. newlib uses __errno. */
int *__errno_location(void)
{
	return __errno();
}

/* aarch64 Linux numbers. Rust std parks threads on futex and draws entropy
   with getrandom; both go through syscall(). */
#define SYS_futex 98
#define SYS_gettid 178
#define SYS_getrandom 278

#define FUTEX_WAIT 0
#define FUTEX_WAKE 1
#define FUTEX_WAIT_BITSET 9
#define FUTEX_WAKE_BITSET 10
#define FUTEX_PRIVATE_FLAG 128
#define FUTEX_CLOCK_REALTIME 256
#define FUTEX_CMD_MASK 127

#define KERNEL_RESULT_TIMEOUT 0xEA01

static long futex_op(va_list ap)
{
	uint32_t *uaddr;
	int op;
	uint32_t val;
	int cmd;
	Result rc;

	uaddr = va_arg(ap, uint32_t *);
	op = va_arg(ap, int);
	val = va_arg(ap, uint32_t);
	cmd = op & FUTEX_CMD_MASK;

	if (cmd == FUTEX_WAIT || cmd == FUTEX_WAIT_BITSET) {
		const struct timespec *ts = va_arg(ap, const struct timespec *);
		s64 timeout;
		char buf[96];
		static int traced;

		if (ts == NULL) {
			timeout = -1;
		} else if (cmd == FUTEX_WAIT_BITSET) {
			/* Absolute time. Rust reads CLOCK_MONOTONIC via clock_gettime;
			   that is not the tick counter since boot. */
			struct timespec now_ts;
			int clk = (op & FUTEX_CLOCK_REALTIME) ? CLOCK_REALTIME : CLOCK_MONOTONIC;
			uint64_t now;
			uint64_t abs;

			if (clock_gettime(clk, &now_ts) != 0) {
				errno = EINVAL;
				return -1;
			}
			now = (uint64_t)now_ts.tv_sec * 1000000000ull + (uint64_t)now_ts.tv_nsec;
			abs = (uint64_t)ts->tv_sec * 1000000000ull + (uint64_t)ts->tv_nsec;
			if (abs <= now) {
				errno = ETIMEDOUT;
				return -1;
			}
			timeout = (s64)(abs - now);
		} else {
			timeout = (s64)ts->tv_sec * 1000000000ll + (s64)ts->tv_nsec;
			if (timeout < 0)
				timeout = 0;
		}

		if (traced < 24) {
			traced++;
			snprintf(buf, sizeof(buf), "futex wait %p op=%d val=%u ns=%lld",
				(void *)uaddr, op, val, (long long)timeout);
			switch_trace(buf);
		}

		/* Wakeups can be missed. Re-read the word every slice so a writer
		   that already stored the new value cannot leave us parked. */
		for (;;) {
			s64 slice = 20000000;

			if (*uaddr != val)
				return 0;
			if (timeout == 0) {
				errno = ETIMEDOUT;
				return -1;
			}
			if (timeout > 0 && timeout < slice)
				slice = timeout;
			rc = svcWaitForAddress(uaddr, ArbitrationType_WaitIfEqual, (s64)val, slice);
			if (*uaddr != val)
				return 0;
			/* Success with the word unchanged is not a wakeup. Stay here:
			   returning EAGAIN makes Rust call again, and the log filled
			   with the same wait. */
			if (R_SUCCEEDED(rc)) {
				/* Not a real wakeup. A short yield keeps the core from
				 * spinning; sleeping the whole 20 ms slice makes the game
				 * run in slow motion. */
				svcSleepThread(100000);
				if (timeout < 0)
					continue;
			} else if (R_VALUE(rc) != KERNEL_RESULT_TIMEOUT) {
				errno = EAGAIN;
				return -1;
			}
			if (timeout < 0)
				continue;
			timeout -= slice;
			if (timeout <= 0) {
				errno = ETIMEDOUT;
				return -1;
			}
		}
	}

	if (cmd == FUTEX_WAKE || cmd == FUTEX_WAKE_BITSET) {
		s32 count = (s32)val;

		/* Rust's wake-all passes i32::MAX. Horizon uses -1 to signal every waiter. */
		if (count < 0 || count > 1000000)
			count = -1;
		rc = svcSignalToAddress(uaddr, SignalType_Signal, 0, count);
		if (R_FAILED(rc))
			return 0;
		return count < 0 ? 1 : count;
	}

	errno = ENOSYS;
	return -1;
}

static long getrandom_op(va_list ap)
{
	void *buf = va_arg(ap, void *);
	size_t len = va_arg(ap, size_t);
	unsigned int flags = va_arg(ap, unsigned int);

	static int traced;

	(void)flags;
	if (buf == NULL) {
		errno = EFAULT;
		return -1;
	}
	if (traced < 4)
		switch_trace("getrandom");
	randomGet(buf, len);
	if (traced < 4) {
		traced++;
		switch_trace("getrandom ok");
	}
	return (long)len;
}

static long switch_gettid(void)
{
	Thread *self = threadGetSelf();
	u32 handle = 0;

	if (self != NULL)
		handle = self->handle;
	if (handle == 0)
		handle = (u32)envGetMainThreadHandle();
	if (handle == 0)
		handle = 1;
	return (long)handle;
}

long syscall(long number, ...)
{
	va_list ap;
	long ret;

	va_start(ap, number);
	switch (number) {
	case SYS_futex:
		ret = futex_op(ap);
		break;
	case SYS_getrandom:
		ret = getrandom_op(ap);
		break;
	case SYS_gettid:
		/* Rust's panic hook asks for this before it can print. ENOSYS
		   leaves the hook with no thread id and the message never lands. */
		ret = switch_gettid();
		break;
	default: {
		static int traced;
		char buf[64];

		if (traced < 20) {
			traced++;
			snprintf(buf, sizeof(buf), "syscall %ld", number);
			switch_trace(buf);
		}
		errno = ENOSYS;
		ret = -1;
		break;
	}
	}
	va_end(ap);
	return ret;
}

/* oaknut's CodeBlock asks for an anonymous RWX page and treats a null
   return as failure. Horizon never grants writable executable memory, so
   this is ordinary anonymous storage. Generated code cannot run from it. */
void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
	size_t align = 0x1000;
	size_t rounded;
	void *p;

	(void)addr;
	(void)prot;
	(void)flags;
	(void)fd;
	(void)offset;
	if (length == 0) {
		errno = EINVAL;
		return NULL;
	}
	rounded = (length + align - 1) & ~(align - 1);
	p = memalign(align, rounded);
	if (p == NULL) {
		errno = ENOMEM;
		return NULL;
	}
	memset(p, 0, rounded);
	return p;
}

int munmap(void *addr, size_t length)
{
	(void)length;
	if (addr != NULL)
		free(addr);
	return 0;
}

/* glibc aarch64 struct stat64. newlib's struct stat is a different layout,
   so the *64 calls copy the fields Rust actually reads (type, size, times).
   newlib #defines st_atime as st_atim.tv_sec, which would rewrite these fields. */
#undef st_atime
#undef st_mtime
#undef st_ctime
struct linux_stat64 {
	uint64_t st_dev;
	uint64_t st_ino;
	uint32_t st_mode;
	uint32_t st_nlink;
	uint32_t st_uid;
	uint32_t st_gid;
	uint64_t st_rdev;
	uint64_t pad1;
	int64_t st_size;
	int32_t st_blksize;
	int32_t pad2;
	int64_t st_blocks;
	int64_t st_atime;
	int64_t st_atime_nsec;
	int64_t st_mtime;
	int64_t st_mtime_nsec;
	int64_t st_ctime;
	int64_t st_ctime_nsec;
	int32_t unused[2];
};

/* glibc dirent64. Rust only depends on the offset of d_name (19) and d_type (18). */
struct linux_dirent64 {
	uint64_t d_ino;
	int64_t d_off;
	uint16_t d_reclen;
	uint8_t d_type;
	char d_name[256];
};

_Static_assert(offsetof(struct linux_stat64, st_size) == 48, "stat64 size offset");
_Static_assert(sizeof(struct linux_stat64) == 128, "stat64 size");
_Static_assert(offsetof(struct linux_dirent64, d_type) == 18, "dirent64 type offset");
_Static_assert(offsetof(struct linux_dirent64, d_name) == 19, "dirent64 name offset");

/* Linux open flags differ from newlib's. Rust passes the Linux values. */
static int linux_open_flags(int flags)
{
	int out = flags & 3;

	if (flags & 0x40)
		out |= O_CREAT;
	if (flags & 0x80)
		out |= O_EXCL;
	if (flags & 0x100)
		out |= O_NOCTTY;
	if (flags & 0x200)
		out |= O_TRUNC;
	if (flags & 0x400)
		out |= O_APPEND;
	if (flags & 0x800)
		out |= O_NONBLOCK;
	if (flags & 0x4000)
		out |= O_DIRECTORY;
	if (flags & 0x8000)
		out |= O_NOFOLLOW;
	if (flags & 0x80000)
		out |= O_CLOEXEC;
	return out;
}

static void copy_stat64(struct linux_stat64 *out, const struct stat *in)
{
	memset(out, 0, sizeof(*out));
	out->st_dev = (uint64_t)in->st_dev;
	out->st_ino = (uint64_t)in->st_ino;
	out->st_mode = (uint32_t)in->st_mode;
	out->st_nlink = in->st_nlink;
	out->st_uid = in->st_uid;
	out->st_gid = in->st_gid;
	out->st_rdev = (uint64_t)in->st_rdev;
	out->st_size = (int64_t)in->st_size;
	out->st_blksize = (int32_t)in->st_blksize;
	out->st_blocks = (int64_t)in->st_blocks;
	out->st_atime = (int64_t)in->st_atim.tv_sec;
	out->st_atime_nsec = (int64_t)in->st_atim.tv_nsec;
	out->st_mtime = (int64_t)in->st_mtim.tv_sec;
	out->st_mtime_nsec = (int64_t)in->st_mtim.tv_nsec;
	out->st_ctime = (int64_t)in->st_ctim.tv_sec;
	out->st_ctime_nsec = (int64_t)in->st_ctim.tv_nsec;
}

int open64(const char *path, int flags, ...)
{
	mode_t mode = 0;

	if (flags & 0x40) {
		va_list ap;

		va_start(ap, flags);
		mode = (mode_t)va_arg(ap, int);
		va_end(ap);
	}
	if (path != NULL) {
		static int traced;

		if (traced < 40) {
			char buf[160];

			traced++;
			snprintf(buf, sizeof(buf), "open %s", path);
			switch_trace(buf);
		}
	}
	return open(path, linux_open_flags(flags), mode);
}

int openat64(int dirfd, const char *path, int flags, ...)
{
	mode_t mode = 0;

	if (flags & 0x40) {
		va_list ap;

		va_start(ap, flags);
		mode = (mode_t)va_arg(ap, int);
		va_end(ap);
	}
	/* AT_FDCWD */
	if (dirfd != -100) {
		errno = ENOSYS;
		return -1;
	}
	return open(path, linux_open_flags(flags), mode);
}

int stat64(const char *path, struct linux_stat64 *buf)
{
	struct stat st;

	if (stat(path, &st) != 0)
		return -1;
	copy_stat64(buf, &st);
	return 0;
}

int fstat64(int fd, struct linux_stat64 *buf)
{
	struct stat st;

	if (fstat(fd, &st) != 0)
		return -1;
	copy_stat64(buf, &st);
	return 0;
}

int lstat64(const char *path, struct linux_stat64 *buf)
{
	struct stat st;

	if (lstat(path, &st) != 0)
		return -1;
	copy_stat64(buf, &st);
	return 0;
}

int64_t lseek64(int fd, int64_t offset, int whence)
{
	return (int64_t)lseek(fd, (off_t)offset, whence);
}

struct linux_dirent64 *readdir64(DIR *dirp)
{
	struct dirent *ent;
	static _Thread_local struct linux_dirent64 out;

	ent = readdir(dirp);
	if (ent == NULL)
		return NULL;
	memset(&out, 0, sizeof(out));
	out.d_ino = (uint64_t)ent->d_ino;
	out.d_off = (int64_t)telldir(dirp);
	out.d_reclen = (uint16_t)sizeof(out);
	out.d_type = ent->d_type;
	memcpy(out.d_name, ent->d_name, sizeof(out.d_name));
	out.d_name[sizeof(out.d_name) - 1] = '\0';
	return &out;
}

int dirfd(DIR *dirp)
{
	(void)dirp;
	errno = EBADF;
	return -1;
}

DIR *fdopendir(int fd)
{
	(void)fd;
	errno = ENOSYS;
	return NULL;
}

int unlinkat(int dirfd, const char *path, int flags)
{
	if (dirfd != -100) {
		errno = ENOSYS;
		return -1;
	}
	/* AT_REMOVEDIR */
	if (flags & 0x200)
		return rmdir(path);
	return unlink(path);
}

ssize_t readv(int fd, const struct iovec *iov, int iovcnt)
{
	ssize_t total = 0;
	int i;

	for (i = 0; i < iovcnt; i++) {
		ssize_t n = read(fd, iov[i].iov_base, iov[i].iov_len);

		if (n < 0)
			return total > 0 ? total : -1;
		total += n;
		if ((size_t)n < iov[i].iov_len)
			break;
	}
	return total;
}

ssize_t writev(int fd, const struct iovec *iov, int iovcnt)
{
	ssize_t total = 0;
	int i;

	for (i = 0; i < iovcnt; i++) {
		ssize_t n = write(fd, iov[i].iov_base, iov[i].iov_len);

		if (n < 0)
			return total > 0 ? total : -1;
		total += n;
		if ((size_t)n < iov[i].iov_len)
			break;
	}
	return total;
}

void *mmap64(void *addr, size_t length, int prot, int flags, int fd, int64_t offset)
{
	void *p;

	/* MAP_ANONYMOUS is 0x20 on Linux. File-backed maps are refused so
	   backtrace symbolization falls back instead of reading a bad page. */
	if ((flags & 0x20) == 0) {
		errno = ENODEV;
		return (void *)-1;
	}
	p = mmap(addr, length, prot, flags, fd, (off_t)offset);
	if (p == NULL) {
		errno = ENOMEM;
		return (void *)-1;
	}
	return p;
}

ssize_t splice(int fd_in, int64_t *off_in, int fd_out, int64_t *off_out, size_t len, unsigned int flags)
{
	(void)fd_in;
	(void)off_in;
	(void)fd_out;
	(void)off_out;
	(void)len;
	(void)flags;
	errno = ENOSYS;
	return -1;
}

ssize_t sendfile64(int out_fd, int in_fd, int64_t *offset, size_t count)
{
	(void)out_fd;
	(void)in_fd;
	(void)offset;
	(void)count;
	errno = ENOSYS;
	return -1;
}

int sched_getaffinity(pid_t pid, size_t cpusetsize, void *mask)
{
	unsigned long *bits = mask;

	(void)pid;
	if (mask == NULL || cpusetsize < sizeof(unsigned long)) {
		errno = EINVAL;
		return -1;
	}
	memset(mask, 0, cpusetsize);
	/* Cores 0-2. Core 3 is reserved for the system. */
	bits[0] = 0x7;
	return 0;
}

long sysconf(int name)
{
	switch (name) {
	case 30: /* _SC_PAGESIZE */
		return 4096;
	case 83: /* _SC_NPROCESSORS_CONF */
	case 84: /* _SC_NPROCESSORS_ONLN */
		return 3;
	case 75: /* _SC_THREAD_STACK_MIN */
		return 65536;
	case 2: /* _SC_CLK_TCK */
		return 100;
	default:
		errno = EINVAL;
		return -1;
	}
}

int pause(void)
{
	/* Rust's stack-overflow lock calls this in a retry loop. A real
	   pause() would sleep until a signal that never arrives. */
	svcSleepThread(1000);
	return 0;
}

int pthread_setname_np(pthread_t thread, const char *name)
{
	(void)thread;
	(void)name;
	return 0;
}

int __real_clock_gettime(clockid_t clock_id, struct timespec *tp);

int __wrap_clock_gettime(clockid_t clock_id, struct timespec *tp)
{
	/* Rust's linux-gnu libc uses Linux ids: REALTIME is 0, MONOTONIC is 1.
	   libnx only accepts newlib REALTIME (1) and MONOTONIC (4). Any other
	   id returns EINVAL, and SystemTime::now unwraps that. Both accepted
	   ids are the same unix clock, so the fallback is REALTIME. */
	if (clock_id != CLOCK_REALTIME && clock_id != CLOCK_MONOTONIC)
		clock_id = CLOCK_REALTIME;
	return __real_clock_gettime(clock_id, tp);
}

int clock_nanosleep(int clock_id, int flags, const struct timespec *req, struct timespec *rem)
{
	uint64_t ns;

	if (req == NULL || req->tv_nsec < 0 || req->tv_nsec >= 1000000000L) {
		errno = EINVAL;
		return EINVAL;
	}
	ns = (uint64_t)req->tv_sec * 1000000000ull + (uint64_t)req->tv_nsec;
	/* TIMER_ABSTIME. "now" has to be the same clock the caller read:
	   libnx's CLOCK_MONOTONIC is unix time plus uptime, not ticks since boot. */
	if (flags & 1) {
		struct timespec now_ts;
		uint64_t now;

		if (clock_gettime(clock_id, &now_ts) != 0)
			return EINVAL;
		now = (uint64_t)now_ts.tv_sec * 1000000000ull + (uint64_t)now_ts.tv_nsec;
		if (ns <= now)
			ns = 0;
		else
			ns -= now;
	}
	while (ns > 0) {
		uint64_t step = ns > 1000000000ull ? 1000000000ull : ns;

		svcSleepThread(step);
		ns -= step;
	}
	if (rem != NULL) {
		rem->tv_sec = 0;
		rem->tv_nsec = 0;
	}
	return 0;
}

int ftruncate64(int fd, int64_t length)
{
	return ftruncate(fd, (off_t)length);
}

int fstatat64(int dirfd, const char *path, struct linux_stat64 *buf, int flags)
{
	struct stat st;
	int rc;

	/* AT_FDCWD. A real directory fd has no newlib fstatat. */
	if (dirfd != -100) {
		errno = ENOSYS;
		return -1;
	}
	/* AT_SYMLINK_NOFOLLOW */
	if (flags & 0x100)
		rc = lstat(path, &st);
	else
		rc = stat(path, &st);
	if (rc != 0)
		return -1;
	copy_stat64(buf, &st);
	return 0;
}

int dl_iterate_phdr(void *callback, void *data)
{
	(void)callback;
	(void)data;
	return 0;
}

const char *gnu_get_libc_version(void)
{
	/* Old enough that Rust stays on the portable paths. */
	return "2.17";
}

int __res_init(void)
{
	return 0;
}

void *dlsym(void *handle, const char *symbol)
{
	(void)handle;
	(void)symbol;
	return NULL;
}

/* Rust's linux target reads the thread pointer with `mrs tpidr_el0`.
   Horizon leaves that register at 0 and libnx keeps the ELF thread
   pointer at the end of the kernel TLS page (ThreadVars.tls_tp). A load
   at tpidr+0x198 then faults at address 0x198. */
void switch_bind_thread_pointer(void)
{
	void *tp = *(void **)((unsigned char *)armGetTls() + 0x1f8);

	__asm__ volatile("msr tpidr_el0, %0" :: "r"(tp) : "memory");
}

__attribute__((constructor))
static void switch_bind_thread_pointer_ctor(void)
{
	switch_bind_thread_pointer();
}

struct switch_thread_start {
	void *(*fn)(void *);
	void *arg;
};

static void *switch_thread_start(void *raw)
{
	struct switch_thread_start *start = raw;
	void *(*fn)(void *) = start->fn;
	void *arg = start->arg;

	free(start);
	switch_bind_thread_pointer();
	return fn(arg);
}

int __real_pthread_create(pthread_t *thread, const pthread_attr_t *attr,
	void *(*fn)(void *), void *arg);

ssize_t __real_write(int fd, const void *buf, size_t count);

ssize_t __wrap_write(int fd, const void *buf, size_t count)
{
	return __real_write(fd, buf, count);
}

#define SWITCH_POOL 4
#define SWITCH_JOB_MAGIC 0x504f4f4cu

struct switch_job {
	uint32_t magic;
	int used;
	int done;
	int detached;
	void *(*fn)(void *);
	void *arg;
	pthread_cond_t cv;
};

struct switch_worker {
	int alive;
	struct switch_job *job;
	pthread_cond_t cv;
};

static struct switch_job switch_jobs[SWITCH_POOL];
static struct switch_worker switch_workers[SWITCH_POOL];
static pthread_mutex_t switch_pool_mu = PTHREAD_MUTEX_INITIALIZER;
static int switch_pool_ready;

static int switch_job_p(pthread_t thread)
{
	struct switch_job *job = (struct switch_job *)thread;

	if (job < switch_jobs || job >= switch_jobs + SWITCH_POOL)
		return 0;
	return job->used && job->magic == SWITCH_JOB_MAGIC;
}

static void switch_clear_rust_thread_tls(void)
{
	uintptr_t tp;

	/* This rustc's local-exec TLS. thread_start aborts when CURRENT at
	   tpidr+0x1b0 is already set, so a reused worker has to look new. */
	__asm__ volatile("mrs %0, tpidr_el0" : "=r"(tp));
	if (tp == 0)
		return;
	*(uintptr_t *)(tp + 0x1b0) = 0;
	*(uintptr_t *)(tp + 0x1b8) = 0;
}

static void switch_job_release(struct switch_job *job)
{
	job->used = 0;
	job->magic = 0;
	pthread_cond_destroy(&job->cv);
}

static void *switch_pool_main(void *raw)
{
	struct switch_worker *self = raw;

	switch_bind_thread_pointer();
	pthread_mutex_lock(&switch_pool_mu);
	for (;;) {
		struct switch_job *job;

		while (self->job == NULL)
			pthread_cond_wait(&self->cv, &switch_pool_mu);
		job = self->job;
		pthread_mutex_unlock(&switch_pool_mu);

		switch_clear_rust_thread_tls();
		(void)job->fn(job->arg);

		pthread_mutex_lock(&switch_pool_mu);
		job->done = 1;
		pthread_cond_broadcast(&job->cv);
		self->job = NULL;
		if (job->detached)
			switch_job_release(job);
	}
	return NULL;
}

void switch_thread_pool_init(void)
{
	pthread_attr_t attr;
	int i;
	int n = 0;
	char buf[32];

	if (switch_pool_ready)
		return;
	switch_pool_ready = 1;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 128 * 1024);
	for (i = 0; i < SWITCH_POOL; i++) {
		pthread_t tid;

		pthread_cond_init(&switch_workers[i].cv, NULL);
		if (__real_pthread_create(&tid, &attr, switch_pool_main,
		    &switch_workers[i]) != 0)
			continue;
		switch_workers[i].alive = 1;
		n++;
	}
	pthread_attr_destroy(&attr);
	snprintf(buf, sizeof(buf), "thread pool %d", n);
	switch_trace(buf);
}

static struct switch_job *switch_job_alloc(void *(*fn)(void *), void *arg)
{
	int i;

	for (i = 0; i < SWITCH_POOL; i++) {
		if (switch_jobs[i].used)
			continue;
		switch_jobs[i].used = 1;
		switch_jobs[i].magic = SWITCH_JOB_MAGIC;
		switch_jobs[i].done = 0;
		switch_jobs[i].detached = 0;
		switch_jobs[i].fn = fn;
		switch_jobs[i].arg = arg;
		pthread_cond_init(&switch_jobs[i].cv, NULL);
		return &switch_jobs[i];
	}
	return NULL;
}

int __real_pthread_join(pthread_t thread, void **retval);

int __wrap_pthread_join(pthread_t thread, void **retval)
{
	struct switch_job *job;

	if (!switch_job_p(thread))
		return __real_pthread_join(thread, retval);
	job = (struct switch_job *)thread;
	pthread_mutex_lock(&switch_pool_mu);
	while (!job->done)
		pthread_cond_wait(&job->cv, &switch_pool_mu);
	switch_job_release(job);
	pthread_mutex_unlock(&switch_pool_mu);
	if (retval != NULL)
		*retval = NULL;
	return 0;
}

int __real_pthread_detach(pthread_t thread);

int __wrap_pthread_detach(pthread_t thread)
{
	struct switch_job *job;

	if (!switch_job_p(thread))
		return __real_pthread_detach(thread);
	job = (struct switch_job *)thread;
	pthread_mutex_lock(&switch_pool_mu);
	job->detached = 1;
	if (job->done)
		switch_job_release(job);
	pthread_mutex_unlock(&switch_pool_mu);
	return 0;
}

int __wrap_pthread_create(pthread_t *thread, const pthread_attr_t *attr,
	void *(*fn)(void *), void *arg)
{
	struct switch_thread_start *start;
	struct switch_job *job;
	pthread_attr_t capped;
	const pthread_attr_t *use = attr;
	int have_cap = 0;
	int i;
	int rc;

	if (!switch_pool_ready)
		switch_thread_pool_init();

	/* Threads created after the game is loaded come back ENOMEM: the
	   stack region is already taken. Workers started at boot still have
	   theirs. Running the routine on the caller aborts, because Rust
	   refuses a second thread::current on the same thread. */
	pthread_mutex_lock(&switch_pool_mu);
	for (i = 0; i < SWITCH_POOL; i++) {
		if (!switch_workers[i].alive || switch_workers[i].job != NULL)
			continue;
		job = switch_job_alloc(fn, arg);
		if (job == NULL)
			break;
		switch_workers[i].job = job;
		pthread_cond_signal(&switch_workers[i].cv);
		pthread_mutex_unlock(&switch_pool_mu);
		*thread = (pthread_t)job;
		return 0;
	}
	pthread_mutex_unlock(&switch_pool_mu);

	if (attr != NULL && pthread_attr_init(&capped) == 0) {
		int detach = PTHREAD_CREATE_JOINABLE;

		pthread_attr_getdetachstate(attr, &detach);
		pthread_attr_setdetachstate(&capped, detach);
		pthread_attr_setstacksize(&capped, 128 * 1024);
		use = &capped;
		have_cap = 1;
	}

	start = malloc(sizeof(*start));
	if (start == NULL) {
		if (have_cap)
			pthread_attr_destroy(&capped);
		return EAGAIN;
	}
	start->fn = fn;
	start->arg = arg;
	rc = __real_pthread_create(thread, use, switch_thread_start, start);
	if (have_cap)
		pthread_attr_destroy(&capped);
	if (rc != 0) {
		free(start);
		switch_trace("pthread pool full");
	}
	return rc;
}

/* Dynarmic writes host code and jumps to it. Horizon never grants write and
   execute on the same address, so each block is a libnx JIT with a RW alias
   and an RX alias. */
#define SWITCH_JIT_SLOTS 4

static Jit switch_jits[SWITCH_JIT_SLOTS];
static int switch_jit_on[SWITCH_JIT_SLOTS];
static pthread_mutex_t switch_jit_mu = PTHREAD_MUTEX_INITIALIZER;

static void switch_jit_log(const char *msg)
{
	FILE *log;

	mkdir("sdmc:/switch", 0755);
	mkdir("sdmc:/switch/rombundler", 0755);
	log = fopen("sdmc:/switch/rombundler/error.log", "w");
	if (!log)
		return;
	fputs(msg, log);
	fputc('\n', log);
	fclose(log);
	fsdevCommitDevice("sdmc");
}

static Jit *switch_jit_at(void *p)
{
	uintptr_t addr = (uintptr_t)p;
	int i;

	for (i = 0; i < SWITCH_JIT_SLOTS; i++) {
		Jit *j = &switch_jits[i];
		uintptr_t rw;
		uintptr_t rx;

		if (!switch_jit_on[i] || j->rw_addr == NULL || j->rx_addr == NULL)
			continue;
		rw = (uintptr_t)j->rw_addr;
		rx = (uintptr_t)j->rx_addr;
		if ((addr >= rw && addr < rw + j->size) || (addr >= rx && addr < rx + j->size))
			return j;
	}
	return NULL;
}

void *switch_jit_create(size_t size, void **rx)
{
	int slot = -1;
	int i;
	Result rc;
	char buf[160];

	if (rx != NULL)
		*rx = NULL;
	snprintf(buf, sizeof(buf), "jit create %zu", size);
	switch_trace(buf);
	pthread_mutex_lock(&switch_jit_mu);
	for (i = 0; i < SWITCH_JIT_SLOTS; i++) {
		if (!switch_jit_on[i]) {
			slot = i;
			break;
		}
	}
	if (slot < 0) {
		pthread_mutex_unlock(&switch_jit_mu);
		switch_jit_log("JIT: no free code-memory slot");
		return NULL;
	}
	memset(&switch_jits[slot], 0, sizeof(switch_jits[slot]));
	rc = jitCreate(&switch_jits[slot], size);
	if (R_FAILED(rc) || switch_jits[slot].rw_addr == NULL || switch_jits[slot].rx_addr == NULL) {
		pthread_mutex_unlock(&switch_jit_mu);
		snprintf(buf, sizeof(buf),
			"JIT: jitCreate failed for %zu bytes (result 0x%x)", size, rc);
		switch_jit_log(buf);
		return NULL;
	}
	switch_jit_on[slot] = 1;
	if (rx != NULL)
		*rx = switch_jits[slot].rx_addr;
	snprintf(buf, sizeof(buf), "jit ok rw=%p rx=%p type=%d",
		switch_jits[slot].rw_addr, switch_jits[slot].rx_addr,
		(int)switch_jits[slot].type);
	pthread_mutex_unlock(&switch_jit_mu);
	switch_trace(buf);
	return switch_jits[slot].rw_addr;
}

/* Dynarmic's own lock is generated code that uses WFE and exclusive
   accesses. Those can sit forever when the code runs from a JIT alias.
   The generated stub calls these instead. */
void switch_spinlock_lock(volatile int *p)
{
	while (__sync_lock_test_and_set(p, 1)) {
		while (*p)
			svcSleepThread(20000);
	}
}

void switch_spinlock_unlock(volatile int *p)
{
	__sync_lock_release(p);
}

void switch_jit_destroy(void *rw)
{
	int i;

	pthread_mutex_lock(&switch_jit_mu);
	for (i = 0; i < SWITCH_JIT_SLOTS; i++) {
		if (switch_jit_on[i] && switch_jits[i].rw_addr == rw) {
			jitClose(&switch_jits[i]);
			switch_jit_on[i] = 0;
			break;
		}
	}
	pthread_mutex_unlock(&switch_jit_mu);
}

void switch_jit_writable(void *rw)
{
	Jit *j;

	pthread_mutex_lock(&switch_jit_mu);
	j = switch_jit_at(rw);
	if (j != NULL && j->type == JitType_SetProcessMemoryPermission)
		jitTransitionToWritable(j);
	pthread_mutex_unlock(&switch_jit_mu);
}

void switch_jit_executable(void *rw)
{
	Jit *j;

	pthread_mutex_lock(&switch_jit_mu);
	j = switch_jit_at(rw);
	if (j != NULL && j->type == JitType_SetProcessMemoryPermission) {
		armDCacheFlush(j->rw_addr, j->size);
		jitTransitionToExecutable(j);
		armICacheInvalidate(j->rx_addr, j->size);
	}
	pthread_mutex_unlock(&switch_jit_mu);
}

void switch_jit_flush(void *p, size_t size)
{
	Jit *j;
	uintptr_t addr = (uintptr_t)p;
	uintptr_t off;
	void *rw;
	void *rx;

	pthread_mutex_lock(&switch_jit_mu);
	j = switch_jit_at(p);
	if (j == NULL) {
		pthread_mutex_unlock(&switch_jit_mu);
		return;
	}
	if (addr >= (uintptr_t)j->rx_addr && addr < (uintptr_t)j->rx_addr + j->size)
		off = addr - (uintptr_t)j->rx_addr;
	else
		off = addr - (uintptr_t)j->rw_addr;
	if (off >= j->size) {
		pthread_mutex_unlock(&switch_jit_mu);
		return;
	}
	if (size > j->size - off)
		size = j->size - off;
	rw = (unsigned char *)j->rw_addr + off;
	rx = (unsigned char *)j->rx_addr + off;
	armDCacheFlush(rw, size);
	if (j->type == JitType_CodeMemory || j->is_executable)
		armICacheInvalidate(rx, size);
	pthread_mutex_unlock(&switch_jit_mu);
}
