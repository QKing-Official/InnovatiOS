#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CONFIG_FILE="$ROOT_DIR/.config"
MODE="menu"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --defconfig)
            MODE="defconfig"
            shift
            ;;
        --config)
            CONFIG_FILE="${2:?--config requires a path}"
            shift 2
            ;;
        *)
            CONFIG_FILE="$1"
            shift
            ;;
    esac
done


CATEGORIES=(DRIVERS CORE PROGRAMS SHELL ADVANCED)

declare -A CATEGORY_LABEL=(
    [DRIVERS]="Device Drivers"
    [CORE]="Kernel Core Subsystems"
    [PROGRAMS]="Userland Programs"
    [SHELL]="Userspace Shell"
    [ADVANCED]="Advanced Options"
)

declare -A CATEGORY_FEATURES=(
    [DRIVERS]="CONFIG_SERIAL CONFIG_FRAMEBUFFER CONFIG_PIT CONFIG_KEYBOARD"
    [CORE]="CONFIG_GDT CONFIG_INTERRUPTS CONFIG_PMM CONFIG_VMM CONFIG_HEAP CONFIG_SCHEDULER CONFIG_ATA CONFIG_VFS CONFIG_INNOFS"
    [PROGRAMS]="CONFIG_PROG_HELP CONFIG_PROG_WHOAMI CONFIG_PROG_USERS CONFIG_PROG_ADDUSER CONFIG_PROG_UNAME CONFIG_PROG_UPTIME CONFIG_PROG_MEMINFO CONFIG_PROG_REBOOT"
    [SHELL]="CONFIG_SHELL CONFIG_SHELL_HISTORY CONFIG_SHELL_ECHO CONFIG_SHELL_COLORTEST CONFIG_SHELL_SUGGEST CONFIG_SHELL_SETTINGS CONFIG_SHELL_ALIAS CONFIG_SHELL_ENV CONFIG_SHELL_PREFIX_MATCH CONFIG_SHELL_THEME CONFIG_SHELL_CONFIRM_DESTRUCTIVE CONFIG_SHELL_SYSINFO CONFIG_SHELL_CALC CONFIG_SHELL_REPEAT CONFIG_SHELL_MOTD"
    [ADVANCED]="CONFIG_USER_ADDED_PROGRAMS"
)

FEATURES=()
for _cat in "${CATEGORIES[@]}"; do
    for _feat in ${CATEGORY_FEATURES[$_cat]}; do
        FEATURES+=("$_feat")
    done
done
unset _cat _feat

feature_label() {
    case "$1" in
        CONFIG_SERIAL)       echo "Serial logging" ;;
        CONFIG_FRAMEBUFFER)  echo "Framebuffer driver" ;;
        CONFIG_PIT)          echo "Programmable timer" ;;
        CONFIG_KEYBOARD)     echo "Keyboard input" ;;
        CONFIG_GDT)          echo "GDT setup" ;;
        CONFIG_INTERRUPTS)   echo "Interrupt subsystem" ;;
        CONFIG_PMM)          echo "Physical memory manager" ;;
        CONFIG_VMM)          echo "Virtual memory manager" ;;
        CONFIG_HEAP)         echo "Kernel heap" ;;
        CONFIG_SCHEDULER)    echo "Process scheduler" ;;
        CONFIG_ATA)          echo "ATA PIO Storage Driver" ;;
        CONFIG_VFS)          echo "Virtual File System (VFS)" ;;
        CONFIG_INNOFS)       echo "InnoFS Persistent Filesystem" ;;
        CONFIG_PROG_HELP)    echo "[prog] help" ;;
        CONFIG_PROG_WHOAMI)  echo "[prog] whoami" ;;
        CONFIG_PROG_USERS)   echo "[prog] users" ;;
        CONFIG_PROG_ADDUSER) echo "[prog] adduser" ;;
        CONFIG_PROG_UNAME)   echo "[prog] uname" ;;
        CONFIG_PROG_UPTIME)  echo "[prog] uptime" ;;
        CONFIG_PROG_MEMINFO) echo "[prog] meminfo" ;;
        CONFIG_PROG_REBOOT)  echo "[prog] reboot" ;;
        CONFIG_SHELL)                    echo "Enable userspace shell" ;;
        CONFIG_SHELL_HISTORY)            echo "[shell] command history" ;;
        CONFIG_SHELL_ECHO)               echo "[shell] echo command" ;;
        CONFIG_SHELL_COLORTEST)          echo "[shell] colors command" ;;
        CONFIG_SHELL_SUGGEST)            echo "[shell] typo suggestions" ;;
        CONFIG_SHELL_SETTINGS)           echo "[shell] set command" ;;
        CONFIG_SHELL_ALIAS)              echo "[shell] aliases and macros" ;;
        CONFIG_SHELL_ENV)                echo "[shell] environment variables" ;;
        CONFIG_SHELL_PREFIX_MATCH)       echo "[shell] unambiguous prefix shortcuts" ;;
        CONFIG_SHELL_THEME)              echo "[shell] theme command" ;;
        CONFIG_SHELL_CONFIRM_DESTRUCTIVE) echo "[shell] confirm before reboot" ;;
        CONFIG_SHELL_SYSINFO)            echo "[shell] sysinfo command" ;;
        CONFIG_SHELL_CALC)               echo "[shell] calc command" ;;
        CONFIG_SHELL_REPEAT)             echo "[shell] repeat command" ;;
        CONFIG_SHELL_MOTD)               echo "[shell] motd command" ;;
        CONFIG_USER_ADDED_PROGRAMS) echo "User added programs" ;;
        *) echo "$1" ;;
    esac
}

