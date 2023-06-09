from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout
from conan.tools.build import cross_building


class PortableconCurrencyConan(ConanFile):
    name = "portable_concurrency"
    version = "0.11.0"
    license = "CC0"
    url = "https://github.com/VestniK/portable_concurrency"
    description = "Portable future/promise implemenattion close to ConcurrencyTS specification"
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "fPIC": [True, False],
        "shared": [True, False],
        "no_deprecated": [True, False],
    }
    default_options = {
        "fPIC": True,
        "shared": False,
        "no_deprecated": False,
    }
    exports_sources = "*", "!build/*"

    def layout(self):
        cmake_layout(self)

    def config_options(self):
        if self.settings.os == "Windows":
            self.options.rm_safe("fPIC")

    def requirements(self):
        self.requires("gtest/1.13.0", private=True)
        self.requires("asio/1.28.0", private=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.generate()

    def build(self):
        cmake = CMake(self)
        if self.develop:
            cmake.definitions['PC_DEV_BUILD'] = True
        if self.options.no_deprecated:
            cmake.definitions['PC_NO_DEPRECATED'] = True
        cmake.configure()
        cmake.build()
        if not cross_building(self):
            cmake.test()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = ["portable_concurrency"]
        if self.options.no_deprecated:
            self.cpp_info.defines = ["PC_NO_DEPRECATED"]
