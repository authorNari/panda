/*	PANDA -- a simple transaction monitor

Copyright (C) 2000-2003 Ogochan & JMA (Japan Medical Association).

This module is part of PANDA.

	PANDA is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY.  No author or distributor accepts responsibility
to anyone for the consequences of using it or for whether it serves
any particular purpose or works at all, unless he says so in writing.
Refer to the GNU General Public License for full details. 

	Everyone is granted permission to copy, modify and redistribute
PANDA, but only under the conditions described in the GNU General
Public License.  A copy of this license is supposed to have been given
to you along with PANDA so you can know your rights and
responsibilities.  It should be in a file named COPYING.  Among other
things, the copyright notice and this notice must be preserved on all
copies. 
*/

#ifndef	_INC_SQL_LEX_H
#define	_INC_SQL_LEX_H
#include	<glib.h>
#include	"Lex.h"

#define	SIZE_SYMBOL		255

#define	T_SQL			(T_YYBASE +1)
#define	T_INTO			(T_YYBASE +2)
#define	T_INSERT		(T_YYBASE +3)
#define	T_LIKE			(T_YYBASE +4)
#define	T_ILIKE			(T_YYBASE +5)

#ifdef	_SQL_PARSER
#define	GLOBAL	/*	*/
#else
#define	GLOBAL	extern
#endif

#undef	GLOBAL

extern	int		SQL_Lex(Bool fName);
extern	void	SQL_LexInit(void);

#endif