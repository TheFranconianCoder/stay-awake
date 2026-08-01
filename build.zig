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
            .link_libc = true,
        }),
    });

    if (optimize == .ReleaseSmall) {
        exe.root_module.strip = true;
        exe.lto = .full;
    }

    exe.root_module.addCSourceFiles(.{
        .files = &.{
            "src/main.c",
            "src/config.c",
            "src/tray.c",
            "src/power.c",
        },
        .flags = &.{
            "-std=c23",
            "-Wall",
            "-Wextra",
            "-DUNICODE",
            "-D_UNICODE",
        },
    });

    exe.root_module.addIncludePath(b.path("src"));

    const res_file = b.path("resource.rc");
    exe.root_module.addWin32ResourceFile(.{
        .file = res_file,
    });

    const icon_dep = b.path("app_icon.ico");
    exe.step.dependOn(&b.addInstallFile(icon_dep, "app_icon.ico").step);

    exe.root_module.linkSystemLibrary("user32", .{});
    exe.root_module.linkSystemLibrary("shell32", .{});
    exe.root_module.linkSystemLibrary("kernel32", .{});
    exe.root_module.linkSystemLibrary("gdi32", .{});
    exe.root_module.linkSystemLibrary("ole32", .{});
    exe.root_module.linkSystemLibrary("advapi32", .{});

    exe.subsystem = .Windows;
    b.installArtifact(exe);
}
