#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/file.h>

#define PID_DIR "/var/run"
#define MAX_PID_FILE_LENGTH 256

// Function to create pid filename
void get_pid_file(char *pid_file, const char *daemon_name) {
    snprintf(pid_file, MAX_PID_FILE_LENGTH, "%s/%s.pid", PID_DIR, daemon_name);
}

// Function to write PID file
int write_pid_file(const char *pid_file) {
    char str[32];
    int fd = open(pid_file, O_RDWR | O_CREAT, 0640);
    if (fd < 0) {
        return 1;
    }
    
    // Try to lock the file
    if (flock(fd, LOCK_EX | LOCK_NB) < 0) {
        close(fd);
        return 1;
    }
    
    // Truncate the file
    if (ftruncate(fd, 0) < 0) {
        close(fd);
        return 1;
    }
    
    sprintf(str, "%d\n", getpid());
    write(fd, str, strlen(str));
    // Keep the file open to maintain the lock
    return 0;
}

// Function to read PID from file
pid_t read_pid_file(const char *pid_file) {
    FILE *f = fopen(pid_file, "r");
    if (!f) {
        return -1;
    }
    
    pid_t pid;
    if (fscanf(f, "%d", &pid) != 1) {
        fclose(f);
        return -1;
    }
    fclose(f);
    return pid;
}

// Function to check if process is running
int is_process_running(pid_t pid) {
    return kill(pid, 0) == 0;
}

// Function to start daemon
int start_daemon(const char *daemon_name, char *argv[]) {
    char pid_file[MAX_PID_FILE_LENGTH];
    get_pid_file(pid_file, daemon_name);
    
    // Check if already running
    pid_t existing_pid = read_pid_file(pid_file);
    if (existing_pid > 0 && is_process_running(existing_pid)) {
        fprintf(stderr, "Daemon already running with PID %d\n", existing_pid);
        return 1;
    }

    // First fork
    pid_t pid = fork();
    if (pid < 0) {
        return 1;
    }
    if (pid > 0) {
        exit(0);
    }

    // Create new session
    if (setsid() < 0) {
        exit(1);
    }

    // Second fork
    pid = fork();
    if (pid < 0) {
        exit(1);
    }
    if (pid > 0) {
        exit(0);
    }

    // Change working directory and set umask
    chdir("/");
    umask(0);

    // Close file descriptors
    for (int x = sysconf(_SC_OPEN_MAX); x >= 0; x--) {
        close(x);
    }

    // Redirect standard file descriptors
    freopen("/dev/null", "r", stdin);
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);

    // Write PID file
    if (write_pid_file(pid_file) != 0) {
        syslog(LOG_ERR, "Could not create PID file");
        exit(1);
    }

    // Execute the command
    execvp(argv[0], argv);
    syslog(LOG_ERR, "Failed to execute %s: %s", argv[0], strerror(errno));
    exit(1);
}

// Function to stop daemon
int stop_daemon(const char *daemon_name) {
    char pid_file[MAX_PID_FILE_LENGTH];
    get_pid_file(pid_file, daemon_name);
    
    pid_t pid = read_pid_file(pid_file);
    if (pid < 0) {
        fprintf(stderr, "PID file not found or invalid\n");
        return 1;
    }
    
    if (!is_process_running(pid)) {
        fprintf(stderr, "Process not running\n");
        unlink(pid_file);
        return 1;
    }
    
    // Send SIGTERM
    if (kill(pid, SIGTERM) < 0) {
        fprintf(stderr, "Failed to stop process: %s\n", strerror(errno));
        return 1;
    }
    
    // Wait for process to stop
    int max_wait = 10;  // 10 seconds timeout
    while (max_wait-- > 0 && is_process_running(pid)) {
        sleep(1);
    }
    
    // If still running, send SIGKILL
    if (is_process_running(pid)) {
        kill(pid, SIGKILL);
    }
    
    unlink(pid_file);
    return 0;
}

// Function to restart daemon
int restart_daemon(const char *daemon_name, char *argv[]) {
    stop_daemon(daemon_name);
    sleep(1);  // Give it a moment to fully stop
    return start_daemon(daemon_name, argv);
}

void usage(const char *program_name) {
    fprintf(stderr, "Usage: %s <name> start|stop|restart -- command [args...]\n", program_name);
    fprintf(stderr, "Example: %s myapp start -- /usr/local/bin/myapp -c config.conf\n", program_name);
    exit(1);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        usage(argv[0]);
    }

    const char *daemon_name = argv[1];
    const char *action = argv[2];
    
    // Find the command after "--"
    int cmd_start = -1;
    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "--") == 0) {
            cmd_start = i + 1;
            break;
        }
    }

    if (strcmp(action, "start") == 0) {
        if (cmd_start == -1 || cmd_start >= argc) {
            usage(argv[0]);
        }
        return start_daemon(daemon_name, &argv[cmd_start]);
    } else if (strcmp(action, "stop") == 0) {
        return stop_daemon(daemon_name);
    } else if (strcmp(action, "restart") == 0) {
        if (cmd_start == -1 || cmd_start >= argc) {
            usage(argv[0]);
        }
        return restart_daemon(daemon_name, &argv[cmd_start]);
    } else {
        usage(argv[0]);
    }

    return 0;
}