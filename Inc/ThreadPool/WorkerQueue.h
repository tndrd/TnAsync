#pragma once
#include "TnStatus.h"
#include "assert.h"
#include "malloc.h"

typedef size_t WorkerID;

typedef struct {
  WorkerID* Buffer;
  size_t Size;
  size_t Head;
  size_t Tail;
  size_t Capacity;

} WorkerQueue;

#ifdef __cplusplus
extern "C" {
#endif

TnStatus WorkerQueueInit(WorkerQueue* wq, size_t capacity);
TnStatus WorkerQueueDestroy(WorkerQueue* wq);
TnStatus WorkerQueuePush(WorkerQueue* wq, const WorkerID* id);
TnStatus WorkerQueuePop(WorkerQueue* wq, WorkerID* id);
TnStatus WorkerQueueSize(const WorkerQueue* wq, size_t* size);

#ifdef __cplusplus
}
#endif
