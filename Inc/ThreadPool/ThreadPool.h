#pragma once
#include "ThreadPool/TQMonitor.h"
#include "ThreadPool/WQMonitor.h"
#include "ThreadPool/WorkerArray.h"

typedef struct {
  TQMonitor Tasks;
  WQMonitor FreeWorkers;
  WorkerArray Workers;
} ThreadPool;

static void WorkerCallback(Worker* worker, void* args);

#ifdef __cplusplus
extern "C" {
#endif

TnStatus ThreadPoolInit(ThreadPool* tp, size_t nWorkers);
TnStatus ThreadPoolRun(ThreadPool* tp);
TnStatus ThreadPoolStop(ThreadPool* tp);
TnStatus ThreadPoolDestroy(ThreadPool* tp);
TnStatus ThreadPoolAddTask(ThreadPool* tp, WorkerTask task);
TnStatus ThreadPoolWaitAll(ThreadPool* tp);

#ifdef __cplusplus
}
#endif