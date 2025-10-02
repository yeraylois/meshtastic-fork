#!/bin/bash

# /**************************************************************
#  PROJECT : BLACKOUT TRAFFIC LIGHT SYSTEM
#  SCRIPT  : COVERAGE ORCHESTRATOR (INTERACTIVE TEST + COVERAGE)
#  AUTHOR  : YERAY LOIS SÁNCHEZ
#  EMAIL   : yerayloissanchez@gmail.com
# **************************************************************

#  OVERVIEW
#  ==============================================================
#  INTERACTIVE MENU TO:
#    • SELECT MODULES (CHECKBOX-STYLE).
#    • RESOLVE A PLATFORMIO TEST ENV PER MODULE.
#    • RUN `PIO TEST` WITH LLVM/CLANG COVERAGE INSTRUMENTATION.
#    • MERGE .PROFRAW → .PROFDATA AND GENERATE HTML COVERAGE
#      REPORTS WITH `XCRUN LLVM-COV SHOW` (macOS).
#
#  STRICT TEST-DIR DISCOVERY
#  ==============================================================
#    • USE HINT IF PROVIDED AND EXISTS.
#    • OTHERWISE USE STRICT NAME:  test_<ALIAS_SNAKE>.
#    • IF NONE EXISTS, IT SHOWS "(MISSING)" AND THE MODULE IS
#      SKIPPED AT RUN TIME. NO FUZZY MATCHING.
#
#  AUTO-SELECTION OF PLATFORMIO ENVS (NO SUBMENU)
#  ==============================================================
#    • A MODULE IS PRE-SELECTED ONLY IF ITS **MANDATORY ENV**
#      EXISTS IN platformio.ini. THIS AVOIDS WRONG PRESELECTIONS.
#
#    • MANDATORY NAMING CONVENTION (REQUIRED):
#        [env:test_<ALIAS_SNAKE>_native_cov]
#      WHERE <ALIAS_SNAKE> IS THE MODULE ALIAS IN SNAKE_CASE.
#
#      EXAMPLES:
#        ALIAS : T114OptoFlagBridgeModule
#        SNAKE : t114_opto_flag_bridge_module
#        ENV   : test_t114_opto_flag_bridge_module_native_cov
#
#        ALIAS : PowerBudgetModule
#        SNAKE : power_budget_module
#        ENV   : test_power_budget_module_native_cov
#
#    • IF YOUR PROJECT DOESN’T AUTO-SELECT AND STILL OPENS THE
#      ENV PICKER SUBMENU, DO ONE OF THE FOLLOWING:
#        (1) RENAME YOUR platformio.ini SECTIONS TO MATCH:
#              [env:test_<alias_snake>_native_cov]
#            (PREFERRED, ZERO-CODE CHANGE).
#
#        (2) OR UPDATE THE SCRIPT SO IT RETURNS/DETECTS YOUR NAMES:
#              - FUNCTION `MANDATORY_ENV_FOR_ALIAS()` MUST RETURN
#                YOUR EXACT ENV STRING.
#              - FUNCTION `EXISTS_ENV_IN_INI()` MUST GREP THAT NAME.
#
#  HOW TESTS + COVERAGE RUN
#  ==============================================================
#    • PER SELECTED MODULE:
#        LLVM_PROFILE_FILE="<build>/<env>/%p-<testdir>.profraw"
#        pio test -e <env> -f <testdir>
#        COPY "<build>/<env>/program" → "program_<testdir>"
#    • PER USED ENV:
#        xcrun llvm-profdata merge -sparse *.profraw → .profdata
#        xcrun llvm-cov report   (-object program_*)
#        xcrun llvm-cov show     (-object program_* → HTML)
#        OUTPUT: .pio/build/<env>/coverage_html/index.html
#
#  MENU KEYS
#  ==============================================================
#    • DIGITS (E.G., "1 3 8") : TOGGLE SELECTION
#    • A / N                  : SELECT ALL / NONE
#    • R                      : RUN TESTS + COVERAGE
#    • H OR ?                 : HELP / EXAMPLES
#    • B                      : BACK (IN SUBMENUS, CANCELS & RETURNS)
#    • Q                      : QUIT
#
#  macOS DETAILS
#  ==============================================================
#    • READS FROM /dev/tty (RELIABLE FROM IDEs/VSCode).
#    • USES `xcrun llvm-*` FROM XCODE COMMAND LINE TOOLS.
#    • TRIES TO EXPORT PROFILE RUNTIME:
#        ${CLANG_RESOURCE_DIR}/lib/darwin/libclang_rt.profile_osx.a
#
#  REQUIREMENTS
#  ==============================================================
#    • PLATFORMIO (`pio`) IN PATH.
#    • XCODE COMMAND LINE TOOLS (`xcrun`, `llvm-profdata`, `llvm-cov`).
#    • COVERAGE ENVS MUST ENABLE:
#        -fprofile-instr-generate
#        -fcoverage-mapping
#      AND LINK AGAINST libclang_rt.profile_osx.a WHEN NEEDED.
# **************************************************************/

