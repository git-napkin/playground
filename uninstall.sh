#!/bin/sh
set -eu

echo "[-] Uninstalling Plugin Playground..."

if [ "$(id -u)" -ne 0 ]; then
    echo "Elevating privileges to uninstall system-wide files..."
    exec sudo "$0" "$@"
fi

echo "Pausing injection to stop new spawns from referencing deleted dylibs..."
if [ -f "/opt/pluginplayground/current.options" ]; then
    defaults write /opt/pluginplayground/current.options pauseInjection -bool true
fi

echo "Unloading grant daemon..."
launchctl bootout system/com.pluginplayground.grant 2>/dev/null || true
sleep 1

echo "Removing Configurator application..."
rm -rf "/Applications/Plugin Playground.app"

echo "Removing core binaries and data..."
rm -rf "/opt/pluginplayground"

echo "Removing launch daemon..."
rm -f "/Library/LaunchDaemons/com.pluginplayground.grant.plist"

echo "Removing log files..."
rm -rf "/var/log/pluginplayground"

echo "Forgetting package receipt..."
pkgutil --forget "com.pluginplayground.core" > /dev/null 2>&1 || true

echo "[-] Uninstallation complete."
echo "    A reboot is recommended to fully deactivate any injected code."
