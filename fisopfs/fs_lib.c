#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <string.h>
#include <stdlib.h>
#include <libgen.h>
#include <time.h>
#include <errno.h>

#define F_WRITE "w"
#define F_READ "r"

#define ROOT "/"

#define MAX_DIRECTORIOS 20
#define MAX_ARCHIVOS 20
#define MAX_CONTENIDO 100
#define MAX_NAME 50

typedef struct fs_d_entry {
	char path[MAX_NAME];
	struct fs_d_entry *d_parent;
	// Stats:
	mode_t mode;
	uid_t uid;
	gid_t gid;
	time_t time_last_access;
	time_t time_last_modification;
	time_t time_creation;
	size_t size;
} fs_d_entry_t;

typedef struct fs_file {
	char path[MAX_NAME];
	fs_d_entry_t *entry;
	char content[MAX_CONTENIDO];
	// Stats:
	mode_t mode;
	uid_t uid;
	gid_t gid;
	time_t time_last_access;
	time_t time_last_modification;
	time_t time_creation;
	size_t size;
} fs_file_t;

typedef struct fs {
	fs_d_entry_t directories[MAX_DIRECTORIOS];
	size_t d_size;
	fs_file_t files[MAX_ARCHIVOS];
	size_t f_size;
} fs_t;


static int
get_dir_index(fs_t *fs, const char *path)
{
	if (strcmp(path, ROOT) == 0 || strlen(path) == 0)
		return 0;

	for (size_t i = 0; i < fs->d_size; i++) {
		if (strcmp(fs->directories[i].path, path) == 0)
			return i;
	}

	return -1;
}

// ## get_dir
//
// Verifica si un directorio con el nombre especificado existe.
//
// Devuelve un puntero al directorio si existe, NULL en caso contrario.
//
static fs_d_entry_t *
get_dir(fs_t *fs, const char *path)
{
	int index = get_dir_index(fs, path);
	if (index != -1)
		return &fs->directories[index];

	return NULL;
}


static int
get_file_index(fs_t *fs, const char *path)
{
	if (fs == NULL || path == NULL)
		return -1;

	for (size_t i = 0; i < fs->f_size; i++) {
		if (strcmp(fs->files[i].path, path) == 0)
			return i;
	}

	return -1;
}

// ## get_file
//
// Verifica si un archivo con el nombre especificado existe.
//
// Devuelve un puntero al archivo si existe, NULL en caso contrario.
//
static fs_file_t *
get_file(fs_t *fs, const char *path)
{
	int index = get_file_index(fs, path);
	if (index != -1)
		return &fs->files[index];

	return NULL;
}

// ## fs_create_dir
//
// Crea un directorio con el nombre y directorio especificados.
//
// Devuelve un puntero al directorio creado, NULL en caso de error.
//
static fs_d_entry_t *
fs_create_dir(fs_t *fs, const char *name, fs_d_entry_t *parent, mode_t mode)
{
	if (fs == NULL || name == NULL)
		return NULL;

	strcpy(fs->directories[fs->d_size].path, name);

	fs->directories[fs->d_size].d_parent = parent;

	fs->directories[fs->d_size].size = 0;
	fs->directories[fs->d_size].uid = 1717;
	fs->directories[fs->d_size].gid = getgid();
	fs->directories[fs->d_size].mode = mode;
	fs->directories[fs->d_size].time_last_access = time(NULL);
	fs->directories[fs->d_size].time_last_modification = time(NULL);
	fs->directories[fs->d_size].time_creation = time(NULL);

	fs->d_size++;
	return &fs->directories[fs->d_size - 1];
}

// ## Creación de directorios
//
static int
fs_mkdir(fs_t *fs, const char *path, mode_t mode)
{
	if (strlen(path) < 2) {
		fprintf(stderr, "Nombre de directorio inválido.\n");
		return -1;
	}

	if (strlen(path) - 1 == MAX_NAME) {
		fprintf(stderr, "Nombre de directorio demasiado largo.\n");
		return -ENAMETOOLONG;
	}

	if (fs->d_size == MAX_DIRECTORIOS) {
		fprintf(stderr, "No se pueden crear más directorios.\n");
		return -1;
	}

	char temp_path[MAX_NAME];
	strcpy(temp_path, path);
	char parent_path[MAX_NAME];
	strcpy(parent_path, dirname(temp_path));
	fs_d_entry_t *dir = get_dir(fs, parent_path);
	if (!dir) {
		fprintf(stderr, "Error al crear el directorio.\n");
		return -1;
	}
	fs_d_entry_t *new_dir = fs_create_dir(fs, path, dir, mode);
	if (!new_dir) {
		fprintf(stderr, "Error al crear el directorio.\n");
		return -1;
	}

	return EXIT_SUCCESS;
}

static int
dir_set_ts(fs_d_entry_t *dir, const struct timespec ts[2])
{
	dir->time_last_access = ts[0].tv_sec;
	dir->time_last_modification = ts[1].tv_sec;
	return EXIT_SUCCESS;
}

