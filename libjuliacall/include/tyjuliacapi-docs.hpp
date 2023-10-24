// This file is generated. Do not modify it directly.
// This file is for documentation purpose only. Do not include it for compilation.
#include "tyjuliacapi-types.hpp"

void JLFreeFromMe(JV ref);
ErrorCode JLEval(/* out */JV* out, JV module, SList<uint8_t> code);
ErrorCode FetchJLErrorSize(/* out */int64_t* size);
ErrorCode FetchJLError(/* out */JSym* out, SList<uint8_t> msgBuffer);
JV JSymToJV(JSym sym);
JV JLTypeOf(JV value);
int64_t JLTypeOfAsTypeSlot(JV value);
bool8_t JLIsInstanceWithTypeSlot(JV value, int64_t slot);
ErrorCode JLCall(/* out */JV* out, JV func, SList<JV> args, SList<STuple<JSym,JV>> kwargs);
ErrorCode JLDotCall(/* out */JV* out, JV func, SList<JV> args, SList<STuple<JSym,JV>> kwargs);
ErrorCode JLCompare(/* out */bool8_t* out, Compare cmp, JV a, JV b);
ErrorCode JLGetProperty(/* out */JV* out, JV self, JSym property);
ErrorCode JLSetProperty(JV self, JSym property, JV value);
ErrorCode JLHasProperty(/* out */bool8_t* out, JV self, JSym property);
ErrorCode JLGetIndex(/* out */JV* out, JV self, SList<JV> index);
ErrorCode JLGetIndexI(/* out */JV* out, JV self, int64_t index);
ErrorCode JLSetIndex(JV self, SList<JV> index, JV value);
ErrorCode JLSetIndexI(JV self, int64_t index, JV value);
ErrorCode JLGetSymbol(/* out */JSym* out, JV value, bool8_t doCast);
ErrorCode JLGetBool(/* out */bool8_t* out, JV value, bool8_t doCast);
ErrorCode JLGetUInt8(/* out */uint8_t* out, JV value, bool8_t doCast);
ErrorCode JLGetUInt32(/* out */uint32_t* out, JV value, bool8_t doCast);
ErrorCode JLGetUInt64(/* out */uint64_t* out, JV value, bool8_t doCast);
ErrorCode JLGetInt32(/* out */int32_t* out, JV value, bool8_t doCast);
ErrorCode JLGetInt64(/* out */int64_t* out, JV value, bool8_t doCast);
ErrorCode JLGetSingle(/* out */float* out, JV value, bool8_t doCast);
ErrorCode JLGetDouble(/* out */double* out, JV value, bool8_t doCast);
ErrorCode JLGetComplexF64(/* out */complex_t* out, JV value, bool8_t doCast);
ErrorCode JLGetUTF8String(SList<uint8_t> out, JV value);
ErrorCode JLGetArrayPointer(/* out */uint8_t** out, /* out */int64_t* len, JV value);
ErrorCode JSymFromString(/* out */JSym* out, SList<uint8_t> value);
ErrorCode ToJLInt64(/* out */JV* out, int64_t value);
ErrorCode ToJLUInt64(/* out */JV* out, uint64_t value);
ErrorCode ToJLUInt32(/* out */JV* out, uint32_t value);
ErrorCode ToJLUInt8(/* out */JV* out, uint8_t value);
ErrorCode ToJLString(/* out */JV* out, SList<uint8_t> value);
ErrorCode ToJLBool(/* out */JV* out, bool8_t value);
ErrorCode ToJLFloat64(/* out */JV* out, double value);
ErrorCode ToJLComplexF64(/* out */JV* out, complex_t value);
ErrorCode JLStrVecWriteEltWithUTF8(JV self, int64_t i, SList<uint8_t> value);
ErrorCode JLStrVecGetEltNBytes(/* out */int64_t* out, JV self, int64_t i);
ErrorCode JLStrVecReadEltWithUTF8(JV self, int64_t i, SList<uint8_t> value);
ErrorCode JLTypeToIdent(/* out */int64_t* out, JV jv);
ErrorCode JLTypeFromIdent(/* out */JV* out, int64_t slot);
ErrorCode JLNew_F64Array(/* out */JV* out, SList<int64_t> dims);
ErrorCode JLNew_U64Array(/* out */JV* out, SList<int64_t> dims);
ErrorCode JLNew_U32Array(/* out */JV* out, SList<int64_t> dims);
ErrorCode JLNew_U8Array(/* out */JV* out, SList<int64_t> dims);
ErrorCode JLNew_I64Array(/* out */JV* out, SList<int64_t> dims);
ErrorCode JLNew_BoolArray(/* out */JV* out, SList<int64_t> dims);
ErrorCode JLNew_ComplexF64Array(/* out */JV* out, SList<int64_t> dims);
ErrorCode JLNew_StringVector(/* out */JV* out, int64_t length);
ErrorCode JLArray_Size(/* out */int64_t* out, JV self, int64_t i);
ErrorCode JLArray_Rank(/* out */int64_t* out, JV self);
void JLError_EnableBackTraceMsg(bool8_t status);
uint8_t JLError_HasBackTraceMsg();
ErrorCode JLError_FetchMsgSize(/* out */int64_t* size);
ErrorCode JLError_FetchMsgStr(/* out */JSym* out, SList<uint8_t> msgBuffer);
