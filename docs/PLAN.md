# qpb — Kế hoạch triển khai

> Đặc tả: [`SPEC.md`](SPEC.md). Ý tưởng gốc: [`brainstorm.md`](brainstorm.md).
> Giả định nguồn lực: **bán thời gian (~15 giờ/tuần)**, một người. v0.1 time-box **4 tuần**.
> Nếu hết tuần 4 mà M3 chưa xong: **cắt phạm vi, không kéo dài** (xem mục 5).

---

## 1. Tổng quan milestone

| Milestone | Tuần | Kết quả kiểm chứng được                                                    | Giả định được test (brainstorm §9)            |
|-----------|------|------------------------------------------------------------------------------|-----------------------------------------------|
| **M0** Kiểm chứng & khung   | 0 (2–3 ngày) | Quyết định "viết mới" có căn cứ; repo build được lib rỗng + test rỗng | Lib hiện có không đáp ứng; nhu cầu ≥ 3 project |
| **M1** Core + Model         | 1–2  | Toàn bộ `qpb::core` pass test, kể cả `QAbstractItemModelTester`               | Builder + `QVariant` đủ tiện tay              |
| **M2** Tree view + editor   | 2–3  | Demo inspector sửa được 7 kiểu, bàn phím đầy đủ                               | UX editor làm được trong `QStyledItemDelegate` |
| **M3** Hoàn thiện v0.1      | 4    | List mode, reset, QColor example, CMake install; tag `v0.1.0`                 | Một model nuôi nhiều view (một nửa)           |
| **M4** v0.2                 | 5–7  | Form view + filter; inspector chuyển 3 view                                   | Một model nuôi cả tree/list/form              |
| **M5** v0.3                 | 8–11 | QObject adapter, serialize, qint64                                            | —                                             |

Tổng thời gian đến v0.3 ước tính 11 tuần bán thời gian. Chỉ M0–M3 là cam kết; M4–M5 lập kế hoạch lại sau khi dùng v0.1 trong một project thật.

---

## 2. Chi tiết công việc

Mỗi task có **Done khi** (tiêu chí chấp nhận). Ước lượng tính bằng giờ tập trung.

### M0 — Kiểm chứng & khung (≈ 8h)

| ID    | Task                                                                                         | Ước lượng | Done khi |
|-------|----------------------------------------------------------------------------------------------|-----------|----------|
| M0.1  | Liệt kê ≥ 3 project/panel thật sẽ dùng lib (ghi vào `docs/use-cases.md`)                    | 1h        | Có tên project + danh sách property của ít nhất 1 panel thật |
| M0.2  | Time-box 1h: thử bản port Qt 6 của QtPropertyBrowser và QtnProperty với panel ở M0.1          | 2h        | Ghi kết luận "đủ/không đủ, vì sao" vào `docs/use-cases.md`. Nếu "đủ" → **dừng, cân nhắc wrap/fork** |
| M0.3  | Khung CMake: `qpb_core`, `qpb_widgets` (1 file rỗng mỗi lib), alias `qpb::*`, option `QPB_BUILD_TESTS/EXAMPLES`, cờ cảnh báo | 2h | `cmake --build` + `ctest` chạy trên Qt 6.8 |
| M0.4  | Test rỗng dùng Qt Test ở `tests/core`, `tests/widgets` (offscreen)                            | 1h        | `ctest` báo 2 test pass |
| M0.5  | `.clang-format`, `.gitignore`, `LICENSE` (MIT), cập nhật `README.md` (mục tiêu + link docs)   | 1h        | Có trong repo |
| M0.6  | (tùy chọn) GitHub Actions: Ubuntu + Qt 6.8, build + test                                      | 1h        | Workflow xanh |

### M1 — Core + Model (≈ 22h)

