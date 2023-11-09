#include "ThreadPool/WorkerQueue.h"

/* Helper */
static void WorkerQueueDump(const WorkerQueue* wq) {
  assert(wq);
  fprintf(stderr,
          "WorkerQueue: \n"
          "  Size: %lu\n"
          "  C-ty: %lu\n"
          "  Head: %lu\n"
          "  Tail: %lu\n"
          "  Data: { ",
          wq->Size, wq->Capacity, wq->Head, wq->Tail);

  for (int i = 0; i < wq->Capacity; ++i)
    fprintf(stderr, "%lu ", wq->Buffer[(wq->Tail + i) % wq->Capacity]);

  fprintf(stderr, "}\n");
}

TnStatus WorkerQueueInit(WorkerQueue* wq, size_t capacity) {
  assert(wq);
  assert(capacity > 0);

  WorkerID* newBuffer = (WorkerID*)malloc(capacity * sizeof(WorkerID));
  if (!newBuffer) return TNSTATUS(TN_BAD_ALLOC);

  wq->Buffer = newBuffer;
  wq->Size = 0;
  wq->Tail = 0;
  wq->Head = 0;
  wq->Capacity = capacity;

  return TN_OK;
}

TnStatus WorkerQueueDestroy(WorkerQueue* wq) {
  assert(wq);

  free(wq->Buffer);
  return TN_OK;
}

TnStatus WorkerQueuePush(WorkerQueue* wq, const WorkerID* id) {
  assert(wq);
  assert(id);

  if (wq->Size == wq->Capacity) return TNSTATUS(TN_OVERFLOW);

  wq->Buffer[wq->Head] = *id;
  wq->Head = (wq->Head + 1) % wq->Capacity;
  wq->Size++;

  return TN_OK;
}

TnStatus WorkerQueuePop(WorkerQueue* wq, WorkerID* id) {
  assert(wq);
  assert(id);

  if (wq->Size == 0) return TNSTATUS(TN_UNDERFLOW);

  wq->Size--;
  *id = wq->Buffer[wq->Tail];
  wq->Tail = (wq->Tail + 1) % wq->Capacity;

  return TN_OK;
}

TnStatus WorkerQueueSize(const WorkerQueue* wq, size_t* size) {
  assert(wq);
  assert(size);

  *size = wq->Size;

  return TN_OK;
}