#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <tyjuliacapi.hpp>
#include <TyPython.hpp>
#include <common.hpp>
#include <assert.h>
#include <time.h>

DLLEXPORT int init_libjuliacall(void *lpfnJLCApiGetter, void *lpfnPyCast2JL, void *lpfnPyCast2Py)
{
  if (pycast2jl != NULL && pycast2py != NULL)
  {
    return 0;
  }

  if (library_init((_get_capi_t)(lpfnJLCApiGetter)) == 0)
  {
    printf("library_init failed: TyJuliaCAPI is invalid\n");
    return 1;
  }

  pycast2jl = (t_pycast2jl)lpfnPyCast2JL;
  pycast2py = (t_pycast2py)lpfnPyCast2Py;

  return 0;
}



static PyObject *jl_eval(PyObject *self, PyObject *args)
{
  const char *_command;
  JV result;
  if (!PyArg_ParseTuple(args, "s", &_command))
  {
    return NULL;
  }
  char *command = const_cast<char *>(_command);
  ErrorCode ret = JLEval(&result, NULL, SList_adapt(reinterpret_cast<uint8_t *>(command), strlen(command))); 
  if (ret != ErrorCode::ok)
  {
    return HandleJLErrorAndReturnNULL(); // 如果是错误的话，则处理
  }
  return box_julia(result);
}

static PyObject *jl_square(PyObject *self, PyObject *args)
{
  JV v = reasonable_unbox(args);

  JV jret;
  ErrorCode ret = JLCall(&jret, MyJLAPI.f_square, SList_adapt(&v, 1), emptyKwArgs());
  if (ret != ErrorCode::ok)
  {
    return HandleJLErrorAndReturnNULL();
  }

  PyObject *py = reasonable_box(jret); // jret变成python类型
  JLFreeFromMe(jret);
  return py;
}

static PyObject *jl_display(PyObject *self, PyObject *arg)
{
  // check arg type
  if (!PyObject_IsInstance(arg, MyPyAPI.t_JV))
  {
    PyErr_SetString(JuliaCallError, "jl_display: expect object of JV class.");
    return NULL;
  }

  // unbox jv from arg (use unbox_julia)
  JV jv = unbox_julia(arg);
  JV jret;

  // call julia function repr
  ErrorCode ret = JLCall(&jret, MyJLAPI.f_repr, SList_adapt(&jv, 1), emptyKwArgs());
  if (ret != ErrorCode::ok)
  {
    return HandleJLErrorAndReturnNULL();
  }

  // convert Julia's String to Python's str.
  // pycast2py is a C Function from julia,
  // we could use JLGetUTF8String and PyUnicode_FromString instead,
  // but this one is simple.
  PyObject *pyjv = pycast2py(jret);

  // free this Julia String
  JLFreeFromMe(jret);
  return pyjv;
}

static PyObject *jl_getattr1(PyObject *self, PyObject *args)
{
  PyObject *pyjv;
  const char *attr;
  if (!PyArg_ParseTuple(args, "Os", &pyjv, &attr))
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

  JSym sym;
  JSymFromString(&sym, attr);

  JV out;
  ErrorCode ret = JLGetProperty(&out, slf, sym);
  if (ret != ErrorCode::ok)
  {
    return HandleJLErrorAndReturnNULL();
  }

  PyObject *pyout = reasonable_box(out);
  if (!PyObject_IsInstance(pyout, MyPyAPI.t_JV))
  {
    // if pyout is a JV object, we should not free it from Julia.
    JLFreeFromMe(out);
  }
  return pyout;
}

