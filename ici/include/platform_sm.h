/*

	platform_sm.h:	more platform-dependent porting adaptations.

	Copyright (c) 2001, California Institute of Technology.
	ALL RIGHTS RESERVED.  U.S. Government Sponsorship
	acknowledged.
									*/
/*	Author: Alan Schlutsmeyer, Jet Propulsion Laboratory		*/
/*									*/
#ifndef _PLATFORM_SM_H_
#define _PLATFORM_SM_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
	int		wmKey;
	long		wmSize;
	char		*wmAddress;
	char		*wmName;
} sm_WmParms;

typedef int             sm_SemId;
#define SM_SEM_NONE	(-1)

#define SM_SEM_FIFO	0
#define SM_SEM_PRIORITY	1

#define SM_NO_KEY	-1
extern int		sm_GetUniqueKey();

#define	ICI_PRIORITY	250

/*      IPC services access control */
extern int		sm_ipc_init();
extern void		sm_ipc_stop();
extern int		sm_GetUniqueKey();

/*	Portable semaphore management routines.				*/

extern sm_SemId		sm_SemCreate(int key, int semType);
extern int		sm_SemTake(sm_SemId semId);
extern void		sm_SemGive(sm_SemId semId);
extern int		sm_SemUnwedge(sm_SemId semId, int interval);
extern void		sm_SemDelete(sm_SemId semId);
extern void		sm_SemEnd(sm_SemId semId);
extern int		sm_SemEnded(sm_SemId semId);
extern void		sm_SemUnend(sm_SemId semId);
extern sm_SemId		sm_GetTaskSemaphore(int taskId);

/*	Portable shared-memory region access routines.			*/

extern int		sm_ShmAttach(int key, int size, char **shmPtr, int *id);
extern void		sm_ShmDetach(char *shmPtr);
extern void		sm_ShmDestroy(int id);

/*	Portable task (process) management routines.			*/

extern int		sm_TaskIdSelf();
extern int		sm_TaskExists(int taskId);
extern void		sm_TaskVarAdd(int *var);
extern void		sm_TaskSuspend();
extern void		sm_TaskDelay(int seconds);
extern void		sm_TaskYield();
extern int		sm_TaskSpawn(char *name, char *arg1, char *arg2,
				char *arg3, char *arg4, char *arg5, char *arg6,
				char *arg7, char *arg8, char *arg9, char *arg10,
				int priority, int stackSize);
extern void		sm_TaskKill(int taskId, int sigNbr);
extern void		sm_TaskDelete(int taskId);
extern void		sm_Abort();
extern void		sm_ConfigurePthread(pthread_attr_t *attr,
				size_t stackSize);
extern int		pseudoshell(char *commandLine);

#ifdef __cplusplus
}
#endif

#endif  /* _PLATFORM_SM_H_ */
