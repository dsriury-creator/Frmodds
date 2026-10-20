// FR Mod - lib nativa (arm64). Sem offsets: tudo e resolvido por NOME via API do IL2CPP.
// Estrategia: troca o ponteiro de metodo de DriftCarV4.Update. Assim a nossa logica
// roda na thread principal do Unity, a cada frame, com o carro ja em maos.
#include <jni.h>
#include <dlfcn.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/mman.h>
#include <atomic>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <unordered_map>

#ifdef __ANDROID__
#include <android/log.h>
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "FRMOD", __VA_ARGS__)
#else
#define LOGI(...) (printf(__VA_ARGS__), printf("\n"))
#endif

// ---------------------------------------------------------------- flags (vindas do menu Java)
static std::atomic<int> fThick{0};      // 0 THICK SMOKE
static std::atomic<int> fDur{0};        // 1 SMOKE DURATION++
static std::atomic<int> fCarNoDmg{0};   // 2 CAR NO DAMAGE
static std::atomic<int> fTireNoDmg{0};  // 3 TIRE NO DAMAGE
static std::atomic<int> fSpeed{100};    // 4 SPEED em % (100 = normal)
static std::atomic<int> fFps{0};        // 5 FPS alvo (0 = padrao)
static std::atomic<bool> fpsDirty{false};

// ---------------------------------------------------------------- API do IL2CPP
static void* (*il2cpp_domain_get)();
static void** (*il2cpp_domain_get_assemblies)(void*, size_t*);
static void* (*il2cpp_assembly_get_image)(void*);
static const char* (*il2cpp_image_get_name)(void*);
static void* (*il2cpp_class_from_name)(void*, const char*, const char*);
static void* (*il2cpp_class_get_method_from_name)(void*, const char*, int);
static void* (*il2cpp_class_get_field_from_name)(void*, const char*);
static void* (*il2cpp_object_get_class)(void*);
static void (*il2cpp_field_get_value)(void*, void*, void*);
static void (*il2cpp_field_set_value)(void*, void*, void*);
static void* (*il2cpp_field_get_type)(void*);
static char* (*il2cpp_type_get_name)(void*);
static void* (*il2cpp_runtime_invoke)(void*, void*, void**, void**);
static void* (*il2cpp_thread_attach)(void*);

