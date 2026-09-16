#define _GNU_SOURCE     // for GNU basename(), strerrorname_np()
#include <arpa/inet.h>  // for INET_ADDRSTRLEN, inet_ntop()
#include <assert.h>     // for assert()
#include <errno.h>      // for errno
#include <netdb.h>      // for gethostbyname(), gethostbyname2(), h_errno,
                        //     HOST_NOT_FOUND, hostent, hstrerror(),
                        //     NETDB_INTERNAL, NETDB_SUCCESS, NO_ADDRESS,
                        //     NO_RECOVERY, TRY_AGAIN
#include <stdio.h>      // for fprintf(), NULL, printf(), puts(), snprintf(),
                        //     stderr
#include <stdlib.h>     // for EXIT_FAILURE, EXIT_SUCCESS
#include <string.h>     // for GNU basename(), strcmp(), strerror()
                        //     strerrorname_np()
#include <sys/prctl.h>  // for prctl(), PR_GET_NAME
#include <sys/socket.h> // for AF_INET
#include <sys/types.h>  // for pid_t
#include <time.h>       // for timespec, clock_gettime(), CLOCK_MONOTONIC
#include <unistd.h>     // for getpid()

#define EXACTLY_ONE_ARGUMENT 2

#define NSEC_PER_SEC 1000000000L /* Nanoseconds per second. */

/*
 * On Linux the maximum length of the name of a task *including* the null
 * terminator.
 */
#define TASK_COMM_LEN 16

enum level {
    INFO = 0,
    WARNING,
    ERROR
};

const char *level_name[] = {
    "INFO",
    "WARNING",
    "ERROR"
};

const char* hstrerrorname(int err) {
    switch (err) {
        case NETDB_INTERNAL: return "NETDB_INTERNAL";
        case NETDB_SUCCESS:  return "NETDB_SUCCESS";
        case HOST_NOT_FOUND: return "HOST_NOT_FOUND";
        case TRY_AGAIN:      return "TRY_AGAIN";
        case NO_RECOVERY:    return "NO_RECOVERY";
        case NO_ADDRESS:     return "NO_ADDRESS";
        default:             return "UNKNOWN_ERROR";
    }
}

int main(int argc, char **argv) {
    char *program_basename = basename(argv[0]);

    if (argc != EXACTLY_ONE_ARGUMENT) {
        fprintf(stderr, "usage: %s <hostname|ip4addr>\n", program_basename);
        return EXIT_FAILURE;
    }
    const char *question = argv[1];

    pid_t pid = getpid();

    char comm[TASK_COMM_LEN];
    if (prctl(PR_GET_NAME, comm) < 0) {
        snprintf(comm, sizeof(comm), "%s", "UNKNOWN_COMM");
    }

    errno = 0;
    h_errno = 0;
    struct timespec start, end;
    struct hostent *answer = NULL;

    /* `gethostbyname*()` called is based on the invocation. */
    if (strcmp(program_basename, "gethostbyname2") == 0) {
        clock_gettime(CLOCK_MONOTONIC, &start);
        answer = gethostbyname2(question, AF_INET);
        clock_gettime(CLOCK_MONOTONIC, &end);
    } else {
        clock_gettime(CLOCK_MONOTONIC, &start);
        answer = gethostbyname(question);
        clock_gettime(CLOCK_MONOTONIC, &end);
    }

    int _errno = 0;
    int _h_errno = 0;
    enum level level = INFO;

    if (!answer) {
        _errno = errno;
        _h_errno = h_errno;
        level = (_errno != 0) ? ERROR : WARNING;
    }

    double duration = (end.tv_sec - start.tv_sec)
        + (double)(end.tv_nsec - start.tv_nsec)
        / NSEC_PER_SEC;

    printf(
        "{\n"
        "  \"level\": \"%s\",\n"
        "  \"event\": \"%s\",\n"
        "  \"pid\": %d,\n"
        "  \"comm\": \"%s\",\n"
        "  \"question\": \"%s\",\n"
        "  \"duration\": %f,\n",
        level_name[level],
        program_basename,
        pid,
        comm,
        question,
        duration
    );

    if (_errno) {
        /*
         * When errno is a failure (!zero), h_errno must be too (!zero).
         * Therefore, we can always output the trailing comma since h_errno
         * is guaranteed be included in the output json.
         */
        assert(_h_errno);
        printf(
            "  \"errno\": {\n"
            "    \"value\": %d,\n"
            "    \"name\": \"%s\",\n"
            "    \"string\": \"%s\"\n"
            "  },\n",
            _errno,
            strerrorname_np(_errno),
            strerror(_errno)
        );
    }

    if (_h_errno) {
        printf(
            "  \"h_errno\": {\n"
            "    \"value\": %d,\n"
            "    \"name\": \"%s\",\n"
            "    \"string\": \"%s\"\n"
            "  }\n",
            _h_errno,
            hstrerrorname(_h_errno),
            hstrerror(_h_errno)
        );
    }

    if (answer) {
        puts("  \"answer\": [");

        /* Guarantee IPv4 to prevent legacy `RES_USE_INET6` corner-case. */
        assert(answer->h_addrtype == AF_INET);

        char addrstr[INET_ADDRSTRLEN];
        int i = 0;
        while(answer->h_addr_list[i] != NULL) {
            inet_ntop(
                answer->h_addrtype,
                answer->h_addr_list[i],
                addrstr,
                INET_ADDRSTRLEN
            );

            if (i > 0) {
                printf(",\n");
            }
            printf("    \"%s\"", addrstr);
            i++;
        }
        puts("\n  ]");
    }
    puts("}");

    /* Exit successfully even when `gethostbyname*()` failed. */
    return EXIT_SUCCESS;
}
