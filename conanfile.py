from conans import ConanFile, CMake


class LibarcConan(ConanFile):
    name = "libarc"
    version = "1.0.0"
    settings = "os", "compiler", "build_type", "arch"
    requires = ["mariadb-connector-c/3.1.12", "fmt/9.1.0"]
    generators = "cmake", "cmake_find_package"
    options = {"build_test": [True, False]}
    default_options = {"build_test": True}
    exports_sources = "*"

    def _configure_cmake(self) -> CMake:
        cmake = CMake(self)
        if self.options.build_test:
            cmake.definitions["ARC_BUILD_TESTS"] = "ON"
        cmake.configure()
        return cmake

    def configure(self):
        if self.options.build_test:
            self.requires.add("gtest/cci.20210126")
        

    def build(self):
        cmake = self._configure_cmake()
        cmake.build()

    def package(self):
        cmake = self._configure_cmake()
        cmake.build()
        cmake.install()
