#!/usr/bin/env bash
# GrokBot-CI-ARM orchestrator — does not modify tracked source; build dirs OK.
set -uo pipefail
source /workspace/env/pico.sh
source /workspace/env/emulators.sh
export PATH="/workspace/tools/bin:$PATH"
unset GCC_ROOT
export CC=/workspace/tools/bin/gcc
export CXX=/workspace/tools/bin/g++

# Preflight: workspace ARM emulators (qemu-system-arm + Renode; not Espressif/simavr)
if ! bash /workspace/tools/ci/check_arm_emulators.sh; then
  echo "ci_arm_cycle: ARM emulator preflight FAILED — aborting matrix" >&2
  exit 2
fi

set +e  # never abort the matrix on a single path failure
# Note: do not re-enable set -e in helpers before "return $rc".
# bash errexit treats `return <nonzero>` as a failing command and aborts the whole matrix.
# The 2026-10-03 .bak re-enabled set -e after BUILD_FAIL; this file must not.


LOG=/workspace/analysis/ci-arm.log
BUILDS=/workspace/builds
mkdir -p "$BUILDS"

ts() { date -Iseconds; }
log() {
  local line
  line="$(ts) $*"
  echo "$line" >>"$LOG"
  # Command substitutions (host_line=$(...)) must not capture log lines.
  if [[ "${RR_LOG_STDERR:-}" == 1 ]]; then
    echo "$line" >&2
  else
    echo "$line"
  fi
}

# Interpret emulator exit: 0 or timeout(124) => PASS smoke; else FAIL
emu_verdict() {
  local rc=$1
  if [[ "$rc" -eq 0 || "$rc" -eq 124 ]]; then echo PASS; else echo FAIL; fi
}

find_elf() {
  local root="$1"
  # Prefer named firmware ELFs
  local cand
  for cand in \
    "$root"/build/firmware/*/lcc_node.elf \
    "$root"/build/*/lcc_node.elf \
    "$root"/build/lcc_node.elf \
    "$root"/build/main.elf \
    "$root"/Pico-ResTouch-LCD-X_X_Code/C/build/main.elf \
    "$root"/build/**/*.elf
  do
    [[ -f "$cand" ]] && { echo "$cand"; return 0; }
  done
  # Fallback: newest non-host .elf under build/
  find "$root" -path '*/build/*' -name '*.elf' ! -path '*/host/*' ! -path '*/host-*/*' 2>/dev/null \
    | head -1
}

clone_fresh() {
  local owner_repo="$1" dest="$2" branch="$3" tag="${4:-}"
  cd /workspace || true
  mkdir -p "$(dirname "$dest")"
  rm -rf "$dest"
  log "CLONE $owner_repo -> $dest (branch=$branch tag=${tag:-none})"
  if ! gh repo clone "$owner_repo" "$dest" -- --branch "$branch" >>"$LOG" 2>&1; then
    if ! git clone --branch "$branch" "https://github.com/${owner_repo}.git" "$dest" >>"$LOG" 2>&1; then
      log "CLONE_FAIL $owner_repo branch=$branch"
      return 1
    fi
  fi
  (
    cd "$dest" || exit 1
    git fetch --tags --force >>"$LOG" 2>&1 || true
    if [[ -n "$tag" ]]; then
      log "CHECKOUT tag $tag"
      git checkout "$tag" >>"$LOG" 2>&1 || { log "CHECKOUT_FAIL tag=$tag"; exit 1; }
    fi
    log "HEAD=$(git rev-parse --short HEAD) describe=$(git describe --tags --always 2>/dev/null || true)"
  ) || return 1
  return 0
}

