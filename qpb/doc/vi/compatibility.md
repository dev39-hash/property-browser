# Tương thích

> Đây là bản dịch tiếng Việt của tài liệu qpb 1.9.0. Bản tiếng Anh ([../compatibility.md](../compatibility.md)) là bản
> chuẩn; khi hai bản khác nhau thì theo bản tiếng Anh.

Các phiên bản 1.x cam kết điều gì, điều gì có thể và không thể thay đổi trong một bản phát hành minor hay patch,
deprecation hoạt động ra sao, Qt 5 và Qt 6 khác nhau thế nào, và cách kiểm tra phiên bản. Quay lại
[mục lục tài liệu](README.md#tài-liệu).

## Cam kết của 1.x

Trong một phiên bản chính (1.x), code build được với 1.a thì build được và hoạt động như cũ với mọi 1.b về sau, chỉ
bằng cách thay thế thư mục `qpb/`. Code và CMake của bạn không đổi; điều này đúng cho từng phiên bản chính của Qt (xem
[Qt 5 và Qt 6](#qt-5-và-qt-6)).

- Cam kết này là tương thích **mã nguồn**, không phải tương thích nhị phân. Bạn luôn build lại từ thư mục, nên các header
  được tự do giấu các chi tiết nội bộ, và một thư viện shared của phiên bản này không thể thay thế trực tiếp cho phiên
  bản khác (xem [Tích hợp](integration.md#static-hay-shared)).
- Ngoại lệ duy nhất là sửa lỗi: khi hành vi cũ mâu thuẫn với tài liệu, hành vi được sửa lại và mục trong
  [CHANGELOG.md](../../CHANGELOG.md) (bằng tiếng Anh) có nói rõ.
- Các phiên bản tối thiểu của C++, Qt và CMake chỉ tăng trong một phiên bản chính mới.

## Điều gì có thể và không thể thay đổi trong 1.x

| Có thể thay đổi trong bản minor hoặc patch | Không đổi trước 2.0 |
|---|---|
| Lớp, hàm tự do và header mới | Xóa hoặc đổi tên bất cứ thứ gì public |
| Hàm thành viên non-virtual mới và overload mới không gây mơ hồ | Đổi kiểu tham số hoặc kiểu trả về của hàm đang có, thêm tham số (kể cả tham số có giá trị mặc định) |
| Giá trị enum mới, được thêm vào cuối; role model mới, được thêm vào cuối | Giá trị số của một giá trị enum hoặc role model đang có |
| Field mới ở cuối `TypeHandler`, `EditorHandler`, `EnumOption`, `ValidationResult`; field để trống giữ hành vi cũ | Ý nghĩa của các field đang có; khởi tạo theo vị trí không được hỗ trợ (xem bên dưới) |
| Hàm virtual mới có cài đặt mặc định giữ hành vi cũ | Một hàm pure virtual mới trong lớp mà bạn có thể kế thừa |
| Attribute, type id và signal mới | Chữ ký của một signal hoặc slot đang có |
| Tùy chọn CMake mới có mặc định giữ hành vi cũ | Tên của target, tùy chọn và đường dẫn include; mức tối thiểu của C++, Qt và CMake |
| Đánh dấu deprecated cho API đã có thứ thay thế | Xóa API đã deprecated |
| Văn bản có thể dịch và bản dịch mới | Context hoặc văn bản gốc của một văn bản có thể dịch đang có |
| Sửa lỗi khi hành vi mâu thuẫn với tài liệu (được nêu trong changelog) | Hành vi và mặc định đã được ghi trong tài liệu |

Hành vi mới mà có thể làm thay đổi cách ứng dụng hoạt động sau khi nâng cấp, chẳng hạn một hành vi bàn phím, luôn là
opt-in (ví dụ `setTabStopsOnCheckBoxes()`, `setLiveReadOnlyProperties()`).

## Bạn nên làm gì để đáp lại

- Gán các field của `TypeHandler`, `EditorHandler`, `EnumOption` và `ValidationResult` theo tên. Đừng bao giờ khởi tạo
  chúng theo vị trí bằng `{a, b, c}`: field mới được thêm vào cuối, điều đó sẽ làm đổi ý nghĩa. `EnumOption` có dạng hai
  đối số như trong các ví dụ; hãy tạo `ValidationResult` bằng `valid()` và `error()`.
- Gọi các hàm của `qpb::serialization` kèm tên namespace đầy đủ. Các hàm tự do được thêm sau 1.0 nằm trong các namespace
  lồng nhau, nên chúng không bao giờ xung đột với những lời gọi không kèm namespace của chính bạn; viết kèm namespace là
  cách để với tới chúng.
- Kiểm tra `ValidationResult::ok`, đừng bao giờ kiểm tra câu chữ của thông điệp. Câu chữ của các thông điệp có sẵn được
  dịch và có thể được cải thiện.
- Đừng kế thừa `Property` hay `PropertyGroup`; chúng không được thiết kế để mở rộng. Hãy thêm hành vi thông qua
  validator, điều kiện, type handler và editor handler.
- Hãy bắt đầu các role model của riêng bạn từ `PropertyModel::UserRole`. Khoảng từ `Qt::UserRole + 1` đến
  `Qt::UserRole + 99` được dành riêng cho qpb.
- Dùng type id với tiền tố của riêng bạn (`"myapp.color"`); các id bắt đầu bằng `qpb.` là dành riêng.
- Đừng dựa vào bất cứ thứ gì nội bộ: namespace `qpb::detail`, thư mục `src/`, và tên lớp của các widget nội bộ của qpb
  (thay vào đó hãy dùng các property của style sheet được mô tả trong [Style sheet](guide/style-sheets.md)).

## Deprecation

API đã được thay thế được đánh dấu `QPB_DEPRECATED_X("use X instead")` (ánh xạ sang `[[deprecated]]`, nên trình biên
dịch cảnh báo ở nơi bạn còn dùng nó) và được giữ đến hết 1.x; nó chỉ bị xóa trong 2.0. Mọi khai báo deprecated đều được
bọc trong `#if !defined(QPB_DISABLE_DEPRECATED)`. Để tìm các chỗ còn dùng trước khi nâng cấp, hãy định nghĩa
`QPB_DISABLE_DEPRECATED` cho bản build của bạn: khi đó mỗi chỗ dùng sẽ không biên dịch được.

```cmake
target_compile_definitions(my_app PRIVATE QPB_DISABLE_DEPRECATED)
```

Mỗi phiên bản trong [CHANGELOG.md](../../CHANGELOG.md) (bằng tiếng Anh) liệt kê những gì đã được *Added*, *Changed*,
*Deprecated* và *Fixed*, và có *Upgrade notes*, thường là "nothing to do" (không cần làm gì).

## Văn bản có thể dịch

Các context và văn bản gốc của những văn bản có thể dịch của qpb ổn định trong 1.x, nên một bản dịch mà bạn đã làm cho
ngôn ngữ khác, hoặc một cách diễn đạt khác mà bạn đã cài, vẫn tiếp tục hoạt động. Danh sách nằm trong
[Bản dịch](guide/translations.md).

## Qt 5 và Qt 6

Từ 1.7 qpb build với Qt 5.15 và với Qt 6.5 trở lên từ cùng một mã nguồn, với cùng hành vi. Cam kết tương thích đúng cho
từng phiên bản chính của Qt: một ứng dụng build với Qt 5 tiếp tục build với Qt 5 khi thư mục được cập nhật, và Qt 6 cũng
vậy. Chuyển một ứng dụng từ Qt 5 sang Qt 6 là việc di chuyển (migration) của riêng Qt và có thể cần hai thay đổi dưới
đây.

Khi Qt 5 không có kiểu tương đương với một kiểu của Qt 6, các header public khác nhau:

- `Types::*` và `Attr::*` là các hằng `QLatin1String` với Qt 5 và các hằng `QLatin1StringView` với Qt 6. Cả hai đều
  chuyển đổi được sang `QString`, nên code dùng chúng làm type id hoặc khóa attribute là như nhau.
- `TypeHandler::storageType` là một type id `int` với Qt 5, mặc định là `QMetaType::UnknownType`, và là một `QMetaType`
  với Qt 6. `registerType<T>(id, handler)` đặt nó ở cả hai. Với Qt 5 bạn cũng có thể viết
  `handler.storageType = qMetaTypeId<T>();`.

Nếu bạn muốn code biên dịch được với cả hai, hãy dùng `registerType<T>()` và chỉ dùng các hằng ở nơi nhận một `QString`.
Cách thiết lập CMake cho Qt 5 và các giới hạn về trình biên dịch nằm trong [Tích hợp](integration.md#qt-5).

## Kiểm tra phiên bản

`qpbglobal.h` cung cấp phiên bản của các header mà bạn biên dịch cùng, được sinh từ tệp `VERSION`:

| Tên | Ý nghĩa |
|---|---|
| `QPB_VERSION_MAJOR`, `QPB_VERSION_MINOR`, `QPB_VERSION_PATCH` | Ba con số |
| `QPB_VERSION_STR` | Chuỗi đầy đủ, gồm cả hậu tố pre-release nếu có, chẳng hạn `"1.0.0-rc1"` |
| `QPB_VERSION` | Phiên bản được đóng gói vào một số nguyên, so sánh được trong `#if` |
| `QPB_VERSION_CHECK(major, minor, patch)` | Đóng gói một phiên bản theo cùng cách |
| `qpb::version()` | Phiên bản của thư viện được liên kết lúc chạy, ví dụ `"1.8.0"` |

```cpp
#if QPB_VERSION >= QPB_VERSION_CHECK(1, 4, 0)
    model.onValueChanged("Transform/fov", this, [this](const QVariant& v) { apply(v); });
#else
    connect(&model, &qpb::PropertyModel::valueChanged, this, &Window::onValueChanged);
#endif
```

So sánh `qpb::version()` với `QPB_VERSION_STR` để phát hiện sự không khớp giữa header và thư viện trong bản build shared.