static int
file_set_ts(fs_file_t *file, const struct timespec ts[2])
{
	file->time_last_access = ts[0].tv_sec;
	file->time_last_modification = ts[1].tv_sec;
	return EXIT_SUCCESS;
}

// ## Cambio de tiempo de acceso y modificación de un archivo o directorio
//
static int
fs_utimens(fs_t *fs, const char *path, const struct timespec ts[2])
{
	fs_d_entry_t *dir = get_dir(fs, path);
	if (dir)
		return dir_set_ts(dir, ts);

	fs_file_t *file = get_file(fs, path);
	if (file)
		return file_set_ts(file, ts);

	fprintf(stderr,
	        "Error al actualizar los tiempos de acceso y modificación.\n");
	return -ENOENT;
}

// ## Obtener atributos de un archivo
//
static int
fs_getattr(fs_t *fs, const char *path, struct stat *st)
{
	fs_d_entry_t *dir = get_dir(fs, path);
	if (dir) {
		st->st_mode = __S_IFDIR | 0755;
		st->st_nlink = 2;
		st->st_uid = dir->uid;
		st->st_gid = dir->gid;
		st->st_size = dir->size;
		st->st_atime = dir->time_last_access;
		st->st_mtime = dir->time_last_modification;
		st->st_ctime = dir->time_creation;
		st->st_dev = 0;
		st->st_ino = get_dir_index(fs, path);
		return EXIT_SUCCESS;
	}

	fs_file_t *file = get_file(fs, path);
	if (file) {
		st->st_mode = __S_IFREG | 0644;
		st->st_nlink = 1;
		st->st_uid = file->uid;
		st->st_gid = file->gid;
		st->st_size = file->size;
		st->st_atime = file->time_last_access;
		st->st_mtime = file->time_last_modification;
		st->st_ctime = file->time_creation;
		st->st_dev = 0;
		st->st_ino = get_file_index(fs, path);
		return EXIT_SUCCESS;
	}

	return -ENOENT;
}

// ## create_file
//
// Crea un archivo con el nombre y directorio especificados.
//
// Devuelve 0 en caso de éxito, -1 en caso de error.
//
static int
create_file(fs_t *fs, const char *path, mode_t mode)
{
	if (fs == NULL || path == NULL)
		return -1;

	char temp_path[MAX_NAME];
	strcpy(temp_path, path);
	char parent_path[MAX_NAME];
	strcpy(parent_path, dirname(temp_path));
	fs_d_entry_t *dir = get_dir(fs, parent_path);
	if (!dir) {
		fprintf(stderr, "Error al crear el archivo.\n");
		return -1;
	}

	fs_file_t file;

	strcpy(file.path, path);
	strcpy(file.content, "");

	file.entry = dir;

	file.mode = mode;
	file.uid = 1818;
	file.gid = getgid();
	file.size = 0;
	file.time_last_access = time(NULL);
	file.time_last_modification = time(NULL);
	file.time_creation = time(NULL);

	fs->files[fs->f_size] = file;
	fs->f_size++;

	return 0;
}

// ## touch_file
//
// Actualiza la fecha de modificación de un archivo.
//
// Devuelve 0 si pudo actualizar la fecha de modificación, -1 en caso contrario.
//
static int
touch_file(fs_t *fs, fs_file_t *file)
{
	if (fs == NULL || file == NULL)
		return -1;

	file->time_last_modification = time(NULL);
	return 0;
}

// ## Creación de archivos
//
static int
fs_create(fs_t *fs, const char *path, mode_t mode)
{
	if (strlen(path) < 2) {
		fprintf(stderr, "Nombre de archivo inválido.\n");
		return -1;
	}

	if (strlen(path) - 1 == MAX_NAME) {
		fprintf(stderr, "Nombre de archivo demasiado largo.\n");
		return -ENAMETOOLONG;
	}

	if (fs->f_size == MAX_ARCHIVOS) {
		fprintf(stderr, "No se pueden crear más archivos.\n");
		return -1;
	}

	fs_file_t *file = get_file(fs, path);

	if (!file) {
		int status = create_file(fs, path, mode);
		if (status == -1)
			return -1;
	} else {
		int status = touch_file(fs, file);
		if (status == -1)
			return -1;
	}

	return EXIT_SUCCESS;
}


static int
remove_file(fs_t *fs, const char *name)
{
	int index = get_file_index(fs, name);
	if (index == -1) {
		fprintf(stderr, "Error al eliminar el archivo.\n");
		return -ENOENT;
	}

	for (size_t i = index; i < fs->f_size - 1; i++)
		fs->files[i] = fs->files[i + 1];

	fs->f_size--;
	return 0;
}

static int
remove_dir(fs_t *fs, const char *name)
{
	int index = get_dir_index(fs, name);
	if (index == -1) {
		fprintf(stderr, "Error al eliminar el directorio.\n");
		return -ENOENT;
	}

	for (size_t i = index; i < fs->d_size - 1; i++)
		fs->directories[i] = fs->directories[i + 1];

	fs->d_size--;
	return 0;
}

