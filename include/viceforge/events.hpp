#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
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
        const std::type_index key(typeid(EventT));
        auto next_list = std::make_shared<std::vector<ListenerEntry>>();
        if (auto it = listeners_.find(key); it != listeners_.end() && it->second) {
            *next_list = *it->second;
        }
        next_list->push_back(ListenerEntry{
            id,
            priority,
            [fn = std::move(handler)](void* event_ptr) { fn(*static_cast<EventT*>(event_ptr)); }
        });
        std::stable_sort(next_list->begin(), next_list->end(),
                         [](const ListenerEntry& a, const ListenerEntry& b) {
                             return static_cast<int>(a.priority) < static_cast<int>(b.priority);
                         });
        listeners_.insert_or_assign(key, std::move(next_list));
        return id;
    }

    template <typename EventT>
    SubscriptionId subscribe(std::function<void(EventT&)> handler) {
        return subscribe<EventT>(EventPriority::Normal, std::move(handler));
    }

    template <typename EventT>
    bool unsubscribe(SubscriptionId id) {
        auto it = listeners_.find(std::type_index(typeid(EventT)));
        if (it == listeners_.end() || !it->second) {
            return false;
        }
        const auto& current = *it->second;
        auto next_list = std::make_shared<std::vector<ListenerEntry>>();
        next_list->reserve(current.size());
        bool removed = false;
        for (const auto& entry : current) {
            if (entry.id == id) {
                removed = true;
            } else {
                next_list->push_back(entry);
            }
        }
        if (!removed) {
            return false;
        }
        it->second = std::move(next_list);
        return true;
    }

    template <typename EventT>
    void post(EventT& event) const {
        auto it = listeners_.find(std::type_index(typeid(EventT)));
        if (it == listeners_.end() || !it->second) {
            return;
        }
        // Hold an immutable reference-counted snapshot for the loop duration: zero allocations per tick.
        const auto snapshot = it->second;
        for (const auto& entry : *snapshot) {
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
    std::unordered_map<std::type_index, std::shared_ptr<const std::vector<ListenerEntry>>> listeners_;
};

} // namespace viceforge
