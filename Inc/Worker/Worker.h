#pragma once

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <unistd.h>

#include "TnStatus.h"

struct WorkerImpl;
typedef size_t WorkerID;

typedef void (*WorkerFooT)(void* args, void* result);
typedef void (*WorkerCallbackFooT)(struct WorkerImpl* self, void* args);

typedef struct {
  void* Args;
  WorkerCallbackFooT Function;
} WorkerCallbackT;

typedef struct {
  WorkerFooT Function;
  void* Args;
  void* Result;
} WorkerTask;

typedef enum {
  WORKER_STARTED,
  WORKER_READY,
  WORKER_BUSY,
  WORKER_DONE,
  WORKER_STOPPED
} WorkerState;

typedef struct WorkerImpl {
  WorkerID ID;

  pthread_t Thread;
  pthread_mutex_t Mutex;
  pthread_cond_t Cond;

  WorkerCallbackT StateCallback;

  WorkerTask Task;

  int DoStart;
  int DoStop;
  int DoReset;
  size_t NTask;

  /* Main-R, Thread-W */
  WorkerState State;

} Worker;

#ifdef __cplusplus
extern "C" {
#endif

TnStatus WorkerInit(Worker* self, WorkerID id);
TnStatus WorkerDestroy(Worker* self);

TnStatus WorkerRun(Worker* self, WorkerCallbackT* stateCb);
TnStatus WorkerStop(Worker* self);

TnStatus WorkerAssignTask(Worker* self, WorkerTask task);
TnStatus WorkerWaitTask(Worker* self);
TnStatus WorkerFinishTask(Worker* self);

TnStatus WorkerAssignTaskAsync(Worker* self, WorkerTask task);
TnStatus WorkerFinishTaskAsync(Worker* self);

TnStatus WorkerGetState(Worker* self, WorkerState* state);

#ifdef __cplusplus
}
#endif

static TnStatus ValidateTask(WorkerTask task);
static void WorkerAssignToCore(Worker* self);

static void WorkerLock(Worker* self);
static void WorkerUnlock(Worker* Worker);

static void WorkerSleep(Worker* self);
static void WorkerWakeUp(Worker* self);

static void WorkerSleepUntilCond(Worker* self, int* condition);
static void WorkerSleepUntilStateChanges(Worker* self);

static void* WorkerLoop(void* selfPtr);