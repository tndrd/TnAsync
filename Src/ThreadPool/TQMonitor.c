#include "ThreadPool/TQMonitor.h"

TnStatus TQMonitorInit(TQMonitor* tqm) {
  assert(tqm);
  TnStatus status;
  int res;

  status = TaskQueueInit(&tqm->Tasks);
  if (!TnStatusOk(status)) return status;

  res = pthread_mutex_init(&tqm->Mutex, NULL);

  if (res != 0) {
    errno = res;
    TaskQueueDestroy(&tqm->Tasks);
    return TNSTATUS(TN_ERRNO);
  }

  res = pthread_cond_init(&tqm->CondEmpty, NULL);

  if (res != 0) {
    errno = res;
    TaskQueueDestroy(&tqm->Tasks);
    pthread_mutex_destroy(&tqm->Mutex);

    return TNSTATUS(TN_ERRNO);
  }

  tqm->HasError = 0;

  return TN_OK;
}

TnStatus TQMonitorDestroy(TQMonitor* tqm) {
  assert(tqm);

  TaskQueueDestroy(&tqm->Tasks);
  pthread_mutex_destroy(&tqm->Mutex);
  pthread_cond_destroy(&tqm->CondEmpty);

  return TN_OK;
}

static void TQMonitorLock(TQMonitor* tqm) {
  assert(tqm);
  pthread_mutex_lock(&tqm->Mutex);
}

static void TQMonitorUnlock(TQMonitor* tqm) {
  assert(tqm);
  pthread_mutex_unlock(&tqm->Mutex);
}

TnStatus TQMonitorAddTask(TQMonitor* tqm, const WorkerTask* task) {
  TnStatus status;
  assert(tqm);
  assert(task);

  TQMonitorLock(tqm);
  status = TaskQueuePush(&tqm->Tasks, task);
  TQMonitorUnlock(tqm);

  return status;
}

TnStatus TQMonitorGetTask(TQMonitor* tqm, WorkerTask* task) {
  TnStatus status;
  assert(tqm);
  assert(task);

  TQMonitorLock(tqm);
  status = TaskQueuePop(&tqm->Tasks, task);

  if (tqm->Tasks.Size == 0) pthread_cond_signal(&tqm->CondEmpty);

  TQMonitorUnlock(tqm);

  return status;
}

TnStatus TQMonitorWaitEmpty(TQMonitor* tqm) {
  assert(tqm);

  TQMonitorLock(tqm);
  while (!tqm->HasError && tqm->Tasks.Size != 0)
    pthread_cond_wait(&tqm->CondEmpty, &tqm->Mutex);
  TQMonitorUnlock(tqm);

  return TN_OK;
}

TnStatus TQMonitorSignalError(TQMonitor* tqm) {
  assert(tqm);

  TQMonitorLock(tqm);
  tqm->HasError = 1;
  pthread_cond_signal(&tqm->CondEmpty);
  TQMonitorUnlock(tqm);

  return TN_OK;
}