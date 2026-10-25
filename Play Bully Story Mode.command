#!/bin/bash
# Double-click launcher for Bully Remake story mode (windowed). Builds first if the sources changed.
cd "$(dirname "$0")" || exit 1
"./tools/play_story.sh" "$@"
RC=$?
if [ $RC -ne 0 ]; then echo; echo "Story mode exited with code $RC. Press Return to close."; read -r _; fi
exit $RC
