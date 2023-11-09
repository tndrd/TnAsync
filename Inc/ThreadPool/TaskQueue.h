#pragma once
#include "Worker/Worker.h"
#include "malloc.h"
#include "string.h"

#define TQ_INITIAL_CAPACITY 2

typedef struct {
  WorkerTask* Tasks;
  size_t Head;
  size_t Tail;
  size_t Size;
  size_t Capacity;
} TaskQueue;

#ifdef __cplusplus
extern "C" {
#endif

TnStatus TaskQueueInit(TaskQueue* tq);
TnStatus TaskQueueDestroy(TaskQueue* tq);
TnStatus TaskQueuePush(TaskQueue* tq, const WorkerTask* task);
TnStatus TaskQueuePop(TaskQueue* tq, WorkerTask* task);
TnStatus TaskQueueSize(const TaskQueue* tq, size_t* size);

#ifdef __cplusplus
}
#endif

static TnStatus TaskQueueResize(TaskQueue* tq);