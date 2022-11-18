from conans import ConanFile, CMake

class DLSM(ConanFile):
    name = "DLSM"
    version="0.0.0.0"
    generators = "cmake"
    settings = "os", "arch", "compiler", "build_type"
    requires = [
        "benchmark/1.7.0@#35565541008e14b7f9441f2b26bcc4de",
        "gtest/1.12.1@#43277155c631b245cd49ef6a07281989",
        "flatbuffers/22.10.26@#5962e0699bf63423f2cf234d67362856",
        "nlohmann_json/3.11.2@#a35423bb6e1eb8f931423557e282c7ed",
        "zeromq/4.3.4@#30554b087a2f3c8117b27787a8a4e0e9",
        "zmqpp/4.2.0@#1fca59746eafbac0b1baf2b9935b11e9",
    ]

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
        cmake.test(output_on_failure=True)
        cmake.install()