feature_help() {
    case "$1" in
        CONFIG_SERIAL)       echo "Send kernel log output over the serial port (COM1)." ;;
        CONFIG_FRAMEBUFFER)  echo "Draw to a linear framebuffer for graphical output." ;;
        CONFIG_PIT)          echo "Programmable Interval Timer, used for tick counting." ;;
        CONFIG_KEYBOARD)     echo "PS/2 keyboard driver and input queue." ;;
        CONFIG_GDT)          echo "Global Descriptor Table setup for segmentation." ;;
        CONFIG_INTERRUPTS)   echo "IDT, ISRs, IRQ handling and dispatch." ;;
        CONFIG_PMM)          echo "Tracks physical page frames (bitmap/stack allocator)." ;;
        CONFIG_VMM)          echo "Page tables, mapping and address space management." ;;
        CONFIG_HEAP)         echo "Kernel dynamic memory allocator (kmalloc/kfree)." ;;
        CONFIG_SCHEDULER)    echo "Preemptive round-robin process scheduler." ;;
        CONFIG_ATA)          echo "Driver for legacy ATA IDE hard drives." ;;
        CONFIG_VFS)          echo "Virtual File System abstraction layer." ;;
        CONFIG_INNOFS)       echo "Persistent block-based filesystem for InnovatiOS." ;;
        CONFIG_PROG_HELP)    echo "Built-in 'help' shell command." ;;
        CONFIG_PROG_WHOAMI)  echo "Built-in 'whoami' shell command." ;;
        CONFIG_PROG_USERS)   echo "Built-in 'users' shell command." ;;
        CONFIG_PROG_ADDUSER) echo "Built-in 'adduser' shell command." ;;
        CONFIG_PROG_UNAME)   echo "Built-in 'uname' shell command." ;;
        CONFIG_PROG_UPTIME)  echo "Built-in 'uptime' shell command." ;;
        CONFIG_PROG_MEMINFO) echo "Built-in 'meminfo' shell command." ;;
        CONFIG_PROG_REBOOT)  echo "Built-in 'reboot' shell command." ;;
        CONFIG_SHELL)
            echo "Master switch for the interactive userspace shell. If disabled, the login session just prints a message and does nothing else; every option below is ignored." ;;
        CONFIG_SHELL_HISTORY)
            echo "Keeps a ring buffer of typed commands, powers 'history', 'history find', 'history clear' and '!!'." ;;
        CONFIG_SHELL_ECHO)
            echo "Adds the 'echo' command that prints text back to the console." ;;
        CONFIG_SHELL_COLORTEST)
            echo "Adds the 'colors' command that lists the console colors this shell can use." ;;
        CONFIG_SHELL_SUGGEST)
            echo "When a command isn't found, suggests the closest known command by edit distance." ;;
        CONFIG_SHELL_SETTINGS)
            echo "Adds the 'set' command for toggling the counter, suggestions and prompt style." ;;
        CONFIG_SHELL_ALIAS)
            echo "Adds 'alias'/'unalias', including multi-command macros chained with ';'." ;;
        CONFIG_SHELL_ENV)
            echo "Adds 'export'/'unset'/'env' and \$VAR expansion, including built-ins like \$USER." ;;
        CONFIG_SHELL_PREFIX_MATCH)
            echo "Lets you type an unambiguous prefix of a command name to run it, e.g. 'wh' for 'whoami'." ;;
        CONFIG_SHELL_THEME)
            echo "Adds the 'theme' command: built-in and user-saved prompt color/symbol themes." ;;
        CONFIG_SHELL_CONFIRM_DESTRUCTIVE)
            echo "Makes 'reboot' ask for a yes/no confirmation before calling prog_reboot()." ;;
        CONFIG_SHELL_SYSINFO)
            echo "Adds 'sysinfo', which runs uname/uptime/meminfo together in one command." ;;
        CONFIG_SHELL_CALC)
            echo "Adds a small integer calculator: 'calc <num> <+|-|*|/> <num>'." ;;
        CONFIG_SHELL_REPEAT)
            echo "Adds 'repeat <count> <command>' to run a command several times in a row." ;;
        CONFIG_SHELL_MOTD)
            echo "Adds 'motd'/'motd set'/'motd reset' to pin a custom login tip message." ;;
        CONFIG_USER_ADDED_PROGRAMS) echo "Enable hooks for user-supplied third-party programs." ;;
        *) echo "No description available." ;;
    esac
}


