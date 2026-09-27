# Property Browser cho Qt 6 — Bản ý tưởng đã tinh chỉnh

> Tài liệu này được phát triển từ bản nháp gốc (xem Phụ lục A) theo quy trình idea-refine:
> mở rộng (divergent) → đánh giá (convergent) → chốt phạm vi.
> Các chỗ đánh dấu **[Giả định]** là quyết định tạm thời, cần xác nhận ở mục 13.

---

## 1. Vấn đề cần giải (Problem Statement)

**How Might We** giúp lập trình viên Qt Widgets *khai báo một tập thuộc tính đúng một lần* và nhận được panel chỉnh sửa hoàn chỉnh (tree / list / form), mà không phải viết lại form thủ công cho mỗi project và không bị khóa vào một cách hiển thị duy nhất?

Nỗi đau thực tế:

- Mỗi tool/editor viết bằng Qt đều cần một panel "chỉnh thuộc tính của object đang chọn". Viết tay bằng `QFormLayout` + hàng chục widget mỗi lần vừa tốn công vừa không đồng nhất giữa các màn hình.
- QtPropertyBrowser (Qt Solutions) là chuẩn cũ từ thời Qt 4: API manager/factory rườm rà, không có tách model/view rõ ràng, và không được Qt bảo trì chính thức cho Qt 6.
- Các lib thay thế (QtnProperty...) buộc dùng DSL riêng hoặc gắn chặt vào một kiểu view.

---

## 2. Người dùng mục tiêu & Job to be done

**Người dùng chính [Giả định]:** chính bạn và các project C++/Qt Widgets bạn làm (internal tool, editor, ứng dụng kỹ thuật/CAD, dialog cấu hình). Đối tượng thứ hai: lập trình viên Qt khác nếu lib được public.

**Job to be done:**

> Khi tôi đang xây một tool bằng Qt và cần cho người dùng chỉnh cấu hình của một object,
> tôi muốn *khai báo* các thuộc tính (tên, kiểu, ràng buộc) rồi nhận UI sẵn dùng,
> để dành thời gian cho logic nghiệp vụ thay vì cho form.

- **Functional:** sinh UI chỉnh sửa từ mô tả thuộc tính; nhận thông báo khi giá trị đổi.
- **Emotional:** tự tin rằng thêm panel mới chỉ mất vài phút, không còn cảm giác "lại phải viết form".
- **Social:** sản phẩm trông chuyên nghiệp, nhất quán như Inspector của Unity/Godot hay PropertyGrid của .NET.

---

## 3. Tiêu chí thành công (đề xuất)

| #  | Tiêu chí                        | Cách đo                                                                 |
|----|---------------------------------|-------------------------------------------------------------------------|
| S1 | Tích hợp vào project mới nhanh  | ≤ 30 dòng code cho panel 10 thuộc tính, ≤ 30 phút kể cả CMake           |
| S2 | Mở rộng không cần sửa lib       | Thêm 1 kiểu dữ liệu mới (vd `QColor`) ≤ 100 dòng, nằm hoàn toàn ngoài lib |
| S3 | Một model, ba view              | Chuyển tree ↔ list ↔ form không đổi dữ liệu, chỉ đổi lớp view          |
| S4 | Thực sự được tái sử dụng        | Dùng trong ≥ 2 project thật trong 6 tháng sau v0.1                      |
| S5 | API ổn định                     | Không breaking change giữa các bản minor sau v1.0                       |

---

## 4. Bối cảnh & prior art

| Giải pháp                          | Điểm mạnh                                                  | Điểm yếu / bài học                                                        |
|------------------------------------|------------------------------------------------------------|---------------------------------------------------------------------------|
| QtPropertyBrowser (Qt Solutions)   | Chuẩn de-facto, có sẵn 3 kiểu browser (tree, groupbox, button) | API manager + factory rườm rà; không có model/view rõ ràng; Qt 6 chỉ có community port |
| QtnProperty                        | Nhiều kiểu, delegate tùy biến được                         | Cần DSL `.pef` + code generator; đường cong học cao                        |
| Qt Designer property editor        | UX chuẩn Qt, in đậm khi khác default                       | Nội bộ Qt, không tái sử dụng được                                         |
| .NET PropertyGrid                  | Reflection + attribute-driven; TypeConverter/UITypeEditor  | Ý tưởng "type registry + attribute" rất đáng học                          |
| Unity / Unreal / Godot Inspector   | Multi-object edit, conditional visibility, undo, array     | Chuẩn mực UX cao nhất, nhưng phạm vi rất lớn                              |

