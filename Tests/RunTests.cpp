#include <vector>

#include "Worker/Worker.h"
#include "ThreadPool/ThreadPool.h"
#include "gtest/gtest.h"

#define CALL(foo) ASSERT_EQ(foo.Code, TN_SUCCESS);

const int DefaultArg = 666;

void Callback(Worker* worker, void* arg) {
  ASSERT_NE(worker, nullptr);
  ASSERT_NE(arg, nullptr);
  ASSERT_EQ(*((int*)(arg)), DefaultArg);
}

void DummyCallbackFoo(Worker* worker, void* arg) {}

TEST(Worker, Init) {
  Worker worker;
  CALL(WorkerInit(&worker, 0));

  WorkerCallbackT callback;
  callback.Function = DummyCallbackFoo;

  CALL(WorkerRun(&worker, &callback));
  CALL(WorkerStop(&worker));
  CALL(WorkerDestroy(&worker));
}

TEST(Worker, Callback) {
  Worker worker;
  CALL(WorkerInit(&worker, 0));

  int arg = DefaultArg;
  WorkerCallbackT callback;
  callback.Args = &arg;
  callback.Function = Callback;

  CALL(WorkerRun(&worker, &callback));
  sleep(1);
  CALL(WorkerStop(&worker));
  CALL(WorkerDestroy(&worker));
}

void Pow(void* args, void* res) {
  int* val = (int*)args;
  int* result = (int*)res;

  *result = *val * *val;
}

void TaskCompleteCallback(Worker* worker, void* args) { *(int*)args = 1; }

TEST(Worker, SyncTaskChain) {
  Worker worker;
  CALL(WorkerInit(&worker, 0));

  int arg, res, done;
  WorkerCallbackT callback;
  callback.Args = &done;
  callback.Function = TaskCompleteCallback;

  WorkerTask task;
  task.Args = &arg;
  task.Result = &res;
  task.Function = Pow;

  CALL(WorkerRun(&worker, &callback));

  int nRepeats = 5;
  for (int i = 0; i < nRepeats; ++i) {
    arg = i;
    res = 0;
    done = 0;

    CALL(WorkerAssignTask(&worker, task));
    CALL(WorkerWaitTask(&worker));

    ASSERT_EQ(done, 1);
    ASSERT_EQ(res, i * i);

    CALL(WorkerFinishTask(&worker));
  }

  CALL(WorkerStop(&worker));
  CALL(WorkerDestroy(&worker));
}

#define N_REPEATS 5

const int NRepeats = N_REPEATS;
int TestBuf[N_REPEATS];
int DoStop = 0;

void WorkerChainTaskCallback(Worker* worker, void* args) {
  static int i = 0;
  WorkerTask* task = (WorkerTask*)(args);

  if (worker->State == WORKER_READY) {
    int* arg = (int*)(task->Args);
    *arg = i;
    CALL(WorkerAssignTaskAsync(worker, *task));
  }

  if (worker->State == WORKER_DONE) {
    int* result = (int*)(task->Result);
    TestBuf[i] = *result;
    CALL(WorkerFinishTaskAsync(worker));

    ASSERT_EQ(*result, i * i);
    i++;
    if (i == NRepeats) DoStop = 1;
  }
}

TEST(Worker, AsyncTaskChain) {
  Worker worker;
  CALL(WorkerInit(&worker, 0));

  WorkerTask task;
  int arg;
  int result;

  task.Args = &arg;
  task.Result = &result;
  task.Function = Pow;

  WorkerCallbackT callback;
  callback.Args = &task;
  callback.Function = WorkerChainTaskCallback;

  CALL(WorkerRun(&worker, &callback));
  while (!DoStop)
    ;
  CALL(WorkerStop(&worker));
  CALL(WorkerDestroy(&worker));

  for (int i = 0; i < NRepeats; ++i) {
    ASSERT_EQ(TestBuf[i], i * i);
  }
}

TEST(WorkerArray, Init) {
  size_t NWorkers = 10;

  WorkerArray wa;
  CALL(WorkerArrayInit(&wa, NWorkers));

  int arg = DefaultArg;
  WorkerCallbackT callback;
  callback.Args = &arg;
  callback.Function = Callback;

  CALL(WorkerArrayRun(&wa, callback));

  for (int i = 0; i < NWorkers; ++i) {
    Worker* worker;
    CALL(WorkerArrayGet(&wa, i, &worker));

    ASSERT_NE(worker, nullptr);
    EXPECT_EQ(worker->ID, i);
  }

  CALL(WorkerArrayStop(&wa));
  CALL(WorkerArrayDestroy(&wa));
}

