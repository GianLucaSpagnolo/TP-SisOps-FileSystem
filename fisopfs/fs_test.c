#include "testing.c"
#include "fs_lib.c"
#include <libgen.h>

void
prueba_split_path()
{
	char str[] = "/dir1";
	char *file = basename(str);
	test_nuevo_sub_grupo("Obtención de path con una parte");
	test_afirmar(strcmp(file, "dir1") == 0, "Se obtiene el directorio");
	char *dir = dirname(str);
	test_afirmar(strcmp(dir, "/") == 0, "Se obtiene el directorio padre");

	char str2[] = "/dir1/file.txt";
	char *file2 = basename(str2);
	test_nuevo_sub_grupo("Obtención de path con dos partes");
	test_afirmar(strcmp(file2, "file.txt") == 0, "Se obtiene el archivo");
	char *dir2 = basename(dirname(str2));
	test_afirmar(strcmp(dir2, "dir1") == 0, "Se obtiene el directorio padre");

	char str3[] = "/dir1/dir2/file";
	char *file3 = basename(str3);
	test_nuevo_sub_grupo("Obtención de path con varias partes");
	test_afirmar(strcmp(file3, "file") == 0, "Se obtiene el archivo");
	char *dir3 = basename(dirname(str3));
	test_afirmar(strcmp(dir3, "dir2") == 0, "Se obtiene el directorio padre");
}

void
prueba_crear_file_system()
{
	fs_t *fs = fs_build();
	test_afirmar(fs != NULL, "Se crea un file system");
	if (fs == NULL)
		return;
	test_afirmar(fs->d_size == 1, "El file system solo tiene un directorio");
	test_afirmar(!strcmp(fs->directories[0].path, ROOT),
	             "El directorio raiz se llama '/'");
	test_afirmar(fs->f_size == 0, "El file system no tiene archivos");
	test_afirmar(!strcmp(fs->directories[0].path, ROOT),
	             "El file system no tiene directorios");
	free(fs);
}

void
prueba_crear_un_directorio()
{
	fs_t *fs = fs_build();

	test_nuevo_sub_grupo("Se crea un directorio sobre root");

	char dir1[] = "/dir1";
	test_afirmar(fs_mkdir(fs, dir1, 1) == 0, "Se crea un directorio");
	test_afirmar(fs->d_size == 2, "El file system tiene la cantidad correspondiente de directorios");
	test_afirmar(!strcmp(fs->directories[1].path, "/dir1"),
	             "El directorio se llama 'dir1'");
	test_afirmar(!strcmp(fs->directories[1].d_parent->path, ROOT),
	             "El directorio 'dir1' es hijo de '/'");

	char dir2[] = "/dir2/dir3";
	test_nuevo_sub_grupo(
	        "No se puede crear un directorio sobre directorio inexistente");
	test_afirmar(fs_mkdir(fs, dir2, 1) == -1, "No se crea un directorio");
	test_afirmar(fs->d_size == 2, "El file system tiene la cantidad correspondiente de directorios");

	char dir3[] = "/dir1/dir2";
	test_nuevo_sub_grupo(
	        "Se puede crear un directorio sobre un directorio existente");
	test_afirmar(fs_mkdir(fs, dir3, 1) == 0, "Se crea un directorio");
	test_afirmar(fs->d_size - 1 == 2, "El file system tiene la cantidad correspondiente de directorios");

	test_afirmar(!strcmp(fs->directories[2].path, "/dir1/dir2"),
	             "El directorio se llama '/dir1/dir2'");
	test_afirmar(!strcmp(fs->directories[2].d_parent->path, "/dir1"),
	             "El directorio 'dir2' es hijo de 'dir1'");

	free(fs);
}

