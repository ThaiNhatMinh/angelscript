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
	~MyRefBase() { 
		dtorCount++; }
	void AddRef()  { refCount++; }
	void Release() { if (--refCount == 0) delete this; }

	int    x;
	float  y;
	int    refCount;
};
int MyRefBase::dtorCount = 0;
MyRefBase *MyRefBase_Factory() {
	return new MyRefBase();
}
int         MyRefBase_Sum(MyRefBase *o) {
	return o->x + (int)o->y;
}
void        MyRefBase_SetXY(MyRefBase *o, int _x, float _y)
{ 
	o->x = _x; o->y = _y;
}
static void MyRefBase_Dtor(MyRefBase *o)
{
	o->~MyRefBase();
}

// --- Plain value type ---
struct Vec3
{
	static int dtorCount;
	Vec3() : x(0), y(0), z(0) {}
	Vec3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
	~Vec3() { dtorCount++; }
	float Length() const { 
		return sqrtf(x*x + y*y + z*z); }

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

static float SumVec3(const Vec3 &v)
{
	return v.x + v.y + v.z;
}

static float SumVec3_Obj(Vec3 v)
{
	return v.x + v.y + v.z;
}

static void FloatValue(float f)
{
	f;
}

static void IntValue(int i)
{
	i;
}

static void IntValue2(int Left, int Right)
{
	Left;
	Right;
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
	// Test ?: Ref type - multi-level inheritance with polymorphism
	// Tests a deep ref type inheritance chain (Gadget : Widget : MyRefBase),
	// polymorphic storage via base handles, object identity through handle
	// chain, passing derived ref to function expecting base ref, and
	// ref-count integrity across the entire inheritance tree.
	// ------------------------------------------------------------------
	{
		asIScriptEngine* engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);
		engine->RegisterGlobalFunction("void FloatValue(float)", asFUNCTION(FloatValue), asCALL_CDECL);
		engine->RegisterGlobalFunction("void IntValue(int)", asFUNCTION(IntValue), asCALL_CDECL);
		engine->RegisterGlobalFunction("void IntValue2(int, int)", asFUNCTION(IntValue2), asCALL_CDECL);

		MyRefBase::dtorCount = 0;

		engine->RegisterObjectType("MyRefBase", 0, asOBJ_REF);
		engine->RegisterObjectBehaviour("MyRefBase", asBEHAVE_FACTORY, "MyRefBase@ f()", asFUNCTION(MyRefBase_Factory), asCALL_CDECL);
		engine->RegisterObjectBehaviour("MyRefBase", asBEHAVE_ADDREF, "void f()", asMETHOD(MyRefBase, AddRef), asCALL_THISCALL);
		engine->RegisterObjectBehaviour("MyRefBase", asBEHAVE_RELEASE, "void f()", asMETHOD(MyRefBase, Release), asCALL_THISCALL);
		engine->RegisterObjectProperty("MyRefBase", "int x", asOFFSET(MyRefBase, x));
		engine->RegisterObjectProperty("MyRefBase", "float y", asOFFSET(MyRefBase, y));
		engine->RegisterObjectMethod("MyRefBase", "int Sum() const", asFUNCTION(MyRefBase_Sum), asCALL_CDECL_OBJFIRST);
		engine->RegisterObjectMethod("MyRefBase", "void SetXY(int, float)", asFUNCTION(MyRefBase_SetXY), asCALL_CDECL_OBJFIRST);
		engine->RegisterObjectBehaviour("MyRefBase", asBEHAVE_DESTRUCT, "void f()", asFUNCTION(MyRefBase_Dtor), asCALL_CDECL_OBJLAST);

		asIScriptModule* mod = engine->GetModule("test_ref_multi", asGM_ALWAYS_CREATE);
		bout.buffer = "";

		// Multi-level inheritance chain:
		//   MyRefBase (base: x, y, Sum(), SetXY())
		//   └─ Widget  (adds: id, MakeID(), GetInfo())
		//      └─ Gadget (adds: tag, MakeTag(), Describe())
		mod->AddScriptSection("test_ref_multi",
			"class Widget : MyRefBase {                         \n"
			"  int id;                                           \n"
			"  Widget() {                                        \n"
			"    SetXY(1, 2.0f);                                 \n"
			"    id = 100;                                       \n"
			"  }                                                  \n"
			"  int MakeID() const { return id + x; }              \n"
			"}                                                    \n"
			"                                                     \n"
			"class Gadget : Widget {                              \n"
			"  int tag;                                           \n"
			"  Gadget() {                                         \n"
			"    SetXY(10, 20.0f);                                \n"
			"    id = 200;                                        \n"
			"    tag = 300;                                       \n"
			"  }                                                   \n"
			"  int MakeTag() const { return tag + id + Sum(); }   \n"
			"}                                                     \n"
			"                                                      \n"
			// Function accepting middle-type handle — tests polymorphism
			"int InspectWidget(Widget @w) { return w.MakeID(); }  \n"
			"                                                      \n"
			// Function accepting base handle — broader polymorphism
			"int InspectBase(MyRefBase @b) { return b.Sum(); }    \n"
		);

		r = mod->Build();
		if (r < 0)
		{
			PRINTF("Build failed: %s\n", bout.buffer.c_str());
			TEST_FAILED;
		}

		// --- Run the complex test script ---
		r = ExecuteString(engine,
			// 1. Direct construction — access all levels of properties & methods
			"Gadget @g = Gadget();                               \n"
			"assert(g.x == 10);                                  \n"
			"assert(g.y == 20.0f);                               \n"
			"assert(g.id == 200);                                \n"
			"assert(g.tag == 300);                               \n"
			"assert(g.Sum() == 30);                              \n" // MyRefBase method
			"assert(g.MakeID() == 210);                          \n" // Widget method: 200+10
			"assert(g.MakeTag() == 530);                         \n" // Gadget method: 300+200+30
			"                                                    \n"
			// 2. Polymorphism: upcast Gadget -> Widget -> MyRefBase
			"Widget @w = g;                                      \n"
			"assert(w.x == 10);                                  \n"
			"assert(w.id == 200);                                \n"
			"assert(w.MakeID() == 210);                          \n"
			"assert(w.Sum() == 30);                              \n"
			"                                                    \n"
			"MyRefBase @b = g;                                   \n"
			"assert(b.x == 10);                                  \n"
			"assert(b.y == 20.0f);                               \n"
			"assert(b.Sum() == 30);                              \n"
			"                                                    \n"
			// 3. Object identity: modifying through base handle
			//    is visible through all derived handles
			"b.SetXY(50, 60.0f);                                 \n"
			"assert(g.x == 50);                                  \n"
			"assert(g.y == 60.0f);                               \n"
			"assert(w.x == 50);                                  \n"
			"assert(g.Sum() == 110);                             \n" // 50+60
			"assert(g.MakeID() == 250);                          \n" // 200+50
			"                                                    \n"
			// 4. Pass derived handles to base-handle functions
			"assert(InspectWidget(g) == 250);                    \n" // Gadget@ -> Widget@
			"assert(InspectBase(g) == 110);                      \n" // Gadget@ -> MyRefBase@
			"assert(InspectBase(w) == 110);                      \n" // Widget@ -> MyRefBase@
			"                                                    \n"
			// 5. Second instance — verify independent objects
			"Gadget @g2 = Gadget();                              \n"
			"g2.SetXY(1, 2.0f);                                  \n"
			"assert(g2.Sum() == 3);                              \n"
			"assert(g.Sum() == 110);                             \n" // g unchanged
			"assert(g2.MakeTag() == 503);                        \n" // 300+200+3
			"                                                    \n"
			// 6. Multiple Widget sibling from same base
			"Widget @w2 = Widget();                              \n"
			"assert(w2.x == 1);                                  \n"
			"assert(w2.id == 100);                               \n"
			"assert(w2.MakeID() == 101);                         \n" // 100+1
			"assert(w.Sum() == 110);                             \n" // Gadget handle still valid
			, mod);

		if (r != asEXECUTION_FINISHED)
		{
			PRINTF("Run failed: %s\n", bout.buffer.c_str());
			if (r == asEXECUTION_EXCEPTION)
				PRINTF("Exception: %s\n", "Ref multi-inherit exception");
			TEST_FAILED;
		}

		// Verify type info chain: Gadget -> Widget -> MyRefBase
		asITypeInfo* gt = mod->GetTypeInfoByName("Gadget");
		asITypeInfo* wt = mod->GetTypeInfoByName("Widget");
		if (!gt || !wt)
			TEST_FAILED;
		if (!gt->GetBaseType() || strcmp(gt->GetBaseType()->GetName(), "Widget") != 0)
		{
			PRINTF("Expected Gadget base to be Widget, got %s\n",
				gt->GetBaseType() ? gt->GetBaseType()->GetName() : "null");
			TEST_FAILED;
		}
		if (!wt->GetBaseType() || strcmp(wt->GetBaseType()->GetName(), "MyRefBase") != 0)
		{
			PRINTF("Expected Widget base to be MyRefBase, got %s\n",
				wt->GetBaseType() ? wt->GetBaseType()->GetName() : "null");
			TEST_FAILED;
		}
		if (!gt->DerivesFrom(engine->GetTypeInfoByName("Widget")))
			TEST_FAILED;
		if (!gt->DerivesFrom(engine->GetTypeInfoByName("MyRefBase")))
			TEST_FAILED;
		if (!wt->DerivesFrom(engine->GetTypeInfoByName("MyRefBase")))
			TEST_FAILED;

		// GC and verify correct destructor count:
		// Two Gadget objects + one Widget object = 3 MyRefBase instances
		engine->GarbageCollect();
		engine->ShutDownAndRelease();

		if (MyRefBase::dtorCount != 3)
		{
			PRINTF("Expected 3 MyRefBase destructor calls (two Gadget + one Widget), got %d\n",
				MyRefBase::dtorCount);
			TEST_FAILED;
		}
	}

