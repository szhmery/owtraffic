/*
 * utility.cpp
 *
 *  Created on: Apr 3, 2018
 *      Author: changqwa
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>

void SetProcessHighPriority() {
    struct sched_param param;
    int prio_min = sched_get_priority_min( SCHED_FIFO);
    if (prio_min == -1) {
        printf("sched_get_priority_min error: %d: %s\n", errno,
                strerror(errno));
        exit(-1);
    }
    printf("sched_get_priority_min = %d\n", prio_min);

    int prio_max = sched_get_priority_max( SCHED_FIFO);
    if (prio_max == -1) {
        printf("sched_get_priority_max error: %d: %s\n", errno,
                strerror(errno));
        exit(-1);
    }
    printf("sched_get_priority_max = %d\n", prio_max);

    param.sched_priority = prio_max;    // real-time priority
    int rc = sched_setscheduler(0, SCHED_FIFO, &param);
    if (rc != 0) {
        printf("sched_setscheduler error: %d: %s\n", errno, strerror(errno));
        exit(-1);
    }
    printf("set priority to SCHED_FIFO, %d\n", param.sched_priority);
}