| ID    | Task                                                                                         | Ước lượng | Done khi |
|-------|----------------------------------------------------------------------------------------------|-----------|----------|
| M1.1  | `Property`: dữ liệu, cờ, trạng thái hiệu lực kế thừa (SPEC §4.2), `path()`                    | 3h        | Test: kế thừa readOnly/enabled/visible qua 3 tầng; path đúng |
| M1.2  | `PropertyGroup`: con có thứ tự, `add`/`remove`/`find`, id trùng (SPEC §4.3)                   | 2h        | Test: find theo path lồng, remove, id trùng không tạo nút mới |
| M1.3  | `PropertyBuilder<T>`, `EnumBuilder`, `PathBuilder`, hằng `Attr::*` (SPEC §4.3–4.4)            | 3h        | Ví dụ Phụ lục A biên dịch; `addInt(...).regex(...)` **không** biên dịch (test compile-fail tùy chọn) |
| M1.4  | `TypeRegistry` + `TypeHandler` + 7 kiểu cơ bản: displayText, normalize (clamp/decimals), validate (SPEC §4.5, §4.7) | 4h | Test cho mỗi kiểu: hiển thị, clamp, regex, enum ngoài options, mustExist |
| M1.5  | `PropertyModel` chỉ đọc: `index/parent/rowCount/columnCount/data/flags/headerData`, role tùy biến | 3h     | `QAbstractItemModelTester` (Fatal) pass trên cây 3 tầng |
| M1.6  | Pipeline ghi giá trị (SPEC §4.6, 6 bước) + `valueChanged`/`validationFailed`; `Property::setValue`, `PropertyModel::setValue` | 3h | `QSignalSpy`: đúng 1 signal khi đổi, 0 khi bằng giá trị cũ, 0 khi read-only, `validationFailed` khi lỗi |
| M1.7  | `TreeObserver`: insert/remove/metadata khi model sống; `setRoot`                             | 3h        | Tester pass khi thêm/xóa property lúc chạy; `dataChanged` khi `setReadOnly`/`setVisible` |
| M1.8  | Batch (`beginBatch/endBatch` lồng) + `resetToDefault` (đệ quy cho group)                      | 1h        | Test: `batchValueChanged` phát 1 lần ở batch ngoài cùng |

**Checkpoint cuối M1:** viết `examples/quickstart` (chưa có view, chỉ in `valueChanged`) đúng 30 dòng. Nếu API gây khó chịu → sửa builder **ngay bây giờ**, trước khi có code view phụ thuộc.

### M2 — Tree view + editor (≈ 24h)

Làm theo thứ tự rủi ro cao trước: Int + String + FilePath để kiểm chứng UX editor (brainstorm §9, prototype 1 ngày).

| ID    | Task                                                                                         | Ước lượng | Done khi |
|-------|----------------------------------------------------------------------------------------------|-----------|----------|
| M2.1  | `EditorFactory` + `EditorHandler`, tra `editorId` → `typeId`, `notifyCommit` (SPEC §5.1–5.2) | 2h        | Test headless: tạo editor theo typeId và theo editorId |
| M2.2  | `PropertyDelegate` cơ bản: create/set/commit qua factory; Int + String                        | 3h        | Test: edit Int → model đổi; Esc hủy; focus-out commit |
| M2.3  | `PropertyTreeView` Mode::Tree: 2 cột, group spanned, ẩn hàng theo `IsVisibleRole`, edit triggers | 3h     | Demo `examples/inspector` hiện cây Phụ lục A |
| M2.4  | `PathEdit` (file/dir) + chặn `FocusOut` khi dialog mở + hook test (SPEC §5.3–5.4, D6)          | 4h        | Test: mở "dialog" giả → editor không đóng; chọn xong → commit ngay. Kiểm tay trên 1 OS có dialog native |
| M2.5  | Double, Enum (commit khi chọn), Bool (checkbox qua `CheckStateRole`, click + Space)           | 3h        | Test cho từng kiểu trong tree |
| M2.6  | Điều hướng bàn phím: Enter, Esc, Tab/Shift+Tab sang ô editable kế tiếp (bỏ group/read-only)  | 4h        | Test `QTest::keyClick` qua 5 property có group xen giữa |
| M2.7  | Hiển thị lỗi validation (tooltip tại ô, D5); elide path giữa; paint group                    | 2h        | Nhập regex sai → giá trị cũ giữ nguyên + tooltip |
| M2.8  | Kiểm tay trọn bộ trên inspector, ghi lỗi UX vào issue                                         | 3h        | Danh sách lỗi UX, lỗi chặn đã sửa |

