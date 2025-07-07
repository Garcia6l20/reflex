#pragma once

#include <unistd.h>
#include <fcntl.h>
#include <cstdio>
#include <functional>
#include <array>
#include <stdexcept>

class pipe_capture
{
public:
  pipe_capture(FILE *stream) : stream_(stream), original_fd_(fileno(stream_))
  {
    // Flush the stream before piping
    ::fflush(stream_);

    // Create a pipe
    if (::pipe(pipe_fd_) == -1)
      throw std::runtime_error("pipe() failed");

    // Save original stream
    saved_fd_ = ::dup(original_fd_);
    if (saved_fd_ == -1)
      throw std::runtime_error("dup() failed");

    // Redirect stream to pipe write end
    if (::dup2(pipe_fd_[1], original_fd_) == -1)
      throw std::runtime_error("dup2() failed");

    // Close the duplicated write end in this class
    ::close(pipe_fd_[1]);
  }

  ~pipe_capture()
  {
    // Best-effort restore (only if not already restored)
    if (saved_fd_ != -1)
    {
      ::dup2(saved_fd_, original_fd_);
      ::close(saved_fd_);
    }
    ::close(pipe_fd_[0]);
  }

  std::string read_all()
  {
    // Flush the stream before restoring
    ::fflush(stream_);

    // Restore the original stream
    if (::dup2(saved_fd_, original_fd_) == -1)
      throw std::runtime_error("dup2 restore failed");
    ::close(saved_fd_);

    // Read all content from the read end
    std::string result;
    std::array<char, 128> buffer;
    ssize_t count;
    while ((count = ::read(pipe_fd_[0], buffer.data(), buffer.size())) > 0)
    {
      result.append(buffer.data(), count);
    }
    ::close(pipe_fd_[0]);
    return result;
  }

private:
  FILE *stream_;
  int original_fd_;
  int saved_fd_ = -1;
  int pipe_fd_[2]{-1, -1};
};

std::pair<std::string, std::string> capture_out_err(const std::function<void()>& fn) {
    pipe_capture capture_out(stdout);
    pipe_capture capture_err(stderr);

    fn();

    return {capture_out.read_all(), capture_err.read_all()};
}
