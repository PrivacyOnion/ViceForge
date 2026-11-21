#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace viceforge {

enum class EventPriority : int {
    Highest = 0,
    High = 1,
    Normal = 2,
    Low = 3,
    Lowest = 4
};

using SubscriptionId = uint64_t;

class EventBus {
public:
    template <typename EventT>
    SubscriptionId subscribe(EventPriority priority, std::function<void(EventT&)> handler) {
        const SubscriptionId id = next_id_++;
        auto& bucket = listeners_[std::type_index(typeid(EventT))];
        bucket.push_back(ListenerEntry{
            id,
            priority,
            [fn = std::move(handler)](void* event_ptr) { fn(*static_cast<EventT*>(event_ptr)); }
        });
        std::stable_sort(bucket.begin(), bucket.end(), [](const ListenerEntry& a, const ListenerEntry& b) {
            return static_cast<int>(a.priority) < static_cast<int>(b.priority);
        });
        return id;
    }

    template <typename EventT>
    SubscriptionId subscribe(std::function<void(EventT&)> handler) {
        return subscribe<EventT>(EventPriority::Normal, std::move(handler));
    }

    template <typename EventT>
    bool unsubscribe(SubscriptionId id) {
        auto it = listeners_.find(std::type_index(typeid(EventT)));
        if (it == listeners_.end()) {
            return false;
        }
        auto& vec = it->second;
        const size_t before = vec.size();
        vec.erase(std::remove_if(vec.begin(), vec.end(),
                                 [id](const ListenerEntry& entry) { return entry.id == id; }),
                  vec.end());
        return vec.size() != before;
    }

    template <typename EventT>
    void post(EventT& event) const {
        auto it = listeners_.find(std::type_index(typeid(EventT)));
        if (it == listeners_.end()) {
            return;
        }
        // Snapshot the listener vector so callbacks can inspect state safely.
        const auto snapshot = it->second;
        for (const auto& entry : snapshot) {
            entry.callback(&event);
            if constexpr (requires(EventT& e) { e.cancellable; e.cancelled; }) {
                if (event.cancellable && event.cancelled) {
                    break;
                }
            }
        }
    }

    void clear() { listeners_.clear(); }

private:
    struct ListenerEntry {
        SubscriptionId id = 0;
        EventPriority priority = EventPriority::Normal;
        std::function<void(void*)> callback;
    };

    SubscriptionId next_id_ = 1;
    std::unordered_map<std::type_index, std::vector<ListenerEntry>> listeners_;
};

} // namespace viceforge