# ==============================================================
# STYLE 
# ==============================================================
if command -v tput >/dev/null 2>&1 && [ -t 1 ]; then
  BOLD="$(tput bold 2>/dev/null || true)"
  NORM="$(tput sgr0 2>/dev/null || true)"
else
  BOLD=""
  NORM=""
fi

# ==============================================================
# UI WIDTH + BOX HELPERS
# ==============================================================
CALC_UI_WIDTH() {
  local cols_arg="$1"
  local cols_detected

  if [[ -n "$cols_arg" ]]; then
    UI_WIDTH="$cols_arg"
  else
    cols_detected="$(tput cols 2>/dev/null || echo 122)"
    UI_WIDTH=$(( cols_detected - 2 ))
  fi

  if (( UI_WIDTH < 60 )); then
    UI_WIDTH=60
  fi
}

BOX_TOP() {
  local w="${1:-$UI_WIDTH}"

  printf "┌"
  printf '─%.0s' $(seq 1 "$w")
  printf "┐\n"
}

BOX_BOTTOM() {
  local w="${1:-$UI_WIDTH}"

  printf "└"
  printf '─%.0s' $(seq 1 "$w")
  printf "┘\n"
}

VISIBLE_LEN() {
  local text="$1"
  local stripped

  stripped="$(printf '%s' "$text" | sed -E $'s/\x1B\\[[0-9;]*[A-Za-z]//g')"
  printf "%d" "${#stripped}"
}

BOX_LINE_CENTER() {
  local w="${1:-$UI_WIDTH}"
  local text="$2"
  local len

  len="$(VISIBLE_LEN "$text")"

  if (( len > w )); then
    text="$(printf '%s' "$text" | cut -c1-"$w")"
    len="$w"
  fi

  local left=$(( (w - len) / 2 ))
  local right=$(( w - len - left ))

  printf "│%*s%s%*s│\n" "$left" "" "$text" "$right" ""
}

BOX_LINE_LEFT() {
  local w="${1:-$UI_WIDTH}"
  local text="$2"
  local len

  len="$(VISIBLE_LEN "$text")"

  if (( len > w )); then
    local toprint
    toprint="$(printf '%s' "$text" | awk -v w="$w" '{print substr($0,1,w)}')"
    text="$toprint"
    len="$w"
  fi

  local pad=$(( w - len ))
  printf "│%s%*s│\n" "$text" "$pad" ""
}

MAKE_BAR() {
  local ch="${1:-=}"
  local edge="${2:-}"
  local w="${3:-$UI_WIDTH}"
  local s
  local mid

  printf -v s '%*s' "$w" ''
  mid="${s// /$ch}"

  case "$edge" in
    "")
      echo "$mid"
      ;;
    "┌")
      echo "┌${mid}┐"
      ;;
    "└")
      echo "└${mid}┘"
      ;;
    *)
      echo "${edge}${mid}${edge}"
      ;;
  esac
}

