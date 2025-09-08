#!/bin/bash
set -e

# default configurations
COMPILER="clang"
LIB_TYPE="non_lto"
DOCKERFILE_PATH="docker/linux/Dockerfile"

usage() {
cat <<EOF
Usage: $0 [--compiler <gcc|clang>] [--lib <lto|non_lto>] [--dockerfile <path>]

Defaults:
  --compiler    ${COMPILER}
  --lib         ${LIB_TYPE}
  --dockerfile  ${DOCKERFILE_PATH}
EOF
}

# parse command line arguments
while [ "$#" -gt 0 ]; do
    case "$1" in
        --compiler)
            COMPILER="$2"; shift 2;;
        --lib)
            LIB_TYPE="$2"; shift 2;;
        --dockerfile)
            DOCKERFILE_PATH="$2"; shift 2;;
        -h|--help)
            usage; exit 0;;
        *)
            echo "Unknown parameter: $1" >&2; usage; exit 1;;
    esac
done

IMAGE_TAG="Linux-${COMPILER}-${LIB_TYPE}"
IMAGE_NAME="clice-dev-container:${IMAGE_TAG}"

echo "==========================================="
echo "Building image: ${IMAGE_NAME}"
echo "Compiler: ${COMPILER}"
echo "Lib Type: ${LIB_TYPE}"
echo "Dockerfile: ${DOCKERFILE_PATH}"
echo "==========================================="

# build the docker image with specified arguments
docker buildx build -t "${IMAGE_NAME}" \
    --build-arg COMPILER="${COMPILER}" \
    --build-arg LTO_TYPE="${LIB_TYPE}" \
    -f "${DOCKERFILE_PATH}" .

echo "Build complete. Image:${IMAGE_NAME}"