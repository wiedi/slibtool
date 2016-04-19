/*******************************************************************/
/*  slibtool: a skinny libtool implementation, written in C        */
/*  Copyright (C) 2016  Z. Gilboa                                  */
/*  Released under the Standard MIT License; see COPYING.SLIBTOOL. */
/*******************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

#define ARGV_DRIVER

#include <slibtool/slibtool.h>
#include "slibtool_install_impl.h"
#include "slibtool_readlink_impl.h"
#include "slibtool_spawn_impl.h"
#include "slibtool_symlink_impl.h"
#include "argv/argv.h"

static int slbt_install_usage(
	const char *			program,
	const char *			arg,
	const struct argv_option *	options,
	struct argv_meta *		meta)
{
	char header[512];

	snprintf(header,sizeof(header),
		"Usage: %s --mode=install <install> [options] [SOURCE]... DEST\n"
		"Options:\n",
		program);

	argv_usage(stdout,header,options,arg);
	argv_free(meta);

	return SLBT_USAGE;
}

static int slbt_exec_install_fail(
	struct slbt_exec_ctx *	actx,
	struct argv_meta *	meta)
{
	argv_free(meta);
	slbt_free_exec_ctx(actx);
	return -1;
}

static int slbt_exec_install_init_dstdir(
	struct argv_entry *	dest,
	struct argv_entry *	last,
	char *			dstdir)
{
	struct stat	st;
	char *		slash;
	size_t		len;

	if (dest)
		last = dest;

	/* dstdir: initial string */
	if ((size_t)snprintf(dstdir,PATH_MAX,"%s",
			last->arg) >= PATH_MAX)
		return -1;

	/* dstdir might end with a slash */
	len = strlen(dstdir);

	if (dstdir[--len] == '/')
		dstdir[len] = '\0';

	/* -t DSTDIR? */
	if (dest)
		return 0;

	/* is DEST a directory? */
	if (!(stat(dstdir,&st)))
		if (S_ISDIR(st.st_mode))
			return 0;

	/* remove last path component */
	if ((slash = strrchr(dstdir,'/')))
		*slash = '\0';

	return 0;
}

