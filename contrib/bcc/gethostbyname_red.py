#!/usr/bin/env python
#
# gethostbyname_red  Time and inspect gethostbyname[2] calls.
#                    For Linux, uses BCC, eBPF. Embedded C.
#
# This can be useful for identifying DNS latency, by identifying which
# remote host name lookups were slow, and by how much.
#
# This uses dynamic tracing of user-level functions and registers, and may
# need modifications to match your software and processor architecture.
#
# Copyright 2016 Netflix, Inc.
# Licensed under the Apache License, Version 2.0 (the "License")
#
# 28-Jan-2016    Brendan Gregg   Created this.
# 30-Mar-2016   Allan McAleavy updated for BPF_PERF_OUTPUT
# 15-Sep-2026    Matt Cover      Extended this.

from __future__ import print_function
from bcc import BPF
from time import strftime
import argparse

examples = """examples:
    ./gethostbyname_red          # time and inspect gethostbyname[2] calls
    ./gethostbyname_red -p 181   # only trace PID 181
"""
parser = argparse.ArgumentParser(
    description="Time and inspect gethostbyname[2] calls",
    formatter_class=argparse.RawDescriptionHelpFormatter,
    epilog=examples)
parser.add_argument("-p", "--pid", help="trace this PID only", type=int,
    default=-1)
parser.add_argument("--ebpf", action="store_true",
    help=argparse.SUPPRESS)
args = parser.parse_args()

# load BPF program
bpf_text = """
#include <uapi/linux/ptrace.h>
#include <linux/sched.h>

struct val_t {
    u32 pid;
    char comm[TASK_COMM_LEN];
    char host[80];
    u64 ts;
    int *h_errnop;
};

struct data_t {
    u32 pid;
    u64 delta;
    char comm[TASK_COMM_LEN];
    int errno;
    int h_errno;
    char host[80];
};

BPF_HASH(start, u32, struct val_t);
BPF_PERF_OUTPUT(events);

static __always_inline int _do_entry(const char *name, int *h_errnop) {
    if (!name)
        return 0;

    struct val_t val = {};
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u32 pid = pid_tgid >> 32;
    u32 tid = (u32)pid_tgid;

    if (bpf_get_current_comm(&val.comm, sizeof(val.comm)) == 0) {
        bpf_probe_read_user_str(&val.host, sizeof(val.host),
                       (void *)name);
        val.pid = pid;
        val.ts = bpf_ktime_get_ns();
        val.h_errnop = h_errnop;
        start.update(&tid, &val);
    }

    return 0;
}

int do_entry(struct pt_regs *ctx, const char *name, void *ret, char *buf, size_t buflen, void **result, int *h_errnop) {
    return _do_entry(name, h_errnop);
}

int do_entry2(struct pt_regs *ctx, const char *name, int af, void *ret, char *buf, size_t buflen, void **result) {
    int *h_errnop = NULL;
    bpf_probe_read_user(&h_errnop, sizeof(h_errnop), (void *)(PT_REGS_SP(ctx) + 8));
    return _do_entry(name, h_errnop);
}

int do_return(struct pt_regs *ctx) {
    struct val_t *valp;
    struct data_t data = {};
    u64 delta;
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u32 tid = (u32)pid_tgid;

    u64 tsp = bpf_ktime_get_ns();

    valp = start.lookup(&tid);
    if (valp == 0)
        return 0;       // missed start

    bpf_probe_read_kernel(&data.comm, sizeof(data.comm), valp->comm);
    bpf_probe_read_kernel(&data.host, sizeof(data.host), (void *)valp->host);

    bpf_probe_read_user(&data.h_errno, sizeof(data.h_errno), (void *)valp->h_errnop);

    data.pid = valp->pid;
    data.delta = tsp - valp->ts;
    data.errno = PT_REGS_RC(ctx);
    events.perf_submit(ctx, &data, sizeof(data));
    start.delete(&tid);
    return 0;
}
"""
if args.ebpf:
    print(bpf_text)
    exit()

b = BPF(text=bpf_text)
b.attach_uprobe(name="c", sym="gethostbyname_r", fn_name="do_entry",
                pid=args.pid)
b.attach_uprobe(name="c", sym="gethostbyname2_r", fn_name="do_entry2",
                pid=args.pid)
b.attach_uretprobe(name="c", sym="gethostbyname_r", fn_name="do_return",
                   pid=args.pid)
b.attach_uretprobe(name="c", sym="gethostbyname2_r", fn_name="do_return",
                   pid=args.pid)

# header
print("%-9s %-6s %-16s %10s %-5s %-7s %s" % ("TIME", "PID", "COMM", "LATms", "ERRNO", "H_ERRNO", "HOST"))

def print_event(cpu, data, size):
    event = b["events"].event(data)
    print("%-9s %-6d %-16s %10.2f %-5i %-7i %s" % (strftime("%H:%M:%S"), event.pid,
        event.comm.decode('utf-8', 'replace'), (float(event.delta) / 1000000),
        event.errno,
        event.h_errno,
        event.host.decode('utf-8', 'replace')))

# loop with callback to print_event
b["events"].open_perf_buffer(print_event)
while 1:
    try:
        b.perf_buffer_poll()
    except KeyboardInterrupt:
        exit()
