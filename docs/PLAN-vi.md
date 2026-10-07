# qpb — Kế hoạch triển khai

> **Bản dịch tiếng Việt để tham khảo.** Bản chính thức là [`PLAN.md`](PLAN.md) (tiếng Anh).

> Đặc tả: [`SPEC-vi.md`](SPEC-vi.md). Ý tưởng gốc: [`brainstorm-vi.md`](brainstorm-vi.md).
> Giả định nguồn lực: **bán thời gian (~15 giờ/tuần)**, một người.

## 0. Định hướng (bản điều chỉnh)

Ba mục tiêu chi phối toàn bộ kế hoạch (SPEC §1.1):

1. **API cố định (G5):** project đã dùng qpb nâng cấp trong cùng major version **không phải sửa code**.
   → Thiết kế API *trước* khi cài đặt (API-first), bản đầu tiên cho project thật là **1.0** và khóa API ngay từ đó.
   0.x chỉ dùng nội bộ trong repo.
2. **Phân phối dạng component folder (G6):** chép `qpb/` vào `components/qpb/` của project, thêm 2 dòng CMake.
   Cập nhật = thay folder + build lại. → Cấu trúc repo lấy folder `qpb/` làm trung tâm từ ngày đầu,
   tests và examples dùng nó y như một consumer.
3. **Viết mới hoàn toàn (G7):** không wrap/fork, không chép code từ QtPropertyBrowser/QtnProperty.
   (Bước "thử lib có sẵn rồi quyết định wrap" của bản kế hoạch trước đã bị bỏ.)

Hệ quả so với bản trước: thêm ~18 giờ cho thiết kế API, kiểm thử tương thích và đóng gói;
có thêm giai đoạn **dùng thử (RC)** trong một project thật trước khi tag 1.0.

---

## 1. Tổng quan milestone

| Milestone | Tuần (ước lượng) | Kết quả kiểm chứng được                                                                 |
|-----------|------------------|------------------------------------------------------------------------------------------|
| **M0** Khung component           | 1        | Folder `qpb/` build được như một component; `tests/consumer` dùng nó bằng `add_subdirectory` |
| **M1** Thiết kế API 1.0          | 1–2      | Toàn bộ header public 1.0 được viết và review; code mẫu biên dịch được trên header       |
| **M2** Core + Model              | 2–3      | `qpb::core` cài đặt đủ, pass test kể cả `QAbstractItemModelTester`                       |
| **M3** Tree view + editor        | 4–5      | Inspector sửa được 7 kiểu, bàn phím đầy đủ → tag nội bộ `0.1.0`                          |
| **M4** Hoàn thiện & khóa API     | 6        | List mode, reset, test tương thích, gói release → `1.0.0-rc1`                            |
| **RC** Dùng thử                  | 7–8      | rc1 chạy các kịch bản tham chiếu trong ứng dụng độc lập mà không đổi API → tag `1.0.0`   |
| **M5** 1.1                       | sau 1.0  | Form view + filter (chỉ bổ sung)                                                          |
| **M6** 1.2                       | sau 1.1  | QObject adapter, serialize, Int64 (chỉ bổ sung)                                          |

M0–M4 ≈ 86 giờ (≈ 6 tuần bán thời gian). M5–M6 sẽ lập kế hoạch lại sau khi 1.0 được dùng thật.

---

## 2. Chi tiết công việc

Mỗi task có ước lượng (giờ tập trung) và tiêu chí **Done khi**.

### M0 — Khung component (≈ 8h)

**Trạng thái:** xong. Không có project thật nên M0.1 dùng các kịch bản tham chiếu trong `docs/use-cases.md`.
Option `QPB_INSTALL` (tùy chọn, SPEC §6.3) chưa thêm vì chưa có install rule nào để bật (R4).

| ID    | Task                                                                                                  | Giờ | Done khi |
|-------|-------------------------------------------------------------------------------------------------------|-----|----------|
| M0.1  | Ghi `docs/use-cases.md`: 3 kịch bản tham chiếu (không có project thật) kèm danh sách property đầy đủ | 1   | Có file; dùng làm dữ liệu cho M1 |
| M0.2  | Tạo cấu trúc repo theo SPEC §6.1: `qpb/{CMakeLists.txt,VERSION,LICENSE,CHANGELOG.md,include/qpb,src}`, CMake gốc cho dev | 2 | `cmake -S . -B build && cmake --build build` chạy |
| M0.3  | `qpb/CMakeLists.txt` tuân **toàn bộ** SPEC §6.3 (không biến global, static mặc định, `find_package` có điều kiện, không `.qrc`) | 2 | Review checklist §6.3 từng dòng |
| M0.4  | `qpbglobal.h` sinh từ `VERSION`: `QPB_VERSION*`, `QPB_VERSION_CHECK`, export macro, `QPB_DEPRECATED_X`, `QPB_DISABLE_DEPRECATED` | 1 | Test in ra `qpb::version()` |
| M0.5  | `tests/consumer/`: project CMake độc lập chép `qpb/` vào `components/qpb/` (bước copy trong CTest) rồi build app tối thiểu | 1 | CTest pass; build cả C++17 và C++20 |
| M0.6  | `.clang-format`, `.gitignore`, cập nhật README; (tùy chọn) GitHub Actions Ubuntu + Qt 6.8                | 1   | Có trong repo |

### M1 — Thiết kế API 1.0 (≈ 10h) — *API-first*

**Trạng thái:** xong. Header ở `qpb/include/qpb/`, example chỉ biên dịch ở `examples/`, phác thảo tương lai và kiểm tra
header tự đủ ở `tests/api/`, review ở [`api-review.md`](api-review.md). Example sẽ link được khi M2/M3 cài đặt xong API.

Mục tiêu: chốt hình dạng API **trước khi** có code cài đặt để phụ thuộc vào, vì sau 1.0 không còn sửa được.

| ID    | Task                                                                                                  | Giờ | Done khi |
|-------|-------------------------------------------------------------------------------------------------------|-----|----------|
| M1.1  | Viết toàn bộ header public 1.0 (chỉ khai báo + comment tài liệu): `Property`, `PropertyGroup`, builders, `Attr`, `Types`, `TypeRegistry`, `ValidationResult`, `PropertyModel`, `EditorFactory`, `PropertyDelegate`, `PropertyTreeView`, `qpb.h` | 4 | Mọi header tuân SPEC §9.3 (d-pointer, không data member public, không logic inline) |
| M1.2  | Viết trên header (chỉ biên dịch, chưa link): `examples/quickstart` (≤ 30 dòng), `examples/custom_type` (QColor), và các kịch bản tham chiếu từ M0.1 | 2 | Biên dịch được thành object file; tự đánh giá độ tiện tay |
| M1.3  | **Kiểm tra chừa chỗ cho tương lai:** phác thảo (không cài đặt) header của `PropertyFormView`, `PropertyFilterProxyModel`, `QObjectPropertySource`, serialize, `Int64` chỉ dùng API public 1.0 | 2 | Không cần thêm/sửa gì ở API 1.0; nếu cần → sửa API **bây giờ** |
| M1.4  | Review API theo checklist SPEC §9.2–9.3; ghi quyết định mới vào SPEC Phụ lục B                          | 1   | Checklist ký tên; SPEC cập nhật |
| M1.5  | Test "header tự đủ" (mỗi header public include riêng lẻ) đưa vào CTest                               | 1   | CTest pass |

