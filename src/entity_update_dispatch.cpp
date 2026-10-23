#include "viper/entity_update_dispatch.hpp"

namespace viper {

std::vector<EntityVirtualCall> entity_update_callback_order(
    const std::span<const EntityUpdateDispatchInput> objects)
{
    std::vector<EntityVirtualCall> calls;
    calls.reserve(objects.size() * 5);

    struct DeferredObject {
        std::size_t object_index;
        float order_key;
    };
    std::vector<DeferredObject> deferred;
    deferred.reserve(objects.size());

    for (std::size_t object_index = 0; object_index < objects.size(); ++object_index) {
        const auto& object = objects[object_index];
        calls.push_back({object_index, 0x08});
        calls.push_back({object_index, 0x0C});
        if (!object.update_enabled) {
            continue;
        }

        calls.push_back({object_index, 0x10});
        if (!object.defer_update) {
            calls.push_back({object_index, 0x14});
            continue;
        }

        calls.push_back({object_index, 0x20});
        std::size_t insert_at = 0;
        while (insert_at < deferred.size() &&
               !(deferred[insert_at].order_key < object.deferred_order_key)) {
            ++insert_at;
        }
        deferred.insert(deferred.begin() + static_cast<std::ptrdiff_t>(insert_at),
                        {object_index, object.deferred_order_key});
    }

    for (const auto& object : deferred) {
        calls.push_back({object.object_index, 0x14});
    }
    return calls;
}

} // namespace viper