void
prueba_crear_o_modificar_un_archivo()
{
	fs_t *fs = fs_build();
	char file[] = "/archivo1.txt";

	test_nuevo_sub_grupo("Se crea un archivo sobre root");
	char file1[] = "/archivo1.txt";
	test_afirmar(fs_create(fs, file1, 1) == 0, "Se crea un archivo");
	test_afirmar(
	        fs->f_size == 1,
	        "El file system tiene la cantidad correspondiente de archivos");
	test_afirmar(!strcmp(fs->files[0].path, file1),
	             "El archivo se llama 'archivo1.txt'");
	test_afirmar(fs->files[0].size == 0, "El archivo tiene tamaño 0");
	test_afirmar(!strcmp(fs->files[0].entry->path, ROOT),
	             "El archivo 'archivo1.txt' esta en '/'");

	sleep(1);
	test_nuevo_sub_grupo("Se modifica un archivo existente");
	test_afirmar(fs_create(fs, file1, 1) == 0, "Se modifica un archivo");
	test_afirmar(
	        fs->f_size == 1,
	        "El file system tiene la cantidad correspondiente de archivos");
	test_afirmar(!strcmp(fs->files[0].path, file),
	             "El archivo se llama 'archivo1.txt'");
	test_afirmar(fs->files[0].size == 0, "El archivo tiene tamaño 0");
	test_afirmar(fs->files[0].time_last_modification >
	                     fs->files[0].time_creation,
	             "El archivo 'archivo1.txt' se modifico");

	test_nuevo_sub_grupo(
	        "No se crea un archivo sobre un directorio inexistente");
	char file2[] = "/dir1/archivo1.txt";
	test_afirmar(fs_create(fs, file2, 1) == -1, "No se crea un archivo");
	test_afirmar(
	        fs->f_size == 1,
	        "El file system tiene la cantidad correspondiente de archivos");

	test_nuevo_sub_grupo(
	        "Se crea un archivo sobre un directorio existente");
	char dir1[] = "/dir1";
	char file3[] = "/dir1/archivo1.txt";
	test_afirmar(fs_mkdir(fs, dir1, 1) == 0, "Se crea un directorio");
	test_afirmar(fs_create(fs, file3, 1) == 0, "Se crea un archivo");
	test_afirmar(
	        fs->f_size == 2,
	        "El file system tiene la cantidad correspondiente de archivos");
	test_afirmar(!strcmp(fs->files[1].path, file3),
	             "El archivo se llama 'archivo1.txt'");
	test_afirmar(fs->files[1].size == 0, "El archivo tiene tamaño 0");
	test_afirmar(!strcmp(fs->files[1].entry->path, dir1),
	             "El archivo 'archivo1.txt' esta en 'dir1'");

	free(fs);
}

void
prueba_comparacion_y_verificacion_de_atributos()
{
	fs_t *fs = fs_build();
	char path_file1[] = "/archivo1.txt";

	test_nuevo_sub_grupo("Se obtienen los atributos de un archivo");
	test_afirmar(fs_create(fs, path_file1, 1) == 0, "Se crea un archivo");
	fs_file_t *file1 = get_file(fs, path_file1);
	test_afirmar(file1 != NULL, "Se obtiene un archivo");
	test_afirmar(!strcmp(file1->path, path_file1),
	             "El archivo se llama 'archivo1.txt'");
	test_afirmar(file1->size == 0, "El archivo tiene tamaño 0");
	test_afirmar(!strcmp(file1->entry->path, ROOT),
	             "El archivo 'archivo1.txt' esta en '/'");
	test_afirmar(file1->time_creation > 0,
	             "El archivo tiene fecha de creación");
	test_afirmar(file1->time_last_access > 0,
	             "El archivo tiene fecha de acceso");
	test_afirmar(file1->time_last_modification > 0,
	             "El archivo tiene fecha de modificación");
	test_afirmar(!strcmp(file1->content, ""),
	             "El archivo tiene contenido vacio");
	test_afirmar(
	        fs->f_size == 1,
	        "El file system tiene la cantidad correspondiente de archivos");

	sleep(1);

	test_nuevo_sub_grupo("Se comparan los atributos de dos archivos");
	char path_file2[] = "/archivo2.txt";
	test_afirmar(fs_create(fs, path_file2, 1) == 0, "Se crea un archivo");
	fs_file_t *file2 = get_file(fs, path_file2);
	test_afirmar(file2 != NULL, "Se obtiene un archivo");
	test_afirmar(!strcmp(file2->path, path_file2),
	             "El archivo se llama 'archivo2.txt'");
	test_afirmar(file2->size == 0, "El archivo tiene tamaño 0");
	test_afirmar(!strcmp(file2->entry->path, ROOT),
	             "El archivo 'archivo2.txt' esta en '/'");
	test_afirmar(file2->time_creation > file1->time_creation,
	             "El archivo 2 tiene fecha de creación mas reciente al "
	             "archivo 1");
	test_afirmar(
	        file2->time_last_access > file1->time_last_access,
	        "El archivo 2 tiene fecha de acceso mas reciente al archivo 1");
	test_afirmar(file2->time_last_modification > file1->time_last_modification,
	             "El archivo 2 tiene fecha de modificación mas reciente al "
	             "archivo 1");
	test_afirmar(!strcmp(file2->content, ""),
	             "El archivo tiene contenido vacio");
	test_afirmar(
	        fs->f_size == 2,
	        "El file system tiene la cantidad correspondiente de archivos");

	sleep(1);

	test_afirmar(fs_create(fs, path_file1, 1) == 0,
	             "Se interactua con el archivo 1");
	test_afirmar(file1->time_last_modification > file2->time_last_modification,
	             "El archivo 1 tiene fecha de modificación mas reciente al "
	             "archivo 2");

	free(fs);
}

