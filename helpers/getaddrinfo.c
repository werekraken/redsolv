#define _GNU_SOURCE     // for GNU basename(), strerrorname_np()
#include <arpa/inet.h>  // for INET6_ADDRSTRLEN, inet_ntop()
#include <assert.h>     // for assert()
#include <errno.h>      // for errno
#include <netdb.h>      // for addrinfo, EAI_ADDRFAMILY, EAI_AGAIN,
                        //     EAI_ALLDONE, EAI_BADFLAGS, EAI_CANCELED,
                        //     EAI_FAIL, EAI_FAMILY, EAI_IDN_ENCODE,
                        //     EAI_INPROGRESS, EAI_INTR,EAI_MEMORY, EAI_NODATA,
                        //     EAI_NONAME, EAI_NOTCANCELED, EAI_OVERFLOW,
                        //     EAI_SERVICE, EAI_SOCKTYPE, EAI_SYSTEM,
                        //     gai_strerror(), getaddrinfo(), freeaddrinfo()
#include <netinet/in.h> // for sockaddr_in, sockaddr_in6
#include <stdio.h>      // for fprintf(), NULL, printf(), puts(), snprintf(),
                        //     stderr
#include <stdlib.h>     // for EXIT_FAILURE, EXIT_SUCCESS
#include <string.h>     // for GNU basename(), strerror(), strerrorname_np()
#include <sys/prctl.h>  // for prctl(), PR_GET_NAME
#include <sys/socket.h> // for AF_INET, AF_INET6, AF_UNSPEC
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

const char* gai_strerrorname(int err) {
    switch (err) {
        case 0:               return "SUCCESS";
        case EAI_ADDRFAMILY:  return "EAI_ADDRFAMILY";
        case EAI_AGAIN:       return "EAI_AGAIN";
        case EAI_BADFLAGS:    return "EAI_BADFLAGS";
        case EAI_FAIL:        return "EAI_FAIL";
        case EAI_FAMILY:      return "EAI_FAMILY";
        case EAI_MEMORY:      return "EAI_MEMORY";
        case EAI_NODATA:      return "EAI_NODATA";
        case EAI_NONAME:      return "EAI_NONAME";
        case EAI_SERVICE:     return "EAI_SERVICE";
        case EAI_SOCKTYPE:    return "EAI_SOCKTYPE";
        case EAI_SYSTEM:      return "EAI_SYSTEM";
        case EAI_INPROGRESS:  return "EAI_INPROGRESS";
        case EAI_CANCELED:    return "EAI_CANCELED";
        case EAI_NOTCANCELED: return "EAI_NOTCANCELED";
        case EAI_ALLDONE:     return "EAI_ALLDONE";
        case EAI_INTR:        return "EAI_INTR";
        case EAI_IDN_ENCODE:  return "EAI_IDN_ENCODE";
        case EAI_OVERFLOW:    return "EAI_OVERFLOW";
        default:              return "UNKNOWN_ERROR";
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

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;

    struct addrinfo *answer = NULL;
    struct timespec start, end;
    errno = 0;
    clock_gettime(CLOCK_MONOTONIC, &start);
    int gai_errno = getaddrinfo(question, NULL, &hints, &answer);
    clock_gettime(CLOCK_MONOTONIC, &end);

    struct addrinfo *tofree = answer;
    int _errno = 0;
    enum level level = INFO;

    switch (gai_errno) {
        case 0:
            break; /* level = INFO */
        case EAI_NONAME:
        case EAI_NODATA:
            level = WARNING;
            break;
        case EAI_SYSTEM:
            _errno = errno;
            level = ERROR;
            break;
        default:
            level = ERROR;
            break;
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
         * When errno is a failure (!zero), gai_errno must be too (!zero).
         * Therefore, we can always output the trailing comma since gai_errno
         * is guaranteed be included in the output json.
         */
        assert(gai_errno);
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

    if (gai_errno) {
        printf(
            "  \"gai_errno\": {\n"
            "    \"value\": %d,\n"
            "    \"name\": \"%s\",\n"
            "    \"string\": \"%s\"\n"
            "  }\n",
            gai_errno,
            gai_strerrorname(gai_errno),
            gai_strerror(gai_errno)
        );
    } else {
        puts("  \"answer\": [");

        void *addrptr;
        char addrstr[INET6_ADDRSTRLEN];
        int i = 0;
        while(answer) {
            if (answer->ai_family == AF_INET) {
                addrptr = &((struct sockaddr_in *) answer->ai_addr)->sin_addr;
            } else if (answer->ai_family == AF_INET6) {
                addrptr =
                    &((struct sockaddr_in6 *) answer->ai_addr)->sin6_addr;
            } else {
                answer = answer->ai_next;
                continue;
            }

            inet_ntop(
                answer->ai_family,
                addrptr,
                addrstr,
                sizeof(addrstr)
            );

            if (i > 0) {
                printf(",\n");
            }
            printf("    \"%s\"", addrstr);
            answer = answer->ai_next;
            i++;
        }
        puts("\n  ]");
        freeaddrinfo(tofree);
    }
    puts("}");

    /* Exit successfully even when `getaddrinfo()` failed. */
    return EXIT_SUCCESS;
}