**Vì sao là lúc này:** Qt 6 đã ổn định (6.5 / 6.8 LTS), QtPropertyBrowser không còn được bảo trì chính thức, và Qt 6 có `QMetaType` mới cùng `QProperty`/`QBindable` có thể tận dụng. Khoảng trống "lib property browser gọn, model-first, cho Qt 6" là có thật.

---

## 5. Các hướng đã khám phá (Divergent)

Mỗi hướng dưới đây sinh ra từ một lăng kính khác nhau. Mục tiêu là đẩy xa hơn bản nháp trước khi hội tụ.

1. **Model-first, view mỏng** *(lens: đơn giản hóa)*
   Một `PropertyModel` (kế thừa `QAbstractItemModel`) là nguồn sự thật duy nhất; tree/list/form chỉ là ba cách render. Đây là cách duy nhất để "3 view" không biến thành "3 lib".

2. **Zero-config reflection** *(lens: loại bỏ)*
   Trỏ vào một `QObject` có `Q_PROPERTY`, lib tự sinh panel, không cần khai báo gì. Rất hấp dẫn, nhưng `Q_PROPERTY` không mang được metadata (min/max, file filter, nhóm) nên vẫn cần thêm một lớp annotate.

3. **Schema-driven** *(lens: thay thế)*
   Mô tả thuộc tính bằng JSON / `QVariantMap`, UI sinh từ schema. Hợp với app cấu hình bằng dữ liệu, plugin, script; đổi lại mất type-safety lúc compile.

4. **Inspector-grade** *(lens: 10x)*
   Multi-object editing, undo/redo, conditional visibility, array/struct lồng nhau, search. Đây là đích 2-3 năm, không phải v0.1.

5. **Headless core + widgets tùy chọn** *(lens: bỏ ràng buộc công nghệ)*
   Core (model, type, validation) không phụ thuộc `QtWidgets`; view Widgets là module riêng. Mở đường cho Qt Quick sau này và cho unit test không cần GUI.

6. **Chỉ là delegate, không phải widget** *(lens: đảo ngược)*
   Lib chỉ cung cấp editor + `QStyledItemDelegate` cắm vào bất kỳ `QTreeView`/`QTableView` có sẵn. Tối giản nhất, nhưng đẩy việc quản lý hierarchy về phía người dùng.

7. **Type ecosystem** *(lens: kết hợp)*
   Core ship 7 kiểu cơ bản; mọi kiểu khác (color, font, vector3, gradient, curve) là plugin đăng ký qua registry. Đây chính là chữ "custom" trong bản nháp, nhưng được cụ thể hóa thành cơ chế.

8. **Form-first cho settings dialog** *(lens: đổi đối tượng)*
   Phiên bản 10x đơn giản: chỉ sinh form từ struct/settings. Bỏ tree, bỏ group lồng nhau. Giải quyết 60% nhu cầu với 20% công sức.

---

## 6. Đánh giá & hội tụ (Convergent)

Gom 8 ý trên thành 3 hướng khác biệt thực sự:

- **A. Reflection-driven** (ý 2 + 4): `Q_PROPERTY` là nguồn sự thật, annotate thêm bằng `Q_CLASSINFO` hoặc attribute.
- **B. Explicit model + pluggable views/types** (ý 1 + 5 + 7): khai báo property tường minh qua builder API, một model, nhiều view, type registry mở.
- **C. Delegate toolkit** (ý 6): chỉ editor + delegate, bring-your-own view.

| Tiêu chí              | A. Reflection                                                  | B. Explicit model                                                    | C. Delegate toolkit                          |
|-----------------------|----------------------------------------------------------------|----------------------------------------------------------------------|----------------------------------------------|
| Giá trị người dùng    | Cao khi object đã là `QObject`; thấp khi dữ liệu là struct/JSON | Cao, phủ mọi nguồn dữ liệu                                            | Trung bình, người dùng vẫn phải tự lo model  |
| Khả thi               | Trung bình: thiếu metadata → phải phát minh cơ chế annotate     | Cao: toàn bộ là Qt model/view chuẩn                                   | Rất cao, nhưng nhỏ                           |
| Khác biệt             | Không nhiều (fork QtPropertyBrowser đã có `QtVariantPropertyManager`) | "Một model, ba view" + registry mở là điểm khác thật             | Yếu: chỉ là bộ editor                        |
| Phần khó nhất         | Ánh xạ metadata; đồng bộ hai chiều notify signal                | UX của editor (commit/cancel, keyboard, focus) và giữ 3 view nhất quán | Không có                                    |
| Rủi ro giết dự án     | Bị ép dùng `QObject` cho mọi thứ                                | Trừu tượng hóa quá sớm, mãi không xong                                | Không ai cần                                 |