Từ sau M1, thay đổi header public phải ghi lý do vào Phụ lục B (vẫn cho phép đến trước 1.0, nhưng phải có lý do).

### M2 — Core + Model (≈ 22h)

**Trạng thái:** xong. `qpb::core` đã cài đặt (`qpb/src/core/`) với 61 hàm test trong `tests/core/`
(`tst_property`, `tst_typeregistry`, `tst_propertymodel`; test model chạy dưới `QAbstractItemModelTester` chế độ Fatal).
Tiêu chí "quickstart link được" của M2.3 cần module widgets (M3); phần core được chứng minh qua `tests/consumer`, giờ dựng
model và chạy pipeline giá trị từ bản `qpb/` link static (cũng là kiểm tra link static của M2.4).

| ID    | Task                                                                                         | Giờ | Done khi |
|-------|----------------------------------------------------------------------------------------------|-----|----------|
| M2.1  | `Property` + d-pointer: dữ liệu, cờ, trạng thái hiệu lực kế thừa, `path()` (SPEC §4.2)        | 3   | Test kế thừa readOnly/enabled/visible qua 3 tầng; path đúng |
| M2.2  | `PropertyGroup`: `add`/`remove`/`find`, id trùng (SPEC §4.3)                                  | 2   | Test find lồng, remove, id trùng không tạo nút mới |
| M2.3  | Builders + `Attr::*` (SPEC §4.3–4.4)                                                          | 3   | quickstart ở M1.2 giờ link và chạy |
| M2.4  | `TypeRegistry` + 7 kiểu cơ bản: displayText, normalize, validate; đăng ký lười (SPEC §4.5, §4.7, §6.3) | 4 | Test từng kiểu; test link static không mất kiểu cơ bản |
| M2.5  | `PropertyModel` đọc: index/parent/data/flags/header, role (kể cả dải `UserRole`)             | 3   | `QAbstractItemModelTester` (Fatal) pass trên cây 3 tầng |
| M2.6  | Pipeline ghi giá trị 6 bước + `valueChanged`/`validationFailed` (SPEC §4.6)                   | 3   | `QSignalSpy`: 1 signal khi đổi, 0 khi bằng cũ/read-only; lỗi → `validationFailed` |
| M2.7  | `TreeObserver`: thêm/xóa/metadata khi model sống; `setRoot`                                   | 3   | Tester pass khi thêm/xóa lúc chạy |
| M2.8  | Batch lồng + `resetToDefault` đệ quy                                                          | 1   | `batchValueChanged` phát 1 lần ở batch ngoài cùng |

### M3 — Tree view + editor (≈ 24h)

**Trạng thái:** xong, trừ việc gắn tag nội bộ `0.1.0` (M3.8) để bạn quyết định. `qpb::widgets` đã cài đặt
(`EditorFactory` với 7 editor dựng sẵn, `PathEdit` nội bộ, `PropertyDelegate`, `PropertyTreeView`); cả 5 example đã link và chạy.
`tests/widgets` kiểm tra factory (13 hàm) và tương tác view/delegate (19 hàm: Enter/Esc/focus-out, Tab/Shift+Tab bỏ qua hàng
không sửa được, commit enum và path, bảo vệ focus khi mở dialog, checkbox, tooltip lỗi, các mode, hàng ẩn, menu reset, proxy model).
Kiểm tra thủ công bằng ảnh chụp offscreen (`QPB_SCREENSHOT_DIR`). Điểm dừng quyết định sau M3.6 không xảy ra: chỉ cần một event
filter và override `moveCursor()`.

Làm phần rủi ro cao trước: Int + String + FilePath (UX editor, focus khi mở dialog).

| ID    | Task                                                                                         | Giờ | Done khi |
|-------|----------------------------------------------------------------------------------------------|-----|----------|
| M3.1  | `EditorFactory` + tra `editorId` → `typeId` + `notifyCommit` (SPEC §5.1–5.2)                 | 2   | Test headless tạo editor theo typeId/editorId |
| M3.2  | `PropertyDelegate`: create/set/commit qua factory; Int + String                               | 3   | Edit Int → model đổi; Esc hủy; focus-out commit |
| M3.3  | `PropertyTreeView` Mode::Tree: 2 cột, group spanned, ẩn theo `IsVisibleRole`                  | 3   | `examples/inspector` hiện cây Phụ lục A |
| M3.4  | `PathEdit` file/dir + chặn FocusOut khi dialog mở + hook test (SPEC §5.3–5.4, D6)             | 4   | Dialog giả không đóng editor; chọn xong commit ngay; thử tay dialog native |
| M3.5  | Double, Enum (commit khi chọn), Bool (`CheckStateRole`, click + Space)                        | 3   | Test từng kiểu |
| M3.6  | Bàn phím: Enter, Esc, Tab/Shift+Tab bỏ qua group/read-only                                    | 4   | `QTest::keyClick` qua 5 property có group xen giữa |
| M3.7  | Lỗi validation (tooltip tại ô), elide path giữa, paint group, in đậm khi modified             | 2   | Regex sai → giữ giá trị cũ + tooltip |
| M3.8  | Kiểm tay toàn bộ inspector; sửa lỗi chặn; tag nội bộ `0.1.0`                                  | 3   | Danh sách lỗi UX; lỗi chặn đã sửa |

**Điểm dừng quyết định (cuối M3.6):** nếu Tab/FocusOut cần thay cả cơ chế edit của `QAbstractItemView`
→ cân nhắc đưa Form view (editor thường trực) lên làm view chính của 1.0. Quyết định này **phải có trước M4**
vì sau 1.0 không đổi được.

### M4 — Hoàn thiện & khóa API (≈ 22h)

**Trạng thái:** mọi việc không cần publish đã xong; bản `1.0.0-rc1` (M4.9: tag, GitHub Release, đẩy nhánh `qpb-release`)
chờ bạn quyết định. M4.1/M4.2 đã làm cùng M3. Mới trong M4: `tests/api_compat/v1_0.cpp` (M4.4), các kịch bản consumer cho
build shared, rò rỉ thiết lập của project chủ và nâng cấp tại chỗ (M4.5), `tools/check_architecture.cmake` với luật mới R6 và
`tools/count_lines.cmake` (M4.3, M4.6), CI cho Windows (MSVC) và macOS (M4.7), `tools/api_snapshot.py` với baseline
`tests/api_compat/api-1.0.txt` (M4.8), `tools/make_release.sh` và [`RELEASING.md`](RELEASING.md) (M4.9), README hướng dẫn
tích hợp/nâng cấp và review cuối trong [`api-review.md`](api-review.md) (M4.10). Các kiểm tra đã phát hiện hai lỗi thật trong
CMake của component (D31, D32).

