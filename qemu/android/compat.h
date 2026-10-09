/*
 * Ergänzungen für Android (bionic), die QEMU erwartet.
 * Wird beim Kompilieren von QEMU in jede Datei eingebunden (-include).
 */
#ifndef HAKONIWA_ANDROID_COMPAT_H
#define HAKONIWA_ANDROID_COMPAT_H

#ifdef __ANDROID__
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* POSIX shared memory gibt es in bionic nicht (Ersatz in compat.c). */
int shm_open(const char *name, int oflag, mode_t mode);
int shm_unlink(const char *name);

#ifdef __cplusplus
}
#endif
#endif /* __ANDROID__ */

#endif /* HAKONIWA_ANDROID_COMPAT_H */
