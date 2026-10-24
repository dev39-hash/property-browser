# qpb - trình duyệt property cho Qt 6 và Qt 5.15 Widgets

> Đây là bản dịch tiếng Việt của tài liệu qpb 1.9.0. Bản tiếng Anh ([../../README.md](../../README.md)) là bản chuẩn;
> khi hai bản khác nhau thì theo bản tiếng Anh.

qpb hiển thị và chỉnh sửa một cây các property có kiểu ("tên | giá trị") trong ứng dụng Qt Widgets: trình kiểm tra đối
tượng (object inspector), hộp thoại cài đặt, cấu hình plugin và thiết bị. Bạn mô tả các property trong code; qpb cung cấp
model, các view và editor, việc xác thực (validation), đặt lại về giá trị mặc định, thông báo thay đổi, lưu và nạp.

Thư mục này là toàn bộ thư viện. Chép nó vào dự án của bạn, thêm hai dòng CMake rồi build. Cập nhật nghĩa là thay thế
thư mục: trong cùng một phiên bản chính (1.x), code và CMake của bạn không bao giờ phải đổi (xem
[Tương thích](#tương-thích)).

Nội dung: [Yêu cầu](#yêu-cầu) - [Tích hợp](#tích-hợp) - [Tính năng](#tính-năng) -
[Bắt đầu nhanh](#bắt-đầu-nhanh) - [Tài liệu](#tài-liệu) - [Tương thích](#tương-thích) -
[Lịch sử phiên bản](#lịch-sử-phiên-bản)

## Yêu cầu

- C++17 trở lên, Qt 6.5 trở lên hoặc (từ 1.7) Qt 5.15 (Core, Widgets), CMake 3.21 trở lên.
- Windows (MSVC) và Linux (GCC, Clang) với bản Qt hỗ trợ chúng. Windows được kiểm chứng cho mọi bản phát hành, Linux
  được hỗ trợ; xem [Nền tảng](integration.md#nền-tảng) và [Qt 5](integration.md#qt-5) để biết chi tiết và các trình biên
  dịch mà Qt 5.15 chấp nhận.

## Tích hợp

Đặt thư mục này vào dự án của bạn, ví dụ thành `components/qpb/`, rồi thêm:

```cmake
find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets)
add_subdirectory(components/qpb)
target_link_libraries(my_app PRIVATE qpb::widgets)   # or qpb::core without widgets
```

```cpp
#include <qpb/qpb.h>       // everything; <qpb/qpbcore.h> for qpb::core only
```

Thư mục này không thay đổi bất kỳ thiết lập CMake nào của dự án bạn. Cách lấy thư mục (zip bản phát hành, git subtree,
git submodule), các tùy chọn CMake, build static và shared, Qt 5 và nâng cấp được mô tả trong
[doc/vi/integration.md](integration.md).

## Tính năng

- **Cây property**: các nhóm (group) và property với id, đường dẫn (`"Transform/x"`), tên hiển thị, tooltip, giá trị và
  giá trị mặc định. Được dựng trong code bằng các builder dạng fluent.
- **Kiểu**: bool, int, int 64 bit, double, chuỗi (một dòng hoặc nhiều dòng), enum, đường dẫn tệp, đường dẫn thư mục, và
  các kiểu của riêng bạn (ví dụ `QColor`) thông qua type registry.
- **Attribute**: khoảng giá trị, bước nhảy, số chữ số thập phân (làm tròn là tùy chọn), commit ở mỗi bước, tiền tố và
  hậu tố, độ dài tối đa, placeholder, biểu thức chính quy, bộ lọc tệp, chế độ hộp thoại, thư mục mặc định, "phải tồn
  tại", các tùy chọn của enum.
- **Xác thực**: kiểm tra chuyển đổi, chuẩn hóa (clamp, làm tròn) và kiểm tra kiểu, cộng thêm một validator cho mỗi
  property. Giá trị bị từ chối thì giữ nguyên giá trị cũ và báo một thông điệp.
- **Trạng thái**: chỉ đọc (read-only), vô hiệu hóa (disabled), ẩn (hidden) và live (giá trị do ứng dụng duy trì), được
  các phần tử con thừa hưởng. Các điều kiện cho phép hoặc hiển thị một property tùy theo giá trị của property khác.
- **Model**: một `QAbstractItemModel` hai cột cho mọi view của Qt; signal thay đổi theo từng giá trị và theo từng batch,
  một signal riêng cho các chỉnh sửa của người dùng, callback theo đường dẫn; có thể thêm và xóa property khi model đang
  hiển thị; đặt lại một property, một nhóm hoặc tất cả; chuẩn hóa các giá trị và biến chúng thành giá trị mặc định.
- **View**: `PropertyTreeView` (cây có thể thu gọn hoặc danh sách phẳng với tiêu đề section) và `PropertyFormView` (một
  form với các editor luôn mở và các section có thể thu gọn). Tên in đậm cho giá trị đã sửa, menu ngữ cảnh "Reset to
  default", dùng được hoàn toàn bằng bàn phím (Enter, Esc, Tab), thông điệp xác thực hiện dưới dạng tooltip. Form view có
  thể hiển thị màu `Qt::ForegroundRole` của model trên nhãn và tiêu đề section (`showModelForeground`, mặc định tắt).
- **Editor**: một editor cho mỗi kiểu, có thể thay thế theo kiểu hoặc theo property; editor tùy biến có thể mở hộp thoại.
- **Tìm kiếm**: một filter proxy model theo tên hiển thị cho cả hai view.
- **Property của QObject**: hiển thị các `Q_PROPERTY` của object của bạn và giữ hai phía đồng bộ.
- **Lưu**: giá trị sang và từ JSON và `QSettings`.
- **Giao diện (theming)**: style sheet của Qt, kèm các hook cho những gì qpb tự vẽ.
- **Bản dịch**: các văn bản riêng của qpb bằng tiếng Anh, tiếng Nhật và tiếng Việt, đổi được khi đang chạy.

## Bắt đầu nhanh

```cpp
#include <qpb/qpb.h>
#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    auto root = qpb::PropertyGroup::create("Camera");
    root->addString("name", "Main camera");
    root->addBool("visible", true);
    auto& transform = root->addGroup("Transform");
    transform.addDouble("x", 0.0).range(-100.0, 100.0).suffix(" m");
    transform.addInt("fov", 60).range(10, 170).displayName("Field of view");
    root->addEnum("projection", {"Perspective", "Orthographic"}, 0);
    root->addFilePath("lut", {}).filter("LUT files (*.cube)");

    qpb::PropertyModel model(std::move(root));
    model.onValueChanged("Transform/fov", &app, [](const QVariant& v) { qDebug() << "fov" << v; });

    qpb::PropertyTreeView view;   // or qpb::PropertyFormView
    view.setModel(&model);
    view.show();
    return app.exec();
}
```

## Tài liệu

| Trang | Nội dung |
|---|---|
| [Tích hợp](integration.md) | Lấy thư mục, CMake, các tùy chọn, build static và shared, Qt 5, nền tảng, nâng cấp |
| [Tổng quan API](api-overview.md) | Các lớp public theo từng header |
| [Giá trị và xác thực](guide/values-and-validation.md) | Pipeline xử lý giá trị, các kiểm tra có sẵn, validator, giá trị mặc định và đặt lại |
| [Thay đổi và điều kiện](guide/changes-and-conditions.md) | Phản ứng với thay đổi, batch, phụ thuộc giữa các property, giá trị live |
| [Lưu và nạp](guide/serialization.md) | JSON và `QSettings` |
| [Kiểu và editor tùy biến](guide/custom-types-and-editors.md) | Các kiểu property của riêng bạn, thay thế hoặc viết editor |
| [Property của QObject](guide/qobject-source.md) | Hiển thị các `Q_PROPERTY` của object bằng metadata `Q_CLASSINFO` |
| [View](guide/views.md) | Tree view, form view, filter proxy, bàn phím, thread |
| [Style sheet](guide/style-sheets.md) | Tạo giao diện cho các view và editor |
| [Bản dịch](guide/translations.md) | Các ngôn ngữ riêng của qpb, văn bản của bạn, đổi ngôn ngữ khi đang chạy |
| [Tương thích](compatibility.md) | Cam kết của 1.x, deprecation, khác biệt giữa Qt 5 và Qt 6, kiểm tra phiên bản |

Tài liệu gốc bằng tiếng Anh: [../../README.md](../../README.md). Bản tiếng Anh là bản chuẩn; bản dịch này được cập
nhật theo mỗi bản phát hành.

## Tương thích

Trong 1.x, cập nhật thư mục không bao giờ đòi hỏi thay đổi code hay CMake của bạn (với cùng một phiên bản chính của
Qt): không có gì public bị xóa hay thay đổi, chỉ có thêm vào. Điều gì có thể và không thể thay đổi, và bạn nên làm gì
để đáp lại, được mô tả trong [doc/vi/compatibility.md](compatibility.md).

## Lịch sử phiên bản

Mỗi phiên bản có một mục trong [`CHANGELOG.md`](../../CHANGELOG.md) (bằng tiếng Anh, nằm trong thư mục `qpb/`) ghi những
gì đã được thêm, thay đổi, deprecated và sửa, cùng các ghi chú nâng cấp.

- **1.9.0** - thay đổi do người dùng sửa được báo riêng (`userValueChanged`), chuẩn hóa giá trị và đặt giá trị hiện
  tại làm giá trị mặc định, số thực không làm tròn, màu của model trong form view, handler được phép thay đổi cây
  hoặc xóa model; tài liệu tiếng Việt.
- **1.8.0** - bản dịch các văn bản riêng của qpb (tiếng Anh, tiếng Nhật, tiếng Việt).
- **1.7.0** - Qt 5.15 như một cấu hình thứ hai.
- **1.6.1** - chỉ có tài liệu: README này trong thư mục component.
- **1.6.0** - hook style sheet cho tree view và form view.
- **1.5.0** - `resetAllToDefault()`, check box trong chuỗi Tab.
- **1.4.0** - callback `onValueChanged()`, điều kiện `enabledWhen()` / `visibleWhen()`.
- **1.3.0** - giá trị live, tiêu đề nhóm từ `QObjectPropertySource`.
- **1.2.0** - `QObjectPropertySource`, tuần tự hóa JSON và `QSettings`, `Int64`.
- **1.1.0** - `PropertyFormView`, `PropertyFilterProxyModel`, chuỗi nhiều dòng.
- **1.0.0** - bản ổn định đầu tiên: model, tree view, editor cho các kiểu cơ bản.
