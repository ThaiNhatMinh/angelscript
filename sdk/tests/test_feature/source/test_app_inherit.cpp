#include "utils.h"
#include <math.h> // sqrtf

using namespace std;

namespace TestAppInherit
{

// =====================================================
// C++ types that scripts will inherit from
// =====================================================

// --- Ref-counted reference type ---
struct MyRefBase
{
	static int dtorCount;
	MyRefBase() : x(0), y(0.0f), refCount(1) {}
	~MyRefBase() { dtorCount++; }
	void AddRef()  { refCount++; }
	void Release() { if (--refCount == 0) delete this; }

	int    x;
	float  y;
	int    refCount;
};
int MyRefBase::dtorCount = 0;
MyRefBase *MyRefBase_Factory() { return new MyRefBase(); }
int         MyRefBase_Sum(MyRefBase *o) { return o->x + (int)o->y; }
void        MyRefBase_SetXY(MyRefBase *o, int _x, float _y) { o->x = _x; o->y = _y; }
static void MyRefBase_Dtor(MyRefBase *o) { o->~MyRefBase(); }

// --- Plain value type ---
struct Vec3
{
	static int dtorCount;
	Vec3() : x(0), y(0), z(0) {}
	Vec3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
	~Vec3() { dtorCount++; }
	float Length() const { return sqrtf(x*x + y*y + z*z); }

