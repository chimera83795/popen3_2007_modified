/*
 * This implementation of popen3() was created from scratch in June of 2011.  It
 * is less likely to leak file descriptors if an error occurs than the 2007
 * version and has been tested under valgrind.  It also differs from the 2007
 * version in its behavior if one of the file descriptor parameters is NULL.
 * Instead of closing the corresponding stream, it is left unmodified (typically
 * sharing the same terminal as the parent process).  It also lacks the
 * non-blocking option present in the 2007 version.
 *
 * No warranty of correctness, safety, performance, security, or usability is
 * given.  This implementation is released into the public domain/CC0, but if used
 * in an open source application, attribution would be appreciated.
 *
 * Mike Bourgeous
 * https://github.com/mike-bourgeous
 * http://www.mikebourgeous.com/
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include <stdlib.h>
#include <string.h>

/*
 * Sets the FD_CLOEXEC flag.  Returns 0 on success, -1 on error.
 */
static int set_cloexec(int fd)
{
	if(fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | FD_CLOEXEC) == -1) {
		perror("Error setting FD_CLOEXEC flag");
		return -1;
	}

	return 0;
}

/*
 * Runs command in another process, with full remote interaction capabilities.
 * Be aware that command is passed to sh -c, so shell expansion will occur.
 * Writing to *writefd will write to the command's stdin.  Reading from *readfd
 * will read from the command's stdout.  Reading from *errfd will read from the
 * command's stderr.  If NULL is passed for writefd, readfd, or errfd, then the
 * command's stdin, stdout, or stderr will not be changed.  Returns the child
 * PID on success, -1 on error.
 */
pid_t popen3(int *writefd, int *readfd, int *errfd)
{
	int in_pipe[2] = {-1, -1};
	int out_pipe[2] = {-1, -1};
	int err_pipe[2] = {-1, -1};
	pid_t pid;

	// 2011 implementation of popen3() by Mike Bourgeous
	// https://gist.github.com/1022231

	if(writefd && pipe(in_pipe)) {
		perror("Error creating pipe for stdin");
		goto error;
	}
	if(readfd && pipe(out_pipe)) {
		perror("Error creating pipe for stdout");
		goto error;
	}
	if(errfd && pipe(err_pipe)) {
		perror("Error creating pipe for stderr");
		goto error;
	}

	pid = fork();
	switch(pid) {
		case -1:
			// Error
			perror("Error creating child process");
			goto error;

		case 0:
			// Child
			if(writefd) {
				close(in_pipe[1]);
				if(dup2(in_pipe[0], 0) == -1) {
					perror("Error assigning stdin in child process");
					exit(-1);
				}
				close(in_pipe[0]);
			}
			if(readfd) {
				close(out_pipe[0]);
				if(dup2(out_pipe[1], 1) == -1) {
					perror("Error assigning stdout in child process");
					exit(-1);
				}
				close(out_pipe[1]);
			}
			if(errfd) {
				close(err_pipe[0]);
				if(dup2(err_pipe[1], 2) == -1) {
					perror("Error assigning stderr in child process");
					exit(-1);
				}
				close(err_pipe[1]);
			}
			execl("/bin/sh", "/bin/sh", (char *)NULL); //example shell
			perror("Error executing command in child process");
			exit(-1);

		default:
			// Parent
			break;
	}

	if(writefd) {
		close(in_pipe[0]);
		set_cloexec(in_pipe[1]);
		*writefd = in_pipe[1];
	}
	if(readfd) {
		close(out_pipe[1]);
		set_cloexec(out_pipe[0]);
		*readfd = out_pipe[0];
	}
	if(errfd) {
		close(err_pipe[1]);
		set_cloexec(out_pipe[0]);
		*errfd = err_pipe[0];
	}

	return pid;

error:
	if(in_pipe[0] >= 0) {
		close(in_pipe[0]);
	}
	if(in_pipe[1] >= 0) {
		close(in_pipe[1]);
	}
	if(out_pipe[0] >= 0) {
		close(out_pipe[0]);
	}
	if(out_pipe[1] >= 0) {
		close(out_pipe[1]);
	}
	if(err_pipe[0] >= 0) {
		close(err_pipe[0]);
	}
	if(err_pipe[1] >= 0) {
		close(err_pipe[1]);
	}

	return -1;
}

int main()
{
	//small poc
	char buffer[4096];
	char cmd[10] = "id\n";
	int input, output, error;
	if(popen3(&input, &output, &error)<0){
		printf("error while popen3\n");
	}
	printf("[*]created popen3 descriptors\n");
	memset(&buffer, 0, sizeof(buffer));
	printf("cmd:%s\n", cmd);
	printf("bytes_writen_to_stdin: %d\n", write(input, &cmd, sizeof(cmd)));
	printf("bytes_read_from_stdout: %d\n",read(output, &buffer, sizeof(buffer)));
	printf("output: %s\n", &buffer);

	return 0;
}
