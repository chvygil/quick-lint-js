// Copyright (C) 2020  Matthew "strager" Glazar
// See end of file for extended copyright information.

#pragma once

#if defined(__EMSCRIPTEN__)
// No filesystem on web.
#else

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <quick-lint-js/container/hash-map.h>
#include <quick-lint-js/container/result.h>
#include <quick-lint-js/io/file.h>
#include <quick-lint-js/port/span.h>
#include <quick-lint-js/util/synchronized.h>
#include <string>
#include <vector>

namespace quick_lint_js {
class Trace_Writer;

using Trace_Flusher_Thread_Index = std::uint64_t;

// These member functions are called with a lock held. Do not interact with
// trace_flusher in any implementations of these functions.
class Trace_Flusher_Backend {
 public:
  explicit Trace_Flusher_Backend() = default;

  Trace_Flusher_Backend(Trace_Flusher_Backend&&) = default;
  Trace_Flusher_Backend& operator=(Trace_Flusher_Backend&&) = default;

  virtual ~Trace_Flusher_Backend() = default;

  // For a single trace_flusher, each call to trace_thread_begin is made with a
  // unique value for thread_index.
  //
  // Called from any thread.
  virtual void trace_thread_begin(Trace_Flusher_Thread_Index thread_index) = 0;

  // For a single trace_flusher, trace_thread_end is called exactly 0 or 1
  // times. It is called 0 times if trace_thread_begin was never called, and it
  // is called 1 times if trace_thread_end was ever called.
  //
  // thread_index was previously given to trace_thread_begin.
  //
  // Called from any thread.
  virtual void trace_thread_end(Trace_Flusher_Thread_Index thread_index) = 0;

  // trace_thread_write_data can be called zero or more times.
  //
  // thread_index was previously given to trace_thread_begin (but not
  // trace_thread_end).
  //
  // Called from any thread.
  virtual void trace_thread_write_data(Trace_Flusher_Thread_Index thread_index,
                                       Span<const std::byte> data) = 0;
};

class Trace_Flusher_Directory_Backend final : public Trace_Flusher_Backend {
 public:
  const std::string& trace_directory() const { return this->trace_directory_; }

  void trace_thread_begin(Trace_Flusher_Thread_Index thread_index) override;
  void trace_thread_end(Trace_Flusher_Thread_Index thread_index) override;
  void trace_thread_write_data(Trace_Flusher_Thread_Index thread_index,
                               Span<const std::byte> data) override;

  // Creates a 'metadata' file in the given directory.
  //
  // If the directory does not exist, or if creating the 'metadata' file fails,
  // an error is returned.
  static Result<Trace_Flusher_Directory_Backend, Write_File_IO_Error>
  init_directory(const std::string& trace_directory);

  // Creates the given directory if it doesn't exist, then creates a
  // subdirectory with a timestamped name, then calls init_directory.
  //
  // If there was an error creating the directory, logs a message and returns
  // nullopt.
  //
  // Thread-safe.
  static std::optional<Trace_Flusher_Directory_Backend> create_child_directory(
      const std::string& directory);

 private:
  explicit Trace_Flusher_Directory_Backend(const std::string& trace_directory);

  std::string trace_directory_;
  Hash_Map<Trace_Flusher_Thread_Index, Platform_File> thread_files_;
};

// A Trace_Flusher gives trace_writer instances and writes traces to files.
//
// See docs/TRACING.md for details on the file format.
//
// Typical use:
//
// 1. Get a trace_flusher using Trace_Flusher::instance.
// 2. Enable tracing with enable_backend. (This can be done at any time.)
// 3. On threads which want to write data, call register_current_thread.
// 4. Periodically, call trace_writer_for_current_thread()->write_[event].
// 5. After events have been written, call
//    trace_writer_for_current_thread()->commit.
//
// Unless otherwise written, all public trace_flusher member functions are
// thread-safe. They can be called from any thread without synchronization.
class Trace_Flusher {
 private:
  // trace_flusher is a Singleton.
  /*implicit*/ Trace_Flusher();

 public:
  Trace_Flusher(const Trace_Flusher&) = delete;
  Trace_Flusher& operator=(const Trace_Flusher&) = delete;

  ~Trace_Flusher();

  static Trace_Flusher* instance();

  void enable_backend(Trace_Flusher_Backend*);

  // disable_backend is synchronizing. After disable_backend returns, it is
  // guaranteed that no methods on the backend are being called on other
  // threads.
  void disable_backend(Trace_Flusher_Backend*);

  // For testing only:
  void disable_all_backends();
  void unregister_all_threads();

  bool is_enabled();

  Trace_Flusher_Thread_Index register_current_thread();
  void unregister_current_thread();
  Trace_Writer* trace_writer_for_current_thread();

  void flush_sync();
  void flush_async();

  // start_flushing_thread can only be called once until a call to
  // stop_flushing_thread.
  //
  // start_flushing_thread is not thread-safe. Calls must be synchronized with
  // stop_flushing_thread.
  void start_flushing_thread();

  // stop_flushing_thread is idempotent; it can be called whether or not
  // start_flushing_thread has been called, and it can be called multiple times.
  //
  // stop_flushing_thread is not thread-safe. Calls must be synchronized with
  // start_flushing_thread and concurrent calls to stop_flushing_thread.
  void stop_flushing_thread();

 private:
  struct Registered_Thread;

  struct Shared_State {
    std::vector<Trace_Flusher_Backend*> backends;
    std::vector<std::unique_ptr<Registered_Thread>> registered_threads;
    Trace_Flusher_Thread_Index next_thread_index = 1;
    bool stop_flushing_thread = false;

    bool is_enabled();
  };

  void flush_sync(Lock_Ptr<Shared_State>&);

  void enable_backend(Lock_Ptr<Shared_State>&, Trace_Flusher_Backend*);
  void disable_backend(Lock_Ptr<Shared_State>&, Trace_Flusher_Backend*);

  void flush_one_thread_sync(Lock_Ptr<Shared_State>&, Registered_Thread&);

  void enable_thread_writer(Lock_Ptr<Shared_State>&, Registered_Thread&,
                            Trace_Flusher_Backend*);

  void write_thread_header_to_backend(Lock_Ptr<Shared_State>&,
                                      Registered_Thread&,
                                      Trace_Flusher_Backend*);

  // If tracing is enabled, this points to a registered_thread::stream_writer
  // from this->registered_threads_.
  //
  // If tracing is disabled, this points to nullptr.
  static thread_local std::atomic<Trace_Writer*> thread_stream_writer_;

  Synchronized<Shared_State> state_;
  Condition_Variable flush_requested_cond_;

  Thread flushing_thread_;
};
}

#endif

// quick-lint-js finds bugs in JavaScript programs.
// Copyright (C) 2020  Matthew "strager" Glazar
//
// This file is part of quick-lint-js.
//
// quick-lint-js is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// quick-lint-js is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with quick-lint-js.  If not, see <https://www.gnu.org/licenses/>.
