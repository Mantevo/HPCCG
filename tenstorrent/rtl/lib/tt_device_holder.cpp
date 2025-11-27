#include "tt_device_holder.hpp"

#include <tt-metalium/host_api.hpp>
#include <tt-metalium/device_pool.hpp>

namespace tt::daisy {

// Global device pointer
static tt::tt_metal::IDevice* g_device = nullptr;

// Initialize the global device
tt::tt_metal::IDevice* get_device() {
    tt::umd::chip_id_t target_id = 0;

    if (g_device == nullptr) {
        if (tt::DevicePool::is_initialized()) {
            auto& pool = tt::DevicePool::instance();
            if (pool.is_device_active(target_id)) {
                g_device = pool.get_active_device(target_id);
            }
        }
        if (g_device == nullptr) {
            g_device = tt_metal::CreateDevice(target_id);
        }
        // Register cleanup function to run at program exit
        std::atexit([]() {
            if (g_device != nullptr) {
                if (tt::DevicePool::is_initialized() && tt::DevicePool::instance().is_device_active(0)) {
                    tt::tt_metal::CloseDevice(g_device);
                }
                g_device = nullptr;
            }
        });
    }
    return g_device;
}

}
