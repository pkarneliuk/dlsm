#include "impl/SharedMemory.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <cstdlib>
#include <stdexcept>
#include <thread>

#include "impl/Str.hpp"

namespace {
class ShmOpen final : public dlsm::SharedMemory {
    std::string name_;  // name in /dev/shm/
    std::size_t size_ = 0;
    void* address_ = nullptr;
    bool created_ = false;
    bool keep_ = false;

public:
    ShmOpen(const std::string_view& options) {
        const auto opts = dlsm::Str::ParseOpts(options);
        name_ = opts.required("name");
        bool purge = opts.get("purge", "off") != "off";
        bool create = opts.get("create", "off") != "off";  // on/off or delay in ms: 200
        bool open = opts.get("open", "off") != "off";      // on/off or number of retries: 1x200
        keep_ = opts.get("keep", "off") != "off";
        bool readonly = opts.get("readonly", "off") != "off";
        bool huge2mb = opts.get("huge2mb", "off") != "off";
        bool huge1gb = opts.get("huge1gb", "off") != "off";
        bool lock = opts.get("lock", "off") != "off";
        long size = 0;
        if (create) {
            const auto& size_str = opts.required("size");
            size = std::atol(size_str.c_str());
            if (size <= 0) {
                throw std::invalid_argument("'size' has invalid value:" + size_str);
            }
        }

        if (!create && !open) {
            throw std::invalid_argument("'create' and/or 'open' options must be on");
        }

        if (create && readonly) {
            throw std::invalid_argument("'create' and 'readonly' options are not allowed");
        }

        if (purge) {
            if (0 != ::shm_unlink(name_.c_str()) && errno != ENOENT) {
                throw std::system_error(errno, std::generic_category(), "shm_unlink() purge failed");
            }
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
        } closer;

        const int oflag = (readonly ? O_RDONLY : O_RDWR);
        const mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;

        if (create) {  // Attempt to create exclusively - has ownership
            closer.fd = ::shm_open(name_.c_str(), oflag | O_CREAT | O_EXCL, mode);
            if (closer.fd != -1) {
                created_ = true;
                closer.name = name_;
            } else if (!(open && errno == EEXIST)) {
                throw std::system_error(errno, std::generic_category(), "shm_open(CREATE) with: " + name_);
            }
        }

        if (!created_ && open) {  // Attempt to open existing if allowed
            closer.fd = ::shm_open(name_.c_str(), oflag, mode);
            if (closer.fd == -1) {
                auto retries = dlsm::Str::xpair(opts.get("open"));
                for (std::size_t i = 0; i < retries.first; ++i) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(retries.second));
                    closer.fd = ::shm_open(name_.c_str(), oflag, mode);
                    if (closer.fd != -1) break;
                }
                if (closer.fd == -1)
                    throw std::system_error(errno, std::generic_category(), "shm_open(OPEN) with: " + name_);
            }
        }

        if (created_) {
            if (auto delay = std::atol(opts.get("create").c_str()); delay > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(delay));
            }

            if (0 != ::ftruncate(closer.fd, size)) {
                throw std::system_error(errno, std::generic_category(), "ftruncate()");
            }
        }

        const auto st_size = [&]() {
            struct stat statbuf = {};
            if (0 != ::fstat(closer.fd, &statbuf)) {
                throw std::system_error(errno, std::generic_category(), "fstat()");
            }
            return static_cast<std::size_t>(statbuf.st_size);
        };

        size_ = st_size();
        if (open && size == 0) {  // /dev/shm/name has been created but not yet ftruncate() to expected size
            auto retries = dlsm::Str::xpair(opts.get("open"));
            for (std::size_t i = 0; i < retries.first; ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(retries.second));
                size_ = st_size();
                if (size_ > 0) break;
            }
        }

        const int prot = (readonly ? PROT_READ : (PROT_READ | PROT_WRITE));
        int flags = MAP_SHARED | MAP_POPULATE;

        if (huge2mb) {
            flags |= MAP_HUGETLB | (21 << MAP_HUGE_SHIFT) | MAP_ANONYMOUS;
        } else if (huge1gb) {
            flags |= MAP_HUGETLB | (30 << MAP_HUGE_SHIFT) | MAP_ANONYMOUS;
        }

        address_ = ::mmap(nullptr, size_, prot, flags, closer.fd, 0);
        if (address_ == MAP_FAILED) {
            throw std::system_error(errno, std::generic_category(), "mmap()");
        }

        closer.adderss = address_;
        closer.size = size_;
        ::close(closer.fd);  // legal place to close fd after mmap
        closer.fd = -1;

        if (lock) {
            if (0 != ::mlock(address_, size_)) {
                throw std::system_error(errno, std::generic_category(), "mlock()");
            }
        }

        closer = Closer{};  // reset and drop
    }

    ~ShmOpen() noexcept final {
        if (0 != ::munmap(address_, size_)) {
            // ignore errno
            // throw std::system_error(errno, std::generic_category(), "munmap()");
        }

        if (!keep_ && created_ && 0 != ::shm_unlink(name_.c_str())) {
            // throw std::system_error(errno, std::generic_category(), "shm_unlink()");
        }
    }

    bool owner() const noexcept override { return created_; }
    const std::string& name() const noexcept override { return name_; }
    void* address() const noexcept override { return address_; }
    std::size_t size() const noexcept override { return size_; }
};
}  // namespace

namespace dlsm {

SharedMemory::UPtr SharedMemory::create(const std::string& options) try {
    return std::make_unique<ShmOpen>(options);
} catch (const std::exception& e) {
    throw std::invalid_argument("SharedMemory with opts: '" + options + "' error: " + e.what());
}
}  // namespace dlsm