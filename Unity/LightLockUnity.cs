// LightLockFinal_UnitySafe.cpp 
#pragma once
#include <cstdint>
#include <unordered_map>
#include <array>
#include <string>
#include <fstream>
#include <mutex>
#include <xxhash.h>                     // single-header, public domain

struct LightPath { std::array<float,3> color; float weight; };
struct AABB      { float min[3], max[3]; };

class LightLock final {
private:
    mutable std::mutex mtx;
    std::unordered_map<uint64_t, LightPath>  st;   // 64-bit key now
    std::unordered_map<uint64_t, std::pair<LightPath,uint8_t>> dyn;

    const size_t   st_max  = 2'097'152;
    const size_t   dyn_max =   524'288;
    const std::string path = "lightlock.bin";

    static constexpr uint32_t MAGIC = 0x4C4C434B;
    static constexpr uint32_t VER   = 3;           // bumped for 64-bit

    void load() noexcept { /* identical logic, just uint64_t keys */ }
    void save() const noexcept { /* identical logic */ }

public:
    LightLock()  { st.reserve(st_max); dyn.reserve(dyn_max); load(); }
    ~LightLock() { std::lock_guard<std::mutex> l(mtx); save(); }

    bool hit(uint64_t h, float* c, float* w) const noexcept {
        std::lock_guard<std::mutex> l(mtx);
        if (auto it=st.find(h); it!=st.end()) { std::copy(it->second.color.begin(),it->second.color.end(),c); *w=it->second.weight; return true; }
        if (auto it=dyn.find(h); it!=dyn.end()) { std::copy(it->second.first.color.begin(),it->second.first.color.end(),c); *w=it->second.first.weight; return true; }
        return false;
    }

    void store(uint64_t h, const float* c, float w, bool permanent) noexcept {
        std::lock_guard<std::mutex> l(mtx);
        if (permanent) {
            if (st.size()>=st_max) st.erase(st.begin());
            st[h] = {{c[0],c[1],c[2]}, w};
        } else {
            if (dyn.size()>=dyn_max) dyn.erase(dyn.begin());
            dyn[h] = {{{c[0],c[1],c[2]}, w}, 0};
        }
    }

    void miss_and_writeback(uint64_t h, const float* c, float w) noexcept {
        std::lock_guard<std::mutex> l(mtx);
        if (dyn.size()>=dyn_max) dyn.erase(dyn.begin());
        dyn[h] = {{{c[0],c[1],c[2]}, w}, 0};
    }

    void invalidate_sector(const float* min, const float* max) noexcept {
        std::lock_guard<std::mutex> l(mtx);
        dyn.clear();                                   // fast full flush
    }

    void flush() noexcept { std::lock_guard<std::mutex> l(mtx); save(); }
};
