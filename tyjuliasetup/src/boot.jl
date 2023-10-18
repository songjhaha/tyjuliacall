using Libdl
using TyJuliaCAPI
import TyPython.CPython
import TyPython.CPython: Py, py_cast, UnsafeNew, PyObject, BorrowReference

const LibJuliaCall = Ref{Ptr{Cvoid}}(C_NULL)
const _get_capi = Ref{Ptr{Cvoid}}(C_NULL)
const _pycast2jl = Ref{Ptr{Cvoid}}(C_NULL)
const _pycast2py = Ref{Ptr{Cvoid}}(C_NULL)
const _jl_repr_pretty = Ref{Ptr{Cvoid}}(C_NULL)

function pycast2jl(out::Ptr{TyJuliaCAPI.JV}, T::Int64, p::Ptr{Cvoid})
    py = Py(BorrowReference(), reinterpret(CPython.C.Ptr{CPython.PyObject}, p))
    t = TyJuliaCAPI.JTypeFromIdent(T)
    try
        p = TyJuliaCAPI.JV_ALLOC(py_cast(t,py))
        unsafe_store!(out, p)
    catch
        return TyJuliaCAPI.ERROR
    end
    return TyJuliaCAPI.OK
end

get_pycast2jl() = @cfunction(pycast2jl, TyJuliaCAPI.ErrorCode, (Ptr{TyJuliaCAPI.JV}, Int64, Ptr{Cvoid}, ))

function pycast2py(v::TyJuliaCAPI.JV)
    v′ = TyJuliaCAPI.JV_LOAD(v)
    try
        py = py_cast(TyPython.CPython.Py, v′)
        TyPython.CPython.PyAPI.Py_IncRef(py)
        ptr = reinterpret(Ptr{TyPython.CPython.PyObject}, TyPython.CPython.unsafe_unwrap(py))
        return ptr
    catch
        return reinterpret(Ptr{TyPython.CPython.PyObject}, C_NULL)
    end
end

get_pycast2py() = @cfunction(pycast2py, Ptr{TyPython.CPython.PyObject}, (TyJuliaCAPI.JV, ))


function jl_display(self::TyJuliaCAPI.JV)
    old_stdout = stdout
    rd, wr = redirect_stdout()
    try
        show(wr, "text/plain", TyJuliaCAPI.JV_LOAD(self))
    catch
        return reinterpret(Ptr{TyPython.CPython.PyObject}, C_NULL)
    finally
        try
            close(wr)
        catch
        end
        redirect_stdout(old_stdout)
    end
    s = read(rd, String)

    try
        py = py_cast(TyPython.CPython.Py, s)
        TyPython.CPython.PyAPI.Py_IncRef(py)
        ptr = reinterpret(Ptr{TyPython.CPython.PyObject}, TyPython.CPython.unsafe_unwrap(py))
        return ptr
    catch
        return reinterpret(Ptr{TyPython.CPython.PyObject}, C_NULL)
    end
end

get_jl_repr_pretty() = @cfunction(jl_display, Ptr{TyPython.CPython.PyObject}, (TyJuliaCAPI.JV,))


function boot()
    _get_capi[] = TyJuliaCAPI.get_capi_getter()
    _pycast2jl[] = get_pycast2jl()
    _pycast2py[] = get_pycast2py()
    _jl_repr_pretty[] = get_jl_repr_pretty()
    LibJuliaCall[] = dlopen(joinpath(@__DIR__, "libjuliacall"))
    init_LibJuliaCall = dlsym(LibJuliaCall[], :init_libjuliacall)
    err = ccall(
        init_LibJuliaCall,
        Cint,
        (Ptr{Cvoid}, Ptr{Cvoid}, Ptr{Cvoid}, Ptr{Cvoid}),
        _get_capi[], _pycast2jl[], _pycast2py[], _jl_repr_pretty[]
    )
    if err != 0
        error("Failed to initialize LibJuliaCall")
    end

    init_PyModule = dlsym(LibJuliaCall[], :init_PyModule)
    ccall(init_PyModule, Ptr{Cvoid}, ())

    return nothing
end

precompile(boot, ())







