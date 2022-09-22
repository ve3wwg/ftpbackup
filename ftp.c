/* ftp.c
 *
 * Under GPL2 license : See file COPYING
 *
 * Fri Apr 26 1996 was the original creation date, by Warren W. Gay VE3WWG.
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <assert.h>

#include "ftplib.h"

char ftpBuf[FTPBUF_SIZE];		/* Internal ftp buffer */
extern short ftpDontLoop;		/* For internal use */

/*
 * FUNCTION:		ftpOpen
 *
 * SHORT DESCRIPTION:	Open ftp server connection
 *
 * INPUT:
 *
 *	host		ftp hostname
 *	port		ftp server port
 *
 * OUTPUT:		-1 failed, -2 critical, >= is socket fd
 *
 * FULL DESCRIPTION:
 * 
 */
int
ftpOpen(const char *host,int port) {
	int s;
	struct sockaddr_in sin;
	struct hostent *hp;

	/*
	 * Locate the Host's IP Address.
	 */
	if ( (hp = gethostbyname(host)) == NULL )
		return -errno;

	/*
	 * Obtain a socket to use :
	 */
	if ( (s = socket(AF_INET,SOCK_STREAM,0)) < 0 )
		return -errno;

	/*
	 * Setup a connect address :
	 */
	memcpy(&sin.sin_addr.s_addr,hp->h_addr,hp->h_length);
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);	

	/*
	 * Attempt Connection :
	 */
	if ( connect(s,(struct sockaddr *)&sin,sizeof sin) ) {
		int er = errno;
		close(s);
		return -er;
	}

	/*
	 * Get initial ftp server response :
	 */
	if ( ftpio_GetResp(s) < 0 ) {
		close(s);
		errno = EINVAL;
		return -errno;
	}

	/*
	 * Return connected socket :
	 */
	return s;	
}

/*
 * FUNCTION:		ftpUserPW
 *
 * SHORT DESCRIPTION:	login ftp account
 *
 * INPUT:
 *
 *	s		open ftp socket from ftpOpen
 *	userid		login userid
 *	passwd		login password
 *
 * OUTPUT:		-1 failed, -2 critical error, 0 successful
 *
 * FULL DESCRIPTION:
 * 
 */
int 
ftpUserPW(int s,const char *userid,const char *passwd) {
	int rc;

	if ( (rc = ftpio_PutCmdf(s,"USER %s",userid)) < 0 )
		return rc;
	return ftpio_PutCmdf(s,"PASS %s",passwd);
}

/*
 * FUNCTION:		ftpType
 *
 * SHORT DESCRIPTION:	Set ftp data type: BINARY/TEXT
 *
 * INPUT:
 *
 *	s		open ftp server socket
 *	Type		'I'=Binary, 'A'=Text(ASCII)
 *
 * OUTPUT:		-2 critical error, -1 failed, 0 success
 *
 * FULL DESCRIPTION:
 * 
 */
int
ftpType(int s,char Type) {

	return ftpio_PutCmdf(s,"TYPE %c",Type);
}

/*
 * FUNCTION:		ftpQuit
 *
 * SHORT DESCRIPTION:	Tell ftp server that we 'quit'
 *
 * INPUT:
 *
 *	s		open ftp server socket
 *
 * OUTPUT:		-2 critical error, -1 failed, 0 successful
 *
 * FULL DESCRIPTION:
 * 
 */
int
ftpQuit(int s) {

	return ftpio_PutCmdf(s,"QUIT");
}

/*
 * FUNCTION:		ftpSyst
 *
 * SHORT DESCRIPTION:	Solicit ftp server system response
 *
 * INPUT:
 *
 *	s		open ftp server socket
 *
 * OUTPUT:		NULL if failed, pointer if successful (to msg)
 *
 * FULL DESCRIPTION:
 * 
 */
char *
ftpSyst(int s) {

	if ( ftpio_PutCmdf(s,"SYST") <= 0 )
		return NULL;
	return ftpBuf;
}	

/*
 * Internal routine to support STOR/RETR/LIST :
 */