static bool resolveApi() {
    void* h = dlopen("libil2cpp.so", RTLD_NOW | RTLD_NOLOAD);
    if (!h) return false;
#define R(name)                                  \
    *(void**)&name = dlsym(h, #name);            \
    if (!name) { LOGI("falta simbolo %s", #name); return false; }
    R(il2cpp_domain_get)
    R(il2cpp_domain_get_assemblies)
    R(il2cpp_assembly_get_image)
    R(il2cpp_image_get_name)
    R(il2cpp_class_from_name)
    R(il2cpp_class_get_method_from_name)
    R(il2cpp_class_get_field_from_name)
    R(il2cpp_object_get_class)
    R(il2cpp_field_get_value)
    R(il2cpp_field_set_value)
    R(il2cpp_field_get_type)
    R(il2cpp_type_get_name)
    R(il2cpp_runtime_invoke)
    R(il2cpp_thread_attach)
#undef R
    return true;
}

static void* findImage(const char* name) {
    void* dom = il2cpp_domain_get();
    if (!dom) return nullptr;
    size_t n = 0;
    void** asms = il2cpp_domain_get_assemblies(dom, &n);
    if (!asms) return nullptr;
    for (size_t i = 0; i < n; ++i) {
        void* img = il2cpp_assembly_get_image(asms[i]);
        const char* nm = img ? il2cpp_image_get_name(img) : nullptr;
        if (nm && !strcmp(nm, name)) return img;
    }
    return nullptr;
}

// ---------------------------------------------------------------- acesso a campos por nome
// (estes helpers so rodam na thread principal, dentro do hook)
struct FR { void* f; int kind; };  // kind: 0 outro, 1 float, 2 int, 3 double
static std::map<std::pair<void*, std::string>, FR> fieldCache;

static FR getField(void* klass, const char* name) {
    auto key = std::make_pair(klass, std::string(name));
    auto it = fieldCache.find(key);
    if (it != fieldCache.end()) return it->second;
    FR r{nullptr, 0};
    r.f = il2cpp_class_get_field_from_name(klass, name);
    if (r.f) {
        void* ty = il2cpp_field_get_type(r.f);
        const char* tn = ty ? il2cpp_type_get_name(ty) : nullptr;
        if (tn) {
            if (!strcmp(tn, "System.Single")) r.kind = 1;
            else if (!strcmp(tn, "System.Int32")) r.kind = 2;
            else if (!strcmp(tn, "System.Double")) r.kind = 3;
        }
    }
    fieldCache[key] = r;
    return r;
}

static void* getRef(void* obj, const char* name) {
    if (!obj) return nullptr;
    FR r = getField(il2cpp_object_get_class(obj), name);
    if (!r.f) return nullptr;
    void* out = nullptr;
    il2cpp_field_get_value(obj, r.f, &out);
    return out;
}

static bool getNum(void* obj, const char* name, double* v) {
    if (!obj) return false;
    FR r = getField(il2cpp_object_get_class(obj), name);
    if (!r.f || !r.kind) return false;
    if (r.kind == 1) { float x = 0; il2cpp_field_get_value(obj, r.f, &x); *v = x; }
    else if (r.kind == 2) { int x = 0; il2cpp_field_get_value(obj, r.f, &x); *v = x; }
    else { double x = 0; il2cpp_field_get_value(obj, r.f, &x); *v = x; }
    return true;
}

static bool setNum(void* obj, const char* name, double v) {
    if (!obj) return false;
    FR r = getField(il2cpp_object_get_class(obj), name);
    if (!r.f || !r.kind) return false;
    if (r.kind == 1) { float x = (float)v; il2cpp_field_set_value(obj, r.f, &x); }
    else if (r.kind == 2) { int x = (int)v; il2cpp_field_set_value(obj, r.f, &x); }
    else { double x = v; il2cpp_field_set_value(obj, r.f, &x); }
    return true;
}

static std::map<std::pair<void*, std::string>, void*> methodCache;
static void callVoid(void* obj, const char* name) {
    if (!obj) return;
    void* k = il2cpp_object_get_class(obj);
    auto key = std::make_pair(k, std::string(name));
    void* m;
    auto it = methodCache.find(key);
    if (it != methodCache.end()) m = it->second;
    else { m = il2cpp_class_get_method_from_name(k, name, 0); methodCache[key] = m; }
    if (!m) return;
    void* exc = nullptr;
    il2cpp_runtime_invoke(m, obj, nullptr, &exc);
}

// ---------------------------------------------------------------- Unity estatico (FPS)
static void* g_core = nullptr;  // UnityEngine.CoreModule.dll
static int origFps = -2;

static int getStaticInt(const char* cls, const char* method) {
    void* k = il2cpp_class_from_name(g_core, "UnityEngine", cls);
    void* m = k ? il2cpp_class_get_method_from_name(k, method, 0) : nullptr;
    if (!m) return -1;
    void* exc = nullptr;
    char* r = (char*)il2cpp_runtime_invoke(m, nullptr, nullptr, &exc);
    if (exc || !r) return -1;
    return *(int*)(r + 16);  // valor dentro do objeto "boxed"
}

static void setStaticInt(const char* cls, const char* method, int val) {
    void* k = il2cpp_class_from_name(g_core, "UnityEngine", cls);
    void* m = k ? il2cpp_class_get_method_from_name(k, method, 1) : nullptr;
    if (!m) return;
    void* args[1] = {&val};
    void* exc = nullptr;
    il2cpp_runtime_invoke(m, nullptr, args, &exc);
}

static void applyFps() {
    int want = fFps.load();
    if (origFps == -2) origFps = getStaticInt("Application", "get_targetFrameRate");
    if (want > 0) {
        setStaticInt("QualitySettings", "set_vSyncCount", 0);
        setStaticInt("Application", "set_targetFrameRate", want);
    } else if (origFps != -2) {
        setStaticInt("Application", "set_targetFrameRate", origFps);
    }
}

// ---------------------------------------------------------------- logica por carro (todo frame)
static void* mIsAI = nullptr;
static bool mIsAIResolved = false;

static bool isPlayerCar(void* car, void* cls) {
    if (!mIsAIResolved) {
        mIsAI = il2cpp_class_get_method_from_name(cls, "get_IsAIMovement", 0);
        mIsAIResolved = true;
    }
    if (mIsAI) {
        void* exc = nullptr;
        char* r = (char*)il2cpp_runtime_invoke(mIsAI, car, nullptr, &exc);
        if (!exc && r && *(r + 16)) return false;  // e carro de IA
    }
    FR g = getField(cls, "<isGhost>k__BackingField");
    if (g.f) {
        unsigned char b = 0;
        il2cpp_field_get_value(car, g.f, &b);
        if (b) return false;  // fantasma
    }
    return true;
}

static std::unordered_map<void*, std::array<double, 3>> smokeOrig;
static std::unordered_map<void*, std::array<double, 2>> speedOrig;
static bool smokeActive = false, speedActive = false;
static unsigned frameNo = 0;

static void onCar(void* car) {
    if (!car) return;
    if (fpsDirty.exchange(false)) applyFps();

    void* cls = il2cpp_object_get_class(car);
    if (!isPlayerCar(car, cls)) return;
    ++frameNo;

    const bool thick = fThick.load() != 0;
    const bool dur = fDur.load() != 0;
    const bool tireNoDmg = fTireNoDmg.load() != 0;
    const bool carNoDmg = fCarNoDmg.load() != 0;
    const int spd = fSpeed.load();

    // ---- pneu: fumaca e dano
    void* tire = getRef(car, "<tire>k__BackingField");
    void* data = getRef(tire, "<data>k__BackingField");
    if (data) {
        if (thick || dur) {
            auto it = smokeOrig.find(data);
            if (it == smokeOrig.end()) {
                double a, b, c;
                if (getNum(data, "<smokeRate>k__BackingField", &a) &&
                    getNum(data, "<smokeSize>k__BackingField", &b) &&
                    getNum(data, "<smokeLife>k__BackingField", &c)) {
                    smokeOrig[data] = {a, b, c};
                    it = smokeOrig.find(data);
                }
            }
            if (it != smokeOrig.end()) {
                double m = thick ? 4.0 : 1.0;
                double l = dur ? 4.0 : 1.0;
                setNum(data, "<smokeRate>k__BackingField", it->second[0] * m);
                setNum(data, "<smokeSize>k__BackingField", it->second[1] * m);
                setNum(data, "<smokeLife>k__BackingField", it->second[2] * l);
                smokeActive = true;
            }
        } else if (smokeActive) {
            auto it = smokeOrig.find(data);
            if (it != smokeOrig.end()) {
                setNum(data, "<smokeRate>k__BackingField", it->second[0]);
                setNum(data, "<smokeSize>k__BackingField", it->second[1]);
                setNum(data, "<smokeLife>k__BackingField", it->second[2]);
            }
            smokeOrig.clear();
            smokeActive = false;
        }

        if (tireNoDmg) {
            double mx;
            if (getNum(data, "max_health", &mx)) setNum(data, "health", mx);
            setNum(tire, "wear", 0.0);
        }
    }

    // ---- velocidade: multiplica potencia e aceleracao do movimento do heroi
    void* mv = getRef(car, "hero_movement");
    if (mv) {
        if (spd != 100) {
            auto it = speedOrig.find(mv);
            if (it == speedOrig.end()) {
                double hp, ac;
                if (getNum(mv, "horsePower", &hp) && getNum(mv, "acclFactor", &ac)) {
                    speedOrig[mv] = {hp, ac};
                    it = speedOrig.find(mv);
                }
            }
            if (it != speedOrig.end()) {
                double f = spd / 100.0;
                setNum(mv, "horsePower", it->second[0] * f);
                setNum(mv, "acclFactor", it->second[1] * f);
                speedActive = true;
            }
        } else if (speedActive) {
            auto it = speedOrig.find(mv);
            if (it != speedOrig.end()) {
                setNum(mv, "horsePower", it->second[0]);
                setNum(mv, "acclFactor", it->second[1]);
            }
            speedOrig.clear();
            speedActive = false;
        }
    }

    // ---- carro sem dano: limpa as deformacoes (a cada 6 frames)
    if (carNoDmg && (frameNo % 6) == 0) {
        void* br = getRef(car, "<bodyRoll>k__BackingField");
        void* st = getRef(br, "structure");
        callVoid(st, "ClearDeforms");
    }
}

// ---------------------------------------------------------------- hook (troca do ponteiro do metodo)
typedef void (*UpdateFn)(void*, void*);
static UpdateFn origUpdate = nullptr;

static void hkUpdate(void* self, void* mi) {
    origUpdate(self, mi);
    onCar(self);
}

static bool installHook(void* klass) {
    void* mi = il2cpp_class_get_method_from_name(klass, "Update", 0);
    if (!mi) { LOGI("DriftCarV4.Update nao achado"); return false; }
    void** slot = (void**)mi;  // MethodInfo: [0]=methodPointer, [1]=virtualMethodPointer
    origUpdate = (UpdateFn)slot[0];
    uintptr_t page = (uintptr_t)slot & ~(uintptr_t)4095;
    if (mprotect((void*)page, 8192, PROT_READ | PROT_WRITE) != 0) {
        LOGI("mprotect falhou");
        return false;
    }
    void* orig = slot[0];
    slot[0] = (void*)hkUpdate;
    if (slot[1] == orig) slot[1] = (void*)hkUpdate;
    LOGI("hook instalado em DriftCarV4.Update");
    return true;
}

// ---------------------------------------------------------------- inicializacao (thread propria)
static bool g_api = false, g_attached = false;

static bool tryInit() {
    if (!g_api) {
        if (!resolveApi()) return false;
        g_api = true;
    }
    void* dom = il2cpp_domain_get();
    if (!dom) return false;
    if (!g_attached) {
        il2cpp_thread_attach(dom);
        g_attached = true;
    }
    void* img = findImage("Assembly-CSharp.dll");
    void* core = findImage("UnityEngine.CoreModule.dll");
    if (!img || !core) return false;
    void* k = il2cpp_class_from_name(img, "", "DriftCarV4");
    if (!k) return false;
    g_core = core;
    return installHook(k);
}

static void* initThread(void*) {
    for (int i = 0; i < 720; ++i) {  // tenta por ~6 minutos
        if (tryInit()) return nullptr;
        usleep(500 * 1000);
    }
    LOGI("init desistiu");
    return nullptr;
}

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM*, void*) {
    pthread_t t;
    pthread_create(&t, nullptr, initThread, nullptr);
    pthread_detach(t);
    return JNI_VERSION_1_6;
}

extern "C" JNIEXPORT void JNICALL Java_com_frmod_Native_setFlag(JNIEnv*, jclass, jint id, jint v) {
    switch (id) {
        case 0: fThick = v; break;
        case 1: fDur = v; break;
        case 2: fCarNoDmg = v; break;
        case 3: fTireNoDmg = v; break;
        case 4: fSpeed = v; break;
        case 5: fFps = v; fpsDirty = true; break;
        default: break;  // -1 = so para carregar a lib
    }
}
