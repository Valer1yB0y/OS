#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
static void print_err(const char *msg) {
write(STDERR_FILENO, msg, strlen(msg));
}
static void fail(const char *msg) {
print_err(msg);
pid_t ppid = getppid();
if (ppid > 1) {
kill(ppid, SIGUSR1);
}
exit(EXIT_FAILURE);
}
int main(int argc, char *argv[]) {
if (argc != 2) {
fail("child: error: mapped file name is not specified\n");
}
int map = open(argv[1], O_RDONLY);
if (map == -1) {
fail("child: error: failed to open mapped file\n");
}
struct stat st;
if (fstat(map, &st) == -1 || st.st_size < 1) {
fail("child: error: failed to get mapped file size\n");
}
size_t size = (size_t)st.st_size;
const char *data = mmap(NULL, size, PROT_READ, MAP_SHARED, map, 0);
if (data == MAP_FAILED) {
fail("child: error: failed to map file\n");
}
close(map);
const char *p = data;
while (*p) {
float result = 0.0f;
int count = 0;
for (;;) {
while (*p == ' ' || *p == '\t' || *p == '\r') {
++p;
}
if (*p == '\n' || *p == '\0') {
break;
}
char *end;
float v = strtof(p, &end);
if (end == p) {
fail("child: error: invalid number in command\n");
}
if (count == 0) {
result = v;
} else {
if (v == 0.0f) {
fail("child: error: division by zero\n");
}
result /= v;
}
++count;
p = end;
}
if (*p == '\n') {
++p;
}
if (count > 0) {
char out[128];
int len = snprintf(out, sizeof(out), "%g\n", result);
if (write(STDOUT_FILENO, out, len) != len) {
fail("child: error: failed to write to stdout\n");
}
}
}
if (munmap((void *)data, size) == -1) {
fail("child: error: failed to unmap file\n");
}
return 0;
}