| ID    | Task                                                                                         | Giờ | Done khi |
|-------|----------------------------------------------------------------------------------------------|-----|----------|
| M4.1  | Mode::List (D4) + nút chuyển trong inspector                                                  | 2   | Đổi Tree↔List giữ giá trị và selection |
| M4.2  | Context menu Reset to default / Reset group                                                   | 1   | Action disabled khi không modified |
| M4.3  | Hoàn thiện examples; đo S1 (≤ 30 dòng) và S2 (≤ 100 dòng) bằng script                         | 2   | Script trong CTest |
| M4.4  | `tests/api_compat/v1_0.cpp`: dùng **toàn bộ** API public 1.0 (build + chạy)                   | 3   | CTest pass; file được đóng băng từ đây |
| M4.5  | Mở rộng `tests/consumer`: static/shared, C++17/C++20, kiểm tra không rò rỉ biến global (so sánh `CMAKE_*` trước/sau `add_subdirectory`), mô phỏng "thay folder rồi build lại" | 3 | CTest pass |
| M4.6  | `tools/check_arch.sh`: core không include QtWidgets/QtGui; view không rẽ nhánh theo typeId; `src/` không bị include từ header public (R1, R2, R5) | 1 | Chạy trong CTest |
| M4.7  | Build Windows (MSVC) + macOS; header public sạch cảnh báo dưới `-Wall -Wextra -Wpedantic` / `/W4` | 3 | Sạch trên 3 OS |
| M4.8  | Tạo snapshot API gốc (`tools/api_snapshot`) để so sánh từ 1.1                                  | 2   | File snapshot trong repo |
| M4.9  | Quy trình phát hành (mục 4): `CHANGELOG.md`, zip `qpb-1.0.0-rc1.zip` chỉ chứa `qpb/`, nhánh `qpb-release` bằng `git subtree split` | 3 | Release rc1 trên GitHub |
| M4.10 | Review API lần cuối với SPEC §9; README hướng dẫn tích hợp component + chính sách nâng cấp       | 2   | Review xong |

### RC — Dùng thử trước khi khóa (1–2 tuần, song song việc khác)

**Trạng thái:** vòng 1 (`1.0.0-rc1`) phát hiện hai vấn đề hành vi, đã sửa trong `1.0.0-rc2`. Vòng 2 và 3 (`1.0.0-rc2`,
vòng 3 phủ rộng API hơn và build static/shared/Clang) không cần thay đổi API hay hành vi nào, nên RC.2 đã xong. RC.3: `1.0.0` có nội dung
của `1.0.0-rc2`, chỉ đổi `VERSION` và `CHANGELOG.md`; xem [`rc-trial.md`](rc-trial.md).
Ứng dụng thử nghiệm nằm trong `rc-trial/` và được chạy lại với mỗi release candidate.

| ID    | Task                                                                                         | Done khi |
|-------|----------------------------------------------------------------------------------------------|----------|
| RC.1  | Dựng 3 kịch bản tham chiếu (M0.1) thành một ứng dụng độc lập ngoài repo, nhúng `1.0.0-rc1` qua `components/qpb/`; thêm project thật nếu lúc đó đã có | Mọi kịch bản chạy đúng |
| RC.2  | Ghi mọi chỗ "khó chịu" với API. Nếu phải đổi API → sửa, phát `rc2`, lặp lại RC                  | Một vòng RC không cần đổi API |
| RC.3  | Tag `1.0.0` (cùng nội dung RC cuối, chỉ đổi `VERSION`)                                          | Tag + release zip |

### M5 — 1.1 (chỉ bổ sung, lập kế hoạch lại sau 1.0)

M5.1 `PropertyFormView` (SPEC §5.6) · M5.2 đồng bộ hai chiều form ↔ model · M5.3 `PropertyFilterProxyModel` + ô search ·
M5.4 attribute `multiline` · M5.5 `tests/api_compat/v1_1.cpp` + diff API snapshot (chỉ được thêm) · M5.6 release 1.1.0.

**Trạng thái:** M5.1–M5.5 xong. `PropertyFormView` (`qpb/src/widgets/PropertyFormView.cpp`, 17 test trong
`tests/widgets/tst_propertyformview.cpp`), `PropertyFilterProxyModel` trong `qpb::core` (7 test), attribute `multiline`
(tree view và form), `tests/api_compat/v1_1.cpp` và snapshot `api-1.1.txt` (chứa trọn `api-1.0.txt`, file này vẫn được
kiểm tra). Ô search là `QLineEdit` của ứng dụng nối với `setFilterFixedString()`, minh họa trong `examples/form_view`.
Quyết định D36–D39 trong SPEC. M5.6: đã phát hành `1.1.0`.

### M6 — 1.2 (chỉ bổ sung)

M6.1 `QObjectPropertySource` · M6.2 serialize JSON/`QSettings` · M6.3 `Types::Int64` · M6.4 example `QUndoStack` ·
M6.5 `v1_2.cpp` + diff snapshot · M6.6 release 1.2.0.

**Trạng thái:** M6.1–M6.5 xong. `QObjectPropertySource` (SPEC §4.9, 8 test), `qpb::serialization` (§4.8, 10 test gồm
`QSettings` INI và native), `Types::Int64` với spin box 64-bit nội bộ, `examples/object_editor` (QObject source +
`QUndoStack` + JSON), `tests/api_compat/v1_2.cpp` và `api-1.2.txt` (chứa trọn 1.1). Bước kiểm tra biên dịch phát hiện hàm
tự do trong `qpb` làm hỏng lời gọi không kèm namespace của ứng dụng qua ADL, nên dùng namespace lồng (D40). Quyết định
D40–D42. M6.6: đã phát hành `1.2.0`, sau vòng thử RC 4 (docs/rc-trial.md) phát hiện và sửa F7.

### M7 — 1.3 (chỉ bổ sung): cải thiện `QObjectPropertySource` (F8, F9)

Lập sau vòng thử RC 4 ([`rc-trial.md`](rc-trial.md)). Cả hai điểm đều giải quyết bằng cách bổ sung; code viết cho 1.0–1.2
build và chạy như cũ.

**F8 — tiêu đề group.** Group tạo từ một `QObject` có tiêu đề là id (tên object, vd. `studio_mic`). Id giữ nguyên (path
phải ổn định cho serialization và code ứng dụng); chỉ **tên hiển thị** của group thay đổi:

- `Q_CLASSINFO("qpb:title", "name")`: tên hiển thị của group là giá trị của Q_PROPERTY đó, cập nhật qua signal NOTIFY.
  Class tự quyết định.
- `QObjectPropertySource::setTitleProperty(const QString& name)`: tương tự cho object mà class không có class info này,
  dành cho class ứng dụng không sửa được (vd. từ thư viện khác). Áp cho object thêm sau đó; class info được ưu tiên.
- Không đặt gì: như cũ (id), giống 1.2.

**F9 — giá trị do ứng dụng quản lý.** Có những property không phải thiết lập mà là giá trị trực tiếp (bộ đếm, trạng
thái, dung lượng đã dùng). Chúng không bao giờ nên trông như "đã sửa", bị reset hay được lưu.

- Cờ mới `Property::Flag::Live` (thêm ở cuối, `0x8`), với `isLive()` / `setLive()` và `PropertyBuilderBase::live()`.
  Property live: `isModified()` luôn `false` (view không bao giờ in đậm, `IsModifiedRole` là `false`); "Reset to
  default" và `resetToDefault()` bỏ qua nó; `qpb::serialization` không ghi, không đọc. Vẫn sửa được trừ khi đồng thời
  read-only.
