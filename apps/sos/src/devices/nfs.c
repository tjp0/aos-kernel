#include <autoconf.h>
#include <clock/clock.h>
#include <copy.h>
#include <filetable.h>
#include <lwip/ip_addr.h>
#include <nfs/nfs.h>
#include <sos.h>
#include <utils/math.h>
#include <utils/picoro.h>
#define verbose 0
#include <sys/debug.h>
#include <sys/kassert.h>

#define NFS_TIMEOUT 1000 * 100  // 100ms
#define NFS_BUFFER_SIZE 7000

#define POSIX_USER_BITSHIFT 6

static fhandle_t mounted_fs;

struct nfs_lookup_callback_t {
	enum nfs_stat status;
	fhandle_t* fh;
	fattr_t* fattr;
};
static void nfs_lookup_callback(uintptr_t token, enum nfs_stat status,
								fhandle_t* fh, fattr_t* fattr) {
	coro ret_co = (coro)token;
	struct nfs_lookup_callback_t cb = {
		.status = status, .fh = fh, .fattr = fattr};
	resume(ret_co, &cb);
}

struct nfs_read_callback_t {
	enum nfs_stat status;
	fattr_t* fattr;
	int count;
	void* data;
};

static void nfs_read_callback(uintptr_t token, enum nfs_stat status,
							  fattr_t* fattr, int count, void* data) {
	coro ret_co = (coro)token;
	struct nfs_read_callback_t cb = {
		.status = status, .fattr = fattr, .count = count, .data = data};
	resume(ret_co, &cb);
}

struct nfs_write_callback_t {
	enum nfs_stat status;
	fattr_t* fattr;
	int count;
};

static void nfs_write_callback(uintptr_t token, enum nfs_stat status,
							   fattr_t* fattr, int count) {
	coro ret_co = (coro)token;
	struct nfs_write_callback_t cb = {
		.status = status, .fattr = fattr, .count = count};
	resume(ret_co, &cb);
}

struct nfs_create_callback_t {
	enum nfs_stat status;
	fhandle_t* fh;
	fattr_t* fattr;
};

static void nfs_create_callback(uintptr_t token, enum nfs_stat status,
								fhandle_t* fh, fattr_t* fattr) {
	coro ret_co = (coro)token;
	struct nfs_create_callback_t cb = {
		.status = status, .fattr = fattr, .fh = fh};
	resume(ret_co, &cb);
}

struct nfs_readdir_callback_t {
	enum nfs_stat status;
	int num_files;
	char** file_names;
	nfscookie_t nfscookie;
};

static void nfs_readdir_callback(uintptr_t token, enum nfs_stat status,
								 int num_files, char* file_names[],
								 nfscookie_t nfscookie) {
	coro ret_co = (coro)token;
	struct nfs_readdir_callback_t cb = {.status = status,
										.num_files = num_files,
										.file_names = file_names,
										.nfscookie = nfscookie};
	resume(ret_co, &cb);
}

static void nfs_timeout_awaken(uint32_t id, void* data) {
	(void)id;
	(void)data;
	nfs_timeout();
	int err = register_timer(NFS_TIMEOUT, nfs_timeout_awaken, NULL);
	conditional_panic(err < 0, "NFS timer failed to reregister");
}

int nfs_dev_init(void) {
	ip_addr_t ip;
	ip.addr = ipaddr_addr(CONFIG_SOS_GATEWAY);
	trace(5);
	rpc_stat_t ret = nfs_init(&ip);
	trace(5);
	if (ret != RPC_OK) {
		dprintf(0, "NFS Failed, ret: %u\n", ret);
		trace(5);
		return -1;
	}
	trace(5);
	int err = register_timer(NFS_TIMEOUT, nfs_timeout_awaken, NULL);
	trace(5);
	conditional_panic(err < 0, "Failed to register NFS timer");
	if (verbose > 0) {
		trace(5);
		nfs_print_exports();
	}
	trace(5);
	ret = nfs_mount(CONFIG_SOS_NFS_DIR, &mounted_fs);
	trace(5);
	if (ret != RPC_OK) {
		trace(5);
		dprintf(0, "NFS failed to mount, ret: %u\n", ret);
		return -1;
	}
	trace(5);
	dprintf(0, "NFS share mounted\n");
	return 0;
};

