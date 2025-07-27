const std = @import("std");
const testing = std.testing;

test "application test suite" {
    std.debug.print("\n🧪 Application Test Suite\n", .{});
    std.debug.print("========================\n\n", .{});

    // Ensure binary is built
    const allocator = testing.allocator;
    const binary_path = "./zig-out/bin/fry";
    const file = std.fs.cwd().openFile(binary_path, .{}) catch {
        std.debug.print("📦 Building application binary...\n", .{});
        try runBuild(allocator);
        std.debug.print("✓ Build complete\n\n", .{});
        return;
    };
    file.close();

    std.debug.print("Running all tests...\n\n", .{});

    // Run basic functionality tests
    try testHelp(allocator);
    try testVersion(allocator);
    try testCommands(allocator);
    try testInvalidCommand(allocator);

    // Note: TUI menu command is not tested here as it requires
    // an interactive terminal. It should be tested manually.
}

fn runBuild(allocator: std.mem.Allocator) !void {
    const result = try std.process.Child.run(.{
        .allocator = allocator,
        .argv = &[_][]const u8{ "zig", "build" },
    });
    defer allocator.free(result.stdout);
    defer allocator.free(result.stderr);

    if (result.term.Exited != 0) {
        std.debug.print("Build failed:\n{s}\n", .{result.stderr});
        return error.BuildFailed;
    }
}

fn testHelp(allocator: std.mem.Allocator) !void {
    std.debug.print("Testing --help flag...\n", .{});

    const result = try std.process.Child.run(.{
        .allocator = allocator,
        .argv = &[_][]const u8{ "./zig-out/bin/fry", "--help" },
    });
    defer allocator.free(result.stdout);
    defer allocator.free(result.stderr);

    try testing.expect(result.term.Exited == 0);
    try testing.expect(result.stdout.len > 0);
    try testing.expect(std.mem.indexOf(u8, result.stdout, "USAGE") != null);

    std.debug.print("✓ Help flag works correctly\n", .{});
}

fn testVersion(allocator: std.mem.Allocator) !void {
    std.debug.print("Testing --version flag...\n", .{});

    const result = try std.process.Child.run(.{
        .allocator = allocator,
        .argv = &[_][]const u8{ "./zig-out/bin/fry", "--version" },
    });
    defer allocator.free(result.stdout);
    defer allocator.free(result.stderr);

    try testing.expect(result.term.Exited == 0);
    try testing.expect(result.stdout.len > 0);

    std.debug.print("✓ Version flag works correctly\n", .{});
}

fn testCommands(allocator: std.mem.Allocator) !void {
    std.debug.print("Testing built-in commands...\n", .{});

    // Test accounts list command
    {
        const result = try std.process.Child.run(.{
            .allocator = allocator,
            .argv = &[_][]const u8{ "./zig-out/bin/fry", "accounts", "list" },
        });
        defer allocator.free(result.stdout);
        defer allocator.free(result.stderr);

        // This command should succeed even with no accounts
        try testing.expect(result.term.Exited == 0);
    }

    // Test config show command
    {
        const result = try std.process.Child.run(.{
            .allocator = allocator,
            .argv = &[_][]const u8{ "./zig-out/bin/fry", "config", "show" },
        });
        defer allocator.free(result.stdout);
        defer allocator.free(result.stderr);

        try testing.expect(result.term.Exited == 0);
        try testing.expect(std.mem.indexOf(u8, result.stdout, "Configuration:") != null);
    }

    std.debug.print("✓ All commands work correctly\n", .{});
}

fn testInvalidCommand(allocator: std.mem.Allocator) !void {
    std.debug.print("Testing invalid command handling...\n", .{});

    const result = try std.process.Child.run(.{
        .allocator = allocator,
        .argv = &[_][]const u8{ "./zig-out/bin/fry", "invalid" },
    });
    defer allocator.free(result.stdout);
    defer allocator.free(result.stderr);

    try testing.expect(result.term.Exited != 0);

    // Check stderr for the error message (error output goes to stderr)
    try testing.expect(std.mem.indexOf(u8, result.stderr, "Error: Missing verb for noun") != null);
    std.debug.print("✓ Invalid command handling works correctly\n", .{});
}