load_defaults() {
    declare -gA STATE=()
    local feature
    for feature in "${FEATURES[@]}"; do
        STATE["$feature"]=y
    done
    STATE["CONFIG_USER_ADDED_PROGRAMS"]=n

    if [[ "$MODE" != "defconfig" ]] && [[ -f "$CONFIG_FILE" ]]; then
        while IFS='=' read -r key value; do
            [[ "$key" == CONFIG_* ]] || continue
            value="${value//$'\r'/}"
            value="${value//\"/}"
            if [[ "$value" == y ]]; then
                STATE["$key"]=y
            else
                STATE["$key"]=n
            fi
        done < "$CONFIG_FILE"
    fi
}

write_config() {
    mkdir -p "$(dirname "$CONFIG_FILE")"
    {
        echo "# InnovatiOS configuration"
        echo "# Generated by make config"
        local feature
        for feature in "${FEATURES[@]}"; do
            echo "$feature=${STATE[$feature]}"
        done
    } > "$CONFIG_FILE"
}

load_defaults

if [[ "$MODE" == "defconfig" ]]; then
    write_config
    printf 'Wrote default config to %s\n' "$CONFIG_FILE"
    exit 0
fi


cleanup() {
    echo -ne "\e[0m\e[?25h\e[?1049l"
}
trap cleanup EXIT SIGINT SIGTERM

echo -ne "\e[?1049h\e[?25l"

get_dimensions() {
    lines=$(tput lines 2>/dev/null || echo 24)
    cols=$(tput cols 2>/dev/null || echo 80)
    [[ "$lines" =~ ^[0-9]+$ ]] || lines=24
    [[ "$cols" =~ ^[0-9]+$ ]] || cols=80
}

draw_background() {
    echo -ne "\e[40;37m\e[2J\e[H"
    if [[ -z "${_BG_DRAWN:-}" ]]; then
        local empty
        printf -v empty "%*s" "$cols" ""
        for ((i=1; i<=lines; i++)); do
            echo -ne "\e[${i};1H${empty}"
        done
        _BG_DRAWN=1
    fi
    echo -ne "\e[1;1H\e[1;37mInnovatiOS Configuration"
}

draw_box() {
    local x=$1 y=$2 w=$3 h=$4 title="$5"
    local i
    echo -ne "\e[${y};${x}H\e[36;40m┌"
    for ((i=0; i<w-2; i++)); do echo -ne "─"; done
    echo -ne "┐"
    
    for ((i=1; i<h-1; i++)); do
        echo -ne "\e[$((y+i));${x}H\e[36;40m│\e[37;40m"
        printf "%-$((w-2))s" ""
        echo -ne "\e[36;40m│"
    done
    
    echo -ne "\e[$((y+h-1));${x}H\e[36;40m└"
    for ((i=0; i<w-2; i++)); do echo -ne "─"; done
    echo -ne "┘\e[0m"
    
    if [[ -n "$title" ]]; then
        local tlen=${#title}
        local tx=$((x + (w - tlen - 2) / 2))
        echo -ne "\e[${y};${tx}H\e[36;40m $title \e[0m"
    fi
}

read_key() {
    local key ext
    IFS= read -rsn1 key
    case "$key" in
        $'\e')
            IFS= read -rsn2 -t 0.1 ext
            if [[ "$ext" == "[A" ]]; then echo "UP"; return; fi
            if [[ "$ext" == "[B" ]]; then echo "DOWN"; return; fi
            if [[ "$ext" == "[C" ]]; then echo "RIGHT"; return; fi
            if [[ "$ext" == "[D" ]]; then echo "LEFT"; return; fi
            echo "ESC"
            ;;
        "") echo "ENTER" ;;
        ' ') echo "SPACE" ;;
        'h'|'H'|'?') echo "HELP" ;;
        's'|'S') echo "SAVE" ;;
        'q'|'Q') echo "QUIT" ;;
        *) echo "$key" ;;
    esac
}