int nfs_dev_stat(struct vspace* vspace, const char* filename,
				 vaddr_t sos_stat) {
	enum rpc_stat ret = nfs_lookup(&mounted_fs, filename, nfs_lookup_callback,
								   (uintptr_t)current_coro());
	if (ret != RPC_OK) {
		trace(2);
		return -1;
	}

	struct nfs_lookup_callback_t* callback = yield(NULL);
	if (callback->status != NFS_OK) {
		return -1;
	}
	sos_stat_t local_stat;
	fattr_t* f = callback->fattr;
	local_stat.st_size = f->size;
	local_stat.st_atime = f->atime.seconds * 1000 + f->atime.useconds / 1000;
	local_stat.st_ctime = f->ctime.seconds * 1000 + f->ctime.useconds / 1000;
	if (f->type == NFREG) {
		local_stat.st_type = ST_FILE;
	} else {
		local_stat.st_type = ST_SPECIAL;
	}
	local_stat.st_fmode = (f->mode >> POSIX_USER_BITSHIFT);

	if (copy_sos2vspace(&local_stat, sos_stat, vspace, sizeof(local_stat), 0) <
		0) {
		return -1;
	}
	return 0;
}

int nfs_dev_getdirent(struct vspace* vspace, int pos, vaddr_t name,
					  size_t nbyte) {
	/* This is really slow if there's a lot of files, with many round trips, but
	 * readdir is a relatively unused operation */
	nfscookie_t cookie = 0;
	struct nfs_readdir_callback_t* cb;
	while (1) {
		enum rpc_stat stat =
			nfs_readdir(&mounted_fs, cookie, nfs_readdir_callback,
						(uintptr_t)current_coro());
		if (stat != RPC_OK) {
			return -1;
		}
		cb = yield(NULL);

		if (cb->num_files == 0) {
			return 0;
		}
		if (pos < cb->num_files) {
			/* We've found the right position */
			break;
		}
		cookie = cb->nfscookie;
		pos -= cb->num_files;
	}
	char* str = cb->file_names[pos];

	size_t len = strlen(str) + 1;
	char* copy = malloc(len);
	if (copy == NULL) {
		return -1;
	}
	strcpy(copy, str);
	copy[nbyte - 1] = '\0';
	dprintf(0, "Found dir entity %s\n", str);

	return copy_sos2vspace(copy, name, vspace, (nbyte < len ? nbyte : len),
						   COPY_RETURNWRITTEN);
}

struct nfs_fd_data {
	fhandle_t fhandle;
	fattr_t fattr;
};

static int nfs_dev_read(struct fd* fd, struct vspace* vspace, vaddr_t procbuf,
						size_t length) {
	trace(2);
	char buffer[NFS_BUFFER_SIZE];
	kassert(fd != NULL);
	struct nfs_fd_data* data = fd->data;
	size_t read = 0;
	while (read < length) {
		size_t toread = (length - read <= NFS_BUFFER_SIZE) ? length - read
														   : NFS_BUFFER_SIZE;
		enum rpc_stat stat =
			nfs_read(&data->fhandle, fd->offset + read, toread,
					 nfs_read_callback, (uintptr_t)current_coro());
		if (stat != RPC_OK) {
			return -stat;
		}
		trace(2);
		struct nfs_read_callback_t* callback = yield(NULL);
		int call_count = callback->count;

		if (callback->status != NFS_OK) {
			trace(2);
			return -callback->status;
		}
		memcpy(buffer, callback->data, call_count);
		trace(2);
		if (copy_sos2vspace(buffer, procbuf + read, vspace, call_count, 0) <
			0) {
			trace(2);
			goto read_err;
		}
		read += call_count;
		trace(2);
		if (call_count == 0) {
			trace(2);
			break;
		}
	}
	fd->offset += read;
	trace(2);
	return read;
read_err:
	trace(2);
	return -1;
}

// parameters stolen from serial_write serial.c
// filetable.h
// func pointers

static int nfs_dev_write(struct fd* fd, struct vspace* vspace, vaddr_t procbuf,
						 size_t length) {
	/*
	 * write the callback struct and function
	 * this function
	 * copy from vspace to sos
	 * the data the needs to be written
	 * passed via vspace and procbuf
	 * copy_vspace2sos(....)
	 * copy that into a buffer on the stack
	 * basically call nfsrite on the buffer
	 * do it until all the data is written
	 * don't use malloc to get the buffer
	 * malloc is realtively slow
	 * alloce t it staitclal on the stack
	 * and copy at most that many bytes
	 */

	char buffer[NFS_BUFFER_SIZE];
	struct nfs_fd_data* data = fd->data;
	unsigned int num_written = 0;

	trace(2);
	while (num_written < length) {
		trace(2);

		unsigned int amt2write = MIN(sizeof(buffer), length - num_written);
		// copy data from vspace to sos into buffer
		if (copy_vspace2sos(procbuf + num_written, buffer, vspace, amt2write,
							0) < 0) {
			trace(2);
			return -1;
		}
		// call nfs write with buffer
		enum rpc_stat stat =
			nfs_write(&data->fhandle, fd->offset + num_written, amt2write,
					  buffer, nfs_write_callback, (uintptr_t)current_coro());

		if (stat != RPC_OK) {
			trace(2);
			return -stat;
		}

		struct nfs_write_callback_t* callback = yield(NULL);

		if (callback->status != NFS_OK) {
			trace(2);
			return -callback->status;
		}

		num_written += callback->count;
	}

	fd->offset += num_written;
	trace(2);

	return num_written;
}