**Điểm dừng quyết định (cuối tuần 3):** nếu M2.4 hoặc M2.6 cần hack sâu hơn `eventFilter`/`QTreeView::moveCursor`
(vd. phải thay cả cơ chế edit của `QAbstractItemView`) → ghi lại, và cân nhắc để Form view (editor thường trực) làm view chính.

### M3 — Hoàn thiện v0.1 (≈ 14h)

| ID    | Task                                                                                         | Ước lượng | Done khi |
|-------|----------------------------------------------------------------------------------------------|-----------|----------|
| M3.1  | Mode::List (D4): không decoration, indent 0, expandAll giữ khi insert, group là section header | 2h       | Test: đổi Tree↔List giữ giá trị + selection; inspector có nút chuyển |
| M3.2  | Context menu Reset to default / Reset group                                                  | 1h        | Test: action disabled khi không modified |
| M3.3  | `examples/custom_type`: `ColorButton` + đăng ký `app.color` ≤ 100 dòng (S2)                   | 2h        | Đếm dòng ≤ 100; màu hiển thị ô swatch; edit được |
| M3.4  | Hoàn thiện `examples/quickstart` với view (S1 ≤ 30 dòng)                                      | 1h        | Đếm dòng ≤ 30 |
| M3.5  | `install(EXPORT)`, `qpbConfig.cmake`, version file; test consumer bằng `find_package` và `FetchContent` | 3h | Thư mục `tests/consumer` build được từ bản install |
| M3.6  | Kiểm tra luật kiến trúc: grep `QtWidgets` trong `src/core` = 0; không `switch` theo typeId trong view (R1, R2) | 1h | Script `tools/check_arch.sh` chạy trong CTest |
| M3.7  | Build Windows (MSVC) + macOS ít nhất một lần; sửa cảnh báo                                    | 2h        | Build sạch cảnh báo trên 3 OS |
| M3.8  | `CHANGELOG.md`, cập nhật README (quickstart), tag `v0.1.0`                                     | 2h        | Tag đã push |

### M4 — v0.2 (≈ 30h, lập kế hoạch lại sau v0.1)

| ID    | Task                                                                                         | Done khi |
|-------|----------------------------------------------------------------------------------------------|----------|
| M4.1  | `PropertyFormView`: dựng form từ model, editor thường trực, group collapsible (SPEC §5.6)    | Inspector hiện Form cùng model với Tree |
| M4.2  | Đồng bộ hai chiều form ↔ model (dataChanged, insert/remove/reset) không vòng lặp             | Test: sửa ở Tree → Form cập nhật và ngược lại, 2 view cùng mở |
| M4.3  | `PropertyFilterProxyModel` + ô search trong inspector (SPEC §5.7)                            | Lọc "x" hiện `Transform/x` kèm group cha |
| M4.4  | String `multiline` (`QPlainTextEdit`, Ctrl+Enter commit)                                     | Test commit/hủy |
| M4.5  | In đậm tên khi modified (`IsModifiedRole`), tooltip đầy đủ                                    | Test ảnh chụp/role |
| M4.6  | Tag `v0.2.0`                                                                                 | — |

### M5 — v0.3 (≈ 40h, lập kế hoạch lại sau v0.2)

| ID    | Task                                                                                         | Done khi |
|-------|----------------------------------------------------------------------------------------------|----------|
| M5.1  | `QObjectPropertySource`: đọc `Q_PROPERTY`, metadata qua `Q_CLASSINFO`, đồng bộ qua notify signal | Test với QObject mẫu: đổi từ UI → setter được gọi; đổi từ code → UI cập nhật |
| M5.2  | Serialize: `toJson/fromJson`, `save/load(QSettings&)` theo path                              | Round-trip test |
| M5.3  | `qint64` + spinbox 64-bit                                                                     | Test giá trị > INT_MAX |
| M5.4  | Tài liệu tích hợp `QUndoStack` (example dùng old/new trong `valueChanged`)                    | Example chạy được |

