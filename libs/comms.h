/*
 * PANDA -- a simple transaction monitor
 * Copyright (C) 2000-2008 Ogochan & JMA (Japan Medical Association).
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef	_INC_COMMS_H
#define	_INC_COMMS_H
#include	"libmondai.h"
#include	"net.h"
extern	void	SendStringDelim(NETFILE *fp, char *str);
extern	Bool	RecvStringDelim(NETFILE *fp, size_t size, char *str);
extern	Bool	RecvLargeString(NETFILE *fp, LargeByteString *lbs);
extern	void	SendLargeString(NETFILE *fp, LargeByteString *lbs);
extern	void	SetValueName(char *name);
extern	void	SendValueString(NETFILE *fpComm, ValueStruct *value, char *name,
								Bool fName, Bool fType, char *encode);

#undef	GLOBAL
#ifdef	_COMMS
#define	GLOBAL		/*	*/
#else
#define	GLOBAL		extern
#endif

#undef	GLOBAL

#endif
