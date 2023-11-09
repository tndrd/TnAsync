#pragma once
#include <malloc.h>

#include "Worker/Worker.h"

typedef struct {
  Worker* Workers;
  size_t Size;
} WorkerArray;

#ifdef __cplusplus
extern "C" {
#endif

TnStatus WorkerArrayInit(WorkerArray* workers, size_t size);
TnStatus WorkerArrayRun(WorkerArray* workers, WorkerCallbackT callback);
TnStatus WorkerArrayStop(WorkerArray* workers);
TnStatus WorkerArrayDestroy(WorkerArray* workers);
TnStatus WorkerArrayGet(WorkerArray* workers, WorkerID id, Worker** worker);

#ifdef __cplusplus
}
#endif