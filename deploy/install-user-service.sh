#!/bin/sh
set -eu

archive=${1:?Usage: install-user-service.sh RELEASE.tar.gz}
archive=$(CDPATH= cd -- "$(dirname -- "$archive")" && pwd -P)/$(basename -- "$archive")
package=$(basename -- "$archive" .tar.gz)
case "$package" in
  WizardCard-CentralServer-*-linux-x86_64) ;;
  *) echo "Unexpected release archive name: $package" >&2; exit 2 ;;
esac

install_root="$HOME/.local/opt/wizardcard-central-server"
release_root="$install_root/releases"
release_path="$release_root/$package"
install -d -m 0755 "$release_root"
temporary=$(mktemp -d "$release_root/.unpack.XXXXXX")
trap 'rm -rf "$temporary"' EXIT HUP INT TERM
if [ -e "$release_path" ]; then
  echo "Release already exists: $release_path" >&2
  exit 1
fi
tar -xzf "$archive" -C "$temporary"
test -x "$temporary/$package/bin/wizard_server"
mv "$temporary/$package" "$release_path"
ln -sfn "$release_path" "$install_root/.current.next"
mv -Tf "$install_root/.current.next" "$install_root/current"

config_root="$HOME/.config/wizardcard-central-server"
data_root="$HOME/.local/share/wizardcard-central-server"
unit_root="$HOME/.config/systemd/user"
install -d -m 0700 "$config_root" "$data_root"
install -d -m 0755 "$unit_root"
if [ ! -e "$config_root/server.env" ]; then
  install -m 0600 "$release_path/server.env.example" "$config_root/server.env"
fi
if [ ! -e "$unit_root/wizard-server.service" ]; then
  install -m 0644 "$release_path/systemd/wizard-server.service" "$unit_root/wizard-server.service"
fi
if grep -q '/home/your-user' "$config_root/server.env"; then
  printf 'Installed %s. Edit %s, install the certificate and key, then start with: systemctl --user enable --now wizard-server.service\n' \
    "$package" "$config_root/server.env"
  exit 0
fi
systemctl --user daemon-reload
systemctl --user enable wizard-server.service
if systemctl --user is-active --quiet wizard-server.service; then
  systemctl --user restart wizard-server.service
else
  systemctl --user start wizard-server.service
fi
printf 'Installed %s and restarted wizard-server.service\n' "$package"
