#include "linux_win_api.h"

void lw_lock_init(LW_MUTEX *pLock)
{
#ifdef WIN32
	InitializeCriticalSection(pLock);
#else
	pthread_mutexattr_t mutex_attr;
	pthread_mutexattr_init(&mutex_attr);
	pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_RECURSIVE_NP);
	pthread_mutex_init(pLock, &mutex_attr);
#endif
}

void lw_lock(LW_MUTEX *pLock)
{
#ifdef WIN32
	EnterCriticalSection(pLock);
#else
	pthread_mutex_lock(pLock);
#endif
}

void lw_unlock(LW_MUTEX *pLock)
{
#ifdef WIN32
	LeaveCriticalSection(pLock);
#else
	pthread_mutex_unlock(pLock);
#endif
}

