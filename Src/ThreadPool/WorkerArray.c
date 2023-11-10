#include "ThreadPool/WorkerArray.h"

TnStatus WorkerArrayInit(WorkerArray* workers, size_t size) {
  TnStatus status = TN_OK;
  assert(workers);
  assert(size);

  size_t newSize = size;
  Worker* newWorkers = (Worker*)malloc(newSize * sizeof(Worker));

  if (!newWorkers) return TNSTATUS(TN_BAD_ALLOC);

  int created = 0;
  for (; created < newSize; ++created) {
    status = WorkerInit(newWorkers + created, created);
    if (!TnStatusOk(status)) break;
  }

  if (TnStatusOk(status)) {
    workers->Workers = newWorkers;
    workers->Size = newSize;
    return status;
  }

  TnStatus oldStatus = status;

  for (int i = created - 1; i >= 0; --i) {
    status = WorkerDestroy(newWorkers + i);
  }

  return oldStatus;
}

TnStatus WorkerArrayDestroy(WorkerArray* workers) {
  TnStatus status;
  assert(workers);

  for (int i = 0; i < workers->Size; ++i) {
    WorkerDestroy(workers->Workers + i);
  }

  free(workers->Workers);

  return TN_OK;
}

TnStatus WorkerArrayGet(WorkerArray* workers, WorkerID id, Worker** workerPtr) {
  assert(workers);
  assert(workerPtr);

  if (id + 1 > workers->Size) return TNSTATUS(TN_OVERFLOW);

  *workerPtr = &workers->Workers[id];

  return TN_OK;
}

TnStatus WorkerArrayRun(WorkerArray* workers, WorkerCallbackT callback) {
  TnStatus status;
  Worker* worker;
  assert(workers);

  int i = 0;
  for (; i < workers->Size; ++i) {
    status = WorkerArrayGet(workers, i, &worker);
    assert(TnStatusOk(status));

    status = WorkerRun(worker, &callback);
    if (!TnStatusOk(status)) break;
  }

  if (TnStatusOk(status)) return status;
  TnStatus oldStatus = status;

  for (i = i - 1; i >= 0; --i) {
    status = WorkerArrayGet(workers, i, &worker);
    assert(TnStatusOk(status));

    WorkerStop(worker);
  }

  return oldStatus;
}

TnStatus WorkerArrayStop(WorkerArray* workers) {
  TnStatus status;
  Worker* worker;
  assert(workers);

  for (int i = 0; i < workers->Size; ++i) {
    status = WorkerArrayGet(workers, i, &worker);
    assert(TnStatusOk(status));

    WorkerStop(worker);
  }

  return TN_OK;
}
