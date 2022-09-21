/* $Id: ftplib.h,v 1.2 2009/12/02 15:28:04 ve3wwg Exp $
 *
 * Under GPL2 License : See file COPYING
 *
 * Originally created on Sat Jun  8 08:28:22 1996 by Warren W. Gay VE3WWG
 */
#ifndef _ftplib_h_
#define _ftplib_h_ 

#define VERS	"2.2"

#define FTPBUF_SIZE	1024

extern char ftpBuf[FTPBUF_SIZE];
extern short ftpDebug;
extern int cmdopt_D;
extern int cmdopt_z;

/*
 * Support I/O functions :
 */
extern int ftpio_GetResp(int s);
extern int ftpio_PutCmdf(int s,char *format,...);

/*
 * ftplib API :
 */
extern int ftpOpen(const char *host,int port);
extern int ftpUserPW(int s,const char *userid,const char *passwd);
extern int ftpType(int s,char Type);
extern int ftpQuit(int s);
extern char *ftpSyst(int s);

extern int ftpChdir(int s,const char *directory);
extern int ftpStore(int s,const char *RemoteFileName);
extern int ftpRetrieve(int s,const char *RemoteFileName);
extern int ftpList(int s,const char *What);
extern int ftpClose(int s,int s2);

#endif /* _ftplib_h_ */

/* $Source: /cvsroot/ftpbackup/code/ftplib.h,v $ */