run_host_tests() {
  local root="$1"
  local host_status=SKIPPED host_rc=0 host_reason="no host test scripts found"
  if [[ -x "$root/scripts/build_host.sh" || -f "$root/scripts/build_host.sh" ]]; then
    log "HOST build_host.sh"
    set +e
    ( cd "$root" && bash scripts/build_host.sh ) >>"$LOG" 2>&1
    host_rc=$?
    if [[ $host_rc -ne 0 ]]; then
      echo "FAIL|$host_rc|build_host.sh failed"
      return 0
    fi
    if [[ -f "$root/scripts/run_tests.sh" ]]; then
      log "HOST run_tests.sh"
      set +e
      ( cd "$root" && bash scripts/run_tests.sh ) >>"$LOG" 2>&1
      host_rc=$?
      if [[ $host_rc -eq 0 ]]; then echo "PASS|0|run_tests.sh"; else echo "FAIL|$host_rc|run_tests.sh failed"; fi
      return 0
    fi
    echo "PASS|$host_rc|build_host only (no run_tests.sh)"
    return 0
  fi
  # Waveshare tree: scripts under Pico-ResTouch-LCD-X_X_Code/C
  local cdir="$root/Pico-ResTouch-LCD-X_X_Code/C"
  if [[ -f "$cdir/scripts/build_host.sh" ]]; then
    log "HOST Waveshare build_host.sh"
    set +e
    ( cd "$cdir" && bash scripts/build_host.sh ) >>"$LOG" 2>&1
    host_rc=$?
    if [[ $host_rc -ne 0 ]]; then echo "FAIL|$host_rc|Waveshare build_host failed"; return 0; fi
    if [[ -f "$cdir/scripts/run_tests.sh" ]]; then
      set +e
      ( cd "$cdir" && bash scripts/run_tests.sh ) >>"$LOG" 2>&1
      host_rc=$?
      if [[ $host_rc -eq 0 ]]; then echo "PASS|0|Waveshare run_tests"; else echo "FAIL|$host_rc|Waveshare run_tests"; fi
      return 0
    fi
    echo "PASS|$host_rc|Waveshare build_host only"
    return 0
  fi
  # ctest / pytest / make test
  if [[ -d "$root/build" ]] && command -v ctest >/dev/null; then
    if find "$root/build" -name CTestTestfile.cmake 2>/dev/null | grep -q .; then
      set +e
      ( cd "$root/build" && ctest --output-on-failure ) >>"$LOG" 2>&1
      host_rc=$?
      if [[ $host_rc -eq 0 ]]; then echo "PASS|0|ctest"; else echo "FAIL|$host_rc|ctest"; fi
      return 0
    fi
  fi
  echo "SKIPPED|0|$host_reason"
}

install_ci_hub_stub() {
  local root="$1"
  if [[ -f "$root/local/hub_host.h" ]]; then
    return 0
  fi
  if ! grep -Rqs '#include "hub_host.h"' "$root/src" 2>/dev/null; then
    return 0
  fi
  mkdir -p "$root/local"
  cp /workspace/tools/ci/stubs/hub_host.h "$root/local/hub_host.h"
  log "CI_STUB installed local/hub_host.h dummy RR_HUB_HOST 192.168.1.1 (gitignored; not a source edit)"
  return 0
}

# True when scripts/build_firmware.py reads PICO_BOARD and LCC_TRANSPORT from the environment.
script_honors_board_env() {
  local py="$1/scripts/build_firmware.py"
  [[ -f "$py" ]] || return 1
  grep -q 'os.environ.get("PICO_BOARD"' "$py" || return 1
  grep -q 'os.environ.get("LCC_TRANSPORT"' "$py" || return 1
  return 0
}