#define FILLSIZE 625

TEST(TaskQueue, FillAndFlush) {
  TaskQueue tq;
  CALL(TaskQueueInit(&tq));

  int dummy;
  WorkerTask task;

  for (int i = 0; i < FILLSIZE; ++i) {
    task.Args = &dummy + i;
    CALL(TaskQueuePush(&tq, &task));
  }

  task.Args = NULL;
  for (int i = 0; i < FILLSIZE; ++i) {
    CALL(TaskQueuePop(&tq, &task));
    ASSERT_EQ(task.Args, &dummy + i) << "On #" << i << std::endl;
  }

  EXPECT_EQ(TaskQueuePop(&tq, &task).Code, TN_UNDERFLOW);

  CALL(TaskQueueDestroy(&tq));
}

TnStatus FillStatus = TN_OK;
int NFilled = 0;

TnStatus FlushStatus = TN_OK;
bool FlushResult = true;
int ErrorPop = 0;
void* ErrorVal = NULL;

void* Flush(void* tqmPtr) {
  size_t N = FILLSIZE;
  WorkerTask task;
  TnStatus status;

  TQMonitor* tqm = (TQMonitor*)(tqmPtr);
  task.Args = NULL;

  for (int i = 0; i < N;) {
    FlushStatus = TQMonitorGetTask(tqm, &task);
    if (FlushStatus.Code == TN_UNDERFLOW) continue;
    if (FlushStatus.Code != TN_SUCCESS) {
      ErrorPop = i;
      TQMonitorSignalError(tqm);
      break;
    }
    
    if (task.Args != tqm + i) {
      FlushResult = false;
      ErrorPop = i;
      ErrorVal = task.Args;
      TQMonitorSignalError(tqm);
      break;
    }

    i++;
  }

  return NULL;
}

TEST(TQMonitor, FillFlushWait) {
  FlushStatus = TN_OK;
  FlushResult = true;
  ErrorPop = 0;
  ErrorVal = 0;

  TQMonitor tqm;
  CALL(TQMonitorInit(&tqm));

  pthread_t thread;
  ASSERT_EQ(pthread_create(&thread, NULL, Flush, &tqm), 0);

  WorkerTask task;
  for (int i = 0; i < FILLSIZE; ++i) {
    task.Args = &tqm + i;
    CALL(TQMonitorAddTask(&tqm, &task));
  }

  TQMonitorWaitEmpty(&tqm);

  if (FlushStatus.Code != TN_SUCCESS) {
    ASSERT_EQ(FlushStatus.Code, TN_SUCCESS)
        << "On pop: " << ErrorPop << std::endl;
  }

  if (!FlushResult) {
    ASSERT_EQ(FlushResult, true)
        << "On pop: " << ErrorPop << " with value: " << ErrorVal << " expected: " << &tqm + ErrorPop << std::endl;
  }

  WorkerTask dummy;
  EXPECT_EQ(TQMonitorGetTask(&tqm, &dummy).Code, TN_UNDERFLOW);

  CALL(TQMonitorDestroy(&tqm));

  pthread_join(thread, NULL);
}

TEST(WorkerQueue, FillAndFlush) {
  size_t NWorkers = 10;
  WorkerQueue wq;
  WorkerID id;

  WorkerQueueInit(&wq, NWorkers);

  ASSERT_EQ(WorkerQueuePop(&wq, &id).Code, TN_UNDERFLOW);

  for (int i = 0; i < NWorkers; ++i) {
    id = i;
    CALL(WorkerQueuePush(&wq, &id));
  }

  ASSERT_EQ(WorkerQueuePush(&wq, &id).Code, TN_OVERFLOW);

  id = 42;

  for (int i = 0; i < NWorkers; ++i) {
    CALL(WorkerQueuePop(&wq, &id));
    ASSERT_EQ(id, i);
  }

  ASSERT_EQ(WorkerQueuePop(&wq, &id).Code, TN_UNDERFLOW);

  CALL(WorkerQueueDestroy(&wq));
}

