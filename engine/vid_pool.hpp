#pragma once

#include "engine/vid.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

// Generic pool with stable VID handles. Provides contiguous storage with bounded
// capacity. Each slot keeps its VID (id == slot index, version increments on reuse).
template <typename T>
class VidPool {
  public:
    struct Entry {
        VID vid{};
        bool active{false};
        T value{};
    };

    VidPool() = default;
    explicit VidPool(std::size_t capacity) {
        init(capacity);
    }

    void init(std::size_t capacity) {
        entries_.clear();
        entries_.resize(capacity);
        for (std::size_t i = 0; i < entries_.size(); ++i) {
            entries_[i].vid.id = i;
            entries_[i].active = false;
            entries_[i].vid.version = 0;
            entries_[i].value = T{};
        }
    }

    std::size_t capacity() const { return entries_.size(); }

    Entry* acquire() {
        for (auto& entry : entries_) {
            if (!entry.active) {
                entry.active = true;
                entry.vid.version += 1;
                entry.value = T{};
                return &entry;
            }
        }
        return nullptr;
    }

    void release(VID vid) {
        if (Entry* slot = find_entry(vid)) {
            slot->active = false;
            slot->vid.version += 1;
        }
    }

    void clear() {
        for (auto& entry : entries_) {
            if (entry.active) {
                entry.active = false;
                entry.vid.version += 1;
            }
            entry.value = T{};
        }
    }

    Entry* find_entry(VID vid) {
        if (vid.id >= entries_.size())
            return nullptr;
        Entry& e = entries_[vid.id];
        if (!e.active || e.vid.version != vid.version)
            return nullptr;
        return &e;
    }

    const Entry* find_entry(VID vid) const {
        if (vid.id >= entries_.size())
            return nullptr;
        const Entry& e = entries_[vid.id];
        if (!e.active || e.vid.version != vid.version)
            return nullptr;
        return &e;
    }

    std::vector<Entry>& entries() { return entries_; }
    const std::vector<Entry>& entries() const { return entries_; }

  private:
    std::vector<Entry> entries_;
};