- `QObjectPropertySource`: khóa metadata `live` (`Q_CLASSINFO("qpb:used", "live")`).
- Quyết định còn mở (D44): Q_PROPERTY **không có WRITE nhưng có NOTIFY** có tự động thành live không? Đó là điều vòng thử
  RC muốn, nhưng đổi hành vi của 1.2 (hiện các property này in đậm sau khi thay đổi). Đề xuất: mặc định giữ hành vi 1.2
  và thêm `QObjectPropertySource::setLiveReadOnlyProperties(bool)` (mặc định `false`), ứng dụng bật bằng một lời gọi.
  Phương án khác: coi là sửa lỗi (SPEC §9.1 cho phép sửa hành vi mâu thuẫn tài liệu) và ghi rõ trong CHANGELOG.

| ID   | Việc                                                                                              | Ước tính (h) | Xong khi |
|------|---------------------------------------------------------------------------------------------------|--------------|----------|
| M7.1 | SPEC: §4.2 (cờ Live), §4.8 (serialization bỏ qua live), §4.9 (title, `live`), D43 (tiêu đề), D44 (Live); `-vi` | 1 | Ghi xong quyết định |
| M7.2 | `Flag::Live`, `isLive()`/`setLive()`, builder `live()`; `isModified()`, reset và serialization tuân theo | 2 | Test core: modified, reset (đơn và group), JSON và QSettings bỏ qua |
| M7.3 | View: dự kiến không đổi code (dùng `IsModifiedRole` và hàm reset dùng chung); test property live không bao giờ in đậm và menu reset bỏ qua, ở cả tree và form | 1 | Test widget đạt |
| M7.4 | `QObjectPropertySource`: `qpb:title`, `setTitleProperty()`, khóa `live`, `setLiveReadOnlyProperties()` (nếu D44 được chấp nhận); tiêu đề theo NOTIFY | 3 | Test source: tiêu đề đặt/cập nhật/xóa theo object, metadata live |
| M7.5 | `tests/api_compat/v1_3.cpp` + `api-1.3.txt` (chứa trọn 1.2); `examples/object_editor` có tiêu đề và giá trị live | 1 | Test tương thích và snapshot đạt |
| M7.6 | Vòng thử RC 5: trang Devices dùng `qpb:title` và `live`; đóng F8, F9, không có thay đổi hành vi mới | 1 | Test trial đạt, cập nhật báo cáo |
| M7.7 | Phát hành 1.3.0 (PR, CI xanh, zip, `qpb-release`)                                                 | 0.5 | Có zip phát hành |

Tổng ≈ 9,5 h.

**Trạng thái:** M7.1–M7.6 xong theo đề xuất D44 (bật bằng `setLiveReadOnlyProperties()`). `Flag::Live` (core, 4 hàm test
ở property, model, serialization), tiêu đề và giá trị live trong `QObjectPropertySource` (3 test), tree view và form view
không phải sửa (2 test), `tests/api_compat/v1_3.cpp` và `api-1.3.txt` (chứa trọn 1.2), `examples/object_editor` có tiêu
đề, bộ đếm live và undo bỏ qua nó, SPEC §4.2, §4.8, §4.9, D43, D44. Vòng thử RC 5 đóng F8 và F9. M7.7: đã phát hành
`1.3.0`.

### M8 — 1.4 (chỉ bổ sung): phản ứng theo giá trị (F3, F10)

Lập sau vòng thử RC 1 và 4 ([`rc-trial.md`](rc-trial.md)). Ở mọi kịch bản, ứng dụng lắng nghe
`PropertyModel::valueChanged` và so sánh chuỗi `path`: một lần để chép giá trị đã sửa về dữ liệu của mình (F3, chuỗi
`if (path == ...)`), một lần để bật hoặc hiện một property tùy theo property khác (F10). 1.4 thêm cách làm trực tiếp cho
cả hai; code viết cho 1.0–1.3 build và chạy như cũ.

**F3 — callback cho từng property.** Đặt trên model, theo path, giống cách kết nối của Qt:

```cpp
QMetaObject::Connection PropertyModel::onValueChanged(const QString& path, const QObject* context,
    std::function<void(const QVariant& value)> handler);

model.onValueChanged("Transform/x", this, [this](const QVariant& v) { m_object.x = v.toDouble(); });
```

- Được gọi sau `valueChanged` của đúng path đó, cho cả người dùng sửa lẫn ứng dụng ghi. `context` bị hủy thì kết nối tự
  kết thúc; kết nối trả về có thể ngắt.
- Theo path, không theo `Property*`, nên vẫn còn sau `setRoot()` và khi property bị xóa rồi thêm lại (inspector thay cây
  mỗi khi chọn object khác).
- Path của một group cũng báo thay đổi của con cháu, qua overload thứ hai nhận
  `std::function<void(const QString& path, const QVariant& value)>`.
- Quyết định còn mở **D45:** đặt trên model (đề xuất; model đã quản lý thông báo thay đổi và path) hay trên `Property`
  (chạy được không cần model, nhưng `Property` không phải QObject nên cần một kiểu handle riêng để quản lý vòng đời và
  ngắt kết nối). Đề xuất: model.

**F10 — điều kiện giữa các property.** Khai báo một lần, model tự đánh giá:

```cpp
general.addInt("autosaveMinutes", 5).enabledWhen("General/autosave");          // bool true / giá trị khác rỗng
camera.addDouble("orthoScale", 1.0).visibleWhen("Camera/projection", 1);        // bằng một giá trị
limits.addInt64("quota", 0).enabledWhen("Limits/mode", [](const QVariant& v) { return v != "unlimited"; });
```

- `Property::setEnabledWhen()` / `setVisibleWhen()` (+ hàm builder), mỗi hàm có ba dạng: nguồn có giá trị "đúng", nguồn
  bằng một giá trị, hoặc một predicate trên giá trị nguồn. Mỗi loại một điều kiện cho mỗi property; `clear...()` để bỏ.
- Điều kiện được **kết hợp** với cờ riêng của property (`isEnabled()` = cờ riêng và điều kiện và tổ tiên), nên không bao
  giờ ghi đè cái ứng dụng đặt bằng `setEnabled()` / `setVisible()`. View không phải sửa: chúng đã theo trạng thái hiệu lực.
- Model đánh giá điều kiện khi giá trị nguồn đổi, khi cây đổi (`setRoot()`, thêm/xóa hàng) và khi đặt điều kiện.
  Property chưa nằm trong model, hoặc path nguồn không tồn tại, coi điều kiện là "đúng" (gõ nhầm không làm ẩn hay
  disable gì; ghi cảnh báo một lần).
- `QObjectPropertySource`: khóa metadata `enabledWhen=<id>` và `visibleWhen=<id>`, tương đối với group của object.
- Quyết định còn mở **D46:** kết hợp với cờ riêng (đề xuất) hay để model ghi cờ `Disabled` / `Hidden` (đơn giản hơn,
  nhưng ghi đè `setEnabled(false)` của ứng dụng).
- Ngoài phạm vi: điều kiện trên nhiều nguồn, giá trị tính toán, điều kiện read-only. Dạng predicate đã phủ phần lớn
  trường hợp (predicate có thể đọc property khác qua model mà nó capture); API luật tổng quát có thể thêm sau.

