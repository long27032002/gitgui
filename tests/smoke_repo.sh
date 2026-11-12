#!/bin/sh
# Tạo repo tạm đang ở giữa một merge có conflict (cả khối nội dung lẫn xoá/sửa) và in đường dẫn.
set -e
d=$(mktemp -d)
g() { git -C "$d" -c user.name=t -c user.email=t@t "$@"; }
g init -q -b main
printf 'top\nline\nbottom\n' > "$d/f.txt"
printf 'keep\n' > "$d/d.txt"
g add -A && g commit -qm base
g switch -qc other
printf 'top\nOTHER\nbottom\n' > "$d/f.txt"
g rm -q d.txt
g commit -qam other
g switch -q main
printf 'top\nMAIN\nbottom\n' > "$d/f.txt"
printf 'keep main\n' > "$d/d.txt"
g commit -qam main
g merge other >/dev/null 2>&1 || true
printf 'new\n' > "$d/untracked.txt"
echo "$d"
