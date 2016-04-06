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

static int slbt_exec_link_adjust_argument_vector(
	const struct slbt_driver_ctx *	dctx,
	struct slbt_exec_ctx *		ectx,
	bool				flibrary)
{
	char ** carg;
	char ** aarg;
	char *	slash;
	char *	mark;
	char *	dot;
	char	arg[PATH_MAX];
	bool	fwholearchive = false;

	carg = ectx->cargv;
	aarg = ectx->altv;

	for (; *carg; ) {
		if (!strcmp(*carg,"-Wl,--whole-archive"))
			fwholearchive = true;
		else if (!strcmp(*carg,"-Wl,--no-whole-archive"))
			fwholearchive = false;



		if (**carg == '-') {
			*aarg++ = *carg++;

		} else if (!(dot = strrchr(*carg,'.'))) {
			*aarg++ = *carg++;

		} else if (!(strcmp(dot,".a"))) {
			if (flibrary && !fwholearchive)
				*aarg++ = "-Wl,--whole-archive";

			*aarg++ = *carg++;

			if (flibrary && !fwholearchive)
				*aarg++ = "-Wl,--no-whole-archive";

		} else if (strcmp(dot,dctx->cctx->settings.dsosuffix)) {
			*aarg++ = *carg++;

		} else {
			/* account for {'-','L','-','l'} */
			if ((size_t)snprintf(arg,sizeof(arg),"%s",
					*carg) >= (sizeof(arg) - 4))
				return -1;

			if ((slash = strrchr(arg,'/'))) {
				sprintf(*carg,"-L%s",arg);

				mark   = strrchr(*carg,'/');
				*mark  = '\0';

				if (ectx->fwrapper) {
					*slash = '\0';

					if (fprintf(ectx->fwrapper,
							"DL_PATH=\"$DL_PATH$COLON%s\"\n"
							"COLON=':'\n\n",
							arg) < 0)
						return -1;
				}

				*aarg++ = *carg++;
				*aarg++ = ++mark;

				++slash;
				slash += strlen(dctx->cctx->settings.dsoprefix);

				sprintf(mark,"-l%s",slash);
				dot  = strrchr(mark,'.');
				*dot = '\0';
			} else {
				*aarg++ = *carg++;
			}
		}
	}

	return 0;
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
	char	soname [PATH_MAX];

	/* initial state */
	slbt_reset_arguments(ectx);

	/* placeholders */
	slbt_reset_placeholders(ectx);

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

	/* -soname */
	if ((size_t)snprintf(soname,sizeof(soname),"-Wl,%s%s%s.%d",
				dctx->cctx->settings.dsoprefix,
				dctx->cctx->libname,
				dctx->cctx->settings.dsosuffix,
				dctx->cctx->verinfo.major)
			>= sizeof(soname))
		return -1;

	*ectx->soname  = "-Wl,-soname";
	*ectx->lsoname = soname;

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

	/* .libs/libfoo.so --> -L.libs -lfoo */
	if (slbt_exec_link_adjust_argument_vector(
			dctx,ectx,true))
		return -1;

	/* using alternate argument vector */
	ectx->argv    = ectx->altv;
	ectx->program = ectx->altv[0];

	/* step output */
	if (!(dctx->cctx->drvflags & SLBT_DRIVER_SILENT))
		if (slbt_output_link(dctx,ectx))
			return -1;

	/* spawn */
	if ((slbt_spawn(ectx,true) < 0) || ectx->exitcode)
		return -1;

	return 0;
}

static int slbt_exec_link_create_executable(
	const struct slbt_driver_ctx *	dctx,
	struct slbt_exec_ctx *		ectx,
	const char *			exefilename)
{
	char ** parg;
	char	cwd    [PATH_MAX];
	char	output [PATH_MAX];
	char	wrapper[PATH_MAX];

	/* initial state */
	slbt_reset_arguments(ectx);

	/* placeholders */
	slbt_reset_placeholders(ectx);

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

	/* executable wrapper: header */
	if ((size_t)snprintf(wrapper,sizeof(wrapper),"%s.wrapper.tmp",
				dctx->cctx->output)
			>= sizeof(wrapper))
		return -1;

	if (!(ectx->fwrapper = fopen(wrapper,"w")))
		return -1;

	if (fprintf(ectx->fwrapper,
			"#!/bin/sh\n"
			"# slibtool (pre-alpha): generated executable wrapper\n\n"
			"if [ -z \"$%s\" ]; then\n"
			"\tDL_PATH=\n"
			"\tCOLON=\n"
			"\tLCOLON=\n"
			"else\n"
			"\tDL_PATH=\n"
			"\tCOLON=\n"
			"\tLCOLON=':'\n"
			"fi\n\n",
			dctx->cctx->settings.ldpathenv) < 0)
		return -1;

	/* output */
	if ((size_t)snprintf(output,sizeof(output),"%s",
				exefilename)
			>= sizeof(output))
		return -1;

	*ectx->lout[0] = "-o";
	*ectx->lout[1] = output;

	/* .libs/libfoo.so --> -L.libs -lfoo */
	if (slbt_exec_link_adjust_argument_vector(
			dctx,ectx,false))
		return -1;

	/* using alternate argument vector */
	ectx->argv    = ectx->altv;
	ectx->program = ectx->altv[0];


	/* executable wrapper: footer */
	if (!getcwd(cwd,sizeof(cwd)))
		return -1;

	if (fprintf(ectx->fwrapper,
			"DL_PATH=\"$DL_PATH$LCOLON$%s\"\n\n"
			"export %s=$DL_PATH\n\n"
			"exec %s/%s \"$@\"\n",
			dctx->cctx->settings.ldpathenv,
			dctx->cctx->settings.ldpathenv,
			cwd,exefilename) < 0)
		return -1;

	/* step output */
	if (!(dctx->cctx->drvflags & SLBT_DRIVER_SILENT))
		if (slbt_output_link(dctx,ectx))
			return -1;

	/* spawn */
	if ((slbt_spawn(ectx,true) < 0) || ectx->exitcode)
		return -1;

	/* executable wrapper: finalize */
	fclose(ectx->fwrapper);
	ectx->fwrapper = 0;

	if (rename(wrapper,dctx->cctx->output))
		return -1;

	if (chmod(dctx->cctx->output,0755))
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

	/* executable */
	if (!dctx->cctx->rpath && !dctx->cctx->libname) {
		/* linking: .libs/exefilename */
		if (slbt_exec_link_create_executable(
				dctx,ectx,
				ectx->exefilename)) {
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
		"# slibtool (pre-alpha) generated file\n\n");

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
