load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def assimp_archive(name, build_file, strip_prefix):
    http_archive(
        name = name,
        urls = ["https://github.com/lun0522/lib-assimp/archive/5.0.1.zip"],
        sha256 = "18ec9cb0f91f4ac1904676a756d41595e227a173ff4c8477e9a840e36e12efc6",
        strip_prefix = "lib-assimp-5.0.1/" + strip_prefix,
        build_file = build_file,
    )

def freetype_archive(name, build_file):
    http_archive(
        name = name,
        urls = ["https://download.savannah.gnu.org/releases/freetype/freetype-2.10.4.tar.gz"],
        sha256 = "5eab795ebb23ac77001cfb68b7d4d50b5d6c7469247b0b01b2c953269f658dac",
        strip_prefix = "freetype-2.10.4",
        build_file = build_file,
    )

def glfw_archive(name, build_file, strip_prefix):
    http_archive(
        name = name,
        urls = ["https://github.com/lun0522/lib-glfw/archive/3.3.4.zip"],
        sha256 = "47d144571d813f3641ab23d9f6fb2e38e04cb70ef0ebecfb1657a29342800e17",
        strip_prefix = "lib-glfw-3.3.4/" + strip_prefix,
        build_file = build_file,
    )

def spirv_cross_archive(platform, url, sha256):
    http_archive(
        name = "lib-spirv-cross-" + platform,
        urls = [url],
        sha256 = sha256,
        build_file = "//:third_party/BUILD.spirv_cross",
    )
