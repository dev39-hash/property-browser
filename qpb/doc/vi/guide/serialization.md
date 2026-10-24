# Lưu và nạp

> Đây là bản dịch tiếng Việt của tài liệu qpb 1.9.0. Bản tiếng Anh
> ([../../guide/serialization.md](../../guide/serialization.md)) là bản chuẩn; khi hai bản khác nhau thì theo bản tiếng
> Anh.

Ghi giá trị property ra JSON hoặc `QSettings` và đọc chúng trở lại (từ 1.2). Quay lại
[mục lục tài liệu](../README.md#tài-liệu).

```cpp
#include <qpb/Serialization.h>

qpb::serialization::save(*model.root(), settings);   // keys are paths: "General/language"
model.beginBatch();
qpb::serialization::load(*model.root(), settings);   // values go through validation
model.endBatch();
```

Bốn hàm này làm việc trên một nhóm và các phần tử con cháu của nó:

| Hàm | Công dụng |
|---|---|
| `QJsonObject toJson(const PropertyGroup&)` | Mỗi nhóm trở thành một object lồng nhau với khóa là id của nó, mỗi property khác là giá trị của nó |
| `bool fromJson(PropertyGroup&, const QJsonObject&)` | Đặt các giá trị tìm thấy trong object |
| `void save(const PropertyGroup&, QSettings&)` | Ghi một khóa cho mỗi giá trị dưới group hiện tại của settings; các khóa là đường dẫn tương đối so với nhóm (`"General/language"`) |
| `bool load(PropertyGroup&, const QSettings&)` | Đặt các giá trị đã được `save()` lưu |

## Những gì được lưu

- Chỉ có giá trị được lưu, không bao giờ lưu cấu trúc. Ứng dụng dựng cây trước, rồi nạp giá trị vào đó.
- Các property read-only và [property live](changes-and-conditions.md#giá-trị-live) đều không được ghi cũng không được
  đọc: ứng dụng duy trì chúng, và việc nạp sẽ ghi đè, chẳng hạn, phiên bản đang chạy bằng một phiên bản đã lưu. Các
  property hidden và disabled thì được lưu.
- Các nhóm không có gì để lưu (trống, hoặc chỉ chứa property read-only) không được ghi.
- Các giá trị được khôi phục bằng `Property::setValue()`, nên chúng được chuyển đổi, clamp và xác thực như mọi giá trị
  khác mà code của bạn đặt (xem [Giá trị và xác thực](values-and-validation.md)).
- Các khóa không có property tương ứng, và các property không có khóa, bị bỏ qua: property giữ nguyên giá trị của nó.
- `fromJson()` và `load()` trả về `false` nếu một giá trị bị từ chối. Property bị từ chối giữ nguyên giá trị và các giá
  trị còn lại vẫn được áp dụng.
- Hãy bọc việc nạp trong `beginBatch()` / `endBatch()` để nhận một `batchValueChanged` thay vì nhiều signal.

## Dạng JSON của một giá trị

Mặc định một giá trị được ghi bằng `QJsonValue::fromVariant()` và đọc bằng `QJsonValue::toVariant()`. `Int64` ghi các
số đến 2^53 dưới dạng số JSON và các giá trị lớn hơn dưới dạng chuỗi, để không mất độ chính xác. Một kiểu cần dạng khác
thì đặt `TypeHandler::toJson` và `TypeHandler::fromJson`; xem [Kiểu và editor tùy biến](custom-types-and-editors.md).
Ví dụ, một kiểu `QColor` có thể được lưu dưới dạng `"#ff0000"`.

## Gọi các hàm kèm namespace đầy đủ

Các hàm nằm trong namespace `qpb::serialization`, nên hãy viết `qpb::serialization::save(...)`. Chúng nằm trong một
namespace lồng nhau có chủ đích: argument-dependent lookup không tìm trong đó, nên qpb không bao giờ có thể làm cho lời
gọi `save(group, settings)` không kèm namespace của chính bạn trở nên mơ hồ, bây giờ hay sau một lần nâng cấp.
