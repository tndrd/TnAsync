#pragma once
#include "ThreadPool/TaskQueue.h"
#include "errno.h"
#include "pthread.h"

typedef struct {
  TaskQueue Tasks;

  pthread_mutex_t Mutex;
  pthread_cond_t CondEmpty;

  int HasError;
} TQMonitor;

#ifdef __cplusplus
extern "C" {
#endif

TnStatus TQMonitorInit(TQMonitor* tqm);
TnStatus TQMonitorDestroy(TQMonitor* tqm);
TnStatus TQMonitorAddTask(TQMonitor* tqm, const WorkerTask* task);
TnStatus TQMonitorGetTask(TQMonitor* tqm, WorkerTask* task);
TnStatus TQMonitorWaitEmpty(TQMonitor* tqm);
TnStatus TQMonitorSignalError(TQMonitor* tqm);

#ifdef __cplusplus
}
#endif

static void TQMonitorLock(TQMonitor* tqm);
static void TQMonitorUnlock(TQMonitor* tqm);