| ID   | Việc                                                                                              | Ước tính (h) | Xong khi |
|------|---------------------------------------------------------------------------------------------------|--------------|----------|
| M8.1 | SPEC: §4.2 (điều kiện), §4.6 (`onValueChanged`), §4.9 (khóa metadata), D45, D46; `-vi`             | 1 | Ghi xong quyết định |
| M8.2 | `PropertyModel::onValueChanged()` (path chính xác, path group, vòng đời context, ngắt, `setRoot()`) | 2 | Test model đạt |
| M8.3 | Điều kiện trong core: lưu trong `PropertyPrivate`, `isEnabled()` / `isVisible()` hiệu lực, model đánh giá, thông báo thay đổi, nguồn không tồn tại | 4 | Test core: cả ba dạng, kết hợp cờ, group lồng, đổi cấu trúc, batch |
| M8.4 | Builder, metadata của `QObjectPropertySource`; view: test tree và form theo điều kiện (dự kiến không sửa code view) | 2 | Test widget và source đạt |
| M8.5 | `tests/api_compat/v1_4.cpp` + `api-1.4.txt` (chứa trọn 1.3); example `settings_dialog` và `inspector` bỏ chuỗi if trên `valueChanged` | 1.5 | Test tương thích, snapshot và build example đạt |
| M8.6 | Vòng thử RC 6: ứng dụng thử thay code `valueChanged` bằng `onValueChanged()` và `enabledWhen()`; đóng F3 và F10 | 1.5 | Test trial đạt, cập nhật báo cáo |
| M8.7 | Phát hành 1.4.0 (PR, CI xanh, zip, `qpb-release`)                                                 | 0.5 | Có zip phát hành |

Tổng ≈ 12,5 h.

**Trạng thái:** M8.1–M8.6 xong theo D45 (đặt trên model) và D46 (kết hợp với cờ riêng). `onValueChanged()` (2 hàm test),
điều kiện (`tests/core/tst_conditions.cpp`, 13 hàm test), metadata của `QObjectPropertySource`, tree view và form view không
phải sửa (2 test), `tests/api_compat/v1_4.cpp` và `api-1.4.txt` (chứa trọn 1.3); example `settings_dialog` (không còn code
`valueChanged`) và `inspector` (`visibleWhen`, `onValueChanged`); SPEC §4.2, §4.6, §4.9, D45, D46. Phát hiện trong lúc làm:
số `0` viết trực tiếp bị mơ hồ với overload predicate (đã thêm overload `int`), và bỏ một điều kiện đang sai không báo
view (đã sửa). Vòng thử RC 6 đóng F3 và F10. M8.7: đã phát hành `1.4.0`.

### M9 — 1.5 (chỉ bổ sung): reset toàn bộ và checkbox trong chuỗi Tab (F4, F5)

Lên kế hoạch sau vòng thử RC 1 và 3 ([`rc-trial.md`](rc-trial.md)): hai phát hiện được giữ lại ở 1.0 vì có thể giải
quyết sau mà không phá vỡ gì. Code viết cho 1.0–1.4 build và chạy như cũ. 1.5 cũng phát hành bản sửa đã ghi ở mục
*Unreleased* của CHANGELOG (cảnh báo MSVC C4458 khi build `qpb/`).

**F4 — reset cả cây từ model.** `PropertyModel::resetToDefault()` nhận một index mà gốc cây thì không có, nên ứng dụng
phải viết `model.root()->resetToDefault()`. 1.5 thêm lời gọi mà người dùng sẽ tìm trên model:

```cpp
bool PropertyModel::resetAllToDefault();

connect(resetButton, &QPushButton::clicked, &model, &PropertyModel::resetAllToDefault);
```

- Tác dụng giống `root()->resetToDefault()`: là code ứng dụng nên property read-only và disabled cũng được reset (D34),
  property live được giữ nguyên (D44); chỉ một `batchValueChanged` cho cả cây. Trả về false nếu có reset bị từ chối;
  true với model không có root (không có gì để reset).
- `resetToDefault(QModelIndex())` vẫn trả về false: cho nó reset tất cả sẽ đổi hành vi đã ghi trong tài liệu (§9.2).

**F5 — checkbox trong chuỗi Tab.** Trong `PropertyTreeView`, Tab / Shift+Tab nối các editor và bỏ qua property Bool
(SPEC §5.5), vốn là checkbox không có editor; người dùng bàn phím phải tới chúng bằng phím mũi tên.

```cpp
view.setTabStopsOnCheckBoxes(true);   // cũng là một Q_PROPERTY
```

- Mặc định tắt: chuỗi Tab của 1.4 không đổi.
- Khi bật: Tab / Shift+Tab dừng cả ở ô giá trị của property Bool mà người dùng đổi được (enabled, không read-only, đang
  hiện). Ở đó không mở editor; Space bật tắt (như hiện nay), và Tab / Shift+Tab tiếp theo đi tiếp theo chuỗi, ra khỏi
  view ở hai đầu như trước.
- Chỉ tree view: trong `PropertyFormView` checkbox là widget và vốn đã nằm trong chuỗi Tab.
- Quyết định còn mở **D49:** bật theo lựa chọn (đề xuất) hay thành mặc định. Đổi mặc định sẽ đổi trải nghiệm bàn phím
  của mọi ứng dụng sau khi nâng cấp, điều §9.2 cấm; bật theo lựa chọn giống D44.
- Quyết định **D48** ghi lại lựa chọn cho F4: thêm hàm mới thay vì gán ý nghĩa cho `resetToDefault(QModelIndex())`.

| ID   | Việc                                                                                              | Ước tính (h) | Xong khi |
|------|---------------------------------------------------------------------------------------------------|----------|-----------|
| M9.1 | SPEC: §4.6 (`resetAllToDefault`), §5.5 (`tabStopsOnCheckBoxes`), §8 (1.5), D48, D49; `-vi`           | 1 | Đã ghi quyết định |
| M9.2 | `PropertyModel::resetAllToDefault()`                                                              | 1 | Test model: group lồng nhau, reset read-only, bỏ qua live, một `batchValueChanged`, reset bị từ chối → false, không có root → true |
| M9.3 | `PropertyTreeView::setTabStopsOnCheckBoxes()`: `moveCursor()` và Tab / Shift+Tab từ hàng checkbox  | 2.5 | Test widget: tắt thì giữ chuỗi 1.4; bật thì dừng ở checkbox cả hai chiều, Space bật tắt, Tab tiếp theo mở editor kế tiếp, bỏ qua checkbox disabled / read-only / ẩn, qua proxy, ở cuối view |
| M9.4 | `tests/api_compat/v1_5.cpp` + `api-1.5.txt` (chứa trọn 1.4); `examples/settings_dialog` dùng cả hai bổ sung | 1 | Test tương thích, snapshot và build example đạt |
| M9.5 | Vòng thử RC 7: trang settings dùng `resetAllToDefault()` và `setTabStopsOnCheckBoxes(true)`; đóng F4 và F5 | 1.5 | Test trial đạt, cập nhật báo cáo |
| M9.6 | Phát hành 1.5.0 (PR, zip, tag `v1.5.0` và `qpb-v1.5.0`, `qpb-release`, GitHub Release)              | 0.5 | Có zip phát hành |

Tổng ≈ 7.5 h. GitHub Actions đang tắt (tài khoản không khởi động được job), nên "CI xanh" nghĩa là chạy đủ `ctest` ở
máy local (Windows, MSVC, Qt 6.11) cho tới khi bật lại; trong thời gian đó Linux và macOS không được kiểm tra.