**Ma trận quyết định:** **B** nằm ở ô *giá trị cao / khả thi cao* → làm trước.
**A** trở thành *adapter* trên B (v0.3): một `QObjectPropertySource` đọc `Q_PROPERTY` và đổ vào cùng model.
**C** là *sản phẩm phụ* miễn phí của B, vì delegate và editor factory vốn được expose public.

---

## 7. Hướng đề xuất (Recommended Direction)

Xây **thư viện C++17, Qt Widgets, model-first**: một cây `Property` tường minh, một `PropertyModel` chuẩn `QAbstractItemModel`, một `TypeRegistry` mở, và ba view mỏng dùng chung model + editor factory.

**Lý do:** nó trả lời trực tiếp cả ba mục tiêu trong bản nháp.
"Custom & reuse" đến từ registry và từ việc mọi thứ đều là Qt model/view chuẩn (người dùng có thể thay view bằng của họ).
"Ba dạng hiển thị" khả thi vì chúng chỉ là render khác nhau của cùng một model.
"Qt 6" được tận dụng qua `QMetaType` mới, và có thể `QProperty` ở các bản sau.

**Điều làm nó khác QtPropertyBrowser:** không có cặp manager/factory cho từng kiểu; property là dữ liệu thuần, kiểu là một entry trong registry, view không biết gì về kiểu cụ thể.

### 7.1 Kiến trúc (phác thảo)

```text
┌──────────────────────────────────────────────────────────────┐
│  Views (QtWidgets)                                            │
│  PropertyTreeView    PropertyListView    PropertyFormView     │
│        └── dùng chung PropertyDelegate ──┘       └── dùng EditorFactory trực tiếp
├──────────────────────────────────────────────────────────────┤
│  PropertyModel : QAbstractItemModel  (2 cột: Name | Value)    │
├──────────────────────────────────────────────────────────────┤
│  Core (không phụ thuộc QtWidgets)                             │
│  Property / PropertyGroup     TypeRegistry     Validators     │
└──────────────────────────────────────────────────────────────┘
```

- **Property:** `id`, `displayName`, `value` (`QVariant`), `defaultValue`, `typeId`, `attributes` (`QVariantMap`: min/max/step/suffix/options/filter...), flags (`readOnly`, `enabled`, `visible`), `tooltip`.
- **PropertyGroup:** Property có con → hierarchy. Group lồng nhau không giới hạn.
- **TypeRegistry:** map `typeId → TypeHandler { createEditor, setEditorData, editorData, displayText/paint, validate }`. 7 kiểu cơ bản đăng ký sẵn; người dùng đăng ký thêm.
- **PropertyModel:** adapter giữa cây Property và `QAbstractItemModel`; phát `valueChanged(Property&, QVariant)`; `setData` đi qua validation.
- **Views:**
  - *Tree* = `QTreeView` + `PropertyDelegate`.
  - *List* = cùng `QTreeView` nhưng flatten, group thành section header.
  - *Form* = `QScrollArea` + `QFormLayout`, group thành `QGroupBox`/collapsible section, editor tạo qua `EditorFactory` và tồn tại thường trực.

### 7.2 Phác thảo API (chưa chốt)

```cpp
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

PropertyModel model(root);
PropertyTreeView view;          // hoặc PropertyListView / PropertyFormView
view.setModel(&model);

QObject::connect(&model, &PropertyModel::valueChanged,
                 [](const Property& p, const QVariant& v) {
                     qDebug() << p.path() << "=" << v;
                 });
```

Đăng ký kiểu mới hoàn toàn từ phía người dùng, không đụng vào lib:

