#!/usr/bin/env bash
set -euo pipefail

project_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)
editor_path=${GODOT_BIN:-godot}
assets_path="$project_root/assets/release-v260"
user_directory=""
while (($#)); do
    case "$1" in
        --editor-path|--assets|--user-data)
            (($# >= 2)) || { echo "Missing value for $1" >&2; exit 2; }
            case "$1" in
                --editor-path) editor_path=$2 ;;
                --assets) assets_path=$2 ;;
                --user-data) user_directory=$2 ;;
            esac
            shift 2 ;;
        --) shift; break ;;
        -h|--help)
            echo "Usage: $0 [--editor-path <godot>] [--assets <directory>] [--user-data <directory>] [-- <Godot options>]"
            exit 0 ;;
        *) echo "Unknown argument: $1" >&2; exit 2 ;;
    esac
done
editor_path=$(command -v -- "$editor_path") || { echo "Provide --editor-path for Godot 4.7.2" >&2; exit 1; }
editor_path=$(realpath -- "$editor_path")
editor_version=$("$editor_path" --version)
if [[ ! $editor_version =~ ^4\.7\.2\.stable\.[A-Za-z0-9_-]+\.ed1daf0bf$ ]]; then
    echo "Expected Godot 4.7.2 commit ed1daf0bf, found $editor_version" >&2
    exit 1
fi
[[ -f "$project_root/godot/bin/libwizard_godot_bridge.debug.so" ]] || {
    echo "Build cmake --preset linux-godot-debug first; see godot/README.md" >&2; exit 1;
}
assets_path=$(realpath -- "$assets_path")
[[ -f "$assets_path/catalog.json" ]] || { echo "Missing assets/catalog.json" >&2; exit 1; }
frontend_args=(--assets "$assets_path")
if [[ -n $user_directory ]]; then
    frontend_args+=(--user-data "$(realpath -m -- "$user_directory")")
fi
"$editor_path" --headless --path "$project_root/godot" --import --frame-delay 1000
launch_directory=$(mktemp -d -t wizardcard-launch-XXXXXXXX)
trap 'rm -rf -- "$launch_directory"' EXIT
cd -- "$launch_directory"
"$editor_path" --path "$project_root/godot" "$@" -- "${frontend_args[@]}"
