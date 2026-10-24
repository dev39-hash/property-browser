# Thay đổi và điều kiện

> Đây là bản dịch tiếng Việt của tài liệu qpb 1.9.0. Bản tiếng Anh
> ([../../guide/changes-and-conditions.md](../../guide/changes-and-conditions.md)) là bản chuẩn; khi hai bản khác nhau
> thì theo bản tiếng Anh.

Phản ứng với các thay đổi giá trị, làm cho một property phụ thuộc vào property khác, và đánh dấu những giá trị mà ứng
dụng của bạn duy trì. Quay lại [mục lục tài liệu](../README.md#tài-liệu).

## Phản ứng với thay đổi

Model thông báo các thay đổi bằng signal. Chúng mang theo đường dẫn của property (một `QString`), không phải tham chiếu
tới nó, nên một đường dẫn mà bạn lưu lại vẫn còn ý nghĩa khi cây thay đổi; hãy dùng `model.find(path)` để lấy property.

| Signal | Khi nào |
|---|---|
| `valueChanged(path, newValue, oldValue)` | Một giá trị đã thay đổi, từ bất kỳ nguồn nào: người dùng, `setValue()`, một lần đặt lại, việc nạp |
| `userValueChanged(path, newValue, oldValue)` (từ 1.9) | Một giá trị đã thay đổi vì người dùng chỉnh sửa nó; được phát ngay sau `valueChanged` với cùng các đối số |
| `validationFailed(path, rejectedValue, message)` | Một giá trị bị từ chối; giá trị cũ được giữ lại |
| `batchValueChanged(paths)` | `endBatch()` ngoài cùng, một lần, với đường dẫn của mọi thứ đã thay đổi trong batch |

Hãy ưu tiên `onValueChanged()` (từ 1.4) hơn một handler của `valueChanged` phải so sánh đường dẫn:

```cpp
model.onValueChanged("Transform/x", this, [this](const QVariant& v) { m_object.x = v.toDouble(); });
model.onValueChanged("Output", this, [](const QString& path, const QVariant& v) { /* any child */ });
```

- Dạng thứ nhất được gọi với giá trị mới của một property. Dạng thứ hai nhận đường dẫn của một nhóm (đường dẫn trống
  nghĩa là cả cây) và được gọi cho các phần tử con cháu của nhóm, kèm đường dẫn của phần tử đã thay đổi.
- Nó hoạt động như `QObject::connect`: kết nối kết thúc khi `context` bị hủy (truyền `nullptr` để gắn nó với model) và
  có thể được ngắt thông qua `QMetaObject::Connection` được trả về.
- Nó đi theo đường dẫn, không phải một đối tượng `Property`, nên vẫn hoạt động sau `setRoot()` và khi một property bị
  xóa rồi được thêm lại.
- Nó được gọi sau `valueChanged`, và sau khi các điều kiện bên dưới đã được đánh giá, nên handler thấy trạng thái
  enabled và visible mới.

**Các chỉnh sửa của người dùng.** Để ghi ra thiết bị hoặc tệp những gì người dùng đã gõ, chứ không phải những gì code
của bạn đã lưu, hãy dùng `userValueChanged` (từ 1.9). Các view ghi qua `PropertyModel::setData()`, nên mọi editor, check
box và các lệnh "Reset to default" và "Reset group" đều được coi là người dùng, bất cứ khi nào giá trị thay đổi.
`setValue()`, `Property::setValue()`, `resetToDefault()`, `resetAllToDefault()`, `normalizeValue()`, việc nạp, và một
`QObjectPropertySource` đi theo object của nó là các lần ghi của ứng dụng: chúng chỉ phát `valueChanged`. Do đó các lần
làm mới và hoàn tác (rollback) của chính bạn không quay lại handler của bạn, và bạn không bao giờ phải chặn các signal
của model (các view đang lắng nghe chúng).

```cpp
model.onUserValueChanged("Transform/x", this, [this](const QVariant& v) { m_device.setX(v.toDouble()); });
model.onUserValueChanged("Output", this, [](const QString& path, const QVariant& v) { /* any child */ });
```

`onUserValueChanged()` có hai dạng của `onValueChanged()` và cùng các quy tắc (context, ngắt kết nối, đi theo đường
dẫn). Bên trong một batch, `userValueChanged` được phát cho mọi thay đổi, như `valueChanged`. Một handler hoàn tác một
chỉnh sửa bằng `setValue()` tạo ra một lần ghi của ứng dụng, lần ghi này không đến `userValueChanged` nữa. Các validator
chạy cho cả hai loại ghi.

Các đối số của `userValueChanged` là lần ghi của người dùng đúng như nó đã được thực hiện. Khi code khác điều chỉnh giá
trị ở giữa - một handler của `valueChanged` hoàn tác hoặc clamp nó, hoặc một `QObjectPropertySource` có object điều
chỉnh nó (một lần ghi của ứng dụng lồng bên trong, được chuyển đi trước) - thì property đã giữ một giá trị khác khi
`userValueChanged` đến. Vì vậy một handler ghi ra thiết bị nên đọc giá trị hiện tại của property
(`model.find(path)->value()`) thay vì dùng giá trị được đưa cho nó.

**Batch.** `valueChanged` được phát cho mọi thay đổi. Để chỉ phản ứng một lần với một cập nhật hàng loạt, chẳng hạn nạp
cài đặt hoặc đặt lại một nhóm, hãy bọc nó trong `beginBatch()` / `endBatch()`; `endBatch()` ngoài cùng phát
`batchValueChanged` với tất cả đường dẫn. Các batch lồng nhau được.

```cpp
model.beginBatch();
qpb::serialization::load(*model.root(), settings);
model.endBatch();
```

**Thay đổi cây khi nó đang được hiển thị.** Có thể thêm và xóa property trong các nhóm khi một view đang hiển thị
model; các view đi theo. Các con trỏ `Property*` thô chết cùng với việc xóa property hoặc với `setRoot()`; hãy giữ
đường dẫn nếu bạn cần tham chiếu tới một property về sau.

Các handler của `valueChanged`, `userValueChanged` và `batchValueChanged` có thể thay đổi cây: gọi `setRoot()` để dựng
lại nó, hoặc xóa nhóm hay property vừa được ghi. Lời gọi đã ghi giá trị trả về bình thường, và các handler được kết nối
sau đó vẫn nhận các giá trị của thay đổi ấy. Một phần của cây mà handler tháo ra vẫn sống cho đến khi thay đổi giá trị
ngoài cùng đã được chuyển đi xong; ngoài một handler như vậy, `setRoot()` và `remove()` hủy ngay lập tức. Điều này cũng
đúng khi một lần đặt lại hoặc `qpb::serialization::load()` / `fromJson()` đang ghi giá trị. Các lệnh "Reset to default"
và "Reset group" của các view dừng ghi khi một handler đã thay thế cây và bỏ qua các property đã bị xóa. Một lần đặt lại
trong code (`resetToDefault()` của một nhóm, `PropertyModel::resetAllToDefault()`) và các hàm nạp tiếp tục trên các
property mà chúng đã bắt đầu; những property mà một handler đã tháo ra vẫn được ghi, nhưng không có thông báo.

Một handler của `valueChanged`, `userValueChanged`, `batchValueChanged`, `validationFailed` hoặc `dataChanged` cũng có
thể xóa `PropertyModel`, kể cả trong lúc một nhóm đang được đặt lại (`resetToDefault()` của một nhóm hoặc của model,
`resetAllToDefault()`), chuẩn hóa (`normalizeValue()`, `normalizeAllValues()`), biến thành mặc định
(`makeValueDefault()`, `makeAllValuesDefault()`) hoặc nạp (`qpb::serialization::load()` / `fromJson()`), và trong lúc
một view đặt lại một nhóm. Với `dataChanged`, điều này còn bao gồm `PropertyGroup::add()` và `remove()`, việc đặt hoặc
xóa `enabledWhen` / `visibleWhen`, và các setter trình bày (`setDisplayName()`, `setToolTip()`, `setAttribute()`,
`setFlags()` và những hàm tương tự). Lời gọi hoặc lượt duyệt kết thúc trên các property mà nó đã bắt đầu, không có thêm
thông báo nào và không đụng đến model đã bị xóa, và cây bị hủy khi lời gọi ngoài cùng đã trả về. Việc đặt lại của một
view dừng ghi. Khi một handler đã xóa model, tham chiếu do `add()` trả về (và builder do các hàm `addXxx()` trả về) cùng
mọi con trỏ vào model đã bị xóa là không hợp lệ sau khi lời gọi trả về.

Xóa model từ một handler của các signal cấu trúc của nó (hàng được thêm hoặc xóa, reset model) không được hỗ trợ:
`QAbstractItemModel` vẫn dùng model sau khi phát chúng.

Một handler của `dataChanged` không nên tự thay đổi cây (`setRoot()`, `add()`, `remove()`): các receiver được kết nối
sau nó, như view, proxy model và delegate, sẽ nhận các index của những hàng đã đổi vị trí. Để thay đổi cây khi phản ứng
với `dataChanged`, hãy hoãn thay đổi đến khi việc phát signal kết thúc bằng cách đưa một lời gọi vào event loop
(`QMetaObject::invokeMethod(..., Qt::QueuedConnection)` hoặc `QTimer::singleShot(0, ...)`), và khi đó tìm các property
theo đường dẫn.

## Phụ thuộc giữa các property

Một property có thể được enable hoặc hiển thị tùy theo giá trị của một property khác, được xác định bằng đường dẫn của
nó từ gốc:

```cpp
general.addBool("autosave", true);
general.addInt("autosaveMinutes", 5).enabledWhen("General/autosave");              // true / non-empty
camera.addDouble("orthoScale", 1.0).visibleWhen("Camera/projection", 1);            // equals a value
limits.addInt64("quota", 0).enabledWhen("Limits/mode", [](const QVariant& v) { return v != "unlimited"; });
```

`enabledWhen()` và `visibleWhen()` (từ 1.4) có trên mọi builder và dưới dạng `Property::setEnabledWhen()` /
`setVisibleWhen()`, với ba dạng:

| Dạng | Điều kiện được thỏa khi giá trị nguồn |
|---|---|
| `enabledWhen(path)` | là "true": một `bool` true, một số khác không, một chuỗi không trống; với các kiểu khác, hợp lệ và không null |
| `enabledWhen(path, value)` | bằng `value` (một overload `int` giúp một literal `0`, chẳng hạn chỉ số enum, không bị nhầm với dạng predicate) |
| `enabledWhen(path, condition)` | làm cho predicate `Property::Condition` trả về true |

Một property có nhiều nhất một điều kiện cho mỗi loại (enabled, visible); đặt điều kiện khác sẽ thay thế nó. Xóa một
điều kiện bằng `clearEnabledWhen()` / `clearVisibleWhen()`.

- Model đánh giá các điều kiện khi một giá trị nguồn thay đổi, khi property được thêm hoặc xóa, khi một điều kiện được
  đặt, và khi một cây được gắn bằng `setRoot()`. Các view không cần code: một property không enabled thì không thể
  chỉnh sửa, và một property không visible thì bị ẩn.
- Kết quả được kết hợp với các flag riêng của property, nên một điều kiện không bao giờ ghi đè `setEnabled()` hay
  `setVisible()`: một property là enabled khi nó không `Disabled`, điều kiện của nó được thỏa và các tổ tiên của nó
  enabled. `isEnabled()` và `isVisible()` cho trạng thái hiệu lực này.
- Một điều kiện được tính là thỏa khi đường dẫn nguồn không tồn tại, và cũng thỏa với một property nằm ngoài
  model. Khi một cây được gắn vào model và một đường dẫn nguồn bị thiếu, một cảnh báo được ghi một lần.
- Điều kiện của các property hiển thị từ một `QObject` được đặt bằng class info; xem
  [Property của QObject](qobject-source.md).

## Giá trị live

Một property do ứng dụng duy trì (một bộ đếm, một trạng thái, dung lượng đã dùng) không phải là một cài đặt. Hãy đánh dấu
nó `live()` (từ 1.3):

```cpp
status.addInt64("bytesUsed", 0).readOnly().live();
```

- Nó không bao giờ hiển thị là đã sửa, nên các view không bao giờ hiển thị nó bằng chữ đậm.
- Việc đặt lại một nhóm, cả cây và "Reset group" bỏ qua nó. `resetToDefault()` gọi trên chính property live đó vẫn đặt
  lại nó.
- `qpb::serialization` không ghi cũng không đọc nó.
- Nó vẫn chỉnh sửa được trừ khi nó cũng read-only; flag được các phần tử con của một nhóm live thừa hưởng, và
  `isLive()` cho trạng thái hiệu lực.
- Chương trình của bạn cập nhật nó bằng `setValue()`, hàm này cũng hoạt động trên các property read-only.
