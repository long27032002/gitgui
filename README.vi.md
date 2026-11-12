# GitGui

[![Bản phát hành mới nhất](https://img.shields.io/github/v/release/long27032002/gitgui?display_name=tag)](https://github.com/long27032002/gitgui/releases/latest)
[![Giấy phép MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

**GitGui là ứng dụng desktop giúp làm việc với repository Git.** Ứng dụng kết
hợp màn hình xem thay đổi và commit với đồ thị lịch sử nhiều nhánh, công cụ quản
lý nhánh, thao tác remote và giải quyết conflict. GitGui được viết bằng C++20,
Qt 6 và QML.

[Tải bản mới nhất](https://github.com/long27032002/gitgui/releases/latest) · [English](README.md)

GitGui là dự án độc lập. Giao diện lấy cảm hứng từ GitHub Desktop; dự án không
liên kết hay được GitHub bảo trợ. GitGui gọi chương trình Git trên máy, nên
remote và xác thực dùng cấu hình Git, SSH agent hoặc credential helper hiện có.

## Tính năng

- **Xem và commit thay đổi:** xem diff của từng file, chọn file và từng dòng,
  nhập nội dung commit, thêm đồng tác giả, amend commit phù hợp hoặc hoàn tác
  commit cuối đủ điều kiện.
- **Xem lịch sử:** xem commit trên đồ thị nhiều nhánh có màu; xem chi tiết commit
  và file thay đổi; cherry-pick, revert hoặc squash một đoạn commit.
- **Quản lý nhánh:** tạo, chuyển, đổi tên và xoá nhánh; merge kiểu merge commit,
  squash hoặc fast-forward; rebase; chọn cách xử lý thay đổi chưa commit khi
  chuyển nhánh.
- **Làm việc với remote:** clone từ URL, fetch, pull, push và force-push có kiểm
  tra lease.
- **Giải quyết conflict:** xem file xung đột, chọn từng phần ngay trong app hoặc
  mở editor bên ngoài, sau đó chủ động bấm hoàn tất merge.
- **Giữ khả năng khôi phục:** thay đổi bị loại bỏ được sao lưu để có thể phục
  hồi. File conflict không tự động được stage.
- **Tuỳ chỉnh giao diện:** đổi tiếng Việt/tiếng Anh và chọn giao diện tối/sáng.

## Tải và cài đặt

Hiện có bản phát hành **x86_64** cho distro dựa trên Arch (bao gồm EndeavourOS),
Debian/Ubuntu và một gói portable cho Windows.

### EndeavourOS và Arch Linux

Tải file `gitgui-*-x86_64.pkg.tar.zst` từ
[bản phát hành mới nhất](https://github.com/long27032002/gitgui/releases/latest),
sau đó cài trong thư mục chứa file vừa tải:

```sh
sudo pacman -U ./gitgui-*-x86_64.pkg.tar.zst
```

Gói sẽ khai báo các phụ thuộc Git và Qt cần thiết.

### Debian và Ubuntu

Tải file `.deb` từ
[bản phát hành mới nhất](https://github.com/long27032002/gitgui/releases/latest),
rồi cài bằng lệnh:

```sh
sudo apt install ./gitgui-*-x86_64.deb
```

Trình quản lý gói sẽ cài các thư viện cần thiết. Nếu `apt` báo thiếu phụ thuộc,
chạy `sudo apt --fix-broken install` rồi thử cài lại.

### Windows

Tải và giải nén `GitGui-windows-x64.zip`, sau đó chạy `gitgui.exe` trong thư
mục vừa giải nén. File ZIP có kèm thư viện Qt. Hãy cài
[Git for Windows](https://git-scm.com/download/win) và bảo đảm lệnh Git có trong
`PATH`; GitGui dùng Git để thao tác với repository. File `gitgui.exe` tải riêng
không kèm thư viện Qt, nên hãy dùng ZIP để chạy app.

## Bắt đầu sử dụng

1. Cài Git. Trên Windows, cấu hình credential helper hoặc SSH agent trước khi
   kết nối remote.
2. Mở GitGui, chọn mở repository có sẵn, clone bằng URL hoặc tạo repository
   mới trên máy.
3. Trong tab **Thay đổi**, xem diff, chọn file và từng dòng muốn đưa vào commit,
   nhập nội dung rồi commit.
4. Trong tab **Lịch sử**, chọn commit để xem chi tiết và thực hiện thao tác phù
   hợp. Đồ thị hiển thị cả nhánh local và remote.
5. Mở **View → Ngôn ngữ** để đổi ngôn ngữ. App lưu lựa chọn ngôn ngữ và giao
   diện trên máy.

### Cách chọn nội dung để commit

GitGui giữ lựa chọn file và dòng trong app. Khi commit, app dựng lại Git index
theo đúng lựa chọn đó. Nếu bạn stage thay đổi bằng công cụ Git khác, hãy kiểm
tra lại lựa chọn trong GitGui trước khi commit; app có thể thay trạng thái index
bằng các thay đổi được chọn trong giao diện của nó. Thay đổi mới mặc định được
chọn.

### An toàn và giới hạn hiện tại

- Force-push dùng `--force-with-lease` và kiểm tra commit upstream trước khi
  xác nhận.
- Amend và hoàn tác commit cuối chỉ áp dụng cho commit chưa push.
- Trước khi bỏ thay đổi, app tạo bản sao lưu; dùng chức năng khôi phục để lấy
  lại.
- Hiện chưa có đăng nhập tài khoản GitHub, quản lý pull request hoặc danh sách
  repository theo tài khoản. Hãy clone repository bằng URL.
- Chưa có màn hình quản lý stash thủ công. Khi chuyển nhánh, app có thể đề nghị
  stash thay đổi đang có.

## Build từ mã nguồn

### Yêu cầu

- CMake 3.21 trở lên
- Trình biên dịch hỗ trợ C++20
- Git 2.28 trở lên
- Qt 6.4 trở lên, gồm Core, Gui, Quick, Quick Controls 2, Concurrent, Test và
  LinguistTools
- Khuyến nghị dùng Ninja với CMake

Cài công cụ build trên EndeavourOS/Arch:

```sh
sudo pacman -S --needed base-devel cmake ninja git qt6-base qt6-declarative qt6-tools
```

Cài công cụ build trên Debian/Ubuntu:

```sh
sudo apt install build-essential cmake ninja-build git qt6-base-dev \
  qt6-declarative-dev qt6-tools-dev qt6-tools-dev-tools qt6-l10n-tools
```

### Biên dịch và chạy

Tại thư mục gốc của repository:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/src/app/gitgui
```

Muốn chạy bộ kiểm thử sau khi build:

```sh
ctest --test-dir build --output-on-failure
```

Có thể bỏ các target kiểm thử khi cấu hình bằng
`-DGITGUI_BUILD_TESTS=OFF`.

## Giấy phép

GitGui được phát hành theo [MIT License](LICENSE). Các phần mềm bên thứ ba, bao
gồm Qt và Git, vẫn tuân theo giấy phép và điều khoản riêng của chúng.