	float x, y, z;
};
int Vec3::dtorCount = 0;
static void Vec3_DefaultCtor(Vec3 *o)
{
	new(o) Vec3();
}
static void Vec3_Ctor(float x, float y, float z, Vec3 *o)
{ 
	new(o) Vec3(x,y,z); 
}
static void Vec3_CopyCtor(const Vec3 &other, Vec3 *o)
{
	new(o) Vec3(other);
}
static void Vec3_Dymmy(const Vec3& other, Vec3* o)
{
	new(o) Vec3(other);
}
static void Vec3_DefaultDtor(Vec3* o)
{
	o->~Vec3();
}

static void FloatValue(float f)
{
	f;
}

// --- Final class (cannot be inherited) ---
struct MyFinal
{
	MyFinal() : value(0), refCount(1) {}
	void AddRef()  { refCount++; }
	void Release() { if (--refCount == 0) delete this; }
	int value;
	int refCount;
};
MyFinal *MyFinal_Factory() { return new MyFinal(); }

// =====================================================
// Tests
// =====================================================
bool Test()
{
	RET_ON_MAX_PORT

	bool fail = false;
	int  r;
	CBufferedOutStream bout;



	// ------------------------------------------------------------------
	// Test 8: Value type - explicit copy constructor with super(other)
	// ------------------------------------------------------------------
	{
		asIScriptEngine* engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		engine->RegisterObjectType("Vec3", sizeof(Vec3), asOBJ_VALUE | asGetTypeTraits<Vec3>());
		engine->RegisterObjectProperty("Vec3", "float x", asOFFSET(Vec3, x));
		engine->RegisterObjectProperty("Vec3", "float y", asOFFSET(Vec3, y));
		engine->RegisterObjectProperty("Vec3", "float z", asOFFSET(Vec3, z));
		engine->RegisterObjectBehaviour("Vec3", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(Vec3_DefaultCtor), asCALL_CDECL_OBJLAST);
		engine->RegisterObjectBehaviour("Vec3", asBEHAVE_CONSTRUCT, "void f(float,float,float)", asFUNCTION(Vec3_Ctor), asCALL_CDECL_OBJLAST);
		engine->RegisterObjectBehaviour("Vec3", asBEHAVE_CONSTRUCT, "void f(const Vec3 &in)", asFUNCTION(Vec3_CopyCtor), asCALL_CDECL_OBJLAST);
		engine->RegisterObjectBehaviour("Vec3", asBEHAVE_DESTRUCT, "void f()", asFUNCTION(Vec3_DefaultDtor), asCALL_CDECL_OBJLAST);
		engine->RegisterObjectMethod("Vec3", "float Length() const", asMETHOD(Vec3, Length), asCALL_THISCALL);
		engine->RegisterObjectMethod("Vec3", "void Dummy(const Vec3 &in) const", asFUNCTION(Vec3_Dymmy), asCALL_CDECL_OBJLAST);

		asIScriptModule* mod = engine->GetModule("test_copy_ctor", asGM_ALWAYS_CREATE);
		bout.buffer = "";

		// Derived class with explicit copy constructor calling super(other)
		mod->AddScriptSection("test_copy_ctor",
			"class Copyable : Vec3 {                   \n"
			"  int tag;                                 \n"
			"  Copyable() {                             \n"
			//"    super();                               \n"
			"    tag = 0;                               \n"
			"  }                                        \n"
			"  Copyable(const Copyable &other) {        \n"
			"    super(other);                          \n"
			"    tag = other.tag;                       \n"
			"  }                                        \n"
			"}                                          \n"
			"void CheckCopy(Copyable c) {               \n"
			"  assert(c.x == 1.0f);                     \n"
			"  assert(c.y == 2.0f);                     \n"
			"  assert(c.z == 3.0f);                     \n"
			"  assert(c.tag == 99);                     \n"
			"}                                          \n"
		);

		r = mod->Build();
		if (r < 0)
		{
			PRINTF("Build failed: %s\n", bout.buffer.c_str());
			TEST_FAILED;
			return fail;
		}

		r = ExecuteString(engine,
			"Copyable param; param.x = 123;                                \n"
			"Copyable param2; param2.y = 123;                                \n"
			"Copyable a;                                \n"
			"a.Dummy(param);                                \n"
			"param.Dummy(param2);                                \n"
			"param2.Dummy(a);                                \n"
			"a.x = 1.0f;                                \n"
			"a.y = 2.0f;                                \n"
			"a.z = 3.0f;                                \n"
			"a.tag = 99;                                \n"
			// Explicit copy construction
			"Copyable b = a;                            \n"
			"assert(b.x == 1.0f);                       \n"
			"assert(b.y == 2.0f);                       \n"
			"assert(b.z == 3.0f);                       \n"
			"assert(b.tag == 99);                       \n"
			 //Pass by value (triggers copy constructor)
			"CheckCopy(a);                               \n"
			// Verify original unchanged
			"assert(a.x == 1.0f);                       \n"
			"assert(a.tag == 99);                       \n"
			, mod);
		if (r != asEXECUTION_FINISHED)
		{
			if (r == asEXECUTION_EXCEPTION)
				PRINTF("Exception: %s\n", "Copyable test exception");
			TEST_FAILED;
		}

		engine->GarbageCollect();
		engine->ShutDownAndRelease();
	}

	return true;

	// ------------------------------------------------------------------
	// Test 9: Value type - default generated copy constructor in derived class
	// ------------------------------------------------------------------
	{
		asIScriptEngine* engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);
		engine->RegisterGlobalFunction("void FloatValue(float)", asFUNCTION(FloatValue), asCALL_CDECL);

		Vec3::dtorCount = 0;

		engine->RegisterObjectType("Vec3", sizeof(Vec3), asOBJ_VALUE | asGetTypeTraits<Vec3>());
		engine->RegisterObjectProperty("Vec3", "float x", asOFFSET(Vec3, x));
		engine->RegisterObjectProperty("Vec3", "float y", asOFFSET(Vec3, y));
		engine->RegisterObjectProperty("Vec3", "float z", asOFFSET(Vec3, z));
		engine->RegisterObjectBehaviour("Vec3", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(Vec3_DefaultCtor), asCALL_CDECL_OBJLAST);
		engine->RegisterObjectBehaviour("Vec3", asBEHAVE_CONSTRUCT, "void f(float,float,float)", asFUNCTION(Vec3_Ctor), asCALL_CDECL_OBJLAST);
		engine->RegisterObjectBehaviour("Vec3", asBEHAVE_CONSTRUCT, "void f(const Vec3 &in)", asFUNCTION(Vec3_CopyCtor), asCALL_CDECL_OBJLAST);
		engine->RegisterObjectBehaviour("Vec3", asBEHAVE_DESTRUCT, "void f()", asFUNCTION(Vec3_DefaultDtor), asCALL_CDECL_OBJLAST);
		engine->RegisterObjectMethod("Vec3", "float Length() const", asMETHOD(Vec3, Length), asCALL_THISCALL);
		engine->RegisterObjectMethod("Vec3", "void Dummy(const Vec3 &in) const", asFUNCTION(Vec3_Dymmy), asCALL_CDECL_OBJLAST);
		asIScriptModule* mod = engine->GetModule("test_default_copy", asGM_ALWAYS_CREATE);
		bout.buffer = "";

		// Derived class without explicit copy constructor — compiler should auto-generate one
		mod->AddScriptSection("test_default_copy",
			"class AutoCopy : Vec3 {                   \n"
			"  int extra;                               \n"
			"  AutoCopy() {                             \n"
			"    super();                               \n"
			"    extra = 0;                             \n"
			"  }                                        \n"
			"}                                          \n"
		);

		r = mod->Build();
		if (r < 0)
		{
			PRINTF("Build failed: %s\n", bout.buffer.c_str());
			TEST_FAILED;
			return fail;
		}

		r = ExecuteString(engine,
			//"Vec3 v; \n"
			//"Vec3 v2; \n"
			//"v2.Dummy(v); \n"
			"AutoCopy a;                                \n"
			"a.x = 1.5f;                                \n"
			"AutoCopy b = a;                            \n"
			"assert(b.x == 1.5f);                       \n"
			"assert(b.y == 0.0f);                       \n"
			"assert(b.z == 0.0f);                       \n"
			"assert(b.extra == 0);                      \n"
			// Modify a, make sure b is a separate copy
			"a.x = 5.0f;                                \n"
			"FloatValue(a.x);                            \n"
			"FloatValue(b.x);                            \n"
			"assert(b.x == 1.5f);                       \n"
			, mod);
		if (r != asEXECUTION_FINISHED)
		{
			if (r == asEXECUTION_EXCEPTION)
				PRINTF("Exception: %s\n", "AutoCopy exception");
			TEST_FAILED;
		}

		engine->GarbageCollect();
		engine->ShutDownAndRelease();
	}

