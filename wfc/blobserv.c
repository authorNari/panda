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
#include    <sys/types.h>
#include	<sys/stat.h>
#include	<glib.h>

#include	"types.h"

#include	"libmondai.h"
#include	"net.h"
#include	"comm.h"
#include	"blob.h"
#include	"blobcom.h"
#include	"blobserv.h"
#include	"debug.h"

extern	void
PassiveBLOB(
	NETFILE		*fp,
	BLOB_Space	*Blob)
{
	MonObjectType	obj;
	int				mode;
	size_t			size;
	ssize_t			ssize;
	byte			*buff;

	switch	(RecvPacketClass(fp)) {
	  case	BLOB_CREATE:
		mode = RecvInt(fp);
		if		(  NewBLOB(Blob,&obj,mode)  ) {
			SendPacketClass(fp,BLOB_OK);
			SendObject(fp,&obj);
		} else {
			SendPacketClass(fp,BLOB_NOT);
		}
		break;
	  case	BLOB_OPEN:
		mode = RecvInt(fp);
		RecvObject(fp,&obj);
		if		(  OpenBLOB(Blob,&obj,mode)  >=  0  ) {
			SendPacketClass(fp,BLOB_OK);
		} else {
			SendPacketClass(fp,BLOB_NOT);
		}
		break;
	  case	BLOB_WRITE:
		RecvObject(fp,&obj);
		if		(  ( size = RecvLength(fp) )  >  0  ) {
			buff = xmalloc(size);
			Recv(fp,buff,size);
			size = WriteBLOB(Blob,&obj,buff,size);
			xfree(buff);
		}
		SendLength(fp,size);
		break;
	  case	BLOB_READ:
		RecvObject(fp,&obj);
		if		(  ( size = RecvLength(fp) )  >  0  ) {
			buff = xmalloc(size);
			size = ReadBLOB(Blob,&obj,buff,size);
			Send(fp,buff,size);
			xfree(buff);
		}
		SendLength(fp,size);
		break;
	  case	BLOB_EXPORT:
		RecvObject(fp,&obj);
		if		(  ( ssize = OpenBLOB(Blob,&obj,BLOB_OPEN_READ) )  >=  0  ) {
			SendPacketClass(fp,BLOB_OK);
			SendLength(fp,ssize);
			buff = xmalloc((  ssize  >  SIZE_BUFF  ) ? SIZE_BUFF : ssize);
			while	(  ssize  >  0  ) {
				size = (  ssize  >  SIZE_BUFF  ) ? SIZE_BUFF : ssize;
				ReadBLOB(Blob,&obj,buff,size);
				Send(fp,buff,size);
				ssize -= size;
			}
			CloseBLOB(Blob,&obj);
			xfree(buff);
		} else {
			SendPacketClass(fp,BLOB_NOT);
		}
		break;
	  case	BLOB_IMPORT:
		if		(  NewBLOB(Blob,&obj,BLOB_OPEN_WRITE)  ) {
			SendPacketClass(fp,BLOB_OK);
			SendObject(fp,&obj);
			ssize = RecvLength(fp);
			buff = xmalloc((  ssize  >  SIZE_BUFF  ) ? SIZE_BUFF : ssize);
			while	(  ssize  >  0  ) {
				size = (  ssize  >  SIZE_BUFF  ) ? SIZE_BUFF : ssize;
				Recv(fp,buff,size);
				WriteBLOB(Blob,&obj,buff,size);
				ssize -= size;
			}
			CloseBLOB(Blob,&obj);
			xfree(buff);
		} else {
			SendPacketClass(fp,BLOB_NOT);
		}
		break;
	  case	BLOB_CLOSE:
		RecvObject(fp,&obj);
		if		(  CloseBLOB(Blob,&obj)  ) {
			SendPacketClass(fp,BLOB_OK);
		} else {
			SendPacketClass(fp,BLOB_NOT);
		}
		break;
	  default:
		break;
	}
}
