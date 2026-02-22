#!/bin/bash
set -e

echo "Codestyle check script:"
echo

# Directories/files to check — scoped to the matchmaking logic and test code.
CHECK_PATHS=(
    "src"
    "tests"
)

declare -A singleLineRegexChecks=(
    ["LOG_.+GetCounter"]="Use ObjectGuid::ToString().c_str() method instead of ObjectGuid::GetCounter() when logging. Check the lines above"
    ["[[:blank:]]$"]="Remove whitespace at the end of the lines above"
    ["\t"]="Replace tabs with 4 spaces in the lines above"
)

for check in ${!singleLineRegexChecks[@]}; do
    echo "  Checking RegEx: '${check}'"

    if grep -P -r -I -n "${check}" "${CHECK_PATHS[@]}"; then
        echo
        echo "${singleLineRegexChecks[$check]}"
        exit 1
    fi
done

declare -A multiLineRegexChecks=(
    ["LOG_[^;]+GetCounter"]="Use ObjectGuid::ToString().c_str() method instead of ObjectGuid::GetCounter() when logging. Check the lines above"
    ["\n\n\n"]="Multiple blank lines detected, keep only one. Check the files above"
)

for check in ${!multiLineRegexChecks[@]}; do
    echo "  Checking RegEx: '${check}'"

    if grep -Pzo -r -I "${check}" "${CHECK_PATHS[@]}"; then
        echo
        echo
        echo "${multiLineRegexChecks[$check]}"
        exit 1
    fi
done

echo
echo "Everything looks good"
