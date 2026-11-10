#!/bin/bash
# Draw mona2_keymap.svg from the keymap with keymap-drawer (https://github.com/caksoylar/keymap-drawer).
set -euo pipefail
cd "$(dirname "$0")/.."

keymap -c keymap_drawer.config.yaml parse -c 11 -z config/mona2.keymap >mona2_keymap.yaml
# Use the moNa2 physical layout from the shield
sed -i '' '1s|.*|layout: { dts_layout: boards/shields/mona2/mona2.dtsi }|' mona2_keymap.yaml
keymap -c keymap_drawer.config.yaml draw mona2_keymap.yaml >mona2_keymap.svg
