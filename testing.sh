#!/bin/bash
set -euo pipefail

SUPPORT="/opt/pluginplayground"
TWEAKS="$SUPPORT/tweaks"
OPTS="$SUPPORT/current.options"
RESULTS="$HOME/pluginplayground_test_results.txt"
SELF="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WHITELISTED=(Terminal Finder Dock Safari)

cleanup() {
    local rv=$?
    set +e
    echo ""
    echo "--- Cleaning up ---"

    if [ -f "$TWEAKS/testing.dylib" ]; then
        sudo rm -f "$TWEAKS/testing.dylib" \
                  "$TWEAKS/testing.dylib.whitelist" \
                  "$TWEAKS/testing.dylib.options"
    fi

    if [ -f "$OPTS" ]; then
        local current
        current=$(plutil -convert json -o - "$OPTS" 2>/dev/null || echo '{}')
        local cleaned
        cleaned=$(echo "$current" | python3 -c "
import sys, json
d = json.load(sys.stdin)
et = d.get('enabledTweaks', [])
d['enabledTweaks'] = [e for e in et if e != 'testing.dylib']
json.dump(d, sys.stdout)
" 2>/dev/null) || true
        if [ -n "$cleaned" ]; then
            echo "$cleaned" | plutil -convert xml1 -o "$OPTS" - 2>/dev/null || true
        fi
    fi

    if [ -f "$RESULTS" ]; then
        rm -f "$RESULTS"
    fi
    echo "Cleanup done."
    exit $rv
}

trap cleanup EXIT INT TERM

echo "============================================"
echo "  Plugin Playground — Capability Test Suite"
echo "============================================"
echo ""

echo "1) Checking Plugin Playground installation..."
for d in "$SUPPORT" "$SUPPORT/lib" "$TWEAKS"; do
    if [ ! -d "$d" ]; then
        echo "   FAIL: $d does not exist"
        echo "   Install playground first: sudo ./install.sh"
        exit 1
    fi
done
for f in "$SUPPORT/lib/libfangs_hook.dylib" \
         "$SUPPORT/lib/libplayground_opener.dylib" \
         "$SUPPORT/lib/fridagum.dylib" \
         "$SUPPORT/bin/grant" \
         "$OPTS"; do
    if [ ! -f "$f" ]; then
        echo "   FAIL: $f not found"
        exit 1
    fi
done
echo "   Infrastructure OK"

echo ""
echo "2) Checking LaunchDaemon..."
if ! launchctl list com.pluginplayground.grant &>/dev/null; then
    echo "   FAIL: grant LaunchDaemon not loaded"
    echo "   Install playground first: sudo ./install.sh"
    exit 1
fi
echo "   LaunchDaemon OK (grant loaded)"

echo ""
echo "3) Building testing tweak..."
make -C "$SELF/testing" clean 2>/dev/null || true
make -C "$SELF/testing"
echo "   Build OK"

echo ""
echo "4) Installing testing tweak..."
sudo cp "$SELF/testing/testing.dylib"           "$TWEAKS/testing.dylib"
sudo cp "$SELF/testing/testing.dylib.whitelist" "$TWEAKS/testing.dylib.whitelist"
sudo cp "$SELF/testing/testing.dylib.options"   "$TWEAKS/testing.dylib.options"
sudo chown root:wheel "$TWEAKS/testing.dylib" \
                      "$TWEAKS/testing.dylib.whitelist" \
                      "$TWEAKS/testing.dylib.options"
sudo chmod 644 "$TWEAKS/testing.dylib.whitelist" \
               "$TWEAKS/testing.dylib.options"

current=$(plutil -convert json -o - "$OPTS" 2>/dev/null || echo '{}')
patched=$(echo "$current" | python3 -c "
import sys, json
d = json.load(sys.stdin)
et = d.get('enabledTweaks', [])
if 'testing.dylib' not in et:
    et.append('testing.dylib')
d['enabledTweaks'] = et
json.dump(d, sys.stdout)
")
echo "$patched" | sudo plutil -convert xml1 -o "$OPTS" -
echo "   Install OK"

echo ""
echo "5) Triggering test via SIGUSR1..."
rm -f "$RESULTS"
TARGET=""
for name in "${WHITELISTED[@]}"; do
    pid=$(pgrep -x "$name" 2>/dev/null || true)
    if [ -z "$pid" ]; then continue; fi
    if lsof -p "$pid" 2>/dev/null | grep -q "libplayground_opener"; then
        TARGET="$name (PID $pid)"
        echo "   Sending SIGUSR1 to $name (PID $pid)"
        kill -SIGUSR1 "$pid" 2>/dev/null || true
        break
    fi
done

if [ -z "$TARGET" ]; then
    echo "   No whitelisted process with playground_opener loaded found."
    echo "   Launch a whitelisted app (e.g. Terminal) and re-run."
    exit 1
fi

echo ""
echo "6) Waiting for results..."
for i in $(seq 1 30); do
    if [ -f "$RESULTS" ]; then
        echo ""
        cat "$RESULTS"
        break
    fi
    sleep 1
done

if [ ! -f "$RESULTS" ]; then
    echo "   FAIL: no results file after 30 seconds."
    echo "   Check /var/log/system.log for playground_opener errors."
    exit 1
fi

passed=$(grep -c '\[OK\]' "$RESULTS" 2>/dev/null || echo 0)
failed=$(grep -c "FAILED" "$RESULTS" 2>/dev/null || echo 0)

echo ""
echo "============================================"
echo "  PASSED: $passed   FAILED: $failed   TOTAL: $((passed + failed))"
echo "============================================"

if [ "$failed" -ne 0 ]; then
    exit 1
fi

echo "  ALL CAPABILITIES VERIFIED"
echo ""