static PyObject *jl_setattr(PyObject *self, PyObject *args)
{
  // jl_setattr(self: JV, attr: str, value)
  // 1. check args type, we should get 3 args: PyObject* pyjv, const char* attr, PyObject* value
  PyObject *pyjv;
  PyObject *value;
  const char *attr;
  if (!PyArg_ParseTuple(args, "OsO", &pyjv, &attr, &value))
  {
    return NULL;
  }
  // 2. check pyjv is a JV object, and unbox it as JV
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
  // 3. unbox value as JV
  JV v = reasonable_unbox(value);
  // 4. call JLSetProperty
  JSym sym;
  JSymFromString(&sym, attr);
  ErrorCode ret = JLSetProperty(slf, sym, v);
  // 5. check if error occurs, if so, handle it and return NULL
  if (ret != ErrorCode::ok)
  {
    return HandleJLErrorAndReturnNULL();
  }

  if (!PyObject_IsInstance(value, MyPyAPI.t_JV))
  {
    // if pyout is a JV object, we should not free it from Julia.
    JLFreeFromMe(v);
  }
  // 6. if success, return Py_None
  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *jl_getitem(PyObject *self, PyObject *args)
{
  PyObject *pyjv;
  PyObject *item;
  if (!PyArg_ParseTuple(args, "OO", &pyjv, &item))
  {
    return NULL;
  }
  // check pyjv is a JV object, and unbox it as JV
  JV slf;
  if (!PyObject_IsInstance(pyjv, MyPyAPI.t_JV))
  {
    PyErr_SetString(JuliaCallError, "jl_getitem: expect object of JV class.");
    return NULL;
  }
  else
  {
    slf = unbox_julia(pyjv);
  }

  //如果是元组，获取好几个元素
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
    for (Py_ssize_t i = 0; i < length; i++)
    {
      PyObject* element = PyTuple_GetItem(item, i);
      JV unboxed_element;
      unboxed_element = reasonable_unbox(element);

      // 将解包后的元素添加到结果列表
      if (unboxed_element != MyJLAPI.t_Nothing)
      {
        JV jret;
        JV jargs[2];
        jargs[0] = slf;
        jargs[1] = unboxed_element;
        ErrorCode ret = JLCall(&jret, MyJLAPI.f_getindex, SList_adapt(jargs,2), emptyKwArgs());
        if (ret != ErrorCode::ok)
        {
          return HandleJLErrorAndReturnNULL();
        }
        jv_list[i] = jret;
      } else {
          // 错误处理
          Py_DECREF(element); // 释放结果列表
          return NULL;
      }
    }
    JV jret1;
    ErrorCode ret1 = JLCall(&jret1, MyJLAPI.t_Tuple, SList_adapt(jv_list,length), emptyKwArgs());
    if (ret1 != ErrorCode::ok)
    {
      return HandleJLErrorAndReturnNULL();
    }
    PyObject *py = reasonable_box(jret1);
    if (!PyObject_IsInstance(py, MyPyAPI.t_JV))
    {
      // if pyout is a JV object, we should not free it from Julia.
      JLFreeFromMe(jret1);
    }
    return py;
  }
  else
  {
    JV jret;
    JV v = reasonable_unbox(item);
    JV jargs[2];
    jargs[0] = slf;
    jargs[1] = v;
    ErrorCode ret2 = JLCall(&jret, MyJLAPI.f_getindex, SList_adapt(jargs,2), emptyKwArgs());
    if (ret2 != ErrorCode::ok)
    {
      return HandleJLErrorAndReturnNULL();
    }
    if (!PyObject_IsInstance(item, MyPyAPI.t_JV))
    {
      // if pyout is a JV object, we should not free it from Julia.
      JLFreeFromMe(v);
    }
    PyObject *py = reasonable_box(jret);
    if (!PyObject_IsInstance(py, MyPyAPI.t_JV))
    {
      // if pyout is a JV object, we should not free it from Julia.
      JLFreeFromMe(jret);
    }
    return py;
  }
}


static PyObject *jl_arithmetic_operation(PyObject *self, PyObject *args, JV f)
{
  // 1. check args type
  PyObject *pyjv;
  PyObject *value;
  if (!PyArg_ParseTuple(args, "OO", &pyjv, &value))
  {
    return NULL;
  }
  // 2. check pyjv is a JV object, and unbox it as JV
  JV slf;
  if (!PyObject_IsInstance(pyjv, MyPyAPI.t_JV))
  {
    PyErr_SetString(JuliaCallError, "expect object of JV class.");
    return NULL;
  }
  else
  {
    slf = unbox_julia(pyjv);
  }
  // 3. unbox value as JV
  JV v = reasonable_unbox(value);
  // 4. call JLCallS
  JV jret;
  JV jargs[2];
  jargs[0] = slf;
  jargs[1] = v;

  ErrorCode ret;
  ret = JLCall(&jret, f, SList_adapt(jargs, 2), emptyKwArgs());
  // 5. check if error occurs, if so, handle it and return NULL
  if (ret != ErrorCode::ok)
  {
    return HandleJLErrorAndReturnNULL();
  }

  if (!PyObject_IsInstance(value, MyPyAPI.t_JV))
  {
    // if pyout is a JV object, we should not free it from Julia.
    JLFreeFromMe(v);
  }
  PyObject *py = reasonable_box(jret);
  if (!PyObject_IsInstance(py, MyPyAPI.t_JV))
  {
    // if pyout is a JV object, we should not free it from Julia.
    JLFreeFromMe(jret);
  }
  return py;
}

