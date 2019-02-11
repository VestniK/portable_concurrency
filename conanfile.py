from conans import ConanFile, CMake, tools


class PortableconCurrencyConan(ConanFile):
    build_requires = "gtest/1.8.0@VestniK/stable"
    name = "portable_concurrency"
    version = "0.8.1"
    license = "CC0"
    url = "https://github.com/VestniK/portable_concurrency"
    description = "Portable future/promise implemenattion close to ConcurrencyTS specification"
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False]}
    default_options = "shared=False"
    generators = "cmake"
    exports_sources = "*", "!build/*"

    def build(self):
        cmake = CMake(self)
        cmake.configure(source_folder="./")
        cmake.build()
        if not tools.cross_building(self.settings):
            cmake.test()
        cmake.install()

    def package(self):
        pass

    def package_info(self):
        self.cpp_info.libs = ["portable_concurrency"]
