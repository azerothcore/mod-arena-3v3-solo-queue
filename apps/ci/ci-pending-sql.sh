#!/usr/bin/env bash
set -e

# Check for pending SQL files that have not yet been committed to the
# data/sql/ directory.  Pending files live in data/sql/pending_*/.
PENDING=$(find data/sql -type d -name "pending_*" -exec find {} -name "*.sql" \; 2>/dev/null)

if [ -n "$PENDING" ]; then
    echo "Pending SQL files found — please move them to the appropriate data/sql/ subdirectory before merging:"
    echo "$PENDING"
    exit 1
fi

echo "No pending SQL files found."
exit 0
