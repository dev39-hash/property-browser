# Property của QObject

> Đây là bản dịch tiếng Việt của tài liệu qpb 1.9.0. Bản tiếng Anh
> ([../../guide/qobject-source.md](../../guide/qobject-source.md)) là bản chuẩn; khi hai bản khác nhau thì theo bản
> tiếng Anh.

Hiển thị các `Q_PROPERTY` của object của riêng bạn trong một model và giữ hai phía đồng bộ (từ 1.2). Quay lại
[mục lục tài liệu](../README.md#tài-liệu).

`QObjectPropertySource` thêm một nhóm cho mỗi object vào một `PropertyModel`, với một property cho mỗi `Q_PROPERTY`. Các
chỉnh sửa trong model được ghi vào object; các thay đổi của object xuất hiện trong model. Bạn không phải đổi lớp mà bạn
hiển thị: metadata tùy chọn là `Q_CLASSINFO`, thứ mà một lớp có thể thêm vào mà không đụng đến logic của nó.

```cpp
class Light : public QObject {
    Q_OBJECT
    Q_CLASSINFO("qpb:title", "name")                         // group title follows this Q_PROPERTY
    Q_CLASSINFO("qpb:intensity", "min=0;max=100;suffix= %")
    Q_CLASSINFO("qpb:used", "live")
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY changed)
    ...
};
qpb::QObjectPropertySource source(&model);
source.addObject(&light);
```

Model không thuộc sở hữu của source và phải sống lâu hơn nó.

## Những property nào được hiển thị

- Mặc định là các property được khai báo bởi lớp của object và các lớp cơ sở của nó, trừ những property của chính
  `QObject` (`objectName`). `Q_CLASSINFO("qpb:properties", "name,fov,lut")` chọn chúng và đặt thứ tự của chúng.
- Các kiểu được hỗ trợ: `bool` (Bool), `int` (Int), `qint64` (Int64), `double` và `float` (Double), `QString` (String),
  các enumeration được khai báo bằng `Q_ENUM` (Enum, với các khóa làm nhãn và các giá trị nguyên), và bất kỳ kiểu nào
  khác đã được đăng ký trong `TypeRegistry` cho `QMetaType` của property. Các kiểu khác, kể cả flags, bị bỏ qua.
- Một `Q_PROPERTY` không có accessor `WRITE` trở thành read-only.
- Các giá trị mà object có khi nó được thêm vào là các giá trị mặc định.

## Metadata

`Q_CLASSINFO("qpb:<property>", "key=value;key=value")` mô tả một property. Một khóa không có `=value` nghĩa là true.

| Khóa | Tác dụng |
|---|---|
| `type` | Type id dùng thay cho id suy ra từ kiểu C++, ví dụ `type=filepath` |
| `displayName`, `toolTip` | Các văn bản |
| `readOnly`, `hidden`, `disabled` | Các flag trạng thái |
| `exclude` | Không hiển thị property này |
| `live` | Một giá trị live (từ 1.3); xem [Thay đổi và điều kiện](changes-and-conditions.md#giá-trị-live) |
| `enabledWhen`, `visibleWhen` | Tên của một `Q_PROPERTY` khác của cùng object; property này được enable / hiển thị khi property kia là "true" (từ 1.4); xem [Thay đổi và điều kiện](changes-and-conditions.md#phụ-thuộc-giữa-các-property) |
| bất kỳ khóa attribute nào | Một attribute như `suffix`, `step`, `decimals`, `filter` (`min` và `max` thay cho `minimum` và `maximum`) |

```cpp
Q_CLASSINFO("qpb:fov", "min=10;max=170;suffix= deg;displayName=Field of view")
Q_CLASSINFO("qpb:lut", "type=filepath;filter=LUT (*.cube)")
```

Các văn bản của class info như `displayName=` và `toolTip=`, và các khóa của `Q_ENUM`, được dùng đúng như đã viết. Để
dịch chúng, hãy đặt lại chúng bằng `setDisplayName()` / `setToolTip()` sau `addObject()`; xem
[Bản dịch](translations.md).

## Tiêu đề của nhóm

Id của nhóm, và do đó mọi đường dẫn, là tên object; điều này giữ các đường dẫn ổn định cho các tệp đã lưu và code ứng
dụng. Tên hiển thị của nhóm có thể đi theo một `Q_PROPERTY` thay vào đó (từ 1.3):

- `Q_CLASSINFO("qpb:title", "name")` trong lớp, hoặc
- `source.setTitleProperty("name")` cho các lớp bạn không thể sửa, áp dụng cho các object được thêm sau đó. Class info
  được ưu tiên.

Tiêu đề được cập nhật qua signal `NOTIFY` của property; một giá trị trống giữ nguyên tiêu đề trước đó.

## Các property read-only live

`setLiveReadOnlyProperties(true)` (từ 1.3) biến mọi `Q_PROPERTY` của các object được thêm sau đó mà có signal `NOTIFY`
nhưng không có accessor `WRITE` thành một property live: một giá trị do object duy trì, không bao giờ hiển thị là đã
sửa và không được lưu. Mặc định nó tắt, nghĩa là các property như vậy vẫn là property read-only thông thường.

## Thêm, xóa và giữ đồng bộ

- `addObject(object, parentGroup, id)` thêm một nhóm và trả về nó. Cha mặc định là gốc của model (một model không có
  gốc sẽ có một gốc trống); một nhóm cha được chỉ định phải thuộc về model. Id của nhóm là `id`, nếu không thì là tên
  object, nếu không nữa thì là tên lớp, được làm cho duy nhất bằng hậu tố `_2`, `_3`, ... Thêm lại một object sẽ trả về
  nhóm đã có của nó. Nó trả về `nullptr` với một object null.
- `removeObject(object)` xóa nhóm và ngừng đồng bộ nó; nó trả về `false` nếu object chưa từng được thêm. `objects()`
  liệt kê các object và `groupOf(object)` tìm một nhóm. Xóa một object, hoặc hủy nó, là an toàn vào bất kỳ lúc nào: kể
  cả từ một handler của `valueChanged`, từ một slot trên signal `NOTIFY` của chính object hoặc từ setter của nó, trong
  lúc source đang ghi một giá trị hoặc đọc lại object. Nhóm rời khỏi model và object ngừng được đồng bộ ngay lập tức.
- Các chỉnh sửa trong model được ghi bằng `QMetaProperty::write`. Nếu object điều chỉnh hoặc từ chối giá trị, model hiển
  thị giá trị của object.
- Các thay đổi của object được đọc lại qua signal `NOTIFY` của nó. Với một `Q_PROPERTY` không có signal đó, hãy gọi
  `refresh()`, hàm này đọc lại mọi property của mọi object.
- Khi một object bị hủy, nhóm của nó bị xóa. Khi source bị hủy, các nhóm vẫn ở lại trong model và việc đồng bộ dừng lại.
- Source, hoặc cha của nó, có thể bị xóa trong lúc đồng bộ: từ một handler của `valueChanged`, `dataChanged`,
  `rowsInserted` hoặc `rowsRemoved` của model, từ setter của object, hoặc từ một slot trên signal `NOTIFY` của object
  (không phải từ một getter của nó). Thao tác ghi hoặc đọc đang diễn ra sẽ hoàn tất và không có gì được đồng bộ thêm.
  Một source là con của model sẽ đi cùng model khi một handler của `valueChanged` hoặc `dataChanged` xóa nó; xóa model
  từ một handler của các signal khác thì không được hỗ trợ.
- Một handler của `valueChanged` hoặc `dataChanged` cũng có thể xóa model trong khi source vẫn còn sống, ví dụ khi source
  không phải là con của nó: source không đồng bộ thêm gì nữa, và các object vẫn ở trạng thái đã được thêm nhưng không
  được đồng bộ.
- Một handler của `valueChanged` hoặc `dataChanged` có thể xóa nhóm, hoặc một trong các property của nó, trong khi
  `addObject()` vẫn đang thêm nó. Lời gọi dừng ngay lập tức; object không được đồng bộ và không có bản ghi nào trong
  source. Nhóm chỉ giữ các property đã được thêm cho đến lúc đó. `addObject()` trả về nhóm đó khi nó còn trong model, và
  `nullptr` khi model đã bị xóa hoặc nhóm đã bị xóa trong lúc đó.