```cpp
TypeRegistry::global().registerType<QColor>({
    .createEditor  = [](QWidget* parent, const Property&) { return new ColorButton(parent); },
    .setEditorData = [](QWidget* e, const QVariant& v) { static_cast<ColorButton*>(e)->setColor(v.value<QColor>()); },
    .editorData    = [](QWidget* e) { return QVariant::fromValue(static_cast<ColorButton*>(e)->color()); },
    .displayText   = [](const QVariant& v) { return v.value<QColor>().name(); },
});
```

---

## 8. Phạm vi tính năng chi tiết (mở rộng từ bản nháp)

### 8.1 Kiểu dữ liệu cơ bản và attribute đi kèm

| Kiểu        | Editor mặc định                         | Attribute                                              | Ghi chú                              |
|-------------|-----------------------------------------|--------------------------------------------------------|--------------------------------------|
| Bool        | `QCheckBox` (vẽ inline trong cell)      | —                                                      | Toggle bằng click hoặc Space         |
| Integer     | `QSpinBox`                              | `min`, `max`, `step`, `prefix`, `suffix`               | Hỗ trợ `qint64`                      |
| Float       | `QDoubleSpinBox`                        | `min`, `max`, `step`, `decimals`, `prefix`, `suffix`   | Hiển thị rút gọn khi không edit      |
| String      | `QLineEdit`                             | `maxLength`, `placeholder`, `regex`, `multiline`       | `multiline` → `QTextEdit`            |
| Enum        | `QComboBox`                             | `options` (list of `{label, value}`)                   | `value` có thể là int hoặc string    |
| Path (file) | `QLineEdit` + nút "..." → `QFileDialog` | `filter`, `mode` (open/save), `defaultDir`, `mustExist` | Elide đường dẫn dài khi hiển thị    |
| Directory   | `QLineEdit` + nút "..." → dir dialog    | `defaultDir`, `mustExist`                              |                                      |

### 8.2 Ba chế độ hiển thị

|               | Tree                                   | List                               | Form                                    |
|---------------|----------------------------------------|------------------------------------|-----------------------------------------|
| Dùng cho      | Object nhiều tầng nhóm (inspector)     | Panel phẳng, gọn                   | Dialog cấu hình, wizard                 |
| Widget nền    | `QTreeView`                            | `QTreeView` (flatten)              | `QScrollArea` + `QFormLayout`           |
| Group         | Node collapse được                     | Section header, không edit được    | `QGroupBox` / collapsible               |
| Editor        | Qua delegate, tạo khi bắt đầu edit     | Như tree                           | Widget tồn tại thường trực              |
| Dùng chung    | Model + Delegate + EditorFactory       | Model + Delegate + EditorFactory   | Model + EditorFactory                   |

### 8.3 Tính năng chung

- **Group / hierarchy:** lồng không giới hạn; group có thể collapse mặc định; đường dẫn dạng `Transform/x`.
- **Custom property set** *(cách hiểu "custom property table" của bản nháp, cần xác nhận ở mục 13):* tập thuộc tính do project khai báo lúc runtime, không hard-code trong lib; thêm/xóa property được khi model đang sống.
- **Trạng thái:** `readOnly`, `enabled`, `visible` cho từng property và group.
- **Default & reset:** mỗi property có `defaultValue`; context menu "Reset to default"; in đậm khi khác default (giống Qt Designer).
- **Validation:** theo attribute (range, regex) và callback tùy ý; giá trị không hợp lệ bị từ chối, không lọt vào model.
- **Thông báo thay đổi:** signal cấp model (`valueChanged`) và cấp property; có batch update để tránh bão signal.
- **Tooltip / mô tả** cho từng property.
- **Tìm kiếm / lọc theo tên:** qua `QSortFilterProxyModel`, gần như miễn phí vì model là chuẩn Qt.

### 8.4 Cơ chế mở rộng (customization)

- Đăng ký kiểu mới qua `TypeRegistry` mà không sửa lib.
- Ghi đè editor cho *một* property cụ thể mà không cần tạo kiểu mới.
- Thay delegate, hoặc thay hẳn view bằng `QTreeView` riêng của người dùng.
- Style qua QSS bình thường vì toàn bộ là widget chuẩn Qt.

### 8.5 Đóng gói & tích hợp

