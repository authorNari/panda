/*	PANDA -- a simple transaction monitor

Copyright (C) 2001-2004 Ogochan & JMA (Japan Medical Association).

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

#define	MAIN
/*
#define	DEBUG
#define	TRACE
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif
#include	<signal.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include    <sys/types.h>
#include    <sys/socket.h>
#include    <sys/select.h>
#include	<unistd.h>
#ifdef	USE_SSL
#include	<openssl/crypto.h>
#include	<openssl/x509.h>
#include	<openssl/pem.h>
#include	<openssl/ssl.h>
#include	<openssl/err.h>
#endif

#include	"const.h"
#include	"types.h"
#include	"libmondai.h"
#include	"comms.h"
#include	"socket.h"
#include	"fdd.h"
#include	"option.h"
#include	"message.h"
#include	"debug.h"

static	char	*PortNumber;
static	int		Back;
static	char	*WorkDir;

static	void
Process(
	NETFILE	*fpComm)
{
	Bool	fOK;
	char	buff[SIZE_BUFF+1];
	char	*filename
		,	*tempname
		,	*command;
	size_t	size
		,	left;
	int		fd;
	int		ac;
	FILE	*fp;

	filename = NULL;
	command = NULL;
	size = 0;
	do {
		if		(  ( fOK = RecvStringDelim(fpComm,SIZE_BUFF,buff) )  ) {
			if		(  strlicmp(buff,"Command: ")  ==  0  ) {
				command = StrDup(strchr(buff,' ')+1);
			} else
			if		(  strlicmp(buff,"Filename: ")  ==  0  ) {
				filename = StrDup(strchr(buff,' ')+1);
			} else
			if		(  strlicmp(buff,"Size: ")  ==  0  ) {
				left = atoi(strchr(buff,' ')+1);
			}
		} else
			break;
	}	while	(  *buff  !=  0  );
	printf("size  = [%d]\n",left);
	sprintf(buff,"%s/XXXXXX",WorkDir);
	fd = mkstemp(buff);
	fp = fdopen(fd,"w");
	tempname = StrDup(buff);
	do {
		if		(  left  >  SIZE_BUFF  ) {
			size = SIZE_BUFF;
		} else {
			size = left;
		}
		size = Recv(fpComm,buff,size);		ON_IO_ERROR(fpComm,badio);
		if		(  size  >  0  ) {
			fwrite(buff,size,1,fp);
			ac = 0;
		} else {
			ac = ERROR_WRITE;
		}
		SendChar(fpComm,ac);
		left -= size;
	}	while	(  left  >  0  );
	fclose(fp);
#if	0
	printf("command = [%s]\n",command);
	printf("file  = [%s]\n",filename);
	printf("temp  = [%s]\n",tempname);
#endif
	sprintf(buff,"%s %s %s",command,tempname,filename);
	ac = system(buff);
	unlink(tempname);
	SendChar(fpComm,ac);
  badio:;
}

extern	void
ExecuteServer(void)
{
	int		pid;
	int		fd
	,		_fd;
	NETFILE	*fpComm;
#ifdef	USE_SSL
	SSL_CTX	*ctx;
#endif

ENTER_FUNC;
	signal(SIGCHLD,SIG_IGN);

	_fd = InitServerPort(PortNumber,Back);
#ifdef	USE_SSL
	ctx = NULL;
	if		(  fSsl  ) {
		if		(  ( ctx = MakeCTX(KeyFile,CertFile,CA_File,CA_Path,fVerify) )
				   ==  NULL  ) {
			fprintf(stderr,"CTX make error\n");
			exit(1);
		}
	}
#endif
	while	(TRUE)	{
		if		(  ( fd = accept(_fd,0,0) )  <  0  )	{
			printf("_fd = %d\n",_fd);
			Error("INET Domain Accept");
		}
		if		(  ( pid = fork() )  >  0  )	{	/*	parent	*/
			close(fd);
		} else
		if		(  pid  ==  0  )	{	/*	child	*/
#ifdef	USE_SSL
			if		(  fSsl  ) {
				fpComm = MakeSSL_Net(ctx,fd);
				SSL_accept(NETFILE_SSL(fpComm));
			} else {
				fpComm = SocketToNet(fd);
			}
#else
			fpComm = SocketToNet(fd);
#endif
			close(_fd);
			Process(fpComm);
			CloseNet(fpComm);
			exit(0);
		}
	}
	close(_fd);
LEAVE_FUNC;
}

extern	void
InitSystem(void)
{
dbgmsg(">InitSystem");
	InitNET();
dbgmsg("<InitSystem");
}

static	ARG_TABLE	option[] = {
	{	"port",		STRING,		TRUE,	(void*)&PortNumber,
		"�ݡ����ֹ�"	 								},
	{	"back",		INTEGER,	TRUE,	(void*)&Back,
		"��³�Ԥ����塼�ο�" 							},
	{	"workdir",	STRING,		TRUE,	(void*)&WorkDir,
		"����ե��������ǥ��쥯�ȥ�"				},
#ifdef	USE_SSL
	{	"ssl",		BOOLEAN,	TRUE,	(void*)&fSsl,
		"SSL��Ȥ�"				 						},
	{	"key",		STRING,		TRUE,	(void*)&KeyFile,
		"���ե�����̾(pem)"		 						},
	{	"cert",		STRING,		TRUE,	(void*)&CertFile,
		"������ե�����̾(pem)"	 						},
	{	"verifypeer",BOOLEAN,	TRUE,	(void*)&fVerify,
		"���饤����Ⱦ�����θ��ڤ�Ԥ�"				},
	{	"CApath",	STRING,		TRUE,	(void*)&CA_Path,
		"CA������ؤΥѥ�"								},
	{	"CAfile",	STRING,		TRUE,	(void*)&CA_File,
		"CA������ե�����"								},
#endif

	{	NULL,		0,			FALSE,	NULL,	NULL 	}
};

static	void
SetDefault(void)
{
	PortNumber = IntStrDup(PORT_FDD);
	Back = 5;
	WorkDir = "/tmp";
#ifdef	USE_SSL
	fSsl = FALSE;
	KeyFile = NULL;
	CertFile = NULL;
	fVerify = FALSE;
	CA_Path = NULL;
	CA_File = NULL;
#endif	
}

extern	int
main(
	int		argc,
	char	**argv)
{
	FILE_LIST	*fl;
	char		*name;

	SetDefault();
	fl = GetOption(option,argc,argv);
	InitMessage("fdd",NULL);

	InitSystem();
	ExecuteServer();
	return	(0);
}