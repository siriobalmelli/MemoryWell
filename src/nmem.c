#include <nmem.h>
#include <zed_dbg.h>


/*	nmem_file()
Map a file at 'path' read-only.
Populate '*out'.
Returns 0 on success.
*/
int		nmem_file(const char *path, struct nmem *out)
{
	int err_cnt = 0;
	Z_die_if(!path || !out, "args");

	/* open source file
	This requires that mmap () protection also be read-only.
	*/
	out->o_flags = O_RDONLY;
	out->fd = open(path, out->o_flags);
	Z_die_if(out->fd == -1, "fd %d; open %s", out->fd, path);

	/* get length */
	Z_die_if((
		out->len = lseek(out->fd, 0, SEEK_END)
		) == -1, "SEEK_END of '%s' gives %ld", path, out->len);

	/* mmap file */
	Z_die_if((
		out->mem = mmap(NULL, out->len, PROT_READ, MAP_PRIVATE, out->fd, 0)
		) == MAP_FAILED, "map %s sz %zu fd %"PRId32, path, out->len, out->fd);

	return 0;
out:
	nmem_free(out, NULL);
	return err_cnt;
}

/*	nmem_alloc()
Map 'len' bytes of memory.
If 'tmp_dir' is given, map this as a temp file on disk;
	so that it may be "delivered" on close (nmem_free).
Otherwise map anonymous memory.

In both cases a valid fd is obtained, so that splice()
	calls will succeed into and out of this memory region.
*/
int		nmem_alloc(size_t len, const char *tmp_dir, struct nmem *out)
{
	int err_cnt = 0;
	Z_die_if(!len || !out, "args");
	out->len = len;

	if (!tmp_dir) {
		/* memfd is preferable: splice in and out of virtual memory
			without disk writeback.
		*/
		#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,17,0)
		out->o_flags = 0;
		char name[16];
		snprintf(name, 16, "nmem_%zu", out->len);
		Z_die_if((
			out->fd = syscall(__NR_memfd_create, name, out->o_flags)
			) == -1, "");
		/* fallback: create a temp file on disk */
		#else
		tmp_dir = "/tmp";
		#endif
	}

	/* open an fd */
	if (tmp_dir) {
		out->o_flags = O_RDWR | O_TMPFILE;
		Z_die_if((
			out->fd = open(tmp_dir, out->o_flags, NMEM_PERMS)
			) == -1, "failed to open temp file in %s", tmp_dir);
	}

	/* size and map */
	Z_die_if(ftruncate(out->fd, out->len), "len=%ld", out->len);
	Z_die_if((
		out->mem = mmap(NULL, out->len, PROT_READ | PROT_WRITE, MAP_SHARED, out->fd, 0)
		) == MAP_FAILED, "map sz %zu fd %"PRId32, out->len, out->fd);

	return 0;
out:
	nmem_free(out, NULL);
	return err_cnt;
}

/*	nmem_free()
Free the memory pointed to by '*nm'; clear '*nm'
*/
void		nmem_free(struct nmem *nm, const char *deliver_path)
{
	if (!nm)
		return;

	/* unmap */
	if (nm->mem && nm->mem != MAP_FAILED)
		munmap(nm->mem, nm->len);
	nm->mem = NULL;

	/* deliver if requested */
	if (deliver_path && (nm->o_flags & O_TMPFILE)) {
		char src[32];
		snprintf(src, 32, "/proc/self/fd/%d", nm->fd);
		Z_err_if(
			linkat(AT_FDCWD, src, AT_FDCWD, deliver_path, AT_SYMLINK_FOLLOW)
			, "%s -> %s", src, deliver_path);
	}

	/* close */
	if (nm->fd != -1)
		close(nm->fd);
	nm->o_flags = nm->fd = -1;
}


/*	nmem_in_splice()
Splice 'len' bytes from 'fd_pipe_from' into 'nm' at 'offset'.
Returns number of bytes pushed; which may be less than requested.
*/
NLC_PUBLIC size_t	nmem_in_splice(struct nmem	*nm,
					size_t		offset,
					size_t		len,
					int		fd_pipe_from)
{
	Z_die_if(!nm || fd_pipe_from < 1, "args");

	ssize_t ret = splice(fd_pipe_from, NULL, nm->fd, (loff_t*)&offset,
				len, NMEM_SPLICE_FLAGS);
	Z_die_if(ret < 0, "len %zu", len);

	return ret;
out:
	return 0;
}

/*	nmem_out_splice()
Splice 'len' bytes from 'nm' into 'fd_pipe_to' at 'offset' (if applicable).
Returns number of bytes pushed; which may be less than requested.
*/
NLC_PUBLIC size_t	nmem_out_splice(struct nmem	*nm,
					size_t		offset,
					size_t		len,
					int		fd_pipe_to)
{
	Z_die_if(!nm || fd_pipe_to < 1, "args");

	ssize_t ret = splice(nm->fd, (loff_t*)&offset, fd_pipe_to, NULL,
				len, NMEM_SPLICE_FLAGS);
	Z_die_if(ret < 0, "len %zu", len);

	return ret;
out:
	return 0;
}
