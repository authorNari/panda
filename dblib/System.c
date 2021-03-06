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
#include	<string.h>
#include	<ctype.h>
#include	<glib.h>
#include	<numeric.h>
#include	<netdb.h>
#include	<pthread.h>

#include	"const.h"
#include	"enum.h"
#include	"libmondai.h"
#include	"RecParser.h"
#include	"dbgroup.h"
#include	"term.h"
#include	"directory.h"
#include	"wfcdata.h"
#include	"redirect.h"
#include	"comm.h"
#include	"comms.h"
#include	"sysdata.h"
#include	"sysdbreq.h"
#include	"message.h"
#include	"debug.h"

#define	NBCONN(dbg)		(NETFILE *)((dbg)->process[PROCESS_UPDATE].conn)

static	int
_EXEC(
	DBG_Struct	*dbg,
	char		*sql,
	Bool		fRedirect,
	int			usage)
{
	return	(MCP_OK);
}

static	ValueStruct	*
_DBACCESS(
	DBG_Struct		*dbg,
	DBCOMM_CTRL		*ctrl,
	RecordStruct	*rec,
	ValueStruct		*args)
{
	ValueStruct	*ret;
ENTER_FUNC;
	ret = NULL;
	if		(  rec->type  !=  RECORD_DB  ) {
		ctrl->rc = MCP_BAD_ARG;
	} else {
		ctrl->rc = MCP_OK;
	}
LEAVE_FUNC;
	return	(ret);
}

static	ValueStruct	*
GETDATA(
	DBG_Struct		*dbg,
	DBCOMM_CTRL		*ctrl,
	RecordStruct	*rec,
	ValueStruct		*args)
{
	ValueStruct *ret;
	PacketClass rc;
ENTER_FUNC;
	ret = NULL;
	ctrl->rc = MCP_BAD_OTHER;
	if		(  rec->type  !=  RECORD_DB  ) {
		ctrl->rc = MCP_BAD_ARG;
	} else {
		rc = SYSDB_GetData(NBCONN(dbg), args);
		if (rc == SESSION_CONTROL_OK) {
			ctrl->rc = MCP_OK;
			ret = DuplicateValue(args,TRUE);
		} else {
			ctrl->rc = MCP_BAD_OTHER;
		}
	}
LEAVE_FUNC;
	return	ret;
}

static	ValueStruct	*
SETMESSAGE(
	DBG_Struct		*dbg,
	DBCOMM_CTRL		*ctrl,
	RecordStruct	*rec,
	ValueStruct		*args)
{
	ValueStruct *ret;
	PacketClass rc;
ENTER_FUNC;
	ret = NULL;
	ctrl->rc = MCP_BAD_OTHER;
	if		(  rec->type  !=  RECORD_DB  ) {
		ctrl->rc = MCP_BAD_ARG;
	} else {
		rc = SYSDB_SetMessage(NBCONN(dbg), args);
		ctrl->rc = rc == SESSION_CONTROL_OK ? MCP_OK : MCP_BAD_OTHER;
	}
LEAVE_FUNC;
	return	ret;
}

static	ValueStruct	*
SETMESSAGEALL(
	DBG_Struct		*dbg,
	DBCOMM_CTRL		*ctrl,
	RecordStruct	*rec,
	ValueStruct		*args)
{
	ValueStruct *ret;
	PacketClass rc;
ENTER_FUNC;
	ret = NULL;
	ctrl->rc = MCP_BAD_OTHER;
	if		(  rec->type  !=  RECORD_DB  ) {
		ctrl->rc = MCP_BAD_ARG;
	} else {
		rc = SYSDB_SetMessageAll(NBCONN(dbg), args);
		ctrl->rc = rc == SESSION_CONTROL_OK ? MCP_OK : MCP_BAD_OTHER;
	}
LEAVE_FUNC;
	return	ret;
}

static	int				idx = 0;
static	int				numData = 0;
static	Bool			hasData = FALSE;
static	ValueStruct *	sysdbvals = NULL;

static	ValueStruct	*
SELECTALL(
	DBG_Struct		*dbg,
	DBCOMM_CTRL		*ctrl,
	RecordStruct	*rec,
	ValueStruct		*args)
{
	ValueStruct *ret;
	PacketClass rc;
ENTER_FUNC;
	ret = NULL;
	ctrl->rc = MCP_BAD_OTHER;
	if		(  rec->type  !=  RECORD_DB  ) {
		ctrl->rc = MCP_BAD_ARG;
	} else {
		if (sysdbvals == NULL) {
			sysdbvals = RecParseValueMem(SYSDBVALS_DEF,NULL);
			InitializeValue(sysdbvals);
		}
		rc = SYSDB_GetDataAll(NBCONN(dbg),&numData, sysdbvals);
		if (rc == SESSION_CONTROL_OK) {
			hasData = TRUE;
			idx = 0;
			ctrl->rc = MCP_OK;
		} else {
			hasData = FALSE;
			ctrl->rc = MCP_BAD_OTHER;
		}
	}
LEAVE_FUNC;
	return	ret;
}

static	ValueStruct	*
FETCH(
	DBG_Struct		*dbg,
	DBCOMM_CTRL		*ctrl,
	RecordStruct	*rec,
	ValueStruct		*args)
{
	ValueStruct *ret,*v;
	char vname[256];
ENTER_FUNC;
	ret = NULL;
	ctrl->rc = MCP_BAD_OTHER;
	if (rec->type != RECORD_DB || !hasData){
		ctrl->rc = MCP_BAD_ARG;
		return ret;
	}
	if (idx >= numData) {
		ctrl->rc = MCP_EOF;
		return ret;
	}
	if (sysdbvals == NULL) {
		ctrl->rc = MCP_EOF;
		return ret;
	}
	snprintf(vname,sizeof(vname),"values[%d]",idx);
	v = GetItemLongName(sysdbvals,vname); 
	if (v != NULL) {
		ret = DuplicateValue(v,TRUE); 
	}
	idx += 1;
	ctrl->rc = MCP_OK;
LEAVE_FUNC;
	return	ret;
}

static	DB_OPS	Operations[] = {
	/*	DB operations		*/
	{	"DBOPEN",			(DB_FUNC)SYSDATA_DBOPEN },
	{	"DBDISCONNECT",		(DB_FUNC)SYSDATA_DBDISCONNECT },
	{	"DBSTART",			(DB_FUNC)SYSDATA_DBSTART },
	{	"DBCOMMIT",			(DB_FUNC)SYSDATA_DBCOMMIT },
	/*	table operations	*/
	{	"GETDATA",			GETDATA },
	{	"SETMESSAGE",		SETMESSAGE },
	{	"SETMESSAGEALL",	SETMESSAGEALL },
	{	"SELECTALL",		SELECTALL },
	{	"FETCH",			FETCH },
	{	NULL,				NULL }
};

static	DB_Primitives	Core = {
	_EXEC,
	_DBACCESS,
	NULL
};

extern	DB_Func	*
InitSystem(void)
{
	return	(EnterDB_Function("System",Operations,DB_PARSER_NULL,&Core,"/*","*/\t"));
}
