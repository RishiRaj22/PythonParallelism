# True parallelism in pure-Python code

This is a proof-of-concept implementation of running "pure" Python code truly parallely in a single CPython process. This is achieved utilizing the new [per-interpreter GIL](https://peps.python.org/pep-0684/) construct from Python 3.12.

Here is a rough summary of what I have achieved:-

1. Create a local CPython function `subinterpreter_parallelism.parallel` which allows users to call any arbitrary Python function using multiple threads, each starting up its own sub-interpreter with its own GIL.
1. This function takes in a variable number of lists, where each of the lists would consist of ["module_name", "function_name", args].
1. Internally, we iterate over each of these lists, and spawn a new thread with their own interpreters to run those specific requests parallely.
1. Since we can't easily share objects between these interpreters, I've opted to take module name & function name as strings from user and pass the same as an argument to the thread. Here, for the Python function's arguments, I'm pickling the args object to a `std::string` using `pickle` module.
1. The spawned threads then create a new interpreter with its own GIL & run the function.
1. Once the function completes execution in the thread, it notifies the result to the Python function using a promise.
1. The Python function then unpickles the results as the different futures get resolved, and add them to a list.
1. This list is finally returned to the user when all the function calls have completed.

## Caveats

1. This might not work well, or at all, with various other commonly used Python libraries (e.g. Numpy) in its current state. This is because, by default, all C/C++ extension modules are initialized [without support for multiple interpreters](https://docs.python.org/3/c-api/module.html#c.Py_mod_multiple_interpreters). This holds true for all modules created using [Cythonize](https://github.com/cython/cython/blob/368bbde62565f8798e061754caf60c94107f2d8c/Cython/Compiler/ModuleNode.py#L3547) (like Numpy), as of April, 2024. This is because C extension libraries interact regularly with low-level APIs (like `PyGIL_*`) which are known to not work well with multiple sub-interpreters. Refer [caveats section from Python documentation](https://docs.python.org/3/c-api/init.html#bugs-and-caveats). Hopefully, more libraries add support for this paradigm as it gains more adoption.
2. Performance should still be much better with pure C++ code for highly CPU bound tasks, due to the overhead associated with Python being an interpreted language.
3. Since very little is shared between interpreters in my setup, things like logging configuration, imports etc. need to be explicitly provided in the functions being run parallely.

Note that this is just an experimental project done over a weekend that might be of interest to others interested in parallelism & Python evolution.


## Usage
Please use Python 3.12 & above for testing this out.
Here, the commands given are for Linux & might require tweaking on other operating systems.

1. [REQUIRED] Create & activate a virtual environment 
`python3.12 -m venv .venv && source .venv/bin/activate`

1. Add `benchmarking` directory from this folder is treated as a Python source directory. 
```export PYTHONPATH=$PYTHONPATH:`pwd`/benchmarking```

1. Ensure that setuptools is available locally.
`pip install setuptools`

1. Setup Python extension locally.
`python3 setup.py install`

1. Run demo function
`python3 demo.py`

## Statistics
Using normal Python threads, we can't gain any performance improvement for CPU bound tasks in CPython due to GIL contention. Hence, comparing parallelism using a simple factorial function, we get the following statistics:-

| Method | Total time taken | 
| - | - |
Multi-processing | 15.07s
Sub-interpreters | 11.48s |
C++ extension code (with GIL relinquished) |  0.74s 

Out of the total time taken for running a functions parallely using sub-interpreters, we see the following breakdown of time taken at each step:-
| Step | Time taken (ms) |
| - | - |
| Creating interpreters | 17 | 
| Imports & pickling/unpickling | 35 |
| Function call | 2020 |
| Ending interpreters | 2.7 |


## Takeaways

1. Using sub-interpreter paralllelism, I was able to verify that the Python process is constantly hitting close to full CPU utilization across all cores (1995% CPU utilization for machine with 20 cores). Note that with regular Python threads, the CPU utilization, as expected, hovers around 100% (i.e. almost full utilization of a single core).
2. Significantly (>20%) better performance with subinterpreter parallelism compared to multi-processing.
3. Due to the inherent slowness associated with a interpreted language, it's still better to implement the CPU-bound part of the functionality in C++ using Python extensions.
4. With Python 3.13, much of this work would be redundant as interpreters would be made part of the stdlib itself. However, it's still fascinating to see how we can achieve similar results in Python 3.12.

