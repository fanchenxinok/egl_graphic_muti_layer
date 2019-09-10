#ifndef __LINUX_WIN_API_H__
#define __LINUX_WIN_API_H__
// For distinguish LINUX and WINDOWS

#ifdef _WIN32
#include <windows.h>
typedef CRITICAL_SECTION LW_MUTEX;
#else
#include <pthread.h>
typedef pthread_mutex_t	LW_MUTEX;
#endif

void lw_lock_init(LW_MUTEX *pLock);
void lw_lock(LW_MUTEX *pLock);
void lw_unlock(LW_MUTEX *pLock);

#endif
