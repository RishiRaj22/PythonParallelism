from subinterpreter_parallelism import parallel
from pure_cpp_parallelism import c_factorial
from random import randint
import isolated_benchmark
from multiprocessing import Process
from threading import Thread
import logging
import time

logging.basicConfig(
        format='[%(asctime)s.%(msecs)03d] %(message)s',
        datefmt='%Y-%m-%d %H:%M:%S')

# Number of task runs to spawn.
NUM_TASKS = 10

# Increase MIN & MAX to increase time taken by each task (should be < 1000000000)
MIN = 90000000
MAX = 90000100

# Benchmark Python function against multiple Python processes.
logging.warning(f"Multi-processing mode started for {NUM_TASKS} regular Python tasks")
start = time.time()

processes = [Process(target=isolated_benchmark.py_factorial, args=(randint(MIN, MAX),)) for _ in range(NUM_TASKS)]
for p in processes:
    p.start()
for p in processes:
    p.join()

end = time.time()
logging.warning(f"Multi-processing mode ended for {NUM_TASKS} regular Python tasks in {end-start} seconds")

# Benchmark Python function against the custom handwritten sub-interpreter parallelism module.
logging.warning(f"Sub-interpreter parallelism mode started for {NUM_TASKS} regular Python tasks")
start = time.time()

result = parallel(*[['isolated_benchmark', 'py_factorial', (randint(MIN,MAX),)] for _ in range(NUM_TASKS)])

end = time.time()
logging.warning(f"Sub-interpreter parallelism mode ended with {result=} in {end-start} seconds")

# Benchmark similar C++ extension function.
logging.warning(f"Multi-threaded C++ code execution from Python function started for {NUM_TASKS} tasks")
start = time.time()
threads = [Thread(target=c_factorial, args=(randint(MIN,MAX),)) for _ in range(NUM_TASKS)]
for thread in threads:
    thread.start()
for thread in threads:
    thread.join()
end = time.time()
logging.warning(f"Multi-threaded C++ code execution from Python function in {end-start} seconds")

# Benchmark Python function against a single thread.
start = time.time()
logging.warning("Single threaded mode for 1 regular Python task begins")
result = isolated_benchmark.py_factorial(randint(MIN, MAX))
end = time.time()
logging.warning(f"Single threaded mode ended for 1 regular Python task with {result=} in {end-start} seconds")

# Benchmark Python function against multiple threads.
logging.warning(f"Multi-threaded mode for {NUM_TASKS} regular Python functions begins")
start = time.time()
threads = [Thread(target=isolated_benchmark.py_factorial, args=(randint(MIN,MAX),)) for _ in range(NUM_TASKS)]
for thread in threads:
    thread.start()
for thread in threads:
    thread.join()
end = time.time()
logging.warning(f"Multi-threaded mode ended for {NUM_TASKS} regular Python functions in {end-start} seconds")