static int slbt_exec_install_entry(
	const struct slbt_driver_ctx *	dctx,
	struct slbt_exec_ctx *		ectx,
	struct argv_entry *		entry,
	struct argv_entry *		last,
	struct argv_entry *		dest,
	char *				dstdir,
	char **				src,
	char **				dst)
{
	int		ret;
	char *		dot;
	char *		base;
	char *		slash;
	char		target  [PATH_MAX];
	char		srcfile [PATH_MAX];
	char		dstfile [PATH_MAX];
	char		slnkname[PATH_MAX];
	char		dlnkname[PATH_MAX];
	char		lasource[PATH_MAX];
	bool		fexe = false;
	bool		fpe;
	struct stat	st;

	/* executable wrapper? */
	if ((size_t)snprintf(slnkname,sizeof(slnkname),"%s.exe.wrapper",
			entry->arg) >= sizeof(slnkname))
		return -1;

	fexe = stat(slnkname,&st)
		? false
		: true;

	/* .la ? */
	if (!fexe && (!(dot = strrchr(entry->arg,'.')) || strcmp(dot,".la"))) {
		*src = (char *)entry->arg;
		*dst = dest ? 0 : (char *)last->arg;

		if (!(dctx->cctx->drvflags & SLBT_DRIVER_SILENT))
			if (slbt_output_install(dctx,ectx))
				return -1;

		return (((ret = slbt_spawn(ectx,true)) < 0) || ectx->exitcode)
			? -1 : 0;
	}

	/* srcfile */
	if (strlen(entry->arg) + strlen(".libs/") >= (PATH_MAX-1))
		return -1;

	strcpy(lasource,entry->arg);

	if ((slash = strrchr(lasource,'/'))) {
		*slash++ = '\0';
		sprintf(srcfile,"%s/.libs/%s",lasource,slash);
	} else
		sprintf(srcfile,".libs/%s",lasource);

	/* executable? */
	if (fexe) {
		*src = srcfile;
		*dst = dest ? 0 : (char *)last->arg;

		if (!(dctx->cctx->drvflags & SLBT_DRIVER_SILENT))
			if (slbt_output_install(dctx,ectx))
				return -1;

		return (((ret = slbt_spawn(ectx,true)) < 0) || ectx->exitcode)
			? -1 : 0;
	}

	/* libfoo.la --> libfoo.so */
	strcpy(slnkname,srcfile);
	dot = strrchr(slnkname,'.');
	strcpy(dot,dctx->cctx->settings.dsosuffix);

	/* PE support: does .libs/libfoo.so.def exist? */
	if ((size_t)snprintf(dstfile,sizeof(dstfile),"%s.def",
			slnkname) >= sizeof(dstfile))
		return -1;

	fpe = stat(dstfile,&st) ? false : true;

	/* basename */
	if ((base = strrchr(slnkname,'/')))
		base++;
	else
		base = slnkname;

	/* source (build) symlink target */
	if (slbt_readlink(slnkname,target,sizeof(target)) < 0) {
		/* -avoid-version? */
		if (stat(slnkname,&st))
			return -1;

		/* dstfile */
		if ((size_t)snprintf(dstfile,sizeof(dstfile),"%s/%s",
				dstdir,base) >= sizeof(dstfile))
			return -1;

		/* single spawn, no symlinks */
		*src = slnkname;
		*dst = dest ? 0 : dstfile;

		if (!(dctx->cctx->drvflags & SLBT_DRIVER_SILENT))
			if (slbt_output_install(dctx,ectx))
				return -1;

		if (((ret = slbt_spawn(ectx,true)) < 0) || ectx->exitcode)
			return -1;

		return 0;
	}

	/* srcfile: .libs/libfoo.so.x.y.z */
	slash = strrchr(srcfile,'/');
	strcpy(++slash,target);

	/* dstfile */
	if (!dest)
		if ((size_t)snprintf(dstfile,sizeof(dstfile),"%s/%s",
				dstdir,target) >= sizeof(dstfile))
			return -1;

	/* spawn */
	*src = srcfile;
	*dst = dest ? 0 : dstfile;

	if (!(dctx->cctx->drvflags & SLBT_DRIVER_SILENT))
		if (slbt_output_install(dctx,ectx))
			return -1;

	if (((ret = slbt_spawn(ectx,true)) < 0) || ectx->exitcode)
		return -1;

	/* destination symlink: dstdir/libfoo.so */
	if ((size_t)snprintf(dlnkname,sizeof(dlnkname),"%s/%s",
			dstdir,base) >= sizeof(dlnkname))
		return -1;

	/* create symlink: libfoo.so --> libfoo.so.x.y.z */
	if (slbt_create_symlink(
			dctx,ectx,
			target,dlnkname,
			false))
		return -1;

	/* libfoo.so.x.y.z --> libfoo.so.x */
	strcpy(slnkname,target);

	if ((dot = strrchr(slnkname,'.')))
		*dot = '\0';
	else
		return -1;

	if ((dot = strrchr(slnkname,'.')))
		*dot = '\0';
	else
		return -1;

	/* destination symlink: dstdir/libfoo.so.x */
	if ((size_t)snprintf(dlnkname,sizeof(dlnkname),"%s/%s",
			dstdir,slnkname) >= sizeof(dlnkname))
		return -1;

	if (fpe) {
		/* copy: .libs/libfoo.so.x.y.z --> libfoo.so.x */
		if (slbt_copy_file(
				dctx,ectx,
				srcfile,
				dlnkname))
			return -1;
	} else {
		/* create symlink: libfoo.so.x --> libfoo.so.x.y.z */
		if (slbt_create_symlink(
				dctx,ectx,
				target,dlnkname,
				false))
			return -1;
	}

	return 0;
}

