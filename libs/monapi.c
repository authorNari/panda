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

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<glib.h>
#include	"enum.h"
#include	"libmondai.h"
#include	"comm.h"
#include	"comms.h"
#include	"wfcdata.h"
#include	"wfcio.h"
#include	"socket.h"
#define		_MONAPI
#include	"monapi.h"
#include	"debug.h"

//FIXME ; move another header
#define		TermPort "/tmp/wfc.term"

extern	MonAPIData	*
NewMonAPIData(void)
{
	MonAPIData *data;

ENTER_FUNC;
	data = New(MonAPIData);
	memclear(data->ld,sizeof(data->ld));
	memclear(data->window,sizeof(data->window));
	memclear(data->user,sizeof(data->user));
	memclear(data->host,sizeof(data->host));
	data->value = NULL;
LEAVE_FUNC;
	return	(data); 
}

extern void
FreeMonAPIData(
	MonAPIData *data)
{
ENTER_FUNC;
	xfree(data);
LEAVE_FUNC;
}

extern PacketClass
CallMonAPI(
	MonAPIData *data)
{
	int fd;
	Port *port;
	NETFILE *fp;
	PacketClass status;
	LargeByteString *buff;

ENTER_FUNC;
	status = WFC_API_ERROR;
	port = ParPort(TermPort, PORT_WFC);
	fd = ConnectSocket(port,SOCK_STREAM);
	DestroyPort(port);
	if (fd > 0) {
		fp = SocketToNet(fd);
		SendPacketClass(fp,WFC_API);		ON_IO_ERROR(fp,badio);
		SendString(fp, data->ld);			ON_IO_ERROR(fp,badio);
		SendString(fp, data->window);		ON_IO_ERROR(fp,badio);
		SendString(fp, data->user);			ON_IO_ERROR(fp,badio);
		SendString(fp, data->host);			ON_IO_ERROR(fp,badio);
		buff = NewLBS();
		LBS_ReserveSize(buff,NativeSizeValue(NULL,data->value),FALSE);
		NativePackValue(NULL,LBS_Body(buff),data->value);
		SendLBS(fp, buff);					ON_IO_ERROR(fp,badio);
		
		status = RecvPacketClass(fp);		ON_IO_ERROR(fp,badio);
		if (status == WFC_API_OK) {
			RecvLBS(fp, buff);		ON_IO_ERROR(fp,badio);
			NativeUnPackValue(NULL,LBS_Body(buff),data->value);
		}
		CloseNet(fp);
	} else {
		badio:
		Message("can not connect wfc server");
	}
	return status;
LEAVE_FUNC;
}