show_help_popup() {
    local title="$1"
    local text="$2"
    
    get_dimensions
    local bw=60
    local bh=10
    if (( bw > cols - 4 )); then bw=$((cols - 4)); fi
    local bx=$(( (cols - bw) / 2 ))
    local by=$(( (lines - bh) / 2 ))
    
    draw_box $bx $by $bw $bh "Help: $title"
    
    echo -ne "\e[$((by+2));$((bx+2))H\e[40;37m"
    printf "%-$((bw-4))s" "${text:0:$((bw-4))}"
    
    echo -ne "\e[$((by+bh-2));$((bx+2))H\e[36;40m  <Press any key to close>  \e[0m"
    read -rsn1
}

confirm_exit() {
    get_dimensions
    local bw=40
    local bh=8
    local bx=$(( (cols - bw) / 2 ))
    local by=$(( (lines - bh) / 2 ))
    
    draw_box $bx $by $bw $bh "Save Configuration"
    echo -ne "\e[$((by+2));$((bx+2))H\e[40;37mSave config before exiting?"
    
    local sel=0
    while true; do
        if (( sel == 0 )); then
            echo -ne "\e[$((by+5));$((bx+8))H\e[46;30m < Yes > \e[40;37m  < No > "
        else
            echo -ne "\e[$((by+5));$((bx+8))H\e[40;37m < Yes > \e[46;30m  < No > "
        fi
        
        local key
        key=$(read_key)
        if [[ "$key" == "LEFT" || "$key" == "RIGHT" ]]; then
            sel=$(( 1 - sel ))
        elif [[ "$key" == "ENTER" ]]; then
            if (( sel == 0 )); then
                write_config
            fi
            exit 0
        elif [[ "$key" == "ESC" ]]; then
            return
        fi
    done
}

show_category_checklist() {
    local cat="$1"
    local -a feats=(${CATEGORY_FEATURES[$cat]})
    local total=${#feats[@]}
    local cursor=0
    local scroll_offset=0
    
    while true; do
        get_dimensions
        draw_background
        
        local bw=$((cols - 10))
        local bh=$((lines - 6))
        if (( bw > 80 )); then bw=80; fi
        local bx=$(( (cols - bw) / 2 ))
        local by=$(( (lines - bh) / 2 ))
        
        draw_box $bx $by $bw $bh "Category: ${CATEGORY_LABEL[$cat]}"
        
        echo -ne "\e[$((by+1));$((bx+2))H\e[36;40m[Space] toggle  [Enter] back  [H] help\e[0m"
        
        local list_start_y=$((by + 3))
        local max_visible=$((bh - 4))
        if (( max_visible < 1 )); then max_visible=1; fi
        
        for (( i=0; i<max_visible; i++ )); do
            local item_idx=$((scroll_offset + i))
            if (( item_idx >= total )); then break; fi
            
            local feature="${feats[$item_idx]}"
            local state="[ ]"
            if [[ "${STATE[$feature]}" == y ]]; then
                state="[*]"
            fi
            
            local label="$(feature_label "$feature")"
            local line_text=" $state $label "
            
            if (( item_idx == cursor )); then
                echo -ne "\e[$((list_start_y + i));$((bx+1))H\e[46;30m"
                printf "%-$((bw-2))s" " $line_text"
                echo -ne "\e[0m"
            else
                echo -ne "\e[$((list_start_y + i));$((bx+1))H\e[40;37m"
                printf "%-$((bw-2))s" " $line_text"
                echo -ne "\e[0m"
            fi
        done
        
        local key
        key=$(read_key)
        case "$key" in
            "UP")
                if (( cursor > 0 )); then cursor=$((cursor - 1)); fi
                if (( cursor < scroll_offset )); then scroll_offset=$cursor; fi
                ;;
            "DOWN")
                if (( cursor < total - 1 )); then cursor=$((cursor + 1)); fi
                if (( cursor >= scroll_offset + max_visible )); then scroll_offset=$((cursor - max_visible + 1)); fi
                ;;
            "SPACE")
                local feature="${feats[$cursor]}"
                if [[ "${STATE[$feature]}" == y ]]; then
                    STATE[$feature]=n
                else
                    STATE[$feature]=y
                fi
                ;;
            "ENTER"|"ESC")
                return 0
                ;;
            "HELP")
                local feature="${feats[$cursor]}"
                show_help_popup "$(feature_label "$feature")" "$(feature_help "$feature")"
                ;;
        esac
    done
}

