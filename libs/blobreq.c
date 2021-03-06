/*
 * PANDA -- a simple transaction monitor
 * Copyright (C) 2004-2008 Ogochan & JMA (Japan Medical Association).
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

/*
#define	DEBUG
#define	TRACE
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif
#include	<stdio.h>
#include	<stdlib.h>
#include	<unistd.h>
#include	<string.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<glib.h>


#include	"libmondai.h"
#include	"net.h"
#include	"comm.h"
#include	"blob.h"
#include	"blobreq.h"
#include	"message.h"
#include	"debug.h"

static	Bool
RequestBLOB(
	NETFILE	*fp,
	PacketClass	op)
{
	Bool	rc;

ENTER_FUNC;
	rc = FALSE;
	SendPacketClass(fp,SYSDATA_BLOB);	ON_IO_ERROR(fp,badio);
	SendPacketClass(fp,op);				ON_IO_ERROR(fp,badio);
	rc = TRUE;
  badio:
LEAVE_FUNC;
	return	(rc);
}

extern	MonObjectType
RequestNewBLOB(
	NETFILE	*fp,
	int		mode)
{
	MonObjectType	obj;

ENTER_FUNC;
	obj = GL_OBJ_NULL;
	RequestBLOB(fp,BLOB_CREATE);		ON_IO_ERROR(fp,badio);
	SendInt(fp,mode);					ON_IO_ERROR(fp,badio);
	if		(  RecvPacketClass(fp)  ==  BLOB_OK  ) {
		obj = RecvObject(fp);			ON_IO_ERROR(fp,badio);
	}
  badio:
LEAVE_FUNC;
	return	(obj);
}

extern	size_t
RequestWriteBLOB(
	NETFILE	*fp,
	MonObjectType	obj,
	unsigned char	*buff,
	size_t	size)
{
	size_t	wrote;
	
ENTER_FUNC;
	wrote = 0;
	RequestBLOB(fp,BLOB_WRITE);			ON_IO_ERROR(fp,badio);
	SendObject(fp,obj);					ON_IO_ERROR(fp,badio);
	if		(  RecvPacketClass(fp)  ==  BLOB_OK  ) {
		SendLength(fp,size);				ON_IO_ERROR(fp,badio);
		if		(  size  >  0  ) {
			Send(fp,buff,size);					ON_IO_ERROR(fp,badio);
			wrote = RecvLength(fp);				ON_IO_ERROR(fp,badio);
		}
	}
  badio:
LEAVE_FUNC;
	return	(wrote);
}

extern	size_t
RequestReadBLOB(
	NETFILE	*fp,
	MonObjectType	obj,
	unsigned char	**ret,
	size_t	*size)
{
	unsigned char 	*buff;
	size_t	red;
	
ENTER_FUNC;
	red = 0;
	*ret = NULL;
	RequestBLOB(fp,BLOB_READ);			ON_IO_ERROR(fp,badio);
	SendObject(fp,obj);					ON_IO_ERROR(fp,badio);
	if		(  RecvPacketClass(fp)  ==  BLOB_OK  ) {
		*size = RecvLength(fp);				ON_IO_ERROR(fp,badio);
		if		(  *size  >  0  ) {
			buff = xmalloc(*size);
			red = Recv(fp,buff,*size);
			*ret = buff;
		}
	}
  badio:
LEAVE_FUNC;
	return	(red);
}

extern	Bool
RequestExportBLOB(
	NETFILE	*fp,
	MonObjectType	obj,
	char			*fname)
{
	Bool	rc;
	char	buff[SIZE_BUFF];
	FILE	*fpf;
	size_t	size
		,	left;

ENTER_FUNC;
	rc = FALSE;
	RequestBLOB(fp,BLOB_EXPORT);		ON_IO_ERROR(fp,badio);
	SendObject(fp,obj);					ON_IO_ERROR(fp,badio);
	if		(  RecvPacketClass(fp)  ==  BLOB_OK  ) {
		if		(  ( fpf = Fopen(fname,"w") )  !=  NULL  ) {
			fchmod(fileno(fpf),0600);
			left = RecvLength(fp);
			while	(  left  >  0  ) {
				size = (  left  >  SIZE_BUFF  ) ? SIZE_BUFF : left;
				Recv(fp,buff,size);			ON_IO_ERROR(fp,badio);
				fwrite(buff,size,1,fpf);
				left -= size;
			}
			fclose(fpf);
			rc = TRUE;
		} else {
			Warning("could not open for write: %s", fname);
		}
	}
  badio:
LEAVE_FUNC;
	return	(rc);
}

extern	MonObjectType
RequestImportBLOB(
	NETFILE	*fp,
	char			*fname)
{
	MonObjectType	obj;
	char	buff[SIZE_BUFF];
	FILE	*fpf;
	struct	stat	sb;
	size_t	size
		,	left;

ENTER_FUNC;
	obj = GL_OBJ_NULL;
	RequestBLOB(fp,BLOB_IMPORT);		ON_IO_ERROR(fp,badio);
	if		(  RecvPacketClass(fp)  ==  BLOB_OK  ) {
		obj = RecvObject(fp);				ON_IO_ERROR(fp,badio);
		if		(  ( fpf = fopen(fname,"r") )  !=  NULL  ) {
			fstat(fileno(fpf),&sb);
			left = sb.st_size;
			SendLength(fp,left);
			Flush(fp);
			while	(  left  >  0  ) {
				size = (  left  >  SIZE_BUFF  ) ? SIZE_BUFF : left;
				fread(buff,size,1,fpf);
				Send(fp,buff,size);			ON_IO_ERROR(fp,badio);
				Flush(fp);
				left -= size;
			}
			fclose(fpf);
		} else {
			dbgprintf("could not open for read: %s", fname);
			SendLength(fp,0);
			Flush(fp);
		}
	}
  badio:
LEAVE_FUNC;
	return	(obj);
}

extern	Bool
RequestCheckBLOB(
	NETFILE	*fp,
	MonObjectType	obj)
{
	Bool	rc;

ENTER_FUNC;
	rc = FALSE;
	RequestBLOB(fp,BLOB_CHECK);			ON_IO_ERROR(fp,badio);
	SendObject(fp,obj);					ON_IO_ERROR(fp,badio);
	if		(  RecvPacketClass(fp)  ==  BLOB_OK  ) {
		rc = TRUE;
	}
  badio:
LEAVE_FUNC;
	return	(rc);
}

extern	Bool
RequestDestroyBLOB(
	NETFILE	*fp,
	MonObjectType	obj)
{
	Bool	rc;

ENTER_FUNC;
	rc = FALSE;
	RequestBLOB(fp,BLOB_DESTROY);		ON_IO_ERROR(fp,badio);
	SendObject(fp,obj);					ON_IO_ERROR(fp,badio);
	if		(  RecvPacketClass(fp)  ==  BLOB_OK  ) {
		rc = TRUE;
	}
  badio:
LEAVE_FUNC;
	return	(rc);
}

extern	Bool
RequestStartBLOB(
	NETFILE	*fp)
{
	Bool	rc;

ENTER_FUNC;
	rc = FALSE;
	RequestBLOB(fp,BLOB_START);			ON_IO_ERROR(fp,badio);
	if		(  RecvPacketClass(fp)  ==  BLOB_OK  ) {
		rc = TRUE;
	}
  badio:
LEAVE_FUNC;
	return	(rc);
}

extern	Bool
RequestCommitBLOB(
	NETFILE	*fp)
{
	Bool	rc;

ENTER_FUNC;
	rc = FALSE;
	RequestBLOB(fp,BLOB_COMMIT);		ON_IO_ERROR(fp,badio);
	if		(  RecvPacketClass(fp)  ==  BLOB_OK  ) {
		rc = TRUE;
	}
  badio:
LEAVE_FUNC;
	return	(rc);
}

extern	Bool
RequestAbortBLOB(
	NETFILE	*fp)
{
	Bool	rc;

ENTER_FUNC;
	rc = FALSE;
	RequestBLOB(fp,BLOB_ABORT);			ON_IO_ERROR(fp,badio);
	if		(  RecvPacketClass(fp)  ==  BLOB_OK  ) {
		rc = TRUE;
	}
  badio:
LEAVE_FUNC;
	return	(rc);
}
