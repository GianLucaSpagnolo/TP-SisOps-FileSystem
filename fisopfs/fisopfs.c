#define FUSE_USE_VERSION 30

#include <fuse.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "fs_lib.c"

fs_t *fs;

// PERSISTENCIA
const char *path = "fs.fisopfs";
int save = 0;

// # OPERACIONES DEL SISTEMA DE ARCHIVOS

// ## Creación de directorios
//
// (Con al menos un nivel de recursión)(ej. mkdir ./dir1/dir2/ )
//
// Create a directory with the given name. The directory permissions are encoded
// in mode. See mkdir(2) for details. This function is needed for any reasonable
// read/write filesystem.
//
// Example: mkdir [dir]
//
static int
fisopfs_mkdir(const char *path, mode_t mode)
{
	printf("[debug] fisopfs_mkdir - path: %s\n", path);
	return fs_mkdir(fs, path, mode);
}

// ## Creación de archivos
//
// Create a file with the given name. The file permissions are encoded in mode.
// See open(2) for information about how to encode mode. If the file already
// exists, this function should return -EEXIST.
//
// Example: touch [file]
//
static int
fisopfs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
	printf("[debug] fisopfs_create - path: %s\n", path);
	return fs_create(fs, path, mode);
}

// ## Cambio de tiempo de acceso y modificación
//
// Update the last access time of the given object from ts[0] and the last modification
// time from ts[1]. Both time specifications are given to nanosecond resolution, but
// your filesystem doesn't have to be that precise; see utimensat(2) for full details.
// Note that the time specifications are allowed to have certain special values;
// however, I don't know if FUSE functions have to support them. This function isn't
// necessary but is nice to have in a fully functional filesystem.
//
// Example: touch [file]
//
static int
fisopfs_utimens(const char *path, const struct timespec ts[2])
{
	printf("[debug] fisopfs_utimens - path: %s\n", path);
	return fs_utimens(fs, path, ts);
}

// ## Lectura de directorios
//
// Return one or more directory entries (struct dirent) to the caller. This is
// one of the most complex FUSE functions. It is related to, but not identical
// to, the readdir(2) and getdents(2) system calls, and the readdir(3) library
// function. Because of its complexity, it is described separately below.
// Required for essentially any filesystem, since it's what makes ls and a whole
// bunch of other things work.
//
// Example: ls [dir]
//
static int
fisopfs_readdir(const char *path,
                void *buffer,
                fuse_fill_dir_t filler,
                off_t offset,
                struct fuse_file_info *fi)
{
	printf("[debug] fisopfs_readdir - path: %s\n", path);

	// Los directorios '.' y '..'
	filler(buffer, ".", NULL, 0);
	filler(buffer, "..", NULL, 0);

	fs_d_entry_t *dir = get_dir(fs, path);
	if (!dir) {
		printf("[debug] fisopfs_readdir - directory %s not found\n", path);
		return -ENOENT;
	}

	for (int i = 0; i < fs->f_size; i++) {
		if (strcmp(fs->files[i].entry->path, dir->path) == 0) {
			filler(buffer, basename(fs->files[i].path), NULL, 0);
		}
	}

	for (int i = 0; i < fs->d_size; i++) {
		if (fs->directories[i].d_parent &&
		    strcmp(fs->directories[i].d_parent->path, dir->path) == 0) {
			filler(buffer, basename(fs->directories[i].path), NULL, 0);
		}
	}

	dir->time_last_access = time(NULL);
	return EXIT_SUCCESS;
}

// ## Lectura de archivos
//
// Read size bytes from the given file into the buffer buf, beginning offset
// bytes into the file. See read(2) for full details. Returns the number of
// bytes transferred, or 0 if offset was at or beyond the end of the file.
// Required for any sensible filesystem.
//
// Example: cat [file]
//
static int
fisopfs_read(const char *path,
             char *buffer,
             size_t size,
             off_t offset,
             struct fuse_file_info *fi)
{
	printf("[debug] fisopfs_read - path: %s, offset: %lu, size: %lu\n",
	       path,
	       offset,
	       size);

	if (offset < 0 || size < 0) {
		fprintf(stderr, "Error: datos invalidos\n");
		return -EINVAL;
	}

	fs_file_t *file = get_file(fs, path);
	if (!file) {
		printf("[debug] fisopfs_read - file %s not found\n", path);
		return -ENOENT;
	}

	char *content = file->content;
	size_t file_size = file->size;
	if (offset > file_size) {
		fprintf(stderr, "Error: offset mayor que el tamaño del archivo\n");
		return -EINVAL;
	}

	if (file_size < size - offset)
		size = file_size - offset;

	strncpy(buffer, content + offset, size);
	file->time_last_access = time(NULL);

	return size;
}

// ## Escritura de archivos
//
// As for read above, except that it can't return 0.
//
// Example: echo "hola" > [file]
//
static int
fisopfs_write(const char *path,
              const char *buffer,
              size_t size,
              off_t offset,
              struct fuse_file_info *fi)
{
	printf("[debug] fisopfs_write - path: %s\n", path);

	if (offset < 0 || size < 0 || offset + size > MAX_CONTENIDO) {
		fprintf(stderr, "Error: datos invalidos\n");
		return -EINVAL;
	}