static int nfs_dev_close(struct fd* fd) {
	assert(fd != NULL);
	free(fd->data);
	memset(fd, 0, sizeof(struct fd));
	return 0;
}

int nfs_dev_create(const char* name, const sattr_t* sattr, fhandle_t* fh) {
	/*
	 * An asynchronous function used for creating a new file on the NFS file
	 * server. This function is used to create a new file named "name" with the
	 * attributes "sattr". On completion, the provided callback (@ref
	 * nfs_create_cb_t) will be executed with "token" passed, unmodified, as an
	 * argument.
	 * @param[in] pfh      An NFS file handle (@ref fhandle_t) to the directory
	 *                     that should contain the newly created file.
	 * @param[in] name     The NULL terminated name of the file to create.
	 * @param[in] sattr    The attributes which the file should posses after
	 *                     creation.
	 * @param[in] callback An @ref nfs_create_cb_t callback function to call
	 * once a response arrives.
	 * @param[in] token    A token to pass, unmodified, to the callback
	 * function.
	 * @return             RPC_OK if the request was successfully sent.
	 * Otherwise an appropriate error code will be returned. "callback" will be
	 * called once the response to this request has been
	 *                     received.
	 */
	// enum rpc_stat nfs_create(const fhandle_t* pfh, const char* name,
	// 						 const sattr_t* sattr, nfs_create_cb_t callback,
	// 						 uintpt	r_t token);

	enum rpc_stat zr = nfs_create(&mounted_fs, name, sattr, nfs_create_callback,
								  (uintptr_t)current_coro());
	enum rpc_stat ret = zr;  // clang format breaks sublime colors if ^ too long

	trace(2);
	if (ret != RPC_OK) {
		return -ret;
	}
	struct nfs_create_callback_t* cb = yield(NULL);
	trace(2);
	if (cb->status != NFS_OK) {
		trace(2);
		dprintf(1, "NFS: Failed to create file: %s, with reason %u\n", name,
				cb->status);

		trace(2);
		return -cb->status;
	}
	*fh = *cb->fh;
	return 0;
}

int nfs_dev_open(struct fd* fd, char* name, int flags) {
	trace(2);
	uint32_t struct_size = sizeof(struct nfs_fd_data);

	struct nfs_fd_data* data = (struct nfs_fd_data*)malloc(struct_size);

	if (data == NULL) {
		trace(2);
		return -RPCERR_NOMEM;
	}
	trace(2);
	enum rpc_stat ret = nfs_lookup(&mounted_fs, name, nfs_lookup_callback,
								   (uintptr_t)current_coro());

	// Check the flags to see if it was readonly
	// if so, don't make it
	if (ret != RPC_OK) {
		trace(2);
		free(data);
		return -ret;
	}
	trace(2);
	struct nfs_lookup_callback_t* cb = yield(NULL);

	if (cb->status == NFSERR_NOENT && !(flags & !FM_READ)) {
		// create the file

		sattr_t sat = {
			.mode = ((FM_EXEC | FM_WRITE | FM_READ) << POSIX_USER_BITSHIFT),
			.uid = 0,
			.gid = 0,
			.size = 0,
			.atime = (timeval_t){.seconds = 0, .useconds = 0},
			.mtime = (timeval_t){.seconds = 0, .useconds = 0}};

		fhandle_t fh;
		trace(2);
		int stat = nfs_dev_create(name, &sat, &fh);
		trace(2);

		if (stat != 0) {
			// error
			free(data);
			return -stat;
		}
		data->fhandle = fh;
		trace(2);
	} else if (cb->status != NFS_OK) {
		trace(2);
		dprintf(1, "NFS: Failed to open file: %s, with reason %u\n", name,
				cb->status);
		trace(2);
		free(data);
		trace(2);
		return -cb->status;
	}
	trace(2);
	data->fhandle = *cb->fh;
	trace(2);
	data->fattr = *cb->fattr;
	trace(2);
	fd->used = 1;
	fd->data = data;
	fd->offset = 0;
	fd->dev_read = &nfs_dev_read;
	fd->dev_close = &nfs_dev_close;
	fd->dev_write = &nfs_dev_write;
	return 0;
}

// if the NFSERR_NOENT and the flag to create is given then call nfs_create
