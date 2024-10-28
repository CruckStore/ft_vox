#include <classes/Game/Game.hpp>
#include <classes/Profiler.hpp>
#include <memory>
#include <thread>
#include <vector>
#include <string>
#include <iostream>
#include <algorithm>
#include <exception>

// Configuration pour le profilage via des macros de préprocesseur
#ifdef ENABLE_PROFILER
    #define START_PROFILING(name) Profiler::StartTracking(name)
    #define STOP_PROFILING(name) Profiler::StopTracking(name)
    #define LOG_PROFILER_DATA() Profiler::LogData()
#else
    #define START_PROFILING(name)
    #define STOP_PROFILING(name)
    #define LOG_PROFILER_DATA()
#endif

// Scoped Profiler pour un tracking automatique basé sur le scope
class ScopedProfiler {
public:
    explicit ScopedProfiler(const std::string &name) : name(name) {
        START_PROFILING(name);
    }

    ~ScopedProfiler() {
        STOP_PROFILING(name);
    }

private:
    std::string name;
};

// Simple ThreadPool class for managing threads
class ThreadPool {
public:
    ThreadPool(size_t numThreads) {
        for (size_t i = 0; i < numThreads; ++i) {
            workers.emplace_back([this] {
                while (true) {
                    // Placez votre logique de thread ici...
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            });
        }
    }

    ~ThreadPool() {
        for (std::thread &worker : workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

private:
    std::vector<std::thread> workers;
};

int main(int argc, char **argv) {
    // Activer le profilage si un argument est passé
    if (argc > 1) {
        Profiler::SetSaveOn();
    }

    // Déterminer le nombre de threads en fonction de l'entrée utilisateur ou des capacités matérielles
    unsigned int numThreads = std::thread::hardware_concurrency();
    if (argc > 1) {
        try {
            numThreads = std::stoi(argv[1]);
        } catch (const std::exception &e) {
            std::cerr << "Erreur: argument invalide, utilisation par défaut des threads matériels. (" << e.what() << ")" << std::endl;
        }
    }

    // S'assurer que le nombre de threads est raisonnable
    numThreads = std::clamp(numThreads, 1u, std::thread::hardware_concurrency());

    // Créer un pool de threads avec le nombre de threads déterminé
    ThreadPool pool(numThreads);

    {
        // Utiliser le ScopedProfiler pour un tracking propre et automatique
        ScopedProfiler profiler("Game Constructor");

        // Créer une instance de Game avec un pointeur intelligent
        std::unique_ptr<Game> game = std::make_unique<Game>();

        // Démarrer la boucle principale du jeu
        game->StartLoop();
    }

    // Enregistrer les données de profilage si activé
    LOG_PROFILER_DATA();

    return 0;
}
