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
        "gtest/1.14.0@#0bfd0373138714a7567dcdabd133a722",
        "benchmark/1.8.3@#2b95dcd66432d8ea28c5ac4db0be2fb2",
    ]
    requires = [
        "flatbuffers/24.3.25@#8fc25e15ac8ef302e2c42497d10c95e9",
        "gperftools/2.16@#0248a0632d82ca3440ea4c57e46794d8",
        "nlohmann_json/3.11.3@#45828be26eb619a2e04ca517bb7b828d",
        "spdlog/1.13.0@#8e88198fd5b9ee31d329431a6d0ccaa2",
        "zeromq/4.3.5@#dd23b6f3e4e0131e696c3a0cd8092277",
    ]

    default_options = {
        "gperftools/*:enable-dynamic-sized-delete-support": True,
        "gperftools/*:enable_libunwind": False,
        "gperftools/*:sized_delete": True,
        "spdlog/*:header_only": False,
        "zeromq/*:poller": None,  # [None, "kqueue", "epoll", "devpoll", "pollset", "poll", "select"]
        "zeromq/*:with_draft_api": True,
        "zeromq/*:with_websocket": False,
    }

    def build(self):
        cmake = CMake(self)
        cmake.configure(variables={"CONAN_PACKAGE_VERSION": self.version})
        cmake.build()
        cmake.test()
        cmake.install()
        self.run("cpack %s" % (self.build_folder))
