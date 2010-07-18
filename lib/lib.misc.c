/*
 * Miscellaneous common functions for C/C++ programs.
 *
 * $Id$
 */

#include "lib.misc.h"
#include "lib.error.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <signal.h>
#include <sys/wait.h>

#include "../etc/config.h"


/* Array indices for input/output file descriptors as used by pipe() */
#define PIPE_IN  1
#define PIPE_OUT 0

const int def_stdio_fd[3] = { STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO };

void _alert(const char *libdir, const char *msgtype, const char *description)
{
	static char none[1] = "";
	char *cmd;
	int dummy;

	if ( description==NULL ) description = none;

	cmd = allocstr("%s/alert '%s' '%s' &",libdir,msgtype,description);
	logmsg(LOG_INFO,"executing '%s'",cmd);

	// Assign return value to dummy variable to remove compiler
	// warnings. We're already trying to generate a warning; there's
	// no sense in generating another warning when this gives an error.
	dummy = system(cmd);

	free(cmd);
}

int execute(const char *cmd, char **args, int nargs, int stdio_fd[3], int err2out)
{
	pid_t pid, child_pid;
	int redirect;
	int status;
	int pipe_fd[3][2];
	char **argv;
	int i, dir;

	if ( (argv=(char **) malloc((nargs+2)*sizeof(char **)))==NULL ) return -1;

	if ( err2out ) stdio_fd[2] = FDREDIR_NONE;

	redirect = ( stdio_fd[0]!=FDREDIR_NONE ||
	             stdio_fd[1]!=FDREDIR_NONE ||
	             stdio_fd[2]!=FDREDIR_NONE );

	/* Build the complete argument list for execvp */
	argv[0] = (char *) cmd;
	for(i=0; i<nargs; i++) argv[i+1] = args[i];
	argv[nargs+1] = NULL;

	/* Open pipes for IO redirection */
	for(i=0; i<3; i++) {
		if ( stdio_fd[i]==FDREDIR_PIPE && pipe(pipe_fd[i])!=0 ) return -1;
	}

	switch ( child_pid = fork() ) {
	case -1: /* error */
		return -1;

	case  0: /* child process */
		/* Connect pipes to command stdin/stdout/stderr and close unneeded fd's */
		for(i=0; i<3; i++) {
			if ( stdio_fd[i]==FDREDIR_PIPE ) {
				/* stdin must be connected to the pipe output,
				   stdout/stderr to the pipe input: */
				dir = (i==0 ? PIPE_OUT : PIPE_IN);
				if ( dup2(pipe_fd[i][dir],def_stdio_fd[i])<0 ) return -1;
				if ( close(pipe_fd[i][dir])!=0 ) return -1;
				if ( close(pipe_fd[i][1-dir])!=0 ) return -1;
			}
			if ( stdio_fd[i]>=0 ) {
				if ( dup2(stdio_fd[i],def_stdio_fd[i])<0 ) return -1;
				if ( close(stdio_fd[i])!=0 ) return -1;
			}
		}
		/* Redirect stderr to stdout */
		if ( err2out && dup2(STDOUT_FILENO,STDERR_FILENO)<0 ) return -1;

		/* Replace child with command */
		execvp(cmd,argv);
		abort();

	default: /* parent process */

		free(argv);

		/* Set and close file descriptors */
		for(i=0; i<3; i++) {
			if ( stdio_fd[i]==FDREDIR_PIPE ) {
				/* parent process output must connect to the input of
				   the pipe to child, and vice versa for stdout/stderr: */
				dir = (i==0 ? PIPE_IN : PIPE_OUT);
				stdio_fd[i] = pipe_fd[i][dir];
				if ( close(pipe_fd[i][1-dir])!=0 ) return -1;
			}
		}

		/* Return if some IO is redirected to be able to read/write to child */
		if ( redirect ) return child_pid;

		/* Wait for the child command to finish */
		while ( (pid = wait(&status))!=-1 && pid!=child_pid );
		if ( pid!=child_pid ) return -1;

		/* Test whether command has finished abnormally */
		if ( ! WIFEXITED(status) ) {
			if ( WIFSIGNALED(status) ) return 128+WTERMSIG(status);
			if ( WIFSTOPPED (status) ) return 128+WSTOPSIG(status);
			return -2;
		}
		return WEXITSTATUS(status);
	}

	/* This should never be reached */
	return -2;
}

int exitsignalled;

void sig_handler(int sig)
{
	logmsg(LOG_DEBUG, "Signal %d received", sig);

	switch ( sig ) {
	case SIGTERM:
	case SIGHUP:
	case SIGINT:
		exitsignalled = 1;
		break;
	}
}

void initsignals()
{
	struct sigaction sa;
	sigset_t newmask, oldmask;

	exitsignalled = 0;

	/* unmask all signals */
	memset(&newmask, 0, sizeof(newmask));
	if ( sigprocmask(SIG_SETMASK, &newmask, &oldmask)!=0 ) {
		error(errno,"unmasking signals");
	}

	logmsg(LOG_DEBUG, "Installing signal handlers");

	sa.sa_handler = &sig_handler;
	sa.sa_mask = newmask;
	sa.sa_flags = 0;

	if ( sigaction(SIGTERM,&sa,NULL)!=0 ) error(errno,"installing signal handler");
	if ( sigaction(SIGHUP ,&sa,NULL)!=0 ) error(errno,"installing signal handler");
	if ( sigaction(SIGINT ,&sa,NULL)!=0 ) error(errno,"installing signal handler");
}

char *stripendline(char *str)
{
	size_t i, j;

	for(i=0, j=0; str[i]!=0; i++) {
		if ( ! (str[i]=='\n' || str[i]=='\r') ) str[j++] = str[i];
	}

	str[j] = 0;

	return str;
}