	return true;
	// ------------------------------------------------------------------
	// Test 11: Deep value type inheritance chain (two levels)
	// Tests that multi-level inheritance from a C++ value type works correctly,
	// including property access at each level, inherited methods, copy construction,
	// and type info queries.
	// ------------------------------------------------------------------
	{
		asIScriptEngine* engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);
		engine->RegisterGlobalFunction("void FloatValue(float)", asFUNCTION(FloatValue), asCALL_CDECL);
		engine->RegisterGlobalFunction("void IntValue(int)", asFUNCTION(IntValue), asCALL_CDECL);
		engine->RegisterGlobalFunction("void IntValue2(int,int)", asFUNCTION(IntValue2), asCALL_CDECL);

		engine->RegisterObjectType("Vec3", sizeof(Vec3), asOBJ_VALUE | asGetTypeTraits<Vec3>());
		engine->RegisterObjectProperty("Vec3", "float x", asOFFSET(Vec3, x));
		engine->RegisterObjectProperty("Vec3", "float y", asOFFSET(Vec3, y));
		engine->RegisterObjectProperty("Vec3", "float z", asOFFSET(Vec3, z));
		engine->RegisterObjectBehaviour("Vec3", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(Vec3_DefaultCtor), asCALL_CDECL_OBJLAST);
		engine->RegisterObjectBehaviour("Vec3", asBEHAVE_DESTRUCT, "void f()", asFUNCTION(Vec3_DefaultDtor), asCALL_CDECL_OBJLAST);
		engine->RegisterObjectBehaviour("Vec3", asBEHAVE_CONSTRUCT, "void f(const Vec3 &in)", asFUNCTION(Vec3_CopyCtor), asCALL_CDECL_OBJLAST);
		engine->RegisterObjectBehaviour("Vec3", asBEHAVE_CONSTRUCT, "void f(float,float,float)", asFUNCTION(Vec3_Ctor), asCALL_CDECL_OBJLAST);
		engine->RegisterObjectMethod("Vec3", "float Length() const", asMETHOD(Vec3, Length), asCALL_THISCALL);

