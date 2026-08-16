Import("env")

# SinricProConfig.h hardcodes WEBSOCKET_PING_INTERVAL to only two values:
# 10000ms (if DEBUG_WIFI_ISSUE is defined, our build flag) or 300000ms
# (default) -- it's not guarded with #ifndef like the library's other
# settings, so it can't be overridden with a plain build_flags -D define.
# This patches the installed copy directly so we can use an in-between
# value (30s) instead. Runs on every build so it's reapplied automatically
# after a fresh `pio pkg install` on any machine, not just a one-off edit
# that would silently revert.

import os

PING_INTERVAL_MS = "30000"


def patch_sinricpro_ping(env):
    libdeps_dir = env.subst("$PROJECT_LIBDEPS_DIR")
    pioenv = env["PIOENV"]
    config_path = os.path.join(libdeps_dir, pioenv, "SinricPro", "src", "SinricProConfig.h")

    if not os.path.isfile(config_path):
        print("patch_sinricpro_ping: SinricProConfig.h not found yet -- "
              "skipping, will patch on next build once the library is installed")
        return

    with open(config_path, "r") as f:
        content = f.read()

    # Anchored with a trailing newline: "WEBSOCKET_PING_INTERVAL 30000" would
    # otherwise also match as a substring prefix of the existing (unwanted)
    # "WEBSOCKET_PING_INTERVAL 300000" line.
    old = "#define WEBSOCKET_PING_INTERVAL 10000\n"
    new = "#define WEBSOCKET_PING_INTERVAL " + PING_INTERVAL_MS + "\n"

    if new in content:
        return  # already patched

    if old not in content:
        print("patch_sinricpro_ping: expected line not found in %s -- "
              "library may have changed, skipping patch" % config_path)
        return

    with open(config_path, "w") as f:
        f.write(content.replace(old, new))
    print("patch_sinricpro_ping: set WEBSOCKET_PING_INTERVAL to %sms" % PING_INTERVAL_MS)


patch_sinricpro_ping(env)
