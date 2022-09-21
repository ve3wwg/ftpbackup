/* ftpbackup.c
 *
 * Under GPL2 license : See file COPYING
 *
 * ftpbackup  allows  the  UNIX user to pipe stdin to a remote file
 * using  the remote  site's ftp server.  This allows you to backup
 * your  local system to a remote filename with no special software
 * on the remote end.
 * 
 * Similarly,  without  any  special  software  on  the remote end,
 * ftpbackup  allows  you to  do a restore from a remote file using
 * the  remote ftp  server  has the delivery mechanism.  The remote
 * file is retrieved and written to stdout, so that it can be piped
 * into your  local cpio process for restoring (though you need not
 * be restricted to cpio).
 * 
 * All  this  is  done without temporary files, so that even a full
 * file system  can be backed up to a remote site with lots of file
 * space.
 *
 *	Warren W. Gay VE3WWG
 *	ve3wwg@gmail.com
 */
#include <stdio.h>
#include <getopt.h>
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

extern char *getpw(const char *hostname,const char *userid);
static void usage(int rc);

#if HAVE_GETOPT_LONG
#define	LOPT_HELP	128			/* --help */
#define LOPT_VERSION	129			/* --version */
#else
#define getopt_long(argc,argv,opts,lopts,loptxp) getopt(argc,argv,opts)
#endif

int cmdopt_D = 0;				/* -D ; allow creation of directory when TRUE */
int cmdopt_z = 0;				/* -z ; use passive mode */

