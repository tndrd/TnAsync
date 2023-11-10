#include "Worker/Worker.h"

/* Main */
TnStatus WorkerInit(Worker* self, WorkerID id) {
  if (!self) return TNSTATUS(TN_BAD_ARG_PTR);

  self->ID = id;

  pthread_mutex_init(&self->Mutex, NULL);
  pthread_cond_init(&self->Cond, NULL);

  return TN_OK;
}

/* Main */
TnStatus WorkerDestroy(Worker* self) {
  if (!self) return TNSTATUS(TN_BAD_ARG_PTR);

  WorkerStop(self);

  pthread_mutex_destroy(&self->Mutex);
  pthread_cond_destroy(&self->Cond);

  return TN_OK;
}

/* Main */
TnStatus WorkerRun(Worker* self, WorkerCallbackT* stateCb) {
  if (!self) return TNSTATUS(TN_BAD_ARG_PTR);

  self->State = WORKER_STARTED;
  self->DoStop = 0;
  self->DoReset = 0;
  self->DoStart = 0;
  self->NTask = 0;

  self->StateCallback.Args = (stateCb) ? stateCb->Args : NULL;
  self->StateCallback.Function = (stateCb) ? stateCb->Function : NULL;

  int res = pthread_create(&self->Thread, NULL, WorkerLoop, self);
  if (res != 0) {
    errno = res;
    return TNSTATUS(TN_ERRNO);
  }

  return TN_OK;
}

TnStatus WorkerAssignTask(Worker* self, WorkerTask task) {
  if (!self) return TNSTATUS(TN_BAD_ARG_PTR);
  TnStatus status;

  WorkerLock(self);

  if (self->State == WORKER_STARTED) WorkerSleepUntilStateChanges(self);

  status = WorkerAssignTaskAsync(self, task);
  if (!TnStatusOk(status)) {
    WorkerUnlock(self);
    return status;
  }

  size_t nTask = self->NTask;

  WorkerWakeUp(self);
  WorkerSleepUntilStateChanges(self);

  status = TN_OK;

  WorkerUnlock(self);
  return status;
}

TnStatus WorkerWaitTask(Worker* self) {
  if (!self) return TNSTATUS(TN_BAD_ARG_PTR);
  TnStatus status;

  WorkerLock(self);

  if (self->State == WORKER_BUSY) WorkerSleepUntilStateChanges(self);

  if (self->State == WORKER_DONE) {
    status = TN_OK;
  } else
    status = TNSTATUS(TN_FSM_WRONG_STATE);

  WorkerUnlock(self);

  return status;
}

TnStatus WorkerFinishTask(Worker* self) {
  if (!self) return TNSTATUS(TN_BAD_ARG_PTR);
  TnStatus status;

  WorkerLock(self);

  status = WorkerFinishTaskAsync(self);

  if (!TnStatusOk(status)) {
    WorkerUnlock(self);
    return status;
  }

  WorkerWakeUp(self);
  WorkerSleepUntilStateChanges(self);

  status = TN_OK;

  WorkerUnlock(self);

  return status;
}

TnStatus WorkerStop(Worker* self) {
  if (!self) return TNSTATUS(TN_BAD_ARG_PTR);
  TnStatus status;

  WorkerLock(self);

  if (self->State != WORKER_STOPPED) {
    self->DoStop = 1;
    WorkerWakeUp(self);
    WorkerSleepUntilStateChanges(self);
    pthread_join(self->Thread, NULL);
  }

  WorkerUnlock(self);

  return TN_OK;
}

TnStatus WorkerGetState(Worker* self, WorkerState* state) {
  if (!self || !state) return TNSTATUS(TN_BAD_ARG_PTR);

  WorkerLock(self);
  *state = self->State;
  WorkerUnlock(self);

  return TN_OK;
}

TnStatus WorkerAssignTaskAsync(Worker* self, WorkerTask task) {
  if (!self) return TNSTATUS(TN_BAD_ARG_PTR);

  TnStatus status = ValidateTask(task);
  if (!TnStatusOk(status)) return status;

  if (self->State != WORKER_READY) return TNSTATUS(TN_FSM_WRONG_STATE);

  self->Task = task;
  self->DoStart = 1;
  self->DoReset = 0;

  return TN_OK;
}

TnStatus WorkerFinishTaskAsync(Worker* self) {
  if (!self) return TNSTATUS(TN_BAD_ARG_PTR);

  if (self->State != WORKER_DONE) return TNSTATUS(TN_FSM_WRONG_STATE);

  self->DoReset = 1;
  self->DoStart = 0;
  self->NTask++;

  return TN_OK;
}

/* Thread */
static void WorkerSleep(Worker* self) {
  assert(self);
  pthread_cond_wait(&self->Cond, &self->Mutex);
}

/* Main */
static void WorkerWakeUp(Worker* self) {
  assert(self);
  pthread_cond_signal(&self->Cond);
}

static void WorkerLock(Worker* self) {
  assert(self);
  pthread_mutex_lock(&self->Mutex);
}

static void WorkerUnlock(Worker* self) {
  assert(self);
  pthread_mutex_unlock(&self->Mutex);
}

static void WorkerSleepUntilCond(Worker* self, int* condition) {
  assert(self);
  assert(condition);

  while (!self->DoStop && !*condition) WorkerSleep(self);
}

static void WorkerSleepUntilStateChanges(Worker* self) {
  assert(self);
  WorkerState state = self->State;
  size_t nTask = self->NTask;
  while (self->State != WORKER_STOPPED && self->State == state &&
         self->NTask == nTask)
    WorkerSleep(self);
}

static void WorkerAssignToCore(Worker* self) {
  assert(self);

  cpu_set_t cpuSet;
  int numCores = sysconf(_SC_NPROCESSORS_ONLN);

  int coreNumber = self->ID % numCores;

  CPU_ZERO(&cpuSet);             // clears the cpuset
  CPU_SET(coreNumber, &cpuSet);  // set CPU 2 on cpuset

  sched_setaffinity(0, sizeof(cpuSet), &cpuSet);
}

/* Thread */
static void* WorkerLoop(void* selfPtr) {
  assert(selfPtr);
  Worker* self = (Worker*)selfPtr;

  WorkerAssignToCore(self);

  while (1) {
    WorkerLock(self);

    if (self->DoStop) {
      self->State = WORKER_STOPPED;
      WorkerWakeUp(self);
      WorkerUnlock(self);
      break;
    }

    if (self->StateCallback.Function)
      self->StateCallback.Function(self, self->StateCallback.Args);

    switch (self->State) {
      case WORKER_STARTED:
        self->State = WORKER_READY;
        break;
      case WORKER_READY:
        WorkerWakeUp(self);
        WorkerSleepUntilCond(self, &self->DoStart);
        self->State = WORKER_BUSY;
        break;
      case WORKER_BUSY:
        WorkerWakeUp(self);
        WorkerUnlock(self);
        self->Task.Function(self->Task.Args, self->Task.Result);
        WorkerLock(self);
        self->State = WORKER_DONE;
        break;
      case WORKER_DONE:
        WorkerWakeUp(self);
        WorkerSleepUntilCond(self, &self->DoReset);
        self->State = WORKER_READY;
        break;
      default:
        assert(0);
    }

    WorkerUnlock(self);
  }
}

static TnStatus ValidateTask(WorkerTask task) {
  if (!task.Args || !task.Function || !task.Result)
    return TNSTATUS(TN_BAD_ARG_PTR);
  return TN_OK;
}