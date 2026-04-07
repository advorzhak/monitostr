# shellcheck shell=zsh
# Optional developer aliases for the monitostr repository.
# Source this file from ~/.zshrc:
#   source ~/dev/advorzhak/monitostr/scripts/dev_aliases.zsh

# Resolve the repository root from this file location.
MONITOSTR_REPO_DIR="${${(%):-%N}:A:h:h}"

mroot() {
  cd "$MONITOSTR_REPO_DIR" || return 1
}

mcfg() {
  cmake -S "$MONITOSTR_REPO_DIR" -B "$MONITOSTR_REPO_DIR/build" "$@"
}

mbld() {
  cmake --build "$MONITOSTR_REPO_DIR/build" --parallel "$@"
}

mrun() {
  "$MONITOSTR_REPO_DIR/build/monitostr" "$@"
}

mrelcfg() {
  cmake -S "$MONITOSTR_REPO_DIR" -B "$MONITOSTR_REPO_DIR/build-release" -DCMAKE_BUILD_TYPE=Release "$@"
}

mrelbld() {
  cmake --build "$MONITOSTR_REPO_DIR/build-release" --parallel "$@"
}

mtest() {
  ctest --test-dir "$MONITOSTR_REPO_DIR/build" --output-on-failure "$@"
}

mtesti() {
  ctest --test-dir "$MONITOSTR_REPO_DIR/build" -R integration --output-on-failure "$@"
}

mclean() {
  rm -rf "$MONITOSTR_REPO_DIR/build" "$MONITOSTR_REPO_DIR/build-release"
}

# Run a command with GNU coreutils without changing global PATH.
with_gnu() {
  PATH="/opt/homebrew/opt/coreutils/libexec/gnubin:$PATH" "$@"
}