**Trạng thái:** M9.1–M9.5 xong theo D48 và D49 như đề xuất (thêm hàm mới; bật theo lựa chọn). `resetAllToDefault()`
(1 hàm test), `setTabStopsOnCheckBoxes()` cùng override `focusNextPrevChild()` để Tab tại checkbox đi tiếp theo chuỗi
(4 hàm test: hai chiều, bỏ qua checkbox, cuối view, qua proxy), `tests/api_compat/v1_5.cpp` và `api-1.5.txt` (chứa trọn
1.4), `examples/settings_dialog` có "Restore Defaults" và checkbox trong chuỗi Tab, SPEC §4.6, §5.5, §8, D48, D49.
`ctest` đạt 30/30 ở local. Vòng thử RC 7 đóng F4 và F5. M9.6: đã phát hành `1.5.0`.

### M10 — 1.6 (chỉ bổ sung): đổi theme bằng style sheet (QSS)

Theo yêu cầu của maintainer: ứng dụng đổi theme toàn bộ giao diện bằng style sheet và muốn property browser theo đó. Một
chương trình thử với style sheet dark cho toàn ứng dụng (giống sheet của các ứng dụng thật: rule cho `QWidget`, `QLabel`,
`QToolButton:checked`, `QTreeView::item:hover`, editor) cho thấy selector chuẩn của Qt đã phủ phần lớn qpb (tree view,
header, hover/selection của item, hàng xen kẽ, thanh cuộn, tooltip, menu và mọi editor, trong ô lẫn trong form), nhưng
chưa phủ bốn thứ qpb tự vẽ hoặc tự thiết lập:

1. **Tree view, hàng group:** nền là màu `Button` của palette, mà `background-color` của sheet đặt trùng màu với mọi
   hàng khác, nên hàng group không còn nổi bật; và không selector nào nhắm được hàng group (item không phải widget).
2. **Tree view, màu nhấn:** tên property đã sửa in đậm và giá trị read-only bị làm mờ (`PlaceholderText`), nhưng sheet
   không đổi được màu nào.
3. **Form view, nhãn đã sửa mất chữ đậm** dưới mọi sheet có rule cho `QLabel` (Qt đặt lại font của widget con được
   style) — một lỗi: dấu hiệu "đã sửa" biến mất.
4. **Form view, tiêu đề group** là `QToolButton` checkable thông thường: khi đang mở chúng dính rule `QToolButton:checked`
   chung của ứng dụng, và không có gì để nhắm riêng chúng.

1.6 thêm các móc cho những chỗ này; code viết cho 1.0–1.5 build và chạy như cũ, có hay không có style sheet.

**Tree view: các property màu, đặt được từ sheet bằng `qproperty-`.**

```cpp
Q_PROPERTY(QBrush groupBackground ...)     // mặc định: palette Button (như trước)
Q_PROPERTY(QColor groupForeground ...)     // mặc định: màu chữ của item
Q_PROPERTY(QColor modifiedForeground ...)  // mặc định: màu chữ của item (tên vẫn đậm)
Q_PROPERTY(QColor readOnlyForeground ...)  // mặc định: palette PlaceholderText (như trước)
```

```css
qpb--PropertyTreeView {
    qproperty-groupBackground: #2c3038;
    qproperty-modifiedForeground: #e87c00;
}
```

- Màu không hợp lệ / `Qt::NoBrush` nghĩa là mặc định. Delegate đưa chúng cho style qua `backgroundBrush` và màu `Text`
  của item, mà style của style sheet giữ nguyên (đã kiểm trong mã nguồn Qt 6.5 và 6.11); rule `::item` khớp và có
  `color` hoặc `background` vẫn thắng, như mọi view của Qt.
- Chỉ màu: `qproperty-` không đặt được font, và chữ đậm (group, tên đã sửa) là dấu hiệu cấu trúc.

**Form view: selector ổn định trên các widget (dynamic property).**

| Widget | Selector |
|---|---|
| khối / nút tiêu đề / thân của một group | `QWidget[qpbPart="group"]`, `QToolButton[qpbPart="groupTitle"]`, `QWidget[qpbPart="groupBody"]` |
| nhãn của một property | `QLabel[qpbPart="label"]`, và `[qpbModified="true"]` khi đã sửa |
| chữ read-only của kiểu không có editor | `QLabel[qpbPart="value"]` |
| nút chọn đường dẫn của editor path (cả hai view) | `QToolButton[qpbPart="browse"]` |

- `qpbModified` được cập nhật theo giá trị và nhãn được polish lại, nên rule cho nó có tác dụng ngay.
- Nhãn và tiêu đề đậm được đặt lại sau khi style sheet reset font của chúng (sửa lỗi 3.).
- Tên class C++ của widget nội bộ (`qpb--detail--PathEdit`, ...) không thuộc API; chỉ các property này là API.

**Example `examples/custom_theme`:** một mẫu sheet với placeholder `@{token}` và hai bảng token dark và light (graphite
và cam), đổi khi đang chạy; một tree view và một form view trên cùng model; style widget chuẩn bằng selector chuẩn và
các phần của qpb bằng các móc trên.

Quyết định: **D50** property màu trên tree view (item không phải widget; `qproperty-` là cách của Qt để sheet chạm tới
phần được vẽ); **D51** dynamic property `qpbPart` / `qpbModified` là API style sheet của form view, không tính tên class
của widget nội bộ; **D52** font nhấn được giữ dưới style sheet (đặt lại sau khi sheet reset).

| ID    | Việc                                                                                           | Ước tính (h) | Xong khi |
|-------|------------------------------------------------------------------------------------------------|----------|-----------|
| M10.1 | SPEC: §5.5 (property màu), §5.6 (selector, giữ chữ đậm), §5.8 mới (style sheet), §8 (1.6), D50–D52; `-vi` | 1.5 | Đã ghi quyết định |
| M10.2 | Tree view: bốn property, delegate dùng cho hàng group, tên đã sửa, giá trị read-only            | 3 | Test widget: mặc định, đặt bằng `qproperty-` từ sheet, pixel của hàng group và màu nhấn dưới sheet có rule `::item` |
| M10.3 | Form view và editor path: `qpbPart`, `qpbModified` kèm polish lại, giữ chữ đậm dưới sheet        | 2.5 | Test widget: selector khớp, `qpbModified` theo sửa và reset, nhãn vẫn đậm khi có rule `QLabel` |
| M10.4 | `tests/api_compat/v1_6.cpp` + `api-1.6.txt`; `examples/custom_theme` (+ dự án Qt Creator)        | 2 | Test tương thích, snapshot và build example đạt; ảnh chụp đã kiểm ở cả hai theme |
| M10.5 | Vòng thử RC 8: ứng dụng thử chạy dưới sheet dark dùng các móc; ghi phát hiện mới                 | 1.5 | Test trial đạt, cập nhật báo cáo |
| M10.6 | Phát hành 1.6.0                                                                                  | 0.5 | Có zip phát hành |

Tổng ≈ 11 h.

