from conans import tools, CMake, ConanFile


class TankerConan(ConanFile):
    name = "tanker"
    version = "dev"
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "with_ssl": [True, False],
        "sanitizer": ["address", "leak", "memory", "thread", "undefined", None],
        "coverage": [True, False],
        }
    default_options = "shared=False", "fPIC=True", "with_ssl=True", "sanitizer=None", "coverage=False"
    exports_sources = "CMakeLists.txt", "modules/*"
    generators = "cmake", "json", "ycm"

    @property
    def cross_building(self):
        return tools.cross_building(self.settings)

    @property
    def should_build_tests(self):
        # develop is false when the package is used as a requirement.
        return self.develop and not self.cross_building

    @property
    def should_run_tests(self):
        return self.options.with_ssl and self.should_build_tests and self.should_test and not self.cross_building

    @property
    def sanitizer_flag(self):
        if self.options.sanitizer:
            return " -fsanitize=%s " % self.options.sanitizer
        return None

    def requirements(self):
        # Why only private for Android?
        # Because it's the only platform for which we directly build a shared library which only depends on static libs.
        # Thus we embed every dep in the shared lib
        private = (self.settings.os == "Android")

        self.requires("Boost/1.66.0@tanker/testing", private=private, override=True)
        if self.options.with_ssl:
            self.requires("LibreSSL/2.6.3@tanker/testing", private=private)
        self.requires("cppcodec/edf46ab@tanker/testing", private=private)
        self.requires("enum-flags/0.1a@tanker/testing", private=private)
        self.requires("fmt/5.2.1@tanker/testing", private=private)
        self.requires("gsl-lite/0.32.0@tanker/testing", private=private)
        self.requires("jsonformoderncpp/3.4.0@tanker/testing", private=private)
        self.requires("libsodium/1.0.16@tanker/testing", private=private)
        self.requires("mockaron/0.2@tanker/testing", private=private)
        self.requires("optional-lite/3.1.1@tanker/testing", private=private)
        self.requires("socket.io-client-cpp/1.6.1@tanker/testing", private=private)
        self.requires("sqlpp11/0.57@tanker/testing", private=private)
        self.requires("sqlpp11-connector-sqlite3/0.29@tanker/testing", private=private)
        self.requires("tconcurrent/0.16@tanker/testing", private=private)
        self.requires("variant/1.3.0@tanker/testing", private=private)

    def imports(self):
        if self.settings.os == "iOS":
            # on iOS, we need static libs to create universal binaries
            # Note: libtanker*.a will be copied at install time
            self.copy("*.a", dst="lib", src="lib")
        self.copy("license*", dst="licenses", folder=True, ignore_case=True)
        self.copy("copying", dst="licenses", folder=True, ignore_case=True)

    def configure(self):
        if tools.cross_building(self.settings):
            del self.settings.compiler.libcxx
        if self.settings.os in ["Android", "iOS"]:
            # On Android OpenSSL can't use system ca-certificates, so we ship mozilla's cacert.pem instead
            self.options["socket.io-client-cpp"].embed_cacerts = True

    def build_requirements(self):
        if self.should_build_tests:
            self.build_requires("docopt.cpp/0.6.2@tanker/testing")
            self.build_requires("doctest/2.0.1@tanker/testing")
            self.build_requires("trompeloeil/v29@tanker/testing")

    def build(self):
        cmake = CMake(self)
        if self.options.sanitizer:
            cmake.definitions["CONAN_C_FLAGS"] += self.sanitizer_flag
            cmake.definitions["CONAN_CXX_FLAGS"] += self.sanitizer_flag
        cmake.definitions["BUILD_TESTS"] = self.should_build_tests
        cmake.definitions["BUILD_TANKER_TOOLS"] = self.should_build_tests
        cmake.definitions["TANKER_BUILD_WITH_SSL"] = self.options.with_ssl
        cmake.definitions["BUILD_SHARED_LIBS"] = self.options.shared
        cmake.definitions["CMAKE_POSITION_INDEPENDENT_CODE"] = self.options.fPIC
        cmake.definitions["WITH_COVERAGE"] = self.options.coverage
        if self.should_configure:
            cmake.configure()
        if self.should_build:
            cmake.build()
        if self.should_run_tests:
            cmake.test()
        if self.should_install:
            cmake.install()

    def package_info(self):
        libs = ["tanker"]
        if not self.options.shared:
            libs.extend(["tankercore", "tankerusertoken", "tankercrypto"])

        if self.sanitizer_flag:
            self.cpp_info.sharedlinkflags = [self.sanitizer_flag]
            self.cpp_info.exelinkflags = [self.sanitizer_flag]

        self.cpp_info.libs = libs
