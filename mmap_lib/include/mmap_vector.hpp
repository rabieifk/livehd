//  This file is distributed under the BSD 3-Clause License. See LICENSE for details.
#pragma once

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <thread>
#include <cassert>
#include <functional>
#include <iostream>
#include <atomic>

#include "mmap_gc.hpp"

namespace mmap_lib {
#define MMAPA_MIN_ENTRIES  (1ULL << 10)
#define MMAPA_INCR_ENTRIES (1ULL << 14)
#define MMAPA_MAX_ENTRIES  (1ULL << 31)

template <typename T>
class vector {
protected:
  size_t set_mmap_file() const {
    assert(!mmap_name.empty());

    if (mmap_fd < 0) {
      mmap_fd = mmap_gc::open(mmap_name);
    }

    // Get the size of the file
    struct stat s;
    int         status = fstat(mmap_fd, &s);
    /* LCOV_EXCL_START */
    if (status < 0) {
      std::cerr << "ERROR: Could not check file status " << mmap_name << std::endl;
      exit(-3);
    }
    /* LCOV_EXCL_STOP */

    return s.st_size;
  }

  void truncate_file_adjust_mmap_size(size_t file_size) const {
    if (file_size > mmap_size && (file_size < (4 * mmap_size))) {
      // If the mmap was there, reuse as long as it was not huge
      mmap_size = file_size;
      assert(file_size > 4096);
    }
    if (mmap_size != file_size) {
      int ret = ftruncate(mmap_fd, mmap_size);
      /* LCOV_EXCL_START */
      if (ret < 0) {
        std::cerr << "ERROR: ftruncate could not resize  " << mmap_name << " to " << mmap_size << "\n";
        mmap_base = 0;
        exit(-1);
      }
      /* LCOV_EXCL_STOP */
      file_size = mmap_size;
    }
  }

  void grow_mmap_size(size_t n) const {
    if (n < MMAPA_MIN_ENTRIES)
      n = MMAPA_MIN_ENTRIES;

    size_t req_size = sizeof(T) * n + 4096;
    if (mmap_size == 0) {
      mmap_size = req_size;
    } else if (mmap_size <= req_size) {
      mmap_size += mmap_size / 2;  // 1.5 every time
      while (mmap_size <= req_size) {
        mmap_size += MMAPA_INCR_ENTRIES;
      }
      assert(mmap_size <= MMAPA_MAX_ENTRIES * sizeof(T));
    }
  }

  void adjust_mmap(size_t old_mmap_size) const {
    assert(mmap_base != nullptr);
    assert(mmap_size != old_mmap_size);  // waste of time. Who call it?

    void *base;
    std::tie(base, mmap_size) = mmap_gc::remap(mmap_name, mmap_base, old_mmap_size, mmap_size);
    mmap_base                 = reinterpret_cast<uint8_t *>(base);
    entries_capacity          = (mmap_size - 4096) / sizeof(T);
    entries_size              = (uint64_t *)mmap_base;  // First word in mmap
  }

  void setup_mmap() const {
    assert(mmap_base == nullptr);
    assert(mmap_size >= calc_min_mmap_size());

    void *base;
    std::tie(base, mmap_size) = mmap_gc::mmap(mmap_name,
                                              mmap_fd,
                                              mmap_size,
                                              std::bind(&vector<T>::gc_done, this, std::placeholders::_1, std::placeholders::_2));

    entries_capacity = (mmap_size - 4096) / sizeof(T);
    mmap_base        = reinterpret_cast<uint8_t *>(base);

    entries_size = (uint64_t *)mmap_base;  // First word in mmap
  }

  __attribute__((noinline)) T *reserve_int(size_t n) const {
    if (mmap_base == nullptr) {
      assert(mmap_fd < 0);
      assert(mmap_size == 0);

      grow_mmap_size(n);

      if (!mmap_name.empty()) {
        size_t file_size = set_mmap_file();
        truncate_file_adjust_mmap_size(file_size);
      }

      setup_mmap();

      assert(mmap_base);
      assert(mmap_size >= (sizeof(T) * n + 4096));
      assert(entries_capacity >= n);
      return (T *)(&mmap_base[4096]);
    }

    if (mmap_size > (sizeof(T) * n + 4096)) {
      assert(entries_capacity > n);
      return (T *)(&mmap_base[4096]);
    }

    assert(mmap_base != nullptr);
    assert(mmap_size);

    auto old_mmap_size = mmap_size;
    grow_mmap_size(n);
    if (!mmap_name.empty()) {
      truncate_file_adjust_mmap_size(old_mmap_size);
    }

    adjust_mmap(old_mmap_size);

    return (T *)(&mmap_base[4096]);
  }

