#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <unistd.h>
#include <vector>
#include <thread>
#include <future>
#include <string>
#include <iostream>
#include <chrono>
#include <sstream>

// Uncomment below line for getting debug logs while running parallelism function.
// #define DEBUG 1

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG
#    define PY_LOG(title, py_obj) std::cerr << title << ": "; PyObject_Print(py_obj, stdout, 0); std::cerr << std::endl;
#else
#    define PY_LOG(title, py_obj) do { } while(0);
#endif

// Load pickling/un-pickling functions.
#define LOAD_PICKLE_UNPICKLE \
    PyObject* pkl_mod_name = PyUnicode_FromString("pickle");            \
    PyObject* pkl_mod = PyImport_Import(pkl_mod_name);                  \
    PyObject* pkl_dump_func = PyObject_GetAttrString(pkl_mod, "dumps"); \
    PyObject* pkl_load_func = PyObject_GetAttrString(pkl_mod, "loads");

// Clear all pickling/un-pickling functions.
#define UNLOAD_PICKLE_UNPICKLE Py_DECREF(pkl_mod_name); Py_DECREF(pkl_mod); Py_DECREF(pkl_dump_func); Py_DECREF(pkl_load_func);

// Create a PyObject* "obj" by unpickling "pickled"
#define UNPICKLE_PYOBJECT(obj, pickled) \
    PyObject* pickled_bytes     = PyBytes_FromString(pickled); \
    PyObject* obj = PyObject_CallFunctionObjArgs(pkl_load_func, pickled_bytes, nullptr); \
    Py_DECREF(pickled_bytes);

// Create a std::string "str" by unpickling PyObject* "object"
#define PICKLE_PYOBJECT(str, object) \
    PyObject* protocol = PyLong_FromLong(0); \
    PyObject* decoded_bytes = PyObject_CallFunctionObjArgs(pkl_dump_func, object, protocol , nullptr); \
    std::string str = PyBytes_AsString(decoded_bytes); \
    Py_DECREF(protocol); \
    Py_DECREF(decoded_bytes);

// Check if any Python error has occurred. If so, set exception in the promise & end Python interpreter.
#define CHECK_FOR_ERROR(PROMISE) \
    if (PyErr_Occurred()) {                                         \
        PyObject* err = PyErr_GetRaisedException();                 \
        PyObject* err_str = PyObject_Repr(err);                     \
        auto exc = std::runtime_error(PyUnicode_AsUTF8(err_str));   \
        PROMISE.set_exception(std::make_exception_ptr(exc));        \
        PyErr_Clear();                                              \
        /** Note that ideally, we should have run Py_DECREFs here*/ \
        Py_EndInterpreter(interp_tstate);                           \
        return;                                                     \
    }

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

PyDoc_STRVAR(subinterpreter_parallelsim_fns_documentation,
             "Module for implementing 'true' parallelism using sub-interpreters (needs Python 3.12+)");


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Run a particular function in a new sub-interpreter.
 * This function should always be called from a new thread.
 * mod : Name of module
 * func: Name of function in module to run.
 * args: Pickled tuple of arguments.
 * prom: Promise which is resolved with pickled repr of output when the function completes.
 */
void run_parallely_in_subinterp(std::string mod, std::string func, std::string args, std::promise<std::string>&& prom) {
    // Create a new sub-interpreter with its own GIL.
    PyThreadState *interp_tstate = nullptr;
    Py_ssize_t size;
    PyInterpreterConfig sub_interp_config = {
        .use_main_obmalloc = 0,
        .allow_fork = 0,
        .allow_exec = 0,
        .allow_threads = 0,
        .allow_daemon_threads = 0,
        .check_multi_interp_extensions = 1,
        .gil = PyInterpreterConfig_OWN_GIL,
    };
    PyStatus status = Py_NewInterpreterFromConfig(&interp_tstate, &sub_interp_config);

    // Load pickling/unpickling functions.
    LOAD_PICKLE_UNPICKLE
    CHECK_FOR_ERROR(prom)

    // Load the Python module.
    PyObject* py_mod_str = PyUnicode_FromString(mod.c_str());
    PyObject* py_mod = PyImport_Import(py_mod_str);
    PY_LOG("module", py_mod)
    CHECK_FOR_ERROR(prom)

    // Load the Python function from the module.
    PyObject* py_func = PyObject_GetAttrString(py_mod, func.c_str());
    PY_LOG("function", py_func)
    CHECK_FOR_ERROR(prom)

    // Unpickle arguments
    UNPICKLE_PYOBJECT(py_args, args.c_str())
    PY_LOG("arguments", py_args)
    CHECK_FOR_ERROR(prom)

    // Invoke the Python function with the given arguments.
    PyObject* py_result = PyObject_CallObject(py_func, py_args);
    PY_LOG("py_result", py_result)
    CHECK_FOR_ERROR(prom)

    // Pickle the result & send it as a promise resolution.
    PICKLE_PYOBJECT(result, py_result);
    prom.set_value(result);

    // Cleanup of objects created
    UNLOAD_PICKLE_UNPICKLE
    Py_DECREF(py_mod_str);
    Py_DECREF(py_mod);
    Py_DECREF(py_func);
    Py_DECREF(py_args);
    Py_DECREF(py_result);


    // End the interpreter
    Py_EndInterpreter(interp_tstate);
}


