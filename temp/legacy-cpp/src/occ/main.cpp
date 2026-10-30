#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <sys/stat.h>
#include <dirent.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

// ============================================================
// occ.exe — Omni Core Compiler Pipeline Manager
// ============================================================

class DynLib {
#ifdef _WIN32
    HMODULE handle = nullptr;
#else
    void* handle = nullptr;
#endif

public:
    explicit DynLib(const std::string& path) {
#ifdef _WIN32
        handle = LoadLibraryA(path.c_str());
#else
        handle = dlopen(path.c_str(), RTLD_LAZY);
#endif
    }
    ~DynLib() {
        if (handle) {
#ifdef _WIN32
            FreeLibrary(handle);
#else
            dlclose(handle);
#endif
        }
    }
    bool isLoaded() const { return handle != nullptr; }

    template<typename T>
    T getSymbol(const std::string& sym) {
#ifdef _WIN32
        return reinterpret_cast<T>(reinterpret_cast<void*>(GetProcAddress(handle, sym.c_str())));
#else
        return reinterpret_cast<T>(dlsym(handle, sym.c_str()));
#endif
    }
};

// Parser DLL Signatures
typedef void* (*nxf_create_session_fn)(const char*, int, const char*);
typedef int (*nxf_parse_file_fn)(void*, const char*);
typedef int (*nxf_save_oir_fn)(void*);
typedef int (*nxf_has_errors_fn)(void*);
typedef const char* (*nxf_get_errors_fn)(void*);
typedef void (*nxf_destroy_session_fn)(void*);

// Analyzer DLL Signatures
typedef void (*nxf_init_analyzer_fn)();
typedef int (*nxf_analyze_oir_fn)(const char*, const char*, int);

std::string getTarget(int argc, char** argv) {
    for (int i = 1; i < argc - 1; ++i) {
        if (std::string(argv[i]) == "-t") {
            return argv[i + 1];
        }
    }
#ifdef _WIN32
    return "Win64";
#elif defined(__linux__)
    return "Linux64";
#else
    return "Unknown";
#endif
}

std::string getSourceFile(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-t" || arg == "-d") {
            if (arg == "-t") i++; // skip target value
            continue;
        }
        return arg;
    }
    return "";
}

bool isDebugMode(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "-d") return true;
    }
    return false;
}

bool fileExists(const std::string& name) {
    struct stat buffer;   
    return (stat(name.c_str(), &buffer) == 0); 
}

void printAvailableTargets() {
    std::cout << "\nMevcut Target'lar (targets/ klasoru):\n";
    DIR *dir;
    struct dirent *ent;
    if ((dir = opendir("targets")) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            std::string fname = ent->d_name;
            if (fname.size() > 7 && fname.substr(fname.size() - 7) == ".target") {
                std::cout << "  - " << fname.substr(0, fname.size() - 7) << "\n";
            }
        }
        closedir(dir);
    } else {
        std::cout << "  (targets/ klasoru bulunamadi veya erisilemiyor!)\n";
    }
}

