const Builder = @import("std").build.Builder;

pub fn build(b: &Builder) {
    var exe = b.addExe("src/main.zig", "genesis");

    //exe.addCompileVar("version_major", u32, 0);
    //exe.addCompileVar("version_minor", u32, 0);
    //exe.addCompileVar("version_patch", u32, 0);
}