	// ------------------------------------------------------------------
	// Test 1: Value-type base class inheritance
	// ------------------------------------------------------------------
	{
		asIScriptEngine *engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		engine->RegisterObjectType("Vec3", sizeof(Vec3), asOBJ_VALUE | asGetTypeTraits<Vec3>());
		engine->RegisterObjectProperty("Vec3", "float x", asOFFSET(Vec3, x));
		engine->RegisterObjectProperty("Vec3", "float y", asOFFSET(Vec3, y));
		engine->RegisterObjectProperty("Vec3", "float z", asOFFSET(Vec3, z));
		engine->RegisterObjectBehaviour("Vec3", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(Vec3_DefaultCtor), asCALL_CDECL_OBJLAST);
		engine->RegisterObjectBehaviour("Vec3", asBEHAVE_DESTRUCT, "void f()", asFUNCTION(Vec3_DefaultDtor), asCALL_CDECL_OBJLAST);
		engine->RegisterObjectBehaviour("Vec3", asBEHAVE_CONSTRUCT, "void f(float,float,float)", asFUNCTION(Vec3_Ctor), asCALL_CDECL_OBJLAST);
		engine->RegisterObjectMethod("Vec3", "float Length() const", asMETHOD(Vec3, Length), asCALL_THISCALL);

		asIScriptModule *mod = engine->GetModule("test_val", asGM_ALWAYS_CREATE);
		bout.buffer = "";

		// Inherit from value type
		mod->AddScriptSection("test_val",
			"class Entity : Vec3 {                 \n"
			"  int id;                              \n"
			"  Entity() {                           \n"
			//"    super(3.0f, 4.0f, 0.0f);           \n" // Call Vec3(float,float,float)
			//"    super();           \n" // Call Vec3()
			"    id = 1;                            \n"
			"  }                                    \n"
			"	~Entity() {}                         \n"
			"  float GetLen() const { return Length(); } \n"
			"}                                      \n"
			"class Widget { int vasd; bool b; void CallMe() { vasd = 42; b = true; } } \n"
		);

		r = mod->Build();
		if (r < 0)
		{
			PRINTF("Build failed: %s\n", bout.buffer.c_str());
			TEST_FAILED;
			return false;
		}
		asIScriptContext* ctx = engine->CreateContext();
		// Verify inherited properties and methods work from script
		r = ExecuteString(engine,
			"Vec3 asd; \n"
			"Widget w; w.CallMe(); assert(w.vasd == 42); assert(w.b); \n"
			"Entity entity;                               \n"
			"entity.GetLen();					\n"
			"entity.x = 3.0f;                               \n"
			"entity.y = 4.0f;                               \n"
			"entity.z = 0.0f;                               \n"
			"entity.id = 1;                               \n"
			"entity.Length();                   \n" // Inherited property
			"entity.GetLen();					\n"
			"assert(entity.x == 3.0f);                   \n" // Inherited property
			"assert(entity.y == 4.0f);                   \n"
			"assert(entity.z == 0.0f);                   \n"
			"assert(entity.id == 1);                     \n" // Own property
			"assert(entity.GetLen() == 5.0f);             \n" // Inherited method via own method
			, mod, ctx);

		if (r != asEXECUTION_FINISHED)
		{
			if (r == asEXECUTION_EXCEPTION)
			{
				if (ctx)
					PRINTF("Exception: %s\n", ctx->GetExceptionString());
			}
			TEST_FAILED;
		}
		ctx->Release();

		// Verify DerivesFrom and GetBaseType
		asITypeInfo *et = mod->GetTypeInfoByName("Entity");
		if (!et || !et->GetBaseType())
			TEST_FAILED;
		if (strcmp(et->GetBaseType()->GetName(), "Vec3") != 0)
		{
			PRINTF("Expected base Vec3, got %s\n", et->GetBaseType()->GetName());
			TEST_FAILED;
		}
		if (!et->DerivesFrom(engine->GetTypeInfoByName("Vec3")))
			TEST_FAILED;

		// Force GC and verify Vec3 destructor was called
		engine->GarbageCollect();
		engine->ShutDownAndRelease();
	}



