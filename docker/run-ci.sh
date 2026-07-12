#!/bin/bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"

IMAGE_NAME="projectv-ci"
VOLUME_NAME="projectv-sccache"

echo "=== Building Docker image ${IMAGE_NAME} ==="
docker build --network host -t "${IMAGE_NAME}" -f docker/Dockerfile.ci .

echo "=== Running CI gate in container ==="
docker run --rm --network host \
  -v "$(pwd)":/workspace \
  -v "${VOLUME_NAME}":/root/.cache/sccache \
  "${IMAGE_NAME}" \
  /workspace/docker/build-ci.sh