void* FillWQ(void* wqPtr) {
  WQMonitor* wq = (WQMonitor*)wqPtr;

  for (int i = 0; i < FILLSIZE; ++i) {
    WorkerID id = i;

    FillStatus = WQMonitorAddWorker(wq, &id);
    if (FillStatus.Code != TN_SUCCESS) {
      NFilled = i + 1;
      WQMonitorSignalError(wq);
      break;
    }
  }

  return NULL;
}

TEST(WQMonitor, FillWait) {
  FillStatus = TN_OK;
  NFilled = 0;

  WQMonitor wq;
  WorkerID id;
  CALL(WQMonitorInit(&wq, FILLSIZE));

  ASSERT_EQ(WQMonitorGetWorker(&wq, &id).Code, TN_UNDERFLOW);

  pthread_t thread;
  int ret = pthread_create(&thread, NULL, FillWQ, &wq);
  ASSERT_EQ(ret, 0);

  WQMonitorWaitFull(&wq);

  ASSERT_EQ(FillStatus.Code, TN_SUCCESS) << "Filled totally: " << NFilled << std::endl;
  ASSERT_EQ(WQMonitorAddWorker(&wq, &id).Code, TN_OVERFLOW);

  for (int i = 0; i < FILLSIZE; ++i) {
    CALL(WQMonitorGetWorker(&wq, &id));
    EXPECT_EQ(id, i);
  }

  ASSERT_EQ(WQMonitorGetWorker(&wq, &id).Code, TN_UNDERFLOW);

  CALL(WQMonitorDestroy(&wq));
  pthread_join(thread, NULL);
}

TEST(ThreadPool, Init) {
  size_t NWorkers = 10;
  ThreadPool tp;

  CALL(ThreadPoolInit(&tp, NWorkers));
  CALL(ThreadPoolRun(&tp));
  CALL(ThreadPoolStop(&tp));
  CALL(ThreadPoolDestroy(&tp));
}

struct TaskData {
  int Arg;
  int Res;
};

template<size_t NWorkers, size_t NTasks>
void TestThreadPool() {
  WorkerTask Tasks[NTasks];
  TaskData TasksImpl[NTasks];

  for (int i = 0; i < NTasks; ++i) {
    TasksImpl[i].Arg = i;
    TasksImpl[i].Res = -1;

    Tasks[i].Args = &TasksImpl[i].Arg;
    Tasks[i].Result = &TasksImpl[i].Res;
    Tasks[i].Function = Pow;
  }

  ThreadPool tp;
  CALL(ThreadPoolInit(&tp, NWorkers));
  CALL(ThreadPoolRun(&tp));

  for (int i = 0; i < NTasks; ++i) {
    CALL(ThreadPoolAddTask(&tp, Tasks[i]));
  }

  CALL(ThreadPoolWaitAll(&tp));

  for (int i = 0; i < NTasks; ++i) {
    ASSERT_EQ(TasksImpl[i].Res, i * i);
  }

  CALL(ThreadPoolStop(&tp));
  CALL(ThreadPoolDestroy(&tp));
}

TEST(ThreadPool, OneWorkerOneTask) {
  static constexpr size_t NTasks = 1;
  static constexpr size_t NWorkers = 1;

  TestThreadPool<NWorkers, NTasks>();
}

TEST(ThreadPool, OneWorkerManyTasks) {
  static constexpr size_t NTasks = 10000;
  static constexpr size_t NWorkers = 1;

  TestThreadPool<NWorkers, NTasks>();
}

TEST(ThreadPool, ManyWorkersOneTask) {
  static constexpr size_t NTasks = 1;
  static constexpr size_t NWorkers = 100;

  TestThreadPool<NWorkers, NTasks>();
}

TEST(ThreadPool, FewWorkersManyTasks) {
  static constexpr size_t NTasks = 10000;
  static constexpr size_t NWorkers = 5;

  TestThreadPool<NWorkers, NTasks>();
}

TEST(ThreadPool, ManyWorkersFewTasks) {
  static constexpr size_t NTasks = 10;
  static constexpr size_t NWorkers = 100;

  TestThreadPool<NWorkers, NTasks>();
}

TEST(ThreadPool, EqualWorkersTasks) {
  static constexpr size_t NTasks = 100;
  static constexpr size_t NWorkers = 100;

  TestThreadPool<NWorkers, NTasks>();
}