# qpb — Property Browser cho Qt 6: Đặc tả kỹ thuật (v0.1 → v0.3)

> Nguồn: [`brainstorm.md`](brainstorm.md) (hướng B — *Explicit model + pluggable views/types*).
> Trạng thái: **Draft 1**. Các mục đánh dấu **[Giả định]** lấy từ mục 13 của brainstorm,
> chưa được xác nhận; đổi giả định nào thì cập nhật mục đó và [Phụ lục B](#phụ-lục-b--nhật-ký-quyết-định).

---

## 1. Mục tiêu & phi mục tiêu

### 1.1 Mục tiêu

- **G1.** Lập trình viên *khai báo* một cây thuộc tính một lần (builder API) và nhận panel chỉnh sửa hoàn chỉnh.
- **G2.** Một nguồn dữ liệu (`PropertyModel`, chuẩn `QAbstractItemModel`), nhiều cách hiển thị: **Tree**, **List**, **Form**.
- **G3.** Mở rộng kiểu dữ liệu và editor **từ phía người dùng**, không sửa thư viện.
- **G4.** Tích hợp nhanh: panel 10 thuộc tính ≤ 30 dòng code; ≤ 30 phút kể cả CMake.

### 1.2 Phi mục tiêu (không làm trong v0.x)

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
| S4 | Dùng trong ≥ 2 project thật trong 6 tháng sau v0.1                | Theo dõi ngoài repo                                    |
| S5 | Không breaking change giữa các bản minor sau v1.0                 | Chính sách SemVer (ngoài phạm vi spec này)             |

---

## 2. Ràng buộc kỹ thuật

| Hạng mục        | Quyết định                                                                                   |
|-----------------|----------------------------------------------------------------------------------------------|
| Ngôn ngữ        | **C++17**. API public **không** dùng designated initializer (là C++20). [Giả định]           |
| Qt              | Tối thiểu **6.5**; môi trường phát triển/CI chính **6.8 LTS**. Chỉ dùng Core, Gui, Widgets, Test. [Giả định] |
| Build           | CMake ≥ 3.21; target `qpb::core`, `qpb::widgets`; dùng được qua `find_package`, `FetchContent`, `add_subdirectory` |
| Namespace       | `qpb` [Giả định]                                                                              |
| Thư viện        | Shared hoặc static theo `BUILD_SHARED_LIBS`; export macro `QPB_CORE_EXPORT`, `QPB_WIDGETS_EXPORT` |
| License         | MIT [Giả định]                                                                                |
| Nền tảng        | Linux, Windows (MSVC 2019+), macOS                                                            |
| Phụ thuộc       | `qpb::core` → Qt6::Core **duy nhất**. `qpb::widgets` → `qpb::core`, Qt6::Widgets             |

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
| String     | `maxLength`(int) `placeholder` `regex`(QString) `multiline`(bool, **v0.2**)                | không giới hạn        |
| Enum       | `options`(`QList<EnumOption>` — `{QString label; QVariant value;}`)                         | —                     |
| FilePath   | `filter`(QString) `mode`(`"open"`/`"save"`) `defaultDir` `mustExist`(bool)                  | `"open"`, false       |
| DirPath    | `defaultDir` `mustExist`(bool)                                                             | false                 |
| (mọi kiểu) | `editorId`(TypeId) — ghi đè editor cho riêng property này (xem 5.2)                         | —                     |

**Enum:** `value()` là `value` của option được chọn (int **hoặc** QString, người dùng chọn qua overload).
Overload `addEnum(id, QStringList labels, int index)` tạo option với `value = index`.

**Integer 64-bit:** v0.1 chỉ hỗ trợ `int` (vì `QSpinBox` là `int`). `qint64` dời sang v0.3 kèm spinbox riêng.

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
| Name  | `FontRole`              | Không set trong core (view in đậm dựa trên `IsModifiedRole`, v0.2)       |
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
  (tooltip tại ô, `QToolTip::showText`) — hành vi v0.1. [Quyết định D5]
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

### 5.6 `PropertyFormView : QScrollArea` (v0.2)

- Dựng `QFormLayout` cho mỗi group; group lồng thành `QGroupBox` có thể thu gọn (checkable-less, nút ▸ ở tiêu đề).
- Mỗi property có một editor **thường trực** tạo bởi `EditorFactory` (Bool dùng `QCheckBox`).
- Commit: editor phát tín hiệu thay đổi → `model->setData` (spinbox: `editingFinished`; line edit: `editingFinished`;
  combo/checkbox/path: ngay lập tức hoặc qua `notifyCommit`). Validation lỗi → khôi phục giá trị cũ trên editor + tooltip lỗi.
- Đồng bộ ngược: lắng nghe `dataChanged` → `setEditorData` (chặn vòng lặp bằng `QSignalBlocker`);
  `rowsInserted/rowsRemoved/modelReset/layoutChanged` → dựng lại phần bị ảnh hưởng (v0.2 cho phép dựng lại toàn bộ group chứa nó).
- Tôn trọng `visible` (ẩn cả label + editor), `enabled`, `readOnly`.
- Không dùng `QDataWidgetMapper` (không hỗ trợ cây).

### 5.7 Tìm kiếm / lọc (v0.2)

`PropertyFilterProxyModel : QSortFilterProxyModel` với `recursiveFilteringEnabled = true`, lọc theo `displayName`
(không phân biệt hoa thường). Group hiển thị nếu có con khớp. `PropertyTreeView` và `PropertyFormView` làm việc với proxy.

---

## 6. Tích hợp & đóng gói

```text
property-browser/
├── CMakeLists.txt                 # project(qpb VERSION 0.1.0), option QPB_BUILD_TESTS/EXAMPLES
├── cmake/qpbConfig.cmake.in       # find_dependency(Qt6 COMPONENTS Core Widgets)
├── src/core/        include/qpb/*.h  + *.cpp   → qpb_core   (alias qpb::core)
├── src/widgets/     include/qpb/widgets/*.h + *.cpp → qpb_widgets (alias qpb::widgets)
├── tests/core/      tst_property.cpp, tst_typeregistry.cpp, tst_propertymodel.cpp
├── tests/widgets/   tst_delegate.cpp, tst_treeview.cpp, tst_formview.cpp
├── examples/quickstart/  examples/custom_type/  examples/inspector/
└── docs/
```

- `install(EXPORT qpbTargets NAMESPACE qpb::)`; `qpbConfigVersion.cmake` với `SameMinorVersion` trước v1.0.
- Dùng `qt_standard_project_setup()` khi có (Qt ≥ 6.3); `CMAKE_AUTOMOC ON`.
- Cảnh báo: `-Wall -Wextra -Wpedantic` (GCC/Clang), `/W4` (MSVC) cho target của lib; `QT_NO_CAST_FROM_ASCII` trong lib.

---

## 7. Kiểm thử

| Tầng          | Công cụ                        | Nội dung bắt buộc                                                                 |
|---------------|--------------------------------|-----------------------------------------------------------------------------------|
| Core          | Qt Test (không cần GUI)        | builder, path/find, trạng thái hiệu lực kế thừa, registry, pipeline validation   |
| Model         | Qt Test + `QAbstractItemModelTester` (mode Fatal) | mọi thao tác cấu trúc/giá trị; signal (`QSignalSpy`); batch       |
| Widgets       | Qt Test, `QT_QPA_PLATFORM=offscreen` | tạo/commit/hủy editor cho 7 kiểu; Enter/Esc/Tab; focus-out; mode Tree↔List; reset menu; PathEdit không đóng khi dialog mở (dialog được thay bằng hook test) |
| Example       | build trong CI                  | S1 và S2 được đo tự động bằng script đếm dòng                                     |

`PathEdit` expose một hook tĩnh (`setDialogProviderForTesting`) để test thay `QFileDialog` bằng hàm trả giá trị cố định.

CI (khi có): GitHub Actions, ma trận Ubuntu/Windows/macOS × Qt 6.5 / 6.8, qua `jurplel/install-qt-action`.

---

## 8. Phiên bản & phạm vi theo bản

| Bản  | Nội dung trong spec này                                                                                      |
|------|---------------------------------------------------------------------------------------------------------------|
| v0.1 | §4 toàn bộ; §5.1–5.5; 7 kiểu cơ bản (String không `multiline`); Tree + List; reset; example quickstart, custom_type (QColor), inspector (chưa có form) |
| v0.2 | §5.6 Form view; §5.7 filter; `multiline` (`QPlainTextEdit`); in đậm khi modified; tooltip đầy đủ; inspector chuyển cả 3 view |
| v0.3 | `QObjectPropertySource` (đọc `Q_PROPERTY`, metadata qua `Q_CLASSINFO("qpb:<prop>", "min=0;max=10")`, đồng bộ hai chiều qua notify signal); serialize `toJson/fromJson`, `save/load(QSettings&)`; `qint64`; tài liệu tích hợp `QUndoStack` (dùng `valueChanged` old/new) |
| v1.0 | Đóng băng API, tài liệu API (Doxygen), ≥ 2 project thật                                                      |

---

## Phụ lục A — Ví dụ API cuối cùng (v0.1)

```cpp
#include <qpb/PropertyModel.h>
#include <qpb/widgets/PropertyTreeView.h>

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
| D5 | Validation lỗi từ delegate: đóng editor, giữ giá trị cũ, hiện tooltip lỗi    | Delegate chuẩn đóng editor trước khi biết kết quả `setData`; đơn giản cho v0.1          |
| D6 | `PathEdit` chặn `FocusOut` khi dialog đang mở                               | Tránh commit/đóng editor khi `QFileDialog` modal lấy focus                              |
| D7 | Model sở hữu cây; nút báo thay đổi cấu trúc qua `TreeObserver` nội bộ       | Cho phép thêm/xóa property khi model đang sống mà vẫn giữ builder API                   |
| D8 | Int chỉ `int` ở v0.1; `qint64` ở v0.3                                        | `QSpinBox` chỉ hỗ trợ `int`                                                             |
| D9 | Int/Double clamp thay vì từ chối                                            | Khớp hành vi `QSpinBox`; giá trị từ code ứng dụng cũng nhất quán                       |
| D10| Property ẩn vẫn nằm trong model; view ẩn hàng                               | Index ổn định, filter proxy hoạt động bình thường                                      |
| D11| Signal truyền `path` (QString), không truyền `Property&`                    | An toàn với queued connection, không cần đăng ký metatype                              |
