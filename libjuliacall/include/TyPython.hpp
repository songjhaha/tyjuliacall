#ifndef TYPYTHON_H
#define TYPYTHON_H

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <tyjuliacapi.hpp>
#include <common.hpp>
#include <assert.h>

static void PyCapsule_Destruct_JuliaAsPython(PyObject *capsule)
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

  PyObject *capsule = PyCapsule_New( //把jv对象包装成一个python对象 
      ptr_boxed,
      NULL,
      &PyCapsule_Destruct_JuliaAsPython);

  PyObject *pyjv = PyObject_CallObject(MyPyAPI.t_JV, NULL);//This is the equivalent of the Python expression:callable(arg1, arg2, ...)
  if (pyjv == NULL)
  {
    PyErr_SetString(PyExc_RuntimeError, "box_julia: failed to create a new instance of JV");
    return NULL;
  }

  PyObject_SetAttr(pyjv, name_jlslot, capsule);
  return pyjv;
}

static JV unbox_julia(PyObject *pyjv)
{
  // assume pyjv is a python's JV instance with __jlslot__
  //__jlslot__用于定义对象在转换为字符串时的自定义字符串表示。
  PyObject *capsule = PyObject_GetAttr(pyjv, name_jlslot);
  //用于在一个 Python 对象 pyjv 上获取一个名为 name_jlslot 的属性,并返回该属性的值
  JV *jv = (JV *)PyCapsule_GetPointer(capsule, NULL);
  //函数的作用是从 PyCapsule 对象 capsule 中提取底层的 C 指针，并将其强制类型转换为 (JV *) 类型的指针
  Py_DecRef(capsule);
  return *jv;
}

static JV jl_getattr(PyObject*self,PyObject* args)
 {
    PyObject *pyjv;
    PyObject *item;
    if (!PyArg_ParseTuple(args, "OO", &pyjv, &item))
    {
        return NULL;
    }
    JV slf;
    if (!PyObject_IsInstance(pyjv, MyPyAPI.t_JV))
    {
        PyErr_SetString(JuliaCallError, "jl_getattr: expect object of JV class.");
        return NULL;
    }
    else
    {
        slf = unbox_julia(pyjv);
    }


    if (PyTuple_Check(item))
     {
        // 如果是元组，获取元组的长度
        Py_ssize_t length = PyTuple_Size(item);

        if (length == -1) 
        {
            PyErr_SetString(PyExc_TypeError, "Failed to get tuple size");
            return NULL;
        }

        // 创建一个新的列表来存储解包后的元素
        JV * jv_list = (JV *)malloc(length);
        for (Py_ssize_t i = 0; i < length; i++) {
            PyObject* element = PyTuple_GetItem(item, i);

            // 递归解包元素
            JV unboxed_element;
            unboxed_element = reasonable_unbox(element);
        
            // 将解包后的元素添加到结果列表
            if (unboxed_element != NULL) {
                jv_list[i] = unboxed_element;
                Py_DECREF(unboxed_element); // 减少引用计数
            } else {
                // 错误处理
                Py_DECREF(element); // 释放结果列表
                return NULL;
            }
        }
         


        
    } else {
        // 不是元组，直接返回原始对象
        JV v = reasonable_unbox(item);
        
        
        
        
        
        
        
        
    }
}



// ErrorCode ToJLTupleFromPy(JV *out, PyObject* py)
// {
//     Py_ssize_t i = PyLong_AsSsize_t(py);
//     if (i == -1.0 && PyErr_Occurred() != NULL)
//         return ErrorCode::error;

//     ToJLTuple(out, i);
//     return ErrorCode::ok;
// }


ErrorCode TOJLInt64FromPy(JV *out, PyObject* py)
{
    Py_ssize_t i = PyLong_AsSsize_t(py);
    if (i == -1 && PyErr_Occurred() != NULL)
        return ErrorCode::error;

    ToJLInt64(out, i);
    return ErrorCode::ok;
}

ErrorCode ToJLFloat64FromPy(JV *out, PyObject* py)
{
    double i = PyFloat_AsDouble(py);
    if (i == -1.0 && PyErr_Occurred() != NULL)
        return ErrorCode::error;

    ToJLFloat64(out, i);
    return ErrorCode::ok;
}

ErrorCode ToJLBoolFromPy(JV *out, PyObject* py)
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

ErrorCode ToJLComplexFromPy(JV *out, PyObject* py)
{
    double re = PyComplex_RealAsDouble(py);//Python 复数对象 py 的实部提取为 re 变量的 double 类型值。
    if (re == -1.0 && PyErr_Occurred() != NULL)
        return ErrorCode::error;

    double im = PyComplex_ImagAsDouble(py);//Python 复数对象 py 的虚部提取为 im 变量的 double 类型值。
    if (im == -1.0 && PyErr_Occurred() != NULL)
        return ErrorCode::error;

    ToJLComplexF64(out, complex_t{re, im});//如果成功提取了实部和虚部，它将调用 ToJLComplexF64 函数，将这些值组合成 Julia 中的复数对象，并存储在 out 中。
    return ErrorCode::ok;
}

