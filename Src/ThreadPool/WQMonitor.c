#include "ThreadPool/WQMonitor.h"

TnStatus WQMonitorInit(WQMonitor* wqm, size_t capacity) {
  assert(wqm);
  TnStatus status;
  int res;

  status = WorkerQueueInit(&wqm->Workers, capacity);
  if (!TnStatusOk(status)) return status;

  res = pthread_mutex_init(&wqm->Mutex, NULL);

  if (res != 0) {
    errno = res;
    WorkerQueueDestroy(&wqm->Workers);
    return TNSTATUS(TN_ERRNO);
  }

  res = pthread_cond_init(&wqm->CondFull, NULL);

  if (res != 0) {
    errno = res;
    WorkerQueueDestroy(&wqm->Workers);
    pthread_mutex_destroy(&wqm->Mutex);
    return TNSTATUS(TN_ERRNO);
  }

  wqm->HasError = 0;

  return TN_OK;
}

TnStatus WQMonitorDestroy(WQMonitor* wqm) {
  TnStatus status;
  int res;

  assert(wqm);

  WorkerQueueDestroy(&wqm->Workers);
  pthread_mutex_destroy(&wqm->Mutex);
  pthread_cond_destroy(&wqm->CondFull);

  return TN_OK;
}

static void WQMonitorLock(WQMonitor* wqm) {
  assert(wqm);
  pthread_mutex_lock(&wqm->Mutex);
}

static void WQMonitorUnlock(WQMonitor* wqm) {
  assert(wqm);
  pthread_mutex_unlock(&wqm->Mutex);
}

TnStatus WQMonitorAddWorker(WQMonitor* wqm, const WorkerID* id) {
  TnStatus status;
  assert(wqm);
  assert(id);

  WQMonitorLock(wqm);
  status = WorkerQueuePush(&wqm->Workers, id);

  if (wqm->Workers.Size == wqm->Workers.Capacity)
    pthread_cond_signal(&wqm->CondFull);
  WQMonitorUnlock(wqm);

  return status;
}

TnStatus WQMonitorGetWorker(WQMonitor* wqm, WorkerID* id) {
  TnStatus status;
  assert(wqm);
  assert(id);

  WQMonitorLock(wqm);
  status = WorkerQueuePop(&wqm->Workers, id);
  WQMonitorUnlock(wqm);

  return status;
}

TnStatus WQMonitorWaitFull(WQMonitor* wqm) {
  assert(wqm);
  TnStatus status;

  WQMonitorLock(wqm);
  while (!wqm->HasError && wqm->Workers.Size != wqm->Workers.Capacity)
    pthread_cond_wait(&wqm->CondFull, &wqm->Mutex);
  WQMonitorUnlock(wqm);

  return TN_OK;
}

TnStatus WQMonitorSignalError(WQMonitor* wqm) {
  assert(wqm);

  WQMonitorLock(wqm);
  wqm->HasError = 1;
  pthread_cond_signal(&wqm->CondFull);
  WQMonitorUnlock(wqm);

  return TN_OK;
}
