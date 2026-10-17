#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

static void print_err(const char *msg) {
	write(STDERR_FILENO, msg, strlen(msg));
}

// Обрабатывает одну команду «число число число». Возвращает 0 при успехе,
// -1 при делении на 0, -2 при ошибке разбора.
static int process_line(char *line) {
	float result = 0.0f;
	int count = 0;

	char *p = line;
	while (*p) {
		char *end;
		errno = 0;
		float v = strtof(p, &end);
		if (end == p) {
			if (*p == ' ' || *p == '\t' || *p == '\r') {
				++p;
				continue;
			}
			return -2;
		}
		if (count == 0) {
			result = v;
		} else {
			// Проверка деления на 0 выполняется в дочернем процессе
			if (v == 0.0f)
				return -1;
			result /= v;
		}
		++count;
		p = end;
	}
	if (count == 0)
		return 0; // пустая строка — пропускаем

	char out[128];
	int len = snprintf(out, sizeof(out), "%g\n", result);
	if (write(STDOUT_FILENO, out, len) != len) {
		print_err("error: failed to write to stdout\n");
		exit(EXIT_FAILURE);
	}
	return 0;
}

int main(void) {
	char buf[4096];
	char line[8192];
	size_t llen = 0;
	ssize_t bytes;

	while ((bytes = read(STDIN_FILENO, buf, sizeof(buf))) != 0) {
		if (bytes < 0) {
			if (errno == EINTR)
				continue;
			print_err("error: failed to read from stdin\n");
			exit(EXIT_FAILURE);
		}
		for (ssize_t i = 0; i < bytes; ++i) {
			if (buf[i] != '\n') {
				if (llen < sizeof(line) - 1)
					line[llen++] = buf[i];
				continue;
			}
			line[llen] = '\0';
			llen = 0;
			int r = process_line(line);
			if (r == -1) {
				print_err("child: error: division by zero\n");
				exit(EXIT_FAILURE);
			} else if (r == -2) {
				print_err("child: error: invalid number in command\n");
				exit(EXIT_FAILURE);
			}
		}
	}
	if (llen > 0) { // последняя строка без \n
		line[llen] = '\0';
		int r = process_line(line);
		if (r != 0) {
			print_err(r == -1 ? "child: error: division by zero\n"
			                  : "child: error: invalid number in command\n");
			exit(EXIT_FAILURE);
		}
	}
	return 0;
}