- CMake ≥ 3.21; dùng được qua `find_package`, `FetchContent` hoặc submodule; target `qpb::core` và `qpb::widgets`.
- Qt ≥ 6.5 LTS **[Giả định]**, C++17, chỉ cần module Core + Widgets.
- Namespace `qpb` **[Giả định]**; không đặt mục tiêu header-only.
- License MIT **[Giả định]** để tối đa hóa tái sử dụng.
- Unit test bằng Qt Test cho core và model; demo app trong `examples/inspector`.

---

## 9. Giả định cần kiểm chứng

**Phải đúng (sai thì hủy hướng B):**

- [ ] Nhu cầu đủ thường xuyên: liệt kê được ≥ 3 project (đã có hoặc sắp làm) sẽ dùng lib này.
      *Cách test: viết ra tên project và panel cụ thể.*
- [ ] Các lib hiện có không đáp ứng: dành đúng 1 giờ thử bản port Qt 6 của QtPropertyBrowser và QtnProperty cho một use case thật.
      *Nếu một trong hai "đủ tốt", cân nhắc fork/wrap thay vì viết mới.*

**Nên đúng (sai thì đổi cách tiếp cận, không hủy):**

- [ ] Một `QAbstractItemModel` đủ để nuôi cả tree/list/form mà không cần dữ liệu riêng cho view.
      *Cách test: MVP dựng cả tree và form trên cùng model.*
- [ ] Builder API + `QVariant` đủ type-safe và tiện tay.
      *Cách test: viết demo 30 dòng, tự đánh giá độ "khó chịu".*
- [ ] UX editor (commit khi Enter/focus-out, hủy khi Esc, Tab sang property kế) làm được trong `QStyledItemDelegate` chuẩn mà không hack.
      *Cách test: prototype 1 ngày với Int + String.*

**Có thể đúng (chưa cần test):**

- [ ] Người dùng sẽ cần kiểu phức hợp (color, vector, array).
- [ ] Sẽ có người ngoài dùng nếu public.

---

## 10. Phạm vi MVP (v0.1) và roadmap

**Mục tiêu MVP:** chứng minh giả định "một model, nhiều view, kiểu mở" với chi phí thấp nhất. Nếu MVP không khiến bạn thấy hơi "thiếu", nghĩa là đã làm quá nhiều.

**Trong phạm vi v0.1:**

- Core: `Property`, `PropertyGroup`, `TypeRegistry`, 7 kiểu cơ bản với attribute ở mục 8.1.
- `PropertyModel` (`QAbstractItemModel`, 2 cột) + signal `valueChanged` + validation theo range/regex.
- `PropertyTreeView` với `PropertyDelegate`; list mode là tùy chọn `flatten` trên cùng view.
- `readOnly` / `enabled` / `visible`; `defaultValue` + reset.
- Đăng ký kiểu mới từ bên ngoài, chứng minh bằng ví dụ `QColor` trong `examples/`.
- CMake package, Qt Test cho core/model, 1 demo app.

**Roadmap sau MVP:**

| Bản   | Nội dung                                                                                   |
|-------|--------------------------------------------------------------------------------------------|
| v0.2  | `PropertyFormView`; search/filter; tooltip; multiline string; in đậm khi khác default       |
| v0.3  | `QObjectPropertySource` (reflection từ `Q_PROPERTY`); serialize JSON/`QSettings`; hướng dẫn tích hợp `QUndoStack` qua signal |
| v1.0  | Đóng băng API; tài liệu; ≥ 2 project thật đã dùng                                          |

---

## 11. Không làm (và lý do)

- **Qt Quick / QML** — lớp view khác hẳn. Core được thiết kế không phụ thuộc Widgets để sau này thêm được, nhưng không làm bây giờ.
- **Python binding (PySide6/shiboken)** — chi phí bảo trì lớn, chưa có nhu cầu xác nhận.
- **Multi-object editing** (chỉnh nhiều object cùng lúc, hiển thị "mixed values") — tính năng inspector-grade, nhân đôi độ phức tạp của model. Chỉ cần đảm bảo thiết kế `Property` không cấm việc này về sau.
- **Undo/redo tích hợp sẵn** — mỗi app có `QUndoStack` riêng; lib chỉ cần phát signal đủ thông tin (old/new value) để app tự đẩy command.
- **Kiểu phức hợp trong core** (color, font, vector, array, struct lồng) — chứng minh registry hoạt động bằng ví dụ `QColor`, còn lại để plugin hoặc bản sau.
- **Hỗ trợ Qt 5** — mục tiêu rõ ràng là Qt 6; giữ Qt 5 làm bẩn CMake và API.
- **DSL / code generator riêng** (như `.pef` của QtnProperty) — đi ngược tiêu chí "tích hợp trong 30 phút".
- **Hệ thống theme riêng** — QSS đã đủ.
- **Conditional visibility khai báo** kiểu `visibleIf("mode == Ortho")` — app làm được bằng callback + `setVisible`; DSL biểu thức là quá sớm.

