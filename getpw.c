/* getpw.c
 *
 * Originally created Sat Jun  8 08:42:53 1996 by Warren W. Gay VE3WWG
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <termios.h>

#include "ftplib.h"

static int ttyfd = -1;			/* tty file descriptor */
static struct termios tty0, tty1;	/* Saved TERMIO settings */

/*
 * Catch a signal, restore tty settings and bailout:
 */
static void
Bailer(int signo) {

	tcsetattr(ttyfd,TCSADRAIN,&tty0); /* Restore tty settings */
	write(ttyfd,"\r\n",2);

	exit(signo);			/* Abnormal exit */
}

/*
 * Get password for hostname. If userid is "anonymous", allow echoing
 * of input text (usually an email address):
 */
char *
getpw(const char *hostname,const char *userid) {
	int n;				/* Bytes read */
	int AnonymousFlag;		/* True if userid anonymous */
	static char pwbuf[64];		/* password buffer */

	/*
	 * Open the tty for input since fd 0 may redirected :
	 */
	if ( (ttyfd = open("/dev/tty",O_RDWR)) < 0 ) {
		fprintf(stderr,"%s: open(/dev/tty)\n",strerror(errno));
		return NULL;
	}

	AnonymousFlag = !strcmp(userid,"anonymous");

	/*
	 * Get current tty settings :
	 */
	if ( tcgetattr(ttyfd,&tty0) < 0 ) {
		fprintf(stderr,"%s: tcgetattr()\n",strerror(errno));
		close(ttyfd);
		return NULL;
	}

	signal(SIGINT,Bailer);

	/*
	 * Set up the line for no echo:
	 */
	tty1 = tty0;

	if ( !AnonymousFlag ) {
		tty1.c_lflag &= ~(ECHO | ECHOE | ECHOK);
		tty1.c_lflag |= ECHONL;
	}

	if ( tcsetattr(ttyfd,TCSADRAIN,&tty1) < 0 ) {
		fprintf(stderr,"%s: tcsetattr()\n",strerror(errno));
		close(ttyfd);
		return NULL;
	}

	/*
	 * Input the password :
	 */
	if ( hostname != NULL ) {
		write(ttyfd,hostname,strlen(hostname));
		write(ttyfd," ",1);
	}
	write(ttyfd,"Password: ",10);

	n = read(ttyfd,pwbuf,sizeof pwbuf-1);
	pwbuf[n] = 0;

	/*
	 * Restore tty settings :
	 */
	if ( tcsetattr(ttyfd,TCSADRAIN,&tty0) < 0 ) {
		fprintf(stderr,"%s: tcsetattr()\n",strerror(errno));
		close(ttyfd);
		return NULL;
	}
	close(ttyfd);

	signal(SIGINT,SIG_DFL);

	/*
	 * Remove the newline or CR if present :
	 */
	{	char *cp;

		do	{
			if ( (cp = strchr(pwbuf,'\n')) != NULL )
				*cp = 0;	/* Stomp it out */
			else if ( (cp = strrchr(pwbuf,'\r')) != NULL )
				*cp = 0;
		} while ( cp != NULL );
	}
	return pwbuf;
}

// End getpw.c
