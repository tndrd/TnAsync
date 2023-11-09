#include "Worker/Worker.h"

/* Main */
TnStatus WorkerInit(Worker* worker, WorkerID id) {
  assert(worker);
  worker->ID = id;

  pthread_mutex_init(&worker->Mutex, NULL);
  pthread_cond_init(&worker->Cond, NULL);

  return TN_OK;
}

/* Main */
TnStatus WorkerDestroy(Worker* worker) {
  pthread_mutex_destroy(&worker->Mutex);
  pthread_cond_destroy(&worker->Cond);

  return TN_OK;
}

/* Main */
TnStatus WorkerRun(Worker* worker, WorkerCallbackT callback) {
  assert(worker);
  assert(callback.Args);
  assert(callback.Function);

  worker->State = WORKER_FREE;
  worker->Callback = callback;
  worker->Active = 1;
  worker->HasTask = 0;
  worker->ResultBeenRead = 0;

  int result = pthread_create(&worker->Thread, NULL, WorkerLoop, worker);

  if (result != 0) {
    errno = result;
    return TNSTATUS(TN_ERRNO);
  }

  return TN_OK;
}

/* Thread */
static void WorkerSleep(Worker* worker) {
  assert(worker);
  pthread_cond_wait(&worker->Cond, &worker->Mutex);
}

/* Main */
static void WorkerWakeUp(Worker* worker) {
  assert(worker);
  pthread_cond_signal(&worker->Cond);
  pthread_mutex_unlock(&worker->Mutex);
}

/* Main ^ Thread */
TnStatus WorkerAssignTask(Worker* worker, WorkerTask task) {
  assert(worker);
  assert(task.Args);
  assert(task.Function);
  assert(task.Result);

  pthread_mutex_lock(&worker->Mutex);
  assert(worker->State == WORKER_FREE);

  worker->Task = task;
  worker->HasTask = 1;
  worker->ResultBeenRead = 0;

  WorkerWakeUp(worker);
}

/* Main ^ Thread */
TnStatus WorkerFinish(Worker* worker) {
  pthread_mutex_lock(&worker->Mutex);
  assert(worker->State == WORKER_DONE);

  worker->ResultBeenRead = 1;
  worker->HasTask = 0;
  WorkerWakeUp(worker);
}

/* Main ^ Thread */
TnStatus WorkerStop(Worker* worker) {
  assert(worker);

  pthread_mutex_lock(&worker->Mutex);
  worker->Active = 0;
  WorkerWakeUp(worker);

  pthread_join(worker->Thread, NULL);
}

/* Main */
TnStatus WorkerGetState(const Worker* worker, WorkerState* state) {
  assert(worker);
  assert(state);
  *state = worker->State;

  return TN_OK;
}

/* Thread */
static void WorkerSleepUntil(Worker* worker, int* condition) {
  assert(worker);
  assert(condition);
  while (worker->Active && !*condition) WorkerSleep(worker);
}

/* Thread */
static void WorkerExecute(void* workerArg) {
  Worker* worker = (Worker*)workerArg;

  assert(worker);

  void* args = worker->Task.Args;
  void* result = worker->Task.Result;
  WorkerFooT function = worker->Task.Function;

  assert(result);
  assert(function);
  assert(args);

  pthread_mutex_unlock(&worker->Mutex);
  function(args, result);
  pthread_mutex_lock(&worker->Mutex);
}

/* Thread */
static void WorkerNotify(Worker* worker) {
  assert(worker);

  WorkerCallbackFooT function = worker->Callback.Function;
  void* args = worker->Callback.Args;

  assert(function);
  assert(args);

  function(worker, args);
}

/* Thread */
static void* WorkerLoop(void* workerPtr) {
  assert(workerPtr);
  Worker* worker = (Worker*)workerPtr;

  cpu_set_t cpuset;

  int num_cores = sysconf(_SC_NPROCESSORS_ONLN);

  int cpu = worker->ID % num_cores;

  CPU_ZERO(&cpuset);      // clears the cpuset
  CPU_SET(cpu, &cpuset);  // set CPU 2 on cpuset

  sched_setaffinity(0, sizeof(cpuset), &cpuset);

  while (worker->Active) {
    WorkerNotify(worker);

    pthread_mutex_lock(&worker->Mutex);
    switch (worker->State) {
      case WORKER_FREE:
        WorkerSleepUntil(worker, &worker->HasTask);
        worker->State = WORKER_BUSY;
        break;
      case WORKER_BUSY:
        WorkerExecute(worker);
        worker->State = WORKER_DONE;
        break;
      case WORKER_DONE:
        WorkerSleepUntil(worker, &worker->ResultBeenRead);
        worker->State = WORKER_FREE;
        break;
      default:
        assert(0);
    }
    pthread_mutex_unlock(&worker->Mutex);
  }
}