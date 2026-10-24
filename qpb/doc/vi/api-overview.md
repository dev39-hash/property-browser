# Tổng quan API

> Đây là bản dịch tiếng Việt của tài liệu qpb 1.9.0. Bản tiếng Anh ([../api-overview.md](../api-overview.md)) là bản
> chuẩn; khi hai bản khác nhau thì theo bản tiếng Anh.

Các lớp public theo từng header. Các header trong `include/qpb/` là tài liệu tham chiếu đầy đủ và chính xác (mỗi khai
báo đều có chú thích); trang này cho bạn biết nên tìm ở đâu và các phần ăn khớp với nhau như thế nào. Quay lại
[mục lục tài liệu](README.md#tài-liệu).

Mọi tên đều nằm trong namespace `qpb`. Include `<qpb/qpb.h>` cho tất cả (cần `qpb::widgets`) hoặc `<qpb/qpbcore.h>` chỉ
cho `qpb::core`; hoặc include từng header riêng: `<qpb/Name.h>` cho mỗi lớp, widget nằm trong `<qpb/widgets/Name.h>`.
Chỉ các header này là public; mọi thứ trong `qpb::detail` và dưới `src/` là nội bộ.

| Header | Khai báo | Thư viện |
|---|---|---|
| `Types.h` | `TypeId`, các hằng `Types::` | core |
| `Attributes.h` | các khóa `Attr::`, `EnumOption`, `FileMode` | core |
| `Property.h` | `Property` | core |
| `PropertyGroup.h` | `PropertyGroup` | core |
| `PropertyBuilders.h` | `BoolBuilder`, `IntBuilder`, `Int64Builder`, `DoubleBuilder`, `StringBuilder`, `EnumBuilder`, `FilePathBuilder`, `DirPathBuilder` | core |
| `PropertyModel.h` | `PropertyModel` | core |
| `ValidationResult.h` | `ValidationResult` | core |
| `TypeRegistry.h` | `TypeRegistry`, `TypeHandler` | core |
| `PropertyFilterProxyModel.h` | `PropertyFilterProxyModel` | core |
| `QObjectPropertySource.h` | `QObjectPropertySource` | core |
| `Serialization.h` | `qpb::serialization::toJson`, `fromJson`, `save`, `load` | core |
| `Translations.h` | `loadTranslation()` | core |
| `qpbglobal.h` | macro phiên bản, `version()`, macro deprecation | core |
| `widgets/PropertyTreeView.h` | `PropertyTreeView` | widgets |
| `widgets/PropertyFormView.h` | `PropertyFormView` | widgets |
| `widgets/PropertyDelegate.h` | `PropertyDelegate` | widgets |
| `widgets/EditorFactory.h` | `EditorFactory`, `EditorHandler`, `EditorDialogScope` | widgets |

## Kiểu và attribute (`Types.h`, `Attributes.h`)

| Type id (`Types::...`) | Lưu dưới dạng | Builder | Editor mặc định |
|---|---|---|---|
| `Bool` | `bool` | `addBool` | check box (trong tree: chỉ báo check của ô) |
| `Int` | `int` | `addInt` -> `IntBuilder` | spin box |
| `Int64` | `qint64` | `addInt64` -> `Int64Builder` | spin box 64 bit |
| `Double` | `double` | `addDouble` -> `DoubleBuilder` | double spin box |
| `String` | `QString` | `addString` -> `StringBuilder` | line edit; text edit nhiều dòng với `multiline()` |
| `Enum` | chỉ số `int`, hoặc giá trị của option | `addEnum` -> `EnumBuilder` | combo box |
| `FilePath`, `DirPath` | `QString` | `addFilePath`, `addDirPath` | line edit với nút "..." mở một hộp thoại |
| `Group` | - | `addGroup` | - |

`TypeId` là một `QString`. Các hằng `Types::` chuyển đổi được sang nó. Các kiểu của riêng bạn dùng id của riêng chúng;
một tiền tố như `"myapp.color"` giúp tránh va chạm. Các id bắt đầu bằng `qpb.` và chín id ở trên là dành riêng.

Attribute được lưu theo từng property và tinh chỉnh việc xác thực, hiển thị và editor. Các khóa mà một kiểu không biết sẽ
bị bỏ qua, nên các kiểu của riêng bạn có thể định nghĩa khóa riêng (hãy dùng tiền tố như `"myapp."`). Các khóa chuẩn nằm
trong `Attr::`:

| Khóa (`Attr::...`) | Kiểu | Giá trị | Mặc định |
|---|---|---|---|
| `Minimum`, `Maximum` | Int, Int64, Double | cùng kiểu với giá trị; giá trị bị clamp | toàn bộ khoảng của kiểu; không có với Double |
| `Step` | Int, Int64, Double | bước của spin box | 1 (1.0) |
| `Decimals` | Double | số chữ số thập phân (0 đến 15); giá trị được làm tròn theo đó trừ khi `RoundToDecimals` là `false` | 2 |
| `RoundToDecimals` | Double | `bool`; `false` lưu giá trị không làm tròn (vẫn bị clamp) và chỉ dùng `Decimals` để hiển thị và cho editor (từ 1.9) | `true` |
| `Prefix`, `Suffix` | Int, Int64, Double | `QString` hiển thị trước / sau con số | không có |
| `CommitOnStep` | Int, Int64, Double | `bool`; mỗi bước của spin box được commit và editor vẫn mở (từ 1.9) | `false` |
| `MaxLength` | String | số ký tự tối đa | không giới hạn |
| `Placeholder` | String | văn bản placeholder của editor | không có |
| `RegularExpression` | String | mẫu mà toàn bộ giá trị phải khớp (cú pháp `QRegularExpression`) | không có |
| `Multiline` | String | `bool`; nhiều dòng văn bản (từ 1.1) | `false` |
| `Options` | Enum | `QList<EnumOption>` | - |
| `Filter` | FilePath | bộ lọc tên của hộp thoại tệp, ví dụ `"Images (*.png *.jpg)"` | không có |
| `DialogMode` | FilePath | một `int` chứa một `FileMode` (`Open`, `Save`) | `Open` |
| `DefaultDir` | FilePath, DirPath | thư mục mà hộp thoại bắt đầu khi giá trị trống | không có |
| `MustExist` | FilePath, DirPath | `bool`; một giá trị không trống phải là tên của một tệp / thư mục đang tồn tại | `false` |
| `EditorId` | bất kỳ | id của editor dùng cho property này thay cho editor của kiểu | không có |

Các option của enum là `EnumOption { label, value }`; value là một `int` hoặc một `QString`. `addEnum(id, QStringList
labels, index)` tạo các option có giá trị là chỉ số của chúng và lưu chỉ số; `addEnum(id, QList<EnumOption>, value)` lưu
giá trị của option được chọn.

## Property (`Property.h`, `PropertyGroup.h`, `PropertyBuilders.h`)

`PropertyGroup::create(id)` tạo một nhóm gốc tách rời (detached); đưa nó cho một `PropertyModel`. Một property thuộc sở
hữu của nhóm cha của nó, còn gốc thuộc sở hữu của model. `Property` không phải là `QObject` và không được thiết kế để kế
thừa. Con trỏ tới property còn hợp lệ cho đến khi property bị xóa hoặc cây của model bị thay thế.

**Dựng một cây** (`PropertyGroup`):

- `addGroup(id)` trả về `PropertyGroup&` mới. `addBool`, `addInt`, `addInt64`, `addDouble`, `addString`, `addEnum`,
  `addFilePath` và `addDirPath` trả về một builder. `add(typeId, id, value)` thêm một property thuộc kiểu bất kỳ, và
  `add(std::unique_ptr<Property>)` nhận một property được tạo bằng `Property::create(typeId, id, value)`.
- Giá trị ban đầu cũng là giá trị mặc định.
- Id không được trống và không bao giờ chứa `/`; một id trống trở thành `property` và mỗi `/` trở thành `_`, kèm một
  cảnh báo. Thêm một id đã tồn tại trong nhóm thì không thêm gì, ghi một cảnh báo và trả về phần tử con đang có (với
  `addGroup`, khi phần tử con đó không phải là nhóm, nhóm mới nhận id trống đầu tiên `<id>_2`, `<id>_3`, ...). Cây luôn
  nhất quán; không có gì assert.
- Truy cập: `remove(id)` (trả về false nếu không có phần tử con như vậy), `child(id)`, `child(index)`, `children()`,
  `childCount()`, `indexOf(child)`, `find("Transform/x")` (một đường dẫn tương đối so với nhóm).

**Builder** là các handle nhẹ; mọi setter đều trả về chính builder, và một builder chuyển đổi được sang `Property&`. Hãy
giữ `Property&`, đừng giữ builder. Các setter không áp dụng cho một kiểu thì không có trên builder của kiểu đó, nên một
sai sót sẽ không biên dịch được.

| Builder | Các setter theo kiểu |
|---|---|
| tất cả | `displayName`, `toolTip`, `readOnly`, `enabled`, `visible`, `live`, `enabledWhen`, `visibleWhen`, `validator`, `attribute(key, value)`, `editor(editorId)` |
| `IntBuilder`, `Int64Builder` | `range`, `minimum`, `maximum`, `step`, `prefix`, `suffix` |
| `DoubleBuilder` | như trên, thêm `decimals` |
| `StringBuilder` | `maxLength`, `placeholder`, `regularExpression`, `multiline` |
| `FilePathBuilder` | `filter`, `dialogMode`, `defaultDir`, `mustExist` |
| `DirPathBuilder` | `defaultDir`, `mustExist` |
| `BoolBuilder`, `EnumBuilder` | chỉ các setter chung |

**`Property`:**

- Định danh: `id()`, `path()` (các id nối bằng `/`, trống với gốc), `typeId()`, `isGroup()`, `parent()` (null với một
  property gốc hoặc tách rời), `toGroup()` (null khi không phải là nhóm).
- Trình bày: `displayName()` (mặc định là id), `toolTip()` và các setter của chúng.
- Giá trị: `value()` (không hợp lệ với nhóm), `setValue()`, `defaultValue()`, `setDefaultValue()`, `isModified()`,
  `resetToDefault()`, `normalizeValue()` và `makeValueDefault()` (cả hai từ 1.9), `validator()` / `setValidator()`.
  Xem [Giá trị và xác thực](guide/values-and-validation.md).
- Attribute: `attributes()`, `attribute(key, default)`, `hasAttribute()`, `setAttribute()`, `removeAttribute()`.
- Trạng thái: `flags()`, `setFlags()`, `setFlag(flag, on)` với `Property::Flag::ReadOnly`, `Disabled`, `Hidden`, `Live`;
  các setter `setReadOnly`, `setEnabled`, `setVisible`, `setLive`. Đây là các flag của riêng property. Trạng thái hiệu
  lực còn phụ thuộc vào các property tổ tiên (và các điều kiện): `isReadOnly()`, `isEnabled()`, `isVisible()`,
  `isLive()`.
- Điều kiện: `setEnabledWhen()` / `setVisibleWhen()` ở bốn dạng, `clearEnabledWhen()`, `clearVisibleWhen()`;
  `Property::Condition` là một `std::function<bool(const QVariant&)>`. Xem
  [Thay đổi và điều kiện](guide/changes-and-conditions.md).
- Một setter trên property thuộc về một model sẽ cập nhật model và các view của nó.

## Model (`PropertyModel.h`)

Một `QAbstractItemModel` có hai cột, `NameColumn` và `ValueColumn`. Mọi view hoặc proxy model của Qt đều hiển thị được
nó; các view của riêng qpb thêm editor và cách vẽ riêng cho property. Model sở hữu nhóm gốc. Các property ẩn vẫn nằm
trong model (các view ẩn hàng của chúng), nên các index ổn định và các proxy hoạt động bình thường.

- Dựng và cây: `PropertyModel(std::unique_ptr<PropertyGroup> root, parent)` (hoặc không có gốc); `setRoot()` thay thế
  cây (con trỏ vào cây cũ trở nên không hợp lệ); `root()`, `find(path)`, `propertyAt(index)`,
  `indexOf(property, column)`.
- Giá trị: `setValue(path, value)` cho code ứng dụng; `setData()` cho các view, cũng từ chối các property read-only và
  disabled; `resetToDefault(index)`; `resetAllToDefault()` (từ 1.5); `normalizeAllValues()` và
  `makeAllValuesDefault()` (từ 1.9).
- Batch: `beginBatch()` / `endBatch()`, có thể lồng nhau.
- Signal: `valueChanged(path, newValue, oldValue)`, `userValueChanged(path, newValue, oldValue)` (từ 1.9, chỉ cho các
  chỉnh sửa của người dùng), `validationFailed(path, rejectedValue, message)`, `batchValueChanged(paths)`. Chúng mang
  theo đường dẫn, không phải tham chiếu `Property`.
- `onValueChanged(path, context, handler)` (từ 1.4): một callback cho một đường dẫn, hoặc cho một nhóm và các phần tử
  con cháu của nó (khi đó handler cũng nhận đường dẫn). `onUserValueChanged()` (từ 1.9) có cùng hai dạng cho
  `userValueChanged`.
- Role: `PropertyRole` (`const Property*`), `TypeIdRole`, `PathRole`, `IsGroupRole`, `IsModifiedRole`,
  `AttributesRole`, `IsVisibleRole`. Chúng hoạt động trên cả hai cột. Các role từ `Qt::UserRole + 1` đến
  `Qt::UserRole + 99` được dành riêng cho qpb; role của riêng bạn bắt đầu từ `PropertyModel::UserRole`.

| Cột | Role | Giá trị |
|---|---|---|
| Name | `Qt::DisplayRole`, `Qt::ToolTipRole` | tên hiển thị, tooltip |
| Value | `Qt::DisplayRole` | văn bản hiển thị của kiểu (Bool hiển thị trạng thái check thay vào đó) |
| Value | `Qt::EditRole` | giá trị |
| Value | `Qt::CheckStateRole` | chỉ Bool: được check hoặc không |

Item flag: một item được enable theo `isEnabled()`; ô giá trị chỉnh sửa được (hoặc check được với Bool) khi property được
enable, không read-only và kiểu của nó đã được đăng ký. Nhóm không bao giờ chỉnh sửa được. Tiêu đề các cột là "Property"
và "Value" (đã được dịch).

## Các lớp core khác

- `ValidationResult` (`ValidationResult.h`): `ValidationResult::valid()`, `ValidationResult::error(message)`, `ok`,
  `message`. Chuyển đổi sang `bool` một cách tường minh.
- `TypeRegistry::global()` và `TypeHandler` (`TypeRegistry.h`): `registerType(id, handler)`,
  `registerType<T>(id, handler)` (kiểu lưu trữ `T`), `replaceType()`, `handler()`, `contains()`, `types()`. Các field của
  `TypeHandler`, tất cả đều tùy chọn: `storageType`, `displayText`, `normalize`, `validate`, `toJson`, `fromJson`. Xem
  [Kiểu và editor tùy biến](guide/custom-types-and-editors.md).
- `PropertyFilterProxyModel` (`PropertyFilterProxyModel.h`, từ 1.1): lọc theo tên hiển thị cho một ô tìm kiếm. Xem
  [View](guide/views.md).
- `QObjectPropertySource(model)` (`QObjectPropertySource.h`, từ 1.2): `addObject(object, parentGroup, id)`,
  `removeObject()`, `groupOf()`, `objects()`, `refresh()`, `model()`, `setTitleProperty()` / `titleProperty()`,
  `setLiveReadOnlyProperties()` / `liveReadOnlyProperties()`. Xem [Property của QObject](guide/qobject-source.md).
- `qpb::serialization` (`Serialization.h`, từ 1.2): `toJson(group)`, `fromJson(group, json)`, `save(group, settings)`,
  `load(group, settings)`. Xem [Lưu và nạp](guide/serialization.md).
- `loadTranslation(translator, locale)` (`Translations.h`, từ 1.8). Xem [Bản dịch](guide/translations.md).
- `qpbglobal.h`: `QPB_VERSION`, `QPB_VERSION_CHECK(major, minor, patch)`, `QPB_VERSION_STR`, `QPB_VERSION_MAJOR` /
  `_MINOR` / `_PATCH`, `qpb::version()`, `QPB_DEPRECATED` và `QPB_DEPRECATED_X(text)`. Xem
  [Tương thích](compatibility.md).

## Widgets

- `PropertyTreeView` (một `QTreeView`): `setModel()` (một `PropertyModel` hoặc một proxy của nó), `mode()` /
  `setMode(Mode::Tree | Mode::List)`, `nameColumnWidth()` / `setNameColumnWidth()`, `propertyDelegate()`,
  `tabStopsOnCheckBoxes()` / `setTabStopsOnCheckBoxes()` (từ 1.5), và các property màu cho style sheet
  (`groupBackground`, `groupForeground`, `modifiedForeground`, `readOnlyForeground`; từ 1.6). `mode`,
  `tabStopsOnCheckBoxes` và các màu đều là `Q_PROPERTY`.
- `PropertyFormView` (một `QScrollArea`, từ 1.1): `setModel()`, `model()`, `editor(path)`, `isExpanded(groupPath)`,
  `setExpanded()`, và `showModelForeground()` / `setShowModelForeground()` (từ 1.9; một `Q_PROPERTY`, mặc định tắt):
  nhãn và tiêu đề section hiển thị màu `Qt::ForegroundRole` của model.
- `PropertyDelegate`: delegate của tree view (các chế độ Tree và List); đặt nó trên bất kỳ `QAbstractItemView` nào đang
  hiển thị một `PropertyModel`. `PropertyFormView` có editor riêng.
- `EditorFactory::global()`: `registerEditor(id, handler)`, `replaceEditor()`, `handler()`, `handlerFor(property)`,
  `createEditor(parent, property)`, `contains()`, `editors()`, và các hàm static `notifyCommit(editor)` và
  `notifyCommitKeepOpen(editor)` (từ 1.9). Các field của `EditorHandler`: `createEditor`, `setEditorData`, `editorData`
  (bắt buộc), `applyAttributes`, `paint` (tùy chọn).
- `EditorDialogScope`: giữ một editor mở trong lúc nó hiển thị một hộp thoại.

Xem [View](guide/views.md) để biết các view hoạt động ra sao và
[Kiểu và editor tùy biến](guide/custom-types-and-editors.md) để biết về factory.