static int
ftpData(int s,const char *RemoteFileName,const char *cmd) {
	int s2=-1, s3=-1;
	int sverrno=2, rc=0;
	socklen_t sinlen;
	unsigned port;
	struct sockaddr_in sin;
	unsigned char *ipaddr;
	
	if ( !cmdopt_z ) {
		/*****************************************************
		 * Non-passive mode: Obtain a socket to use :
		 *****************************************************/
		if ( (s3 = socket(AF_INET,SOCK_STREAM,0)) < 0 )
			return -2;

		/*
		 * Find out what our socket's address is :
		 */
		sinlen = sizeof sin;
		if ( (rc = getsockname(s,(struct sockaddr *)&sin,&sinlen)) )
			goto errxit;

		/*
		 * Give our data socket an address and port :
		 */
		sin.sin_port = 0;	/* Any port in a storm */
		if ( (rc = bind(s3,(struct sockaddr *)&sin,sizeof sin)) < 0 )
			goto errxit;

		/*
		 * Find out what our data socket's port # is :
		 */
		sinlen = sizeof sin;
		if ( (rc = getsockname(s3,(struct sockaddr *)&sin,&sinlen)) )
			goto errxit;

		port = (unsigned) ntohs(sin.sin_port);

		/*
		 * Now issue ftp PORT command :
		 */
		ipaddr = (unsigned char *) &sin.sin_addr.s_addr;

		if ( (rc = ftpio_PutCmdf(s,"PORT %lu,%lu,%lu,%lu,%lu,%lu",
			ipaddr[0], ipaddr[1], ipaddr[2], ipaddr[3],
			(port >> 8) & 0xFF,
			port & 0xFF)) < 0 )

			goto errxit;

		/*
		 * Now make the port available :
		 */
		if ( (rc = listen(s3,1)) < 0 )
			goto errxit;

		/*
		 * Now issue STOR/RETR command :
		 */
		ftpDontLoop = 1;
		if ( (rc = ftpio_PutCmdf(s,"%s %s",cmd,RemoteFileName)) < 0 )
			goto errxit;

		/*
		 * Now accept the connection :
		 */	
		sinlen = sizeof sin;
		while ( (rc = s2 = accept(s3,(struct sockaddr *)&sin,&sinlen)) < 0 && errno == EINTR )
			;
		if ( rc < 0 )
			goto errxit;

		/*
		 * Return data socket number :
		 */
		close(s3);
		return s2;
	} else	{
		/*****************************************************
		 * Passive mode
		 *****************************************************/

		if ( (rc = ftpio_PutCmdf(s,"PASV")) < 0 )
			goto errxit;

		// 227 Entering Passive Mode (192,168,0,173,161,5)

		/*
		 * Obtain a socket to use :
		 */
		if ( (s2 = socket(AF_INET,SOCK_STREAM,0)) < 0 )
			goto errxit;

		/*
		 * Setup a connect address :
		 */
		char *cp = strchr(ftpBuf,'(');
		if ( !cp ) {
badfmt:			fprintf(stderr,"Unable to parse PASSV response '%s', cp=%p\n",ftpBuf,cp);
			fflush(stderr);
			goto errxit;
		}

		int dq[4], p[2];
		int n = sscanf(cp,"(%d,%d,%d,%d,%d,%d)",&dq[0],&dq[1],&dq[2],&dq[3],&p[0],&p[1]);
		if ( n != 6 )
			goto badfmt;

		char remhost[256];

		snprintf(remhost,sizeof remhost,"%d.%d.%d.%d",dq[0],dq[1],dq[2],dq[3]);

		sin.sin_family = AF_INET;
		inet_pton(AF_INET,remhost,(void*)&sin.sin_addr.s_addr);
		int port = p[0]*256+p[1];
		sin.sin_port = htons(port);

		/*
		 * Attempt Connection :
		 */
		if ( connect(s2,(struct sockaddr *)&sin,sizeof sin) ) {
			fprintf(stderr,"%s: connect to %s port %d\n",
				strerror(errno),remhost,port);
			fflush(stderr);
			close(s2);
			return -1;
		}

		/*
		 * Now issue STOR/RETR command :
		 */
		ftpDontLoop = 1;
		if ( (rc = ftpio_PutCmdf(s,"%s %s",cmd,RemoteFileName)) < 0 )
			goto errxit;

		/*
		 * Return data socket number :
		 */
		return s2;
	}

	/*
	 * Error Exit :
	 */
errxit:	sverrno = errno;
	if ( s3 >= 0 )
		close(s3);
	if ( s2 >= 0 )
		close(s2);
	errno = sverrno;
	return rc - 1;
}

/*
 * FUNCTION:		ftpStore
 *
 * SHORT DESCRIPTION:	Perform ftp server STOR operation.
 *
 * INPUT:
 *
 *	s		open ftp server socket
 *	RemoteFileName	Remote file name to write
 *
 * OUTPUT:		-2 critical error, -1 failed, 0 successful.
 *
 */
int
ftpStore(int s,const char *RemoteFileName) {
	return ftpData(s,RemoteFileName,"STOR");
}

/*
 * FUNCTION:		ftpRetrieve
 *
 * SHORT DESCRIPTION:	Retrieve a file from ftp server.
 *
 * INPUT:
 *
 *	s		open ftp server socket
 *	RemoteFileName	name of remote file to retrieve
 *
 * OUTPUT:		-2 critical error, -1 failed, 0 successful.
 *
 */
int
ftpRetrieve(int s,const char *RemoteFileName) {
	return ftpData(s,RemoteFileName,"RETR");
}

/*
 * FUNCTION:		ftpList
 *
 * SHORT DESCRIPTION:	Request ftp server to list current directory
 *
 * INPUT:
 *
 *	s		open ftp server socket
 *	What		Possibly wildcarded name of file(s) to list
 *
 * OUTPUT:		-2 critical error, -1 failed, 0 if successful.
 */
int
ftpList(int s,const char *What) {
	return ftpData(s,What,"LIST");
}

/*
 * FUNCTION:		ftpChdir
 *
 * SHORT DESCRIPTION:	Change ftp server current directory
 *
 * INPUT:
 *
 *	s		open ftp server socket
 *	directory	New directory to change to.
 *
 * OUTPUT:		-2 critical error, -1 failed, 0 success.
 */
int
ftpChdir(int s,const char *directory) {
	int e;

	e = ftpio_PutCmdf(s,"CWD %s",directory);
	if ( e != 0 && cmdopt_D != 0 ) {
		/* Change directory above failed, so lets try to create the directory first: */
		if ( !ftpio_PutCmdf(s,"MKD %s",directory) )		/* If we created the dir ok.. */
			e = ftpio_PutCmdf(s,"CWD %s",directory);	/* then try to CD to it now.. */
	}
	return e;
}

/*
 * FUNCTION:		ftpClose
 *
 * SHORT DESCRIPTION:	Close ftp Data connection.
 *
 * INPUT:
 *
 *	s		open ftp server sockete
 *	s2		open ftp data socket
 *
 * OUTPUT:		-2 critical error, -1 failed, 0 success.
 */
int
ftpClose(int s,int s2) {

	close(s2);
	return ftpio_GetResp(s);
}

// End ftp.c
