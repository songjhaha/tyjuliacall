#ifndef TYPYTHON_H
#define TYPYTHON_H

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <tyjuliacapi.hpp>
#include <common.hpp>
#include <assert.h>
#include <stdlib.h>

JV reasonable_unbox(PyObject *py, bool8_t *needToBeFree);

static int PyCheck_Type_Exact(PyObject *py, PyObject *type)
{
    if ((PyObject *)Py_TYPE(py) == type)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

static void
PyCapsule_Destruct_JuliaAsPython(PyObject *capsule)
{
    // destruct of capsule(__jlslot__)
    JV *jv = (JV *)PyCapsule_GetPointer(capsule, NULL);
    JLFreeFromMe(*jv);
    free(jv);
}

static PyObject *box_julia(JV jv)
{
    // JV(julia value) -> PyObject(python's JV with __jlslot__)
    if (jv == JV_NULL)
    {
        PyErr_SetString(PyExc_RuntimeError, "box_julia: failed to create a new instance of JV");
        return NULL;
    }

    JV *ptr_boxed = (JV *)malloc(sizeof(JV));
    *ptr_boxed = jv;

    PyObject *capsule = PyCapsule_New( // 把jv对象包装成一个python对象
        ptr_boxed,
        NULL,
        &PyCapsule_Destruct_JuliaAsPython);

    PyObject *pyjv = PyObject_CallObject(MyPyAPI.t_JV, NULL); // This is the equivalent of the Python expression:callable(arg1, arg2, ...)
    if (pyjv == NULL)
    {
        PyErr_SetString(PyExc_RuntimeError, "box_julia: failed to create a new instance of JV");
        return NULL;
    }

    PyObject_GenericSetAttr(pyjv, name_jlslot, capsule);
    return pyjv;
}

static JV unbox_julia(PyObject *pyjv)
{
    // assume pyjv is a python's JV instance with __jlslot__
    //__jlslot__用于定义对象在转换为字符串时的自定义字符串表示。
    PyObject *capsule = PyObject_GetAttr(pyjv, name_jlslot);
    // 用于在一个 Python 对象 pyjv 上获取一个名为 name_jlslot 的属性,并返回该属性的值
    JV *jv = (JV *)PyCapsule_GetPointer(capsule, NULL);
    // 函数的作用是从 PyCapsule 对象 capsule 中提取底层的 C 指针，并将其强制类型转换为 (JV *) 类型的指针
    Py_DecRef(capsule);
    return *jv;
}

void free_jv_list(JV *jv_list, bool8_t *jv_list_tobefree, int length)
{
    for (int i = 0; i < length; i++)
    {
        if (jv_list_tobefree[i])
        {
            JLFreeFromMe(jv_list[i]);
        }
    }
    free(jv_list);
    free(jv_list_tobefree);
}

ErrorCode ToJListFromPyTuple(JV *out_list, bool8_t *jv_tobefree, PyObject *py, Py_ssize_t length)
{
    bool8_t *ptr_tobefree = jv_tobefree;
    for (Py_ssize_t i = 0; i < length; i++)
    {
        PyObject *element = PyTuple_GetItem(py, i);
        if (element == NULL)
            return ErrorCode::error;
        // 将解包后的元素添加到列表
        JV unbox_element = reasonable_unbox(element, ptr_tobefree);
        if (unbox_element == JV_NULL)
        {
            return ErrorCode::error;
        }
        else
        {
            out_list[i] = unbox_element;
        }
        ptr_tobefree++;
    }

    return ErrorCode::ok;
}

ErrorCode ToJLTupleFromPy(JV *out, PyObject *py)
{
    // 如果是元组，获取元组的长度
    Py_ssize_t length = PyTuple_Size(py);

    // 创建一个新的列表来存储解包后的元素
    // 如果py是python tuple的话，逐个 unbox 成 arg1, arg2, ..., argN
    // getindex(self, arg1, arg2, ..., argN)
    JV *jv_list = (JV *)calloc(length, sizeof(JV));
    bool8_t *jv_tobefree = (bool8_t *)calloc(length, sizeof(bool8_t));
    ErrorCode ret = ToJListFromPyTuple(jv_list, jv_tobefree, py, length);

    ret = JLCall(out, MyJLAPI.f_tuple, SList_adapt(jv_list, length), emptyKwArgs());
    free_jv_list(jv_list, jv_tobefree, length);
    return ret;
}

ErrorCode ToJLSymFromPyStr(JSym *out, PyObject *py)
{
    PyObject *pybytes = PyUnicode_AsUTF8String(py);
    if (pybytes == NULL)
        return ErrorCode::error;

    char *str = PyBytes_AsString(pybytes);
    JSymFromString(out, SList_adapt(reinterpret_cast<uint8_t *>(str), strlen(str)));
    return ErrorCode::ok;
}

ErrorCode TOJLInt64FromPy(JV *out, PyObject *py)
{
    Py_ssize_t i = PyLong_AsSsize_t(py);
    if (i == -1 && PyErr_Occurred() != NULL)
        return ErrorCode::error;

    ToJLInt64(out, i);
    return ErrorCode::ok;
}

ErrorCode ToJLFloat64FromPy(JV *out, PyObject *py)
{
    double i = PyFloat_AsDouble(py);
    if (i == -1.0 && PyErr_Occurred() != NULL)
        return ErrorCode::error;

    ToJLFloat64(out, i);
    return ErrorCode::ok;
}

ErrorCode ToJLBoolFromPy(JV *out, PyObject *py)
{
    if (py == Py_True)
    {
        ToJLBool(out, 1);
    }
    else if (py == Py_False)
    {
        ToJLBool(out, 0);
    }
    else
    {
        int i = PyObject_IsTrue(py);
        bool8_t flag = (i != 0);
        ToJLBool(out, flag);
    }
    return ErrorCode::ok;
}

ErrorCode ToJLComplexFromPy(JV *out, PyObject *py)
{
    double re = PyComplex_RealAsDouble(py); // Python 复数对象 py 的实部提取为 re 变量的 double 类型值。
    if (re == -1.0 && PyErr_Occurred() != NULL)
        return ErrorCode::error;

    double im = PyComplex_ImagAsDouble(py); // Python 复数对象 py 的虚部提取为 im 变量的 double 类型值。
    if (im == -1.0 && PyErr_Occurred() != NULL)
        return ErrorCode::error;

    ToJLComplexF64(out, complex_t{re, im}); // 如果成功提取了实部和虚部，它将调用 ToJLComplexF64 函数，将这些值组合成 Julia 中的复数对象，并存储在 out 中。
    return ErrorCode::ok;
}

ErrorCode ToJLStringFromPy(JV *out, PyObject *py)
{
    // stable api in python 3.7
    PyObject *pybytes = PyUnicode_AsUTF8String(py);
    if (pybytes == NULL)
        return ErrorCode::error;

    char *str = PyBytes_AsString(pybytes);
    ToJLString(out, SList_adapt(reinterpret_cast<uint8_t *>(str), strlen(str)));
    return ErrorCode::ok;
}

ErrorCode ToJLNothingFromPy(JV *out, PyObject *py)
{
    if (py != Py_None)
        return ErrorCode::error;
    else
    {
        *out = MyJLAPI.obj_nothing;
        return ErrorCode::ok;
    }
}

JV reasonable_unbox(PyObject *py, bool8_t *needToBeFree)
{
    if (py == Py_None)
        return MyJLAPI.obj_nothing;

    if (PyCheck_Type_Exact(py, MyPyAPI.t_JV))
        return unbox_julia(py);

    JV out;
    if (PyCheck_Type_Exact(py, MyPyAPI.t_int))
    {
        *needToBeFree = true;
        TOJLInt64FromPy(&out, py);
        return out;
    }

    if (PyCheck_Type_Exact(py, MyPyAPI.t_float))
    {
        *needToBeFree = true;
        ToJLFloat64FromPy(&out, py);
        return out;
    }

    if (PyCheck_Type_Exact(py, MyPyAPI.t_str))
    {
        *needToBeFree = true;
        ToJLStringFromPy(&out, py);
        return out;
    }

    if (PyCheck_Type_Exact(py, MyPyAPI.t_bool))
    {
        *needToBeFree = true;
        ToJLBoolFromPy(&out, py);
        return out;
    }

    if (PyCheck_Type_Exact(py, MyPyAPI.t_complex))
    {
        *needToBeFree = true;
        ToJLComplexFromPy(&out, py);
        return out;
    }

    if (PyCheck_Type_Exact(py, MyPyAPI.t_ndarray))
    {
        *needToBeFree = true;
        ErrorCode ret = pycast2jl(&out, MyJLAPI.t_AbstractArray, py);
        if (ret == ErrorCode::ok)
        {
            return out;
        }
        else
        {
            // TODO(sjh): string array
            return out;
        }
    }

    if (PyCheck_Type_Exact(py, MyPyAPI.t_tuple))
    {
        ErrorCode ret = ToJLTupleFromPy(&out, py);
        if (ret == ErrorCode::ok)
        {
            return out;
        }
        else
        {
            return JV_NULL;
        }
    }

    PyErr_SetString(JuliaCallError, "unbox failed: cannot convert a Python object to Julia object");
    return JV_NULL;
}

PyObject *reasonable_box(JV jv)
{
    PyObject *py;

    if (JLIsInstanceWithTypeSlot(jv, MyJLAPI.t_Nothing))
    {
        Py_IncRef(Py_None);
        return Py_None;
    }

    if (JLIsInstanceWithTypeSlot(jv, MyJLAPI.t_Integer))
    {
        py = pycast2py(jv);
        if (py != NULL)
            return py;
    }

    if (JLIsInstanceWithTypeSlot(jv, MyJLAPI.t_AbstractFloat))
    {
        py = pycast2py(jv);
        if (py != NULL)
            return py;
    }

    if (JLIsInstanceWithTypeSlot(jv, MyJLAPI.t_Bool))
    {
        py = pycast2py(jv);
        if (py != NULL)
            return py;
    }

    if (JLIsInstanceWithTypeSlot(jv, MyJLAPI.t_Complex))
    {
        py = pycast2py(jv);
        if (py != NULL)
            return py;
    }

    if (JLIsInstanceWithTypeSlot(jv, MyJLAPI.t_String))
    {
        py = pycast2py(jv);
        if (py != NULL)
            return py;
    }

    if (JLIsInstanceWithTypeSlot(jv, MyJLAPI.t_Number))
    {
        py = pycast2py(jv);
        if (py != NULL)
            return py;
    }

    if (JLIsInstanceWithTypeSlot(jv, MyJLAPI.t_AbstractArray))
    {
        if (JLIsInstanceWithTypeSlot(jv, MyJLAPI.t_BitArray))
        {
            py = box_julia(jv);
        }
        else
        {
            py = pycast2py(jv);
            if (py == NULL) /* on fail */
                py = box_julia(jv);
        }
        return py;
    }

    if (JLIsInstanceWithTypeSlot(jv, MyJLAPI.t_Tuple))
    {
        JV jv_N;
        ErrorCode ret0 = JLCall(&jv_N, MyJLAPI.f_length, SList_adapt(&jv, 1), emptyKwArgs());
        if (ret0 != ErrorCode::ok)
        {
            return HandleJLErrorAndReturnNULL();
        }

        int64_t N;
        JLGetInt64(&N, jv_N, true);
        JLFreeFromMe(jv_N);
        PyObject *argtuple = PyTuple_New(N);

        for (int64_t i = 0; i < N; i++)
        {
            JV v;
            ErrorCode ret1 = JLGetIndexI(&v, jv, i + 1);
            if (ret1 != ErrorCode::ok)
            {
                return HandleJLErrorAndReturnNULL();
            }
            PyObject *arg = reasonable_box(v);
            if (!PyCheck_Type_Exact(arg, MyPyAPI.t_JV))
            {
                JLFreeFromMe(v);
            }
            // reasonable_box should always return a new reference
            // Py_INCREF(arg);
            PyTuple_SetItem(argtuple, i, arg);
        }
        return argtuple;
    }

    if (JLIsInstanceWithTypeSlot(jv, MyJLAPI.t_AbstractString))
    {
        JV jv_str;
        JV jv_arg[2];
        jv_arg[0] = MyJLAPI.obj_String;
        jv_arg[1] = jv;
        if (ErrorCode::ok == JLCall(&jv_str, MyJLAPI.f_convert, SList_adapt(jv_arg, 2), emptyKwArgs()))
        {
            py = pycast2py(jv_str);
            JLFreeFromMe(jv_str);
            if (py != NULL)
                return py;
        }
    }

    return box_julia(jv);
}

#endif