	// ------------------------------------------------------------------
	// Test 4: Value type with explicit super(args) call to parameterized base constructor
	// ------------------------------------------------------------------
	{
		asIScriptEngine *engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		engine->RegisterObjectType("Vec3", sizeof(Vec3), asOBJ_VALUE | asGetTypeTraits<Vec3>());
		engine->RegisterObjectProperty("Vec3", "float x", asOFFSET(Vec3, x));
		engine->RegisterObjectProperty("Vec3", "float y", asOFFSET(Vec3, y));
		engine->RegisterObjectProperty("Vec3", "float z", asOFFSET(Vec3, z));
		engine->RegisterObjectBehaviour("Vec3", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(Vec3_DefaultCtor), asCALL_CDECL_OBJLAST);
		engine->RegisterObjectBehaviour("Vec3", asBEHAVE_DESTRUCT, "void f()", asFUNCTION(Vec3_DefaultDtor), asCALL_CDECL_OBJLAST);
		engine->RegisterObjectBehaviour("Vec3", asBEHAVE_CONSTRUCT, "void f(float,float,float)", asFUNCTION(Vec3_Ctor), asCALL_CDECL_OBJLAST);
		engine->RegisterObjectMethod("Vec3", "float Length() const", asMETHOD(Vec3, Length), asCALL_THISCALL);

		asIScriptModule *mod = engine->GetModule("test_super_args", asGM_ALWAYS_CREATE);
		bout.buffer = "";

		// Derived class calls super(3.0f, 4.0f, 0.0f) to initialize Vec3 with parameterized constructor
		mod->AddScriptSection("test_super_args",
			"class EntityPos : Vec3 {                    \n"
			"  int id;                                   \n"
			"  EntityPos() {                             \n"
			"    super(3.0f, 4.0f, 0.0f);                \n"
			"    id = 1;                                 \n"
			"  }                                         \n"
			"  float GetLen() const { return Length(); }  \n"
			"}                                           \n"
		);

		r = mod->Build();
		if (r < 0)
		{
			PRINTF("Build failed: %s\n", bout.buffer.c_str());
			TEST_FAILED;
			return fail;
		}
		asIScriptContext* ctx = engine->CreateContext();
		r = ExecuteString(engine,
			"EntityPos entity;                             \n"
			"assert(entity.x == 3.0f);                     \n"
			"assert(entity.y == 4.0f);                     \n"
			"assert(entity.z == 0.0f);                     \n"
			"assert(entity.id == 1);                       \n"
			"assert(entity.GetLen() == 5.0f);              \n"
			, mod, ctx);
		if (r != asEXECUTION_FINISHED)
		{
			if (r == asEXECUTION_EXCEPTION)
			{
				if (ctx)
					PRINTF("Exception: %s\n", ctx->GetExceptionString());
			}
			TEST_FAILED;
		}
		ctx->Release();

		engine->GarbageCollect();
		engine->ShutDownAndRelease();
	}

	// ------------------------------------------------------------------
	// Test 5: Value type with explicit super() calling default base constructor
	// ------------------------------------------------------------------
	{
		asIScriptEngine *engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		Vec3::dtorCount = 0;

		engine->RegisterObjectType("Vec3", sizeof(Vec3), asOBJ_VALUE | asGetTypeTraits<Vec3>());
		engine->RegisterObjectProperty("Vec3", "float x", asOFFSET(Vec3, x));
		engine->RegisterObjectProperty("Vec3", "float y", asOFFSET(Vec3, y));
		engine->RegisterObjectProperty("Vec3", "float z", asOFFSET(Vec3, z));
		engine->RegisterObjectBehaviour("Vec3", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(Vec3_DefaultCtor), asCALL_CDECL_OBJLAST);
		engine->RegisterObjectBehaviour("Vec3", asBEHAVE_DESTRUCT, "void f()", asFUNCTION(Vec3_DefaultDtor), asCALL_CDECL_OBJLAST);
		engine->RegisterObjectBehaviour("Vec3", asBEHAVE_CONSTRUCT, "void f(float,float,float)", asFUNCTION(Vec3_Ctor), asCALL_CDECL_OBJLAST);

		asIScriptModule *mod = engine->GetModule("test_super_default", asGM_ALWAYS_CREATE);
		bout.buffer = "";

		// Derived class explicitly calls super() to invoke default Vec3 constructor
		mod->AddScriptSection("test_super_default",
			"class EntityDefault : Vec3 {                \n"
			"  float val;                                \n"
			"  EntityDefault() {                         \n"
			"    super();                                \n"
			"    val = 42.0f;                            \n"
			"  }                                         \n"
			"}                                           \n"
		);

		r = mod->Build();
		if (r < 0)
		{
			PRINTF("Build failed: %s\n", bout.buffer.c_str());
			TEST_FAILED;
			return fail;
		}
		r = ExecuteString(engine,
			"EntityDefault e;                              \n"
			"assert(e.x == 0.0f);                          \n"
			"assert(e.y == 0.0f);                          \n"
			"assert(e.z == 0.0f);                          \n"
			"assert(e.val == 42.0f);                       \n"
			, mod);
		if (r != asEXECUTION_FINISHED)
		{
			if (r == asEXECUTION_EXCEPTION)
				PRINTF("Exception: %s\n", "EntityDefault exception");
			TEST_FAILED;
		}

		engine->GarbageCollect();
		engine->ShutDownAndRelease();
	}

