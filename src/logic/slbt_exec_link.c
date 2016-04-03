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
/* libfoo.a    [-shared|-static]     bar.lo  libfoo.a              */
/*                                                                 */
/* ar cru libfoo.a bar.o                                           */
/* ranlib libfoo.a                                                 */
/*                                                                 */
/*******************************************************************/

/*******************************************************************/
/*                                                                 */
/* -o <ltlib>  switches              input   result                */
/* ----------  --------------------- -----   ------                */
/* libfoo.la   -shared               bar.lo  libfoo.la             */
/*                                           .libs/libfoo.a        */
/*                                           .libs/libfoo.la (lnk) */
/*                                                                 */
/* ar cru .libs/libfoo.a .libs/bar.o                               */
/* ranlib .libs/libfoo.a                                           */
/* (generate libfoo.la)                                            */
/* ln -s ../libfoo.la .libs/libfoo.la                              */
/*                                                                 */
/*******************************************************************/

/*******************************************************************/
/*                                                                 */
/* -o <ltlib>  switches              input   result                */
/* ----------  --------------------- -----   ------                */
/* libfoo.la   -static               bar.lo  libfoo.la             */
/*                                           .libs/libfoo.a        */
/*                                           .libs/libfoo.la (lnk) */
/*                                                                 */
/* ar cru .libs/libfoo.a bar.o                                     */
/* ranlib .libs/libfoo.a                                           */
/* (generate libfoo.la)                                            */
/* ln -s ../libfoo.la .libs/libfoo.la                              */
/*                                                                 */
/*******************************************************************/

static bool slbt_adjust_input_argument(char * arg, bool fpic)
{
	char *	slash;
	char *	dot;
	char	base[PATH_MAX];

	if (*arg == '-')
		return false;

	if (!(dot = strrchr(arg,'.')))
		return false;

	if (strcmp(dot,".lo"))
		return false;

	if (fpic) {
		if ((slash = strrchr(arg,'/')))
			slash++;
		else
			slash = arg;

		if ((size_t)snprintf(base,sizeof(base),"%s",
				slash) >= sizeof(base))
			return false;

		sprintf(slash,".libs/%s",base);
		dot = strrchr(arg,'.');
	}

	dot[1] = 'o';
	dot[2] = '\0';
	return true;
}

static bool slbt_adjust_linker_argument(
	char *		arg,
	bool		fpic,
	const char *	dsosuffix,
	const char *	arsuffix)
{
	int	fdlib;
	char *	slash;
	char *	dot;
	char	base[PATH_MAX];

	if (*arg == '-')
		return false;

	if (!(dot = strrchr(arg,'.')))
		return false;

	if (strcmp(dot,".la"))
		return false;

	if (fpic) {
		if ((slash = strrchr(arg,'/')))
			slash++;
		else
			slash = arg;

		if ((size_t)snprintf(base,sizeof(base),"%s",
				slash) >= sizeof(base))
			return false;

		sprintf(slash,".libs/%s",base);
		dot = strrchr(arg,'.');
	}

	/* shared library dependency? */
	if (fpic) {
		sprintf(dot,"%s",dsosuffix);

		if ((fdlib = open(arg,O_RDONLY)) >= 0)
			close(fdlib);
		else
			sprintf(dot,"%s",arsuffix);

		return true;
	}

	/* input archive */
	sprintf(dot,"%s",arsuffix);
	return true;
}

static int slbt_exec_link_remove_file(
	const struct slbt_driver_ctx *	dctx,
	struct slbt_exec_ctx *		ectx,
	const char *			target)
{
	/* remove target (if any) */
	if (!(unlink(target)) || (errno == ENOENT))
		return 0;

	if (!(dctx->cctx->drvflags & SLBT_DRIVER_SILENT))
		strerror(errno);

	return -1;
}