void
prueba_eliminacion_de_archivos_y_directorios()
{
	fs_t *fs = fs_build();

	char dir1[] = "/dir1";
	char dir2[] = "/dir1/dir2";
	char dir5[] = "/dir5";
	char dir6[] = "/dir6";
	char file2[] = "/archivo2.txt";
	char file3[] = "/dir1/archivo3.txt";

	fs_mkdir(fs, dir1, 1);
	fs_mkdir(fs, dir2, 1);
	fs_mkdir(fs, dir5, 1);
	fs_mkdir(fs, dir6, 1);
	fs_create(fs, file2, 1);
	fs_create(fs, file3, 1);

	test_nuevo_sub_grupo("Se eliminan directorios");
	test_afirmar(fs->d_size == 5 && fs->f_size == 2,
	             "El file system tiene la cantidad correspondiente de "
	             "directorios y archivos");
	test_afirmar(fs_rmdir(fs, dir5) == 0, "Se elimina un directorio");
	test_afirmar(fs_rmdir(fs, dir2) == 0,
	             "Se elimina un directorio dentro de un directorio");
	test_afirmar(fs->d_size == 3 && fs->f_size == 2,
	             "El file system actualizo la cantidad de directorios");

	test_nuevo_sub_grupo("Se eliminan archivos");
	test_afirmar(fs_unlink(fs, file2) == 0, "Se elimina un archivo");
	test_afirmar(fs_unlink(fs, file3) == 0,
	             "Se elimina un archivo dentro de un directorio");
	test_afirmar(fs->d_size == 3 && fs->f_size == 0, "El file system actualizo la cantidad de directorios y archivos");

	free(fs);
}

void
prueba_de_eliminacion_de_subdirectorios_y_archivos()
{
	fs_t *fs = fs_build();

	char dir1[] = "/dir1";
	char dir2[] = "/dir1/dir2";
	char file1[] = "/dir1/archivo1.txt";

	fs_mkdir(fs, dir1, 1);
	fs_mkdir(fs, dir2, 1);
	fs_create(fs, file1, 1);

	test_nuevo_sub_grupo(
	        "Se eliminan archivos y subdirectorios de un directorio");
	test_afirmar(fs->d_size == 3 && fs->f_size == 1,
	             "El file system tiene la cantidad correspondiente de "
	             "directorios y archivos");
	test_afirmar(fs_unlink(fs, file1) == 0,
	             "Se elimina un archivo de un directorio");
	test_afirmar(fs_rmdir(fs, dir2) == 0,
	             "Se elimina un subdirectorio de un directorio");
	test_afirmar(fs_rmdir(fs, dir1) == 0,
	             "Se elimina un directorio con subdirectorios y archivos");
	test_afirmar(fs->d_size == 1 && fs->f_size == 0, "El file system ha borrado correctamente los directorios y archivos");
}