main_menu() {
    local cursor=0
    local scroll_offset=0
    local -a cats=("${CATEGORIES[@]}" "SAVE" "EXIT")
    local total=${#cats[@]}
    
    while true; do
        get_dimensions
        draw_background
        
        local bw=$((cols - 10))
        local bh=$((lines - 6))
        if (( bw > 80 )); then bw=80; fi
        local bx=$(( (cols - bw) / 2 ))
        local by=$(( (lines - bh) / 2 ))
        
        draw_box $bx $by $bw $bh "Main Menu"
        
        echo -ne "\e[$((by+1));$((bx+2))H\e[36;40m[Enter] select  [Esc] exit  [H] help\e[0m"
        
        local list_start_y=$((by + 3))
        local max_visible=$((bh - 4))
        if (( max_visible < 1 )); then max_visible=1; fi
        
        for (( i=0; i<max_visible; i++ )); do
            local item_idx=$((scroll_offset + i))
            if (( item_idx >= total )); then break; fi
            
            local cat="${cats[$item_idx]}"
            local label=""
            if [[ "$cat" == "SAVE" ]]; then
                label="Save Configuration"
            elif [[ "$cat" == "EXIT" ]]; then
                label="Exit"
            else
                local enabled=$(category_enabled_count "$cat")
                local catfeats=(${CATEGORY_FEATURES[$cat]})
                local count=${#catfeats[@]}
                label="${CATEGORY_LABEL[$cat]} ---> ($enabled/$count)"
            fi
            
            local line_text=" $label "
            if (( item_idx == cursor )); then
                echo -ne "\e[$((list_start_y + i));$((bx+1))H\e[46;30m"
                printf "%-$((bw-2))s" " $line_text"
                echo -ne "\e[0m"
            else
                echo -ne "\e[$((list_start_y + i));$((bx+1))H\e[40;37m"
                printf "%-$((bw-2))s" " $line_text"
                echo -ne "\e[0m"
            fi
        done
        
        local key
        key=$(read_key)
        case "$key" in
            "UP")
                if (( cursor > 0 )); then cursor=$((cursor - 1)); fi
                if (( cursor < scroll_offset )); then scroll_offset=$cursor; fi
                ;;
            "DOWN")
                if (( cursor < total - 1 )); then cursor=$((cursor + 1)); fi
                if (( cursor >= scroll_offset + max_visible )); then scroll_offset=$((cursor - max_visible + 1)); fi
                ;;
            "ENTER"|"RIGHT")
                local cat="${cats[$cursor]}"
                if [[ "$cat" == "SAVE" ]]; then
                    write_config
                    show_help_popup "Saved" "Configuration written to $CONFIG_FILE"
                elif [[ "$cat" == "EXIT" ]]; then
                    confirm_exit
                else
                    show_category_checklist "$cat"
                fi
                ;;
            "ESC"|"QUIT")
                confirm_exit
                ;;
            "HELP")
                local cat="${cats[$cursor]}"
                if [[ "$cat" != "SAVE" && "$cat" != "EXIT" ]]; then
                    show_help_popup "${CATEGORY_LABEL[$cat]}" "Press Enter to configure options in this category."
                fi
                ;;
            "SAVE")
                write_config
                show_help_popup "Saved" "Configuration written to $CONFIG_FILE"
                ;;
        esac
    done
}

category_enabled_count() {
    local cat="$1" feat count=0
    for feat in ${CATEGORY_FEATURES[$cat]}; do
        if [[ "${STATE[$feat]}" == y ]]; then
            count=$((count + 1))
        fi
    done
    echo "$count"
}

main_menu