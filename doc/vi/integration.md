# Tích hợp

> Đây là bản dịch tiếng Việt của tài liệu qpb 1.9.0. Bản tiếng Anh ([../integration.md](../integration.md)) là bản
> chuẩn; khi hai bản khác nhau thì theo bản tiếng Anh.

Cách lấy thư mục qpb, thêm nó vào một dự án CMake, chọn giữa thư viện static và shared, dùng với Qt 5, và cập nhật nó.
Quay lại [mục lục tài liệu](README.md#tài-liệu).

Nội dung: [Lấy thư mục](#lấy-thư-mục) - [CMake](#cmake) - [Static hay shared](#static-hay-shared) -
[Qt 5](#qt-5) - [Nền tảng](#nền-tảng) - [Nâng cấp](#nâng-cấp)

## Lấy thư mục

Thư mục `qpb/` là toàn bộ thư viện: header, mã nguồn, các tệp CMake, bản dịch, tài liệu này. Đặt nó vào dự án của bạn
thành `components/qpb/` (đường dẫn nào cũng được; các ví dụ dùng đường dẫn này):

```text
my-app/
  CMakeLists.txt
  components/
    qpb/          <- this folder
  src/
```

Có ba cách để lấy và cập nhật nó. Cả ba đều cho ra cùng một `components/qpb/` với nội dung của zip bản phát hành, nên
các dòng CMake bên dưới giống nhau cho mỗi cách.

| Cách | Phù hợp với | Được ghim vào một phiên bản bởi |
|---|---|---|
| Zip bản phát hành (mặc định) | Mọi dự án, không cần git | Zip đã được giải nén (`components/qpb/VERSION`) |
| `git subtree` | Dự án git commit luôn thư mục này | Tag trong lệnh `add` / `pull` |
| `git submodule` | Dự án git đã dùng submodule | Commit được ghi cho `components/qpb` |

**Zip bản phát hành.** Mỗi bản phát hành là một zip, `qpb-X.Y.Z.zip`, chứa thư mục `qpb/` và không gì khác. Xóa thư mục
cũ rồi giải nén thư mục mới vào chỗ của nó, lần đầu và mỗi lần cập nhật:

```sh
rm -rf components/qpb && unzip qpb-X.Y.Z.zip -d components     # the zip contains qpb/
```

**git subtree và git submodule.** Cả hai đều làm việc từ một bản clone của repository qpb (`<remote>` bên dưới là đường
dẫn hoặc URL của bản clone đó). Mỗi bản phát hành công bố nhánh `qpb-release`, có lịch sử chỉ gồm thư mục `qpb/` và có
đỉnh luôn là bản phát hành mới nhất, cùng tag `qpb-vX.Y.Z` trên commit của bản phát hành đó. Hãy dùng các tag này: các tag
`vX.Y.Z` thông thường đánh dấu cả repository và không dùng được ở đây, vì subtree và submodule cần component nằm ở gốc.

```sh
# git subtree - the folder is committed in your repository
git subtree add  --prefix components/qpb <remote> qpb-vX.Y.Z --squash     # first time
git subtree pull --prefix components/qpb <remote> qpb-vX.Y.Z --squash     # update

# git submodule - your repository records a commit of qpb-release
git submodule add -b qpb-release <remote> components/qpb                  # first time, then pin:
git -C components/qpb fetch --tags && git -C components/qpb checkout qpb-vX.Y.Z
git add components/qpb && git commit -m "Use qpb X.Y.Z"                   # same two lines to update
```

Với submodule, `git submodule update --remote components/qpb` chuyển sang bản phát hành mới nhất (đỉnh của
`qpb-release`). Bản clone của submodule cũng tải về cả lịch sử phát triển, nhưng chỉ có component được checkout.

Đừng sửa các tệp trong thư mục này. Zip sẽ ghi đè thay đổi cục bộ, subtree báo nó như một xung đột merge, và submodule
đòi phải commit nó bên trong submodule. Thay đổi nên được thực hiện trong repository qpb.

## CMake

```cmake
find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets)
add_subdirectory(components/qpb)
target_link_libraries(my_app PRIVATE qpb::widgets)   # pulls in qpb::core
```

```cpp
#include <qpb/qpb.h>       // everything; needs qpb::widgets
#include <qpb/qpbcore.h>   // qpb::core only: no QtWidgets or QtGui headers
```

Liên kết một target sẽ mang theo thư mục include của nó, yêu cầu C++17 và các thư viện Qt mà nó cần. Không cần thiết
lập gì khác: không có gì để khởi tạo, không có resource để import và không có plug-in để nạp. Các kiểu và editor tự đăng
ký lần đầu tiên chúng được dùng.

| Target | Chứa | Phụ thuộc vào |
|---|---|---|
| `qpb::core` | property, model, kiểu, xác thực, tuần tự hóa, nguồn QObject, filter proxy, bản dịch | Qt Core |
| `qpb::widgets` | tree view, form view, delegate, editor | `qpb::core`, Qt Widgets |

Code chỉ dùng model (một công cụ dòng lệnh, một server, một unit test) liên kết `qpb::core` và không cần thư viện GUI.

**Tùy chọn.** Một tùy chọn cache, đặt trên dòng lệnh hoặc trước `add_subdirectory`:

| Tùy chọn | Mặc định | Ý nghĩa |
|---|---|---|
| `QPB_BUILD_SHARED` | `OFF` | Build qpb thành thư viện shared thay vì static (xem bên dưới). Độc lập với `BUILD_SHARED_LIBS` của riêng bạn |

Thư mục này không có bước install và không có tệp cấu hình package: nó được thiết kế để dùng với `add_subdirectory`.

**Qt nào được dùng.** qpb dùng bản Qt mà dự án của bạn đã tìm thấy. Nếu `Qt6::Widgets` tồn tại khi `add_subdirectory`
chạy thì dùng Qt 6; nếu không, mà `Qt5::Widgets` tồn tại thì dùng Qt 5. Nếu dự án của bạn chưa tìm Qt, qpb tự đi tìm:
Qt 6 trước, rồi Qt 5. qpb kiểm tra phiên bản tối thiểu (6.5 hoặc 5.15) và dừng với một thông báo lỗi nêu tên phiên bản
nó tìm thấy. Để qpb dùng Qt 5 trên máy có cả hai, hãy gọi `find_package(Qt5 5.15
REQUIRED COMPONENTS Widgets)` trước `add_subdirectory`. Việc chỉ cho CMake biết Qt nằm ở đâu (`CMAKE_PREFIX_PATH`) là việc
của bạn, như với mọi dự án Qt.

**Dự án của bạn giữ nguyên.** Thư mục này không thay đổi thiết lập CMake nào của bạn: chuẩn C++, cờ trình biên dịch,
thư mục output và AUTOMOC chỉ được đặt trên các target của riêng qpb. C++17 là mức tối thiểu, nên dự án của bạn có thể
build với C++20. Các cờ cảnh báo dùng để build qpb là private; các header public biên dịch không có cảnh báo dưới `/W4`
và dưới `-Wall -Wextra -Wpedantic`.

**Phiên bản.** Phiên bản của thư mục nằm trong tệp `VERSION`. CMake chạy lại khi tệp đó thay đổi, ví dụ sau khi bạn thay
thế thư mục. Lúc biên dịch, phiên bản có sẵn dưới dạng `QPB_VERSION` và `QPB_VERSION_STR`, lúc chạy là
`qpb::version()`; xem [Tương thích](compatibility.md#kiểm-tra-phiên-bản).

## Static hay shared

**Static** là mặc định. Các thư viện qpb được liên kết vào tệp thực thi của bạn; không có gì để triển khai (deploy). Chúng
được build dưới dạng position independent, nên cũng có thể được liên kết vào các thư viện shared của riêng bạn.

**Shared** (`-DQPB_BUILD_SHARED=ON`) build `qpb::core` và `qpb::widgets` thành các thư viện shared, tên là `qpbcore` và
`qpbwidgets`. Hãy deploy cả hai cạnh tệp thực thi của bạn (hoặc nơi nền tảng của bạn tìm thấy chúng): trên Windows là hai
tệp DLL, trên Linux là các tệp `.so` cùng các liên kết phiên bản của chúng. `qpb::widgets` cần `qpb::core`, nên hãy đưa
kèm cả hai ngay cả khi bạn chỉ liên kết `qpb::widgets`. Hãy build các thư viện và ứng dụng của bạn từ cùng một phiên
bản của thư mục; để bắt sự không khớp lúc chạy, so sánh `qpb::version()` với `QPB_VERSION_STR`:

```cpp
if (std::strcmp(qpb::version(), QPB_VERSION_STR) != 0)
    qWarning("qpb headers and library differ");
```

Tương thích trong 1.x là tương thích mã nguồn, không phải tương thích nhị phân: một thư viện shared của phiên bản khác
không thể thay thế trực tiếp. Hãy build lại ứng dụng của bạn sau mỗi lần cập nhật.

## Qt 5

Từ 1.7 thư mục này cũng build được với Qt 5.15. Các dòng CMake giống nhau, chỉ khác là tìm Qt 5 trước:

```cmake
find_package(Qt5 5.15 REQUIRED COMPONENTS Widgets)
add_subdirectory(components/qpb)
target_link_libraries(my_app PRIVATE qpb::widgets)
```

Mọi thứ trong tài liệu này đều áp dụng cho cả hai phiên bản chính của Qt, trừ hai khác biệt về API mà Qt 5 không có thứ
tương đương; chúng được liệt kê trong [Tương thích](compatibility.md#qt-5-và-qt-6). Hãy dùng một phiên bản chính của Qt
cho cả qpb và ứng dụng của bạn.

Trình biên dịch: các header của chính Qt 5.15 không biên dịch được với toolset mặc định của MSVC 2026 (chúng dùng
`stdext::checked_array_iterator`, thứ mà thư viện chuẩn của nó không còn nữa). Hãy dùng toolset MSVC 2019 (14.29) hoặc
2022, GCC hoặc Clang với Qt 5.15; Visual Studio 2026 chạy được với toolset 14.29 đã được cài và chọn
(`vcvarsall x86 -vcvars_ver=14.29`).

## Nền tảng

- **Windows** (MSVC 2019 trở lên; Qt 6 với MSVC 2022 trở lên, Qt 5.15 với toolset MSVC 2019 hoặc 2022): được kiểm chứng
  cho mọi bản phát hành bằng một lần chạy test đầy đủ ở cả hai cấu hình Qt.
- **Linux** (GCC, Clang): được hỗ trợ. Code và các tệp CMake có tính di động, nhưng Linux không được build cho mọi bản
  phát hành.
- **macOS**: không được hỗ trợ.

Chỉ dùng Qt Core, Gui và Widgets (Test cho các test của riêng qpb). Không có phụ thuộc nào khác.

## Nâng cấp

1. Thay thế toàn bộ thư mục: xóa `components/qpb/` và đặt phiên bản mới vào chỗ của nó (hoặc dùng lệnh subtree hay
   submodule ở trên). Không bao giờ giải nén một phiên bản mới đè lên thư mục cũ: các tệp mà phiên bản mới đã bỏ sẽ còn
   lại.
2. Build lại dự án của bạn. CMake chạy lại vì `VERSION` đã thay đổi.
3. Đọc các *Upgrade notes* (ghi chú nâng cấp) của phiên bản mới, và của mọi phiên bản ở giữa, trong
   [CHANGELOG.md](../../CHANGELOG.md) (bằng tiếng Anh). Hầu hết đều ghi "nothing to do" (không cần làm gì).

Trong 1.x, code và CMake của bạn đều không phải đổi; xem [Tương thích](compatibility.md). Để tìm những chỗ dùng API sắp
bị loại bỏ, hãy định nghĩa `QPB_DISABLE_DEPRECATED` một lần, như mô tả ở đó.