		asIScriptModule* mod = engine->GetModule("test_deep_inherit", asGM_ALWAYS_CREATE);
		bout.buffer = "";

		// Two-level inheritance chain: Vec3 -> ColoredVec3 -> NamedLight
		mod->AddScriptSection("test_deep_inherit",
			"class ColoredVec3 : Vec3 {                    \n"
			"  int color;                                  \n"
			"  ColoredVec3() {                             \n"
			"    super();                                  \n"
			"    color = 0;                                \n"
			"  }                                            \n"
			"  ColoredVec3(const ColoredVec3 &other) {     \n"
			"    super(other);                             \n"
			"    color = other.color;                      \n"
			"  }                                            \n"
			"  bool IsDark() const { return color == 0; }   \n"
			"}                                              \n"
			"class NamedLight : ColoredVec3 {               \n"
			"  int nameId;                                 \n"
			"  float intensity;                            \n"
			"  NamedLight() {                              \n"
			"    super();                                  \n"
			"    nameId = -1;                              \n"
			"    intensity = 1.0f;                         \n"
			"  }                                            \n"
			"  NamedLight(const NamedLight &other) {       \n"
			"    super(other);                             \n"
			"    nameId = other.nameId;                    \n"
			"    intensity = other.intensity;              \n"
			"  }                                            \n"
			"  float GetBrightness() const {                \n"
			"    return intensity * Length();              \n"
			"  }                                            \n"
			"}                                              \n"
		);

