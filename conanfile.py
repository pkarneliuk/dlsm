from conan import ConanFile
from conan.tools.cmake import CMake

# See: https://docs.conan.io/2.0/reference/conanfile/attributes.html
class DLSM(ConanFile):
    name = "DLSM"
    version="0.0.0.0"
    generators = "CMakeDeps", "CMakeToolchain"
    settings = "os", "arch", "compiler", "build_type"

    tool_requires = "cmake/3.27.4"
    test_requires = [
        "gtest/1.14.0@#4372c5aed2b4018ed9f9da3e218d18b3",
        "benchmark/1.8.3@#2b95dcd66432d8ea28c5ac4db0be2fb2",
    ]
    requires = [
        "flatbuffers/23.5.26@#b153646f6546daab4c7326970b6cd89c",
        # "iceoryx/2.0.2@#9cba5c596b3fc7fb5e2174f466a12cad",
        "nlohmann_json/3.11.2@#a35423bb6e1eb8f931423557e282c7ed",
        "spdlog/1.12.0@#c5fc262786548cbac34e6c38e16309a9",
        "zeromq/4.3.4@#6aa4ca3273723ebed33e035ace8a265b",
        "zmqpp/4.2.0@#1fca59746eafbac0b1baf2b9935b11e9",
    ]

    default_options = {
        "spdlog/*:header_only": False,
    }

    def build(self):
        cmake = CMake(self)
        cmake.configure(variables={"CONAN_PACKAGE_VERSION": self.version})
        cmake.build()
        cmake.test()
        cmake.install()
        self.run("cpack %s" % (self.build_folder))
