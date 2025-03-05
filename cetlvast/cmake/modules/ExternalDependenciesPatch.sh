#!/bin/bash

# Usage: ./ExternalDependenciesPatch.sh <patch_file>
PATCH_FILE=$1

if [ -z "$PATCH_FILE" ]; then
  echo "Usage: $0 <patch_file>"
  exit 1
fi

if git apply --reverse --check "$PATCH_FILE"; then
  echo "Patch is already applied."
else
  git apply --whitespace=fix "$PATCH_FILE"
fi
