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
#include	<sys/time.h>
#include	<sys/wait.h>
#include	<glib.h>
#include	<pthread.h>
#include	<fcntl.h>
#include	<errno.h>

#include	"types.h"

#include	"libmondai.h"
#include	"net.h"
#include	"comm.h"
#include	"queue.h"
#include	"directory.h"
#include	"blob.h"
#include	"message.h"
#include	"debug.h"

#define	LockBLOB(blob)		pthread_mutex_lock(&(blob)->mutex)
#define	UnLockBLOB(blob)	pthread_mutex_unlock(&(blob)->mutex)
#define	ReleaseBLOB(blob)	pthread_cond_signal(&(blob)->cond)
#define	WaitBLOB(blod)		pthread_cond_wait(&(blob)->cond,&(blob)->mutex);

static	void
OpenEntry(
	BLOB_Entry	*ent)
{
	char	longname[SIZE_LONGNAME+1];
	char	filename[SIZE_NAME+1];
	char	*p;
	int		i;
	int		mode;
	int		fd;

	p  = filename;
	p += sprintf(p,"%d",ObjectID(ent->oid));
	sprintf(longname,"%s/%s",ent->blob->space,filename);
	mode = 0;
	if		(  ( ent->mode & BLOB_OPEN_READ )  !=  0  ) {
		mode |= O_RDONLY;
	}
	if		(  ( ent->mode & BLOB_OPEN_WRITE )  !=  0  ) {
		mode |= O_WRONLY;
	}
	if		(  ( ent->mode & BLOB_OPEN_CREATE )  !=  0  ) {
		mode |= O_CREAT;
	}
	if		(  ( fd = open(longname,mode) )  >=  0  ) {
		ent->fp = FileToNet(fd);
	} else {
		ent->fp = NULL;
	}
}

static	void
DestroyEntry(
	BLOB_Entry	*ent)
{
	if		(  ent->oid  !=  NULL  ) {
		LockBLOB(ent->blob);
		g_hash_table_remove(ent->blob->table,(gpointer)ent->oid);
		UnLockBLOB(ent->blob);
		ReleaseBLOB(ent->blob);
		xfree(ent->oid);
	}
	if		(  ent->fp  !=  NULL  ) {
		CloseNet(ent->fp);
	}
	xfree(ent);
}	

extern	Bool
NewBLOB(
	BLOB_Space		*blob,
	MonObjectType	*obj,
	int				mode)
{
	char	name[SIZE_LONGNAME+1];
	FILE	*fp;
	BLOB_Entry	*ent;
	Bool	ret;

ENTER_FUNC;
	LockBLOB(blob);
	ObjectID(obj) = blob->oid;
	blob->oid ++;
	sprintf(name,"%s/oid",blob->space);
	if		(  ( fp = fopen(name,"w") )  !=  NULL  ) {
		fprintf(fp,"%d\n",blob->oid);
		fclose(fp);
	}
	ent = New(BLOB_Entry);
	ent->oid = New(MonObjectType);
	memcpy(ent->oid,obj,sizeof(MonObjectType));
	g_hash_table_insert(blob->table,(gpointer)ent->oid,ent);
	UnLockBLOB(blob);
	ReleaseBLOB(blob);
	ent->mode = mode | BLOB_OPEN_CREATE;
	ent->blob = blob;
	OpenEntry(ent);
	if		(  ent->fp  ==  NULL  ) {
		DestroyEntry(ent);
		ret = FALSE;
	} else {
		ret = TRUE;
	}
LEAVE_FUNC;
	return	(ret);
}

static	guint
IdHash(
	MonObjectType	*key)
{
	int		i;
	guint	ret;

	ret = 0;
	if		(  key  !=  NULL  ) {
		for	( i = 0 ; i < (SIZE_OID/sizeof(unsigned int)) ; i ++ ) {
			ret += key->id.el[i];
		}
	}
	return	(ret);
}

static	gint
IdCompare(
	MonObjectType	*o1,
	MonObjectType	*o2)
{
	int		i;
	guint	check;

	if		(	(  o1  !=  NULL  )
			&&	(  o2  !=  NULL  ) ) {
		check = 0;
		for	( i = 0 ; i < (SIZE_OID/sizeof(unsigned int)) ; i ++ ) {
			check += o1->id.el[i] - o2->id.el[i];
		}
	} else {
		check = 1;
	}
	return	(check == 0);
}

extern	BLOB_Space	*
InitBLOB(
	char	*space)
{
	FILE	*fp;
	char	name[SIZE_LONGNAME+1];
	char	buff[SIZE_BUFF];
	BLOB_Space	*blob;
	size_t	oid;

	sprintf(name,"%s/pid",space);
	if		(  ( fp = fopen(name,"r") )  !=  NULL  ) {
		if		(  fgets(buff,SIZE_BUFF,fp)  !=  NULL  ) {
			int pid = atoi(buff);

			if		(	(  getpid() != pid  )
					&&	(  kill(pid, 0) == 0 || errno == EPERM  ) ) {
				fprintf(stderr,"another process uses libpandablob. (%d)\n",atoi(buff));
				exit(1);
			}
		}
		fclose(fp);
	}
	if		(  ( fp = fopen(name,"w") )  !=  NULL  ) {
		fprintf(fp,"%d\n",getpid());
		fclose(fp);
	} else {
		fprintf(stderr,"can not make lock file(directory not writable?)\n");
		exit(1);
	}
	sprintf(name,"%s/oid",space);
	if		(  ( fp = fopen(name,"r") )  !=  NULL  ) {
		if		(  fgets(buff,SIZE_BUFF,fp)  !=  NULL  ) {
			oid = atoi(buff);
		} else {
			oid = 0;
		}
		fclose(fp);
	} else {
		oid = 0;
	}

	blob = New(BLOB_Space);
	blob->space = StrDup(space);
	blob->oid = oid;
	blob->table = g_hash_table_new((GHashFunc)IdHash,(GCompareFunc)IdCompare);
	pthread_mutex_init(&blob->mutex,NULL);
	pthread_cond_init(&blob->cond,NULL);

	return	(blob);
}

extern	void
FinishBLOB(
	BLOB_Space	*blob)
{
	FILE	*fp;
	char	name[SIZE_LONGNAME+1];

	sprintf(name,"%s/pid",blob->space);
	unlink(name);

	xfree(blob->space);
	pthread_mutex_destroy(&blob->mutex);
	pthread_cond_destroy(&blob->cond);
	xfree(blob);
}

extern	Bool
OpenBLOB(
	BLOB_Space		*blob,
	MonObjectType	*obj,
	int				mode)
{
}

extern	Bool
CloseBLOB(
	BLOB_Space		*blob,
	MonObjectType	*obj)
{
}

extern	size_t
WriteBLOB(
	BLOB_Space		*blob,
	MonObjectType	*obj,
	byte			*buff,
	size_t			size)
{
}

extern	size_t
ReadBLOB(
	BLOB_Space		*blob,
	MonObjectType	*obj,
	byte			*buff,
	size_t			size)
{
}
