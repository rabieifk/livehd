//  This file is distributed under the BSD 3-Clause License. See LICENSE for details.
#pragma once

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <cassert>
#include "absl/container/flat_hash_map.h"

#ifndef MMAP_LIB_LIKELY
#define MMAP_LIB_LIKELY(x) __builtin_expect((x), 1)
#endif
#ifndef MMAP_LIB_UNLIKELY
#define MMAP_LIB_UNLIKELY(x) __builtin_expect((x), 0)
#endif

namespace mmap_lib {
struct mmap_gc_entry {
  std::string name; // Mostly for debugging
  int         fd;
  size_t      size;
  std::function<void(void *)> gc_function;
};

using gc_pool_type=absl::flat_hash_map<void *, mmap_gc_entry>;
static inline gc_pool_type mmap_gc_pool;
static inline void *next_mmap=nullptr;

class mmap_gc {
protected:
  static inline int n_open_mmaps = 0;
  static inline int n_open_fds   = 0;

  static inline int n_max_mmaps = 256;
  static inline int n_max_fds   = 256;

  static void recycle_int(gc_pool_type::iterator it) {
#ifndef NDEBUG
    static bool recursion_mode=false;
    assert(!recursion_mode); // do not call recycle inside the gc_function
    recursion_mode = true;
#endif

    it->second.gc_function(it->first);

#ifndef NDEBUG
    recursion_mode = false;
#endif

    if (it->second.fd >= 0) {
      close(it->second.fd);
      n_open_fds--;
    }

    n_open_mmaps--;

    munmap(it->first, it->second.size);

    //std::cerr << "mmap_gc_pool del name:" << it->second.name << " fd:" << it->second.fd << " base:" << it->first << std::endl;
    mmap_gc_pool.erase(it);
  }

  static void try_collect_step() {
    auto it = mmap_gc_pool.find(next_mmap);
    if (it == mmap_gc_pool.end())
      it = mmap_gc_pool.begin();
    if (it == mmap_gc_pool.end())
      return;
    it->second.gc_function(it->first);

    auto it2 = it;
    ++it2;
    next_mmap = it2->first;

    mmap_gc::recycle_int(it);
  }

  static void try_collect_fd() {
    int n = 3 * n_open_fds / 4;
    if (n>n_max_fds)
      n_max_fds = n;
    if (n_max_fds<24)
      n_max_fds = 24;
    while(n_open_fds >= n_max_fds)
      try_collect_step();
  }

  static void try_collect_mmap() {
    int n = 3 * n_open_mmaps / 4;
    if (n>n_max_mmaps)
      n_max_mmaps = n;
    if (n_max_mmaps<24)
      n_max_mmaps = 24;
    while(n_open_mmaps >= n_max_mmaps)
      try_collect_step();
  }

  static std::tuple<void *, size_t> mmap_step(std::string_view name, int fd, size_t size) {

    if (fd < 0) {
      void *base = ::mmap(0, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, fd, 0); // superpages
      return {base, size};
    }

    struct stat s;
    int status = ::fstat(fd, &s);
    if (status < 0) {
      std::cerr << "mmap_map::reload ERROR Could not check file status " << name << std::endl;
      exit(-3);
    }
    if (s.st_size <= size) {
      int ret = ::ftruncate(fd, size);
      if (ret<0) {
        std::cerr << "mmap_map::reload ERROR ftruncate could not resize  " << name << " to " << size << "\n";
        exit(-1);
      }
    } else {
      size = s.st_size;
    }

    void *base = ::mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0); // no superpages

    return std::make_tuple(base, size);
  }

public:
  static void delete_file(void *base) {
    auto it = mmap_gc_pool.find(base);
    assert(it!=mmap_gc_pool.end());
    assert(it->second.fd>=0);
    close(it->second.fd);
    unlink(it->second.name.c_str());
    it->second.fd = -1;
  }