int
main(int argc,char **argv) {
	char *host = NULL;		/* ftp host */
	int port = 21;			/* ftp port to use */
	char *userid = NULL;		/* userid to use */
	char *passwd = NULL;		/* password to use */
	char *stor = NULL;		/* remote file to store */
	char *retr = NULL;		/* remote file to retreive */
	char *cwd  = NULL;		/* current working directory */
	int s;				/* ftp socket */
	int s2;				/* ftp data socket */
	int optch;			/* Option char */
	int n, nw;
	static char buf[1024];		/* I/O Buffer */
#if HAVE_GETOPT_LONG
	int loptx;			/* Long option index */
	static struct option lopts[] = {
		{ "help", 0, NULL, LOPT_HELP },
		{ "version", 0, NULL, LOPT_VERSION },
		{ NULL,   0, NULL, 0 },
	};
#endif

	/*
	 * Parse Command line options :
	 */
	while ( (optch = getopt_long(argc,argv,"h:P:u:p:b:r:d:xVDz",lopts,&loptx)) != -1 ) {
		switch( optch ) {

		/*
		 * --help
		 */
		default :
		case LOPT_HELP :
			usage(0);		/* Does exit() */
			break;

		/*
		 * --version or -V
		 */
		case LOPT_VERSION :
		case 'V' :
			fprintf(stderr,"\nftpbackup version %s\n\n",VERS);
			return 0;

		/*
		 * -h hostname		(ftp hostname)
		 */
		case 'h':
			if ( !*optarg || optarg == NULL )
				usage(0);
			host = optarg;
			break;

		/*
		 * -P port		(tcp/ip port)
		 */
		case 'P':
			port = atoi(optarg);
			break;

		/*
		 * -u userid		(long userid)
		 */
		case 'u':
			userid = optarg;
			break;

		/*
		 * -p password		(login password)
		 */
		case 'p':
			passwd = optarg;
			break;

		/*
		 * -x			(debug ftp messages)
		 */
		case 'x':
			ftpDebug = 1;
			break;

		/*
		 * -d directory		(change working directory)
		 */
		case 'd':
			cwd = optarg;
			break;

		/*
		 * Allow creation of directory:
		 */
		case 'D':
			cmdopt_D = 1;
			break;

		/*
		 * -b filename		(backup to filename)
		 */
		case 'b':
			stor = optarg;
			break;

		/*
		 * -r filename		(restore from filename)
		 */
		case 'r':
			retr = optarg;
			break;
		/*
		 * -z			Enter passive mode for transfer
		 */
		case 'z':
			cmdopt_z = 1;
			break;
		}
	} // while getopt

	/*
	 * Check options given :
	 */
	if ( host == NULL )
		host = "localhost";

	if ( userid == NULL )
		userid = "anonymous";

	if ( stor == NULL && retr == NULL ) {
		fputs("You must specify -b or -r, but not both.\n",stderr);
		usage(1);
	}

	if ( stor != NULL && retr != NULL ) {
		fputs("Cannot use -b and -r together.\n",stderr);
		usage(1);
	}

	/*
	 * Get password if none was given:
	 */
	if ( passwd == NULL )
		passwd = getpw(host,userid);

	/*
	 * Make ftp connection to the server :
	 */
	if ( (s = ftpOpen(host,port)) < 0 ) {
		fprintf(stderr,"Unable to connect to %s port %d\n",
			host,port);
		exit(2);
	}

	/*
	 * Userid and password :
	 */
	if ( ftpUserPW(s,userid,passwd) < 0 ) {
		fprintf(stderr,"Bad userid %s and/or password %s.\n",
			userid,passwd);
		goto quitxit;
	}

	/*
	 * Set type to binary :
	 */
	if ( ftpType(s,'I') < 0 ) {
		fputs("Cannot set data type to BINARY.\n",stderr);
		goto quitxit;
	}

	/*
	 * Change directories if required :
	 */
	if ( cwd != NULL && ftpChdir(s,cwd) < 0 ) {
		fprintf(stderr,"Cannot chdir to %s\n",cwd);
		goto errxit;
	}

	if ( stor != NULL ) {
		/*
		 * STOR data from stdin :
		 */
		if ( (s2 = ftpStore(s,stor)) < 0 ) {
			fprintf(stderr,"Unable to store remote file %s\n",stor);
			goto quitxit;
		}

		while ( (n = fread(buf,1,sizeof buf,stdin)) > 0 ) {
			if ( (nw = write(s2,buf,n)) != n ) {
				perror("write(STOR)");
				goto errxit;
			}
		}

		if ( ftpClose(s,s2) < 0 ) {
			fputs("ftp close error.\n",stderr);
			goto errxit;
		}
	} else	{
		/*
		 * Retrieve ftp file :
		 */
		if ( (s2 = ftpRetrieve(s,retr)) < 0 ) {
			fprintf(stderr,"Unable to retrieve remote file %s\n",retr);
			goto quitxit;
		}

		while ( (n = read(s2,buf,sizeof buf)) > 0 )
			write(1,buf,n);

		if ( n < 0 ) {
			perror("read(RETR)");
			goto errxit;
		}

		if ( ftpClose(s,s2) < 0 ) {
			fputs("ftp close error.\n",stderr);
			goto errxit;
		}
	}

	if ( ftpQuit(s) < 0 )
		fputs("ftp QUIT Failed.\n",stderr);
	
	close(s);
	exit(0);

errxit:	close(s);
	exit(13);

quitxit:ftpQuit(s);
	goto errxit;
}

/*
 * Give usage instructions :
 */
static void
usage(int rc) {

	fputs(
		"ftpbackup -h host [-P port] -u userid [-p pw] [-d dir] [-D] [-x] <mode>\n\n"
		"\t-h host\t\tftp host name to connect to\n"
		"\t-P port\t\tport # to use (defaults to 21)\n"
		"\t-u userid\taccount userid (defaults as anonymous)\n"
		"\t-p passwd\tpassword\n"
		"\t-z\t\tUse passive mode\n\n"
		"Other options:\n"
		"\t-x\t\tftp debug messages to stderr\n"
		"\t-d dir\t\tChange working directory to dir\n"
		"\t-D\t\tAttempt to create directory, if necessary.\n\n"
#if HAVE_GETOPT_LONG
		"\t--help\t\tLists this info and exits.\n"
		"\t--version\t(or -V) Lists version info.\n"
#else
		"\t-V\t\tLists version info.\n"
#endif
		"Mode:\n"
		"\t-b filename\tBackup to remote filename from stdin\n"
		"\t-r filename\tRestore from remote filename to stdout\n",
		stderr);
	exit(rc);
}

// End ftpbackup.c
