# qpb — Property Browser cho Qt 6: Đặc tả kỹ thuật (1.0 → 1.2)

> Nguồn: [`brainstorm.md`](brainstorm.md) (hướng B — *Explicit model + pluggable views/types*).
> Trạng thái: **Draft 2** — thêm mục tiêu ổn định API (§9) và phân phối dạng component folder (§6). Các mục đánh dấu **[Giả định]** lấy từ mục 13 của brainstorm,
> chưa được xác nhận; đổi giả định nào thì cập nhật mục đó và [Phụ lục B](#phụ-lục-b--nhật-ký-quyết-định).

---

## 1. Mục tiêu & phi mục tiêu

### 1.1 Mục tiêu

- **G1.** Lập trình viên *khai báo* một cây thuộc tính một lần (builder API) và nhận panel chỉnh sửa hoàn chỉnh.
- **G2.** Một nguồn dữ liệu (`PropertyModel`, chuẩn `QAbstractItemModel`), nhiều cách hiển thị: **Tree**, **List**, **Form**.
- **G3.** Mở rộng kiểu dữ liệu và editor **từ phía người dùng**, không sửa thư viện.
- **G4.** Tích hợp nhanh: panel 10 thuộc tính ≤ 30 dòng code; ≤ 30 phút kể cả CMake.
- **G5. API cố định.** Từ bản 1.0, nâng cấp thư viện **không bắt project đang dùng phải sửa code**
  (trong cùng major version). Chính sách chi tiết ở §9.
- **G6. Phân phối dạng component folder.** Project dùng thư viện chép folder `qpb/` vào
  (vd. `components/qpb/`), thêm 2 dòng CMake. Cập nhật = thay folder + build lại. Chi tiết ở §6.
- **G7. Viết mới hoàn toàn.** Không wrap, không fork, không chép code từ QtPropertyBrowser/QtnProperty.

### 1.2 Phi mục tiêu (không làm trong 1.x)

Qt Quick/QML view · Python binding · multi-object editing · undo/redo tích hợp sẵn ·
kiểu phức hợp trong core (color, font, vector, array) · Qt 5 · DSL/code generator ·
theme riêng (dùng QSS) · conditional visibility khai báo bằng biểu thức.

Lý do chi tiết: brainstorm mục 11. Thiết kế **không được cấm** multi-object editing và QML về sau
(→ core không phụ thuộc QtWidgets, `Property` không giả định có đúng một object nguồn).

### 1.3 Tiêu chí chấp nhận cấp dự án

| #  | Tiêu chí                                                         | Kiểm chứng bằng                                        |
|----|------------------------------------------------------------------|--------------------------------------------------------|
| S1 | Panel 10 thuộc tính ≤ 30 dòng                                     | `examples/quickstart/main.cpp` (đếm dòng, không tính include) |
| S2 | Kiểu `QColor` ≤ 100 dòng, nằm ngoài lib                           | `examples/custom_type/`                                |
| S3 | Chuyển Tree ↔ List ↔ Form không đổi model                         | `examples/inspector` có nút chuyển view + test         |
| S4 | Dùng trong ≥ 2 project thật trong 6 tháng sau 1.0                 | Theo dõi ngoài repo                                    |
| S5 | Nâng 1.x → 1.y không phải sửa code consumer                        | Bộ test tương thích API (§9.5) pass trên mọi bản 1.y  |
| S6 | Cập nhật thư viện = thay folder `components/qpb/` + build lại      | `tests/consumer` build lại sau khi thay folder, không đổi CMake của consumer |

---

## 2. Ràng buộc kỹ thuật

| Hạng mục        | Quyết định                                                                                   |
|-----------------|----------------------------------------------------------------------------------------------|
| Ngôn ngữ        | **C++17**. API public **không** dùng designated initializer (là C++20). [Giả định]           |
| Qt              | Tối thiểu **6.5**; môi trường phát triển/CI chính **6.8 LTS**. Chỉ dùng Core, Gui, Widgets, Test. [Giả định] |
| Build           | CMake ≥ 3.21; target `qpb::core`, `qpb::widgets`. Cách tích hợp **chính**: `add_subdirectory(components/qpb)` (§6) |
| Namespace       | `qpb` [Giả định]                                                                              |
| Thư viện        | **Static mặc định** (`QPB_BUILD_SHARED=OFF`), không theo `BUILD_SHARED_LIBS` của consumer; export macro `QPB_CORE_EXPORT`, `QPB_WIDGETS_EXPORT` vẫn có cho trường hợp shared |
| License         | MIT [Giả định]                                                                                |
| Nền tảng        | Linux, Windows (MSVC 2019+), macOS                                                            |
| Phụ thuộc       | `qpb::core` → Qt6::Core **duy nhất**. `qpb::widgets` → `qpb::core`, Qt6::Widgets. Không phụ thuộc thư viện bên thứ ba |
| Nâng yêu cầu    | Tăng C++ standard, Qt tối thiểu, CMake tối thiểu = **breaking change** (chỉ ở major mới)       |

---

## 3. Kiến trúc

```text
┌─────────────────────────────── qpb::widgets (Qt6::Widgets) ───────────────────────────────┐
│  PropertyTreeView ──┐                                                                     │
│   (mode Tree/List)  ├── PropertyDelegate ──┐                                              │
│                     │                      ├── EditorFactory  (typeId → EditorHandler)    │
│  PropertyFormView ──┴──────────────────────┘                                              │
├─────────────────────────────── qpb::core (chỉ Qt6::Core) ─────────────────────────────────┤
│  PropertyModel : QAbstractItemModel                                                       │
│  Property / PropertyGroup (cây dữ liệu)     TypeRegistry (typeId → TypeHandler)           │
│  Attributes (khóa chuẩn)   Validation                                                     │
└───────────────────────────────────────────────────────────────────────────────────────────┘
```

**Luật kiến trúc (bắt buộc, kiểm tra khi review):**

- **R1.** View không `switch`/`if` theo `typeId` hay `QMetaType`. Mọi hành vi phụ thuộc kiểu đi qua `TypeRegistry` hoặc `EditorFactory`.
- **R2.** `qpb::core` không include bất kỳ header nào của QtWidgets/QtGui (ngoại trừ `Qt::ItemDataRole` nằm trong QtCore).
- **R3.** Mọi thay đổi giá trị đến từ UI đều đi qua `PropertyModel::setData` (→ validation → signal). View không ghi thẳng vào `Property`.
- **R4.** Không public API nào chưa có ít nhất một nơi sử dụng (example hoặc test).
- **R5.** Chỉ header trong `qpb/include/qpb/` là public. Mọi thứ trong namespace `qpb::detail` hoặc
  thư mục `src/` là nội bộ, đổi tự do. Mọi thay đổi public header phải tuân §9.

---

## 4. Core (`qpb::core`)

### 4.1 Định danh kiểu (`TypeId`)

Kiểu được định danh bằng **ID logic** (chuỗi), không phải `QMetaType`, vì nhiều kiểu logic cùng
kiểu lưu trữ (`String`, `FilePath`, `DirPath` đều là `QString`).

```cpp
namespace qpb {
using TypeId = QString;
namespace Types {
inline const TypeId Bool     = QStringLiteral("bool");
inline const TypeId Int      = QStringLiteral("int");
inline const TypeId Double   = QStringLiteral("double");
inline const TypeId String   = QStringLiteral("string");
inline const TypeId Enum     = QStringLiteral("enum");
inline const TypeId FilePath = QStringLiteral("filepath");
inline const TypeId DirPath  = QStringLiteral("dirpath");
inline const TypeId Group    = QStringLiteral("group");
}
}
```

Kiểu do người dùng đăng ký dùng ID tự chọn; khuyến nghị có tiền tố (`"myapp.color"`) để tránh va chạm.
ID bắt đầu bằng `qpb.` hoặc trùng 8 ID ở trên là dành riêng.

### 4.2 `Property`

Nút trong cây. Không phải `QObject` (nhẹ, không cần moc). Được sở hữu bởi group cha
(`std::unique_ptr`); gốc được sở hữu bởi `PropertyModel` sau khi gắn vào model.
Dữ liệu nằm sau **d-pointer** (`std::unique_ptr<detail::PropertyPrivate>`), header chỉ có hàm (xem §9.3).
Không copy được; không có constructor public (tạo qua `PropertyGroup::add*` hoặc `Property::create`).

| Thành phần        | Kiểu                   | Ghi chú                                                                 |
|-------------------|------------------------|-------------------------------------------------------------------------|
| `id()`            | `QString`              | Bắt buộc, duy nhất trong cùng một group cha; không chứa `/`             |
| `path()`          | `QString`              | Nối các `id` từ con của gốc: `"Transform/x"`; gốc có path rỗng          |
| `displayName()`   | `QString`              | Mặc định = `id`                                                         |
| `typeId()`        | `TypeId`               | Bất biến sau khi tạo                                                    |
| `value()`         | `QVariant`             | Group: luôn invalid                                                     |
| `defaultValue()`  | `QVariant`             | Mặc định = giá trị lúc tạo                                              |
| `attributes()`    | `QVariantMap`          | Xem 4.4                                                                 |
| `toolTip()`       | `QString`              |                                                                         |
| `isReadOnly()`    | `bool`                 | Hiệu lực = bản thân **hoặc** bất kỳ tổ tiên nào readOnly                 |
| `isEnabled()`     | `bool`                 | Hiệu lực = bản thân **và** mọi tổ tiên enabled                           |
| `isVisible()`     | `bool`                 | Ẩn group ⇒ ẩn toàn bộ con                                               |
| `parent()`        | `PropertyGroup*`       | `nullptr` với gốc                                                       |
| `isModified()`    | `bool`                 | `value() != defaultValue()` (so sánh `QVariant`)                        |
| `validator`       | `std::function<ValidationResult(const QVariant&)>` | Tùy chọn, chạy sau validation của kiểu          |

Setter tương ứng (`setDisplayName`, `setToolTip`, `setReadOnly`, `setEnabled`, `setVisible`,
`setAttribute`, `setValidator`, `setDefaultValue`) **thông báo cho model** nếu property đã gắn vào model
(xem 4.6), để view cập nhật.

`setValue()` trên `Property` là **API cho code ứng dụng** (vd. nạp dữ liệu). Nó chạy qua cùng pipeline
validation + signal như `PropertyModel::setData` (R3), trả về `bool`.

### 4.3 `PropertyGroup` và builder API

`PropertyGroup : Property` với `typeId() == Types::Group`, có danh sách con có thứ tự.

```cpp
class PropertyGroup : public Property {
public:
    static std::unique_ptr<PropertyGroup> create(const QString& id);

    PropertyGroup& addGroup(const QString& id);
    PropertyBuilder<bool>    addBool  (const QString& id, bool v);
    PropertyBuilder<int>     addInt   (const QString& id, int v);
    PropertyBuilder<double>  addDouble(const QString& id, double v);
    PropertyBuilder<QString> addString(const QString& id, const QString& v);
    EnumBuilder              addEnum  (const QString& id, const QStringList& labels, int index);
    EnumBuilder              addEnum  (const QString& id, const QList<EnumOption>& options, const QVariant& v);
    PathBuilder              addFilePath(const QString& id, const QString& v);
    PathBuilder              addDirPath (const QString& id, const QString& v);
    Property&                add(const TypeId& type, const QString& id, const QVariant& v); // kiểu tùy biến
    Property&                add(std::unique_ptr<Property> p);

    bool remove(const QString& id);
    int  childCount() const;
    Property* child(int i) const;
    Property* find(const QString& path) const;   // "Transform/x"
};
```

- Thêm `id` trùng trong cùng group: **assert trong debug**, trả về property đã tồn tại trong release (không tạo mới).
- `PropertyBuilder<T>` là wrapper nhẹ quanh `Property&`, trả về `*this` cho mỗi setter:
  `displayName`, `toolTip`, `readOnly`, `enabled`, `visible`, `validator`, và setter theo kiểu
  (`range`, `step`, `decimals`, `prefix`, `suffix`, `maxLength`, `placeholder`, `regex`, `filter`, `mode`, `defaultDir`, `mustExist`).
  Setter không hợp lệ với kiểu (vd. `regex` trên `int`) **không biên dịch được** (chỉ khai báo trên specialization tương ứng).
- Builder chuyển ngầm được sang `Property&`.

### 4.4 Attribute chuẩn

Khóa là hằng `qpb::Attr::*` (`QString`). Thuộc tính không nhận ra bị bỏ qua (không lỗi), cho phép kiểu tùy biến có attribute riêng.

| Kiểu       | Attribute (kiểu giá trị)                                                                 | Mặc định              |
|------------|-------------------------------------------------------------------------------------------|-----------------------|
| Int        | `min`(int) `max`(int) `step`(int) `prefix` `suffix`                                        | INT_MIN / INT_MAX / 1 |
| Double     | `min` `max` `step`(double) `decimals`(int) `prefix` `suffix`                               | −∞ / +∞ / 1.0 / 2     |
| String     | `maxLength`(int) `placeholder` `regex`(QString) `multiline`(bool, **1.1**)                | không giới hạn        |
| Enum       | `options`(`QList<EnumOption>` — `{QString label; QVariant value;}`)                         | —                     |
| FilePath   | `filter`(QString) `mode`(`"open"`/`"save"`) `defaultDir` `mustExist`(bool)                  | `"open"`, false       |
| DirPath    | `defaultDir` `mustExist`(bool)                                                             | false                 |
| (mọi kiểu) | `editorId`(TypeId) — ghi đè editor cho riêng property này (xem 5.2)                         | —                     |

**Enum:** `value()` là `value` của option được chọn (int **hoặc** QString, người dùng chọn qua overload).
Overload `addEnum(id, QStringList labels, int index)` tạo option với `value = index`.

**Integer 64-bit:** 1.0 chỉ hỗ trợ `int` (vì `QSpinBox` là `int`). `qint64` thêm ở 1.2 dưới dạng **kiểu mới** `Types::Int64` (không đổi hành vi `Int` → không breaking).

### 4.5 `TypeRegistry` (phần không có UI)

```cpp
struct TypeHandler {
    int storageType = QMetaType::UnknownType;                           // kiểu QVariant mong đợi
    std::function<QString(const QVariant&, const Property&)> displayText; // null → QVariant::toString()
    std::function<ValidationResult(const QVariant&, const Property&)> validate; // null → luôn hợp lệ
    std::function<QVariant(const QVariant&, const Property&)> normalize;  // null → giữ nguyên (vd. clamp)
};

class TypeRegistry {
public:
    static TypeRegistry& global();
    bool registerType(const TypeId& id, TypeHandler h);   // false nếu id đã có (không ghi đè)
    void replaceType (const TypeId& id, TypeHandler h);   // ghi đè có chủ đích
    const TypeHandler* handler(const TypeId& id) const;   // nullptr nếu chưa đăng ký
    template <class T> bool registerType(const TypeId& id, TypeHandler h); // tự set storageType
};
```

- 7 kiểu cơ bản được đăng ký khi `TypeRegistry::global()` được gọi lần đầu.
- Cấu hình bằng gán từng trường (C++17), không dùng designated initializer.
- `TypeRegistry` **không thread-safe**; đăng ký trong luồng chính trước khi tạo model.
- Property có `typeId` chưa đăng ký: vẫn hiển thị (text = `QVariant::toString()`), **read-only**, `qWarning` một lần.

### 4.6 `PropertyModel`

```cpp
class PropertyModel : public QAbstractItemModel {
    Q_OBJECT
public:
    enum Column { NameColumn = 0, ValueColumn = 1 };
    enum Role {
        PropertyRole = Qt::UserRole + 1, // Property* (const)
        TypeIdRole,                      // QString
        PathRole,                        // QString
        IsGroupRole,                     // bool
        IsModifiedRole,                  // bool
        AttributesRole,                  // QVariantMap
        IsVisibleRole,                   // bool (hiệu lực, đã tính tổ tiên)
        // Qt::UserRole+1 … Qt::UserRole+99 dành cho qpb (thêm role mới ở cuối, không đổi số cũ)
        UserRole = Qt::UserRole + 100    // role của ứng dụng bắt đầu từ đây
    };

    explicit PropertyModel(QObject* parent = nullptr);
    explicit PropertyModel(std::unique_ptr<PropertyGroup> root, QObject* parent = nullptr);

    void setRoot(std::unique_ptr<PropertyGroup> root);   // beginResetModel/endResetModel
    PropertyGroup* root() const;
    Property* propertyAt(const QModelIndex& idx) const;
    QModelIndex indexOf(const Property* p, int column = NameColumn) const;
    Property* find(const QString& path) const;

    bool setValue(const QString& path, const QVariant& v);
    void resetToDefault(const QModelIndex& idx);          // group ⇒ reset đệ quy

    void beginBatch();                                    // lồng được
    void endBatch();

signals:
    void valueChanged(const QString& path, const QVariant& newValue, const QVariant& oldValue);
    void validationFailed(const QString& path, const QVariant& rejected, const QString& message);
    void batchValueChanged(const QStringList& paths);     // chỉ phát khi endBatch() ngoài cùng
};
```

**Ánh xạ dữ liệu → role:**

| Cột   | Role                    | Giá trị                                                                  |
|-------|-------------------------|--------------------------------------------------------------------------|
| Name  | `DisplayRole`           | `displayName()`                                                          |
| Name  | `ToolTipRole`           | `toolTip()`                                                              |
| Name  | `FontRole`              | Không set trong core (view in đậm dựa trên `IsModifiedRole`, có từ 1.0)  |
| Value | `DisplayRole`           | `TypeHandler::displayText` (Bool: rỗng, dùng `CheckStateRole`)           |
| Value | `EditRole`              | `value()`                                                                |
| Value | `CheckStateRole`        | Chỉ Bool: `Qt::Checked`/`Qt::Unchecked`                                  |
| Cả hai| role tùy biến ở trên    | như bảng enum                                                             |

**`flags()`:** `ItemIsEnabled` theo `isEnabled()` hiệu lực; `ItemIsSelectable` luôn có;
cột Value: `ItemIsEditable` (không phải Bool) hoặc `ItemIsUserCheckable` (Bool) khi không read-only và kiểu đã đăng ký.
Group không có cờ edit.

**Thuộc tính ẩn (`visible = false`):** model **vẫn chứa** hàng đó; view tự ẩn (`setRowHidden`), để
`QSortFilterProxyModel` và index ổn định. Model thông báo đổi visibility qua `dataChanged` với role `IsVisibleRole`.

**Pipeline ghi giá trị** (`setData(ValueColumn, EditRole|CheckStateRole)`, `setValue`, `Property::setValue`):

1. Property read-only / disabled / kiểu chưa đăng ký → trả `false`, không signal.
2. Chuyển đổi `QVariant` sang `storageType` (`QVariant::convert`); thất bại → `validationFailed`, trả `false`.
3. `normalize` (vd. Int/Double clamp theo `min`/`max`, làm tròn `decimals`).
4. `TypeHandler::validate` → `Property::validator`. Lỗi → `validationFailed(path, v, message)`, trả `false`, giá trị cũ giữ nguyên.
5. Giá trị bằng giá trị cũ → trả `true`, **không** signal.
6. Ghi giá trị; `dataChanged(nameIdx, valueIdx)`; `valueChanged(path, new, old)`.
   Trong batch: `valueChanged` vẫn phát từng cái; đồng thời gom path để phát `batchValueChanged` khi batch kết thúc.

**Thay đổi cấu trúc khi model đang sống:** mỗi nút giữ con trỏ nội bộ tới một `detail::TreeObserver`
(interface trong core, `PropertyModel` implement). `PropertyGroup::add*` / `remove` gọi
`observer->aboutToInsert/inserted/aboutToRemove/removed`, model chuyển thành `beginInsertRows`/`endInsertRows`/...
Setter metadata gọi `observer->changed(node, roles)` → `dataChanged`. Nút chưa gắn model: observer null, không tốn gì.
Xóa property đang được edit: view đóng editor trước (Qt xử lý qua `rowsAboutToBeRemoved`).

**Ownership:** `PropertyModel` sở hữu gốc. Con trỏ `Property*` trả ra hợp lệ tới khi nút bị `remove` hoặc `setRoot`.

### 4.7 Validation

```cpp
struct ValidationResult {
    bool ok = true;
    QString message;
    static ValidationResult valid();
    static ValidationResult error(QString msg);
};
```

Validation mặc định của kiểu cơ bản:

| Kiểu       | Kiểm tra                                                                  |
|------------|---------------------------------------------------------------------------|
| Int/Double | Sau normalize luôn trong khoảng → luôn hợp lệ (clamp thay vì từ chối)     |
| String     | `maxLength`; `regex` (khớp toàn bộ, `QRegularExpression::anchoredPattern`) |
| Enum       | Giá trị thuộc `options`                                                   |
| FilePath   | `mustExist` + mode `open` → `QFileInfo::isFile()`; chuỗi rỗng luôn hợp lệ |
| DirPath    | `mustExist` → `QFileInfo::isDir()`; chuỗi rỗng luôn hợp lệ                |

---

## 5. Widgets (`qpb::widgets`)

### 5.1 `EditorFactory`

```cpp
struct EditorHandler {
    std::function<QWidget*(QWidget* parent, const Property&)> createEditor;   // bắt buộc
    std::function<void(QWidget*, const QVariant&, const Property&)> setEditorData; // bắt buộc
    std::function<QVariant(QWidget*, const Property&)> editorData;            // bắt buộc
    std::function<void(QPainter*, const QStyleOptionViewItem&, const QVariant&, const Property&)> paint; // tùy chọn
    std::function<void(QWidget*, const Property&)> applyAttributes;          // tùy chọn, gọi sau createEditor và khi attribute đổi
};

class EditorFactory {
public:
    static EditorFactory& global();
    bool registerEditor(const TypeId& id, EditorHandler h);
    void replaceEditor (const TypeId& id, EditorHandler h);
    const EditorHandler* handler(const TypeId& id) const;

    QWidget* createEditor(QWidget* parent, const Property& p) const; // dùng attribute editorId nếu có
    // Editor phát tín hiệu này khi người dùng "chốt" giá trị (vd. chọn xong file) để view commit ngay.
    static void notifyCommit(QWidget* editor);
};
```

- Tra handler theo thứ tự: `attributes["editorId"]` → `typeId()`. Không tìm thấy → không có editor (ô chỉ hiển thị).
- Editor custom muốn commit ngay (không chờ focus-out) gọi `EditorFactory::notifyCommit(this)`; delegate và form view lắng nghe.

### 5.2 Ghi đè editor cho một property

Đăng ký một editor dưới ID riêng (vd. `"myapp.slider"`) rồi set `attribute editorId = "myapp.slider"` cho
property đó. Kiểu lưu trữ và validation vẫn theo `typeId` gốc.

### 5.3 Editor mặc định

| Kiểu     | Widget                                                        | Hành vi                                                                 |
|----------|---------------------------------------------------------------|-------------------------------------------------------------------------|
| Bool     | *không tạo editor* trong Tree/List (checkbox vẽ bởi delegate qua `CheckStateRole`); `QCheckBox` trong Form | Click hoặc Space để toggle |
| Int      | `QSpinBox`                                                    | Áp `min/max/step/prefix/suffix`; `keyboardTracking = false`             |
| Double   | `QDoubleSpinBox`                                              | Như Int + `decimals`; khi không edit hiển thị tối đa `decimals` chữ số thập phân theo `QLocale`, bỏ số 0 thừa |
| String   | `QLineEdit`                                                   | `maxLength`, `placeholder`, `QRegularExpressionValidator` từ `regex`    |
| Enum     | `QComboBox` (không editable)                                  | Commit ngay khi chọn (`notifyCommit` trên `activated`)                  |
| FilePath | `qpb::PathEdit` = `QLineEdit` + `QToolButton "…"`             | Nút mở `QFileDialog::getOpenFileName`/`getSaveFileName` theo `mode`; chọn xong → `notifyCommit` |
| DirPath  | `qpb::PathEdit` (chế độ thư mục)                              | `QFileDialog::getExistingDirectory`                                    |

Hiển thị đường dẫn dài trong ô (không edit): elide ở giữa (`Qt::ElideMiddle`), tooltip là đường dẫn đầy đủ.

### 5.4 `PropertyDelegate : QStyledItemDelegate`

- `createEditor/setEditorData/setModelData` ủy quyền cho `EditorFactory`. `setModelData` gọi `model->setData`;
  nếu trả `false` (validation lỗi) thì editor vẫn đóng, model giữ giá trị cũ, view hiển thị thông báo lỗi
  (tooltip tại ô, `QToolTip::showText`) — hành vi 1.0. [Quyết định D5]
- `paint`: dùng `EditorHandler::paint` nếu có; Bool vẽ checkbox căn giữa trái; group vẽ nền `QPalette::AlternateBase`, chữ đậm.
- **Focus khi mở dialog:** `PathEdit` đặt cờ `dialogOpen` trong lúc dialog modal hiển thị. `PropertyDelegate::eventFilter`
  bỏ qua `FocusOut` của editor có cờ này, nên editor không bị đóng/commit giữa chừng. [Quyết định D6]
- Phím: **Enter** commit + đóng; **Esc** hủy; **Tab/Shift+Tab** commit rồi mở editor ở ô Value kế tiếp/trước đó có thể edit
  (bỏ qua group và read-only); focus-out commit.

### 5.5 `PropertyTreeView : QTreeView`

```cpp
class PropertyTreeView : public QTreeView {
public:
    enum class Mode { Tree, List };
    explicit PropertyTreeView(QWidget* parent = nullptr);
    void setModel(QAbstractItemModel* model) override; // chấp nhận PropertyModel hoặc proxy của nó
    void setMode(Mode m);  Mode mode() const;
    void setNameColumnWidth(int px);
};
```

- Mặc định: `editTriggers = CurrentChanged | SelectedClicked | EditKeyPressed`; 2 cột; header có thể resize;
  `alternatingRowColors = true`; `setUniformRowHeights(true)`; ẩn hàng theo `IsVisibleRole` và cập nhật khi `dataChanged`/insert.
- **Mode::Tree:** group có thể thu gọn, mặc định mở; group hàng dùng `setFirstColumnSpanned(true)`.
- **Mode::List:** *không dùng proxy làm phẳng*. Cùng model, `rootIsDecorated = false`, `indentation = 0`,
  `itemsExpandable = false`, `expandAll()` và giữ expand khi có hàng mới; group hiển thị như section header (spanned, không thu gọn được). [Quyết định D4]
- Context menu trên ô property: **Reset to default** (disabled nếu không modified hoặc read-only); trên group: **Reset group**.
- Chuyển mode không tạo lại model, không mất giá trị, không mất selection hiện tại.

### 5.6 `PropertyFormView : QScrollArea` (1.1)

- Dựng `QFormLayout` cho mỗi group; group lồng thành `QGroupBox` có thể thu gọn (checkable-less, nút ▸ ở tiêu đề).
- Mỗi property có một editor **thường trực** tạo bởi `EditorFactory` (Bool dùng `QCheckBox`).
- Commit: editor phát tín hiệu thay đổi → `model->setData` (spinbox: `editingFinished`; line edit: `editingFinished`;
  combo/checkbox/path: ngay lập tức hoặc qua `notifyCommit`). Validation lỗi → khôi phục giá trị cũ trên editor + tooltip lỗi.
- Đồng bộ ngược: lắng nghe `dataChanged` → `setEditorData` (chặn vòng lặp bằng `QSignalBlocker`);
  `rowsInserted/rowsRemoved/modelReset/layoutChanged` → dựng lại phần bị ảnh hưởng (1.1 cho phép dựng lại toàn bộ group chứa nó).
- Tôn trọng `visible` (ẩn cả label + editor), `enabled`, `readOnly`.
- Không dùng `QDataWidgetMapper` (không hỗ trợ cây).

### 5.7 Tìm kiếm / lọc (1.1)

`PropertyFilterProxyModel : QSortFilterProxyModel` với `recursiveFilteringEnabled = true`, lọc theo `displayName`
(không phân biệt hoa thường). Group hiển thị nếu có con khớp. `PropertyTreeView` và `PropertyFormView` làm việc với proxy.

---

## 6. Phân phối dạng component folder

### 6.1 Cấu trúc repo

Folder `qpb/` là **đơn vị phân phối**: tự chứa, chép nguyên vào project khác là dùng được.
Mọi thứ ngoài `qpb/` chỉ phục vụ phát triển thư viện.

```text
property-browser/                  (repo phát triển)
├── qpb/                           ← COMPONENT FOLDER: chép nguyên folder này
│   ├── CMakeLists.txt             # project(qpb VERSION x.y.z), chỉ build 2 lib
│   ├── VERSION                    # "1.0.0" — nguồn duy nhất của version
│   ├── LICENSE
│   ├── CHANGELOG.md               # kèm hướng dẫn nâng cấp mỗi bản
│   ├── include/qpb/               # header public (duy nhất) — §9
│   │   ├── qpb.h                  # umbrella header: include tất cả
│   │   ├── qpbglobal.h            # export macro, QPB_VERSION*, QPB_DEPRECATED
│   │   ├── Property.h  PropertyGroup.h  PropertyModel.h  TypeRegistry.h ...
│   │   └── widgets/PropertyTreeView.h  EditorFactory.h ...
│   └── src/                       # nội bộ: *.cpp, *_p.h
│       ├── core/
│       └── widgets/
├── CMakeLists.txt                 # dev: add_subdirectory(qpb) + tests + examples
├── tests/   (core, widgets, api_compat, consumer)
├── examples/ (quickstart, custom_type, inspector)
├── tools/
└── docs/
```

`tests/` và `examples/` dùng `qpb` **đúng như một consumer** (`add_subdirectory(../qpb)` + link `qpb::widgets`),
nên mọi lỗi tích hợp lộ ra ngay trong repo.

### 6.2 Cách dùng trong project khác

```text
my-app/
├── CMakeLists.txt
├── components/
│   └── qpb/        ← chép từ property-browser/qpb (hoặc git subtree/submodule)
└── src/
```

```cmake
# my-app/CMakeLists.txt — chỉ cần 2 dòng
find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets)
add_subdirectory(components/qpb)
target_link_libraries(my_app PRIVATE qpb::widgets)   # qpb::core được kéo theo
```

```cpp
#include <qpb/qpb.h>   // hoặc include từng header
```

**Cập nhật thư viện:** xóa `components/qpb/`, chép bản mới vào, build lại. Đọc `CHANGELOG.md` mục của bản mới.
Không cần sửa CMake hay code của consumer khi cùng major version (G5, S6).

Ba cách giữ folder đồng bộ (đều được hỗ trợ, do consumer chọn):

| Cách             | Lệnh cập nhật                                                    | Ghi chú                                  |
|------------------|------------------------------------------------------------------|------------------------------------------|
| Chép tay         | Tải `qpb-x.y.z.zip` từ GitHub Release, giải nén đè               | Đơn giản nhất                            |
| `git subtree`    | `git subtree pull --prefix components/qpb <remote> qpb-release --squash` | Cần nhánh `qpb-release` chỉ chứa folder `qpb/` (tạo bằng `git subtree split`) |
| `git submodule`  | Trỏ tới repo + dùng `add_subdirectory(components/property-browser/qpb)` | Kéo cả repo dev; tests/examples không build khi là subproject |

### 6.3 Yêu cầu với `qpb/CMakeLists.txt` (để không "làm bẩn" project chủ)

- Dùng `CMAKE_CURRENT_SOURCE_DIR`/`CMAKE_CURRENT_BINARY_DIR`, **không bao giờ** `CMAKE_SOURCE_DIR`.
- Không đổi biến global (`CMAKE_CXX_STANDARD`, `CMAKE_CXX_FLAGS`, `CMAKE_AUTOMOC`, output dir...).
  Mọi thiết lập qua `target_*` và `set_target_properties(... AUTOMOC ON)` trên target của qpb.
- `target_compile_features(qpb_core PUBLIC cxx_std_17)` — chỉ yêu cầu *tối thiểu*, consumer dùng C++20 vẫn được.
- Cờ cảnh báo và `QT_NO_CAST_FROM_ASCII` là **PRIVATE**; header public phải sạch cảnh báo dưới
  `-Wall -Wextra -Wpedantic` / `/W4` của consumer.
- Chỉ gọi `find_package(Qt6 6.5 ... Core Widgets)` nếu target `Qt6::Widgets` chưa tồn tại.
- Tên target có tiền tố `qpb_`; alias `qpb::core`, `qpb::widgets`. Option có tiền tố `QPB_`.
- Không có tests/examples trong folder `qpb/`; không `install()` mặc định (option `QPB_INSTALL`, OFF).
- **Không dùng Qt resource (`.qrc`)** trong 1.x: static lib cần `Q_INIT_RESOURCE` ở phía consumer → vi phạm "2 dòng CMake".
  Icon lấy từ `QStyle::standardIcon` hoặc vẽ bằng code.
- **Không dựa vào static initializer** để đăng ký kiểu (linker có thể loại bỏ khi link static).
  Kiểu cơ bản đăng ký lười trong `TypeRegistry::global()` / `EditorFactory::global()`.
- Build shared (`QPB_BUILD_SHARED=ON`) vẫn hỗ trợ; khi đó consumer phải deploy thêm DLL/so — ghi rõ trong README.

## 7. Kiểm thử

| Tầng          | Công cụ                        | Nội dung bắt buộc                                                                 |
|---------------|--------------------------------|-----------------------------------------------------------------------------------|
| Core          | Qt Test (không cần GUI)        | builder, path/find, trạng thái hiệu lực kế thừa, registry, pipeline validation   |
| Model         | Qt Test + `QAbstractItemModelTester` (mode Fatal) | mọi thao tác cấu trúc/giá trị; signal (`QSignalSpy`); batch       |
| Widgets       | Qt Test, `QT_QPA_PLATFORM=offscreen` | tạo/commit/hủy editor cho 7 kiểu; Enter/Esc/Tab; focus-out; mode Tree↔List; reset menu; PathEdit không đóng khi dialog mở (dialog được thay bằng hook test) |
| Example       | build trong CI                  | S1 và S2 được đo tự động bằng script đếm dòng                                     |
| API compat    | chỉ biên dịch + chạy             | §9.5: mã client của mọi bản 1.x đã phát hành vẫn build và chạy đúng               |
| Consumer      | CMake project mẫu `tests/consumer` | Chép `qpb/` vào `components/`, build; kiểm tra không rò rỉ biến global; build với C++17 và C++20; static và shared |

`PathEdit` expose một hook tĩnh (`setDialogProviderForTesting`) để test thay `QFileDialog` bằng hàm trả giá trị cố định.

CI (khi có): GitHub Actions, ma trận Ubuntu/Windows/macOS × Qt 6.5 / 6.8, qua `jurplel/install-qt-action`.

---

## 8. Phiên bản & phạm vi theo bản

Vì mục tiêu là API cố định cho nhiều project, **bản đầu tiên cho project thật dùng là 1.0**.
Các bản 0.x chỉ dùng nội bộ trong repo (examples) để thử API; API 0.x có thể đổi.
Sau 1.0, tính năng mới đến dưới dạng **bổ sung** (minor), không sửa cái đã có.

| Bản   | Nội dung                                                                                                   | Loại thay đổi |
|-------|------------------------------------------------------------------------------------------------------------|---------------|
| 0.1   | Prototype nội bộ: core + model + Tree view, 7 kiểu. API chưa khóa                                           | —             |
| **1.0** | §4 toàn bộ; §5.1–5.5; 7 kiểu cơ bản; Tree + List; reset; in đậm khi modified; tooltip; component folder (§6); chính sách API (§9); example quickstart, custom_type, inspector | **Khóa API** |
| 1.1   | `PropertyFormView` (§5.6); `PropertyFilterProxyModel` (§5.7); attribute `multiline`                          | Bổ sung       |
| 1.2   | `QObjectPropertySource` (đọc `Q_PROPERTY`, metadata qua `Q_CLASSINFO("qpb:<prop>", "min=0;max=10")`, đồng bộ hai chiều); serialize `toJson/fromJson`, `save/load(QSettings&)`; `Types::Int64`; example `QUndoStack` | Bổ sung |
| 2.0   | Chỉ khi thật sự cần phá vỡ API; gom mọi thứ đã deprecate                                                    | Breaking      |

**Thiết kế 1.0 phải "chừa chỗ" cho 1.1/1.2** mà không đổi API: Form view và filter là class mới dùng
`PropertyModel` có sẵn; `QObjectPropertySource` chỉ cần API public của `PropertyGroup`/`PropertyModel`
(`add`, `setValue`, `valueChanged`); `Int64` là `TypeId` mới; serialize là hàm tự do mới.
Việc kiểm tra điều này là một task riêng trong PLAN (M3 — API review).

---

## 9. Chính sách ổn định API

### 9.1 Cam kết

Trong cùng major version (1.x): code consumer đã build được với 1.a sẽ **build được và chạy đúng như cũ** với 1.b (b > a)
chỉ bằng cách thay folder `qpb/`. Ngoại lệ duy nhất: sửa bug mà hành vi cũ trái với tài liệu — phải ghi rõ trong CHANGELOG.

Vì consumer luôn **build lại từ source**, cam kết là **tương thích source (API)**, không phải tương thích nhị phân (ABI).
Dù vậy d-pointer vẫn được dùng (§9.3) để giữ header ổn định và tự do sửa nội bộ.

### 9.2 Thay đổi được phép / không được phép trong 1.x

| Được phép (minor/patch)                                                  | Không được phép (chỉ ở major)                                     |
|--------------------------------------------------------------------------|-------------------------------------------------------------------|
| Thêm class, hàm tự do, header mới                                        | Xóa hoặc đổi tên bất kỳ thứ gì public                             |
| Thêm hàm thành viên **không ảo** mới; thêm overload không gây mơ hồ      | Đổi kiểu tham số/giá trị trả về, thêm tham số (kể cả có default) |
| Thêm giá trị enum **ở cuối**                                             | Đổi giá trị số của enum/role đã có                                |
| Thêm trường `std::function` vào `TypeHandler`/`EditorHandler` (mặc định rỗng = hành vi cũ) | Thêm hàm **pure virtual** vào class người dùng có thể kế thừa     |
| Thêm hàm virtual **có cài đặt mặc định giữ hành vi cũ**                  | Đổi hành vi mặc định đã ghi trong tài liệu                        |
| Thêm attribute, `TypeId`, signal mới                                     | Đổi chữ ký signal/slot đã có                                      |
| Thêm option CMake (mặc định giữ hành vi cũ)                              | Đổi tên target, option, đường dẫn include; tăng yêu cầu C++/Qt/CMake |
| Deprecate (kèm thay thế)                                                  | Xóa thứ đã deprecate                                              |

### 9.3 Quy tắc thiết kế header public

- Class có trạng thái (`Property`, `PropertyGroup`, `PropertyModel`, `TypeRegistry`, `EditorFactory`, view) dùng d-pointer;
  không có data member public/protected ngoài d-pointer. Header không include `*_p.h`.
- Struct cấu hình (`TypeHandler`, `EditorHandler`, `EnumOption`, `ValidationResult`) là aggregate chỉ để **thêm trường ở cuối**;
  tài liệu khuyên gán từng trường, không khởi tạo theo vị trí (`{a, b, c}`).
- Không expose kiểu của thư viện bên thứ ba; kiểu trong API chỉ là Qt + std.
- Không có `inline`/template chứa logic nghiệp vụ trong header (builder chỉ chuyển tiếp sang hàm trong `.cpp`).
- Mọi header public include được **riêng lẻ** (tự đủ), kiểm tra bằng test biên dịch từng header.

### 9.4 Version & deprecation

- `qpbglobal.h` cung cấp `QPB_VERSION_MAJOR/MINOR/PATCH`, `QPB_VERSION_STR`, `QPB_VERSION_CHECK(maj, min, pat)` (sinh từ file `VERSION`),
  và hàm runtime `qpb::version()`.
- API bị thay thế được đánh dấu `QPB_DEPRECATED_X("dùng X thay thế")` (map sang `[[deprecated]]`), giữ **đến hết 1.x**.
  Consumer muốn chủ động dọn có thể định nghĩa `QPB_DISABLE_DEPRECATED` để biến chúng thành lỗi biên dịch.
- `CHANGELOG.md` mỗi bản có mục *Added / Changed / Deprecated / Fixed* và *Upgrade notes* (thường là "không cần làm gì").

### 9.5 Kiểm chứng tự động

- `tests/api_compat/v1_0.cpp`, `v1_1.cpp`, …: mỗi bản phát hành thêm một file dùng **toàn bộ** API public của bản đó
  (cả signal, builder, handler). File cũ **không bao giờ được sửa**; mọi bản sau phải build (warning deprecate cho phép) và chạy pass.
- Test "header tự đủ": mỗi header public được include một mình trong một `.cpp` riêng.
- `tools/api_snapshot`: xuất danh sách symbol public (bằng cách parse header hoặc `abi-dumper` nếu có) và diff với snapshot của bản trước
  → CI báo nếu có xóa/đổi. (Có thể dùng từ 1.1; ở 1.0 chỉ tạo snapshot gốc.)

---

## Phụ lục A — Ví dụ API cuối cùng (1.0)

```cpp
#include <qpb/qpb.h>

using namespace qpb;

auto root = PropertyGroup::create("Camera");
auto& transform = root->addGroup("Transform");
transform.addDouble("x", 0.0).range(-100.0, 100.0).step(0.5).suffix(" m");
transform.addDouble("y", 0.0).range(-100.0, 100.0).step(0.5).suffix(" m");
root->addBool("visible", true);
root->addInt("fov", 60).range(10, 170);
root->addString("name", "Main camera");
root->addEnum("projection", {"Perspective", "Orthographic"}, 0);
root->addFilePath("lut", {}).filter("LUT files (*.cube)");
root->addDirPath("cacheDir", {});

PropertyModel model(std::move(root));
PropertyTreeView view;                       // view.setMode(PropertyTreeView::Mode::List);
view.setModel(&model);

QObject::connect(&model, &PropertyModel::valueChanged,
                 [](const QString& path, const QVariant& v, const QVariant&) { qDebug() << path << "=" << v; });
```

Đăng ký `QColor` (C++17, ngoài lib):

```cpp
TypeHandler t;
t.displayText = [](const QVariant& v, const Property&) { return v.value<QColor>().name(); };
TypeRegistry::global().registerType<QColor>("app.color", t);

EditorHandler e;
e.createEditor  = [](QWidget* parent, const Property&) { return new ColorButton(parent); };
e.setEditorData = [](QWidget* w, const QVariant& v, const Property&) { static_cast<ColorButton*>(w)->setColor(v.value<QColor>()); };
e.editorData    = [](QWidget* w, const Property&) { return QVariant::fromValue(static_cast<ColorButton*>(w)->color()); };
e.paint         = &paintColorSwatch;
EditorFactory::global().registerEditor("app.color", e);

root->add("app.color", "tint", QColor(Qt::white));
```

## Phụ lục B — Nhật ký quyết định

| #  | Quyết định                                                                 | Lý do                                                                                  |
|----|----------------------------------------------------------------------------|----------------------------------------------------------------------------------------|
| D1 | `TypeId` là chuỗi logic, không phải `QMetaType`                             | String/FilePath/DirPath cùng lưu `QString`; Enum có thể int hoặc string                 |
| D2 | Tách `TypeRegistry` (core) và `EditorFactory` (widgets)                     | Giữ core không phụ thuộc QtWidgets (mở đường QML, test headless)                        |
| D3 | C++17, cấu hình handler bằng gán trường                                     | Designated initializer là C++20; MSVC `/std:c++17` từ chối                              |
| D4 | List mode = cùng `QTreeView`, không proxy làm phẳng                         | Rẻ hơn nhiều; index và editor giữ nguyên khi đổi mode                                  |
| D5 | Validation lỗi từ delegate: đóng editor, giữ giá trị cũ, hiện tooltip lỗi    | Delegate chuẩn đóng editor trước khi biết kết quả `setData`; đơn giản cho 1.0          |
| D6 | `PathEdit` chặn `FocusOut` khi dialog đang mở                               | Tránh commit/đóng editor khi `QFileDialog` modal lấy focus                              |
| D7 | Model sở hữu cây; nút báo thay đổi cấu trúc qua `TreeObserver` nội bộ       | Cho phép thêm/xóa property khi model đang sống mà vẫn giữ builder API                   |
| D8 | Int chỉ `int` ở 1.0; `qint64` là kiểu mới `Int64` ở 1.2                      | `QSpinBox` chỉ hỗ trợ `int`; thêm kiểu mới không phá API                               |
| D9 | Int/Double clamp thay vì từ chối                                            | Khớp hành vi `QSpinBox`; giá trị từ code ứng dụng cũng nhất quán                       |
| D10| Property ẩn vẫn nằm trong model; view ẩn hàng                               | Index ổn định, filter proxy hoạt động bình thường                                      |
| D11| Signal truyền `path` (QString), không truyền `Property&`                    | An toàn với queued connection, không cần đăng ký metatype                              |
| D12| Bản đầu cho project thật là 1.0 (khóa API); 0.x chỉ nội bộ                  | Mục tiêu G5: project đã dùng không phải sửa code                                        |
| D13| Cam kết tương thích **source**, không phải ABI                               | Consumer luôn build lại từ folder component                                            |
| D14| Phân phối bằng folder `qpb/` tự chứa + `add_subdirectory`                   | Mục tiêu G6; `find_package`/install thành tùy chọn                                    |
| D15| Static lib mặc định, không dùng `.qrc`, không static initializer            | Tránh phải deploy DLL, `Q_INIT_RESOURCE` và lỗi linker bỏ object khi link static       |
| D16| Viết mới hoàn toàn, không wrap/fork                                          | Mục tiêu G7; kiểm soát trọn API để có thể cam kết ổn định                               |
