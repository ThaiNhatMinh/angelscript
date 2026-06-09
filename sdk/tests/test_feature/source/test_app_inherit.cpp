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
static void Vec3_DefaultDtor(Vec3* o)
{
	o->~Vec3();
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
			"    super(3.0f, 4.0f, 0.0f);           \n" // Call Vec3(float,float,float)
			//"    super();           \n" // Call Vec3()
			"    id = 1;                            \n"
			"  }                                    \n"
			//"	~Entity() {}                         \n"
			"  float GetLen() const { return Length(); } \n"
			"}                                      \n");

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
		if (Vec3::dtorCount != 1)
		{
			PRINTF("Expected 1 Vec3 destructor call, got %d\n", Vec3::dtorCount);
			TEST_FAILED;
		}
	}

	return true;

	// ------------------------------------------------------------------
	// Test 2: Ref-type base class inheritance
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
	// Test 3: Cannot inherit from final class
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

	return fail;
}

} // namespace