**Trạng thái:** M10.1–M10.5 xong theo D50–D52 như đề xuất. Chương trình thử cũng sửa lại hai phỏng đoán ban đầu: hàng
group không mất nền vì rule `::item`, mà vì `background-color` của sheet đặt `Button` của palette trùng màu các hàng; và
style của style sheet giữ màu `Text` của item (mã nguồn Qt 6.5 và 6.11), nên không cần xử lý thêm `WindowText`. Property
màu trên `PropertyTreeView` (1 hàm test kiểm pixel, cả khi rule `::item` thắng), `qpbPart` / `qpbModified` trên widget
của form và nút chọn đường dẫn, chữ đậm được đặt lại sau khi sheet reset (3 hàm test; test hồi quy fail nếu bỏ bản
sửa), `tests/api_compat/v1_6.cpp` và `api-1.6.txt` (chứa trọn 1.5), `examples/custom_theme` (đã xem ảnh chụp: dark, light,
không sheet), SPEC §5.5, §5.6, §5.8, §8, D50–D52. `ctest` đạt 33/33 ở local. Vòng thử RC 8 chạy ứng dụng thử dưới sheet
của ứng dụng dùng các móc; F11 và F12 là hành vi của Qt, đã ghi tài liệu. M10.6: đã phát hành `1.6.0`.

### M11 — Qt 5.15: có build và dùng qpb với nó được không?

Theo yêu cầu của maintainer: kiểm tra và build project với Qt 5.15. Hiện qpb yêu cầu Qt 6.5 (§7, SPEC §1), và API public
được thiết kế trên các kiểu của Qt 6.

**Chương trình dò (M11.1, 2026-10-07).** Qt 5.15.0 (`msvc2019_64`), ba toolset MSVC của Visual Studio 2026, một ứng dụng
Qt 5 tối giản (chỉ kiểm bộ công cụ) và `qpb/` nguyên bản (các target `Qt6::` trỏ sang Qt 5 trong một dự án tạm, gom mọi
lỗi bằng `ninja -k 0`):

| Trình biên dịch | Ứng dụng Qt 5.15 tối giản | `qpb/` |
|---|---|---|
| MSVC 2019 (14.29) | build và chạy được | lỗi: 26 vị trí trong header public của qpb |
| MSVC 2022 (14.44) | build được, 28 cảnh báo C4996 (STL4043: `stdext::checked_array_iterator` deprecated) | chưa thử (cùng header) |
| MSVC 2026 (14.51) | lỗi: chính header của Qt gọi `stdext::make_checked_array_iterator`, đã bị bỏ khỏi thư viện chuẩn | lỗi: header của Qt và của qpb |

1. **Bộ công cụ:** Qt 5.15.0 chạy với MSVC 2019 và 2022, không chạy với MSVC 2026 (cần một bản Qt 5.15 có header không
   còn dùng `stdext`). Cấu hình Qt 5 của qpb sẽ được build và kiểm tra với MSVC 2019 / 2022 (hoặc GCC trên Linux).
2. **Header public dùng API Qt 6**, nên ngay cả include cũng lỗi:
   - `Attr::*` và `Types::*` là `inline constexpr QLatin1StringView` (Qt 6.4); `QLatin1String` của Qt 5 không thể
     `constexpr` từ một chuỗi literal (25 chỗ dùng);
   - `TypeHandler::storageType` là giá trị `QMetaType` và `registerType<T>()` dùng `QMetaType::fromType<T>()`; trong
     Qt 5 `QMetaType` không copy hay gán được và kiểu là id `int`;
   - `<QtCore/qvariantmap.h>` không có trong Qt 5.
3. **Mã nguồn dùng API Qt 6** (thấy khi có shim cho header, 43 vị trí lỗi trong `qpb::core`): `QVariant::metaType()` /
   `typeId()`, `QVariant::convert(QMetaType)`, `QMetaProperty::metaType()`, `QJsonValue::toInteger()`; và, tìm bằng
   search, `QFormLayout::setRowVisible()` (6.4) và `&QComboBox::activated` (có overload, nên mơ hồ, trong Qt 5).
   `qpb::widgets`, test và example chưa được tới.

**Hệ quả.** Bản build với Qt 5 không thể có cùng API public: ít nhất kiểu của `TypeHandler::storageType` và dạng của
các hằng `Attr` / `Types` phải khác. Các phương án (quyết định **D53**: **(b)**, maintainer chọn ngày 2026-10-07):

- **(a) Không hỗ trợ Qt 5.** Giữ tối thiểu Qt 6.5 và ghi rõ trong README (Qt 5.15 bản mã nguồn mở đã hết hỗ trợ từ
  2023). Chỉ sửa tài liệu.
- **(b) Qt 5.15 là cấu hình thứ hai.** CMake tìm Qt 6 hoặc Qt 5; một header tương thích nội bộ cho mã nguồn; biến thể
  `#if QT_VERSION` trong header public cho các hằng và `storageType` (id kiểu `int` với Qt 5); cam kết tương thích
  (SPEC §9) áp dụng theo từng major của Qt, mỗi major một snapshot API; test, example và test consumer cũng build với
  Qt 5.15 và MSVC 2019 / 2022. Bản build Qt 6 không thay đổi gì. Phát hành 1.7.0. Ước tính ≈ 30 h.
- **(c) Một nhánh `qt5` riêng** của qpb: mọi thay đổi sau này phải port hai lần.

| ID    | Việc                                                                                           | Ước tính (h) | Xong khi |
|-------|------------------------------------------------------------------------------------------------|----------|-----------|
| M11.1 | Build dò với Qt 5.15 và MSVC 2019 / 2022 / 2026, liệt kê những gì hỏng                          | 2 | Xong (ở trên) |
| M11.2 | Chốt D53 (a / b / c); ghi vào SPEC (+ `-vi`)                                                     | 0.5 | Đã ghi quyết định |
| M11.3 | (b) Script build local cho Qt 5.15 với MSVC 2019 (14.29); job CI khi bật lại CI                 | 1.5 | `qpb/` configure được với Qt 5.15 |
| M11.4 | (b) CMake: Qt 6 hoặc Qt 5 (`find_package(QT NAMES Qt6 Qt5)`), kiểm tra bản tối thiểu, không đổi thiết lập global | 2 | Test consumer đạt với cả hai |
| M11.5 | (b) Header public: biến thể Qt 5 của các hằng và `storageType`, snapshot theo major Qt; SPEC §9 theo major Qt | 6 | Header tự đủ với cả hai; snapshot Qt 6 không đổi |
| M11.6 | (b) Mã nguồn: header tương thích; `qpb::core` và `qpb::widgets` build với Qt 5.15 không cảnh báo | 10 | Cả hai thư viện build được |
| M11.7 | (b) Test, example, RC trial với Qt 5.15                                                         | 7 | Tất cả đạt với Qt 5.15 và Qt 6 |
| M11.8 | (b) Phát hành 1.7.0                                                                              | 1 | Có zip phát hành |

**Trạng thái:** M11.1-M11.7 xong. Qt 5.15.0 với MSVC 2019 (14.29): thư viện, test và example build không cảnh báo
và `ctest` đạt 34/34; RC trial đạt mà không phải sửa gì; dự án qmake cho Qt Creator build được và cả tám example khởi
động. Qt 6.11 với MSVC 2026: vẫn 34/34, snapshot không đổi. Những việc đã làm:
- CMake tìm Qt 6 hoặc Qt 5 (component, cây phát triển, test, dự án consumer, trial, dự án Qt Creator);
  `QT_DISABLE_DEPRECATED_BEFORE=0x050F00` với Qt 5;
