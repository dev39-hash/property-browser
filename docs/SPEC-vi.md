# qpb — Property Browser cho Qt 6: Đặc tả kỹ thuật (1.0 → 1.2)

> **Bản dịch tiếng Việt để tham khảo.** Bản chính thức là [`SPEC.md`](SPEC.md) (tiếng Anh); khi có khác biệt, bản tiếng Anh được ưu tiên.

> Nguồn: [`brainstorm-vi.md`](brainstorm-vi.md) (hướng B — *Explicit model + pluggable views/types*).
> Trạng thái: **Draft 3** — đã xác nhận C++17, Qt 6.5, namespace `qpb`, chỉ CMake. Các mục còn đánh dấu **[Giả định]**
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
theme riêng (dùng QSS) · conditional visibility khai báo bằng biểu thức · hỗ trợ qmake.

Lý do chi tiết: brainstorm mục 11. Thiết kế **không được cấm** multi-object editing và QML về sau
(→ core không phụ thuộc QtWidgets, `Property` không giả định có đúng một object nguồn).

### 1.3 Tiêu chí chấp nhận cấp dự án

| #  | Tiêu chí                                                         | Kiểm chứng bằng                                        |
|----|------------------------------------------------------------------|--------------------------------------------------------|
| S1 | Panel 10 thuộc tính ≤ 30 dòng                                     | `examples/quickstart/main.cpp` (dòng không trống, không tính comment và `#include`) |
| S2 | Kiểu `QColor` ≤ 100 dòng, nằm ngoài lib                           | `examples/custom_type/ColorType.{h,cpp}` (cùng cách đếm như S1) |
| S3 | Chuyển Tree ↔ List ↔ Form không đổi model                         | `examples/inspector` có nút chuyển view + test         |
| S4 | 3 kịch bản tham chiếu (`docs/use-cases.md`) làm được chỉ bằng API public | Ứng dụng RC độc lập (PLAN RC.1); project thật theo dõi ngoài repo khi có |
| S5 | Nâng 1.x → 1.y không phải sửa code consumer                        | Bộ test tương thích API (§9.5) pass trên mọi bản 1.y  |
| S6 | Cập nhật thư viện = thay folder `components/qpb/` + build lại      | `tests/consumer` build lại sau khi thay folder, không đổi CMake của consumer |

---

## 2. Ràng buộc kỹ thuật

| Hạng mục        | Quyết định                                                                                   |
|-----------------|----------------------------------------------------------------------------------------------|
| Ngôn ngữ        | **C++17** (đã xác nhận). API public **không** dùng designated initializer (là C++20)          |
| Qt              | Tối thiểu **6.5**; môi trường phát triển/CI chính **6.8 LTS**. Chỉ dùng Core, Gui, Widgets, Test. (đã xác nhận) |
| Build           | **Chỉ CMake** (không hỗ trợ qmake), CMake ≥ 3.21; target `qpb::core`, `qpb::widgets`. Cách tích hợp **chính**: `add_subdirectory(components/qpb)` (§6) |
| Namespace       | `qpb` (đã xác nhận)                                                                           |
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
- **R6.** Mọi file trong `qpb/` chỉ dùng ký tự ASCII. MSVC trên code page không phải UTF-8 cảnh báo (C4819) với ký tự khác,
  làm hỏng build của consumer dùng `/WX`. (R1, R2, R5, R6 được kiểm tra bằng `tools/check_architecture.cmake`.)

---

## 4. Core (`qpb::core`)

> Từ M1, header public trong `qpb/include/qpb/` là **tham chiếu API chuẩn**; code ở §4–§5 chỉ tóm tắt.
> Kết quả review API ở M1 ghi trong [`api-review.md`](api-review.md) và Phụ lục B (D21–D28).

### 4.1 Định danh kiểu (`TypeId`)

Kiểu được định danh bằng **ID logic** (chuỗi), không phải `QMetaType`, vì nhiều kiểu logic cùng
kiểu lưu trữ (`String`, `FilePath`, `DirPath` đều là `QString`).

```cpp
namespace qpb {
using TypeId = QString;
namespace Types {
inline constexpr QLatin1StringView Bool{"bool"};
inline constexpr QLatin1StringView Int{"int"};
inline constexpr QLatin1StringView Double{"double"};
inline constexpr QLatin1StringView String{"string"};
inline constexpr QLatin1StringView Enum{"enum"};
inline constexpr QLatin1StringView FilePath{"filepath"};
inline constexpr QLatin1StringView DirPath{"dirpath"};
inline constexpr QLatin1StringView Group{"group"};
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
| `isLive()` (1.3)  | `bool`                 | Hiệu lực = chính nó **hoặc** tổ tiên bất kỳ là live (xem dưới)          |
| `isVisible()`     | `bool`                 | Ẩn group ⇒ ẩn toàn bộ con                                               |
| `parent()`        | `PropertyGroup*`       | `nullptr` với gốc                                                       |
| `isModified()`    | `bool`                 | `value() != defaultValue()` (so sánh `QVariant`); luôn `false` khi live (1.3) |
| `validator()`     | `Property::Validator` = `std::function<ValidationResult(const QVariant&, const Property&)>` | Tùy chọn, chạy sau validation của kiểu |
| `flags()`         | `Property::Flags` (`ReadOnly`, `Disabled`, `Hidden`, `Live` (1.3)) | Trạng thái riêng; `isReadOnly/isEnabled/isVisible/isLive` ở trên là hiệu lực |

**Property live (1.3, `Flag::Live`, `setLive()`, builder `live()`):** giá trị do ứng dụng quản lý (trạng thái, bộ đếm,
dung lượng đã dùng), không phải thiết lập. Chúng không bao giờ modified (view không bao giờ in đậm), các lệnh reset group
(`resetToDefault()` trên group, "Reset group", "Reset to default") bỏ qua chúng, và `qpb::serialization` không ghi, không
đọc. Gọi `resetToDefault()` trực tiếp trên chính property live vẫn reset nó; nó vẫn sửa được trừ khi đồng thời read-only.
[D44]

**Điều kiện (1.4):** `setEnabledWhen(sourcePath, ...)` / `setVisibleWhen(sourcePath, ...)` (+ hàm builder, `clear...()`) cho
property được bật hoặc hiện tùy theo giá trị của property khác. Ba dạng: giá trị nguồn "đúng" (bool true, số khác 0, chuỗi
khác rỗng, kiểu khác hợp lệ và không null), bằng một giá trị (overload `int` để số `0` viết trực tiếp không khớp nhầm dạng
predicate), hoặc thỏa một predicate `Property::Condition`. Mỗi loại một điều kiện cho mỗi property. **`PropertyModel` đánh
giá** khi giá trị nguồn đổi (trước khi phát `valueChanged`, nên handler thấy trạng thái mới), khi thêm/xóa hàng, khi đặt
điều kiện, và trong `setRoot()` trước khi view đọc cây mới; chỉ khi kết quả đổi mới báo view (`dataChanged` cho property
và cây con). Kết quả được **kết hợp với cờ riêng** (`isEnabled()` = không `Disabled`, điều kiện đúng, tổ tiên enabled),
nên không bao giờ ghi đè `setEnabled()`/`setVisible()`. Ngoài model, sau khi bị gỡ khỏi model, hoặc khi path nguồn không
tồn tại, điều kiện coi như đúng; nguồn thiếu phát hiện lúc gắn cây (constructor, `setRoot()`) được ghi cảnh báo một lần.
View không cần code gì: disabled nghĩa là không sửa được, ẩn nghĩa là hàng bị ẩn. [D46]

Setter tương ứng (`setDisplayName`, `setToolTip`, `setReadOnly`, `setEnabled`, `setVisible`, `setLive`,
`setAttribute`, `setValidator`, `setDefaultValue`) **thông báo cho model** nếu property đã gắn vào model
(xem 4.6), để view cập nhật.

`setValue()` trên `Property` là **API cho code ứng dụng** (vd. nạp dữ liệu). Nó chạy qua cùng pipeline
chuyển kiểu, validation và signal như `PropertyModel::setData` (R3), trả về `bool`. Read-only và disabled chỉ giới hạn
**người dùng** (sửa qua view, tức `setData`): code ứng dụng vẫn đặt và reset được giá trị của các property đó, ví dụ một ô
trạng thái read-only do ứng dụng cập nhật. [D34]

### 4.3 `PropertyGroup` và builder API

`PropertyGroup : Property` với `typeId() == Types::Group`, có danh sách con có thứ tự.

```cpp
class PropertyGroup : public Property {
public:
    static std::unique_ptr<PropertyGroup> create(const QString& id);