static int slbt_exec_link_create_archive(
	const struct slbt_driver_ctx *	dctx,
	struct slbt_exec_ctx *		ectx,
	const char *			arfilename,
	bool				fpic)
{
	char ** 	aarg;
	char ** 	parg;
	char *		ranlib[3];
	char		program[PATH_MAX];
	char		output [PATH_MAX];

	/* initial state */
	slbt_reset_arguments(ectx);

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
		if (slbt_adjust_input_argument(*parg,fpic))
			*aarg++ = *parg;

	*aarg = 0;
	ectx->argv = ectx->altv;

	/* step output */
	if (!(dctx->cctx->drvflags & SLBT_DRIVER_SILENT))
		if (slbt_output_link(dctx,ectx))
			return -1;

	/* remove old archive as needed */
	if (slbt_exec_link_remove_file(dctx,ectx,output))
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

static int slbt_exec_link_create_library(
	const struct slbt_driver_ctx *	dctx,
	struct slbt_exec_ctx *		ectx,
	const char *			dsofilename)
{
	char ** parg;
	char	output [PATH_MAX];

	/* initial state */
	slbt_reset_arguments(ectx);

	/* placeholders */
	slbt_reset_placeholders(ectx);

	/* using default argument vector */
	ectx->argv    = ectx->cargv;
	ectx->program = ectx->cargv[0];

	/* input argument adjustment */
	for (parg=ectx->cargv; *parg; parg++)
		slbt_adjust_input_argument(*parg,true);

	/* linker argument adjustment */
	for (parg=ectx->cargv; *parg; parg++)
		slbt_adjust_linker_argument(
			*parg,true,
			dctx->cctx->settings.dsosuffix,
			dctx->cctx->settings.arsuffix);

	/* --no-undefined */
	if (dctx->cctx->drvflags & SLBT_DRIVER_NO_UNDEFINED)
		*ectx->noundef = "-Wl,--no-undefined";

	/* shared object */
	*ectx->dpic = "-shared";
	*ectx->fpic = "-fPIC";

	/* output */
	if ((size_t)snprintf(output,sizeof(output),"%s.%d.%d.%d",
				dsofilename,
				dctx->cctx->verinfo.major,
				dctx->cctx->verinfo.minor,
				dctx->cctx->verinfo.revision)
			>= sizeof(output))
		return -1;

	*ectx->lout[0] = "-o";
	*ectx->lout[1] = output;

	/* step output */
	if (!(dctx->cctx->drvflags & SLBT_DRIVER_SILENT))
		if (slbt_output_link(dctx,ectx))
			return -1;

	/* spawn */
	if ((slbt_spawn(ectx,true) < 0) || ectx->exitcode)
		return -1;

	return 0;
}

static int slbt_exec_link_create_symlink(
	const struct slbt_driver_ctx *	dctx,
	struct slbt_exec_ctx *		ectx,
	const char *			target,
	char *				lnkname,
	bool				flawrapper)
{
	const char *	slash;
	char *		ln[5];
	char *		dotdot;
	char		atarget[PATH_MAX];

	/* atarget */
	if ((slash = strrchr(target,'/')))
		slash++;
	else
		slash = target;

	dotdot = flawrapper ? "../" : "";

	if ((size_t)snprintf(atarget,sizeof(atarget),"%s%s",
			dotdot,slash) >= sizeof(atarget))
		return -1;

	/* ln argv (fake) */
	ln[0] = "ln";
	ln[1] = "-s";
	ln[2] = atarget;
	ln[3] = lnkname;
	ln[4] = 0;
	ectx->argv = ln;

	/* step output */
	if (!(dctx->cctx->drvflags & SLBT_DRIVER_SILENT))
		if (slbt_output_link(dctx,ectx))
			return -1;

	/* remove old symlink as needed */
	if (slbt_exec_link_remove_file(dctx,ectx,lnkname))
		return -1;

	/* create symlink */
	return symlink(atarget,lnkname);
}

static int slbt_exec_link_create_library_symlink(
	const struct slbt_driver_ctx *	dctx,
	struct slbt_exec_ctx *		ectx,
	bool				fmajor)
{
	char	target[PATH_MAX];
	char	lnkname[PATH_MAX];

	sprintf(target,"%s.%d.%d.%d",
		ectx->dsofilename,
		dctx->cctx->verinfo.major,
		dctx->cctx->verinfo.minor,
		dctx->cctx->verinfo.revision);

	if (fmajor)
		sprintf(lnkname,"%s.%d",
			ectx->dsofilename,
			dctx->cctx->verinfo.major);

	else
		strcpy(lnkname,ectx->dsofilename);

	return slbt_exec_link_create_symlink(
		dctx,ectx,
		target,lnkname,
		false);
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
		if (slbt_exec_link_create_archive(dctx,ectx,output,false)) {
			slbt_free_exec_ctx(actx);
			return -1;
		}

	/* pic libfoo.a */
	if (dot && !strcmp(dot,".la"))
		if (slbt_exec_link_create_archive(
				dctx,ectx,
				ectx->arfilename,
				dctx->cctx->drvflags & SLBT_DRIVER_SHARED)) {
			slbt_free_exec_ctx(actx);
			return -1;
		}

	/* dynamic library */
	if (dot && !strcmp(dot,".la") && dctx->cctx->rpath) {
		/* linking: libfoo.so.x.y.z */
		if (slbt_exec_link_create_library(
				dctx,ectx,
				ectx->dsofilename)) {
			slbt_free_exec_ctx(actx);
			return -1;
		}

		/* symlink: libfoo.so.x --> libfoo.so.x.y.z */
		if (slbt_exec_link_create_library_symlink(
				dctx,ectx,
				true)) {
			slbt_free_exec_ctx(actx);
			return -1;
		}

		/* symlink: libfoo.so --> libfoo.so.x.y.z */
		if (slbt_exec_link_create_library_symlink(
				dctx,ectx,
				false)) {
			slbt_free_exec_ctx(actx);
			return -1;
		}
	}

	/* no wrapper? */
	if (!dot || strcmp(dot,".la")) {
		slbt_free_exec_ctx(actx);
		return 0;
	}

	/* hey, yo, let's rap it up */
	if (!(fout = fopen(output,"w"))) {
		slbt_free_exec_ctx(actx);
		return -1;
	}

	ret = fprintf(fout,
		"# slibtool (pre-alphe) generated file\n\n");

	/* wrapper symlink */
	if (slbt_exec_link_create_symlink(
			dctx,ectx,
			output,
			ectx->lafilename,
			true)) {
		slbt_free_exec_ctx(actx);
		return -1;
	}

	/* all done */
	fclose(fout);
	slbt_free_exec_ctx(actx);

	return (ret > 0) ? 0 : -1;
}
