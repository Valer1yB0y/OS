#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define CHILD_PROGRAM_NAME "child"

static void print_err(const char *msg) {
	write(STDERR_FILENO, msg, strlen(msg));
}

int main(void) {
	// Определяем каталог, в котором лежит исполняемый файл родителя
	char progpath[1024];
	ssize_t plen = readlink("/proc/self/exe", progpath, sizeof(progpath) - 1);
	if (plen == -1) {
		print_err("error: failed to read full program path\n");
		exit(EXIT_FAILURE);
	}
	progpath[plen] = '\0';
	while (plen > 0 && progpath[plen] != '/')
		--plen;
	progpath[plen] = '\0';

	// Первая строка: имя файла
	const char prompt[] = "Enter file name: ";
	write(STDOUT_FILENO, prompt, sizeof(prompt) - 1);

	char fname[1024];
	size_t flen = 0;
	for (;;) {
		char c;
		ssize_t n = read(STDIN_FILENO, &c, 1);
		if (n < 0) {
			print_err("error: failed to read file name from stdin\n");
			exit(EXIT_FAILURE);
		}
		if (n == 0 || c == '\n')
			break;
		if (flen < sizeof(fname) - 1)
			fname[flen++] = c;
	}
	fname[flen] = '\0';
	if (flen == 0) {
		print_err("error: empty file name\n");
		exit(EXIT_FAILURE);
	}

	// Родитель открывает файл на чтение
	int file = open(fname, O_RDONLY);
	if (file == -1) {
		print_err("error: failed to open requested file\n");
		exit(EXIT_FAILURE);
	}

	// pipe1: child (stdout) -> parent
	int pipe1[2];
	if (pipe(pipe1) == -1) {
		print_err("error: failed to create pipe\n");
		exit(EXIT_FAILURE);
	}

	const pid_t child = fork();
	if (child == -1) {
		print_err("error: failed to spawn new process\n");
		exit(EXIT_FAILURE);
	}

	if (child == 0) {
		// Дочерний процесс: stdin <- файл, stdout -> pipe1
		close(pipe1[0]);

		if (dup2(file, STDIN_FILENO) == -1 ||
		    dup2(pipe1[1], STDOUT_FILENO) == -1) {
			print_err("error: failed to redirect standard streams\n");
			exit(EXIT_FAILURE);
		}
		close(file);
		close(pipe1[1]);

		char path[2048];
		snprintf(path, sizeof(path), "%s/%s", progpath, CHILD_PROGRAM_NAME);
		char *const args[] = {CHILD_PROGRAM_NAME, NULL};
		execv(path, args);

		print_err("error: failed to exec into new executable image\n");
		exit(EXIT_FAILURE);
	}

	// Родитель
	close(file);
	close(pipe1[1]);

	char buf[4096];
	ssize_t bytes;
	while ((bytes = read(pipe1[0], buf, sizeof(buf))) != 0) {
		if (bytes < 0) {
			if (errno == EINTR)
				continue;
			print_err("error: failed to read from pipe\n");
			exit(EXIT_FAILURE);
		}
		if (write(STDOUT_FILENO, buf, bytes) != bytes) {
			print_err("error: failed to write to stdout\n");
			exit(EXIT_FAILURE);
		}
	}
	close(pipe1[0]);

	int status = 0;
	if (waitpid(child, &status, 0) == -1) {
		print_err("error: failed to wait for child\n");
		exit(EXIT_FAILURE);
	}
	if (WIFEXITED(status) && WEXITSTATUS(status) != EXIT_SUCCESS) {
		print_err("parent: child terminated with error, exiting\n");
		exit(EXIT_FAILURE);
	}
	return 0;
}
