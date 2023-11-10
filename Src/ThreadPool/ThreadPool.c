#include "ThreadPool/ThreadPool.h"

static void WorkerCallback(Worker *worker, void *args) {
  assert(worker);
  assert(args);

  ThreadPool *tp = (ThreadPool *)args;
  WorkerState state = worker->State;
  TnStatus status;
  WorkerTask task;

  if (state == WORKER_READY) {
    status = TQMonitorGetTask(&tp->Tasks, &task);
    TnStatusCode code = status.Code;

    if (code == TN_SUCCESS) {
      status = WorkerAssignTaskAsync(worker, task);
      assert(TnStatusOk(status));
    } else if (code == TN_UNDERFLOW) {
      status = WQMonitorAddWorker(&tp->FreeWorkers, &worker->ID);
      assert(TnStatusOk(status));
    } else
      assert(0);
  } else if (state == WORKER_DONE) {
    status = WorkerFinishTaskAsync(worker);
    assert(TnStatusOk(status));
  }
}

TnStatus ThreadPoolInit(ThreadPool *tp, size_t nWorkers) {
  if (!tp) return TNSTATUS(TN_BAD_ARG_PTR);
  if (nWorkers == 0) return TNSTATUS(TN_BAD_ARG_VAL);

  TnStatus status;

  status = TQMonitorInit(&tp->Tasks);
  if (!TnStatusOk(status)) return status;

  status = WQMonitorInit(&tp->FreeWorkers, nWorkers);
  if (!TnStatusOk(status)) {
    TQMonitorDestroy(&tp->Tasks);
    return status;
  }

  status = WorkerArrayInit(&tp->Workers, nWorkers);
  if (!TnStatusOk(status)) {
    TQMonitorDestroy(&tp->Tasks);
    WQMonitorDestroy(&tp->FreeWorkers);
    return status;
  }

  return TN_OK;
}

TnStatus ThreadPoolRun(ThreadPool *tp) {
  if (!tp) return TNSTATUS(TN_BAD_ARG_PTR);

  WorkerCallbackT callback;
  callback.Args = tp;
  callback.Function = WorkerCallback;

  return WorkerArrayRun(&tp->Workers, callback);
}

TnStatus ThreadPoolStop(ThreadPool *tp) {
  if (!tp) return TNSTATUS(TN_BAD_ARG_PTR);

  return WorkerArrayStop(&tp->Workers);
}

TnStatus ThreadPoolDestroy(ThreadPool *tp) {
  if (!tp) return TNSTATUS(TN_BAD_ARG_PTR);

  TQMonitorDestroy(&tp->Tasks);
  WQMonitorDestroy(&tp->FreeWorkers);
  WorkerArrayDestroy(&tp->Workers);

  return TN_OK;
}

TnStatus ThreadPoolAddTask(ThreadPool *tp, WorkerTask task) {
  if (!tp) return TNSTATUS(TN_BAD_ARG_PTR);

  TnStatus status;
  WorkerID workerID;
  Worker *worker;

  status = WQMonitorGetWorker(&tp->FreeWorkers, &workerID);
  TnStatusCode code = status.Code;

  if (code == TN_SUCCESS) {  // Has free worker, assign task
    status = WorkerArrayGet(&tp->Workers, workerID, &worker);
    assert(TnStatusOk(status));
    if (!TnStatusOk(status)) return status;

    status = WorkerAssignTask(worker, task);

    if (!TnStatusOk(status)) {  // Failed to assign
      assert(TnStatusOk(WQMonitorAddWorker(&tp->FreeWorkers, &workerID)));
    }
  } else if (code == TN_UNDERFLOW) {  // No free workers, save task
    status = TQMonitorAddTask(&tp->Tasks, &task);
    if (!TnStatusOk(status)) {  // Failed to assign
      assert(TnStatusOk(WQMonitorAddWorker(&tp->FreeWorkers, &workerID)));
    }
  } else assert(0);

  return status;
}

TnStatus ThreadPoolWaitAll(ThreadPool *tp) {
  if (!tp) return TNSTATUS(TN_BAD_ARG_PTR);
  TnStatus status;

  status =
      TQMonitorWaitEmpty(&tp->Tasks);  // Wait for all the tasks to be taken
  if (!TnStatusOk(status)) return status;

  status = WQMonitorWaitFull(
      &tp->FreeWorkers);  // Wait for all the workers to finish
  if (!TnStatusOk(status)) return status;

  return status;
}