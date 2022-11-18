from conans import ConanFile, CMake

class DLSM(ConanFile):
    name = "DLSM"
    version="0.0.0.0"
    generators = "cmake"
    settings = "os", "arch", "compiler", "build_type"
    requires = [
        "benchmark/1.7.1@#cd0be7a480a6ace256e2cfe7c9f1c9b8",
        "flatbuffers/23.1.21@#c7b7265bb316e1718a6a7be077d2d126",
        "gtest/1.13.0@#b7b21d0d5503615d4ba0223f720b4c0b",
        "iceoryx/2.0.2@#9cba5c596b3fc7fb5e2174f466a12cad",
        "nlohmann_json/3.11.2@#a35423bb6e1eb8f931423557e282c7ed",
        "spdlog/1.11.0@#faa6eb03bd1009bf2070b0c77e4f56a6",
        "zmqpp/4.2.0@#1fca59746eafbac0b1baf2b9935b11e9",
    ]

    default_options = {
        "spdlog:header_only": False,
    }

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
        cmake.test(output_on_failure=True)
        cmake.install()
        self.run("cpack %s" % (self.build_folder))
