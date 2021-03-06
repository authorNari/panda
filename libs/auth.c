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

/*
#define	DEBUG
#define	TRACE
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifdef HAVE_CRYPT_H
#include	<crypt.h>
#endif
#include	<sys/types.h>
#include	<sys/time.h>
#include	<unistd.h>
#include	<time.h>
#include	<sys/stat.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<ctype.h>
#include	<glib.h>
#include	<errno.h>
#include	<assert.h>

#include	"libmondai.h"
#include	"RecParser.h"
#include	"monapi.h"
#include	"DDparser.h"
#include	"auth.h"
#include	"message.h"
#include	"debug.h"

#undef	Error
#define	Error(msg)	printf("%s\n",(msg))

static	GHashTable		*PasswdTable;
static	PassWord		**PasswdPool;
static	int				cPass;

extern	const	char	*
AuthMakeSalt(void)
{
	struct	timeval	tv;
	static	char	result[40];

	result[0] = '\0';
	strcpy(result, "$1$");  /* magic for the new MD5 crypt() */

	/*
	 * Generate 8 chars of salt, the old crypt() will use only first 2.
	 */
	gettimeofday(&tv, (struct timezone *) 0);
	strcat(result, l64a(tv.tv_usec));
	strcat(result, l64a(tv.tv_sec + getpid() + clock()));

	if (strlen(result) > 3 + 8)  /* magic+salt */
		result[11] = '\0';

	return result;
}

extern	void
AuthClearEntry(void)
{
	int		i;

	if		(  PasswdTable  !=  NULL  ) {
		g_hash_table_destroy(PasswdTable);
		for	( i = 0 ; PasswdPool[i] != NULL ; i ++ ) {
			xfree(PasswdPool[i]);
		}
		xfree(PasswdPool);
	}
	PasswdTable = NewNameHash();
	cPass = 0;
	PasswdPool = (PassWord **)xmalloc(sizeof(PassWord *));
	PasswdPool[0] = NULL;
}

extern	void
AuthAddEntry(
	PassWord	*pw)
{
	PassWord	**pp;

	g_hash_table_insert(PasswdTable,pw->name,pw);
	pp = (PassWord **)xmalloc(sizeof(PassWord *) * ( cPass + 2 ));
	if		(  cPass  >  0  ) {
		memcpy(pp,PasswdPool,sizeof(PassWord *) * cPass);
	}
	xfree(PasswdPool);
	PasswdPool = pp;
	PasswdPool[cPass] = pw;
	PasswdPool[cPass+1] = NULL;
	cPass ++;
}

extern	void
AuthAddUser(
	char	*name,
	char	*pass,
	int		gid,
	int		uid,
	char	*other)
{
	PassWord	*pw;
	Bool	fNew;

dbgmsg(">AuthAddUser");
	if		(  ( pw = g_hash_table_lookup(PasswdTable,name) )  ==  NULL  ) {
		pw = New(PassWord);
		fNew = TRUE;
	} else {
		fNew = FALSE;
	}
	strncpy(pw->name,name,SIZE_USER);
	strcpy(pw->pass,pass);
	pw->gid = gid;
	pw->uid = uid;
	strncpy(pw->other,other,SIZE_OTHER);
	if		(  fNew  ) {
		AuthAddEntry(pw);
	}
dbgmsg("<AuthAddUser");
}

extern	void
AuthDelUser(
	char	*name)
{
	PassWord	*pw;

	if		(  ( pw = g_hash_table_lookup(PasswdTable,name) )  !=  NULL  ) {
		g_hash_table_remove(PasswdTable,name);
		*pw->name = 0;
	}
}

extern	PassWord	*
AuthAuthUser(
	char	*name,
	char	*pass)
{
	PassWord	*pw;

	if		(  ( pw = g_hash_table_lookup(PasswdTable,name) )  !=  NULL  ) {
		if		(  strcmp(pw->pass, crypt(pass,pw->pass))  !=  0  ) {
			pw = NULL;
		}
	}
	return	(pw);
}

extern	PassWord	*
AuthGetUser(
	char	*name)
{
	return	(g_hash_table_lookup(PasswdTable,name));
}

static Bool
scan_passwd_entry(const char *buf, PassWord *pw)
{
    int fields;
    static char *format = NULL;

    if (format == NULL){
        asprintf(&format, "%%%d[^:]:%%%d[^:]:%%d:%%d:%%%dc",
                 SIZE_USER, SIZE_PASS, SIZE_OTHER);
        if (format == NULL){
            Warning("can not make passwd format string");
            return FALSE;
        }
    }
    memset(pw, 0, sizeof(PassWord));
    fields = sscanf(buf, format,
        pw->name, pw->pass, &(pw->uid), &(pw->gid), pw->other);
    if (fields < 4) return FALSE;

    return TRUE;
}

