# contrib/bcc

BCC prototypes for observing RED (request rate, errors, and duration) data for `getaddrinfo`/`gethostbyname[2]` calls.

These tools are extended versions of `gethostlatency`, originally imported from [iovisor/bcc@3297b35](https://github.com/iovisor/bcc/commit/3297b35).

## Contents

- [getaddrinfo_red](getaddrinfo_red.py): Time and inspect getaddrinfo calls. [Examples](getaddrinfo_red_example.txt).
- [gethostbyname_red](gethostbyname_red.py): Time and inspect gethostbyname[2] calls. [Examples](gethostbyname_red_example.txt).
