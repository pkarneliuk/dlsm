#include "impl/SharedMemory.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <cstdlib>
#include <stdexcept>

#include "impl/Str.hpp"

namespace {
class ShmOpen final : public dlsm::SharedMemory {
    std::string name_;  // name in /dev/shm/
    std::size_t size_ = 0;
    void* address_ = nullptr;
    bool owner_ = false;

public:
    struct Params {
        std::string name;
        bool create = false;
        bool readonly = false;
        bool huge2mb = false;
        bool huge1gb = false;
        bool lock = false;
        long size = 0;

        Params(const std::string& options) {
            const auto opts = dlsm::Str::ParseOpts(options);
            name = opts.get("name");
            create = opts.get("create", "off") != "off";
            readonly = opts.get("readonly", "off") != "off";
            huge2mb = opts.get("huge2mb", "off") != "off";
            huge1gb = opts.get("huge1gb", "off") != "off";
            lock = opts.get("lock", "off") != "off";
            if (create) {
                const auto size_str = opts.get("size");
                size = std::atol(size_str.c_str());
                if (size <= 0) {
                    throw std::invalid_argument("'size' has invalid value:" + size_str);
                }
            }
        }
    };

    ShmOpen(Params p) : name_{p.name} {
        if (p.create && p.size <= 0) {
            throw std::invalid_argument("invalid size value:" + std::to_string(p.size));
        }

        if (p.create && p.readonly) {
            throw std::invalid_argument("create and readonly options are not allowed");
        }

        int oflag = (p.readonly ? O_RDONLY : O_RDWR);
        if (p.create) oflag |= O_CREAT | O_EXCL;

        mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;

        const int fd = ::shm_open(name_.c_str(), oflag, mode);
        if (fd == -1) {
            throw std::system_error(errno, std::generic_category(), "shm_open() with: " + name_);
        }

        struct Closer {
            int fd = -1;
            std::string name;
            void* adderss = nullptr;
            std::size_t size = 0;
            ~Closer() {
                if (adderss) ::munmap(adderss, size);
                if (fd != -1) ::close(fd);
                if (!name.empty()) ::shm_unlink(name.c_str());
            }
        } closer = {fd, p.create ? name_ : ""};

        if (p.create) {
            owner_ = true;
            if (0 != ::ftruncate(fd, p.size)) {
                throw std::system_error(errno, std::generic_category(), "ftruncate()");
            }
        }

        struct stat statbuf = {};
        if (0 != fstat(fd, &statbuf)) {
            throw std::system_error(errno, std::generic_category(), "fstat()");
        }
        size_ = static_cast<std::size_t>(statbuf.st_size);

        const int prot = (p.readonly ? PROT_READ : (PROT_READ | PROT_WRITE));
        int flags = MAP_SHARED | MAP_POPULATE;

        if (p.huge2mb) {
            flags |= MAP_HUGETLB | (21 << MAP_HUGE_SHIFT) | MAP_ANONYMOUS;
        } else if (p.huge1gb) {
            flags |= MAP_HUGETLB | (30 << MAP_HUGE_SHIFT) | MAP_ANONYMOUS;
        }

        address_ = ::mmap(NULL, size_, prot, flags, fd, 0);
        if (address_ == MAP_FAILED) {
            throw std::system_error(errno, std::generic_category(), "mmap()");
        }

        closer.adderss = address_;
        closer.size = size_;
        ::close(fd);  // legal place to close fd after mmap
        closer.fd = -1;

        if (p.lock) {
            if (0 != ::mlock(address_, size_)) {
                throw std::system_error(errno, std::generic_category(), "mlock()");
            }
        }

        closer = Closer{};  // reset
    }

    ~ShmOpen() noexcept {
        if (0 != ::munmap(address_, size_)) {
            // ignore errno
            // throw std::system_error(errno, std::generic_category(), "munmap()");
        }
        if (owner_ && 0 != ::shm_unlink(name_.c_str())) {
            // ignore errno
            // throw std::system_error(errno, std::generic_category(), "shm_unlink()");
        }
    }

    const std::string& name() const noexcept override { return name_; }
    void* address() const noexcept override { return address_; }
    std::size_t size() const noexcept override { return size_; }
};
}  // namespace

namespace dlsm {

SharedMemory::UPtr SharedMemory::create(const std::string& options) try {
    return std::make_unique<ShmOpen>(ShmOpen::Params{options});
} catch (const std::exception& e) {
    throw std::invalid_argument("SharedMemory with opts: '" + options + "' error: " + e.what());
}
}  // namespace dlsm