static PyObject *jl_add(PyObject *self, PyObject *args)
{
  return jl_arithmetic_operation(self, args, MyJLAPI.f_add);
}

static PyObject *jl_sub(PyObject *self, PyObject *args)
{
  return jl_arithmetic_operation(self, args, MyJLAPI.f_sub);
}

static PyObject *jl_mul(PyObject *self, PyObject *args)
{
  return jl_arithmetic_operation(self, args, MyJLAPI.f_mul);
}

static PyObject *jl_matmul(PyObject *self, PyObject *args)
{
  return jl_arithmetic_operation(self, args, MyJLAPI.f_matmul);
}

static PyObject *jl_truediv(PyObject *self, PyObject *args)
{
  return jl_arithmetic_operation(self, args, MyJLAPI.f_truediv);
}

static PyObject *jl_floordiv(PyObject *self, PyObject *args)
{
  return jl_arithmetic_operation(self, args, MyJLAPI.f_floordiv);
}

static PyObject *jl_mod(PyObject *self, PyObject *args)
{
  return jl_arithmetic_operation(self, args, MyJLAPI.f_mod);
}

static PyObject *jl_pow(PyObject *self, PyObject *args)
{
  return jl_arithmetic_operation(self, args, MyJLAPI.f_pow);
}

static PyObject *jl_lshift(PyObject *self, PyObject *args)
{
  return jl_arithmetic_operation(self, args, MyJLAPI.f_lshift);
}

static PyObject *jl_rshift(PyObject *self, PyObject *args)
{
  return jl_arithmetic_operation(self, args, MyJLAPI.f_rshift);
}

static PyObject *jl_bitor(PyObject *self, PyObject *args)
{
  return jl_arithmetic_operation(self, args, MyJLAPI.f_bitor);
}

static PyObject *jl_bitxor(PyObject *self, PyObject *args)
{
  return jl_arithmetic_operation(self, args, MyJLAPI.f_bitxor);
}

static PyObject *jl_bitand(PyObject *self, PyObject *args)
{
  return jl_arithmetic_operation(self, args, MyJLAPI.f_bitand);
}

static PyObject *jl_eq(PyObject *self, PyObject *args)
{
  return jl_arithmetic_operation(self, args, MyJLAPI.f_eq);
}

static PyObject *jl_ne(PyObject *self, PyObject *args)
{
  return jl_arithmetic_operation(self, args, MyJLAPI.f_ne);
}

static PyObject *jl_lt(PyObject *self, PyObject *args)
{
  return jl_arithmetic_operation(self, args, MyJLAPI.f_lt);
}

static PyObject *jl_le(PyObject *self, PyObject *args)
{
  return jl_arithmetic_operation(self, args, MyJLAPI.f_le);
}

static PyObject *jl_gt(PyObject *self, PyObject *args)
{
  return jl_arithmetic_operation(self, args, MyJLAPI.f_gt);
}

static PyObject *jl_ge(PyObject *self, PyObject *args)
{
  return jl_arithmetic_operation(self, args, MyJLAPI.f_ge);
}

static PyObject *jl_contains(PyObject *self, PyObject *args)
{
  return jl_arithmetic_operation(self, args, MyJLAPI.f_in);
}

static PyObject *jl_unary_opertation(PyObject *self, PyObject *args, JV f)
{
  // 1. check pyjv is a JV object, and unbox it as JV
  JV slf;
  if (!PyObject_IsInstance(args, MyPyAPI.t_JV))
  {
    PyErr_SetString(JuliaCallError, "expect object of JV class.");
    return NULL;
  }
  else
  {
    slf = unbox_julia(args);
  }
  // 3. call JLCallS
  JV jret;
  ErrorCode ret;
  ret = JLCall(&jret, f, SList_adapt(&slf, 1), emptyKwArgs());
  // 4. check if error occurs, if so, handle it and return NULL
  if (ret != ErrorCode::ok)
  {
    return HandleJLErrorAndReturnNULL();
  }
  PyObject *py = reasonable_box(jret);
  if (!PyObject_IsInstance(py, MyPyAPI.t_JV))
  {
    // if pyout is a JV object, we should not free it from Julia.
    JLFreeFromMe(jret);
  }
  return py;
}