ErrorCode ToJLStringFromPy(JV *out, PyObject* py)
{
    Py_ssize_t len;
    const char* str = PyUnicode_AsUTF8AndSize(py, &len);
    if (str == NULL)
        return ErrorCode::error;

    ToJLString(out, SList_adapt(reinterpret_cast<uint8_t *>(const_cast<char *>(str)), len));
    return ErrorCode::ok;
}

ErrorCode ToJLNothingFromPy(JV *out, PyObject* py)
{
    if (py != Py_None)
        return ErrorCode::error;
    else
    {
        *out = MyJLAPI.obj_nothing;
        return ErrorCode::ok;
    }
}

JV reasonable_unbox(PyObject* py)
{
    if (py == Py_None)
        return MyJLAPI.obj_nothing;

    if (PyObject_IsInstance(py, MyPyAPI.t_JV))
        return unbox_julia(py);

    JV out;
    if (PyObject_IsInstance(py, MyPyAPI.t_int))
    {
        TOJLInt64FromPy(&out, py);
        return out;
    }

    if (PyObject_IsInstance(py, MyPyAPI.t_float))
    {
        ToJLFloat64FromPy(&out, py);
        return out;
    }

    if (PyObject_IsInstance(py, MyPyAPI.t_str))
    {
        ToJLStringFromPy(&out, py);
        return out;
    }

    if (PyObject_IsInstance(py, MyPyAPI.t_bool))
    {
        ToJLBoolFromPy(&out, py);
        return out;
    }

    if (PyObject_IsInstance(py, MyPyAPI.t_complex))
    {
        ToJLComplexFromPy(&out, py);
        return out;
    }

    if (PyObject_IsInstance(py, MyPyAPI.t_ndarray))
    {
        ErrorCode ret = pycast2jl(&out, MyJLAPI.t_AbstractArray, py);
        if (ret == ErrorCode::ok)
        {
            return out;
        }
        else
        {
            // todo: string array
        }
    }

    // todo: python's tuple
    if (PyObject_IsInstance(py, MyPyAPI.t_tuple))
    {


    }


    PyErr_SetString(JuliaCallError, "unbox failed: cannot convert a Python object to Julia object");
    return JV_NULL;
}

// PyObject* UnsafePyLongFromJL(JV jv)
// {
//     int64_t i;
//     JLGetInt64(&i, jv, true);
//     return PyLong_FromLongLong(i);
// }

// PyObject* UnsafePyDoubleFromJL(JV jv)
// {
//     double i;
//     JLGetDouble(&i, jv, true);
//     return PyFloat_FromDouble(i);
// }

// PyObject* UnsafePyBoolFromJL(JV jv)
// {
//     bool8_t i;
//     JLGetBool(&i, jv, true);
//     return PyBool_FromLong(i);
// }

// PyObject* UnsafePyStringFromJL(JV jv)
// {
//     JV ncode;
//     JLCall(&ncode, MyJLAPI.f_ncodeunits, SList_adapt(&jv, 1), emptyKwArgs());
//     int64_t len;
//     JLGetInt64(&len, ncode, true);
//     JLFreeFromMe(ncode);

//     if (len == 0)
//         return PyUnicode_FromString("");
//     else
//     {
//         // this copy the string twice
//         char *buf = (char *)malloc(len + 1);
//         JLGetUTF8String(SList_adapt(reinterpret_cast<uint8_t *>(const_cast<char *>(buf)), len), jv);
//         buf[len] = '\0';
//         PyObject* str = PyUnicode_FromString(buf);
//         free(buf);
//         return str;
//     }
// }


PyObject * reasonable_box(JV jv)
{
    PyObject* py;

    if (JLIsInstanceWithTypeSlot(jv, MyJLAPI.t_Nothing))
    {
        Py_IncRef(Py_None);
        return Py_None;
    }

    if (JLIsInstanceWithTypeSlot(jv, MyJLAPI.t_Integer))
    {
        py = pycast2py(jv);
        return py;
    }

    if (JLIsInstanceWithTypeSlot(jv, MyJLAPI.t_AbstractFloat))
    {
        py = pycast2py(jv);
        return py;
    }

    if (JLIsInstanceWithTypeSlot(jv, MyJLAPI.t_Bool))
    {
        py = pycast2py(jv);
        return py;
    }

    if (JLIsInstanceWithTypeSlot(jv, MyJLAPI.t_Complex))
    {
        py = pycast2py(jv);
        return py;
    }

    if (JLIsInstanceWithTypeSlot(jv, MyJLAPI.t_String))
    {
        py = pycast2py(jv);
        return py;
    }

    if (JLIsInstanceWithTypeSlot(jv, MyJLAPI.t_Number))
    {
        py = pycast2py(jv);
        return py;
    }

    if (JLIsInstanceWithTypeSlot(jv, MyJLAPI.t_AbstractArray))
    {
        // todo
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

    // todo: tuple
    if (JLIsInstanceWithTypeSlot(jv, MyJLAPI.t_Tuple))
    {

    


    }





    

    return box_julia(jv);
}

#endif