- header public: nhánh Qt 5 cho các hằng `Types` / `Attr` (`QLatin1String`), `TypeHandler::storageType` (`int`) và một
  include; `tools/api_snapshot.py --qt-major`, snapshot `api-qt5-1.7.txt`, file tương thích `qt5_v1_7.cpp` (v1_0 chỉ
  build với Qt 6 vì dùng giá trị `QMetaType`);
- mã nguồn: `src/core/compat_p.h` và `src/widgets/compat_p.h` (meta type, số nguyên JSON, kiểm tra tràn số, vị trí chuột,
  hàng của form) và một khác biệt thật: `PropertyFilterProxyModel` bỏ qua bộ lọc `QRegExp` mà `setFilterFixedString()`
  của Qt 5 đặt, nên không lọc gì;
- test và example: lời gọi dùng được cho cả hai bản (`userType()`, `qOverload<int>`, `QRegularExpressionMatchIterator`);
  test widget trên Windows được đặt `QT_QPA_FONTDIR`, vì nền tảng offscreen của Qt 5 không vẽ chữ khi thiếu font;
- CI: job Qt 5.15.2 trên Ubuntu 22.04 và Windows 2022 (chạy khi bật lại CI); SPEC mục 9.6, D53; `qpb/README.md` mục
  "Qt 5". Tiếp theo: M11.8 (phát hành 1.7.0).

---

## 3. Phụ thuộc giữa các task

```text
M0.2 ─► M0.3 ─► M0.4 ─► M0.5
M0.* ─► M1.1 ─► M1.2, M1.3 ─► M1.4 ─► M1.5
M1.* ─► M2.1 ─► M2.2 ─► M2.3
        M2.4 ─► M2.5 ─► M2.6 ─► M2.7 ─► M2.8
M2.4 ─► M3.1 ─► M3.2 ─► M3.3 ─► M3.4, M3.5 ─► M3.6 ─► M3.7 ─► M3.8
M3.* ─► M4.* ─► RC ─► 1.0.0 ─► M5 ─► M6 ─► M7 ─► M8 ─► M9 ─► M10 ─► M11
```

---

## 4. Quy trình phát hành (mọi bản 1.x)

Hướng dẫn chi tiết và công cụ: [`RELEASING.md`](RELEASING.md), `tools/make_release.sh`.

1. Cập nhật `qpb/VERSION` và mục mới trong `qpb/CHANGELOG.md` (Added / Changed / Deprecated / Fixed / **Upgrade notes**).
2. CI xanh: unit test, `api_compat/*` của **mọi** bản trước, `tests/consumer`, `check_arch`, diff API snapshot chỉ có thêm.
3. Thêm `tests/api_compat/v<x_y>.cpp` cho bản mới (bản minor) và cập nhật snapshot.
4. Tag `vX.Y.Z` trên `main`.
5. Tạo artifact: `qpb-X.Y.Z.zip` (chỉ folder `qpb/`) đính kèm GitHub Release; cập nhật nhánh `qpb-release`
   (`git subtree split --prefix qpb -b qpb-release`) và gắn tag `qpb-vX.Y.Z` lên đỉnh nhánh, cho consumer dùng
   `git subtree` hoặc `git submodule` (SPEC §6.2, D47).

**Phía consumer khi nâng cấp:** thay `components/qpb/` (zip, `git subtree pull` hoặc checkout `qpb-vX.Y.Z` trong
submodule) → build lại → đọc *Upgrade notes*. Với 1.x mong đợi là "không cần làm gì".

---

## 5. Quy trình làm việc

- **Nhánh:** mỗi task/nhóm task một nhánh `feat/M2.4-type-registry`, PR vào `main`, squash merge.
- **Definition of Done cho mọi PR:** build sạch cảnh báo; test mới cho hành vi mới; `ctest` pass (widgets `offscreen`);
  không vi phạm R1–R5 (SPEC §3); public API mới có nơi dùng.
- **PR chạm `qpb/include/`:** phải có dòng "API change: none / additive / breaking" trong mô tả.
  Sau 1.0, "breaking" bị từ chối trừ khi đang làm 2.0.
- **Commit:** Conventional Commits (`feat(core): ...`, `fix(widgets): ...`).
- **Không chép code** từ QtPropertyBrowser/QtnProperty hay nguồn có license khác (G7); chỉ tham khảo hành vi UX.

---

## 6. Rủi ro & phương án cắt phạm vi

| Rủi ro                                        | Dấu hiệu                                       | Phản ứng                                                                    |
|-----------------------------------------------|------------------------------------------------|-----------------------------------------------------------------------------|
| API 1.0 sai mà đã khóa                        | Trong RC phải thêm workaround ở project thật   | Kéo dài RC; không tag 1.0 khi còn nghi ngờ. Sau 1.0: chỉ thêm API mới + deprecate cái cũ |
| Chậm tiến độ                                  | Hết tuần 5 chưa xong M3.6                       | Cắt khỏi 1.0 (thêm lại ở 1.x là *bổ sung*, không phá API): DirPath → Tab navigation → build shared. **Không** cắt M1, M4.4–M4.6 |
| UX editor cần hack sâu                        | M3.4/M3.6 vượt 2× ước lượng                    | Điểm dừng cuối M3.6                                                         |
| Consumer có cấu hình CMake lạ làm hỏng build  | RC.1 lỗi CMake                                  | Thêm case vào `tests/consumer`, sửa `qpb/CMakeLists.txt`                   |
| `TreeObserver` gây lỗi index                   | `QAbstractItemModelTester` fail                | 1.0 chỉ cho đổi cấu trúc qua `setRoot`; thêm/xóa lúc chạy là bổ sung ở 1.1 (API `add/remove` đã có sẵn, chỉ ghi rõ hạn chế) |
| Dialog native khác nhau giữa OS               | Editor đóng khi dialog mở trên Windows/macOS   | Fallback: đóng editor trước khi mở dialog, kết quả ghi qua `model->setData` |

---

## 7. Quyết định đã chốt / còn mở

| Câu hỏi                                   | Trạng thái                                     |
|-------------------------------------------|------------------------------------------------|
| C++17 hay C++20?                          | **Chốt: C++17**                                |
| Qt tối thiểu?                             | **Chốt: 6.5**; Qt 5.15 là cấu hình thứ hai từ 1.7 (M11, D53) |
| Namespace / tên target / prefix include   | **Chốt: `qpb`, `qpb::core`, `qpb::widgets`, `<qpb/...>`** |
| Build system của các project dùng qpb     | **Chốt: chỉ CMake**                            |
| Ngôn ngữ code và tài liệu                  | **Chốt: tiếng Anh** (bản `-vi` để tham khảo)   |
| Cách đồng bộ folder components            | **Chốt:** zip phát hành (mặc định), `git subtree` hoặc `git submodule` từ `qpb-release`, cố định version bằng tag `qpb-vX.Y.Z` (SPEC §6.2, D47) |
| License                                   | Mở — giả định MIT                              |
| "Custom property table" nghĩa là gì?      | **Chốt:** người dùng tự dựng property table bằng API public của thư viện (G1, G3); không thêm khái niệm mới |
| Nguồn dữ liệu chính?                      | Mở — giả định builder tường minh; cần chốt trước M1.3 |