/**
 * Python function to run multiple arbitrary python functions parallely 
 * using multiple threads & sub-interpreters with separate GIL.
 */
PyObject* subinterpreter_parallel(PyObject* self, PyObject* args) {
    // Load pickling functions.
    LOAD_PICKLE_UNPICKLE
    PY_LOG("original args", args)


    // Create vectors for storing threads, promises & futures.
    std::vector<std::thread> threads;
    std::vector<std::promise<std::string>> promises(PySequence_Length(args));
    std::vector<std::future<std::string>> futures;
    for (auto& p: promises) {
        futures.push_back(p.get_future());
    }

    // Iterate over all of the arguments.
    PyObject* iterator = PyObject_GetIter(args);
    PyObject* item;
    int idx = 0;
    while ((item = PyIter_Next(iterator))) {
        PyObject* mod_name = PyList_GetItem(item, 0);
        PyObject* func_name = PyList_GetItem(item, 1);
        PyObject* py_args = PyList_GetItem(item, 2);

        // Convert Python objects to strings.
        auto mod_str = std::string(PyUnicode_AsUTF8(mod_name));
        auto func_str = std::string(PyUnicode_AsUTF8(func_name));
        PICKLE_PYOBJECT(arg_str, py_args);

        // Create thread to run idx-th request in a new thread using a new sub-interpreter with its own GIL.
        threads.push_back(std::thread(run_parallely_in_subinterp, mod_str, func_str, arg_str, std::move(promises[idx])));
        idx++;

        // Perform cleanup.
        Py_DECREF(mod_name);
        Py_DECREF(func_name);
        Py_DECREF(py_args);
        Py_DECREF(item);
    }

    // Fetch results from each of the threads and join them to create the final result.
    std::ostringstream errors_buf;
    bool err = false;
    PyObject* list = PyList_New(threads.size());
    for (size_t i = 0; i < threads.size(); ++i) {
        // Release GIL while waiting for thread.
        Py_BEGIN_ALLOW_THREADS
        threads[i].join();
        Py_END_ALLOW_THREADS
        // Re-acquire GIL to perform Python operations.
        try {
            // Load the result & set it to idx-th member of resultant list.
            std::string res = futures[i].get();
            UNPICKLE_PYOBJECT(py_result, res.c_str())
            PyList_SET_ITEM(list, i, py_result);
        } catch (const std::exception& e) {
            // If an error is recd., add it to errors_buff.
            errors_buf << i << "th function call failed with exception: " << e.what() << "\n";
        }
    }

    // Set the result as a runtime error if any of the threads failed, with all the error messages.
    auto errors = errors_buf.str();
    if (errors.size()) {
        PyErr_SetString(PyExc_RuntimeError, errors.c_str());
        Py_DECREF(list);
        list = nullptr;
    }

    // Clear pickle functions.
    UNLOAD_PICKLE_UNPICKLE
    Py_DECREF(iterator);
    return list;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Module functions definition.
static PyMethodDef subinterpreter_parallelsim_fns[] = {
    {"parallel", subinterpreter_parallel, METH_VARARGS, "Run things in parallel threads using different sub-interpreters.\nSince this uses multiple sub-interpreters, ensure that the function being run \ndoesn't require any already imported modules, or any other dependency on current Python state.\n Argument can be multiple [module, function, args] pairs.\nExample: parallel([mod1, fn1, arg1], [mod2, fn2, arg2])"},
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

// Modue definition
PyMODINIT_FUNC
PyInit_subinterpreter_parallelism(void)
{
    static struct PyModuleDef moduledef = {
      PyModuleDef_HEAD_INIT,                                 /* m_base */
      "subinterpreter_parallelism",                          /* m_name */
      subinterpreter_parallelsim_fns_documentation,          /* m_doc */
      -1,                                                    /* m_size */
      subinterpreter_parallelsim_fns,                        /* m_methods */
      nullptr,                                               /* m_slots */
      nullptr,                                               /* m_traverse */
      nullptr,                                               /* m_clear */
      nullptr                                                /* m_free */
  };
  return PyModule_Create(&moduledef);
}

