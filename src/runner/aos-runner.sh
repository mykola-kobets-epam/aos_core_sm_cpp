#!/bin/bash

set -ux

log() {
  echo "$(date +'%Y-%m-%d %H:%M:%S') - $1"
}

if [ $# -ne 3 ]; then
  echo "Usage: $0 <runner-cmd> <start|stop> <instance-id>"
  exit 2
fi

RUNNER="$1"
COMMAND="$2"
INSTANCE_ID="$3"
VERSION="${AOS_SERVICE_VERSION:-unknown}"

RUNTIME="/run/aos/runtime/$INSTANCE_ID/"

log "Current CGroup dir"
cat /proc/self/cgroup
ls -la /sys/fs/cgroup/system.slice/system-aos\\x2dservice.slice/aos-service@73483366-51dc-b074-ff5c-49194a94e82a.service/

case "$COMMAND" in
  start)
    if ! "$RUNNER" delete -f "$INSTANCE_ID"; then
      log "Warning: failed to delete container $INSTANCE_ID"
    else
      log "Container deleted $INSTANCE_ID"
    fi

    if ! "$RUNNER" run -d --pid-file "$RUNTIME/.pid" -b "$RUNTIME" "$INSTANCE_ID"; then
      log "Error: failed to run container $INSTANCE_ID version $VERSION"
      exit 1
    else
      log "Container $INSTANCE_ID version $VERSION started successfully"
    fi
    ;;

  stop)
    if ! "$RUNNER" kill "$INSTANCE_ID" SIGKILL; then
      log "Warning: failed to kill container $INSTANCE_ID"
    else
      log "Killed container $INSTANCE_ID"
    fi

    if ! "$RUNNER" delete -f "$INSTANCE_ID"; then
      log "Warning: failed to delete container $INSTANCE_ID"
    else
      log "Container deleted $INSTANCE_ID"
    fi
    ;;

  *)
    echo "Invalid command: $COMMAND"
    echo "Usage: $0 <runner-cmd> <start|stop> <instance-id>"
    exit 2
    ;;
esac