extern	void
AuthLoadPasswd(char *fname)
{
	char        buff[SIZE_BUFF];
	FILE        *fp;
	PassWord    *pw;
	char        *p;
	int         line = 1;

    AuthClearEntry();
    if ((fp = fopen(fname,"r")) == NULL){
        Warning("[%s] can not open password file", fname);
        return;
    }
    while (fgets(buff, SIZE_BUFF, fp) != NULL){
        if ((p = strchr(buff, '\n')) == NULL){
            Warning("[%s:%d] input line is too long", fname, line);
            break;
        }
        *p = 0;
		pw = New(PassWord);
        if (scan_passwd_entry(buff, pw) != TRUE){
            Warning("[%s:%d] invalid format", fname, line);
            xfree(pw);
            break;
        }
        AuthAddEntry(pw);
        line++;
    }
    fclose(fp);
}

#define SESSION_START_DEF ""								\
"session_start {\n"											\
"	arguments {\n"								        	\
"		user				varchar(" SIZE_USER_STR ");\n"	\
"		password			varchar(256);\n"				\
"		session_type		varchar(256);\n"				\
"		reserve1			varchar(256);\n"				\
"		reserve2			varchar(256);\n"				\
"		reserve3			varchar(256);\n"				\
"	};\n"													\
"	http_method				varchar(8);\n"					\
"	http_status				int;\n"							\
"	content_type			varchar(64);\n"					\
"	body					object;\n"						\
"};"

static RecordStruct *rec = NULL;

extern	Bool
AuthAPI(
	char	*user,
	char	*password,
	char	*type,
	char	*id)
{
	MonAPIData *data;
	Bool rc = FALSE;
	ValueStruct *v;

	if (rec == NULL) {
		RecParserInit();
		rec = ParseRecordMem(SESSION_START_DEF);
	}

	if (user == NULL || strlen(user) <= 0 || password == NULL) {
		return rc;
	}

	data = NewMonAPIData();
	strncpy(data->ld,     "session",sizeof(data->ld));
	strncpy(data->window, "session_start",sizeof(data->ld));
	strncpy(data->user,   "auth", sizeof(data->user));
	strncpy(data->host,   "", sizeof(data->host));
	data->value = rec->value;
	v = rec->value;
	InitializeValue(v);
	ValueInteger(GetItemLongName(v,"http_status")) = 200;
	SetValueString(GetItemLongName(v,"http_method"), "GET",NULL);
	SetValueString(GetItemLongName(v,"arguments.user"),user,NULL);
	SetValueString(GetItemLongName(v,"arguments.password"),password,NULL);
	SetValueString(GetItemLongName(v,"arguments.session_type"),type,NULL);
	switch(CallMonAPI(data)) {
	case WFC_API_OK:
		if (ValueInteger(GetItemLongName(v,"http_status")) == 200) {
			rc = TRUE;
		}
		break;
	case WFC_API_NOT_FOUND:
		break;
	default:
		break;
	}
	FreeMonAPIData(data);
    return rc;
}

extern	Bool
AuthSingle(
	char	*fname,
	char	*name,
	char	*pass,
	char	*other)
{
	char	buff[SIZE_BUFF];
	FILE	*fp;
	char	*p;
	Bool	rc = FALSE;
	int		line = 1;
	PassWord	pw;

    if ((fp = fopen(fname,"r")) == NULL){
        Warning("[%s] can not open password file: %s", fname,strerror(errno));
        return FALSE;
    }

    while (fgets(buff, SIZE_BUFF, fp) != NULL){
        if ((p = strchr(buff, '\n')) == NULL){
            Warning("[%s:%d] input line is too long", fname, line);
            break;
        }
        *p = 0;
        if (scan_passwd_entry(buff, &pw) != TRUE){
            Warning("[%s:%d] invalid format", fname, line);
            break;
        }
        line++;
		if (strcmp(pw.name, name) != 0) continue;
        if (strcmp(pw.pass, crypt(pass, pw.pass)) != 0) continue;
#if 0
		/* 'other' area is used for session type  */
		if (other != NULL) strcpy(other, pw.other);
#endif
        rc = TRUE;
        break;
    }
    fclose(fp);

    return rc;
}

extern	void
AuthSavePasswd(
	char	*fname)
{
	int		i;
	PassWord	*pw;
	FILE	*fp;
	struct stat	st;
	int 	ret;

	ret = stat(fname, &st);
	if		(  ( fp = fopen(fname,"w") )  ==  NULL  ) {
		Error("can not open password file");
		return;
	}
	for	( i = 0 ; PasswdPool[i]  !=  NULL ; i ++ ) {
		pw = PasswdPool[i];
		if		(  *pw->name  !=  0  ) {
			fprintf(fp,"%s:%s:%d:%d:%s\n",
					pw->name,pw->pass,pw->uid,pw->gid,pw->other);
		}
	}
	if		(  ret  ) {
		printf("a new password file was made (%s)\n", fname);
	}
	fclose(fp);
}

static	int		MaxUID;
static	void
CheckMax(
	char	*name,
	PassWord	*pwd,
	void	*data)
{
	if		(  pwd->uid  >  MaxUID  ) {
		MaxUID = pwd->uid;
	}
}

extern	int
AuthMaxUID(void)
{
	MaxUID = 0;

	g_hash_table_foreach(PasswdTable,(GHFunc)CheckMax,NULL);
	return	(MaxUID);
}