	fs_file_t *file = get_file(fs, path);
	if (!file) {
		char dir_path[MAX_NAME];
		strcpy(dir_path, path);
		int status = fisopfs_create(basename(dir_path), 33024, fi);
		if (status < 0) {
			fprintf(stderr, "Error: no se pudo crear el archivo\n");
			return status;
		}
		file = get_file(fs, path);
	}

	if (file->size < offset) {
		fprintf(stderr, "Error: datos invalidos\n");
		return -EINVAL;
	}

	strncpy(file->content + offset, buffer, size);
	file->time_last_access = time(NULL);
	file->time_last_modification = time(NULL);
	file->size = strlen(file->content);
	file->content[file->size] = '\0';

	return (int) size;
}

// ## Acceder a las estadísticas de un archivo
//
// Return file attributes. The "stat" structure is described in detail in the
// stat(2) manual page. For the given pathname, this should fill in the elements
// of the "stat" structure. If a field is meaningless or semi-meaningless (e.g.,
// st_ino) then it should be set to 0 or given a "reasonable" value. This call
// is pretty much required for a usable filesystem.
//
// Example: ls -l , stat [file]
//
static int
fisopfs_getattr(const char *path, struct stat *st)
{
	printf("[debug] fisopfs_getattr - path: %s\n", path);
	int status = fs_getattr(fs, path, st);
	if (status < 0)
		fprintf(stderr,
		        "[debug] fisopfs_getattr - attributes not found\n");

	return status;
}

// ## Borrado de un archivo
//
// Remove (delete) the given file, symbolic link, hard link, or special
// node. Note that if you support hard links, unlink only deletes the
// data when the last hard link is removed. See unlink(2) for details.
//
// Example: rm [file]
//
static int
fisopfs_unlink(const char *path)
{
	printf("[debug] fisopfs_unlink - path: %s\n", path);
	return fs_unlink(fs, path);
}

// ## Borrado de directorios
//
// Remove the given directory. This should succeed only if the directory is
// empty (except for "." and ".."). See rmdir(2) for details.
//
// Example: rmdir [dir]
//
static int
fisopfs_rmdir(const char *path)
{
	printf("[debug] fisopfs_rmdir - path: %s\n", path);
	return fs_rmdir(fs, path);
}

// ## Cambio de tamaño de un archivo
//
// Change the size of the file. This function can be called multiple times
// for the same file during a single open(2). This function should not change
// the file offset. See truncate(2) for details.
//
// Example: truncate [file]
//
static int
fisopfs_truncate(const char *path, off_t size)
{
	printf("[debug] fisopfs_truncate - path: %s\n", path);

	if (size < 0 || size > MAX_CONTENIDO) {
		fprintf(stderr, "Error: tamaño invalido\n");
		return -EINVAL;
	}

	fs_file_t *file = get_file(fs, path);
	if (!file) {
		fprintf(stderr, "Error: archivo no encontrado\n");
		return -ENOENT;
	}

	file->size = size;
	file->time_last_modification = time(NULL);

	return EXIT_SUCCESS;
}


// ----------------------------------------------------------------------

// # Persistencia de datos

// ## Init
//
// Deserialize the filesystem.
//
// Initialize the filesystem. This function can often be left unimplemented, but
// it can be a handy way to perform one-time setup such as allocating variable-sized
// data structures or initializing a new filesystem. The fuse_conn_info structure
// gives information about what features are supported by FUSE, and can be used
// to request certain capabilities (see below for more information). The return
// value of this function is available to all file operations in the private_data
// field of fuse_context. It is also passed as a parameter to the destroy() method.
// (Note: see the warning under Other Options below, regarding relative pathnames.)
//
// Example: mount [dir]
//
void *
fisopfs_init(struct fuse_conn_info *conn)
{
	printf("[debug] Initialize Filesystem! Welcome.\n");
	if (save)
		printf("[debug] Persistency activated - File System will be "
		       "saved\n");

	fs = fs_init(path);
	if (!fs)
		fprintf(stderr, "Error al iniciar el file system.\n");

	return NULL;
}

// ## Destroy
//
// Serilize the filesystem.
//
// Called when the filesystem exits. The private_data comes from the return value of init.
//
// Example: umount [dir]
//
void
fisopfs_destroy(void *private_data)
{
	printf("[debug] Filesystem destroy\n");
	if (save)
		printf("[debug] Saving filesystem\n");

	fs_destroy(path, fs, save);
}

static struct fuse_operations operations = {
	.getattr = fisopfs_getattr,
	.readdir = fisopfs_readdir,
	.read = fisopfs_read,
	.mkdir = fisopfs_mkdir,
	.create = fisopfs_create,
	.utimens = fisopfs_utimens,
	.write = fisopfs_write,
	.truncate = fisopfs_truncate,
	.unlink = fisopfs_unlink,
	.rmdir = fisopfs_rmdir,

	.init = fisopfs_init,
	.destroy = fisopfs_destroy,
};

int
main(int argc, char *argv[])
{
	// Persistencia
	if (strcmp(argv[argc - 1], "-p") == 0) {
		save = 1;
		argc--;
	}

	return fuse_main(argc, argv, &operations, NULL);
}