---

## 3. Phụ thuộc giữa các task

```text
M0.3 ─► M1.1 ─► M1.2 ─► M1.3 ──────────────► (checkpoint quickstart)
             └► M1.4 ─┐
                      ├► M1.5 ─► M1.6 ─► M1.7 ─► M1.8
M1.4 ─► M2.1 ─► M2.2 ─► M2.3 ─► M2.4, M2.5 ─► M2.6 ─► M2.7 ─► M2.8
M2.3 ─► M3.1, M3.2          M2.1 ─► M3.3          M0.3 ─► M3.5
```

M1.4 (registry) và M1.1–M1.3 có thể làm song song. M3.5 (CMake install) độc lập, làm khi cần "nghỉ" khỏi UI.

---

## 4. Quy trình làm việc

- **Nhánh:** mỗi task (hoặc nhóm task nhỏ) một nhánh `feat/M1.4-type-registry`, PR vào `main`, squash merge.
- **Definition of Done cho mọi PR:** build sạch cảnh báo; test mới cho hành vi mới; `ctest` pass (widgets chạy `offscreen`);
  không vi phạm R1–R4 (SPEC §3); public API mới có ít nhất một nơi dùng.
- **Commit:** Conventional Commits (`feat(core): ...`, `fix(widgets): ...`).
- **Review cuối mỗi milestone:** 30 phút đối chiếu với SPEC; cập nhật Phụ lục B (nhật ký quyết định) nếu có đổi thiết kế.

---

## 5. Rủi ro & phương án cắt phạm vi

| Rủi ro                                         | Dấu hiệu                                    | Phản ứng                                                                    |
|------------------------------------------------|---------------------------------------------|-----------------------------------------------------------------------------|
| Chậm tiến độ v0.1                               | Cuối tuần 3 chưa xong M2.6                   | Cắt theo thứ tự: M3.7 (đa nền tảng) → DirPath → Tab navigation (giữ Enter/Esc) → M3.5 (dùng `add_subdirectory` trước) |
| UX editor cần hack sâu                          | M2.4/M2.6 vượt 2× ước lượng                 | Điểm dừng cuối tuần 3 (xem M2)                                              |
| Builder API khó dùng                            | Quickstart > 30 dòng hoặc cần nhiều cast     | Sửa ở checkpoint M1, trước khi viết view                                    |
| `TreeObserver` gây lỗi index khi thêm/xóa        | `QAbstractItemModelTester` fail không rõ lý do | Tạm thời chỉ cho thay đổi cấu trúc qua `setRoot` (reset model) ở v0.1      |
| Khác biệt dialog native giữa OS (focus)          | Editor đóng khi dialog mở trên Windows/macOS | Fallback: đóng editor trước khi mở dialog, dialog trả về thì ghi thẳng qua `model->setData` |
| M0.2 cho thấy lib sẵn có "đủ tốt"               | —                                           | Dừng; viết wrapper mỏng hoặc fork thay vì tiếp tục kế hoạch này             |

---

## 6. Việc cần bạn xác nhận trước khi bắt đầu M1

Các mục sau đang theo giả định trong SPEC; đổi mục nào sẽ ảnh hưởng tới task liệt kê bên cạnh.

| Câu hỏi (brainstorm §13)                  | Giả định hiện tại            | Ảnh hưởng nếu đổi                      |
|-------------------------------------------|------------------------------|----------------------------------------|
| C++17 hay C++20?                          | C++17                        | M1.4, M2.1 (có thể dùng designated initializer) |
| Qt tối thiểu?                             | 6.5 (phát triển trên 6.8)     | M0.3, M3.7                             |
| License / public?                         | MIT, public sau v1.0          | M0.5                                   |
| "Custom property table" nghĩa là gì?      | (a) tập property runtime      | Nếu (b) thêm cột: M1.5 thêm API cột tùy biến |
| Nguồn dữ liệu chính?                      | Builder tường minh            | Nếu là `QObject`: kéo M5.1 lên ngay sau M3 |
| Thời gian cho MVP?                        | 4 tuần × ~15h                 | Toàn bộ lịch                            |
