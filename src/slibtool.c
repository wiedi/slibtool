/*******************************************************************/
/*  slibtool: a skinny libtool implementation, written in C        */
/*  Copyright (C) 2016  Z. Gilboa                                  */
/*  Released under the Standard MIT License; see COPYING.SLIBTOOL. */
/*******************************************************************/

#include <slibtool/slibtool.h>

int main(int argc, char ** argv, char ** envp)
{
	return slbt_main(argc,argv,envp);
}
