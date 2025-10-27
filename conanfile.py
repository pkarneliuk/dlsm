from conan import ConanFile
from conan.tools.cmake import CMake

# See: https://docs.conan.io/2.0/reference/conanfile/attributes.html
class DLSM(ConanFile):
    name = "DLSM"
    version="0.0.0.0"
    generators = "CMakeDeps", "CMakeToolchain"
    settings = "os", "arch", "compiler", "build_type"

    tool_requires = "cmake/[>=3.29]"
    test_requires = [
        "gtest/1.16.0@#4fd8d9d80636ea9504de6a0ec1ab8686",
        "benchmark/1.9.1@#73d267daf0371b7f6605edef1594a681",
    ]
    requires = [
        "boost/1.88.0@#83254a382436347122665d2aea8605f9",
        "flatbuffers/24.12.23@#80629ff8f39788daff0a904c0133ca7b",
        "gperftools/2.16@#0248a0632d82ca3440ea4c57e46794d8",
        "nlohmann_json/3.11.3@#45828be26eb619a2e04ca517bb7b828d",
        "openssl/3.6.0@#89e8af1d4a21afcac0557079d23d8890",
        "spdlog/1.14.1@#972bbf70be1da4bc57ea589af0efde03",
        "zeromq/4.3.5@#dd23b6f3e4e0131e696c3a0cd8092277",
    ]

    default_options = {
        "boost/*:asio_no_deprecated": True,
        "gperftools/*:enable-dynamic-sized-delete-support": True,
        "gperftools/*:enable_libunwind": False,
        "gperftools/*:sized_delete": True,
        "spdlog/*:header_only": False,
        "spdlog/*:use_std_fmt": True,
        "zeromq/*:poller": None,  # [None, "kqueue", "epoll", "devpoll", "pollset", "poll", "select"]
        "zeromq/*:with_draft_api": True,
        "zeromq/*:with_websocket": False,
    }

    def configure(self):
        without_boost = [
            "atomic", "charconv", "chrono", "cobalt", "context", "contract", "coroutine", "date_time", "exception", "fiber",
            "filesystem", "graph", "iostreams", "json", "locale", "log", "math", "nowide", "process", "container", "program_options",
            "random", "regex", "serialization", "stacktrace", "test", "thread", "timer", "type_erasure", "url", "wave"
        ]
        for lib in without_boost:
            self.options["boost"][f"without_{lib}"] = True

    def build(self):
        cmake = CMake(self)
        cmake.configure(variables={"CONAN_PACKAGE_VERSION": self.version})
        cmake.build()
        cmake.test()
        cmake.install()
        self.run("cpack %s" % (self.build_folder))