run_firmware_build() {
  local root="$1" preferred_board="${2:-}" transport="${3:-}" lcd_hint="${4:-}"
  install_ci_hub_stub "$root"
  # Prefer project scripts when they honor board_hint / LCC_TRANSPORT.
  # Human-tip scripts that hardcode WIFI and ignore argv fall through to cmake
  # so path 1c (tag v1.05) and a pico_w hint still build the requested image.
  if [[ -f "$root/scripts/build_firmware.sh" ]] && script_honors_board_env "$root"; then
    log "BUILD scripts/build_firmware.sh board=${preferred_board:-script-default} transport=${transport:-script-default} lcd=${lcd_hint:-script-default}"
    set +e
    (
      cd "$root" || exit 1
      if [[ -n "$preferred_board" ]]; then export PICO_BOARD="$preferred_board"; fi
      if [[ -n "$transport" ]]; then export LCC_TRANSPORT="$transport"; fi
      if [[ -n "$lcd_hint" ]]; then export LCD_PANEL="$lcd_hint"; fi
      if [[ -n "$preferred_board" ]]; then
        bash scripts/build_firmware.sh "$preferred_board"
      else
        bash scripts/build_firmware.sh
      fi
    ) >>"$LOG" 2>&1
    local rc=$?
    set +e
    return "$rc"
  fi
  if [[ -f "$root/scripts/build_firmware.sh" || -f "$root/CMakeLists.txt" ]] && [[ -n "$preferred_board" || -n "$transport" ]]; then
    local board="${preferred_board:-pico2_w}"
    local trans="${transport:-WIFI}"
    local lcd="${lcd_hint:-NONE}"
    if [[ -z "$lcd_hint" && "$board" == "pico2_w" && -f "$root/scripts/build_firmware.py" ]] && grep -q 'LCD2' "$root/scripts/build_firmware.py"; then
      lcd="LCD2"
    fi
    log "BUILD cmake direct board=$board transport=$trans lcd=$lcd (build_firmware.py ignores board_hint)"
    set +e
    (
      cd "$root" || exit 1
      cmake -S . -B "build/firmware/$board" -G Ninja         -DPICO_BOARD="$board" -DLCC_TRANSPORT="$trans" -DLCD_PANEL="$lcd" -DRR_KEY0_GPIO=15
      cmake --build "build/firmware/$board"
    ) >>"$LOG" 2>&1
    local rc=$?
    set +e
    return "$rc"
  fi
  if [[ -f "$root/scripts/build_firmware.sh" ]]; then
    log "BUILD scripts/build_firmware.sh"
    set +e
    ( cd "$root" && bash scripts/build_firmware.sh ) >>"$LOG" 2>&1
    local rc=$?
    set +e
    return "$rc"
  fi
  local cdir="$root/Pico-ResTouch-LCD-X_X_Code/C"
  if [[ -f "$cdir/CMakeLists.txt" ]]; then
    local board="${preferred_board:-pico_w}"
    log "BUILD Waveshare cmake board=$board"
    set +e
    (
      cd "$cdir"
      cmake -S . -B build -DPICO_BOARD="$board" -DCMAKE_BUILD_TYPE=Release -G Ninja \
        -DPICO_SDK_PATH="$PICO_SDK_PATH"
      cmake --build build
    ) >>"$LOG" 2>&1
    local rc=$?
    set +e
    return "$rc"
  fi
  if [[ -f "$root/CMakeLists.txt" ]]; then
    local board="${preferred_board:-pico2_w}"
    log "BUILD cmake root board=$board"
    set +e
    (
      cd "$root"
      cmake -S . -B build -DPICO_BOARD="$board" -DCMAKE_BUILD_TYPE=Release -G Ninja \
        -DPICO_SDK_PATH="$PICO_SDK_PATH"
      cmake --build build
    ) >>"$LOG" 2>&1
    local rc=$?
    set +e
    return "$rc"
  fi
  log "BUILD_FAIL no CMakeLists/scripts found"
  set +e
  return 2
}

run_one() {
  set +e
  local id="$1" owner_repo="$2" branch="$3" tag="$4" dest="$5" emu_kind="$6" board_hint="${7:-}" transport="${8:-}" lcd_hint="${9:-}"
  log "---- PATH $id START repo=$owner_repo branch=$branch tag=${tag:-none} emu=$emu_kind ----"
  local BUILD=FAIL HOST_TEST=SKIPPED EMU=SKIPPED build_rc=99 host_rc=0 emu_rc=0 host_ex= host_note= emu_note=
  if ! clone_fresh "$owner_repo" "$dest" "$branch" "$tag"; then
    log "RESULT $id REPO=$owner_repo BRANCH/TAG=${tag:-$branch} BUILD=FAIL HOST_TEST=SKIPPED EMU=SKIPPED exit=clone_fail"
    RESULTS+=("$id|FAIL|SKIPPED|SKIPPED|clone_fail")
    return 0
  fi
  set +e
  run_firmware_build "$dest" "$board_hint" "$transport" "$lcd_hint"
  build_rc=$?
  if [[ $build_rc -eq 0 ]]; then BUILD=PASS; else BUILD=FAIL; fi
  log "BUILD_$BUILD rc=$build_rc"

  local host_line
  host_line=$(RR_LOG_STDERR=1 run_host_tests "$dest")
  HOST_TEST=${host_line%%|*}
  host_rc=$(echo "$host_line" | cut -d'|' -f2)
  host_note=$(echo "$host_line" | cut -d'|' -f3-)
  log "HOST_TEST=$HOST_TEST rc=$host_rc note=$host_note"

  if [[ "$BUILD" == PASS ]]; then
    local elf
    elf=$(find_elf "$dest" || true)
    if [[ -z "${elf:-}" ]]; then
      EMU=SKIPPED; emu_note="no ELF found after build"; emu_rc=2
      log "EMU_SKIPPED $emu_note"
    else
      log "EMU using ELF=$elf kind=$emu_kind"
      local emulog="$dest/emu-smoke.log"
      set +e
      if [[ "$emu_kind" == "qemu-m33" ]]; then
        bash /workspace/tools/ci/run_qemu_m33.sh "$elf" 90 "$emulog"
        emu_rc=$?
      else
        bash /workspace/tools/ci/run_renode_rp2040.sh "$elf" 90 "$emulog"
        emu_rc=$?
      fi
      EMU=$(emu_verdict "$emu_rc")
      emu_note="exit=$emu_rc"
      # Append short excerpt
      if [[ -f "$emulog.qemu.out" ]]; then tail -20 "$emulog.qemu.out" >>"$LOG"; fi
      if [[ -f "$emulog.renode.out" ]]; then tail -30 "$emulog.renode.out" >>"$LOG"; fi
      log "EMU=$EMU $emu_note"
    fi
  else
    EMU=SKIPPED; emu_note="build failed"; emu_rc=0
    log "EMU_SKIPPED build failed"
  fi

  # Short error excerpt on failures
  local excerpt=""
  if [[ "$BUILD" != PASS || "$HOST_TEST" == FAIL || "$EMU" == FAIL ]]; then
    excerpt=$(tail -n 15 "$LOG" | tr '\n' ' ' | cut -c1-400)
  fi
  log "RESULT $id REPO=$owner_repo BRANCH/TAG=${tag:-$branch} BUILD=$BUILD HOST_TEST=$HOST_TEST EMU=$EMU build_rc=$build_rc host_rc=$host_rc emu_rc=$emu_rc excerpt=${excerpt:-none}"
  RESULTS+=("$id|$BUILD|$HOST_TEST|$EMU|build=$build_rc host=$host_rc emu=$emu_rc")
}