BOX_BAR() {
  local ch="${1:-=}"
  local mid

  mid="$(MAKE_BAR "$ch")"
  printf "│%s│\n" "$mid"
}

# ==============================================================
# RELIABLE INPUT FROM TTY
# ==============================================================
INPUT_FD=0
if [ ! -t 0 ] && [ -r /dev/tty ]; then
  exec 3</dev/tty
  INPUT_FD=3
fi

# ==============================================================
# PATHS
# ==============================================================
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
TEST_DIR="$REPO_ROOT/test"
BUILD_DIR="$REPO_ROOT/.pio/build"
PLATFORMIO_INI="$REPO_ROOT/platformio.ini"

# ==============================================================
# MODULE CATALOG
# ==============================================================
ALIASES=(
  "Ws3OptoFlagBridgeModule"
  "Ws3OptoPMModule"
  "Ws3Rs485TalkerModule"
  "TrafficRs485CoordinatorModule"
  "TrafficLightMeshModule"
  "T114Rs485SlaveModule"
  "T114OptoPMModule"
  "T114OptoFlagBridgeModule"
  "PowerBudgetModule"
)

TESTDIR_HINTS=(
  ""
  ""
  ""
  ""
  ""
  ""
  ""
  "test_t114_opto_flag_bridge"
  ""
)

# ==============================================================
# HELPERS AND UTILS
# ==============================================================
PAD() {
  printf "%-*s" "$1" "$2"
}

SAFE_CLEAR() {
  printf '\033[2J\033[3J\033[H'
}

ALIAS_TO_SNAKE() {
  local s="$1"

  s="${s//::/_}"
  s="$(printf '%s' "$s" | sed -E 's/([A-Z]+)([A-Z][a-z])/\1_\2/g')"
  s="$(printf '%s' "$s" | sed -E 's/([a-z0-9])([A-Z])/\1_\2/g')"
  s="$(printf '%s' "$s" | tr '[:upper:]' '[:lower:]')"
  s="${s//-/_}"

  echo "$s"
}

MANDATORY_ENV_FOR_ALIAS() {
  local alias="$1"
  local snake

  snake="$(ALIAS_TO_SNAKE "$alias")"
  printf "test_%s_native_cov" "$snake"
}

KEYWORDS_FOR_ALIAS() {
  local snake
  snake="$(ALIAS_TO_SNAKE "$1")"

  local out=""
  IFS='_' read -r -a parts <<< "$snake"

  local p
  for p in "${parts[@]}"; do
    [[ "$p" == "module" ]] && continue
    out="${out}${p}"$'\n'
  done

  printf "%s" "$out"
}

EXISTS_ENV_IN_INI() {
  local env="$1"

  LC_ALL=C grep -qE "^\[env:${env}\]" "$PLATFORMIO_INI"
}

