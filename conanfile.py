from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout
from conan.tools.build import cross_building


class PortableconCurrencyConan(ConanFile):
    build_requires = "gtest/1.13.0"
    name = "portable_concurrency"
    version = "0.11.0"
    license = "CC0"
    url = "https://github.com/VestniK/portable_concurrency"
    description = "Portable future/promise implemenattion close to ConcurrencyTS specification"
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False],
               "no_deprecated": [True, False]}
    default_options = {"shared": False,
                       "no_deprecated": False}
    exports_sources = "*", "!build/*"

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.generate()

    def build(self):
        cmake = CMake(self)
        if self.develop:
            cmake.definitions['PC_DEV_BUILD'] = True
        if self.options.no_deprecated:
            cmake.definitions['PC_NO_DEPRECATED'] = True
        cmake.configure(source_folder="./")
        cmake.build()
        if not cross_building(self.settings):
            cmake.test()
        cmake.install()

    def package(self):
        pass

    def package_info(self):
        self.cpp_info.libs = ["portable_concurrency"]
        if self.options.no_deprecated:
            self.cpp_info.defines = ["PC_NO_DEPRECATED"]
