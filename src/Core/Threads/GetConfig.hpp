#pragma once
#include <thread>
#include <chrono>
#include <Core/Lifecycle.hpp>
#include "GetConfig.cpp"

namespace Core {

    namespace Threads {

        class GetConfig {
        public:

            std::atomic<bool> running{ true };
            void Update(uint64_t worker_epoch) {
                while (running && Core::ShouldGameThreadRun(worker_epoch)) {
                    refreshconfig();
                    std::this_thread::sleep_for(std::chrono::milliseconds(1200));
                }
            }
        };
        inline GetConfig g_GetConfig;
    }
}
