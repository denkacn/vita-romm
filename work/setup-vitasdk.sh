#!/usr/bin/env bash
set -euo pipefail

LINUX_USER="${1:-denkacn}"
VITASDK_DIR="/usr/local/vitasdk"
VDPM_DIR="/home/${LINUX_USER}/src/vdpm"
LOG_PREFIX="[vitasdk-setup]"

echo "${LOG_PREFIX} user: ${LINUX_USER}"
echo "${LOG_PREFIX} installing host packages"
export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y \
  build-essential \
  ca-certificates \
  cmake \
  curl \
  git \
  make \
  patch \
  pkg-config \
  p7zip-full \
  python-is-python3 \
  python3 \
  tar \
  unzip \
  wget \
  xz-utils

echo "${LOG_PREFIX} preparing shell environment"
cat >/etc/profile.d/vitasdk.sh <<'ENVEOF'
export VITASDK=/usr/local/vitasdk
export PATH=$VITASDK/bin:$PATH
ENVEOF

USER_HOME="/home/${LINUX_USER}"
touch "${USER_HOME}/.bashrc"
if ! grep -q "VITASDK=/usr/local/vitasdk" "${USER_HOME}/.bashrc"; then
  cat >>"${USER_HOME}/.bashrc" <<'ENVEOF'

# VitaSDK
export VITASDK=/usr/local/vitasdk
export PATH=$VITASDK/bin:$PATH
ENVEOF
fi
chown "${LINUX_USER}:${LINUX_USER}" "${USER_HOME}/.bashrc"

echo "${LOG_PREFIX} cloning/updating vdpm"
install -d -o "${LINUX_USER}" -g "${LINUX_USER}" "${USER_HOME}/src"
if [ -d "${VDPM_DIR}/.git" ]; then
  su - "${LINUX_USER}" -c "cd '${VDPM_DIR}' && git pull --ff-only"
else
  su - "${LINUX_USER}" -c "git clone https://github.com/vitasdk/vdpm '${VDPM_DIR}'"
fi

if [ ! -x "${VITASDK_DIR}/bin/arm-vita-eabi-gcc" ]; then
  echo "${LOG_PREFIX} removing incomplete ${VITASDK_DIR}"
  rm -rf "${VITASDK_DIR}"
fi

cd "${VDPM_DIR}"
export VITASDK="${VITASDK_DIR}"
export PATH="${VITASDK_DIR}/bin:${PATH}"
if [ -x "${VITASDK_DIR}/bin/arm-vita-eabi-gcc" ]; then
  echo "${LOG_PREFIX} VitaSDK core already bootstrapped"
else
  echo "${LOG_PREFIX} bootstrapping VitaSDK"
  ./bootstrap-vitasdk.sh
fi

echo "${LOG_PREFIX} installing common VitaSDK packages"
yes | ./install-all.sh
chown -R "${LINUX_USER}:${LINUX_USER}" "${VITASDK_DIR}"

echo "${LOG_PREFIX} versions"
su - "${LINUX_USER}" -c "export VITASDK='${VITASDK_DIR}' && export PATH='${VITASDK_DIR}/bin':\$PATH && which arm-vita-eabi-gcc && arm-vita-eabi-gcc --version | head -n 1 && which vita-mksfoex && which vita-pack-vpk"

echo "${LOG_PREFIX} done"
