/*******************************************************************/
/*  slibtool: a skinny libtool implementation, written in C        */
/*  Copyright (C) 2016  Z. Gilboa                                  */
/*  Released under the Standard MIT License; see COPYING.SLIBTOOL. */
/*******************************************************************/

#include <string.h>
#include <stdbool.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

#include <slibtool/slibtool.h>
#include "slibtool_spawn_impl.h"


int slbt_exec_link(
	const struct slbt_driver_ctx *	dctx,
	struct slbt_exec_ctx *		ectx)
{
	int			ret;
	int			fdlibs;
	char *			dot;
	FILE *			fout;
	struct slbt_exec_ctx *	actx;

	/* context */
	if (ectx)
		actx = 0;
	else if ((ret = slbt_get_exec_ctx(dctx,&ectx)))
		return ret;
	else
		actx = ectx;

	/* .libs directory */
	if (dctx->cctx->drvflags & SLBT_DRIVER_SHARED) {
		if ((fdlibs = open(ectx->ldirname,O_DIRECTORY)) >= 0)
			close(fdlibs);
		else if ((errno != ENOENT) || mkdir(ectx->ldirname,0777)) {
			slbt_free_exec_ctx(actx);
			return -1;
		}
	}

	/* no wrapper? */
	if (!(dot = strrchr(dctx->cctx->output,'.')) || strcmp(dot,".la")) {
		slbt_free_exec_ctx(actx);
		return 0;
	}

	/* hey, yo, let's rap it up */
	if (!(fout = fopen(ectx->ltobjname,"w"))) {
		slbt_free_exec_ctx(actx);
		return -1;
	}

	ret = fprintf(fout,
		"# slibtool (pre-alphe) generated file\n\n");

	/* all done */
	fclose(fout);
	slbt_free_exec_ctx(actx);

	return (ret > 0) ? 0 : -1;
}