    PropertyGroup&  addGroup(const QString& id);
    BoolBuilder     addBool  (const QString& id, bool value);
    IntBuilder      addInt   (const QString& id, int value);
    DoubleBuilder   addDouble(const QString& id, double value);
    StringBuilder   addString(const QString& id, const QString& value);
    EnumBuilder     addEnum  (const QString& id, const QStringList& labels, int currentIndex);
    EnumBuilder     addEnum  (const QString& id, const QList<EnumOption>& options, const QVariant& value);
    FilePathBuilder addFilePath(const QString& id, const QString& path);
    DirPathBuilder  addDirPath (const QString& id, const QString& path);
    Property&       add(const TypeId& type, const QString& id, const QVariant& value); // custom type
    Property&       add(std::unique_ptr<Property> property);

    bool remove(const QString& id);
    int  childCount() const;
    Property* child(int index) const;
    Property* child(const QString& id) const;
    QList<Property*> children() const;
    int  indexOf(const Property* child) const;
    Property* find(const QString& path) const;   // "Transform/x"
};
```

- Thêm `id` trùng trong cùng group: ghi cảnh báo và trả về property đã tồn tại (không tạo mới); `addGroup()` trùng với một property
  không phải group thì thêm group dưới id trống đầu tiên (`<id>_2`, ...). Id rỗng hoặc chứa `/` được sửa lại kèm cảnh báo. [D29]
- `PropertyBuilder<T>` là wrapper nhẹ quanh `Property&`, trả về `*this` cho mỗi setter:
  `displayName`, `toolTip`, `readOnly`, `enabled`, `visible`, `validator`, và setter theo kiểu
  (`range`, `minimum`, `maximum`, `step`, `decimals`, `prefix`, `suffix`, `maxLength`, `placeholder`, `regularExpression`, `filter`, `dialogMode`, `defaultDir`, `mustExist`), cùng `attribute` và `editor` cho mọi kiểu.
  Builder là các class cụ thể (`BoolBuilder`, `IntBuilder`, `DoubleBuilder`, `StringBuilder`, `EnumBuilder`, `FilePathBuilder`, `DirPathBuilder`)
  dùng chung `PropertyBuilderBase<Derived>`; setter không hợp lệ với kiểu (vd. `regularExpression` trên int) **không biên dịch được**.
- Builder chuyển ngầm được sang `Property&`.

### 4.4 Attribute chuẩn

Khóa là hằng `qpb::Attr::*` (`QString`). Thuộc tính không nhận ra bị bỏ qua (không lỗi), cho phép kiểu tùy biến có attribute riêng.

| Kiểu       | Attribute (kiểu giá trị)                                                                 | Mặc định              |
|------------|-------------------------------------------------------------------------------------------|-----------------------|
| Int        | `minimum`(int) `maximum`(int) `step`(int) `prefix` `suffix`                                | INT_MIN / INT_MAX / 1 |
| Int64 (1.2)| `minimum` `maximum` `step`(qint64) `prefix` `suffix`                                       | toàn dải qint64 / 1   |
| Double     | `minimum` `maximum` `step`(double) `decimals`(int) `prefix` `suffix`                       | −∞ / +∞ / 1.0 / 2     |
| String     | `maxLength`(int) `placeholder` `regularExpression`(QString) `multiline`(bool, **1.1**)    | không giới hạn |
| Enum       | `options`(`QList<EnumOption>` — `{QString label; QVariant value;}`)                         | —                     |
| FilePath   | `filter`(QString) `dialogMode`(int, `qpb::FileMode::Open`/`Save`) `defaultDir` `mustExist`(bool) | `Open`, false |
| DirPath    | `defaultDir` `mustExist`(bool)                                                             | false                 |
| (mọi kiểu) | `editorId`(TypeId) — ghi đè editor cho riêng property này (xem 5.2)                         | —                     |

**Multiline (1.1):** giá trị giữ nguyên các dấu xuống dòng; `displayText` nối các dòng bằng ` ¶ ` (ký hiệu ¶) để ô
chỉ hiện một dòng. String không có attribute này hiển thị như cũ. [D36]

**Enum:** `value()` là `value` của option được chọn (int **hoặc** QString, người dùng chọn qua overload).
Overload `addEnum(id, QStringList labels, int index)` tạo option với `value = index`.

**Integer 64-bit:** 1.0 chỉ hỗ trợ `int` (vì `QSpinBox` là `int`). 1.2 thêm **kiểu mới** `Types::Int64` (`"int64"`, lưu
`qint64`, `addInt64()` / `Int64Builder`), đăng ký sau bảy kiểu của 1.0; hành vi của `Int` không đổi. [D8] Lưu ý: ứng dụng tự
đăng ký kiểu `"int64"` trước 1.2 giờ nhận `false` từ `registerType()` và dùng kiểu có sẵn (ghi trong CHANGELOG).

### 4.5 `TypeRegistry` (phần không có UI)

```cpp
struct TypeHandler {
    QMetaType storageType;                                                // invalid → stored unconverted
    std::function<QString(const QVariant&, const Property&)> displayText; // empty → QVariant::toString()
    std::function<QVariant(const QVariant&, const Property&)> normalize;  // empty → unchanged (e.g. clamp)
    std::function<ValidationResult(const QVariant&, const Property&)> validate; // empty → always valid
};

