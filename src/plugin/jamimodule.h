#define PY_SSIZE_T_CLEAN
#include <Python.h>
// #include <jami.h>
#include <jami/jami.h>
#include <jami/videomanager_interface.h>
#include <iostream>

static int numargs = 0;

/* Return the number of arguments of the application command line */
static PyObject*
jami_numargs(PyObject *self, PyObject *args)
{
    if(!PyArg_ParseTuple(args, ":numargs"))
        return NULL;

    return PyLong_FromLong(numargs);
}

static PyObject*
jami_version(PyObject* self, PyObject *args)
{
    if(!PyArg_ParseTuple(args, ":version"))
        return NULL;
    auto version = libjami::version();
    // std::cout << "hello! " << version << std::endl;

    return PyUnicode_FromString(version);
}

static PyMethodDef jamiMethods[] = {
    {"numargs", jami_numargs, METH_VARARGS,
     "Return the number of arguments received by the process."},
    {"version", jami_version, METH_VARARGS,
     "Return jami version."},
    {NULL, NULL, 0, NULL}
};

static PyModuleDef jamiModule = {
    PyModuleDef_HEAD_INIT, "jami", NULL, -1, jamiMethods,
    NULL, NULL, NULL, NULL
};

static PyObject*
PyInit_jami(void)
{
    return PyModule_Create(&jamiModule);
}