	// ------------------------------------------------------------------
	// Test 6: Derived class with multiple constructors calling different super() variants
	// ------------------------------------------------------------------
	{
		asIScriptEngine *engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		engine->RegisterObjectType("Vec3", sizeof(Vec3), asOBJ_VALUE | asGetTypeTraits<Vec3>());
		engine->RegisterObjectProperty("Vec3", "float x", asOFFSET(Vec3, x));
		engine->RegisterObjectProperty("Vec3", "float y", asOFFSET(Vec3, y));
		engine->RegisterObjectProperty("Vec3", "float z", asOFFSET(Vec3, z));
		engine->RegisterObjectBehaviour("Vec3", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(Vec3_DefaultCtor), asCALL_CDECL_OBJLAST);
		engine->RegisterObjectBehaviour("Vec3", asBEHAVE_DESTRUCT, "void f()", asFUNCTION(Vec3_DefaultDtor), asCALL_CDECL_OBJLAST);
		engine->RegisterObjectBehaviour("Vec3", asBEHAVE_CONSTRUCT, "void f(float,float,float)", asFUNCTION(Vec3_Ctor), asCALL_CDECL_OBJLAST);
		engine->RegisterObjectMethod("Vec3", "float Length() const", asMETHOD(Vec3, Length), asCALL_THISCALL);

		asIScriptModule *mod = engine->GetModule("test_multi_ctor", asGM_ALWAYS_CREATE);
		bout.buffer = "";

		// Derived class with two constructors: one calls super(), the other super(args)
		mod->AddScriptSection("test_multi_ctor",
			"class MultiCtor : Vec3 {                    \n"
			"  int id;                                   \n"
			"  MultiCtor() {                             \n"
			"    super();                                \n"
			"    id = 10;                                \n"
			"  }                                         \n"
			"  MultiCtor(float v) {                      \n"
			"    super(v, v, v);                         \n"
			"    id = 20;                                \n"
			"  }                                         \n"
			"}                                           \n"
		);

		r = mod->Build();
		if (r < 0)
		{
			PRINTF("Build failed: %s\n", bout.buffer.c_str());
			TEST_FAILED;
			return fail;
		}
		r = ExecuteString(engine,
			"MultiCtor a;                                 \n"
			"assert(a.x == 0.0f);                         \n"
			"assert(a.y == 0.0f);                         \n"
			"assert(a.z == 0.0f);                         \n"
			"assert(a.id == 10);                          \n"
			"MultiCtor b(5.0f);                           \n"
			"assert(b.x == 5.0f);                         \n"
			"assert(b.y == 5.0f);                         \n"
			"assert(b.z == 5.0f);                         \n"
			"assert(b.id == 20);                          \n"
			, mod);
		if (r != asEXECUTION_FINISHED)
		{
			if (r == asEXECUTION_EXCEPTION)
				PRINTF("Exception: %s\n", "MultiCtor exception");
			TEST_FAILED;
		}

		engine->GarbageCollect();
		engine->ShutDownAndRelease();
	}

