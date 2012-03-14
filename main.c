/*
 * pfind - a simple search for files in a directory hierarchy
 */

#include <stddef.h>		// NULL
#include <stdlib.h>		// malloc()
#include <stdio.h>		// printf()
#include <sys/types.h>
#include <dirent.h>		// readdir()
#include <getopt.h>		// getopt_long()
#include <sys/stat.h>	// lstat()
#include <string.h>		// strlen()
#include <fnmatch.h>	// fnmatch()
#include <errno.h>		// Environmental variable for readdir()
#include <libgen.h>		// libgen.h()
#include <unistd.h>		// access()

static int convertBitPat( char *type );
void searchdir( char *dn, char *findme, int bitpat);
int checkFile( char *path, char *filename, char *findme, int bitpat );
char *buildPath( char *dn, char *filename, int dnlen);
void *emalloc( size_t size );

static char *program_name;

int main( int ac, char **av )
{
	char *dn = NULL;
	char *pattern = NULL;
	char *type = NULL;

	int opt;
	int len;	// Length of directory name

	program_name = basename(av[0]);

	while (1)
	{
		int opt_index = 0;

		static struct option long_opts[] =
		{
			{"name", 1, NULL, 'n'},
			{"type", 1, NULL, 't'},
			{0, 0, 0, 0}
		};

		opt = getopt_long_only(ac, av, "-n:t:", long_opts, &opt_index);

		if (opt == EOF)
			break;

		switch(opt)
		{
		case 1:
			dn = optarg;
			break;
		case 'n':
			if (pattern != NULL) {
				fprintf(stderr, "%s: Syntax error\n", program_name);
				exit(1);
			}
			pattern = optarg;
			break;
		case 't':
			if (type != NULL) {
				fprintf(stderr, "%s: Syntax error\n", program_name);
				exit(1);
			}
			type = optarg;
			break;
		}
	}

	if (dn == NULL) {
		fprintf(stderr, "%s: Please specify a start directory\n",
				program_name);
		exit(1);

	}

	if (pattern == NULL) {
		pattern = "*";
	}

	len = strlen(dn);
	if (dn[len - 1] == '/')
		dn[len - 1] = '\0';

	int bitpat = convertBitPat(type);
	checkFile(dn, dn, pattern, bitpat);

	exit(0);
}

/*
 * This function takes a pointer to a character representing a file
 * type and returns the corresponding symbolic name, which is a bit
 * pattern defined as an integer.
 */
static int convertBitPat( char *type )
{
	int bitpat = 0;
	if (type == NULL)
		return bitpat;
	switch(*type)
	{
	case 'f':	 bitpat = __S_IFREG;	break;
	case 'd':	 bitpat = __S_IFDIR;	break;
	case 'b':	 bitpat = __S_IFBLK;	break;
	case 'c':	 bitpat = __S_IFCHR;	break;
	case 'p':	 bitpat = __S_IFIFO;	break;
	case 'l':	 bitpat = __S_IFLNK;	break;
	case 's':	 bitpat = __S_IFSOCK;	break;
	default:
		fprintf(stderr, "%s: Invalid type '%c'\n", program_name, *type);
		exit(1);
	}
	return bitpat;
}

/*
 * This function stores directory entries
 */
void searchdir( char *dn, char *findme, int bitpat)
{
	DIR *dirstr;			// Directory stream
	int numents = 0;		// Number of directory entries
	char **fnames;			// Pointer to a pointer array of filenames
	int fnpos = 0;			// Position in filename array
	struct dirent *dentry;	// Directory entry

	if ((dirstr = opendir(dn)) == NULL)
		perror(dn);

	/* Sum total number of entries */
	errno = 0;
	while (readdir(dirstr)) {
		numents++;
	}
	rewinddir(dirstr);
	if (errno != 0) {
		perror(dn);
	}

	/* Allocate memory for pointer array */
	fnames = emalloc(numents * sizeof(char *));

	/* Store directory name */
	size_t len;
	errno = 0;
	while ((dentry = readdir(dirstr))) {
		len = strlen(dentry->d_name);
		fnames[fnpos] = emalloc(len+1);
		strcpy(fnames[fnpos], dentry->d_name);
		fnpos++;
	}
	if (errno != 0) {
		perror(dn);
	}

	if (closedir(dirstr))
		perror(dn);

	/* Check each directory entry */
	int dnlen = strlen(dn);
	int i;
	for (i = 0; i < numents; i++) {
		char *fn = fnames[i];

		if (strcmp(fn, ".") == 0 || strcmp(fn, "..") == 0)
			continue;

		char *path = buildPath(dn, fn, dnlen);
		int chk = checkFile(path, fn, findme, bitpat);
		if ( chk == 1) {
			free(path);
			continue;
		}

		free(path);
	}

	/* Free pointer array memory */
	int j;
	for (j = 0; j < numents; j++)
		free(fnames[j]);
	free(fnames);
}

/*
 * This function concatenates a directory name and a filename into a
 * path, which it returns.
 */
char *buildPath( char *dn, char *filename, int dnlen)
{
	int fnlen = strlen(filename);
	int pnlen = dnlen + 2 + fnlen;
	char *path = emalloc(pnlen);

	snprintf(path, pnlen, "%s/%s", dn, filename);

	return path;
}

/*
 * This function checks if a file is a directory. If so, it calls the
 * recursive search function. If not, it checks the type of the file
 * against the program type argument.
 */
int checkFile( char *path, char *filename, char *findme, int bitpat )
{
	struct stat statbuf;
	int match;

	if ( lstat(path, &statbuf) == -1 ) {
		perror(path);
		return -1;
	}

	while (bitpat == 0 || ((statbuf.st_mode & __S_IFMT) == (bitpat)))
	{
		match = fnmatch(findme, filename, 0);
		if (match == FNM_NOMATCH) {
			break;
		}
		else if (match == 0) {
			printf("%s\n", path);
			break;
		}
		else {
			fprintf(stderr, "%s: fnmatch has encountered an error\n",
					program_name);
			return -1;
		}
	}

	if ( S_ISDIR(statbuf.st_mode) ) {
		if ( access(path, R_OK|X_OK) == -1 ) {
			fprintf(stderr, "%s: %s: Permission denied\n",
					program_name, path);	//TODO (PM) perror()
			return -1;
		}
		else {
			searchdir(path, findme, bitpat);
			return 0;
		}
	}

	return 0;
}

/*
 * This function terminates the program if malloc() fails.
 */
void *emalloc( size_t size )
{
	void *p;

	p = malloc(size);
	if (!p) {
		perror("emalloc");
		exit(EXIT_FAILURE);
	}

	return p;
}
