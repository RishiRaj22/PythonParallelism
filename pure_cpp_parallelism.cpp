#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <unistd.h>

PyDoc_STRVAR(c_parallelsim_fns_documentation,
             "Doc for c_parallelism implemented in C");

/**
 * Returns the factorial % 1e9+7 for a given number.
 * This is just used to demonstrate any expensive operation.
 */
int factorial(int val) {
	int i;
	struct timeval t1, t2;
	time_t ltime;
	struct tm *tm;
	double elapsedTime;
	long long int s = 1;
        gettimeofday(&t1, NULL);

	/** ------- ACTUAL BUSINESS LOGIC BEGINS ------------------ */
	for (i = val; i >= 1; --i) {
		s *= i;
		s %= 1000000007;
	}
	/** ------- ACTUAL BUSINESS LOGIC ENDS ------------------ */

	// Calculate elapsed time & log current time
	gettimeofday(&t2, NULL);
	elapsedTime = (t2.tv_sec - t1.tv_sec) * 1000.0;      // sec to ms
	elapsedTime += (t2.tv_usec - t1.tv_usec) / 1000.0;   // us to ms
	ltime=time(NULL);
	tm = localtime(&ltime);
	printf("[%04d-%02d-%02d %02d:%02d:%02d C API] Time taken is %f ms\n", elapsedTime, tm->tm_year+1900, tm->tm_mon, 
		tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
	return s;
}

PyObject* c_factorial(PyObject* self, PyObject* args) {
    int num;
    int sol;
    if (!PyArg_ParseTuple(args, "i", &num)) {
        return NULL;
    }

    Py_BEGIN_ALLOW_THREADS
    sol = factorial(num);
    Py_END_ALLOW_THREADS

    PyObject* object = PyLong_FromLong(sol);
    return object;
}

static PyMethodDef c_parallelsim_fns[] = {
    {"c_factorial", c_factorial, METH_VARARGS, "Calculate factorial in C layer"},
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

PyMODINIT_FUNC
PyInit_pure_cpp_parallelism(void)
{
  static struct PyModuleDef moduledef = {
      PyModuleDef_HEAD_INIT,                   /* m_base */
      "pure_cpp_parallelism",   /* m_name */
      c_parallelsim_fns_documentation,            /* m_doc */
      -1,                                      /* m_size */
      c_parallelsim_fns,                          /* m_methods */
      nullptr,                                    /* m_slots */
      nullptr,                                    /* m_traverse */
      nullptr,                                    /* m_clear */
      nullptr                                     /* m_free */
  };
  return PyModule_Create(&moduledef);
}

