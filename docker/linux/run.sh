#!/bin/bash
set -e

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

PROJECT_PATH="$(pwd)"
CONTAINER_WORKDIR="/clice"

DOCKER_RUN_ARGS=(--rm -it -w "${CONTAINER_WORKDIR}")
DOCKER_RUN_ARGS+=(--mount "type=bind,src=${PROJECT_PATH},target=${CONTAINER_WORKDIR}")
if [[ "${LLVM_PATH}" != "" ]]; then
  LLVM_MOUNT_PATH="/llvm"
  DOCKER_RUN_ARGS+=(--mount "type=bind,src=${LLVM_PATH},target=${LLVM_MOUNT_PATH}")
fi
DOCKER_RUN_ARGS+=(-w "${CONTAINER_WORKDIR}")

echo "==========================================="
echo "Running container from image: ${IMAGE_NAME}"
echo "Project mount: ${PROJECT_PATH} -> ${CONTAINER_WORKDIR}"
if [[ "${LLVM_PATH}" != "" ]]; then
  echo "LLVM mount: ${LLVM_PATH} -> ${LLVM_MOUNT_PATH}"
fi
echo "==========================================="

docker run "${DOCKER_RUN_ARGS[@]}" "${IMAGE_NAME}"