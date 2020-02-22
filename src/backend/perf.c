#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <unistd.h>
#include <malloc.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <poll.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>   /* For SYS_xxx definitions */
#include <sys/sysinfo.h>
#include <linux/perf_event.h>

#include "perf-internal.h"
#include "../memstrack.h"

int perf_event_ring_num;
struct pollfd *perf_fds;
struct PerfEventRing *perf_event_rings;

int perf_handling_init() {
	int err;
	int count = 0;
	int cpu_num = perf_get_cpu_num();
	const struct perf_event_table_entry *entry;

	err = perf_load_events();
	if (err) {
		return err;
	}

	for (int i = 0; i < perf_event_entry_number; i++) {
		if (perf_event_table[i].is_enabled()) {
			perf_event_ring_num += cpu_num;
		}
	}

	perf_event_rings = (struct PerfEventRing*)malloc(perf_event_ring_num * sizeof(struct PerfEventRing));

	for (int cpu = 0; cpu < cpu_num; cpu++) {
		for (int i = 0; i < perf_event_entry_number; i++){
			entry = &perf_event_table[i];
			if (!entry->is_enabled()) {
				continue;
			}
			perf_event_rings[count].event = entry->event;
			perf_event_rings[count].cpu = cpu;
			perf_event_rings[count].sample_handler = entry->handler;
			count++;
		}
	}

	for (int i = 0; i < perf_event_ring_num; i++) {
		err = perf_ring_setup(perf_event_rings + i);
		if (err) {
			return err;
		}
	}

	return 0;
}

int perf_apply_fds(struct pollfd *fds) {
	perf_fds = fds;
	for (int i = 0; i < perf_event_ring_num; i++) {
		perf_fds[i].fd = perf_event_rings[i].fd;
		perf_fds[i].events = POLLIN;
	}
	return 0;
}

int perf_handling_clean() {
	int i;
	for (i = 0; i < perf_event_ring_num; i++) {
		perf_ring_clean(perf_event_rings + i);
	}
	return 0;
}

int perf_handling_start() {
	int err;
	for (int i = 0; i < perf_event_ring_num; i++) {
		err = perf_ring_start_sampling(perf_event_rings + i);
		if (err) {
			log_error("Failed starting perf event sampling: %s!\n", strerror(err));
		}
	}
	return err;
}

int perf_handling_process() {
	int err = 0, i;
	for (i = 0; i < perf_event_ring_num; i++) {
		err = perf_ring_process(perf_event_rings + i);
		if (err) {
			return err;
		}
	}
	return err;
}
