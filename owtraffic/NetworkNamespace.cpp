/*
 * NetworkNamespace.cpp
 *
 *  Created on: May 1, 2018
 *      Author: changqwa
 */

#include <fcntl.h>
#include <sched.h>
#include <stdio.h>
#include <errno.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "NetworkNamespace.h"

#define PROGRAM "owtraffic"
#define NS_PATH "/var/run/netns"

static int current_ns_inode(ino_t *inode) {
    struct stat nsstat;

    if (stat("/proc/self/ns/net", &nsstat) == -1) {
        perror(PROGRAM ": stat(\"/proc/self/ns/net\")");
        return 0;
    }

    *inode = nsstat.st_ino;
    return 1;
}

/* return values:
   0: inode is not a namespace in the nspath
   1: it is
  -1: error encountered
 */
static int inode_in_nspath(ino_t inode) {
    DIR *nsdir;
    char *nspath;
    struct dirent *ns;
    struct stat nsstat;

    if ((nsdir = opendir(NS_PATH)) == NULL) {
        perror(PROGRAM ": opendir(\"" NS_PATH "\")");
        return -1;
    }

    while ((ns = readdir(nsdir)) != NULL) {
        if (strcmp(".", ns->d_name) == 0 || strcmp("..", ns->d_name) == 0)
            continue;

        if (asprintf(&nspath, "%s/%s", NS_PATH, ns->d_name) == -1) {
            perror(PROGRAM ": asprintf");
            return -1;
        }

        if (stat(nspath, &nsstat) == -1) {
            /* i hate to break consistency and use fprintf() rather than
               perror(), but it's necessary here. */
            fprintf(stderr, PROGRAM ": stat(\"%s\"): %s\n", nspath, strerror(errno));
            return -1;
        }

        free(nspath);

        if (nsstat.st_ino == inode)
            return 1;
    }

    if (errno != 0) {
        perror(PROGRAM ": readdir(\"" NS_PATH "\")");
        return -1;
    }

    closedir(nsdir);

    return 0;
}

int already_in_namespace() {
    int status;
    ino_t inode;

    if (!current_ns_inode(&inode))
        return 1;

    if ((status = inode_in_nspath(inode)) == 0) {
        return 0;
    } else {
        if (status == 1)
            fprintf(stderr, PROGRAM ": oops! i can run only in network namespaces not found in " NS_PATH "\n");

        return 1;
    }
}

static int bad_nsname(const char *ns) {
    return strlen(ns) == 0 || strcmp("..", ns) == 0 || strcmp(".", ns) == 0 || strchr(ns, '/') != NULL;
}

int set_netns(const char *ns) {
    int nsfd, perm_issue;
    char *nspath;

    if (bad_nsname(ns)) {
        fprintf(stderr, PROGRAM ": namespace names can't contain '/', be empty, or be '.' or '..'.\n");
        return 0;
    }

    if (asprintf(&nspath, "%s/%s", NS_PATH, ns) == -1) {
        perror(PROGRAM ": asprintf");
        return 0;
    }

    if ((nsfd = open(nspath, O_RDONLY | O_CLOEXEC)) == -1) {
        fprintf(stderr, PROGRAM ": open(\"%s\"): %s\n", nspath, strerror(errno));
        free(nspath);
        return 0;
    }

    free(nspath);

    if (setns(nsfd, CLONE_NEWNET) == -1) {
        perm_issue = errno == EPERM;
        perror(PROGRAM ": setns");

        if (perm_issue)
            fprintf(stderr, "\nis the " PROGRAM " binary missing the setuid bit?\n");

        return 0;
    } else {
        return 1;
    }
}

/* set euid+egid to the real uid+gid */
int deescalate() {
    if (setgid(getgid()) == -1 || setuid(getuid()) == -1) {
        perror(PROGRAM ": set[gu]id");
        return 0;
    } else {
        return 1;
    }
}