FIND_ENV_CANDIDATES() {
  local kws=()

  if [[ $# -gt 0 ]]; then
    kws=( "$@" )
  else
    while IFS= read -r kw; do
      [[ -n "$kw" ]] && kws+=( "$kw" )
    done
  fi

  local names
  names="$(
    LC_ALL=C grep -Eo '^\[env:[^]]+\]' "$PLATFORMIO_INI" \
      | sed -E 's/^\[env:([^]]+)\]/\1/' \
      | LC_ALL=C grep -E '_native_cov$' || true
  )"

  local found=()
  while IFS= read -r n; do
    [[ -z "$n" ]] && continue

    local keep=1
    local k
    for k in "${kws[@]}"; do
      if ! echo "$n" | LC_ALL=C grep -qi -- "$k"; then
        keep=0
        break
      fi
    done

    (( keep )) && found+=( "$n" )
  done <<< "$names"

  if [[ ${#found[@]} -eq 0 ]]; then
    while IFS= read -r n; do
      [[ -z "$n" ]] && continue
      local k
      for k in "${kws[@]}"; do
        if echo "$n" | LC_ALL=C grep -qi -- "$k"; then
          found+=( "$n" )
          break
        fi
      done
    done <<< "$names"
  fi

  if [[ ${#found[@]} -gt 0 ]]; then
    printf "%s\n" "${found[@]}" | sort -u
  fi
}

DISCOVER_TEST_DIR() {
  local alias="$1"
  local hint="$2"

  if [[ -n "$hint" && -d "$TEST_DIR/$hint" ]]; then
    echo "$hint"
    return
  fi

  local strict="test_$(ALIAS_TO_SNAKE "$alias")"

  if [[ -d "$TEST_DIR/$strict" ]]; then
    echo "$strict"
    return
  fi

  echo ""
}

# ==============================================================
# macOS LLVM-COV HELPERS
# ==============================================================
LLVM_TOOLS_CHECK() {
  command -v xcrun >/dev/null 2>&1 || {
    echo "XCRUN NOT FOUND"
    return 1
  }

  xcrun --find llvm-profdata >/dev/null 2>&1 || {
    echo "LLVM-PROFDATA NOT FOUND (INSTALL XCODE CLT)"
    return 1
  }

  xcrun --find llvm-cov >/dev/null 2>&1 || {
    echo "LLVM-COV NOT FOUND (INSTALL XCODE CLT)"
    return 1
  }

  return 0
}

ENSURE_PROFILE_RT() {
  if [[ -z "${PROFILE_RT:-}" ]]; then
    local clang_bin
    local res_dir
    local darwin_dir
    local candidate

    clang_bin="$(xcrun --sdk macosx --find clang 2>/dev/null || true)"

    if [[ -n "$clang_bin" ]]; then
      res_dir="$("$clang_bin" -print-resource-dir)"
      darwin_dir="$res_dir/lib/darwin"
      candidate="$darwin_dir/libclang_rt.profile_osx.a"

      [[ -f "$candidate" ]] && export PROFILE_RT="$candidate"
    fi
  fi
}

# ==============================================================
# STATE
# ==============================================================
MOD_TESTDIR=()
MOD_MANDATORY_ENV=()
CHOSEN_ENV=()
SELECTED=()

INIT_CATALOG() {
  MOD_TESTDIR=()
  MOD_MANDATORY_ENV=()
  CHOSEN_ENV=()
  SELECTED=()

  local i
  for ((i=0; i<${#ALIASES[@]}; i++)); do
    local alias="${ALIASES[$i]}"
    local hint="${TESTDIR_HINTS[$i]}"

    local tdir
    tdir="$(DISCOVER_TEST_DIR "$alias" "$hint")"
    MOD_TESTDIR+=( "$tdir" )

    local menv
    menv="$(MANDATORY_ENV_FOR_ALIAS "$alias")"
    MOD_MANDATORY_ENV+=( "$menv" )

    if EXISTS_ENV_IN_INI "$menv"; then
      CHOSEN_ENV+=( "$menv" )
      SELECTED+=( 1 )
    else
      CHOSEN_ENV+=( "" )
      SELECTED+=( 0 )
    fi
  done
}

# ==============================================================
# MAIN MENU (OUTER BOX WRAPS EVERYTHING)
# ==============================================================
PRINT_MAIN_MENU_BOX() {
  CALC_UI_WIDTH 122

  BOX_TOP "$UI_WIDTH"

  BOX_LINE_CENTER "$UI_WIDTH" "COVERAGE ORCHESTRATOR"
  BOX_BAR "─"
  BOX_LINE_LEFT "$UI_WIDTH" ""

  BOX_LINE_CENTER "$UI_WIDTH" "SELECT MODULES (TYPE NUMBERS, 'A' ALL, 'N' NONE, 'R' RUN, 'B' BACK, 'Q' QUIT, 'H' HELP)"
  BOX_BAR "─"

  BOX_LINE_LEFT "$UI_WIDTH" "$(printf "   %-39s %-28s %-35s" "ALIAS" "TEST DIR" "MANDATORY ENV")"
  BOX_BAR "─"

  local i
  for ((i=0; i<${#ALIASES[@]}; i++)); do
    local mark="[ ]"
    [[ "${SELECTED[$i]}" == "1" ]] && mark="[x]"

    local alias="${ALIASES[$i]}"
    local tdir="${MOD_TESTDIR[$i]}"
    [[ -z "$tdir" ]] && tdir="(missing)"

    local menv="${MOD_MANDATORY_ENV[$i]}"

    local row
    row=$(printf " %3d) %s %-32s %-28s %-35s" $((i+1)) "$mark" "$alias" "$tdir" "$menv")

    BOX_LINE_LEFT "$UI_WIDTH" "$row"
  done

  BOX_LINE_LEFT "$UI_WIDTH" ""
  BOX_LINE_CENTER "$UI_WIDTH" "TIP: PRESS 'H' OR '?' FOR HELP."
  BOX_BOTTOM "$UI_WIDTH"
}

TOGGLE_BY_NUMBERS() {
  local nums="$1"
  nums="${nums//,/ }"

  local tok
  for tok in $nums; do
    [[ "$tok" =~ ^[0-9]+$ ]] || continue

    local idx=$(( tok - 1 ))
    (( idx>=0 && idx<${#ALIASES[@]} )) || continue

    if [[ "${SELECTED[$idx]}" == "1" ]]; then
      SELECTED[$idx]=0
    else
      SELECTED[$idx]=1
    fi
  done
}

TRIM_SPACES() {
  local s="$1"

  s="${s#"${s%%[![:space:]]*}"}"
  s="${s%"${s##*[![:space:]]}"}"

  printf "%s" "$s"
}

READ_USER() {
  printf "> " >&2

  local line=""
  IFS= read -r -u "$INPUT_FD" line || line=""

  line="${line%$'\r'}"
  TRIM_SPACES "$line"
}

HELP_EXAMPLES() {
  SAFE_CLEAR
  CALC_UI_WIDTH 80

  BOX_TOP "$UI_WIDTH"
  BOX_LINE_CENTER "$UI_WIDTH" "EXAMPLES"
  BOX_BAR "─"

  BOX_LINE_LEFT "$UI_WIDTH" ""
  BOX_LINE_LEFT "$UI_WIDTH" "  ■ TOGGLE MODULES 1, 3 AND 8:"
  BOX_LINE_LEFT "$UI_WIDTH" "    > 1 3 8"

  BOX_LINE_LEFT "$UI_WIDTH" ""
  BOX_LINE_LEFT "$UI_WIDTH" "  ■ SELECT ALL, THEN RUN:"
  BOX_LINE_LEFT "$UI_WIDTH" "    > A"
  BOX_LINE_LEFT "$UI_WIDTH" "    > R"

  BOX_LINE_LEFT "$UI_WIDTH" ""
  BOX_LINE_LEFT "$UI_WIDTH" "  ■ CLEAR THE SCREEN:"
  BOX_LINE_LEFT "$UI_WIDTH" "    > clear"

  BOX_LINE_LEFT "$UI_WIDTH" ""
  BOX_LINE_LEFT "$UI_WIDTH" "  ■ GO BACK (IN SUBMENUS) OR CANCEL:"
  BOX_LINE_LEFT "$UI_WIDTH" "    > B"

  BOX_LINE_LEFT "$UI_WIDTH" ""
  BOX_BAR "─"
  BOX_LINE_CENTER "$UI_WIDTH" "PRESS ENTER TO RETURN..."
  BOX_BOTTOM "$UI_WIDTH"

  read -r -u "$INPUT_FD" _

  SAFE_CLEAR
}

# ==============================================================
# ENV RESOLUTION PER MODULE
# ==============================================================
ENSURE_ENVS_FOR_SELECTION() {
  local i
  local CANCEL=0

  for ((i=0; i<${#ALIASES[@]}; i++)); do
    [[ "${SELECTED[$i]}" != "1" ]] && continue
    [[ -n "${CHOSEN_ENV[$i]}" ]] && continue

    local alias="${ALIASES[$i]}"
    local kws=()

    while IFS= read -r kw; do
      [[ -n "$kw" ]] && kws+=( "$kw" )
    done <<< "$(KEYWORDS_FOR_ALIAS "$alias")"

    local candidates
    candidates="$(FIND_ENV_CANDIDATES "${kws[@]}")"

    if [[ -z "$candidates" ]]; then
      CALC_UI_WIDTH 122
      BOX_TOP "$UI_WIDTH"
      BOX_LINE_CENTER "$UI_WIDTH" "WARNING: NO ENV FOUND FOR $alias. IT WILL BE SKIPPED."
      BOX_BOTTOM "$UI_WIDTH"
      continue
    fi

    while true; do
      CALC_UI_WIDTH 122
      BOX_TOP "$UI_WIDTH"
      BOX_LINE_CENTER "$UI_WIDTH" "${BOLD}SELECT ENV FOR:${NORM} $alias"

      local n=1
      local arr=()

      while IFS= read -r c; do
        [[ -z "$c" ]] && continue
        arr+=( "$c" )
        BOX_LINE_LEFT "$UI_WIDTH" "$(printf "  %2d) %s" "$n" "$c")"
        n=$(( n + 1 ))
      done <<< "$candidates"

      BOX_LINE_LEFT "$UI_WIDTH" "  B) BACK (CANCEL SELECTION & RETURN)"
      BOX_BOTTOM "$UI_WIDTH"

      local choice
      choice="$(READ_USER)"

      if [[ -z "$choice" || "$choice" =~ ^[bB]$ ]]; then
        CANCEL=1
        break
      fi

      if [[ "$choice" =~ ^[0-9]+$ ]]; then
        local j=$(( choice - 1 ))
        if (( j>=0 && j<${#arr[@]} )); then
          CHOSEN_ENV[$i]="${arr[$j]}"
          break
        fi
      fi

      CALC_UI_WIDTH 122
      BOX_TOP "$UI_WIDTH"
      BOX_LINE_CENTER "$UI_WIDTH" "INVALID CHOICE."
      BOX_BOTTOM "$UI_WIDTH"
    done

    (( CANCEL )) && break
  done

  SAFE_CLEAR

  if (( CANCEL )); then
    return 2
  fi

  local any=0
  for ((i=0; i<${#ALIASES[@]}; i++)); do
    if [[ "${SELECTED[$i]}" == "1" && -n "${CHOSEN_ENV[$i]}" ]]; then
      any=1
      break
    fi
  done

  if (( ! any )); then
    CALC_UI_WIDTH 122
    BOX_TOP "$UI_WIDTH"
    BOX_LINE_CENTER "$UI_WIDTH" "ERROR: NO ENVIRONMENTS SELECTED."
    BOX_BOTTOM "$UI_WIDTH"
    return 1
  fi

  return 0
}

# ==============================================================
# EXECUTION & COVERAGE
# ==============================================================
TOOL_EXISTS() {
  command -v "$1" >/dev/null 2>&1
}

RUN_COVERAGE() {
  CALC_UI_WIDTH 122
  BOX_TOP "$UI_WIDTH"
  BOX_LINE_CENTER "$UI_WIDTH" "RUNNING TESTS WITH COVERAGE..."
  BOX_BOTTOM "$UI_WIDTH"

  if ! TOOL_EXISTS pio; then
    BOX_TOP "$UI_WIDTH"
    BOX_LINE_CENTER "$UI_WIDTH" "ERROR: PLATFORMIO (PIO) NOT FOUND IN PATH."
    BOX_BOTTOM "$UI_WIDTH"
    return 1
  fi

  if ! LLVM_TOOLS_CHECK; then
    BOX_TOP "$UI_WIDTH"
    BOX_LINE_CENTER "$UI_WIDTH" "ERROR: LLVM TOOLS NOT FOUND (INSTALL XCODE CLT)."
    BOX_BOTTOM "$UI_WIDTH"
    return 1
  fi

  ENSURE_PROFILE_RT

  local i
  USED_ENVS_LIST=()

  for ((i=0; i<${#ALIASES[@]}; i++)); do
    [[ "${SELECTED[$i]}" != "1" ]] && continue

    local alias="${ALIASES[$i]}"
    local env="${CHOSEN_ENV[$i]}"
    local tdir="${MOD_TESTDIR[$i]}"

    if [[ -z "$env" ]]; then
      BOX_TOP "$UI_WIDTH"
      BOX_LINE_CENTER "$UI_WIDTH" "SKIPPING $alias: NO ENV SELECTED."
      BOX_BOTTOM "$UI_WIDTH"
      continue
    fi

    if [[ -z "$tdir" || ! -d "$TEST_DIR/$tdir" ]]; then
      BOX_TOP "$UI_WIDTH"
      BOX_LINE_CENTER "$UI_WIDTH" "SKIPPING $alias: TEST DIR NOT FOUND (${tdir:-missing})."
      BOX_BOTTOM "$UI_WIDTH"
      continue
    fi

    USED_ENVS_LIST+=( "$env" )

    local ENV_BUILD_DIR="$BUILD_DIR/$env"
    mkdir -p "$ENV_BUILD_DIR"

    local profpat="$ENV_BUILD_DIR/%p-${tdir}.profraw"

    BOX_TOP "$UI_WIDTH"
    BOX_LINE_CENTER "$UI_WIDTH" "▶ $alias  (TEST: $tdir, ENV: $env)"
    BOX_BOTTOM "$UI_WIDTH"

    LLVM_PROFILE_FILE="$profpat" pio test -e "$env" -f "$tdir" || {
      BOX_TOP "$UI_WIDTH"
      BOX_LINE_CENTER "$UI_WIDTH" "FAILED ENV: $env / TEST: $tdir"
      BOX_BOTTOM "$UI_WIDTH"
    }

    local program_src="$BUILD_DIR/$env/program"
    local program_dst="$ENV_BUILD_DIR/program_${tdir}"

    [[ -f "$program_src" ]] && cp -f "$program_src" "$program_dst"
  done

  UNIQUE_ENVS=()
  tmpenvs="$(mktemp -t usedenvs.XXXXXX)"

  printf "%s\n" "${USED_ENVS_LIST[@]}" | awk 'NF' | sort -u > "$tmpenvs"

  while IFS= read -r e; do
    [[ -n "$e" ]] && UNIQUE_ENVS+=( "$e" )
  done < "$tmpenvs"

  rm -f "$tmpenvs"

  local env
  for env in "${UNIQUE_ENVS[@]}"; do
    local ENV_BUILD_DIR="$BUILD_DIR/$env"
    local HTML_DIR="$ENV_BUILD_DIR/coverage_html"

    mkdir -p "$HTML_DIR"

    shopt -s nullglob
    local raws=( "$ENV_BUILD_DIR"/*.profraw )
    shopt -u nullglob

    if (( ${#raws[@]} == 0 )); then
      BOX_TOP "$UI_WIDTH"
      BOX_LINE_CENTER "$UI_WIDTH" "NO .PROFRAW TO MERGE FOR ENV '$env'. SKIPPING REPORT."
      BOX_BOTTOM "$UI_WIDTH"
      continue
    fi

    local profdata="$ENV_BUILD_DIR/coverage.profdata"

    BOX_TOP "$UI_WIDTH"
    BOX_LINE_CENTER "$UI_WIDTH" "MERGING PROFILES FOR ENV '$env'..."
    BOX_BOTTOM "$UI_WIDTH"

    xcrun llvm-profdata merge -sparse "${raws[@]}" -o "$profdata" || {
      BOX_TOP "$UI_WIDTH"
      BOX_LINE_CENTER "$UI_WIDTH" "LLVM-PROFDATA MERGE FAILED FOR ENV '$env'"
      BOX_BOTTOM "$UI_WIDTH"
      continue
    }

    local objects=()
    while IFS= read -r -d '' bin; do
      objects+=( "-object" "$bin" )
    done < <(find "$ENV_BUILD_DIR" -maxdepth 1 -type f -name 'program_'* -print0)

    if (( ${#objects[@]} == 0 )) && [[ -f "$BUILD_DIR/$env/program" ]]; then
      objects=( "-object" "$BUILD_DIR/$env/program" )
    fi

    BOX_TOP "$UI_WIDTH"
    BOX_LINE_CENTER "$UI_WIDTH" "CONSOLE REPORT FOR ENV '$env':"
    BOX_BOTTOM "$UI_WIDTH"

    xcrun llvm-cov report "${objects[@]}" -instr-profile="$profdata" || true

    BOX_TOP "$UI_WIDTH"
    BOX_LINE_CENTER "$UI_WIDTH" "GENERATING HTML FOR ENV '$env'..."
    BOX_BOTTOM "$UI_WIDTH"

    xcrun llvm-cov show "${objects[@]}" \
      -instr-profile="$profdata" \
      -format=html \
      -output-dir="$HTML_DIR" \
      -show-line-counts-or-regions || {
        BOX_TOP "$UI_WIDTH"
        BOX_LINE_CENTER "$UI_WIDTH" "LLVM-COV SHOW FAILED FOR ENV '$env'"
        BOX_BOTTOM "$UI_WIDTH"
        continue
      }

    BOX_TOP "$UI_WIDTH"
    BOX_LINE_CENTER "$UI_WIDTH" "HTML REPORT AT: $HTML_DIR/index.html"
    BOX_BOTTOM "$UI_WIDTH"

    if [[ "$OSTYPE" == "darwin"* ]]; then
      open "$HTML_DIR/index.html" >/dev/null 2>&1 || true
    fi
  done
}

# ==============================================================
# MAIN LOOP
# ==============================================================
MAIN_MENU() {
  INIT_CATALOG

  while true; do
    SAFE_CLEAR
    PRINT_MAIN_MENU_BOX

    local ans
    ans="$(READ_USER)"

    if [[ "$ans" == "clear" || "$ans" == "cls" ]]; then
      SAFE_CLEAR
      continue
    fi

    if [[ "$ans" =~ ^[hH\?]$ ]]; then
      HELP_EXAMPLES
      continue
    fi

    if [[ "$ans" =~ ^[qQ]$ ]]; then
      echo "BYE!"
      exit 0
    fi

    if [[ "$ans" =~ ^[bB]$ ]]; then
      SAFE_CLEAR
      continue
    fi

    case "$ans" in
      [aA])
        local i
        for i in "${!ALIASES[@]}"; do
          SELECTED[$i]=1
        done
        SAFE_CLEAR
        ;;
      [nN])
        local i
        for i in "${!ALIASES[@]}"; do
          SELECTED[$i]=0
        done
        SAFE_CLEAR
        ;;
      [rR])
        ENSURE_ENVS_FOR_SELECTION
        rc=$?

        if [[ $rc -eq 0 ]]; then
          RUN_COVERAGE

          CALC_UI_WIDTH 122
          BOX_TOP "$UI_WIDTH"
          BOX_LINE_CENTER "$UI_WIDTH" "PRESS ENTER TO RETURN TO MENU..."
          BOX_BOTTOM "$UI_WIDTH"
          read -r -u "$INPUT_FD" _
        elif [[ $rc -ne 2 ]]; then
          CALC_UI_WIDTH 122
          BOX_TOP "$UI_WIDTH"
          BOX_LINE_CENTER "$UI_WIDTH" "PRESS ENTER TO RETURN TO MENU..."
          BOX_BOTTOM "$UI_WIDTH"
          read -r -u "$INPUT_FD" _
        fi

        SAFE_CLEAR
        ;;
      *)
        if [[ "$ans" =~ ^[0-9\ ]+$ ]]; then
          TOGGLE_BY_NUMBERS "$ans"
          SAFE_CLEAR
        fi
        ;;
    esac
  done
}

MAIN_MENU