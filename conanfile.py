from conans import tools, CMake, ConanFile


class TankerConan(ConanFile):
    name = "tanker"
    version = "dev"
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "tankerlib_shared": [True, False],
        "fPIC": [True, False],
        "with_ssl": [True, False],
        "with_tracer": [True, False],
        "warn_as_error": [True, False],
        "sanitizer": ["address", "leak", "memory", "thread", "undefined", None],
        "coverage": [True, False],
        "coroutinests": [True, False],
        }
    default_options = {"tankerlib_shared": False, "fPIC": True, "with_ssl": True, "with_tracer": False, "warn_as_error": False, "sanitizer": None, "coverage": False, "coroutinests": False}
    exports_sources = "CMakeLists.txt", "modules/*", "cmake/*"
    generators = "cmake", "json", "ycm"
    build_policy = "missing"
    cmake = None

    @property
    def cross_building(self):
        return tools.cross_building(self.settings)

    @property
    def should_build_tests(self):
        # develop is false when the package is used as a requirement,
        # so don't bother compiling tests in that case
        if not self.develop:
            return False

        # Code generated with emscripten must be tested with
        # Javascript code, not C++
        if self.settings.os == "Emscripten":
            return False

        # Usually, tests cannot be run when cross-compiling,
        # _except_ when cross-compiling from win64 to win32 with mingw
        # on Windows, so check for the special use case before
        # reading the value of `self.cross_building`
        if self.is_mingw and tools.os_info.is_windows:
            return True

        if self.cross_building:
            return False

        return True

    @property
    def should_build_bench(self):
        # develop is false when the package is used as a requirement.
        return self.develop and not self.cross_building and self.settings.os == "Linux"

    @property
    def should_build_tracer(self):
        return self.should_build_bench and self.options.with_tracer

    @property
    def sanitizer_flag(self):
        if self.options.sanitizer:
            return " -fsanitize=%s " % self.options.sanitizer
        return None

    @property
    def is_mingw(self):
        return self.settings.os == "Windows" and self.settings.compiler == "gcc"

    def requirements(self):
        private = self.options.tankerlib_shared == True

        self.requires("Boost/1.71.0@tanker/testing", private=private)
        if self.settings.os != "Emscripten":
            if self.options.with_ssl:
                self.requires("LibreSSL/2.9.2@tanker/testing", private=private)
                self.requires("fetchpp/0.4.1@tanker/stable")
            self.requires("socket.io-client-cpp/1.6.3@tanker/testing", private=private)
            self.requires("sqlpp11/0.58@tanker/testing", private=private)
            self.requires("sqlpp11-connector-sqlite3/0.29@tanker/testing", private=private)
        self.requires("cppcodec/edf46ab@tanker/testing", private=private)
        self.requires("enum-flags/0.1a@tanker/testing", private=private)
        self.requires("fmt/6.0.0", private=private)
        self.requires("gsl-lite/0.32.0@tanker/testing", private=private)
        self.requires("jsonformoderncpp/3.4.0@tanker/testing", private=private)
        self.requires("libsodium/1.0.18@tanker/testing", private=private)
        self.requires("tconcurrent/0.30.0@tanker/stable", private=private)
        # Hack to be able to import libc++{abi}.a later on
        if self.settings.os == "iOS":
            self.requires("libc++/9.0@tanker/testing", private=private)

    def imports(self):
        if self.settings.os == "iOS":
            # on iOS, we need static libs to create universal binaries
            # Note: libtanker*.a will be copied at install time
            self.copy("*.a", dst="lib", src="lib")
        self.copy("license*", dst="licenses", folder=True, ignore_case=True)
        self.copy("copying", dst="licenses", folder=True, ignore_case=True)

    def configure(self):
        if self.settings.os == "Emscripten":
            self.options["Boost"].header_only = True
            self.options["emscripten-toolchain"].filesystem = False
        self.options["*"].fPIC = self.options.fPIC

    def build_requirements(self):
        if self.should_build_tests:
            self.build_requires("docopt.cpp/0.6.2@tanker/testing")
            self.build_requires("doctest/2.3.5@tanker/testing")
            self.build_requires("doctest-async/2.3.5-tanker2-r2@tanker/testing")
            self.build_requires("trompeloeil/v37@tanker/testing")
            if self.should_build_bench:
                self.build_requires("google-benchmark/1.4.1@tanker/testing")

    def init_cmake(self):
        if self.cmake:
            return

        self.cmake = CMake(self)

        if "CONAN_CXX_FLAGS" not in self.cmake.definitions:
            self.cmake.definitions["CONAN_CXX_FLAGS"] = ""
        if "CONAN_C_FLAGS" not in self.cmake.definitions:
            self.cmake.definitions["CONAN_C_FLAGS"] = ""

        if self.options.sanitizer:
            self.cmake.definitions["CONAN_C_FLAGS"] += self.sanitizer_flag
            self.cmake.definitions["CONAN_CXX_FLAGS"] += self.sanitizer_flag
        if self.options.coroutinests:
            self.cmake.definitions["CONAN_CXX_FLAGS"] += " -fcoroutines-ts "
        self.cmake.definitions["BUILD_TESTS"] = self.should_build_tests
        self.cmake.definitions["BUILD_BENCH"] = self.should_build_bench
        self.cmake.definitions["WITH_TRACER"] = self.should_build_tracer
        self.cmake.definitions["WARN_AS_ERROR"] = self.options.warn_as_error
        self.cmake.definitions["BUILD_TANKER_TOOLS"] = self.should_build_tests
        self.cmake.definitions["TANKER_BUILD_WITH_SSL"] = self.options.with_ssl
        if self.settings.os not in ["Windows", "Emscripten"]:
            # On Android and iOS OpenSSL can't use system ca-certificates, so we
            # ship mozilla's cacert.pem instead on all platforms but windows
            self.cmake.definitions["TANKER_EMBED_CERTIFICATES"] = True
        self.cmake.definitions["TANKERLIB_SHARED"] = self.options.tankerlib_shared
        self.cmake.definitions["CMAKE_POSITION_INDEPENDENT_CODE"] = self.options.fPIC
        self.cmake.definitions["WITH_COVERAGE"] = self.options.coverage

    def build(self):
        self.init_cmake()
        if self.should_configure:
            self.cmake.configure()
        if self.should_build:
            self.cmake.build()

    def package(self):
        self.init_cmake()
        self.cmake.install()

    def package_id(self):
        del self.info.options.warn_as_error

    def package_info(self):
        libs = [
            "tanker_admin-c",
            "ctanker",
        ]
        if not self.options.tankerlib_shared:
            libs.extend([
                "tanker_async",
                "tankerfunctionalhelpers",
                "tankeradmin",
                "tankertesthelpers",
                "tankercore",
                "tankerstreams",
                "tankernetwork",
                "tankertrustchain",
                "tankeridentity",
                "tankercrypto",
                "tankerserialization",
                "tankererrors",
                "tankerlog",
                "tankerformat",
            ])
            if self.options.with_ssl:
                libs.append("tankercacerts")

        if self.sanitizer_flag:
            self.cpp_info.sharedlinkflags = [self.sanitizer_flag]
            self.cpp_info.exelinkflags = [self.sanitizer_flag]

        if self.settings.os == "Windows" and self.options.with_ssl and not self.options.tankerlib_shared:
            libs.append("crypt32")

        self.cpp_info.libs = libs