RESULTS=()

# Matrix notes:
# - Tag v1.05 names the RP2350-CAN release (on RP2350-Transmit-Test), not pico-w-restouch-3.5.
#   Path 1b therefore pins branch pico-w-restouch-3.5 tip (describe ~v1.00-6); path 1c pins tag v1.05.
log "NOTE: 1b uses branch pico-w-restouch-3.5 tip (tag v1.05 is RP2350-CAN/Transmit-Test, not on restouch)"
log "NOTE: 1c pins tag v1.05 (RP2350-CAN release); rp2350-can tip is v1.04-1 ahead/sibling"

run_one "1a" "wlcs15/RR-Railcom-Pico2W-LCC-Waveshare-LCD2in-CAN" "main" "v0.05" \
  "$BUILDS/RR-Railcom-Pico2W-main-v0.05" "qemu-m33" "pico2_w" "WIFI" "NONE"

run_one "1b" "wlcs15/RR-Railcom-Pico2W-LCC-Waveshare-LCD2in-CAN" "pico-w-restouch-3.5" "" \
  "$BUILDS/RR-Railcom-Pico2W-pico-w-restouch-3.5-v1.05" "renode-rp2040" "pico_w" "WIFI" "NONE"

run_one "1c" "wlcs15/RR-Railcom-Pico2W-LCC-Waveshare-LCD2in-CAN" "rp2350-can" "v1.05" \
  "$BUILDS/RR-Railcom-Pico2W-rp2350-can-v1.05" "qemu-m33" "pico2" "CAN" "NONE"

# Path 1d: human branch pico2-w-display-2.0inch tip (Pico 2 W / LCD2 work). Do not push here.
log "NOTE: 1d uses branch pico2-w-display-2.0inch tip (baseline f75c902 when added; pico2_w; weekly emu Renode per A5.08)"
run_one "1d" "wlcs15/RR-Railcom-Pico2W-LCC-Waveshare-LCD2in-CAN" "pico2-w-display-2.0inch" "" \
  "$BUILDS/RR-Railcom-Pico2W-pico2-w-display-2.0inch" "renode-rp2040" "pico2_w" "WIFI" "LCD2"

run_one "2" "wlcs15/Waveshare_3.5_inch_Touch_Display_Module-Rasp_Pi_Pico" "main" "" \
  "$BUILDS/Waveshare_3.5_Touch-main" "renode-rp2040" "pico_w"

run_one "3" "wlcs15/RR-Railcom-Rasp_Pi_Pico_2w-FreeRTOS-Display" "main" "" \
  "$BUILDS/RR-Railcom-FreeRTOS-Display-main" "qemu-m33" "pico2_w"

log "==== SUMMARY ===="
for r in "${RESULTS[@]}"; do
  log "SUMMARY_ROW $r"
done
log "==== END SUMMARY ===="
log "RUN COMPLETE"
