#include "ThreadPool/ThreadPool.h"

static void WorkerCallback(Worker *worker, void *args) {
  assert(worker);
  assert(args);

  ThreadPool *tp = (ThreadPool *)args;
  WorkerState state = worker->State;
  TnStatus status;
  WorkerTask task;

  if (state == WORKER_FREE) {
    // Worker is free, try to get new task
    status = TQMonitorGetTask(&tp->Tasks, &task);
    TnStatusCode code = status.Code;

    if (code == TN_SUCCESS) {
      // Task available, assign
      status = WorkerAssignTask(worker, task);
      // assert(status == STATUS_SUCCESS);
    } else if (code == TN_UNDERFLOW) {
      // No availble tasks, wait for new ones
      status = WQMonitorAddWorker(&tp->FreeWorkers, &worker->ID);
      // assert(status == STATUS_SUCCESS);
    } else
      assert(0);
  } else if (state == WORKER_DONE) {
    // Worker has finished, so we can let it continue
    status = WorkerFinish(worker);
    // assert(status == STATUS_SUCCESS);
  } else if (state == WORKER_BUSY) {
    // Worker is starting to work on task, do nothing
  } else {
    assert(0);  // Placeholder for error handling
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
  }

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