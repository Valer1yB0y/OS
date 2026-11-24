#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/wait.h>
#define CHILD_PROGRAM_NAME "child"
static char map_path[64];
static int map_created = 0;
static volatile sig_atomic_t child_failed = 0;
static void print_err(const char *msg) {
write(STDERR_FILENO, msg, strlen(msg));
}
static void die(const char *msg) {
print_err(msg);
if (map_created) {
unlink(map_path);
}
exit(EXIT_FAILURE);
}
static void on_sigusr1(int sig) {
(void)sig;
child_failed = 1;
}
int main(void) {
char progpath[1024];
ssize_t plen = readlink("/proc/self/exe", progpath, sizeof(progpath) - 1);
if (plen == -1) {
die("error: failed to read full program path\n");
}
progpath[plen] = '\0';
while (plen > 0 && progpath[plen] != '/') {
--plen;
}
progpath[plen] = '\0';
struct sigaction sa;
memset(&sa, 0, sizeof(sa));
sa.sa_handler = on_sigusr1;
sigemptyset(&sa.sa_mask);
if (sigaction(SIGUSR1, &sa, NULL) == -1) {
die("error: failed to set signal handler\n");
}
const char prompt[] = "Enter file name: ";
if (write(STDOUT_FILENO, prompt, sizeof(prompt) - 1) == -1) {
die("error: failed to write to stdout\n");
}
char fname[1024];
size_t flen = 0;
for (;;) {
char c;
ssize_t n = read(STDIN_FILENO, &c, 1);
if (n < 0) {
die("error: failed to read file name from stdin\n");
}
if (n == 0 || c == '\n') {
break;
}
if (flen < sizeof(fname) - 1) {
fname[flen++] = c;
}
}
fname[flen] = '\0';
if (flen == 0) {
die("error: empty file name\n");
}
int file = open(fname, O_RDONLY);
if (file == -1) {
die("error: failed to open requested file\n");
}
struct stat st;
if (fstat(file, &st) == -1) {
die("error: failed to get file size\n");
}
if (st.st_size == 0) {
die("error: file is empty\n");
}
size_t size = (size_t)st.st_size;
snprintf(map_path, sizeof(map_path), "/tmp/lab3_XXXXXX");
int map = mkstemp(map_path);
if (map == -1) {
die("error: failed to create mapped file\n");
}
map_created = 1;
if (ftruncate(map, (off_t)(size + 1)) == -1) {
die("error: failed to set mapped file size\n");
}
char *data = mmap(NULL, size + 1, PROT_READ | PROT_WRITE, MAP_SHARED, map, 0);
if (data == MAP_FAILED) {
die("error: failed to map file\n");
}
close(map);
size_t got = 0;
while (got < size) {
ssize_t n = read(file, data + got, size - got);
if (n < 0) {
if (errno == EINTR) {
continue;
}
die("error: failed to read from file\n");
}
if (n == 0) {
break;
}
got += (size_t)n;
}
close(file);
const pid_t child = fork();
if (child == -1) {
die("error: failed to spawn new process\n");
}
if (child == 0) {
char path[2048];
snprintf(path, sizeof(path), "%s/%s", progpath, CHILD_PROGRAM_NAME);
char *const args[] = {CHILD_PROGRAM_NAME, map_path, NULL};
execv(path, args);
print_err("error: failed to exec into new executable image\n");
exit(EXIT_FAILURE);
}
int status = 0;
while (waitpid(child, &status, 0) == -1) {
if (errno != EINTR) {
die("error: failed to wait for child\n");
}
}
if (munmap(data, size + 1) == -1) {
die("error: failed to unmap file\n");
}
map_created = 0;
if (unlink(map_path) == -1) {
die("error: failed to remove mapped file\n");
}
if (child_failed || !WIFEXITED(status) || WEXITSTATUS(status) != EXIT_SUCCESS) {
print_err("parent: child terminated with error, exiting\n");
exit(EXIT_FAILURE);
}
return 0;
}