		r = mod->Build();
		if (r < 0)
		{
			PRINTF("Build failed: %s\n", bout.buffer.c_str());
			TEST_FAILED;
			return fail;
		}

		r = ExecuteString(engine,
			// Test default construction — verify all levels
			"NamedLight nl;                                \n"
			"assert(nl.x == 0.0f);                         \n"
			"assert(nl.y == 0.0f);                         \n"
			"assert(nl.z == 0.0f);                         \n"
			"assert(nl.color == 0);                        \n"
			"assert(nl.nameId == -1);                      \n"
			"assert(nl.intensity == 1.0f);                 \n"
			// Test inherited methods (Length from Vec3)
			"assert(nl.Length() == 0.0f);                  \n"
			// Test middle-class method (IsDark from ColoredVec3)
			"assert(nl.IsDark());                          \n"
			// Test own method that chains to inherited
			"assert(nl.GetBrightness() == 0.0f);           \n"
			//Set properties at ALL three levels
			"nl.x = 3.0f;                                  \n"
			"nl.y = 4.0f;                                  \n"
			"nl.z = 0.0f;                                  \n"
			"nl.color = 0xFF0000;                          \n"
			"assert(nl.color == 0xFF0000);                \n"
			"nl.nameId = 42;                               \n"
			"nl.intensity = 2.5f;                          \n"
			"assert(nl.Length() == 5.0f);                  \n"
			"assert(!nl.IsDark());                         \n"
			"assert(nl.GetBrightness() == 12.5f);          \n"
			//Test copy construction at leaf level
			"NamedLight nl2 = nl;                          \n"
			"IntValue2(nl2.color, 0xFF0000);               \n"
			"assert(nl2.x == 3.0f);                        \n"
			"assert(nl2.y == 4.0f);                        \n"
			"assert(nl2.z == 0.0f);                        \n"
			"assert(nl2.color == 0xFF0000);                \n"
			"assert(nl2.nameId == 42);                     \n"
			"assert(nl2.intensity == 2.5f);                \n"
			// Verify independent copy (deep copy semantics)
			"nl2.x = 99.0f;                                \n"
			"nl2.color = 1;                                \n"
			"nl2.intensity = 0.5f;                         \n"
			"assert(nl.x == 3.0f);                         \n"
			"assert(nl.color == 0xFF0000);                 \n"
			"assert(nl.intensity == 2.5f);                 \n"
			, mod);
		if (r != asEXECUTION_FINISHED)
		{
			if (r == asEXECUTION_EXCEPTION)
				PRINTF("Exception: %s\n", "Deep inheritance exception");
			TEST_FAILED;
		}

		// Verify type info — leaf derives from both middle and root
		asITypeInfo* leafType = mod->GetTypeInfoByName("NamedLight");
		asITypeInfo* midType = mod->GetTypeInfoByName("ColoredVec3");
		if (!leafType || !midType)
			TEST_FAILED;
		if (!leafType->DerivesFrom(engine->GetTypeInfoByName("Vec3")))
			TEST_FAILED;
		if (!leafType->DerivesFrom(midType))
			TEST_FAILED;
		if (strcmp(leafType->GetBaseType()->GetName(), "ColoredVec3") != 0)
		{
			PRINTF("Expected direct base ColoredVec3, got %s\n", leafType->GetBaseType()->GetName());
			TEST_FAILED;
		}