int slbt_exec_install(
	const struct slbt_driver_ctx *	dctx,
	struct slbt_exec_ctx *		ectx)
{
	int				ret;
	char **				argv;
	char **				src;
	char **				dst;
	struct slbt_exec_ctx *		actx;
	struct argv_meta *		meta;
	struct argv_entry *		entry;
	struct argv_entry *		copy;
	struct argv_entry *		dest;
	struct argv_entry *		last;
	char				dstdir[PATH_MAX];
	const struct argv_option *	options = slbt_install_options;

	/* context */
	if (ectx)
		actx = 0;
	else if ((ret = slbt_get_exec_ctx(dctx,&ectx)))
		return ret;
	else
		actx = ectx;

	/* initial state, install mode skin */
	slbt_reset_arguments(ectx);
	slbt_disable_placeholders(ectx);
	argv = ectx->cargv;

	/* missing arguments? */
	if (!argv[1] && (dctx->cctx->drvflags & SLBT_DRIVER_VERBOSITY_USAGE))
		return slbt_install_usage(dctx->program,0,options,0);

	/* <install> argv meta */
	if (!(meta = argv_get(
			argv,
			options,
			dctx->cctx->drvflags & SLBT_DRIVER_VERBOSITY_ERRORS
				? ARGV_VERBOSITY_ERRORS
				: ARGV_VERBOSITY_NONE)))
		return slbt_exec_install_fail(actx,meta);

	/* dest, alternate argument vector options */
	argv = ectx->altv;
	copy = meta->entries;
	dest = 0;
	last = 0;

	*argv++ = ectx->cargv[0];

	for (entry=meta->entries; entry->fopt || entry->arg; entry++) {
		if (entry->fopt) {
			switch (entry->tag) {
				case TAG_INSTALL_COPY:
					*argv++ = "-c";
					copy = entry;
					break;

				case TAG_INSTALL_MKDIR:
					*argv++ = "-d";
					copy = 0;
					break;

				case TAG_INSTALL_TARGET_MKDIR:
					*argv++ = "-D";
					copy = 0;
					break;

				case TAG_INSTALL_STRIP:
					*argv++ = "-s";
					break;

				case TAG_INSTALL_PRESERVE:
					*argv++ = "-p";
					break;

				case TAG_INSTALL_USER:
					*argv++ = "-o";
					break;

				case TAG_INSTALL_GROUP:
					*argv++ = "-g";
					break;

				case TAG_INSTALL_MODE:
					*argv++ = "-m";
					break;

				case TAG_INSTALL_DSTDIR:
					*argv++ = "-t";
					dest = entry;
					break;
			}

			if (entry->fval)
				*argv++ = (char *)entry->arg;
		} else
			last = entry;
	}

	/* install */
	if (copy) {
		/* using alternate argument vector */
		ectx->argv    = ectx->altv;
		ectx->program = ectx->altv[0];

		/* marks */
		src = argv++;
		dst = argv++;

		/* dstdir */
		if (slbt_exec_install_init_dstdir(dest,last,dstdir))
			return slbt_exec_install_fail(actx,meta);

		/* install entries one at a time */
		for (entry=meta->entries; entry->fopt || entry->arg; entry++)
			if (!entry->fopt && (dest || (entry != last)))
				if (slbt_exec_install_entry(
						dctx,ectx,
						entry,last,
						dest,dstdir,
						src,dst))
					return slbt_exec_install_fail(actx,meta);
	} else {
		/* using original argument vector */
		ectx->argv    = ectx->cargv;
		ectx->program = ectx->cargv[0];

		/* spawn */
		if (!(dctx->cctx->drvflags & SLBT_DRIVER_SILENT))
			if (slbt_output_install(dctx,ectx))
				return -1;

		if (((ret = slbt_spawn(ectx,true)) < 0) || ectx->exitcode)
			return slbt_exec_install_fail(actx,meta);
	}

	argv_free(meta);
	slbt_free_exec_ctx(actx);

	return 0;
}
