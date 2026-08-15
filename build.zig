const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{ .preferred_optimize_mode = .ReleaseSmall });

    const isWindows = target.result.os.tag == .windows;
    const isLinux = target.result.os.tag == .linux;

    const os_tag = if (isWindows) "windows" else "linux";
    const arch_name = if (target.result.cpu.arch == .x86_64) "x86_64" else @tagName(target.result.cpu.arch);
    const exe_name = b.fmt("StayAwake-{s}-{s}", .{ os_tag, arch_name });

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

    const base_flags: []const []const u8 = &.{
        "-std=c23",
        "-Wall",
        "-Wextra",
    };

    const linux_flags: []const []const u8 = &.{
        "-std=c23",
        "-Wall",
        "-Wextra",
        "-D_GNU_SOURCE",
    };

    const win_flags: []const []const u8 = &.{
        "-std=c23",
        "-Wall",
        "-Wextra",
        "-DUNICODE",
        "-D_UNICODE",
    };

    exe.root_module.addCSourceFiles(.{
        .files = &.{
            "src/main.c",
            "src/config.c",
            "src/tray.c",
            "src/power.c",
        },
        .flags = if (isWindows) win_flags else if (isLinux) linux_flags else base_flags,
    });

    exe.root_module.addIncludePath(b.path("src"));

    if (isWindows) {
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
    }

    if (isLinux) {
        exe.root_module.linkSystemLibrary("gtk+-3.0", .{});
        exe.root_module.linkSystemLibrary("ayatana-appindicator3-0.1", .{});

        // Install icons next to the binary (dark mode = default, light mode = light/ subdir)
        const awake_icon = b.path("src/icons/awake.png");
        exe.step.dependOn(&b.addInstallFileWithDir(awake_icon, .bin, "awake.png").step);
        comptime var pct: u8 = 0;
        inline while (pct <= 100) : (pct += 10) {
            const icon_name = std.fmt.comptimePrint("auto_off_{d:0>3}.png", .{pct});
            const icon_path = b.path("src/icons/" ++ icon_name);
            exe.step.dependOn(&b.addInstallFileWithDir(icon_path, .bin, icon_name).step);
        }
        // Light mode icons
        const awake_icon_light = b.path("src/icons/light/awake.png");
        exe.step.dependOn(&b.addInstallFileWithDir(awake_icon_light, .bin, "light/awake.png").step);
        comptime var pct2: u8 = 0;
        inline while (pct2 <= 100) : (pct2 += 10) {
            const icon_name = std.fmt.comptimePrint("auto_off_{d:0>3}.png", .{pct2});
            const icon_path = b.path("src/icons/light/" ++ icon_name);
            exe.step.dependOn(&b.addInstallFileWithDir(icon_path, .bin, "light/" ++ icon_name).step);
        }
    }

    b.installArtifact(exe);
}