	// ------------------------------------------------------------------
	// Test 10: Value type - passing derived objects by value triggers copy constructor
	// ------------------------------------------------------------------
	{
		asIScriptEngine *engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		engine->RegisterObjectType("Vec3", sizeof(Vec3), asOBJ_VALUE | asGetTypeTraits<Vec3>());
		engine->RegisterObjectProperty("Vec3", "float x", asOFFSET(Vec3, x));
		engine->RegisterObjectProperty("Vec3", "float y", asOFFSET(Vec3, y));
		engine->RegisterObjectProperty("Vec3", "float z", asOFFSET(Vec3, z));
		engine->RegisterObjectBehaviour("Vec3", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(Vec3_DefaultCtor), asCALL_CDECL_OBJLAST);
		engine->RegisterObjectBehaviour("Vec3", asBEHAVE_CONSTRUCT, "void f(float,float,float)", asFUNCTION(Vec3_Ctor), asCALL_CDECL_OBJLAST);
		engine->RegisterObjectBehaviour("Vec3", asBEHAVE_CONSTRUCT, "void f(const Vec3 &in)", asFUNCTION(Vec3_CopyCtor), asCALL_CDECL_OBJLAST);
		engine->RegisterObjectBehaviour("Vec3", asBEHAVE_DESTRUCT, "void f()", asFUNCTION(Vec3_DefaultDtor), asCALL_CDECL_OBJLAST);
		engine->RegisterObjectMethod("Vec3", "float Length() const", asMETHOD(Vec3, Length), asCALL_THISCALL);

		asIScriptModule *mod = engine->GetModule("test_byval", asGM_ALWAYS_CREATE);
		bout.buffer = "";

		mod->AddScriptSection("test_byval",
			"class ByValTest : Vec3 {                  \n"
			"  ByValTest() { super(2.0f, 3.0f, 4.0f); } \n"
			"  float Sum() const { return x + y + z; }  \n"
			"}                                          \n"
			"void TakeByVal(ByValTest v) {              \n"
			"  assert(v.x == 2.0f);                     \n"
			"  assert(v.y == 3.0f);                     \n"
			"  assert(v.z == 4.0f);                     \n"
			"  assert(v.Sum() == 9.0f);                 \n"
			"}                                          \n"
		);

		r = mod->Build();
		if (r < 0)
		{
			PRINTF("Build failed: %s\n", bout.buffer.c_str());
			TEST_FAILED;
			return fail;
		}

		r = ExecuteString(engine,
			"ByValTest a;                               \n"
			"TakeByVal(a);                              \n"
			, mod);
		if (r != asEXECUTION_FINISHED)
		{
			if (r == asEXECUTION_EXCEPTION)
				PRINTF("Exception: %s\n", "ByValTest exception");
			TEST_FAILED;
		}

		engine->GarbageCollect();
		engine->ShutDownAndRelease();
	}

	return true;

	// ------------------------------------------------------------------
	// Test ?: Ref-type base class inheritance
	// ------------------------------------------------------------------
	{
		asIScriptEngine *engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		engine->RegisterObjectType("MyRefBase", 0, asOBJ_REF);
		engine->RegisterObjectBehaviour("MyRefBase", asBEHAVE_FACTORY, "MyRefBase@ f()", asFUNCTION(MyRefBase_Factory), asCALL_CDECL);
		engine->RegisterObjectBehaviour("MyRefBase", asBEHAVE_ADDREF, "void f()", asMETHOD(MyRefBase, AddRef), asCALL_THISCALL);
		engine->RegisterObjectBehaviour("MyRefBase", asBEHAVE_RELEASE, "void f()", asMETHOD(MyRefBase, Release), asCALL_THISCALL);
		engine->RegisterObjectProperty("MyRefBase", "int x", asOFFSET(MyRefBase, x));
		engine->RegisterObjectProperty("MyRefBase", "float y", asOFFSET(MyRefBase, y));
		engine->RegisterObjectMethod("MyRefBase", "int Sum() const", asFUNCTION(MyRefBase_Sum), asCALL_CDECL_OBJFIRST);
		engine->RegisterObjectMethod("MyRefBase", "void SetXY(int, float)", asFUNCTION(MyRefBase_SetXY), asCALL_CDECL_OBJFIRST);
		engine->RegisterObjectBehaviour("MyRefBase", asBEHAVE_DESTRUCT, "void f()", asFUNCTION(MyRefBase_Dtor), asCALL_CDECL_OBJLAST);

		asIScriptModule *mod = engine->GetModule("test_ref", asGM_ALWAYS_CREATE);
		bout.buffer = "";

		mod->AddScriptSection("test_ref",
			"class Widget : MyRefBase {              \n"
			"  int id;                                \n"
			"  Widget() {                             \n"
			"    SetXY(10, 20.0f);                    \n" // Inherited method
			"    id = x + int(y);                     \n" // Access inherited properties
			"  }                                      \n"
			"  int Calc() const { return Sum() + id; } \n" // Inherited method + own property
			"}                                        \n");

		r = mod->Build();
		if (r < 0)
		{
			PRINTF("Build failed: %s\n", bout.buffer.c_str());
			TEST_FAILED;
		}

		r = ExecuteString(engine,
			"Widget w;                               \n"
			"assert(w.x == 10);                      \n" // Inherited property
			"assert(w.y == 20.0f);                   \n"
			"assert(w.id == 30);                     \n" // Own property = 10+20
			"assert(w.Calc() == 40);                 \n" // Sum()=10, id=30 => 40
			"w.SetXY(5, 3.0f);                       \n" // Inherited method
			"assert(w.x == 5);                       \n"
			"assert(w.Sum() == 8);                   \n" // Inherited method directly
			, mod);
		if (r != asEXECUTION_FINISHED)
		{
			if (r == asEXECUTION_EXCEPTION)
				PRINTF("Exception: %s\n", "engine->GetContext()->GetExceptionString()");
			TEST_FAILED;
		}

		asITypeInfo *wt = mod->GetTypeInfoByName("Widget");
		if (!wt || !wt->GetBaseType() || strcmp(wt->GetBaseType()->GetName(), "MyRefBase") != 0)
			TEST_FAILED;
		if (!wt->DerivesFrom(engine->GetTypeInfoByName("MyRefBase")))
			TEST_FAILED;

		// Force GC and verify ref type destructor was called
		engine->GarbageCollect();
		engine->ShutDownAndRelease();
		if (MyRefBase::dtorCount != 1)
		{
			PRINTF("Expected 1 MyRefBase destructor call, got %d\n", MyRefBase::dtorCount);
			TEST_FAILED;
		}
	}

