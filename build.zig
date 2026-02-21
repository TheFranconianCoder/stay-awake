const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{ .preferred_optimize_mode = .ReleaseSmall });

    const arch_name = if (target.result.cpu.arch == .x86_64) "x86_64" else @tagName(target.result.cpu.arch);
    const exe_name = b.fmt("StayAwake-windows-{s}", .{arch_name});

    const exe = b.addExecutable(.{
        .name = exe_name,
        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
        }),
    });

    if (optimize == .ReleaseSmall) {
        exe.root_module.strip = true;
        exe.want_lto = true;
    }

    const c_flags = &.{ "-std=c11", "-DUNICODE", "-D_UNICODE" };

    exe.addCSourceFiles(.{
        .files = &.{
            "src/main.c",
            "src/config.c",
            "src/tray.c",
            "src/power.c",
        },
        .flags = c_flags,
    });

    exe.addIncludePath(b.path("src"));

    const res_file = b.path("resource.rc");
    exe.addWin32ResourceFile(.{
        .file = res_file,
    });

    const icon_dep = b.path("app_icon.ico");
    exe.step.dependOn(&b.addInstallFile(icon_dep, "app_icon.ico").step);

    exe.linkSystemLibrary("user32");
    exe.linkSystemLibrary("shell32");
    exe.linkSystemLibrary("kernel32");
    exe.linkSystemLibrary("gdi32");
    exe.linkSystemLibrary("ole32");
    exe.linkSystemLibrary("advapi32");
    exe.linkLibC();

    exe.subsystem = .Windows;
    b.installArtifact(exe);
}
