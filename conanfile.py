from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps, cmake_layout
from conan.tools.files import copy
import os

class XyoSdkCppConan(ConanFile):
    name = "xyo-sdk-cpp"
    version = "1.0.0"
    license = "Apache-2.0"
    author = "Syniol Limited"
    url = "https://github.com/syniol/xyo-sdk-cpp"
    description = "XYO SDK to connect and consume AI Banking Transaction Enrichment API"
    topics = ("xyo", "banking", "enrichment", "sdk")
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
    }

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def requirements(self):
        self.requires("libcurl/[>=7.78.0 <9.0.0]")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.generate()
        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))

    def package_info(self):
        self.cpp_info.libs = ["xyo_sdk"]
        self.cpp_info.set_property("cmake_file_name", "XYOSDK")
        self.cpp_info.set_property("cmake_target_name", "XYO::SDK")
