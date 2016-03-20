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


/*******************************************************************/
/*                                                                 */
/* -o <ltlib>  switches              input   result                */
/* ----------  --------------------- -----   ------                */
/* libfoo.a    -static               bar.lo  libfoo.a              */
/*                                                                 */
/* ar cru libfoo.a bar.lo                                          */
/* ranlib libfoo.a                                                 */
/*                                                                 */
/*******************************************************************/

static bool slbt_adjust_input_argument(char * arg, bool fpic)
{
	char *	dot;

	if (*arg == '-')
		return false;

	if (!(dot = strrchr(arg,'.')))
		return false;

	if (strcmp(dot,".lo"))
		return false;

	if (fpic) {
		/* to do */
		return false;
	} else {
		dot[1] = 'o';
		dot[2] = '\0';
		return true;
	}
}

static int slbt_exec_link_static_archive(
	const struct slbt_driver_ctx *	dctx,
	struct slbt_exec_ctx *		ectx,
	const char *			arfilename)
{
	char ** 	aarg;
	char ** 	parg;
	char *		ranlib[3];
	char		program[2048];
	char		output[2048];

	/* placeholders */
	slbt_reset_placeholders(ectx);

	/* alternate program (ar, ranlib) */
	ectx->program = program;

	/* output */
	if ((size_t)snprintf(output,sizeof(output),"%s",
			arfilename) >= sizeof(output))
		return -1;

	/* ar alternate argument vector */
	if ((size_t)snprintf(program,sizeof(program),"%s",
			dctx->cctx->host.ar) >= sizeof(program))
		return -1;

	aarg    = ectx->altv;
	*aarg++ = program;
	*aarg++ = "cru";
	*aarg++ = output;

	/* input argument adjustment */
	for (parg=ectx->cargv; *parg; parg++)
		if (slbt_adjust_input_argument(*parg,false))
			*aarg++ = *parg;

	*aarg = 0;
	ectx->argv = ectx->altv;

	/* step output */
	if (!(dctx->cctx->drvflags & SLBT_DRIVER_SILENT))
		if (slbt_output_link(dctx,ectx))
			return -1;

	/* ar spawn */
	if ((slbt_spawn(ectx,true) < 0) || ectx->exitcode)
		return -1;

	/* ranlib argv */
	if ((size_t)snprintf(program,sizeof(program),"%s",
			dctx->cctx->host.ranlib) >= sizeof(program))
		return -1;

	ranlib[0] = program;
	ranlib[1] = output;
	ranlib[2] = 0;
	ectx->argv = ranlib;

	/* step output */
	if (!(dctx->cctx->drvflags & SLBT_DRIVER_SILENT))
		if (slbt_output_link(dctx,ectx))
			return -1;

	/* ranlib spawn */
	if ((slbt_spawn(ectx,true) < 0) || ectx->exitcode)
		return -1;

	return 0;
}


int slbt_exec_link(
	const struct slbt_driver_ctx *	dctx,
	struct slbt_exec_ctx *		ectx)
{
	int			ret;
	int			fdlibs;
	const char *		output;
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

	/* output suffix */
	output = dctx->cctx->output;
	dot    = strrchr(output,'.');

	/* .libs directory */
	if (dctx->cctx->drvflags & SLBT_DRIVER_SHARED) {
		if ((fdlibs = open(ectx->ldirname,O_DIRECTORY)) >= 0)
			close(fdlibs);
		else if ((errno != ENOENT) || mkdir(ectx->ldirname,0777)) {
			slbt_free_exec_ctx(actx);
			return -1;
		}
	}

	/* non-pic libfoo.a */
	if (dot && !strcmp(dot,".a"))
		if (slbt_exec_link_static_archive(dctx,ectx,output)) {
			slbt_free_exec_ctx(actx);
			return -1;
		}

	/* no wrapper? */
	if (!dot || strcmp(dot,".la")) {
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