void
prueba_de_no_eliminacion_de_directorios()
{
	fs_t *fs = fs_build();

	char dir1[] = "/dir1";
	char dir2[] = "/dir1/dir2";
	char dir3[] = "/dir3";
	char file1[] = "/dir3/archivo1.txt";

	fs_mkdir(fs, dir1, 1);
	fs_mkdir(fs, dir2, 1);
	fs_mkdir(fs, dir3, 1);
	fs_create(fs, file1, 1);

	test_nuevo_sub_grupo(
	        "No se eliminan directorios con subdirectorios y archivos");
	test_afirmar(fs->d_size == 4 && fs->f_size == 1,
	             "El file system tiene la cantidad correspondiente de "
	             "directorios y archivos");
	test_afirmar(fs_rmdir(fs, dir1) == -39,
	             "No se elimina un directorio con subdirectorios");
	test_afirmar(fs_rmdir(fs, dir3) == -39,
	             "No se elimina un directorio con archivos");
	test_afirmar(fs->d_size == 4 && fs->f_size == 1,
	             "No ha cambiado la cantidad de directorios y archivos");

	test_nuevo_sub_grupo("Se eliminan directorios luego de eliminar "
	                     "subdirectorios y archivos");
	test_afirmar(fs_unlink(fs, file1) == 0,
	             "Se elimina un archivo de un directorio");
	test_afirmar(fs_rmdir(fs, dir3) == 0,
	             "Se elimina un directorio con archivos");
	test_afirmar(fs_rmdir(fs, dir2) == 0,
	             "Se elimina subdirectorio de un directorio");
	test_afirmar(fs_rmdir(fs, dir1) == 0,
	             "Se elimina un directorio con subdirectorios");
	test_afirmar(fs->d_size == 1 && fs->f_size == 0, "El file system ha borrado correctamente los directorios y archivos");

	free(fs);
}

void
prueba_persistencia()
{
	fs_t *fs_w = fs_build();

	fs_file_t file_w;
	strcpy(file_w.path, "archivo1.txt");
	file_w.size = 8;
	strcpy(file_w.content, "archivo");
	file_w.entry = &fs_w->directories[0];

	fs_w->files[0] = file_w;
	fs_w->f_size = 1;

	fs_destroy("./fs.dat", fs_w, 1);

	fs_t *fs_r = fs_init("./fs.dat");

	test_afirmar(fs_r != NULL, "Se recuperan los datos de un file system");
	test_afirmar(fs_r->d_size == 1, "Se recupera la cantidad de directorios");
	test_afirmar(strcmp(fs_r->directories[0].path, ROOT) == 0,
	             "Se recupera el nombre del directorio raiz");
	test_afirmar(fs_r->f_size == 1, "Se recupera la cantidad de archivos");
	test_afirmar(strcmp(fs_r->files[0].path, "archivo1.txt") == 0,
	             "Se recupera el nombre del archivo");
	test_afirmar(fs_r->files[0].size == 8,
	             "Se recupera el tamaño del archivo");
	test_afirmar(strcmp(fs_r->files[0].content, "archivo") == 0,
	             "Se recupera el contenido del archivo");

	free(fs_r);
}

int
main()
{
	test_nuevo_grupo("División de un path en partes");
	prueba_split_path();
	test_titulo("Operaciones de un sistema de archivos");
	test_nuevo_grupo("Creación de un sistema de archivos");
	prueba_crear_file_system();
	test_nuevo_grupo("Creación de directorios");
	prueba_crear_un_directorio();
	test_nuevo_grupo("Creación de archivos");
	prueba_crear_o_modificar_un_archivo();
	test_nuevo_grupo("Obtención de atributos de archivos");
	prueba_comparacion_y_verificacion_de_atributos();
	test_nuevo_grupo("Eliminación de archivos y directorios");
	prueba_eliminacion_de_archivos_y_directorios();
	prueba_de_eliminacion_de_subdirectorios_y_archivos();
	prueba_de_no_eliminacion_de_directorios();
	test_titulo("Persistencia de datos");
	test_nuevo_grupo("Guardar y recuperar file system en un archivo");
	prueba_persistencia();
	test_titulo("Funciones auxiliares");
	test_mostrar_reporte();
	return 0;
}