static PyObject *jl_invert(PyObject *self, PyObject *args)
{
  return jl_unary_opertation(self, args, MyJLAPI.f_invert);
}

static PyObject *jl_pos(PyObject *self, PyObject *args)
{
  return jl_unary_opertation(self, args, MyJLAPI.f_add);
}

static PyObject *jl_neg(PyObject *self, PyObject *args)
{
  return jl_unary_opertation(self, args, MyJLAPI.f_sub);
}

static PyObject *jl_abs(PyObject *self, PyObject *args)
{
  return jl_unary_opertation(self, args, MyJLAPI.f_abs);
}

static PyObject *jl_bool(PyObject *self, PyObject *args)
{
  //  1. check pyjv is a JV object, and unbox it as JV
  JV slf;
  if (!PyObject_IsInstance(args, MyPyAPI.t_JV))
  {
    PyErr_SetString(JuliaCallError, " expect object of JV class.");
    return NULL;
  }
  else
  {
    slf = unbox_julia(args);
  }
  // 2. check isa Number
  PyObject *py;
  if (JLIsInstanceWithTypeSlot(slf, MyJLAPI.t_Number))
  {
    JV jret;
    JV jargs[2];
    jargs[0] = slf;
    jargs[1] = MyJLAPI.obj_zero;
    ErrorCode ret;
    ret = JLCall(&jret, MyJLAPI.f_ne, SList_adapt(jargs, 2), emptyKwArgs());
    if (ret != ErrorCode::ok)
    {
      return HandleJLErrorAndReturnNULL();
    }
    PyObject *py = reasonable_box(jret);
    JLFreeFromMe(jret); 
    return py;
  }
  // 3.检查是不是抽象数组 抽象字典 抽象集合 抽象字符串
  if (JLIsInstanceWithTypeSlot(slf, MyJLAPI.t_AbstractArray) ||
      JLIsInstanceWithTypeSlot(slf, MyJLAPI.t_AbstractDict) || JLIsInstanceWithTypeSlot(slf, MyJLAPI.t_AbstractSet) ||
      JLIsInstanceWithTypeSlot(slf, MyJLAPI.t_AbstractString))
  {
    JV jret;
    ErrorCode ret;
    ret = JLCall(&jret, MyJLAPI.f_isempty, SList_adapt(&slf, 1), emptyKwArgs());
    if (ret != ErrorCode::ok)
    {
      return HandleJLErrorAndReturnNULL();
    }
    PyObject *py = reasonable_box(jret);
    if (!PyObject_IsInstance(py, MyPyAPI.t_JV))
    {
      // if pyout is a JV object, we should not free it from Julia.
      JLFreeFromMe(jret);
    }
    return py;
  }
  Py_INCREF(Py_True); // 增加引用计数，以防止被销毁
  return Py_True;
}

static PyObject *jl_hash(PyObject *self, PyObject *args)
{
  // 1. check pyjv is a JV object, and unbox it as JV
  JV slf;
  if (!PyObject_IsInstance(args, MyPyAPI.t_JV))
  {
    PyErr_SetString(JuliaCallError, " expect object of JV class.");
    return NULL;
  }
  else
  {
    slf = unbox_julia(args);
  }
  // 2. call JLCallS
  JV jret;
  ErrorCode ret;
  ret = JLCall(&jret, MyJLAPI.f_hash, SList_adapt(&slf, 1), emptyKwArgs());
  // 3. check if error occurs, if so, handle it and return NULL
  if (ret != ErrorCode::ok)
  {
    return HandleJLErrorAndReturnNULL();
  }
  ErrorCode ret1;
  uint64_t result; 
  ret1 = JLGetUInt64(&result,jret,true);
  if (ret1 != ErrorCode::ok)
  {
    return HandleJLErrorAndReturnNULL();
  }
  PyObject *py = PyLong_FromUnsignedLongLong(result);
  if (!PyObject_IsInstance(py, MyPyAPI.t_JV))
    {
      // if pyout is a JV object, we should not free it from Julia.
      JLFreeFromMe(jret);
    }
  return py;                   
}


