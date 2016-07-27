/* 
 * Copyright (c) 2015-2016, Gregory M. Kurtzer. All rights reserved.
 * 
 * “Singularity” Copyright (c) 2016, The Regents of the University of California,
 * through Lawrence Berkeley National Laboratory (subject to receipt of any
 * required approvals from the U.S. Dept. of Energy).  All rights reserved.
 * 
 * This software is licensed under a customized 3-clause BSD license.  Please
 * consult LICENSE file distributed with the sources of this project regarding
 * your rights to use or distribute this software.
 * 
 * NOTICE.  This Software was developed under funding from the U.S. Department of
 * Energy and the U.S. Government consequently retains certain rights. As such,
 * the U.S. Government has been granted for itself and others acting on its
 * behalf a paid-up, nonexclusive, irrevocable, worldwide license in the Software
 * to reproduce, distribute copies to the public, prepare derivative works, and
 * perform publicly and display publicly, and to permit other to do so. 
 * 
 */


#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <pwd.h>
#include <errno.h> 
#include <string.h>
#include <stdio.h>
#include <grp.h>

#include "privilege.h"
#include "config.h"
#include "file.h"
#include "util.h"
#include "message.h"


void update_uid_map(pid_t child, uid_t outside, int is_child) {
    char * map_file;
    char * map;
    ssize_t map_len;
    int fd;

    message(DEBUG, "Updating UID map.\n");
    if (asprintf(&map_file, "/proc/%i/uid_map", child) < 0) {
        message(ERROR, "Can't allocate uid map filename\n");
        ABORT(255);
    }
    if (is_child) {
        map_len = asprintf(&map, "%i 0 1\n", outside);
    } else {
        map_len = asprintf(&map, "0 %i 1\n", outside);
    }
    if (map_len < 0) {
        free(map_file);
        message(ERROR, "Can't allocate uid map\n");
        ABORT(255);
    }

    fd = open(map_file, O_RDWR);
    free(map_file);
    if (fd == -1) {
        perror("Open Mapfile");
        free(map);
        ABORT(255);
    }
    if (write(fd, map, map_len) != map_len) {
        perror("Writing Map");
        free(map);
        ABORT(255);
    }
    free(map);
    close(fd);
}


void update_gid_map(pid_t child, gid_t outside, int is_child) {
    char * setgroups_file, * map_file;
    char * map;
    ssize_t map_len;
    int fd;

    message(DEBUG, "Updating GID map.\n");
    if (asprintf(&map_file, "/proc/%i/gid_map", child) < 0) {
        message(ERROR, "Can't allocate uid map filename\n");
        ABORT(255);
    }
    if (is_child) {
        map_len = asprintf(&map, "%i 0 1\n", outside);
    } else {
        map_len = asprintf(&map, "0 %i 1\n", outside);
    }
    if (map_len < 0) {
        free(map_file);
        message(ERROR, "Can't allocate gid map\n");
        ABORT(255);
    }
    if (asprintf(&setgroups_file, "/proc/%i/setgroups", child) < 0) {
        message(ERROR, "Can't allocate setgroups filename\n");
        ABORT(255);
    }
    fd = open(setgroups_file, O_RDWR);
    free(setgroups_file);
    if (fd == -1) {
        perror("Open /proc/$$/setgroups file");
        free(map_file);
        free(map);
        ABORT(255);
    }
    if (write(fd, "deny", 4) != 4) {
        perror("Writing setgroups deny");
        free(map_file);
        free(map);
        close(fd);
    }
    close(fd);

    fd = open(map_file, O_RDWR);
    free(map_file);
    if (fd == -1) {
        perror("Open GID mapfile");
        free(map);
        exit(-1);
    }
    if (write(fd, map, map_len) != map_len) {
        perror("Writing GID map");
        free(map);
        close(fd);
        exit(-1);
    }
    free(map);
    close(fd);
}


static s_privinfo uinfo;


void priv_init(void) {
    memset(&uinfo, '\0', sizeof(uinfo));

    uinfo.uid = getuid();
    uinfo.gid = getgid();
    uinfo.gids_count = getgroups(0, NULL);

    message(DEBUG, "Called priv_init(void)\n");

    uinfo.gids = (gid_t *) malloc(sizeof(gid_t) * uinfo.gids_count);

    if ( getgroups(uinfo.gids_count, uinfo.gids) < 0 ) {
       message(ERROR, "Could not obtain current supplementary group list: %s\n", strerror(errno));
       ABORT(255);
    }

    uinfo.ready = 1;

    message(DEBUG, "Returning priv_init(void)\n");
}

int priv_userns_enabled() {
    return uinfo.userns_ready;
}

void priv_init_userns_outside() {
#ifdef SINGULARITY_USERNS
    if (!uinfo.ready) {
        message(ERROR, "Internal error: User NS initialization before general privilege initiation.\n");
        ABORT(255);
    }

    uinfo.orig_uid = uinfo.uid;
    uinfo.orig_gid = uinfo.gid;
    uinfo.orig_pid = getpid();

    int ret = unshare(CLONE_NEWUSER);
    if (ret == -1) {
        message(ERROR, "Failed to unshare namespace: %s.\n", strerror(errno));
        ABORT(255);
    }
    update_gid_map(uinfo.orig_pid, uinfo.orig_gid, 0);
    update_uid_map(uinfo.orig_pid, uinfo.orig_pid, 0);
    uinfo.uid = 0;
    uinfo.gid = 0;
    uinfo.userns_ready = 1;
#else  // SINGULARITY_USERNS
    message(ERROR, "Internal error: User NS function invoked without compiled-in support.\n");
    ABORT(255);
#endif  // SINGULARITY_USERNS
}