	// ------------------------------------------------------------------
	// Test ?: Cannot inherit from final class
	// ------------------------------------------------------------------
	{
		asIScriptEngine *engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		bout.buffer = "";

		engine->RegisterObjectType("MyFinal", 0, asOBJ_REF | asOBJ_NOINHERIT);
		engine->RegisterObjectBehaviour("MyFinal", asBEHAVE_FACTORY, "MyFinal@ f()", asFUNCTION(MyFinal_Factory), asCALL_CDECL);
		engine->RegisterObjectBehaviour("MyFinal", asBEHAVE_ADDREF, "void f()", asMETHOD(MyFinal, AddRef), asCALL_THISCALL);
		engine->RegisterObjectBehaviour("MyFinal", asBEHAVE_RELEASE, "void f()", asMETHOD(MyFinal, Release), asCALL_THISCALL);

		asIScriptModule *mod = engine->GetModule("test_final", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test", "class Bad : MyFinal {} \n");
		r = mod->Build();
		if (r >= 0)
			TEST_FAILED;
		if (bout.buffer != "test (1, 14) : Error   : Can't inherit from class 'MyFinal' marked as final\n")
		{
			PRINTF("Got:     '%s'\n", bout.buffer.c_str());
			TEST_FAILED;
		}

		engine->ShutDownAndRelease();
	}

	// ------------------------------------------------------------------
	// Test ?: Ref type with explicit super() call in constructor
	// ------------------------------------------------------------------
	{
		asIScriptEngine *engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		MyRefBase::dtorCount = 0;

		engine->RegisterObjectType("MyRefBase", 0, asOBJ_REF);
		engine->RegisterObjectBehaviour("MyRefBase", asBEHAVE_FACTORY, "MyRefBase@ f()", asFUNCTION(MyRefBase_Factory), asCALL_CDECL);
		engine->RegisterObjectBehaviour("MyRefBase", asBEHAVE_ADDREF, "void f()", asMETHOD(MyRefBase, AddRef), asCALL_THISCALL);
		engine->RegisterObjectBehaviour("MyRefBase", asBEHAVE_RELEASE, "void f()", asMETHOD(MyRefBase, Release), asCALL_THISCALL);
		engine->RegisterObjectProperty("MyRefBase", "int x", asOFFSET(MyRefBase, x));
		engine->RegisterObjectProperty("MyRefBase", "float y", asOFFSET(MyRefBase, y));
		engine->RegisterObjectBehaviour("MyRefBase", asBEHAVE_DESTRUCT, "void f()", asFUNCTION(MyRefBase_Dtor), asCALL_CDECL_OBJLAST);
		engine->RegisterObjectMethod("MyRefBase", "void SetXY(int, float)", asFUNCTION(MyRefBase_SetXY), asCALL_CDECL_OBJFIRST);

		asIScriptModule *mod = engine->GetModule("test_ref_super", asGM_ALWAYS_CREATE);
		bout.buffer = "";

		// Derived class - note: MyRefBase only has a factory,
		// no explicit constructors, so super() is not called.
		// The base sub-object is auto-initialized.
		mod->AddScriptSection("test_ref_super",
			"class WidgetEx : MyRefBase {                \n"
			"  int id;                                   \n"
			"  WidgetEx() {                              \n"
			"    SetXY(7, 3.0f);                         \n"
			"    id = x + int(y);                        \n"
			"  }                                         \n"
			"  int Calc() const { return id + x; }       \n"
			"}                                           \n");

		r = mod->Build();
		if (r < 0)
		{
			PRINTF("Build failed: %s\n", bout.buffer.c_str());
			TEST_FAILED;
		}

		r = ExecuteString(engine,
			"WidgetEx w;                                  \n"
			"assert(w.x == 7);                            \n"
			"assert(w.y == 3.0f);                         \n"
			"assert(w.id == 10);                          \n"
			"assert(w.Calc() == 17);                      \n"
			, mod);
		if (r != asEXECUTION_FINISHED)
		{
			if (r == asEXECUTION_EXCEPTION)
				PRINTF("Exception: %s\n", "WidgetEx exception");
			TEST_FAILED;
		}

		asITypeInfo *wt = mod->GetTypeInfoByName("WidgetEx");
		if (!wt || !wt->GetBaseType() || strcmp(wt->GetBaseType()->GetName(), "MyRefBase") != 0)
			TEST_FAILED;
		if (!wt->DerivesFrom(engine->GetTypeInfoByName("MyRefBase")))
			TEST_FAILED;

		engine->GarbageCollect();
		engine->ShutDownAndRelease();
		if (MyRefBase::dtorCount != 1)
		{
			PRINTF("Expected 1 MyRefBase destructor call, got %d\n", MyRefBase::dtorCount);
			TEST_FAILED;
		}
	}

	// ------------------------------------------------------------------
	// Test ?: Ref type - handle assignment with derived class
	// ------------------------------------------------------------------
	{
		asIScriptEngine *engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		MyRefBase::dtorCount = 0;

		engine->RegisterObjectType("MyRefBase", 0, asOBJ_REF);
		engine->RegisterObjectBehaviour("MyRefBase", asBEHAVE_FACTORY, "MyRefBase@ f()", asFUNCTION(MyRefBase_Factory), asCALL_CDECL);
		engine->RegisterObjectBehaviour("MyRefBase", asBEHAVE_ADDREF, "void f()", asMETHOD(MyRefBase, AddRef), asCALL_THISCALL);
		engine->RegisterObjectBehaviour("MyRefBase", asBEHAVE_RELEASE, "void f()", asMETHOD(MyRefBase, Release), asCALL_THISCALL);
		engine->RegisterObjectProperty("MyRefBase", "int x", asOFFSET(MyRefBase, x));
		engine->RegisterObjectProperty("MyRefBase", "float y", asOFFSET(MyRefBase, y));
		engine->RegisterObjectBehaviour("MyRefBase", asBEHAVE_DESTRUCT, "void f()", asFUNCTION(MyRefBase_Dtor), asCALL_CDECL_OBJLAST);
		engine->RegisterObjectMethod("MyRefBase", "void SetXY(int, float)", asFUNCTION(MyRefBase_SetXY), asCALL_CDECL_OBJFIRST);

		asIScriptModule *mod = engine->GetModule("test_ref_assign", asGM_ALWAYS_CREATE);
		bout.buffer = "";

		// Derived class - verify handle assignment works
		mod->AddScriptSection("test_ref_assign",
			"class Widget : MyRefBase {                \n"
			"  int id;                                 \n"
			"  Widget() {                              \n"
			"    SetXY(3, 5.0f);                      \n"
			"    id = x + int(y);                     \n"
			"  }                                       \n"
			"}                                         \n");

		r = mod->Build();
		if (r < 0)
		{
			PRINTF("Build failed: %s\n", bout.buffer.c_str());
			TEST_FAILED;
		}

		r = ExecuteString(engine,
			"Widget @w = Widget();                      \n"
			"assert(w.id == 8);                         \n"
			// Handle assignment - both point to same object
			"Widget @w2 = w;                            \n"
			"assert(w2.id == 8);                        \n"
			"assert(w2.x == 3);                         \n"
			, mod);
		if (r != asEXECUTION_FINISHED)
		{
			if (r == asEXECUTION_EXCEPTION)
				PRINTF("Exception: %s\n", "Ref assign exception");
			TEST_FAILED;
		}

		engine->GarbageCollect();
		engine->ShutDownAndRelease();
		if (MyRefBase::dtorCount != 1)
		{
			PRINTF("Expected 1 MyRefBase destructor call, got %d\n", MyRefBase::dtorCount);
			TEST_FAILED;
		}
	}

	return fail;
}

} // namespace
