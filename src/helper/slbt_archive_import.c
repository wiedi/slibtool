/*******************************************************************/
/*  slibtool: a skinny libtool implementation, written in C        */
/*  Copyright (C) 2016  Z. Gilboa                                  */
/*  Released under the Standard MIT License; see COPYING.SLIBTOOL. */
/*******************************************************************/

#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/wait.h>

#include <slibtool/slibtool.h>
#include "slibtool_spawn_impl.h"

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

inline int slbt_archive_import(
	const struct slbt_driver_ctx *	dctx,
	struct slbt_exec_ctx *		ectx,
	char *				dstarchive,
	char *				srcarchive)
{
	pid_t	pid;
	int	ret;
	int	code;
	int	fd[2];
	FILE *	fout;
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

	if ((fout = fdopen(fd[1],"a"))) {
		ret = (fprintf(
				fout,
				"OPEN %s\n"
				"ADDLIB %s\n"
				"SAVE\n"
				"END\n",
				dstarchive,
				srcarchive) < 0)
			? -1 : 0;

		fclose(fout);
		close(fd[0]);
	} else {
		ret = -1;
		close(fd[0]);
		close(fd[1]);
	}

	code = waitpid(
		pid,
		&ectx->exitcode,
		0);

	return ret || (code != pid) || ectx->exitcode
		? -1 : 0;
}
