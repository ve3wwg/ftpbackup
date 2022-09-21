/* ftpio.c
 *
 * Under GPL2 license : See file COPYING
 *
 * Originally created on Fri Apr 26 1996 by Warren W. Gay VE3WWG
 */
#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "ftplib.h"

short ftpDebug = 0;			/* TRUE if debug to stderr */
short ftpDontLoop = 0;			/* for internal use */

/*
 * Internal read() handling EINTR.
 */
static int
Read(int fd,char *buf,size_t max) {
	int n;

	while ( (n = read(fd,buf,max)) < 0 && errno == EINTR )
		;
	return n;
}

/*
 * Internal write(), handling EINTR.
 */
static int
Write(int s,char *buf,size_t n) {
	int rc;

	while ( (rc = write(s,buf,n)) < 0 && errno == EINTR )
		;
	return rc;
}

/*
 * Internal ftp GetLine() : Note that this has the limitation that
 * it can only support one outstanding ftp socket per process. To
 * expand on that would require separate buffers for each socket
 * that is open.
 */
static int
GetLine(int s) {
	char *cp = ftpBuf;
	int maxbuf = sizeof ftpBuf;
	static int  in = 0;
	static char ibuf[1024], *ip = ibuf;
	
	while ( (cp - ftpBuf) < maxbuf - 1 ) {	
		if ( in > 0 ) {
			--in;
			/*
			 * Discard blinken carriage returns.
			 */
			if ( *ip == '\r' ) {
				++ip;		/* Skip '\r' */
				continue;
			}
			/*
			 * New line marks the end of a line.
			 */
			if ( *ip == '\n' ) {
				++ip;
				*cp = 0;
				return cp - ftpBuf;
			}
			*cp++ = *ip++;
			continue;
		}
		/*
		 * Read more raw data.
		 */
		if ( (in = Read(s,ip=ibuf,sizeof ibuf)) < 0 )
			return -1;
	}

	/*
	 * Return bytes read in this line.
	 */
	return (int) (cp - ftpBuf);
}

/*
 * Internal routine to write one line to ftp, complete with CRLF.
 */
static int
PutLine(int s,char *buf) {

	if ( ftpDebug ) {
		fputs(buf,stderr);
		fputc('\n',stderr);
	}
	strcat(buf,"\r\n");		/* Note this minor mod to buf! */
	return Write(s,buf,strlen(buf)) > 0 ? 0 : -1;
}

/*
 * Internal routine to get ftp line, with response code.
 */
static int
GetFtpLine(int s) {
	int rc;
	int rcode;
	
	if ( (rc = GetLine(s)) <= 0 )
		return -1;

	if ( sscanf(ftpBuf,"%u",&rcode) != 1 )
		rcode = -1;

	return rcode;
}

/*
 * FUNCTION:		ftpio_GetResp
 *
 * SHORT DESCRIPTION:	Get a ftp response, and return success/failed status
 *
 * INPUT:
 *
 *	s		open ftp socket
 *
 * OUTPUT:		-2 critical error, -1 failed, 0 success.
 */
int
ftpio_GetResp(int s) {
	int rcode;
	int hdig;
	int positive = 0;

	do	{
		if ( (rcode = GetFtpLine(s)) < 0 )
			return -2;
		if ( ftpDebug ) {
			fputs(ftpBuf,stderr);
			fputc('\n',stderr);
		}
		switch ( (hdig = rcode / 100) ) {
		case 1 :
		case 2 :
		case 3 :
			positive |= 1;
			break;
		}
	} while ( (!ftpDontLoop && hdig == 1) || ftpBuf[3] == '-' );

	ftpDontLoop = 0;	/* Reset */

	return positive ? 0 : -1;
}

/*
 * FUNCTION:		ftpio_PutCmdf
 *
 * SHORT DESCRIPTION:	Put ftp command to ftp server, printf style
 *
 * INPUT:
 *
 *	s		open ftp server socket
 *	format		printf styled format string
 *	...		printf styled arguments if any
 *
 * OUTPUT:		-2 critical error, -1 failed, 0 success
 */
int
ftpio_PutCmdf(int s,char *format,...) {
	int rc;
	va_list ap;
	static char buf[1024];

	va_start(ap,format);
	vsprintf(buf,format,ap);
	va_end(ap);
	if ( (rc = PutLine(s,buf)) < 0 )
		return rc;
	return ftpio_GetResp(s);
}

// End ftpio.c
