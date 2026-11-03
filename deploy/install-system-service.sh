#!/bin/sh
set -eu

test "$(id -u)" = 0 || { echo 'Run this installer with sudo.' >&2; exit 1; }
archive=${1:?Usage: install-system-service.sh RELEASE.tar.gz}
archive=$(CDPATH= cd -- "$(dirname -- "$archive")" && pwd -P)/$(basename -- "$archive")
package=$(basename -- "$archive" .tar.gz)
case "$package" in
  WizardCard-CentralServer-*-linux-x86_64) ;;
  *) echo "Unexpected release archive name: $package" >&2; exit 2 ;;
esac

getent group wizardcard >/dev/null || groupadd --system wizardcard
id wizardcard >/dev/null 2>&1 || useradd --system --gid wizardcard \
  --home-dir /var/lib/wizardcard-central-server --no-create-home --shell /usr/sbin/nologin wizardcard
release_root=/opt/wizardcard-central-server/releases
release_path="$release_root/$package"
install -d -m 0755 "$release_root"
install -d -m 0750 -o wizardcard -g wizardcard /var/lib/wizardcard-central-server
install -d -m 0750 -o root -g wizardcard /etc/wizardcard
test ! -e "$release_path" || { echo "Release already exists: $release_path" >&2; exit 1; }
temporary=$(mktemp -d "$release_root/.unpack.XXXXXX")
trap 'rm -rf "$temporary"' EXIT HUP INT TERM
tar -xzf "$archive" -C "$temporary"
test -x "$temporary/$package/bin/wizard_server"
chown -R root:root "$temporary/$package"
mv "$temporary/$package" "$release_path"
ln -sfn "$release_path" /opt/wizardcard-central-server/.current.next
mv -Tf /opt/wizardcard-central-server/.current.next /opt/wizardcard-central-server/current
install -m 0644 "$release_path/systemd/wizard-server-system.service" /etc/systemd/system/wizard-server.service
if [ ! -e /etc/wizardcard/server.env ]; then
  cat > /etc/wizardcard/server.env <<'ENV'
WIZARD_CERT=/etc/wizardcard/server.crt
WIZARD_KEY=/etc/wizardcard/server.key
WIZARD_DATA_DIR=/var/lib/wizardcard-central-server
WIZARD_BIND=0.0.0.0
WIZARD_PORT=24443
WIZARD_HEALTH_PORT=24444
ENV
  chown root:wizardcard /etc/wizardcard/server.env
  chmod 0640 /etc/wizardcard/server.env
fi
systemctl daemon-reload
if [ -r /etc/wizardcard/server.crt ] && [ -r /etc/wizardcard/server.key ]; then
  systemctl enable wizard-server.service
  systemctl restart wizard-server.service
else
  echo 'Install server.crt/server.key in /etc/wizardcard as root:wizardcard, mode 0640, then systemctl enable --now wizard-server.'
fi