  mutable uint8_t  *mmap_base;
  mutable uint64_t *entries_size;
  mutable size_t    entries_capacity;  // size/sizeof - space_control
  mutable size_t    mmap_size;
  mutable int       mmap_fd;
  const std::string mmap_path;
  const std::string mmap_name;

  bool gc_done(void *base, bool force_recycle) const {
    //std::cerr << "trying GC for " << mmap_name << " mutex:" << in_use_mutex.load() << "\n";
    bool lock_was_set = std::atomic_exchange_explicit(&in_use_mutex, true, std::memory_order_relaxed);
    if (lock_was_set)
      return false; // lock in use, abort!!

    //std::cerr << " ACK   GC for " << mmap_name << " mutex:" << in_use_mutex.load() << "\n";

    (void)base;
    assert(base == mmap_base);

    mmap_base    = nullptr; // first thing (atomic pointer)

    if (mmap_fd >= 0 && *entries_size == 0) {
      unlink(mmap_name.c_str());
    }

    entries_size = nullptr;
    mmap_fd      = -1;
    // entries_capacity = 0;

    in_use_mutex.store(false, std::memory_order_release);
    //std::cerr << " REL   GC for " << mmap_name << " mutex:" << in_use_mutex.load() << "\n";
    return true;
  }

  size_t calc_min_mmap_size() const { return sizeof(T) * MMAPA_MIN_ENTRIES + 4096; }

  T *ref_base() const {
    if (MMAP_LIB_LIKELY(mmap_base != nullptr)) {
      return (T *)(mmap_base + 4096);
    }
    if (mmap_name.empty()) {
      mmap_size = calc_min_mmap_size();
    } else {
      size_t file_sz = set_mmap_file();
      if (file_sz == 0) {
        mmap_size = calc_min_mmap_size();
      } else {
        mmap_size = file_sz;
      }
    }
    setup_mmap();
    assert(mmap_base);

    return (T *)(&mmap_base[4096]);
  }

  mutable std::atomic<bool> in_use_mutex{false};

public:
  explicit vector(std::string_view _path, std::string_view _map_name)
      : mmap_base(0)
      , entries_size(nullptr)
      , entries_capacity(0)
      , mmap_size(0)
      , mmap_fd(-1)
      , mmap_path(_path.empty() ? "." : _path)
      , mmap_name{std::string(_path) + std::string("/") + std::string(_map_name)} {
    if (mmap_path != ".") {
      struct stat sb;
      if (stat(mmap_path.c_str(), &sb) != 0 || !S_ISDIR(sb.st_mode)) {
        int e = mkdir(mmap_path.c_str(), 0755);
        (void)e;
        assert(e >= 0);
      }
    }
    if (!mmap_name.empty()) {
      struct stat sb;
      if (stat(mmap_name.c_str(), &sb) == 0) {
        assert(S_ISREG(sb.st_mode));
        entries_capacity = (sb.st_size - 4096) / sizeof(T);
      }
    }
  }

  explicit vector() : mmap_base(0), entries_size(nullptr), entries_capacity(0), mmap_size(0), mmap_fd(-1) {}

  ~vector() {
    if (mmap_base) {
      // WARNING: can not call in_use because recycle needs in_use==0
      mmap_gc::recycle(mmap_base);
      assert(mmap_base == nullptr);
    }
  }

  // Allocates space, but it does not touch contents
  void reserve(size_t n) const {
    // Single thread check: assert(in_use_mutex == 0);
    while(std::atomic_exchange_explicit(&in_use_mutex, true, std::memory_order_relaxed))
      ;

    reserve_int(n);

    in_use_mutex.store(false, std::memory_order_release);
  }

  void emplace_back() {
    // Single thread check: assert(in_use_mutex == 0);
    while(std::atomic_exchange_explicit(&in_use_mutex, true, std::memory_order_relaxed))
      ;

    ref_base();
    if (MMAP_LIB_UNLIKELY(capacity() <= *entries_size)) {
      reserve_int(size() + 1);
    }
    (*entries_size)++;

    in_use_mutex.store(false, std::memory_order_release);
  }