---

## 12. Rủi ro (pre-mortem: "12 tháng sau, dự án thất bại vì...")

| Kịch bản thất bại                                              | Dấu hiệu sớm                                         | Phòng ngừa                                                              |
|----------------------------------------------------------------|------------------------------------------------------|-------------------------------------------------------------------------|
| Trừu tượng hóa quá mức, không bao giờ "xong"                   | Sau 4 tuần chưa có demo chạy được                    | Time-box MVP 3-4 tuần; TreeView trước, mọi thứ khác sau                 |
| Ba view drift, mỗi cái một kiểu bug                            | Có code `switch` theo kiểu dữ liệu nằm trong view    | Rule: view không được biết kiểu; mọi thứ đi qua registry                |
| UX editor nửa vời (mất giá trị khi focus-out, Tab không đi tiếp) | Người dùng demo phàn nàn ngay lần đầu              | Dành riêng 1 sprint cho editor UX; test tương tác bằng `QTest`          |
| API đổi liên tục, project cũ không nâng cấp được               | Project thứ 2 phải sửa nhiều để dùng bản mới         | SemVer; không public API nào chưa dùng ở ≥ 2 chỗ                        |
| Chỉ mình tác giả dùng                                          | Không có ai ngoài thử                                | Không sao, nhưng đừng tốn công vào docs/CI cho public trước v1.0        |

---

## 13. Câu hỏi mở (cần trả lời để chốt)

Tài liệu này được viết khi chưa có câu trả lời của bạn, nên các mục dưới đây đang dùng giả định. Trả lời ngắn từng câu là đủ để cập nhật lại.

1. **Cho ai?** Chỉ cho project của bạn, hay dự định public/open-source?
   *(Giả định: cho bạn trước, public sau v1.0.)*
2. **Thành công trông như thế nào?** Bạn đồng ý với S1-S5 ở mục 3 không, hay có thước đo khác?
   *(Giả định: S1-S4 là bắt buộc, S5 sau v1.0.)*
3. **Ràng buộc kỹ thuật:** Qt Widgets thôi hay cần cả Qt Quick? Qt tối thiểu 6.5 hay 6.2? C++17 hay C++20? License?
   *(Giả định: Widgets, Qt 6.5, C++17, MIT.)*
4. **Đã thử gì trước đây?** Bạn đã dùng QtPropertyBrowser hoặc QtnProperty chưa, và điều gì khiến bạn muốn viết mới?
   *(Câu trả lời này quyết định giả định "phải đúng" thứ 2 ở mục 9.)*
5. **"Custom property table" nghĩa là gì?** (a) tập property do project tự định nghĩa lúc runtime, (b) thêm cột tùy ý vào bảng, hay (c) tùy biến cách hiển thị bảng?
   *(Giả định: a.)*
6. **Nguồn dữ liệu chính là gì?** `QObject` có `Q_PROPERTY`, struct C++ thuần, hay JSON/`QVariantMap`?
   *(Ảnh hưởng thứ tự ưu tiên của v0.3.)*
7. **Thời gian:** bạn dành được bao nhiêu tuần cho MVP?
   *(Giả định: 3-4 tuần bán thời gian.)*

---

## Phụ lục A — Bản nháp gốc

```text
Mục tiêu:
    - Tạo ra một library dạng property browser.
    - Hoạt động với Qt framework từ version 6.
    - Có khả năng custom và reuse cao trong nhiều project.

Các dạng hiển thị:
    - Dạng tree
    - Dạng list
    - Dạng form

Các tính năng:
    - Hỗ trợ custom property table
    - Có dạng group (hỗ trợ hierarchy)

Các dạng biến sẽ hỗ trợ trong property browser:
    - Bool
    - Integer
    - Float
    - String
    - Enum (String list selection)
    - Path (file path selection)
    - Directory (folder selection)
```
