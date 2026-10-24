# Kiểu và editor tùy biến

> Đây là bản dịch tiếng Việt của tài liệu qpb 1.9.0. Bản tiếng Anh
> ([../../guide/custom-types-and-editors.md](../../guide/custom-types-and-editors.md)) là bản chuẩn; khi hai bản khác
> nhau thì theo bản tiếng Anh.

Thêm các kiểu property của riêng bạn (một màu, một vector, một enumeration của miền nghiệp vụ), chọn editor khác cho một
property, và viết editor. Quay lại [mục lục tài liệu](../README.md#tài-liệu).

Một kiểu có hai nửa. **Type handler** (`TypeRegistry`, trong `qpb::core`) cho biết một giá trị được chuyển đổi, hiển thị,
xác thực và lưu như thế nào. Nó không biết gì về widget. **Editor handler** (`EditorFactory`, trong `qpb::widgets`) cho
biết widget nào chỉnh sửa giá trị. Các view không bao giờ tự nhìn vào type id; mọi thứ phụ thuộc kiểu đều đi qua hai
registry này.

## Kiểu của riêng bạn

```cpp
qpb::TypeHandler color;
color.displayText = [](const QVariant& v, const qpb::Property&) { return v.value<QColor>().name(); };
color.toJson = [](const QVariant& v, const qpb::Property&) { return QJsonValue(v.value<QColor>().name()); };
qpb::TypeRegistry::global().registerType<QColor>("color", color);
root->add("color", "tint", QColor(Qt::red));
```

`TypeHandler` được cấu hình bằng cách gán các field; tất cả đều tùy chọn, và một field để trống nghĩa là hành vi mặc
định:

| Field | Mục đích | Khi để trống |
|---|---|---|
| `storageType` | Kiểu mà mọi giá trị được chuyển đổi sang trước khi lưu. `registerType<T>()` đặt nó thành `T` | Giá trị được lưu như được đưa vào |
| `displayText(value, property)` | Văn bản hiển thị cho một giá trị không đang được chỉnh sửa | `QVariant::toString()` |
| `normalize(value, property)` | Điều chỉnh một giá trị đã chuyển đổi trước khi xác thực, ví dụ clamp | Giá trị được giữ nguyên |
| `validate(value, property)` | Chấp nhận hoặc từ chối một giá trị đã chuẩn hóa, trả về một `ValidationResult` | Mọi giá trị đều được chấp nhận |
| `toJson(value, property)` | Dạng JSON của một giá trị cho `qpb::serialization::toJson()` (từ 1.2) | `QJsonValue::fromVariant()` |
| `fromJson(json, property)` | Giá trị cho JSON được `qpb::serialization::fromJson()` đọc, sau đó được đặt bằng `setValue()` (từ 1.2) | `QJsonValue::toVariant()` |

Ghi chú:

- Hãy dùng tiền tố trong các id của bạn (`"myapp.color"`). Các id bắt đầu bằng `qpb.` và id của các kiểu có sẵn là dành
  riêng.
- `registerType()` trả về `false`, không thay đổi gì, nếu id đã được đăng ký hoặc là id dành riêng. `replaceType()` thay
  thế handler của một kiểu đã được đăng ký, kể cả các kiểu có sẵn, và trả về `false` trong trường hợp ngược lại.
  `contains()`, `handler()` và `types()` dùng để tra cứu các kiểu.
- Hãy đăng ký kiểu trên main thread trước khi bạn tạo các property dùng chúng. Registry không thread-safe.
- Một property có kiểu chưa được đăng ký được hiển thị read-only dưới dạng văn bản thuần, kèm một cảnh báo trong log. Nó
  bắt đầu hoạt động bình thường khi kiểu được đăng ký. `Property::create(type, id, value)` tạo một property như vậy mà
  không cần kiểu phải tồn tại sẵn.
- Các attribute tùy biến (`attribute("myapp.something", value)`) bị qpb bỏ qua và được chuyển cho các handler của bạn
  thông qua property.
- Với Qt 5, `TypeHandler::storageType` là một type id `int` thay vì `QMetaType`; `registerType<T>()` hoạt động như nhau
  ở cả hai. Xem [Tương thích](../compatibility.md#qt-5-và-qt-6).

## Editor cho kiểu của bạn

Một `EditorHandler` có năm hàm:

| Field | Mục đích | Bắt buộc |
|---|---|---|
| `createEditor(parent, property)` | Dựng widget làm con của `parent` | có |
| `setEditorData(editor, value, property)` | Hiển thị một giá trị trong widget | có |
| `editorData(editor, property)` | Trả về giá trị mà người dùng đã nhập | có |
| `applyAttributes(editor, property)` | Áp dụng các khoảng giá trị và tương tự; được gọi sau `createEditor` và mỗi khi attribute thay đổi | không |
| `paint(painter, option, value, property)` | Vẽ ô giá trị khi nó không đang được chỉnh sửa (chẳng hạn một ô mẫu màu); khi để trống thì văn bản hiển thị được vẽ | không |

```cpp
qpb::EditorHandler slider;
slider.createEditor = [](QWidget* parent, const qpb::Property&) -> QWidget* {
    return new QSlider(Qt::Horizontal, parent);
};
slider.setEditorData = [](QWidget* editor, const QVariant& v, const qpb::Property&) {
    static_cast<QSlider*>(editor)->setValue(v.toInt());
};
slider.editorData = [](QWidget* editor, const qpb::Property&) {
    return QVariant(static_cast<QSlider*>(editor)->value());
};
slider.applyAttributes = [](QWidget* editor, const qpb::Property& p) {
    static_cast<QSlider*>(editor)->setRange(p.attribute(qpb::Attr::Minimum, 0).toInt(),
                                            p.attribute(qpb::Attr::Maximum, 100).toInt());
};
qpb::EditorFactory::global().registerEditor("myapp.slider", slider);
```

`registerEditor()` trả về `false`, không thay đổi gì, nếu id đã được đăng ký hoặc một field bắt buộc bị trống;
`replaceEditor()` thay thế một editor đã đăng ký, kể cả các editor có sẵn. `contains()`, `handler()` và `editors()` dùng
để tra cứu editor. Hãy dùng factory từ GUI thread.

**Một editor cho một kiểu.** Đăng ký editor dưới id của kiểu: `registerEditor("color", handler)`.

**Một editor khác cho một property.** Đăng ký editor dưới một id riêng và nêu tên nó trong builder:

```cpp
root->addInt("volume", 50).range(0, 100).editor("myapp.slider");
```

Việc này đặt attribute `Attr::EditorId`. Property vẫn giữ kiểu của nó, nên việc lưu trữ và xác thực vẫn theo type id
của nó. Factory tìm editor được nêu bởi `EditorId` trước, rồi đến editor được đăng ký dưới type id
(`handlerFor(property)`); một property mà cả hai đều không có thì không có editor và hiển thị văn bản của nó.

### Commit và hộp thoại

- Các view commit một editor khi nó mất focus. Với những editor mà một lựa chọn là cuối cùng (một mục trong danh sách,
  một tệp đã chọn, một màu), hãy gọi `qpb::EditorFactory::notifyCommit(editor)` khi người dùng đã chọn, để giá trị được
  commit ngay.
- `qpb::EditorFactory::notifyCommitKeepOpen(editor)` (từ 1.9) cũng commit ngay, nhưng editor vẫn mở, dành cho những
  editor mà người dùng tiếp tục làm việc sau mỗi thay đổi (một slider, một bước của spin box). Khi đó tree view ghi giá
  trị mà model đang giữ ngược trở lại vào editor: model có thể đã clamp hoặc chuẩn hóa dữ liệu nhập, hoặc đã từ chối nó
  (thông điệp hiện dưới dạng tooltip). Form view commit như với `notifyCommit()`. Hãy gọi nó từ slot của chính editor, ví
  dụ `connect(slider, &QSlider::valueChanged, this, [this] { qpb::EditorFactory::notifyCommitKeepOpen(this); })`.
- Khi một editor đang hiển thị một hộp thoại, hãy giữ một `qpb::EditorDialogScope` còn sống. Hộp thoại lấy focus; không
  có scope này, view sẽ commit và đóng editor giữa chừng:

```cpp
void ColorButton::pickColor()
{
    qpb::EditorDialogScope scope(this);
    const QColor color = QColorDialog::getColor(m_color, this);
    // ...
}
```

## Editor có sẵn

| Kiểu | Widget | Ghi chú |
|---|---|---|
| Bool | check box | Trong tree view và list view, chỉ báo check của ô được dùng và không có widget editor; Space hoặc một cú click bật/tắt nó. Form view dùng một `QCheckBox` |
| Int | `QSpinBox` | Áp dụng minimum, maximum, step, tiền tố và hậu tố; commit khi việc chỉnh sửa kết thúc, không phải ở mỗi phím. Với `Attr::CommitOnStep` (từ 1.9), mỗi bước (nút mũi tên, phím mũi tên, con lăn, Page Up / Down) được commit ngay và editor vẫn mở; văn bản gõ vào vẫn chờ Enter hoặc mất focus |
| Int64 | một spin box cho giá trị 64 bit | Như Int; các bước dừng ở giới hạn |
| Double | `QDoubleSpinBox` | Như Int, thêm số chữ số thập phân; một ô không đang được chỉnh sửa hiển thị nhiều nhất bấy nhiêu chữ số thập phân, không có số 0 ở cuối. Với `Attr::RoundToDecimals` là false, giá trị được lưu giữ nguyên các chữ số của nó và một editor mà người dùng không đổi sẽ ghi nó ngược lại nguyên như cũ |
| String | `QLineEdit`; `QPlainTextEdit` với `multiline` | Editor một dòng giới hạn việc gõ ngay lập tức theo độ dài tối đa và biểu thức chính quy. Editor nhiều dòng kiểm tra cả hai khi giá trị được commit; nó cao ít nhất bốn dòng, Tab chuyển tiếp, Enter chèn một dấu ngắt dòng và Ctrl+Enter commit (xem [View](views.md#bàn-phím)) |
| Enum | `QComboBox` không chỉnh sửa được | Commit khi một mục được chọn |
| FilePath | line edit với nút "..." | Nút mở hộp thoại mở hoặc lưu tùy theo `DialogMode`; chọn một tệp thì commit |
| DirPath | line edit với nút "..." | Nút mở hộp thoại chọn thư mục |

Các giá trị dài trong một ô không đang được chỉnh sửa được rút gọn (elide) ở giữa. Tooltip của ô giá trị là tooltip của
property nếu nó có, và nếu không thì là văn bản hiển thị, nên một đường dẫn dài hiện đầy đủ.
