#!/usr/bin/env bash

readonly PROCESS_NAME="ircserv"
readonly REFRESH_SECONDS=13

readonly COLOR_RESET=$'\033[0m'
readonly COLOR_BOLD=$'\033[1m'
readonly COLOR_DIM=$'\033[2m'
readonly COLOR_RED=$'\033[31m'
readonly COLOR_GREEN=$'\033[32m'
readonly COLOR_YELLOW=$'\033[33m'
readonly COLOR_BLUE=$'\033[34m'
readonly COLOR_MAGENTA=$'\033[35m'
readonly COLOR_CYAN=$'\033[36m'

cleanup()
{
    printf '\033[?25h\033[?1049l'
}

render_colored_statistics()
{
    awk \
        -v reset="$COLOR_RESET" \
        -v bold="$COLOR_BOLD" \
        -v green="$COLOR_GREEN" \
        -v yellow="$COLOR_YELLOW" \
        -v blue="$COLOR_BLUE" \
        -v magenta="$COLOR_MAGENTA" \
        -v cyan="$COLOR_CYAN" '
        function print_section(title, section_color)
        {
            printf "\n%s%s%s%s\n", bold, section_color, title, reset
            current_color = section_color
        }

        /^Linux / {
            next
        }

        /^Average:/ {
            next
        }

        /^[[:space:]]*$/ {
            next
        }

        /%usr/ {
            print_section("CPU", green)
            print bold cyan $0 reset
            next
        }

        /minflt\/s/ {
            print_section("MEMORY", magenta)
            print bold cyan $0 reset
            next
        }

        /kB_rd\/s/ {
            print_section("DISK I/O", blue)
            print bold cyan $0 reset
            next
        }

        /cswch\/s/ {
            print_section("CONTEXT SWITCHES", yellow)
            print bold cyan $0 reset
            next
        }

        /threads/ && /fd-nr/ {
            print_section("PROCESS RESOURCES", cyan)
            print bold cyan $0 reset
            next
        }

        {
            print current_color $0 reset
        }
    '
}

trap cleanup EXIT
trap 'exit 0' INT TERM

if ! command -v pidstat >/dev/null 2>&1
then
    printf 'pidstat is required. Install the sysstat package.\n' >&2
    exit 1
fi

printf '\033[?1049h\033[?25l'

while true
do
    server_process_id="$(pgrep -n -x "$PROCESS_NAME" 2>/dev/null)"

    if [ -z "$server_process_id" ]
    then
        printf '\033[2J\033[H'
        printf '%s%sIRC SERVER MONITOR%s\n\n' \
            "$COLOR_BOLD" \
            "$COLOR_CYAN" \
            "$COLOR_RESET"

        printf '%sWaiting for %s...%s\n' \
            "$COLOR_YELLOW" \
            "$PROCESS_NAME" \
            "$COLOR_RESET"

        sleep "$REFRESH_SECONDS"
        continue
    fi

    current_statistics="$(
        LC_ALL=C pidstat \
            -p "$server_process_id" \
            -u \
            -r \
            -d \
            -w \
            -v \
            "$REFRESH_SECONDS" \
            1 \
            2>/dev/null
    )"

    printf '\033[2J\033[H'

    printf '%s%sIRC SERVER MONITOR%s  ' \
        "$COLOR_BOLD" \
        "$COLOR_CYAN" \
        "$COLOR_RESET"

    printf '%sPID:%s %s%s%s  ' \
        "$COLOR_DIM" \
        "$COLOR_RESET" \
        "$COLOR_YELLOW" \
        "$server_process_id" \
        "$COLOR_RESET"

    printf '%sRefresh:%s %ss  ' \
        "$COLOR_DIM" \
        "$COLOR_RESET" \
        "$REFRESH_SECONDS"

    printf '%sCtrl+C to exit%s\n' \
        "$COLOR_DIM" \
        "$COLOR_RESET"

    if [ -n "$current_statistics" ]
    then
        printf '%s\n' "$current_statistics" |
            render_colored_statistics
    else
        printf '\n%sProcess data is no longer available.%s\n' \
            "$COLOR_RED" \
            "$COLOR_RESET"
    fi
done