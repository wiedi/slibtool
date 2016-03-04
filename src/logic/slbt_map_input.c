/*******************************************************************/
/*  slibtool: a skinny libtool implementation, written in C        */
/*  Copyright (C) 2016  Z. Gilboa                                  */
/*  Released under the Standard MIT License; see COPYING.SLIBTOOL. */
/*******************************************************************/

#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <slibtool/slibtool.h>

int slbt_map_input(
	int			fd,
	const char *		path,
	int			prot,
	struct slbt_input *	map)
{
	struct stat	st;
	bool		fnew;
	int		ret;

	if ((fnew = (fd < 0)))
		fd  = open(path,O_RDONLY | O_CLOEXEC);

	if (fd < 0)
		return -1;

	if ((ret = fstat(fd,&st) < 0) && fnew)
		close(fd);

	if (ret < 0)
		return -1;

	map->size = st.st_size;
	map->addr = mmap(0,map->size,prot,MAP_PRIVATE,fd,0);

	if (fnew)
		close(fd);

	return (map->addr == MAP_FAILED) ? -1 : 0;
}

int slbt_unmap_input(struct slbt_input * map)
{
	return munmap(map->addr,map->size);
}