		engine->GarbageCollect();
		engine->ShutDownAndRelease();
	}

	// ------------------------------------------------------------------
	// Test ?: Ref-type base class inheritance
	// ------------------------------------------------------------------
	{
		asIScriptEngine* engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
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

		asIScriptModule* mod = engine->GetModule("test_ref", asGM_ALWAYS_CREATE);
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
			return false;
		}
		MyRefBase::dtorCount = 0;
		auto Context = engine->CreateContext();
		r = ExecuteString(engine,
			"Widget w;                               \n"
			"assert(w.x == 10);                      \n" // Inherited property
			"assert(w.y == 20.0f);                   \n"
			"assert(w.id == 30);                     \n" // Own property = 10+20
			"assert(w.Calc() == 60);                 \n" // Sum()=30, id=30 => 60
			"w.SetXY(5, 3.0f);                       \n" // Inherited method
			"assert(w.x == 5);                       \n"
			"assert(w.Sum() == 8);                   \n" // Inherited method directly
			, mod, Context);
		if (r != asEXECUTION_FINISHED)
		{
			if (Context && r == asEXECUTION_EXCEPTION)
				PRINTF("Exception: %s\n", Context->GetExceptionString());
			TEST_FAILED;
		}
		asITypeInfo* wt = mod->GetTypeInfoByName("Widget");
		if (!wt || !wt->GetBaseType() || strcmp(wt->GetBaseType()->GetName(), "MyRefBase") != 0)
			TEST_FAILED;
		if (!wt->DerivesFrom(engine->GetTypeInfoByName("MyRefBase")))
			TEST_FAILED;

		Context->Release();
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

	// ------------------------------------------------------------------
	// Test 14: Value type — calling C++ function with derived class as base type
	// Tests that a script class derived from a C++ value type can be passed
	// to a C++ global function that expects the base type by reference.
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
		engine->RegisterGlobalFunction("float SumVec3(const Vec3 &in)", asFUNCTION(SumVec3), asCALL_CDECL);
		engine->RegisterGlobalFunction("float SumVec3_Obj(Vec3)", asFUNCTION(SumVec3_Obj), asCALL_CDECL);

		asIScriptModule* mod = engine->GetModule("test_derive_call_cpp", asGM_ALWAYS_CREATE);
		bout.buffer = "";

		mod->AddScriptSection("test_derive_call_cpp",
			"class Entity : Vec3 {                           \n"
			"  int id;                                       \n"
			"  Entity() {                                    \n"
			"    super(2.0f, 3.0f, 4.0f);                    \n"
			"    id = 42;                                    \n"
			"  }                                              \n"
			"}                                                \n"
		);

		r = mod->Build();
		if (r < 0)
		{
			PRINTF("Build failed: %s\n", bout.buffer.c_str());
			TEST_FAILED;
			return fail;
		}

		r = ExecuteString(engine,
			"Entity e;                                       \n"
			"assert(SumVec3(e) == 9.0f);                     \n"
			"assert(SumVec3_Obj(e) == 9.0f);                     \n"
			, mod);
		if (r != asEXECUTION_FINISHED)
		{
			if (r == asEXECUTION_EXCEPTION)
				PRINTF("Exception: %s\n", "Derive call C++ exception");
			TEST_FAILED;
		}

		engine->GarbageCollect();
		engine->ShutDownAndRelease();
	}

	// ------------------------------------------------------------------
	// Test 12: Return derived value type by value from function
	// Tests that returning a derived object from a function correctly
	// invokes copy constructors at each level of the inheritance chain.
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
		engine->RegisterObjectBehaviour("Vec3", asBEHAVE_DESTRUCT, "void f()", asFUNCTION(Vec3_DefaultDtor), asCALL_CDECL_OBJLAST);
		engine->RegisterObjectBehaviour("Vec3", asBEHAVE_CONSTRUCT, "void f(const Vec3 &in)", asFUNCTION(Vec3_CopyCtor), asCALL_CDECL_OBJLAST);
		engine->RegisterObjectBehaviour("Vec3", asBEHAVE_CONSTRUCT, "void f(float,float,float)", asFUNCTION(Vec3_Ctor), asCALL_CDECL_OBJLAST);
		engine->RegisterObjectMethod("Vec3", "float Length() const", asMETHOD(Vec3, Length), asCALL_THISCALL);

		asIScriptModule* mod = engine->GetModule("test_return_byval", asGM_ALWAYS_CREATE);
		bout.buffer = "";

		mod->AddScriptSection("test_return_byval",
			"class EntityPos : Vec3 {                      \n"
			"  int id;                                     \n"
			"  EntityPos() {                               \n"
			"    super(1.0f, 2.0f, 3.0f);                  \n"
			"    id = -1;                                  \n"
			"  }                                            \n"
			"  EntityPos(const EntityPos &other) {          \n"
			"    super(other);                             \n"
			"    id = other.id;                            \n"
			"  }                                            \n"
			"  void SetID(int i) { id = i; }                \n"
			"}                                              \n"
			"EntityPos MakeEntity(int val) {                \n"
			"  EntityPos e;                                \n"
			"  e.SetID(val);                               \n"
			"  return e;                                   \n"
			"}                                              \n"
		);

		r = mod->Build();
		if (r < 0)
		{
			PRINTF("Build failed: %s\n", bout.buffer.c_str());
			TEST_FAILED;
			return fail;
		}

		r = ExecuteString(engine,
			// Function returns by value — exercises copy ctor chain
			"EntityPos result = MakeEntity(77);            \n"
			"assert(result.x == 1.0f);                     \n"
			"assert(result.y == 2.0f);                     \n"
			"assert(result.z == 3.0f);                     \n"
			"assert(result.id == 77);                      \n"
			// Calling again with different value
			"EntityPos result2 = MakeEntity(99);            \n"
			"assert(result2.id == 99);                     \n"
			"assert(result2.x == 1.0f);                    \n"
			// Verify original is unchanged after second call
			"assert(result.id == 77);                      \n"
			, mod);
		if (r != asEXECUTION_FINISHED)
		{
			if (r == asEXECUTION_EXCEPTION)
				PRINTF("Exception: %s\n", "Return by value exception");
			TEST_FAILED;
		}

		engine->GarbageCollect();
		engine->ShutDownAndRelease();
	}

	// ------------------------------------------------------------------
	// Test 13: Destructor chaining for derived value types
	// Verifies that the Vec3 C++ destructor is called correctly when
	// derived script objects are created and destroyed.
	// ------------------------------------------------------------------
	{
		asIScriptEngine* engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
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
		engine->RegisterObjectMethod("Vec3", "float Length() const", asMETHOD(Vec3, Length), asCALL_THISCALL);

		asIScriptModule* mod = engine->GetModule("test_dtor_chain", asGM_ALWAYS_CREATE);
		bout.buffer = "";

		mod->AddScriptSection("test_dtor_chain",
			"class Entity : Vec3 {                         \n"
			"  int id;                                     \n"
			"  Entity() {                                  \n"
			"    super(1.0f, 2.0f, 3.0f);                  \n"
			"    id = 0;                                   \n"
			"  }                                            \n"
			"}                                              \n"
			"void TestScope() {                            \n"
			"  Entity e;                                   \n"
			"  assert(e.x == 1.0f);                        \n"
			"  assert(e.y == 2.0f);                        \n"
			"  assert(e.z == 3.0f);                        \n"
			"  {                                           \n"
			"    Entity inner;                             \n"
			"    assert(inner.x == 1.0f);                  \n"
			"    /* inner goes out of scope here */        \n"
			"  }                                            \n"
			"  /* e still alive here */                     \n"
			"}                                              \n"
		);

		r = mod->Build();
		if (r < 0)
		{
			PRINTF("Build failed: %s\n", bout.buffer.c_str());
			TEST_FAILED;
			return fail;
		}

		r = ExecuteString(engine,
			"TestScope();                                  \n"
			, mod);
		if (r != asEXECUTION_FINISHED)
		{
			if (r == asEXECUTION_EXCEPTION)
				PRINTF("Exception: %s\n", "Dtor chain exception");
			TEST_FAILED;
		}

		engine->GarbageCollect();
		engine->ShutDownAndRelease();

		// Two objects created in TestScope: e and inner
		// Plus TempEntity for... actually just e and inner = 2 Vec3 destructor calls
		// script function call creates a temporary context object too
		// At minimum: 2 Vec3 destructor calls (e + inner)
		if (Vec3::dtorCount < 2)
		{
			PRINTF("Expected at least 2 Vec3 destructor calls, got %d\n", Vec3::dtorCount);
			TEST_FAILED;
		}
	}

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

	// ------------------------------------------------------------------
	// Test ?: Cannot inherit from final class
	// ------------------------------------------------------------------
	{
		asIScriptEngine *engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		bout.buffer = "";

		engine->RegisterObjectType("MyFinal", 0, asOBJ_REF);
		engine->RegisterObjectBehaviour("MyFinal", asBEHAVE_FACTORY, "MyFinal@ f()", asFUNCTION(MyFinal_Factory), asCALL_CDECL);
		engine->RegisterObjectBehaviour("MyFinal", asBEHAVE_ADDREF, "void f()", asMETHOD(MyFinal, AddRef), asCALL_THISCALL);
		engine->RegisterObjectBehaviour("MyFinal", asBEHAVE_RELEASE, "void f()", asMETHOD(MyFinal, Release), asCALL_THISCALL);

		asIScriptModule *mod = engine->GetModule("test_final", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test", "class Bad : MyFinal {} \n");
		r = mod->Build();
		if (r >= 0)
			TEST_FAILED;
		if (bout.buffer.find("Base class doesn't have default constructor. Make explicit call to base constructor") == std::string::npos)
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
