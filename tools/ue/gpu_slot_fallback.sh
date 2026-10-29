#!/bin/bash
# Drop-in replacement for the shared GPU-slot coordinator on machines that do
# not have one. Every tools/ue wrapper calls `"$GPU_SLOT" capture --label X --
# <command...>`; this shim consumes the scheduling arguments and execs the
# command directly, so the same wrappers work on a plain checkout.
#
# There is deliberately no GPU accounting here: the real coordinator
# (GPU_SLOT=/path/to/gpu_slot.sh) caps concurrent renderers for shared-GPU
# machines. On your own box, launching one engine at a time is on you.
set -u
while [ $# -gt 0 ]; do
  case "$1" in
    --) shift; exec "$@" ;;
    *) shift ;;
  esac
done
echo "gpu_slot_fallback: no command found after --" >&2
exit 2