class TypeRegistry {
public:
    static TypeRegistry& global();
    bool registerType(const TypeId& id, const TypeHandler& h); // false if id exists or is reserved
    template <class T> bool registerType(const TypeId& id, TypeHandler h); // sets storageType to T
    bool replaceType (const TypeId& id, const TypeHandler& h); // false if id is not registered
    bool contains(const TypeId& id) const;
    const TypeHandler* handler(const TypeId& id) const;        // nullptr if not registered
    QList<TypeId> types() const;                               // registration order
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
    bool resetToDefault(const QModelIndex& idx);          // group ⇒ reset đệ quy
    bool resetAllToDefault();                             // 1.5: cả cây

    void beginBatch();                                    // lồng được
    void endBatch();

    // 1.4: theo path, giống QObject::connect (context kết thúc nó; nullptr = model)
    QMetaObject::Connection onValueChanged(const QString& path, const QObject* context,
        std::function<void(const QVariant& value)> handler);
    QMetaObject::Connection onValueChanged(const QString& path, const QObject* context,   // path và con cháu
        std::function<void(const QString& path, const QVariant& value)> handler);

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

1. Group hoặc kiểu chưa đăng ký → trả `false`, không signal. Riêng `setData` (người dùng sửa): property read-only hoặc disabled → `false`. [D34]
2. Chuyển đổi `QVariant` sang `storageType` (`QVariant::convert`); thất bại → `validationFailed`, trả `false`.
3. `normalize` (vd. Int/Double clamp theo `min`/`max`, làm tròn `decimals`).
4. `TypeHandler::validate` → `Property::validator`. Lỗi → `validationFailed(path, v, message)`, trả `false`, giá trị cũ giữ nguyên.
5. Giá trị bằng giá trị cũ → trả `true`, **không** signal.
6. Ghi giá trị; `dataChanged(nameIdx, valueIdx)`; `valueChanged(path, new, old)`.
   Trong batch: `valueChanged` vẫn phát từng cái; đồng thời gom path để phát `batchValueChanged` khi batch kết thúc.

**Reset về mặc định:** `resetToDefault(idx)` reset property tại `idx` (group: các con cháu, trừ property live) trong một
batch; index không hợp lệ, kể cả của root, trả về `false`. `resetAllToDefault()` (1.5) làm việc đó cho cả cây, giống
`root()->resetToDefault()`: là code ứng dụng nên property read-only và disabled cũng được reset (D34), property live thì
không (D44); chỉ một `batchValueChanged`. Trả về `false` nếu có reset bị từ chối và `true` với model không có root. [D48]

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
| String     | `maxLength`; `regularExpression` (khớp toàn bộ, `QRegularExpression::anchoredPattern`) |
| Enum       | Giá trị thuộc `options`                                                   |
| FilePath   | `mustExist` + `FileMode::Open` → `QFileInfo::isFile()`; chuỗi rỗng luôn hợp lệ |
| DirPath    | `mustExist` → `QFileInfo::isDir()`; chuỗi rỗng luôn hợp lệ                |

### 4.8 Serialization (1.2)

Các hàm tự do trong namespace **`qpb::serialization`** (header `qpb/Serialization.h`, `qpb::core`):

```cpp
namespace qpb::serialization {
QJsonObject toJson(const PropertyGroup& group);
bool fromJson(PropertyGroup& group, const QJsonObject& json);
void save(const PropertyGroup& group, QSettings& settings);   // key = path tương đối với group
bool load(PropertyGroup& group, const QSettings& settings);
}
```

- Chỉ lưu **giá trị**; ứng dụng tự dựng cây rồi ghi giá trị vào. Group thành object JSON lồng nhau theo id (group không có gì để lưu thì bỏ qua, giống `save()` không ghi key nào cho chúng); key của
  `QSettings` là path tương đối với group, nằm dưới group hiện tại của settings.
- **Property read-only và live (1.3) không được ghi cũng không được đọc** (ứng dụng tự quản lý, D34, D44). Property ẩn và disabled thì có.
- Giá trị được khôi phục bằng `Property::setValue()` (chuyển đổi → normalize → validation). Key lạ và key thiếu bị bỏ qua;
  `fromJson`/`load` trả về `false` nếu có giá trị bị từ chối, các giá trị khác vẫn được áp. Bọc trong
  `beginBatch()`/`endBatch()` để chỉ có một `batchValueChanged`.
- Dạng JSON theo kiểu: `TypeHandler::toJson` / `fromJson` (trường tùy chọn mới, 1.2); rỗng → `QJsonValue::fromVariant()` /
  `toVariant()`. `Int64` ghi số tới 2^53, giá trị lớn hơn ghi dạng chuỗi.
- Các hàm nằm trong namespace lồng nên argument-dependent lookup không bao giờ tìm thấy chúng: hàm `save(group, settings)`
  của ứng dụng gọi không kèm namespace vẫn không bị mơ hồ sau khi nâng cấp (quy tắc ở §9.3). [D40, D42]

### 4.9 `QObjectPropertySource` (1.2)

```cpp
class QObjectPropertySource : public QObject {
public:
    explicit QObjectPropertySource(PropertyModel* model, QObject* parent = nullptr);
    PropertyModel* model() const;
    PropertyGroup* addObject(QObject* object, PropertyGroup* parentGroup = nullptr, const QString& id = QString());
    bool removeObject(QObject* object);
    QList<QObject*> objects() const;
    PropertyGroup* groupOf(const QObject* object) const;
    void refresh();   // đọc lại tất cả (Q_PROPERTY không có NOTIFY)
};
```

- `addObject` thêm một group (id: tham số, nếu không thì `objectName`, nếu không thì tên class; thêm `_2`, `_3`, ... cho
  khỏi trùng) với mỗi Q_PROPERTY một property: `bool`→Bool, `int`→Int, `qint64`→Int64, `double`/`float`→Double,
  `QString`→String, `Q_ENUM`→Enum (key làm nhãn, giá trị int), kiểu khác đã đăng ký trong `TypeRegistry` theo storage type.
  Flags và kiểu khác bị bỏ qua. Không ghi được → read-only. Giá trị của object là giá trị mặc định.
- Tập property mặc định: của class và các lớp cha, trừ của `QObject` (`objectName`). `Q_CLASSINFO("qpb:properties", "a,b")`
  chọn và sắp thứ tự.
- Metadata `Q_CLASSINFO("qpb:<prop>", "min=0;max=10;suffix= m")`: key `type` (một TypeId, vd. `filepath`), `displayName`,
  `toolTip`, `readOnly`, `hidden`, `disabled`, `exclude`, và mọi khóa attribute (`min`/`max` = `minimum`/`maximum`); key
  không có `=giá trị` nghĩa là `true`.
- Đồng bộ: model → object khi `valueChanged` (`QMetaProperty::write`; nếu object chỉnh hoặc từ chối giá trị, model hiện
  giá trị của object); object → model qua signal NOTIFY (`Property::setValue`); chặn vòng lặp. Object bị hủy → group bị
  xóa. Source bị hủy → group vẫn còn, ngừng đồng bộ. [D41]
- **Tiêu đề (1.3):** `Q_CLASSINFO("qpb:title", "name")` đặt tên hiển thị của group là giá trị của Q_PROPERTY đó, cập nhật
  qua signal NOTIFY (giá trị rỗng giữ tiêu đề cũ). Với class ứng dụng không sửa được, `setTitleProperty("name")` làm
  tương tự cho các object thêm sau đó; class info được ưu tiên. Id của group (và mọi path) vẫn là tên object. [D43]
- **Giá trị live (1.3):** khóa metadata `live` biến property thành live (§4.2). `setLiveReadOnlyProperties(true)` biến
  mọi Q_PROPERTY có signal NOTIFY nhưng không có WRITE thành live, cho các object thêm sau đó. Mặc định `false`: giữ
  hành vi của 1.2. [D44]
- **Điều kiện (1.4):** khóa metadata `enabledWhen=<tên>` và `visibleWhen=<tên>`: property được bật / hiện khi Q_PROPERTY
  `<tên>` của cùng object có giá trị "đúng" (§4.2).

---

## 5. Widgets (`qpb::widgets`)

### 5.1 `EditorFactory`

```cpp
struct EditorHandler {
    std::function<QWidget*(QWidget* parent, const Property&)> createEditor;        // required
    std::function<void(QWidget*, const QVariant&, const Property&)> setEditorData; // required
    std::function<QVariant(QWidget*, const Property&)> editorData;                 // required
    std::function<void(QPainter*, const QStyleOptionViewItem&, const QVariant&, const Property&)> paint; // optional
    std::function<void(QWidget*, const Property&)> applyAttributes;                // optional
};

class EditorFactory {
public:
    static EditorFactory& global();
    bool registerEditor(const TypeId& id, const EditorHandler& h);
    bool replaceEditor (const TypeId& id, const EditorHandler& h);
    bool contains(const TypeId& id) const;
    const EditorHandler* handler(const TypeId& id) const;
    const EditorHandler* handlerFor(const Property& p) const;  // editorId attribute, then typeId
    QList<TypeId> editors() const;
    QWidget* createEditor(QWidget* parent, const Property& p) const;
    static void notifyCommit(QWidget* editor);                // commit now, don't wait for focus-out
};

class EditorDialogScope {                                     // RAII: editor shows a modal dialog
public:
    explicit EditorDialogScope(QWidget* editor);
    ~EditorDialogScope();
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
| Int64    | `Int64SpinBox` nội bộ (`QAbstractSpinBox` cho qint64) (1.2)   | Như Int; bước tăng/giảm dừng ở giới hạn                                 |
| Double   | `QDoubleSpinBox`                                              | Như Int + `decimals`; khi không edit hiển thị tối đa `decimals` chữ số thập phân theo `QLocale`, bỏ số 0 thừa |
| String   | `QLineEdit`; `QPlainTextEdit` khi `multiline` (1.1)           | `maxLength`, `placeholder`, `QRegularExpressionValidator` từ `regularExpression`; nhiều dòng: `placeholder`, cao tối thiểu 4 dòng, Tab chuyển tiếp |
| Enum     | `QComboBox` (không editable)                                  | Commit ngay khi chọn (`notifyCommit` trên `activated`)                  |
| FilePath | `PathEdit` nội bộ = `QLineEdit` + `QToolButton "…"`           | Nút mở `QFileDialog::getOpenFileName`/`getSaveFileName` theo `dialogMode`; chọn xong → `notifyCommit` |
| DirPath  | `PathEdit` nội bộ (chế độ thư mục)                            | `QFileDialog::getExistingDirectory`                                    |

Hiển thị đường dẫn dài trong ô (không edit): elide ở giữa (`Qt::ElideMiddle`), tooltip là đường dẫn đầy đủ.

### 5.4 `PropertyDelegate : QStyledItemDelegate`

- `createEditor/setEditorData/setModelData` ủy quyền cho `EditorFactory`. `setModelData` gọi `model->setData`;
  nếu trả `false` (validation lỗi) thì editor vẫn đóng, model giữ giá trị cũ, view hiển thị thông báo lỗi
  (tooltip tại ô, `QToolTip::showText`) — hành vi 1.0. [Quyết định D5]
- `paint`: dùng `EditorHandler::paint` nếu có; checkbox lấy từ `CheckStateRole`; group vẽ nền `QPalette::Button`, chữ đậm; tên property
  đã sửa in đậm; giá trị read-only (enabled nhưng không sửa được, không phải checkbox) dùng màu `QPalette::PlaceholderText`; chữ giá trị
  bị cắt ở giữa khi dài. [D30]
- **Focus khi mở dialog:** editor đang mở dialog modal giữ một `qpb::EditorDialogScope` (public, để editor tùy biến như nút chọn màu
  cũng dùng được; `PathEdit` nội bộ cũng dùng nó). `PropertyDelegate::eventFilter` bỏ qua `FocusOut` khi scope còn sống,
  nên editor không bị đóng/commit giữa chừng. [Quyết định D6, D24]
- Phím: **Enter** commit + đóng; **Esc** hủy; **Tab/Shift+Tab** commit rồi mở editor ở ô Value kế tiếp/trước đó có thể edit
  (bỏ qua group và read-only); focus-out commit. Trong editor nhiều dòng (1.1) **Enter** xuống dòng, **Ctrl+Enter** commit.
  Editor cao hơn hàng (nhiều dòng) giãn xuống dưới, hoặc lên trên khi ở cuối viewport.

### 5.5 `PropertyTreeView : QTreeView`

```cpp
class PropertyTreeView : public QTreeView {
public:
    enum class Mode { Tree, List };
    explicit PropertyTreeView(QWidget* parent = nullptr);
    void setModel(QAbstractItemModel* model) override; // chấp nhận PropertyModel hoặc proxy của nó
    void setMode(Mode m);  Mode mode() const;
    int nameColumnWidth() const;
    void setNameColumnWidth(int px);
    PropertyDelegate* propertyDelegate() const;
    void setTabStopsOnCheckBoxes(bool on);  bool tabStopsOnCheckBoxes() const;   // 1.5, Q_PROPERTY, mặc định false
};
```

- Cột tên tự giãn theo nội dung (tối đa 60% view) cho tới khi độ rộng được đặt bằng `setNameColumnWidth()` hoặc kéo header. [D35]
- Mặc định: `editTriggers = CurrentChanged | SelectedClicked | EditKeyPressed`; 2 cột; header có thể resize;
  `alternatingRowColors = true`; `setUniformRowHeights(true)`; ẩn hàng theo `IsVisibleRole` và cập nhật khi `dataChanged`/insert.
- **Mode::Tree:** group có thể thu gọn, mặc định mở; group hàng dùng `setFirstColumnSpanned(true)`.
- **Mode::List:** *không dùng proxy làm phẳng*. Cùng model, `rootIsDecorated = false`, `indentation = 0`,
  `itemsExpandable = false`, `expandAll()` và giữ expand khi có hàng mới; group hiển thị như section header (spanned, không thu gọn được). [Quyết định D4]
- Context menu trên ô property: **Reset to default** (disabled nếu không modified hoặc read-only); trên group: **Reset group**.
- Chuyển mode không tạo lại model, không mất giá trị, không mất selection hiện tại.
- Override `moveCursor()` để `MoveNext`/`MovePrevious` (Tab / Shift+Tab khi đang sửa) nhảy tới giá trị sửa được kế tiếp,
  bỏ qua group, hàng read-only, checkbox và hàng bị ẩn.
- `setTabStopsOnCheckBoxes(true)` (1.5, mặc định tắt) đưa checkbox vào chuỗi đó: Tab / Shift+Tab dừng cả ở ô giá trị của
  property Bool mà người dùng đổi được (enabled, không read-only, đang hiện). Ở đó không mở editor; Space bật tắt nó, và
  Tab / Shift+Tab tiếp theo đi tiếp theo chuỗi, ra khỏi view ở hai đầu như trước. Form view không cần tùy chọn này:
  checkbox của nó là widget nằm trong chuỗi focus thông thường. [D49]

### 5.6 `PropertyFormView : QScrollArea` (1.1)

```cpp
class PropertyFormView : public QScrollArea {
public:
    explicit PropertyFormView(QWidget* parent = nullptr);
    void setModel(QAbstractItemModel* model);           // PropertyModel hoặc proxy của nó; không sở hữu
    QAbstractItemModel* model() const;
    QWidget* editor(const QString& path) const;         // nullptr với group và path bị lọc
    bool isExpanded(const QString& groupPath) const;    // lưu theo path, mặc định true
    void setExpanded(const QString& groupPath, bool expanded);
};
```

- Các property liền nhau của một group dùng chung một `QFormLayout` (label | editor). Mỗi group là một **section** thu gọn
  được: `QToolButton` tiêu đề checkable (chữ đậm, mũi tên) và phần thân thụt lề. Trạng thái thu gọn lưu theo path, giữ qua
  các lần dựng lại. [D37]
- Mỗi property có một editor **thường trực** tạo bởi `EditorFactory` (Bool dùng `QCheckBox`), hiển thị có khung.
  Kiểu không có editor hiển thị bằng `QLabel` chỉ đọc (chọn được chữ) với `displayText`.
- Commit (R3, `model->setData`): khi mất focus (trừ khi `EditorDialogScope` hoặc popup đang mở, hoặc focus chuyển bên trong
  editor), khi nhấn Enter (Ctrl+Enter với text nhiều dòng), ngay lập tức với nút checkable (`toggled`) và editor gọi
  `notifyCommit` (combo box, path). Giá trị không đổi thì không ghi. Giá trị bị từ chối → editor hiện lại giá trị của model +
  tooltip lỗi. **Esc** đưa giá trị của model trở lại editor.
- Đồng bộ ngược: `dataChanged` → label (tên hiển thị, đậm khi modified, tooltip), hiển thị, enabled/read-only,
  `applyAttributes` khi `AttributesRole` đổi, `setEditorData` khi giá trị khác (chặn signal).
  `rowsInserted/rowsRemoved/rowsMoved/modelReset/layoutChanged` → form được **dựng lại một lần**, khi quay về event loop hoặc
  khi gọi `editor()` / `setExpanded()`; editor đang có focus được commit trước và nhận lại focus sau đó; widget cũ bị xóa
  sau (editor có thể đang chạy handler của chính nó). [D38]
- Tôn trọng `visible` (ẩn label + editor, hoặc cả section), `enabled` (label và editor bị disable) và `readOnly`
  (editor bị disable, label bình thường).
- Menu chuột phải trên label: **Reset to default**; trên tiêu đề section: **Reset group** (cùng quy tắc với tree view, D34).
- Không dùng `QDataWidgetMapper` (không hỗ trợ cây).

### 5.7 Tìm kiếm / lọc (1.1)

`PropertyFilterProxyModel : QSortFilterProxyModel` (trong `qpb::core`) với `recursiveFilteringEnabled = true`, lọc theo
`displayName` (cột tên, `DisplayRole`), mặc định không phân biệt hoa thường. Chuỗi lọc đặt bằng hàm chuẩn
`setFilterFixedString()` / `setFilterRegularExpression()`. Group hiển thị khi có con cháu khớp; khi tên group khớp thì mọi
con cháu của nó cũng hiển thị (kiểm tra trong `filterAcceptsRow()`, không dùng `autoAcceptChildRows`, để lớp con vẫn thấy
mọi hàng). Property bị ẩn không bao giờ khớp. `PropertyTreeView` và `PropertyFormView` làm việc với proxy. [D39]

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
│   ├── cmake/                     # helper và template CMake nội bộ
│   ├── include/qpb/               # header public (duy nhất) — §9
│   │   ├── qpb.h                  # umbrella header: tất cả (cần qpb::widgets)
│   │   ├── qpbcore.h              # umbrella header chỉ cho qpb::core
│   │   ├── qpbglobal.h            # export macro, QPB_VERSION*, QPB_DEPRECATED
│   │   ├── Property.h  PropertyGroup.h  PropertyModel.h  TypeRegistry.h ...
│   │   └── widgets/PropertyTreeView.h  EditorFactory.h ...
│   └── src/                       # nội bộ: *.cpp, *_p.h
│       ├── core/
│       └── widgets/
├── CMakeLists.txt                 # dev: add_subdirectory(qpb) + tests + examples
├── tests/   (core, widgets, api_compat, consumer)
├── examples/ (quickstart, custom_type, inspector, ...; qtcreator/: dự án CMake và qmake cho Qt Creator)
├── tools/
└── docs/
```

`tests/` và `examples/` dùng `qpb` **đúng như một consumer** (chỉ link `qpb::core` / `qpb::widgets`),
nên mọi lỗi tích hợp lộ ra ngay trong repo.

### 6.2 Cách dùng trong project khác

```text
my-app/
├── CMakeLists.txt
├── components/
│   └── qpb/        ← component folder: zip phát hành, git subtree hoặc git submodule (bên dưới)
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

**Cập nhật thư viện:** thay nguyên cả `components/qpb/` bằng bản mới (không chép đè file vào folder cũ: file mà bản
mới đã bỏ không được sót lại), build lại, và đọc `CHANGELOG.md` mục của bản mới.
Không cần sửa CMake hay code của consumer khi cùng major version (G5, S6).

Ba cách được hỗ trợ để lấy và cập nhật folder, do consumer chọn. Cả ba đều cho ra cùng một `components/qpb/` với nội
dung của file zip phát hành, nên đoạn CMake ở trên giống nhau cho mọi cách. [D47]

| Cách                     | Phù hợp với                       | Cố định version bằng                            |
|--------------------------|-----------------------------------|-------------------------------------------------|
| Zip phát hành (mặc định) | Mọi project, không cần git        | File zip đã giải nén (`components/qpb/VERSION`) |
| `git subtree`            | Project git commit luôn cả folder | Tag trong lệnh `add` / `pull`                   |
| `git submodule`          | Project git vốn đã dùng submodule | Commit được ghi cho `components/qpb`            |

Mỗi bản phát hành công bố cho các cách trên (xem `docs/RELEASING.md`):

- `qpb-X.Y.Z.zip` trên GitHub Release của tag `vX.Y.Z`: chỉ gồm folder `qpb/`;
- nhánh `qpb-release`: lịch sử của riêng `qpb/` (`git subtree split`), đỉnh nhánh luôn là bản phát hành mới nhất;
- tag `qpb-vX.Y.Z` trên commit của bản đó trong `qpb-release`. `vX.Y.Z` gắn cho cả repo nên subtree và submodule không
  dùng được, vì chúng cần component nằm ở gốc.

```sh
# Zip phát hành — lần đầu và mọi lần cập nhật
rm -rf components/qpb && unzip qpb-X.Y.Z.zip -d components     # zip chứa qpb/

# git subtree — folder được commit trong repo của consumer
git subtree add  --prefix components/qpb <remote> qpb-vX.Y.Z --squash     # lần đầu
git subtree pull --prefix components/qpb <remote> qpb-vX.Y.Z --squash     # cập nhật

# git submodule — repo của consumer ghi một commit của qpb-release
git submodule add -b qpb-release <remote> components/qpb                  # lần đầu, rồi cố định version:
git -C components/qpb fetch --tags && git -C components/qpb checkout qpb-vX.Y.Z
git add components/qpb && git commit -m "Use qpb X.Y.Z"                   # cập nhật: lặp lại hai dòng này
```

Với submodule, `git submodule update --remote components/qpb` chuyển lên bản phát hành mới nhất (đỉnh của
`qpb-release`); khi clone nó tải cả lịch sử phát triển, nhưng chỉ checkout component. Sửa đổi cục bộ trong folder sẽ bị
zip ghi đè, thành conflict khi merge với subtree, và phải commit bên trong submodule; những sửa đổi đó nên đưa về
upstream.

### 6.3 Yêu cầu với `qpb/CMakeLists.txt` (để không "làm bẩn" project chủ)

- Dùng `CMAKE_CURRENT_SOURCE_DIR`/`CMAKE_CURRENT_BINARY_DIR`, **không bao giờ** `CMAKE_SOURCE_DIR`.
- Không đổi biến global (`CMAKE_CXX_STANDARD`, `CMAKE_CXX_FLAGS`, `CMAKE_AUTOMOC`, output dir...).
  Mọi thiết lập qua `target_*` và `set_target_properties(... AUTOMOC ON)` trên target của qpb.
- `target_compile_features(qpb_core PUBLIC cxx_std_17)` — chỉ yêu cầu *tối thiểu*, consumer dùng C++20 vẫn được.
- Cờ cảnh báo và `QT_NO_CAST_FROM_ASCII` là **PRIVATE**; header public phải sạch cảnh báo dưới
  `-Wall -Wextra -Wpedantic` / `/W4` của consumer.
- Chỉ gọi `find_package(Qt6 6.5 ... Core Widgets)` nếu target `Qt6::Widgets` chưa tồn tại; nếu đã có thì kiểm tra version Qt.
- Tên target có tiền tố `qpb_`; alias `qpb::core`, `qpb::widgets`. Option có tiền tố `QPB_`.
- Không có tests/examples trong folder `qpb/`; không `install()` mặc định (option `QPB_INSTALL`, OFF).
- **Không dùng Qt resource (`.qrc`)** trong 1.x: static lib cần `Q_INIT_RESOURCE` ở phía consumer → vi phạm "2 dòng CMake".
  Icon lấy từ `QStyle::standardIcon` hoặc vẽ bằng code.
- **Không dựa vào static initializer** để đăng ký kiểu (linker có thể loại bỏ khi link static).
  Kiểu cơ bản đăng ký lười trong `TypeRegistry::global()` / `EditorFactory::global()`.
- Build shared (`QPB_BUILD_SHARED=ON`) vẫn hỗ trợ; khi đó consumer phải deploy thêm DLL/so — ghi rõ trong README.
- `VERSION` chứa `MAJOR.MINOR.PATCH` kèm hậu tố pre-release tùy chọn (`1.0.0-rc1`); `QPB_VERSION_STR` giữ chuỗi đầy đủ.
  Gọi `project()` **không** kèm `VERSION`: trong thư mục con nó sẽ ghi `CMAKE_PROJECT_VERSION` của project chủ nếu project chủ
  không khai báo version. Sửa `VERSION` sẽ khiến CMake chạy lại (`CMAKE_CONFIGURE_DEPENDS`). [D31, D32]

## 7. Kiểm thử

| Tầng          | Công cụ                        | Nội dung bắt buộc                                                                 |
|---------------|--------------------------------|-----------------------------------------------------------------------------------|
| Core          | Qt Test (không cần GUI)        | builder, path/find, trạng thái hiệu lực kế thừa, registry, pipeline validation   |
| Model         | Qt Test + `QAbstractItemModelTester` (mode Fatal) | mọi thao tác cấu trúc/giá trị; signal (`QSignalSpy`); batch       |
| Widgets       | Qt Test, `QT_QPA_PLATFORM=offscreen` | tạo/commit/hủy editor cho 7 kiểu; Enter/Esc/Tab; focus-out; mode Tree↔List; reset menu; PathEdit không đóng khi dialog mở (dialog được thay bằng hook test) |
| Example       | build trong CI                  | S1 và S2 được đo tự động bằng script đếm dòng                                     |
| API compat    | chỉ biên dịch + chạy             | §9.5: mã client của mọi bản 1.x đã phát hành vẫn build và chạy đúng               |
| Consumer      | CMake project mẫu `tests/consumer` | Chép `qpb/` vào `components/`, build; kiểm tra không rò rỉ biến global; build với C++17 và C++20; static và shared |

`PathEdit` nội bộ có hook cho test (trong `src/`, không public) để test thay `QFileDialog` bằng hàm trả giá trị cố định.

CI: GitHub Actions, ma trận Ubuntu/Windows/macOS × Qt 6.5 / 6.8, qua `jurplel/install-qt-action`.

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
| 1.2   | `QObjectPropertySource` (đọc `Q_PROPERTY`, metadata qua `Q_CLASSINFO("qpb:<prop>", "min=0;max=10")`, đồng bộ hai chiều); serialize `qpb::serialization::toJson/fromJson/save/load` (§4.8); `Types::Int64`; example `QUndoStack` | Bổ sung |
| 1.3   | `Property::Flag::Live` (§4.2); tiêu đề và giá trị live của `QObjectPropertySource` (§4.9)                   | Bổ sung       |
| 1.4   | Điều kiện `enabledWhen`/`visibleWhen` (§4.2); `PropertyModel::onValueChanged()` (§4.6)                      | Bổ sung       |
| 1.5   | `PropertyModel::resetAllToDefault()` (§4.6); `PropertyTreeView::setTabStopsOnCheckBoxes()` (§5.5)           | Bổ sung       |
| 2.0   | Chỉ khi thật sự cần phá vỡ API; gom mọi thứ đã deprecate                                                    | Breaking      |

**Thiết kế 1.0 phải "chừa chỗ" cho 1.1/1.2** mà không đổi API: Form view và filter là class mới dùng
`PropertyModel` có sẵn; `QObjectPropertySource` chỉ cần API public của `PropertyGroup`/`PropertyModel`
(`add`, `setValue`, `valueChanged`); `Int64` là `TypeId` mới; serialize là hàm tự do mới.
Việc kiểm tra điều này là một task riêng trong PLAN (M1.3).

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
- Hàm tự do thêm sau 1.0 nằm trong **namespace lồng** (vd. `qpb::serialization`), không đặt trực tiếp trong `qpb`:
  argument-dependent lookup sẽ tìm thấy chúng với tham số kiểu của qpb và có thể làm lời gọi không kèm namespace của ứng
  dụng bị mơ hồ. `tests/api/future_sketches.cpp` và `tests/api_compat/v1_2.cpp` có sẵn các hàm như vậy của ứng dụng. [D40]

### 9.4 Version & deprecation

- `qpbglobal.h` cung cấp `QPB_VERSION_MAJOR/MINOR/PATCH`, `QPB_VERSION_STR`, `QPB_VERSION`, `QPB_VERSION_CHECK(maj, min, pat)` (sinh từ file `VERSION`),
  và hàm runtime `qpb::version()`.
- API bị thay thế được đánh dấu `QPB_DEPRECATED_X("dùng X thay thế")` (map sang `[[deprecated]]`), giữ **đến hết 1.x**.
  Khai báo deprecated được bọc trong `#if !defined(QPB_DISABLE_DEPRECATED)`, nên consumer muốn chủ động dọn có thể
  định nghĩa `QPB_DISABLE_DEPRECATED` để nhận lỗi biên dịch ở mọi chỗ còn dùng API deprecated.
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
| D17| C++17, Qt ≥ 6.5, namespace `qpb`, chỉ CMake — đã xác nhận                   | Chốt trước M1 vì bị khóa đến 2.0                                                       |
| D18| Code và tài liệu dùng tiếng Anh; bản `-vi` chỉ để tham khảo                  | Quy ước dự án                                                                          |
| D19| "Custom property table" = người dùng tự dựng property table bằng API public (G1, G3) | Bạn đã làm rõ; không phải tính năng riêng                                        |
| D20| Thiết kế API và giai đoạn RC dùng kịch bản tham chiếu thay cho project thật  | Hiện chưa có project thật nào dùng thư viện                                            |
| D21| Header public là tham chiếu API chuẩn; code trong SPEC chỉ tóm tắt          | Tránh hai nguồn sự thật sau M1                                                         |
| D22| TypeId và khóa attribute là hằng `constexpr QLatin1StringView`              | Không lo thứ tự khởi tạo static; tự chuyển sang `QString`                              |
| D23| Trạng thái riêng dạng `Property::Flags` (ReadOnly/Disabled/Hidden) + getter hiệu lực | Một enum mở rộng được thay cho từng cặp getter                                  |
| D24| `EditorDialogScope` public; `PathEdit` giữ nội bộ                            | Editor tùy biến có mở dialog (vd. màu) cần cùng cơ chế bảo vệ focus                    |
| D25| Builder là class cụ thể trên nền CRTP `PropertyBuilderBase`                 | Setter theo kiểu chỉ có ở kiểu phù hợp; class không phải template, export được         |
| D26| Khóa attribute dùng từ đầy đủ (`minimum`, `regularExpression`, `dialogMode`) | Dễ đọc; enum `qpb::FileMode` lưu chế độ dialog                                        |
| D27| `qpb/qpbcore.h` cho người chỉ dùng core; `qpb/qpb.h` gồm cả widgets         | Consumer chỉ dùng core không cần QtWidgets                                             |
| D28| Validator nhận cả property (`(value, property)`) như `TypeHandler::validate` | Một chữ ký thống nhất; validator đọc được attribute                                   |
| D29| Id trùng hoặc không hợp lệ chỉ ghi cảnh báo, không assert                     | Assert sẽ làm sập bản debug của ứng dụng dùng thư viện; cây vẫn nhất quán              |
| D30| Hàng group dùng `QPalette::Button`; giá trị read-only được làm mờ          | `AlternateBase` trùng màu hàng xen kẽ; read-only cần dấu hiệu nhận biết               |
| D31| Component gọi `project()` không kèm `VERSION`; version lấy từ `qpb/VERSION`  | `project(VERSION)` ghi đè `CMAKE_PROJECT_VERSION` của project chủ (test rò rỉ phát hiện) |
| D32| `VERSION` có thể có hậu tố pre-release; CMake chạy lại khi file đổi         | Bản release candidate (`1.0.0-rc1`); thay folder phải cập nhật header version          |
| D33| `qpb/` chỉ dùng ASCII (luật R6)                                              | MSVC cảnh báo C4819 trên code page không phải UTF-8 làm hỏng consumer dùng `/WX`        |
| D34| Read-only/disabled chỉ chặn người dùng sửa (`setData`), không chặn code ứng dụng | RC: ứng dụng không cập nhật được ô trạng thái read-only của chính nó; reset group phụ thuộc thứ tự |
| D35| Cột tên tự giãn theo nội dung cho tới khi độ rộng được đặt rõ ràng           | RC: ảnh chụp cho thấy tên bị cắt ở độ rộng mặc định                                   |
| D36| String nhiều dòng là attribute của `String`, không phải kiểu mới; ô nối các dòng bằng ¶ | Lưu trữ và validation giữ nguyên; ô chỉ đủ chỗ cho một dòng          |
| D37| Section của form dùng `QToolButton` tiêu đề checkable thay cho `QGroupBox`  | `QGroupBox` không thu gọn được nếu không có check box, mà check box dễ hiểu là "bật/tắt" |
| D38| Form view dựng lại một lần mỗi vòng event loop khi cấu trúc đổi; truy vấn thì dựng lại ngay | Đổi bộ lọc phát nhiều signal hàng; editor có thể bị thay khi đang commit |
| D39| Proxy lọc chấp nhận con cháu của group khớp trong `filterAcceptsRow()`      | `autoAcceptChildRows` bỏ qua `filterAcceptsRow()`, làm mất quy tắc của lớp con         |
| D40| Hàm tự do mới nằm trong namespace lồng (`qpb::serialization`)               | Hàm trong `qpb` nhận kiểu của qpb bị ADL tìm thấy và làm hỏng lời gọi `save(group, settings)` không kèm namespace của ứng dụng (phát hiện nhờ `future_sketches.cpp`) |
| D41| `QObjectPropertySource` đọc metadata từ `Q_CLASSINFO`, cần một model, xóa group của object đã bị hủy | Không phải sửa class được hiển thị; thay đổi property chỉ quan sát được qua `valueChanged` của model |
| D42| Serialization bỏ qua property read-only                                      | Ứng dụng tự quản lý chúng; nạp lại sẽ ghi đè vd. version đang chạy bằng giá trị đã lưu |
| D43| `QObjectPropertySource` lấy tiêu đề group từ một Q_PROPERTY (`qpb:title`, `setTitleProperty()`); id vẫn là tên object | RC F8: group hiện `studio_mic`; path phải ổn định cho serialization và code ứng dụng |
| D44| Cờ mới `Live` (không bao giờ modified, bị reset group và serialization bỏ qua); Q_PROPERTY read-only có NOTIFY chỉ thành live khi gọi `setLiveReadOnlyProperties(true)` | RC F9: giá trị live trông như người dùng sửa. Tự động biến thành live sẽ đổi hành vi 1.2 (§9.2), nên phải bật |
| D45| `PropertyModel::onValueChanged(path, context, handler)`, theo path           | RC F3: chuỗi `if (path == ...)`; model quản lý path và signal thay đổi, `Property` không phải QObject; theo path nên còn sau `setRoot()` |
| D46| Điều kiện (`enabledWhen`/`visibleWhen`) kết hợp với cờ riêng, model đánh giá | RC F10: mọi ứng dụng tự nối quan hệ phụ thuộc; ghi `Disabled`/`Hidden` sẽ ghi đè `setEnabled()` của ứng dụng |
| D47| Ba cách đồng bộ component folder (mặc định zip phát hành, `git subtree`, `git submodule` trỏ tới `qpb-release`), đều cho ra `components/qpb/` với nội dung của zip; mỗi bản phát hành gắn thêm tag `qpb-vX.Y.Z` trên `qpb-release` | Trước đây subtree chỉ theo được bản mới nhất, còn submodule kéo cả repo phát triển về một đường dẫn khác (`components/property-browser/qpb`). Một đường dẫn chung giữ CMake của consumer không đổi; tag cho người dùng git cố định và nâng cấp đúng một version |
| D48| `PropertyModel::resetAllToDefault()` reset cả cây; `resetToDefault(QModelIndex())` vẫn trả về `false` | RC F4: root không có index nên ứng dụng phải đi qua `root()->resetToDefault()`. Gán ý nghĩa đó cho index không hợp lệ sẽ đổi hành vi đã ghi trong tài liệu (§9.2) |
| D49| Checkbox chỉ vào chuỗi Tab của tree view khi gọi `setTabStopsOnCheckBoxes(true)` | RC F5: người dùng bàn phím chỉ tới được checkbox bằng phím mũi tên. Đặt làm mặc định sẽ đổi hành vi bàn phím của mọi ứng dụng sau khi nâng cấp (§9.2), nên phải bật, giống D44 |