static PyMethodDef jl_methods[] = {
    {"__jl_repr__", jl_display, METH_O, "display JV as string"},
    {"__jl_getattr__", jl_getattr1, METH_VARARGS, "get attr of JV object"},
    {"__jl_setattr__", jl_setattr, METH_VARARGS, "set attr of JV object"},
    {"__jl_getitem__", jl_getitem, METH_VARARGS, "get item of JV object"},
    {"__jl_add__", jl_add, METH_VARARGS, "add function"},
    {"__jl_sub__", jl_sub, METH_VARARGS, "sub function"},
    {"__jl_mul__", jl_mul, METH_VARARGS, "mul function"},
    {"__jl_matmul__", jl_matmul, METH_VARARGS, "matmul function"},
    {"__jl_truediv__", jl_truediv, METH_VARARGS, "truediv function"},
    {"__jl_floordiv__", jl_floordiv, METH_VARARGS, "floordiv function"},
    {"__jl_mod__", jl_mod, METH_VARARGS, "mod function"},
    {"__jl_pow__", jl_pow, METH_VARARGS, "pow function"},
    {"__jl_lshift__", jl_lshift, METH_VARARGS, "lshift function"},
    {"__jl_rshift__", jl_rshift, METH_VARARGS, "rshift function"},
    {"__jl_bitor__", jl_bitor, METH_VARARGS, "bitor function"},
    {"__jl_bitxor__", jl_bitxor, METH_VARARGS, "bitxor function"},
    {"__jl_bitand__", jl_bitand, METH_VARARGS, "bitand function"},
    {"__jl_eq__", jl_eq, METH_VARARGS, "eq function"},
    {"__jl_ne__", jl_ne, METH_VARARGS, "ne function"},
    {"__jl_lt__", jl_lt, METH_VARARGS, "lt function"},
    {"__jl_le__", jl_le, METH_VARARGS, "le function"},
    {"__jl_gt__", jl_gt, METH_VARARGS, "gt function"},
    {"__jl_ge__", jl_ge, METH_VARARGS, "ge function"},
    {"__jl_contains__", jl_contains, METH_VARARGS, "contains function"},
    {"__jl_invert__", jl_invert, METH_O, "invert function"},
    {"__jl_pos__", jl_pos, METH_O, "pos function"},
    {"__jl_neg__", jl_neg, METH_O, "neg function"},
    {"__jl_abs__", jl_abs, METH_O, "abs function"},
    {"__jl_bool__", jl_bool, METH_O, "bool function"},
    {"__jl_hash__", jl_hash, METH_O, "hash function"},
    {NULL, NULL, 0, NULL}};

static PyObject *setup_api(PyObject *self, PyObject *args)
{
  // check arg type
  PyObject *cls_jv;
  PyObject *m_jv;
  if (!PyArg_ParseTuple(args, "OO", &cls_jv, &m_jv))
  {
    return NULL;
  }


  if (MyPyAPI.t_JV == NULL)
  {
    Py_IncRef(cls_jv); // Py_IncRef 函数用于增加 Python 对象的引用计数。
    init_JLAPI();
    init_PyAPI(cls_jv); // 自定义函数或库初始化函数的调用。
    PyModule_AddFunctions(m_jv, jl_methods);
  }

  Py_INCREF(Py_None);
  return Py_None;
}

static PyMethodDef methods[] = {
    {"setup_api", setup_api, METH_VARARGS, "setup JV class and init MyPyAPI/MyJLAPI"},
    {"jl_square", jl_square, METH_O, "Square function"},
    {"jl_eval", jl_eval, METH_VARARGS, "eval julia function and return a python capsule"},
    {NULL, NULL, 0, NULL}};

static struct PyModuleDef juliacall_module = {PyModuleDef_HEAD_INIT, "_tyjuliacall_jnumpy",
                                              NULL, -1, methods};

DLLEXPORT PyObject *init_PyModule(void)
{
  name_jlslot = PyUnicode_FromString("__jlslot__");
  JuliaCallError = PyErr_NewException("_tyjuliacall_jnumpy.error", NULL, NULL);
  PyObject *m = PyModule_Create(&juliacall_module);
  PyObject *sys = PyImport_ImportModule("sys");
  PyObject *sys_module = PyObject_GetAttrString(sys, "modules");
  Py_IncRef(m);
  PyDict_SetItemString(sys_module, "_tyjuliacall_jnumpy", m);
  Py_DecRef(sys_module);
  Py_DECREF(sys);
  return m;
}
