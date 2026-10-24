#!/bin/bash
set -u

cd "$(dirname "$0")" || exit 1

bash ./scripts/build.sh "$@"
STATUS=$?

echo
if [ -t 0 ]; then
    read -r -p "Press Return to close..." _
fi

exit "$STATUS"
