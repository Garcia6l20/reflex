#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

// A private file mapping is the cheapest way to hand a deserializer one
// contiguous span, which is what keeps its bulk-scan path on. Where mmap is
// unavailable the file is read into an owned std::string instead: same class,
// same contract, one extra copy.
//
// Definable from the command line, so the fallback can be tested where mmap does
// exist rather than hoped about.
#ifndef REFLEX_SERDE_HAVE_MMAP
#if defined(__unix__) or defined(__APPLE__)
#define REFLEX_SERDE_HAVE_MMAP 1
#else
#define REFLEX_SERDE_HAVE_MMAP 0
#endif
#endif

#ifndef REFLEX_MODULE
#include <cerrno>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#if REFLEX_SERDE_HAVE_MMAP
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#endif

REFLEX_EXPORT namespace reflex::serde
{
  // A whole file, presented as a contiguous range of char.
  //
  // Despite the name it is not an std::istream and deliberately not consumed
  // through one: an istream deduces std::istreambuf_iterator, which is not a
  // contiguous iterator, so every deserializer falls back to its
  // character-at-a-time path and loses the bulk scans silently. This exposes
  // begin()/end() as const char*, so `deserializer{stream}` deduces a pointer
  // cursor and `deserializer<...>::bulk_scan` is true.
  //
  //   serde::mmap_input_stream in{"big.xml"};
  //   auto doc = xml::deserializer{in}.load<Config>();
  //
  // Lifetime contract: the bytes are valid exactly as long as this object, and
  // the object must outlive both the deserializer built from it and anything
  // still borrowing from the parse. Views handed out by a deserializer on the
  // contiguous path (a tag name from xml::read_open_tag, an attribute value,
  // a borrowed text run) point into this buffer. Holding the stream is what
  // makes keeping such a view legal.
  //
  // Move-only: it owns a mapping.
  //
  // Caveat, the one every mmap reader has: a file rewritten underneath a
  // MAP_PRIVATE mapping changes while the parse is running, and truncating it
  // raises SIGBUS on the removed pages. Copy the file first if that matters.
  class mmap_input_stream
  {
    // An empty file has no mapping, but data() must still be dereferenceable:
    // the fast paths take std::to_address of the cursor and a null there is one
    // pointer arithmetic mistake away from undefined behaviour.
    static constexpr char empty_[1] = {};

    std::string copy_{};
    const char* data_ = empty_;
    std::size_t size_ = 0;
#if REFLEX_SERDE_HAVE_MMAP
    void* map_ = nullptr;
#endif

    [[noreturn]] static void fail(std::filesystem::path const& path, int err, const char* what)
    {
      throw std::filesystem::filesystem_error(what, path, std::error_code(err, std::generic_category()));
    }

  public:
    explicit mmap_input_stream(std::filesystem::path const& path)
    {
#if REFLEX_SERDE_HAVE_MMAP
      const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
      if(fd < 0)
      {
        fail(path, errno, "serde::mmap_input_stream: cannot open file");
      }
      struct ::stat st{};
      if(::fstat(fd, &st) != 0)
      {
        const int err = errno;
        ::close(fd);
        fail(path, err, "serde::mmap_input_stream: cannot stat file");
      }
      if(not S_ISREG(st.st_mode))
      {
        ::close(fd);
        fail(path, EISDIR, "serde::mmap_input_stream: not a regular file");
      }
      const auto size = static_cast<std::size_t>(st.st_size);
      if(size == 0) // mmap rejects a zero length
      {
        ::close(fd);
        return;
      }
      void* map = ::mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
      ::close(fd); // the mapping keeps its own reference
      if(map == MAP_FAILED)
      {
        fail(path, errno, "serde::mmap_input_stream: cannot map file");
      }
      map_  = map;
      data_ = static_cast<const char*>(map);
      size_ = size;
#else
      // status() first, so a missing file and a directory report the same way
      // they do on the mapping path instead of whatever errno an ifstream left
      std::error_code  ec;
      const auto       st = std::filesystem::status(path, ec);
      if(ec)
      {
        throw std::filesystem::filesystem_error(
            "serde::mmap_input_stream: cannot open file", path, ec);
      }
      if(not std::filesystem::is_regular_file(st))
      {
        fail(path, EISDIR, "serde::mmap_input_stream: not a regular file");
      }
      std::ifstream in{path, std::ios::binary | std::ios::ate};
      if(not in)
      {
        fail(path, EACCES, "serde::mmap_input_stream: cannot open file");
      }
      // tellg reports -1 on failure, which would turn into an enormous resize.
      const auto size = static_cast<std::streamoff>(in.tellg());
      if(size < 0)
      {
        fail(path, EIO, "serde::mmap_input_stream: cannot size file");
      }
      in.seekg(0);
      copy_.resize(static_cast<std::size_t>(size));
      if(size > 0 and not in.read(copy_.data(), size))
      {
        fail(path, errno, "serde::mmap_input_stream: cannot read file");
      }
      size_ = copy_.size();
      data_ = size_ == 0 ? empty_ : copy_.data();
#endif
    }

    mmap_input_stream(mmap_input_stream&& other) noexcept
        : copy_{std::move(other.copy_)}, data_{other.data_}, size_{other.size_}
    {
#if REFLEX_SERDE_HAVE_MMAP
      map_       = other.map_;
      other.map_ = nullptr;
      if(map_ == nullptr)
#endif
      {
        // the bytes live in copy_, whose buffer may have moved with it
        data_ = size_ == 0 ? empty_ : copy_.data();
      }
      other.data_ = empty_;
      other.size_ = 0;
    }

    mmap_input_stream& operator=(mmap_input_stream&& other) noexcept
    {
      if(this != &other)
      {
        release();
        copy_ = std::move(other.copy_);
        data_ = other.data_;
        size_ = other.size_;
#if REFLEX_SERDE_HAVE_MMAP
        map_       = other.map_;
        other.map_ = nullptr;
        if(map_ == nullptr)
#endif
        {
          data_ = size_ == 0 ? empty_ : copy_.data();
        }
        other.data_ = empty_;
        other.size_ = 0;
      }
      return *this;
    }

    mmap_input_stream(mmap_input_stream const&)            = delete;
    mmap_input_stream& operator=(mmap_input_stream const&) = delete;

    ~mmap_input_stream()
    {
      release();
    }

    const char* data() const noexcept
    {
      return data_;
    }

    std::size_t size() const noexcept
    {
      return size_;
    }

    bool empty() const noexcept
    {
      return size_ == 0;
    }

    const char* begin() const noexcept
    {
      return data_;
    }

    const char* end() const noexcept
    {
      return data_ + size_;
    }

    std::string_view view() const noexcept
    {
      return {data_, size_};
    }

  private:
    void release() noexcept
    {
#if REFLEX_SERDE_HAVE_MMAP
      if(map_ != nullptr)
      {
        ::munmap(map_, size_);
        map_ = nullptr;
      }
#endif
    }
  };
}