// ## Eliminación de archivos
//
static int
fs_unlink(fs_t *fs, const char *path)
{
	fs_file_t *file = get_file(fs, path);
	if (file)
		return remove_file(fs, path);

	fprintf(stderr, "Error al eliminar el archivo.\n");
	return -ENOENT;
}

static int
amount_subdirs_and_files(fs_t *fs, fs_d_entry_t *dir)
{
	int amount = 0;

	for (size_t i = 0; i < fs->f_size; i++) {
		if (fs->files[i].entry == dir)
			amount++;
	}

	for (size_t i = 0; i < fs->d_size; i++) {
		if (fs->directories[i].d_parent &&
		    fs->directories[i].d_parent == dir)
			amount++;
	}

	return amount;
}

// Eliminacion de un directorio
//
static int
fs_rmdir(fs_t *fs, const char *path)
{
	fs_d_entry_t *dir = get_dir(fs, path);
	if (!dir) {
		fprintf(stderr, "Error al eliminar el directorio.\n");
		return -ENOENT;
	}

	if (amount_subdirs_and_files(fs, dir) > 0) {
		fprintf(stderr, "Error al eliminar el directorio. No se encuentra vacio.\n");
		return -ENOTEMPTY;
	}

	return remove_dir(fs, path);
}


// ## fs_build
//
// Construye un sistema de archivos vacío.
//
// Devuelve un puntero a una estructura fs_t con un directorio raíz.
// Si no se pudo crear la estructura, devuelve NULL.
//
// Contiene un solo directorio llamado "/". (ROOT)
// No contiene archivos.
//
static fs_t *
fs_build()
{
	fs_t *fs = calloc(1, sizeof(fs_t));
	if (!fs) {
		fprintf(stderr, "Error al crear el file system.\n");
		return NULL;
	}

	strcpy(fs->directories[0].path, ROOT);
	fs->directories[0].d_parent = NULL;
	fs->d_size = 1;
	fs->f_size = 0;

	fs->directories[0].uid = 1717;
	fs->directories[0].gid = getgid();
	fs->directories[0].mode = __S_IFDIR | 0755;
	fs->directories[0].time_creation = time(NULL);
	fs->directories[0].time_last_access = time(NULL);
	fs->directories[0].time_last_modification = time(NULL);
	fs->directories[0].size = 0;

	return fs;
}

// ## Guardar datos en un archivo
//
// Recibe el path de un archivo y una estructura fs_t con los datos a guardar.
// Devuelve 0 si pudo guardar los datos correctamente, -1 en caso contrario.
//
static void
fs_destroy(const char *path, fs_t *fs, int persist)
{
	FILE *fd = fopen(path, "w");

	if (fd == NULL) {
		fprintf(stderr, "Error al persistir el file system.\n");
		free(fs);
		fclose(fd);
		return;
	}

	if (persist == 0) {
		free(fs);
		fclose(fd);
		return;
	}

	int w1 = fwrite(&fs->d_size, sizeof(size_t), 1, fd);
	int w2 = fwrite(fs->directories, sizeof(fs_d_entry_t), fs->d_size, fd);
	int w3 = fwrite(&fs->f_size, sizeof(size_t), 1, fd);
	int w4 = fwrite(fs->files, sizeof(fs_file_t), fs->f_size, fd);

	if (w1 == -1 || w2 == -1 || w3 == -1 || w4 == -1) {
		fprintf(stderr, "Error al persistir el file system.\n");
		free(fs);
		fclose(fd);
		return;
	}

	fclose(fd);
	free(fs);
}

static int
verify_empty_file(FILE *file)
{
	long savedOffset = ftell(file);
	fseek(file, 0, SEEK_END);

	if (ftell(file) == 0) {
		return 1;
	}

	fseek(file, savedOffset, SEEK_SET);
	return 0;
}

// ## Recuperar datos de un archivo
//
// Recibe el path de un archivo serializado y devuelve un puntero a una estructura
// fs_t con los datos recuperados. Si el archivo no existe, devuelve NULL.
//
static fs_t *
fs_init(const char *path)
{
	FILE *fd = fopen(path, "r");

	if (fd == NULL) {
		printf("[debug] Initializing new File System (fs.fisopfs)\n");
		return fs_build();
	}

	if (verify_empty_file(fd) == 1) {
		fs_t *fs = fs_build();
		fclose(fd);
		return fs;
	}

	fs_t *fs = calloc(1, sizeof(fs_t));
	if (!fs) {
		fprintf(stderr, "Error al leer el archivo de persistencia del file system.\n");
		fclose(fd);
		return NULL;
	}

	int r1 = fread(&fs->d_size, sizeof(size_t), 1, fd);
	int r2 = fread(fs->directories, sizeof(fs_d_entry_t), fs->d_size, fd);
	int r3 = fread(&fs->f_size, sizeof(size_t), 1, fd);
	int r4 = fread(fs->files, sizeof(fs_file_t), fs->f_size, fd);

	if (r1 == -1 || r2 == -1 || r3 == -1 || r4 == -1) {
		fprintf(stderr, "Error al leer el archivo de persistencia del file system.\n");
		free(fs);
		fclose(fd);
		return NULL;
	}

	fclose(fd);
	return fs;
}
