local common = {
  Env = {
    CPPPATH = { "." },
    CXXOPTS = {
      -- clang and GCC
      { "-std=c++14"; Config = { "*-gcc-*", "*-clang-*" } },
      { "-g"; Config = { "*-gcc-debug", "*-clang-debug" } },
      { "-g -O2"; Config = { "*-gcc-production", "*-clang-production" } },
      { "-O3"; Config = { "*-gcc-release", "*-clang-release" } },
      { "-Wall", "-Werror", "-Wextra", "-Wno-unused-parameter", "-Wno-unused-function"; Config = { "*-gcc-*", "*-clang-*" } },

      -- MSVC config
      { "/EHsc"; Config = "*-msvc-*" },
      { "/MDd"; Config = "*-msvc-debug" },
      { "/MTd"; Config = { "*-msvc-production", "*-msvc-release" } },
      {
        "/wd4127", -- conditional expression is constant
        "/wd4100", -- unreferenced formal parameter
        "/wd4324", -- structure was padded due to __declspec(align())
        Config = "*-msvc-*"
      },
    },

    CPPDEFS = {
      { "_DEBUG"; Config = "*-*-debug" },
      { "NDEBUG"; Config = "*-*-release" },
      { "_CRT_SECURE_NO_WARNINGS"; Config = "*-msvc-*" },
    },
  },
  ReplaceEnv = {
    LD = { "$(CXX)"; Config = { "*-clang-*", "*-gcc-*" } },
  },
}

Build {
  Units = function ()
    local deluxe = Program {
      Name = "deluxe68",
      Sources = {
        "deluxe.cpp",
        "main.cpp",
        "tokenizer.cpp",
        "registers.cpp"
      },
      Libs = { "pthread"; Config = "linux-*-*" },
    }
    Default(deluxe)

    local deluxeTest = Program {
      Name = "deluxe68test",
      Includes = {
        "external/gtest/googletest/include",
        "external/gtest/googletest"
      },
      Sources = {
        "tokenizer.cpp",
        "deluxe.cpp",
        "registers.cpp",
        "tests/deluxetest.cpp",
        "tests/d68test.cpp",
        "tests/tokenizer_test.cpp",
        "tests/regsave.cpp",
        "external/gtest/googletest/src/gtest-all.cc" },
      Libs = { "pthread"; Config = "linux-*-*" },
    }
    Default(deluxeTest)
  end,

  Configs = {
    Config {
      Name = "win64-msvc",
      DefaultOnHost = "windows",
      Inherit = common,
      Tools = { { "msvc-vs2015"; TargetArch = "x64" }, },
    },

    Config {
      Name = "macosx-clang",
      Inherit = common,
      Tools = { "clang-osx" },
      DefaultOnHost = "macosx",
    },
    Config {
      Name = "linux-gcc",
      Inherit = common,
      Tools = { "gcc" },
      DefaultOnHost = "linux",
    },
  },
}