void priv_init_userns_inside() {
#ifdef SINGULARITY_USERNS
    if (!uinfo.userns_ready) {
        message(ERROR, "Internal error: User NS privilege data structure not initialized.\n");
        ABORT(255);
    }
    int ret = unshare(CLONE_NEWUSER);
    if (ret == -1) {
        message(ERROR, "Failed to unshare namespace: %s.\n", strerror(errno));
        ABORT(255);
    }
    update_gid_map(uinfo.orig_pid, uinfo.orig_gid, 1);
    update_uid_map(uinfo.orig_pid, uinfo.orig_pid, 1);
    uinfo.uid = uinfo.orig_uid;
    uinfo.gid = uinfo.orig_gid;
#else  // SINGULARITY_USERNS
    message(ERROR, "Internal error: User NS function invoked without compiled-in support.\n");
    ABORT(255);
#endif  // SINGULARITY_USERNS
}


void priv_escalate(void) {
    message(DEBUG, "Called priv_escalate(void)\n");

    if ( seteuid(0) < 0 ) {
        message(ERROR, "Could not escalate effective user privileges: %s\n", strerror(errno));
        ABORT(255);
    }

    if ( setegid(0) < 0 ) {
        message(ERROR, "Could not escalate effective group privileges: %s\n", strerror(errno));
        ABORT(255);
    }

    message(DEBUG, "Returning priv_escalate(void)\n");
}

void priv_drop(void) {
    message(DEBUG, "Called priv_drop(void)\n");

    if ( uinfo.ready != 1 ) {
        message(ERROR, "User info is not available\n");
        ABORT(255);
    }

    if ( geteuid() == 0 ) {
        message(DEBUG, "Dropping privileges to GID = '%d'\n", uinfo.gid);
        if ( setegid(uinfo.gid) < 0 ) {
            message(ERROR, "Could not drop effective group privileges to gid %d: %s\n", uinfo.gid, strerror(errno));
            ABORT(255);
        }

        message(DEBUG, "Dropping privileges to UID = '%d'\n", uinfo.uid);
        if ( seteuid(uinfo.uid) < 0 ) {
            message(ERROR, "Could not drop effective user privileges to uid %d: %s\n", uinfo.uid, strerror(errno));
            ABORT(255);
        }

    } else {
        message(DEBUG, "Running as root, no privileges to drop\n");
    }

    message(DEBUG, "Confirming we have correct GID\n");
    if ( getgid() != uinfo.gid ) {
        message(ERROR, "Failed to drop effective group privileges to uid %d: %s\n", uinfo.uid, strerror(errno));
        ABORT(255);
    }

    message(DEBUG, "Confirming we have correct UID\n");
    if ( getuid() != uinfo.uid ) {
        message(ERROR, "Failed to drop effective user privileges to uid %d: %s\n", uinfo.uid, strerror(errno));
        ABORT(255);
    }

    message(DEBUG, "Returning priv_drop(void)\n");
}

void priv_drop_perm(void) {
    message(DEBUG, "Called priv_drop_perm(void)\n");

    if ( uinfo.ready != 1 ) {
        message(ERROR, "User info is not available\n");
        ABORT(255);
    }

    if ( geteuid() == 0 ) {
        if ( !uinfo.userns_ready ) {
            message(DEBUG, "Resetting supplementary groups\n");
            if ( setgroups(uinfo.gids_count, uinfo.gids) < 0 ) {
                fprintf(stderr, "ABOFT: Could not reset supplementary group list: %s\n", strerror(errno));
                ABORT(255);
            }
        } else {
            message(DEBUG, "Not resetting supplementary groups as we are running in a user namespace.\n");
        }

        message(DEBUG, "Dropping real and effective privileges to GID = '%d'\n", uinfo.gid);
        if ( setregid(uinfo.gid, uinfo.gid) < 0 ) {
            message(ERROR, "Could not dump real and effective group privileges: %s\n", strerror(errno));
            ABORT(255);
        }

        message(DEBUG, "Dropping real and effective privileges to UID = '%d'\n", uinfo.uid);
        if ( setreuid(uinfo.uid, uinfo.uid) < 0 ) {
            message(ERROR, "Could not dump real and effective user privileges: %s\n", strerror(errno));
            ABORT(255);
        }

    } else {
        message(DEBUG, "Running as root, no privileges to drop\n");
    }

    message(DEBUG, "Confirming we have correct GID\n");
    if ( getgid() != uinfo.gid ) {
        message(ERROR, "Failed to drop effective group privileges to uid %d: %s\n", uinfo.uid, strerror(errno));
        ABORT(255);
    }

    message(DEBUG, "Confirming we have correct UID\n");
    if ( getuid() != uinfo.uid ) {
        message(ERROR, "Failed to drop effective user privileges to uid %d: %s\n", uinfo.uid, strerror(errno));
        ABORT(255);
    }

    message(DEBUG, "Returning priv_drop_perm(void)\n");
}