  template <class... Args>
  void emplace_back(Args &&...args) {
    // Single thread check: assert(in_use_mutex == 0);
    while(std::atomic_exchange_explicit(&in_use_mutex, true, std::memory_order_relaxed))
      ;

    auto *base = ref_base();
    assert(entries_size);
    if (MMAP_LIB_UNLIKELY(capacity() <= *entries_size)) {
      base = reserve_int(size() + 1);
    }

    base[*entries_size] = T(std::forward<Args>(args)...);
    (*entries_size)++;

    in_use_mutex.store(false, std::memory_order_release);
  }

  void ref_lock() const {
    // Single thread check: assert(in_use_mutex == 0);
    while(std::atomic_exchange_explicit(&in_use_mutex, true, std::memory_order_relaxed))
      ;
  }

  void ref_unlock() const {
    assert(in_use_mutex);
    in_use_mutex.store(false, std::memory_order_release);
  }

  template <class... Args>
  void set(const size_t idx, Args &&...args) {
    // Single thread check: assert(in_use_mutex == 0);
    while(std::atomic_exchange_explicit(&in_use_mutex, true, std::memory_order_relaxed))
      ;

    auto *base = ref_base();
    assert(base);
    assert(idx < capacity());

    base[idx] = T(std::forward<Args>(args)...);

    in_use_mutex.store(false, std::memory_order_release);
  }

  [[nodiscard]] inline const T *ref(size_t const &idx) const {
    assert(in_use_mutex);

    const auto *base = ref_base();
    assert(idx < size());
    return &base[idx];
  }

  [[nodiscard]] inline T *ref(size_t const &idx) {
    assert(in_use_mutex);

    auto *base = ref_base();
    assert(idx < size());
    return &base[idx];
  }

  [[nodiscard]] inline const T operator[](const size_t idx) const {
    // Single thread check: assert(in_use_mutex == 0);
    while(std::atomic_exchange_explicit(&in_use_mutex, true, std::memory_order_relaxed))
      ;

    const auto *base = ref_base();
    assert(idx < size());
    const auto copy = base[idx];

    in_use_mutex.store(false, std::memory_order_release);
    return copy;
  }

  T *      begin() {
    assert(in_use_mutex);
    return ref_base();
  }
  const T *cbegin() const {
    assert(in_use_mutex);
    return ref_base();
  }
  const T *begin() const {
    assert(in_use_mutex);
    return ref_base();
  }

  T *end() {
    assert(in_use_mutex);
    auto *base = ref_base();
    return &base[*entries_size];
  }

  const T *cend() const {
    assert(in_use_mutex);
    const auto *base = ref_base();
    return &base[*entries_size];
  }

  const T *end() const {
    assert(in_use_mutex);
    const auto *base = ref_base();
    return &base[*entries_size];
  }

  void clear() {
    // WARNING: no need to block gc in this method

    if (mmap_base == nullptr) {
      assert(mmap_base == nullptr);
      assert(mmap_fd < 0);
      if (!mmap_name.empty())
        unlink(mmap_name.c_str());

      return;
    }
    if (*entries_size != 0) {
      *entries_size = 0;
    }

    *entries_size = 0;  // Setting zero, triggers an unlink when calling gc_done

    mmap_gc::recycle(mmap_base);
    entries_capacity = 0;
    assert(entries_size == nullptr);
  }

  [[nodiscard]] inline std::string_view get_name() const { return mmap_name; }
  [[nodiscard]] inline std::string_view get_path() const { return mmap_path; }

  [[nodiscard]] inline size_t capacity() const { return entries_capacity; }

#if 0
  uint64_t *ref_config_data(int offset) const {
    assert(offset < 4096 / 8);
    assert(offset > 0);

    ref_base();  // Force to get mmap_base
    assert(mmap_base);

    return (uint64_t *)&mmap_base[offset];
  }
#endif

  [[nodiscard]] size_t size() const {
    if (entries_size != nullptr) {
      return *entries_size;
    }
    if (entries_capacity == 0) {  // Never mmap_base and files does not exist
      return 0;
    }

    // Single thread check: assert(in_use_mutex == 0);
    while(std::atomic_exchange_explicit(&in_use_mutex, true, std::memory_order_relaxed))
      ;

    ref_base();  // Force to get entries_size
    auto v = *entries_size;
    in_use_mutex.store(false, std::memory_order_release);

    return v;
  }

  bool empty() const { return size() == 0; }
};

}  // namespace mmap_lib