int main(int argc, char** argv) {
    std::cout << "[OCC] Omni Core Compiler - Pipeline Manager v0.1\n";

    std::string sourceFile = getSourceFile(argc, argv);
    if (sourceFile.empty()) {
        std::cerr << "Kullanim: occ <file.nx> [-t <target>] [-d]\n";
        return 1;
    }

    std::string targetName = getTarget(argc, argv);
    std::string targetFile = "targets/" + targetName + ".target";

    while (!fileExists(targetFile)) {
        std::cerr << "[HATA] Hedef dosya bulunamadi: " << targetFile << "\n";
        printAvailableTargets();
        std::cout << "\nLutfen gecerli bir target ismi giriniz: ";
        std::cin >> targetName;
        targetFile = "targets/" + targetName + ".target";
    }

    std::cout << "[OCC] Target secildi: " << targetName << "\n";
    std::cout << "[OCC] Kaynak dosya: " << sourceFile << "\n";

    bool debugMode = isDebugMode(argc, argv);
    std::string outOirFile = "intermediate.oir";

    // ============================================================
    // STAGE 1: PARSER DLL
    // ============================================================
    std::cout << "\n--- [STAGE 1] PARSER ---\n";
#ifdef _WIN32
    std::string parserPath = "bin/parser.dll";
#else
    std::string parserPath = "lib/libparser.so";
#endif

    DynLib parserLib(parserPath);
    if (!parserLib.isLoaded()) {
        std::cerr << "[HATA] parser.dll yuklenemedi! Yolu kontrol edin: " << parserPath << "\n";
        return 1;
    }

    auto create_sess = parserLib.getSymbol<nxf_create_session_fn>("nxf_create_session");
    auto parse_file  = parserLib.getSymbol<nxf_parse_file_fn>("nxf_parse_file");
    auto save_oir    = parserLib.getSymbol<nxf_save_oir_fn>("nxf_save_oir");
    auto has_errors  = parserLib.getSymbol<nxf_has_errors_fn>("nxf_has_errors");
    auto get_errors  = parserLib.getSymbol<nxf_get_errors_fn>("nxf_get_errors");
    auto destroy_sess= parserLib.getSymbol<nxf_destroy_session_fn>("nxf_destroy_session");

    if (!create_sess || !parse_file || !save_oir || !has_errors || !get_errors || !destroy_sess) {
        std::cerr << "[HATA] parser.dll icinde gerekli C-ABI fonksiyonlari bulunamadi!\n";
        return 1;
    }

    void* session = create_sess(targetName.c_str(), debugMode ? 1 : 0, outOirFile.c_str());
    
    auto t1 = std::chrono::high_resolution_clock::now();
    parse_file(session, sourceFile.c_str());
    auto t2 = std::chrono::high_resolution_clock::now();
    
    if (has_errors(session)) {
        std::cerr << "[STAGE 1 FAIL] Parser hatalar buldu:\n" << get_errors(session) << "\n";
        std::cerr << "Pipeline durduruldu.\n";
        destroy_sess(session);
        return 1;
    }

    if (save_oir(session) != 0) {
        std::cerr << "[STAGE 1 FAIL] OIR dosyasi kaydedilemedi!\n";
        destroy_sess(session);
        return 1;
    }

    destroy_sess(session);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
    std::cout << "[STAGE 1 OK] OIR uretildi: " << outOirFile << " (" << ms << " ms)\n";

    // ============================================================
    // STAGE 2: ANALYZER DLL
    // ============================================================
    std::cout << "\n--- [STAGE 2] ANALYZER ---\n";
#ifdef _WIN32
    std::string analyzerPath = "bin/analyzer.dll";
#else
    std::string analyzerPath = "lib/libanalyzer.so";
#endif

    DynLib analyzerLib(analyzerPath);
    if (!analyzerLib.isLoaded()) {
        std::cerr << "[HATA] analyzer.dll yuklenemedi! Yolu kontrol edin: " << analyzerPath << "\n";
        return 1;
    }

    auto init_ana = analyzerLib.getSymbol<nxf_init_analyzer_fn>("nxf_init_analyzer");
    auto analyze_oir = analyzerLib.getSymbol<nxf_analyze_oir_fn>("nxf_analyze_oir");

    if (!init_ana || !analyze_oir) {
        std::cerr << "[HATA] analyzer.dll icinde asgari C-ABI fonksiyonlari bulunamadi!\n";
        return 1;
    }

    init_ana();
    if (analyze_oir(outOirFile.c_str(), sourceFile.c_str(), debugMode ? 1 : 0) != 0) {
        std::cerr << "[STAGE 2 FAIL] Semantik Analiz hatalar buldu.\nPipeline durduruldu.\n";
        return 1;
    }
    
    // ============================================================
    // STAGE 3: OPTIMIZER DLL (Placeholder)
    // ============================================================
    // std::cout << "\n--- [STAGE 3] OPTIMIZER ---\n";

    // ============================================================
    // STAGE 4: CODEGEN DLL (Placeholder)
    // ============================================================
    // std::cout << "\n--- [STAGE 4] CODEGEN ---\n";

    std::cout << "\n[OCC] Derleme orkestrasyonu basariyla tamamlandi.\n";
    return 0;
}
