/*******************************************************************/
/*  slibtool: a skinny libtool implementation, written in C        */
/*  Copyright (C) 2016  Z. Gilboa                                  */
/*  Released under the Standard MIT License; see COPYING.SLIBTOOL. */
/*******************************************************************/

#include <stdio.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/wait.h>

#include <slibtool/slibtool.h>
#include "slibtool_spawn_impl.h"

static char * slbt_mri_argument(
	char *	arg,
	char *	buf)
{
	int	i;
	char *	lnk;
	char 	mricwd[PATH_MAX];
	char 	target[PATH_MAX];

	if ((!(strchr(arg,'+'))) && (!(strchr(arg,'-'))))
		return arg;

	if (!(getcwd(mricwd,sizeof(mricwd))))
		return 0;

	if ((size_t)snprintf(target,sizeof(target),"%s/%s",
			mricwd,arg) >= sizeof(target))
		return 0;

	for (i=0,lnk=0; i<1024 && !lnk; i++) {
		if (!(tmpnam(buf)))
			return 0;

		if (!(symlink(target,buf)))
			lnk = buf;
	}

	return lnk;
}

static int slbt_archive_import_child(
	char *	program,
	int	fd[2])
{
	char *	argv[3];

	argv[0] = program;
	argv[1] = "-M";
	argv[2] = 0;

	close(fd[1]);
	close(0);

	if (dup(fd[0]) == 0)
		execvp(program,argv);

	exit(EXIT_FAILURE);
	return -1;
}

int slbt_archive_import(
	const struct slbt_driver_ctx *	dctx,
	struct slbt_exec_ctx *		ectx,
	char *				dstarchive,
	char *				srcarchive)
{
	int	ret;
	pid_t	pid;
	pid_t	rpid;
	int	fd[2];
	FILE *	fout;
	char *	dst;
	char *	src;
	char	mridst [L_tmpnam];
	char	mrisrc [L_tmpnam];
	char	program[PATH_MAX];

	if ((size_t)snprintf(program,sizeof(program),"%s",
			dctx->cctx->host.ar) >= sizeof(program))
		return -1;

	if (pipe(fd))
		return -1;

	if ((pid = fork()) < 0) {
		close(fd[0]);
		close(fd[1]);
		return -1;
	}

	if (pid == 0)
		return slbt_archive_import_child(
			program,
			fd);

	ectx->pid = pid;

	dst = slbt_mri_argument(dstarchive,mridst);
	src = slbt_mri_argument(srcarchive,mrisrc);

	if ((fout = fdopen(fd[1],"a"))) {
		ret = (fprintf(
				fout,
				"OPEN %s\n"
				"ADDLIB %s\n"
				"SAVE\n"
				"END\n",
				dst,
				src) < 0)
			? -1 : 0;

		fclose(fout);
		close(fd[0]);
	} else {
		ret = -1;
		close(fd[0]);
		close(fd[1]);
	}

	rpid = waitpid(
		pid,
		&ectx->exitcode,
		0);

	if (dst == mridst)
		unlink(dst);

	if (src == mrisrc)
		unlink(src);

	return ret || (rpid != pid) || ectx->exitcode
		? -1 : 0;
}
