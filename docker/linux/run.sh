#!/bin/bash
set -e

# Save original working directory and switch to project root
ORIG_PWD="$(pwd)"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
echo "${SCRIPT_DIR}"
cd "${SCRIPT_DIR}/../.."
PROJECT_ROOT="$(pwd)"

trap 'cd "${ORIG_PWD}"' EXIT

# default configurations
COMPILER="clang"
LIB_TYPE="non_lto"
LLVM_PATH=""

usage() {
cat <<EOF
Usage: $0 [--compiler <gcc|clang>] [--lib <lto|non_lto>] [--llvm-path <host-path>]

Defaults:
  --compiler    ${COMPILER}
  --lib         ${LIB_TYPE}
  --llvm-path    (none)
EOF
}

# parse command line arguments
while [ "$#" -gt 0 ]; do
  case "$1" in
    --compiler)
      COMPILER="$2"; shift 2;;
    --lib)
      LIB_TYPE="$2"; shift 2;;
    --llvm-path)
      LLVM_PATH="$2"; shift 2;;
    -h|--help)
      usage; exit 0;;
    *) echo "Unknown parameter: $1"; usage; exit 1;;
  esac
done

IMAGE_TAG="Linux-${COMPILER}-${LIB_TYPE}"
IMAGE_NAME="clice-dev-container:${IMAGE_TAG}"

# If the image doesn't exist, build it automatically by invoking build.sh
if ! docker image inspect "${IMAGE_NAME}" >/dev/null 2>&1; then
  echo "Image ${IMAGE_NAME} not found, invoking build.sh to create it..."
  ./docker/linux/build.sh --compiler "${COMPILER}" --lib "${LIB_TYPE}"
fi

CONTAINER_WORKDIR="/clice"

DOCKER_RUN_ARGS=(--rm -it -w "${CONTAINER_WORKDIR}")
DOCKER_RUN_ARGS+=(--mount "type=bind,src=${PROJECT_ROOT},target=${CONTAINER_WORKDIR}")
if [[ "${LLVM_PATH}" != "" ]]; then
  LLVM_MOUNT_PATH="/llvm"
  DOCKER_RUN_ARGS+=(--mount "type=bind,src=${LLVM_PATH},target=${LLVM_MOUNT_PATH}")
fi
DOCKER_RUN_ARGS+=(-w "${CONTAINER_WORKDIR}")

echo "==========================================="
echo "Running container from image: ${IMAGE_NAME}"
echo "Project mount: ${PROJECT_ROOT} -> ${CONTAINER_WORKDIR}"
if [[ "${LLVM_PATH}" != "" ]]; then
  echo "LLVM mount: ${LLVM_PATH} -> ${LLVM_MOUNT_PATH}"
fi
echo "==========================================="

docker run "${DOCKER_RUN_ARGS[@]}" "${IMAGE_NAME}"