  // mmap_map.hpp:    mmap_fd     = mmap_gc::open(mmap_name);
  //
  // mmap_map.hpp:    mmap_txt_fd = mmap_gc::open(mmap_name + "txt");
  // mmap_vector.hpp: mmap_fd     = mmap_gc::open(mmap_name);
  static int open(const std::string &name) {
#ifndef NDEBUG
    for(const auto &e:mmap_gc_pool) {
      if(e.second.fd <0)
        continue;
      assert(e.second.name != name); // No name duplicate (may be OK for multithreaded access)
    }
#endif

    int fd = ::open(name.c_str(), O_RDWR | O_CREAT, 0644);
    if (fd>=0)
      return fd;

    if (fd == EACCES)
      return -1; // No access, no need to recycle

    try_collect_fd();
    fd = ::open(name.c_str(), O_RDWR | O_CREAT, 0644);
    if (fd>=0)
      return fd;

    return -1;
  }

  // mmap_map.hpp:    mmap_gc::recycle(mmap_base);
  // mmap_vector.hpp: mmap_gc::recycle(mmap_base);
  static void recycle(void *base) {
    // Remove from gc
    auto it = mmap_gc_pool.find(base);
    assert(it!=mmap_gc_pool.end());

    recycle_int(it);
  }

  // mmap_vector.hpp: std::tie(base, mmap_size) = mmap_gc::mmap(mmap_name, mmap_fd, mmap_size, std::bind(&vector<T>::gc_function, this, std::placeholders::_1));
  // mmap_map.hpp:    std::tie(base, size)      = mmap_gc::mmap(mmap_name, fd, size, std::bind(&map<MaxLoadFactor100, Key, T, Hash>::gc_function, this, std::placeholders::_1));
  static std::tuple<void *, size_t> mmap(std::string_view name, int fd, size_t size, std::function<void(void *)> gc_function) {

    auto [base, final_size] = mmap_step(name, fd, size);
    if (base == MAP_FAILED) {
      try_collect_mmap();
      std::tie(base, final_size) = mmap_step(name, fd, size);
      if(base == MAP_FAILED) {
        std::cerr << "ERROR mmap_lib::mmap could not check allocate " << size/1024 << "KBs for " << name << std::endl;
        exit(-3);
      }
    }

    mmap_gc_entry entry;
    entry.name = name;
    entry.fd   = fd;
    entry.size = final_size;
    entry.gc_function = gc_function;

    assert(mmap_gc_pool.find(base) == mmap_gc_pool.end());
    //std::cerr << "mmap_gc_pool add name:" << name << " fd:" << fd << " base:" << base << std::endl;
    mmap_gc_pool[base] = entry;

    return {base, final_size};
  }

  // mmap_vector.hpp: mmap_base     = reinterpret_cast<uint8_t *>(mmap_gc::remap(mmap_name, mmap_base, old_mmap_size, mmap_size));
  // mmap_map.hpp:    mmap_txt_base = reinterpret_cast<uint64_t *>(mmap_gc::remap(mmap_name, mmap_txt_base, mmap_txt_size, size));
  static void *remap(std::string_view mmap_name, void *mmap_old_base, size_t old_size, size_t new_size) {
    auto it = mmap_gc_pool.find(mmap_old_base);
    assert(it!=mmap_gc_pool.end());

    assert(old_size == it->second.size);
    assert(old_size!=new_size);

    void *base = mremap(mmap_old_base, old_size, new_size, MREMAP_MAYMOVE);
    if (base == MAP_FAILED) {
      try_collect_mmap();
      base = mremap(mmap_old_base, old_size, new_size, MREMAP_MAYMOVE);
      if (base == MAP_FAILED) {
        std::cerr << "ERROR: remmap could not allocate" << mmap_name << "txt with " << new_size << "bytes\n";
        exit(-1);
      }
    }
    if (it->second.fd>=0) {
      int ret = ftruncate(it->second.fd, new_size);
      if(ret<0) {
        std::cerr << "ERROR: ftruncate could not allocate " << mmap_name << " to " << new_size << "\n";
        exit(-1);
      }
    }
    auto entry = it->second;
    entry.size = new_size;

    //std::cerr << "mmap_gc_pool del name:" << entry.name << " fd:" << entry.fd << " base:" << mmap_old_base << std::endl;
    mmap_gc_pool.erase(it); // old mmap_old_base

    //std::cerr << "mmap_gc_pool add name:" << entry.name << " fd:" << entry.fd << " base:" << base << std::endl;
    mmap_gc_pool[base] = entry;

    return base;
  }
};

} // namespace mmap_lib

